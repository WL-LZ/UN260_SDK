#include "counting_report_sync.h"
#include "counting_data_store.h"
#include "un260/protocol/protocol_send.h"
#include <stdlib.h>
#include <string.h>

enum { REPORT_IDLE, REPORT_REJECT_LIVE, REPORT_REJECT, REPORT_SERIAL };
enum { REPORT_IDLE_TIMEOUT_MS = 3000, REPORT_TOTAL_TIMEOUT_MS = 120000 };
static struct {
    unsigned stage;
    uint32_t generation, owner, requested, activity;
    bool enabled, add, due, scheduled, started, invalid, live_seen, reusable;
    counting_report_failure_t failure;
    bool live_limit_reported;
    unsigned rows, published_rows;
    int live_next;
    uint64_t denom_amount;
    unsigned denom_pcs, pass_pcs, serial_base;
    bool denom_seen, final_denoms, group_restart;
    uint8_t base_count, pass_count;
    denom_t base_denoms[COUNTING_DENOM_MAX_ITEMS];
    denom_t pass_denoms[COUNTING_DENOM_MAX_ITEMS];
    uint8_t last_payload[20];
    counting_sim_t staging;
    struct {
        bool valid;
        int pcs;
        double amount;
        uint16_t rejects, error_count;
        uint8_t denom_count, error_codes[256], error_pcs[256];
        denom_t denoms[COUNTING_DENOM_MAX_ITEMS];
    } cache;
} report;

static bool marker(const uint8_t *buf, uint8_t len, uint8_t value)
{
    for (unsigned i = 4; i + 1U < len; ++i)
        if (buf[i] != value) return false;
    return true;
}

static bool settled(const counting_session_state_t *s)
{
    return s && !s->start_confirmed && s->phase == COUNTING_SESSION_FINISHED_WAIT_START;
}

static void fail(counting_report_failure_t reason)
{
    report.invalid = true;
    report.cache.valid = false;
    report.failure = reason;
    counting_data_clear_serials(&report.staging);
}

void counting_report_begin(bool add, counting_sim_t *data)
{
    if (++report.generation == 0) ++report.generation;
    /* An old query remains owned until its END arrives. Without a wire
     * transaction id, a timeout is not permission to overlap another query. */
    counting_data_clear_serials(&report.staging);
    report.enabled = true;
    if (report.stage == REPORT_IDLE) report.invalid = report.started = false;
    report.add = add;
    report.denom_seen = report.final_denoms = report.group_restart = false;
    report.pass_pcs = report.pass_count = report.base_count = 0;
    memset(report.pass_denoms, 0, sizeof(report.pass_denoms));
    memset(report.base_denoms, 0, sizeof(report.base_denoms));
    if (add && data) {
        report.base_count = data->denom_number;
        memcpy(report.base_denoms, data->denom, sizeof(report.base_denoms));
    }
    unsigned serial_rows = data ? (unsigned)counting_data_serial_scan_limit(data) : 0;
    while (serial_rows && !data->sn_str[serial_rows - 1U]) --serial_rows;
    /* Allocation capacity is not the previous note count. Preserve trailing
     * unreadable serial slots from either the live pass or final report. */
    if (add && report.live_seen && report.live_next > (int)serial_rows)
        serial_rows = (unsigned)report.live_next;
    if (add && report.cache.valid && report.published_rows > serial_rows)
        serial_rows = report.published_rows;
    report.serial_base = add ? serial_rows : 0;
    report.denom_pcs = add && data && data->total_pcs > 0 ? (unsigned)data->total_pcs : 0;
    report.denom_amount = add && data && data->total_amount > 0 ? (uint64_t)data->total_amount : 0;
    if (!add && data) {
        for (unsigned i = 0; i < data->denom_number; ++i) {
            data->denom[i].pcs = 0;
            data->denom[i].amount = 0;
        }
    }
    report.reusable = add && report.cache.valid && report.stage == REPORT_IDLE;
    report.live_seen = report.due = report.scheduled = false;
    report.live_limit_reported = false;
    report.live_next = -1;
    if (!add) {
        report.cache.valid = false;
        report.published_rows = 0;
        /* The previous result has already transferred to history before
         * START. Never attach its serials to a new non-ADD count. */
        if (data) {
            counting_data_clear_serials(data);
            counting_data_clear_errors(data);
            data->err_expected = 0;
        }
    }
}

/* A is a projection of explicit note denominations, never amount differences. */
static void denom_publish(counting_sim_t *d)
{
    for (unsigned i = 0; i < d->denom_number; ++i)
        d->denom[i].pcs = 0, d->denom[i].amount = 0;
    for (unsigned source = 0; source < 2; ++source) {
        const denom_t *rows = source ? report.pass_denoms : report.base_denoms;
        unsigned count = source ? report.pass_count : report.base_count;
        for (unsigned i = 0; i < count; ++i) {
            unsigned slot = 0;
            while (slot < d->denom_number && d->denom[slot].value != rows[i].value) ++slot;
            if (slot == d->denom_number) {
                if (slot == COUNTING_DENOM_MAX_ITEMS) continue;
                d->denom[slot] = (denom_t){.value = rows[i].value};
                ++d->denom_number;
            }
            unsigned pcs = (unsigned)d->denom[slot].pcs + rows[i].pcs;
            d->denom[slot].pcs = pcs > UINT16_MAX ? UINT16_MAX : pcs;
            d->denom[slot].amount = (float)d->denom[slot].value * d->denom[slot].pcs;
        }
    }
}

static void denom_restart(counting_sim_t *d)
{
    if (!report.denom_pcs && !report.base_count) return;
    report.denom_pcs = report.denom_amount = report.base_count = 0;
    memset(report.base_denoms, 0, sizeof(report.base_denoms));
    report.group_restart = true;
    d->last_total_pcs = d->last_valid_pcs = 0;
    d->last_total_amount = 0;
    d->last_issue_pcs = d->last_suspect_pcs = d->last_damaged_pcs = 0;
    report.cache.valid = report.reusable = false;
    report.published_rows = 0;
    /* Keep this pass's serials when notes arrived before the first total. */
    unsigned base = report.serial_base;
    if (report.live_seen && d->sn_str && base <= (unsigned)d->sn_capacity) {
        for (unsigned i = 0; i < base; ++i) free(d->sn_str[i]);
        unsigned remain = (unsigned)d->sn_capacity - base;
        memmove(d->sn_str, d->sn_str + base, remain * sizeof(*d->sn_str));
        memset(d->sn_str + remain, 0, base * sizeof(*d->sn_str));
        memmove(d->denom_mix, d->denom_mix + base,
                (COUNTING_DATA_MAX_ITEMS - base) * sizeof(d->denom_mix[0]));
        memset(d->denom_mix + COUNTING_DATA_MAX_ITEMS - base, 0,
               base * sizeof(d->denom_mix[0]));
        report.live_next -= (int)base;
    } else {
        counting_data_clear_serials(d);
        report.live_next = report.live_seen ?
            (report.live_next > (int)base ? report.live_next - (int)base : 0) : -1;
    }
    report.serial_base = 0;
    counting_data_clear_errors(d);
    denom_publish(d);
}

bool counting_report_observe_total(counting_sim_t *d, uint32_t amount, unsigned pcs)
{
    if (!report.enabled || !d || d->multi_currency_result || report.final_denoms) return false;
    if (!pcs) {
        if (!report.denom_seen && report.add && !amount) {
            d->total_pcs = (int)report.denom_pcs;
            d->total_amount = (double)report.denom_amount;
        }
        return false;
    }
    bool restarted = !report.denom_seen && pcs < report.denom_pcs;
    if (restarted) denom_restart(d);
    report.denom_seen = true;
    return restarted;
}

bool counting_report_observe_note(counting_sim_t *d, unsigned value)
{
    if (!report.enabled || !d || d->multi_currency_result || report.final_denoms ||
        !value || value > 2147483647U) return false;
    unsigned slot = 0;
    while (slot < report.pass_count && report.pass_denoms[slot].value != (int)value) ++slot;
    if (slot == report.pass_count) {
        if (slot == COUNTING_DENOM_MAX_ITEMS) return false;
        report.pass_denoms[slot] = (denom_t){.value = (int)value};
        ++report.pass_count;
    }
    if (report.pass_denoms[slot].pcs == UINT16_MAX) return false;
    ++report.pass_denoms[slot].pcs;
    ++report.pass_pcs;
    report.cache.valid = report.reusable = false;
    denom_publish(d);
    return true;
}

void counting_report_denom_snapshot(counting_sim_t *d, bool final)
{
    if (!d || !report.enabled || !final) return;
    /* Equal first totals cannot distinguish retained ADD state from a reset.
     * Resolve that ambiguity using the complete explicit denomination table. */
    bool pass_matches = report.pass_pcs != 0;
    for (unsigned i = 0; i < d->denom_number; ++i) {
        unsigned pcs = 0;
        for (unsigned j = 0; j < report.pass_count; ++j)
            if (d->denom[i].value == report.pass_denoms[j].value) pcs = report.pass_denoms[j].pcs;
        if (pcs != d->denom[i].pcs) pass_matches = false;
    }
    for (unsigned i = 0; i < report.pass_count; ++i) {
        bool found = false;
        for (unsigned j = 0; j < d->denom_number; ++j)
            if (report.pass_denoms[i].value == d->denom[j].value) found = true;
        if (!found) pass_matches = false;
    }
    if (report.add && report.denom_pcs && pass_matches) {
        denom_t snapshot[COUNTING_DENOM_MAX_ITEMS];
        uint8_t count = d->denom_number;
        memcpy(snapshot, d->denom, sizeof(snapshot));
        denom_restart(d);
        memcpy(d->denom, snapshot, sizeof(snapshot));
        d->denom_number = count;
    }
    report.final_denoms = true;
}

bool counting_report_take_group_restart(void)
{
    bool restarted = report.group_restart;
    report.group_restart = false;
    return restarted;
}

void counting_report_reset(void)
{
    counting_report_begin(false, NULL);
    report.enabled = false;
    report.published_rows = 0;
}

void counting_report_shutdown(void)
{
    counting_data_clear_serials(&report.staging);
    memset(&report, 0, sizeof(report));
}

bool counting_report_transfer_active(void)
{
    return report.stage != REPORT_IDLE && !report.invalid;
}

static bool unchanged(const counting_sim_t *d)
{
    if (!report.reusable || !report.cache.valid || report.live_seen ||
        d->total_pcs != report.cache.pcs || d->total_amount != report.cache.amount ||
        d->err_expected != report.cache.rejects || d->err_num != report.cache.error_count ||
        d->denom_number != report.cache.denom_count) return false;
    for (unsigned i = 0; i < d->denom_number; ++i) {
        const denom_t *a = &d->denom[i], *b = &report.cache.denoms[i];
        if (a->value != b->value || a->pcs != b->pcs || a->amount != b->amount) return false;
    }
    for (unsigned i = 0; i < d->err_num; ++i)
        if (d->err_code[i] != report.cache.error_codes[i] ||
            d->err_pcs[i] != report.cache.error_pcs[i]) return false;
    return true;
}

static void remember(const counting_sim_t *d)
{
    report.cache.valid = d->err_num <= 256 && d->denom_number <= COUNTING_DENOM_MAX_ITEMS;
    if (!report.cache.valid) return;
    report.cache.pcs = d->total_pcs;
    report.cache.amount = d->total_amount;
    report.cache.rejects = d->err_expected;
    report.cache.error_count = d->err_num;
    report.cache.denom_count = d->denom_number;
    memcpy(report.cache.denoms, d->denom, sizeof(d->denom));
    for (unsigned i = 0; i < d->err_num; ++i) {
        report.cache.error_codes[i] = d->err_code[i];
        report.cache.error_pcs[i] = d->err_pcs[i];
    }
}

void counting_report_poll(const counting_session_state_t *s,
                          uint32_t now)
{
    if (report.stage != REPORT_IDLE) {
        if (!report.invalid &&
            ((uint32_t)(now - report.activity) >= REPORT_IDLE_TIMEOUT_MS ||
             (uint32_t)(now - report.requested) >= REPORT_TOTAL_TIMEOUT_MS))
            fail(report.stage != REPORT_SERIAL ? COUNTING_REPORT_FAILURE_REJECT
                                              : COUNTING_REPORT_FAILURE_SERIAL);
        return;
    }
    if (!report.enabled || !report.due || !settled(s)) return;
    report.due = false;
    report.invalid = report.started = false;
    report.owner = report.generation;
    report.requested = report.activity = now;
    const uint8_t request = 1;
    if (protocol_send(0x0C, &request, 1) < 0) { fail(COUNTING_REPORT_FAILURE_REJECT); return; }
    report.stage = REPORT_REJECT;
}

void counting_report_schedule(const counting_session_state_t *s,
                              uint32_t now)
{
    if (!report.enabled || report.scheduled || !settled(s)) return;
    report.scheduled = report.due = true;
    counting_report_poll(s, now);
}

bool counting_report_discard_stale(uint8_t cmd, const uint8_t *buf, uint8_t len)
{
    unsigned stage = cmd == 0x0C
        ? (report.stage == REPORT_REJECT_LIVE ? REPORT_REJECT_LIVE : REPORT_REJECT)
        : cmd == 0x0D ? REPORT_SERIAL : REPORT_IDLE;
    if (!stage || report.stage != stage ||
        (!report.invalid && report.owner == report.generation)) return false;
    if ((cmd == 0x0C && len == 7) || (cmd == 0x0D && len == 25)) {
        if (marker(buf, len, 0xFF)) {
            report.stage = REPORT_IDLE;
            report.invalid = report.started = false;
            counting_data_clear_serials(&report.staging);
        }
    }
    return true;
}

uint32_t counting_report_live_owner(const counting_session_state_t *s)
{
    if (!s || !report.enabled || (report.invalid && report.stage != REPORT_IDLE) ||
        (report.stage != REPORT_IDLE && report.stage != REPORT_REJECT_LIVE)) return 0;
    if (report.stage == REPORT_REJECT_LIVE && report.owner != report.generation) return 0;
    if ((s->start_confirmed && s->phase != COUNTING_SESSION_FINISHED_WAIT_START) ||
        (report.stage == REPORT_REJECT_LIVE && settled(s))) return report.generation;
    return 0;
}

bool counting_report_live_reject_start(const counting_session_state_t *s, uint32_t now)
{
    if (!s || !s->start_confirmed || !counting_report_live_owner(s)) return false;
    if (report.stage == REPORT_IDLE) {
        report.stage = REPORT_REJECT_LIVE;
        report.invalid = report.started = false;
        report.owner = report.generation;
        report.requested = report.activity = now;
    }
    return true;
}

void counting_report_live_reject_end(const counting_session_state_t *s, uint32_t now)
{
    if (report.stage != REPORT_REJECT_LIVE || !counting_report_live_owner(s)) return;
    report.stage = REPORT_IDLE;
    counting_report_poll(s, now);
}

bool counting_report_accept_reject(const counting_session_state_t *s)
{
    return report.stage == REPORT_REJECT && report.owner == report.generation &&
           !report.invalid && settled(s);
}

bool counting_report_reject_start(void)
{
    if (report.started) { fail(COUNTING_REPORT_FAILURE_REJECT); return false; }
    report.started = true;
    return true;
}

bool counting_report_reject_started(void) { return report.started; }
void counting_report_touch(uint32_t now) { report.activity = now; }

bool counting_report_accept_serial(const counting_session_state_t *s)
{
    return report.stage == REPORT_SERIAL && report.owner == report.generation &&
           !report.invalid && settled(s);
}

counting_report_result_t counting_report_reject_end(
    const counting_session_state_t *s, const counting_sim_t *d, uint32_t now)
{
    if (!counting_report_accept_reject(s)) return COUNTING_REPORT_IGNORED;
    report.stage = REPORT_IDLE;
    if (!report.started || (d->err_expected && !d->err_num)) {
        fail(COUNTING_REPORT_FAILURE_REJECT); return COUNTING_REPORT_FAILED;
    }
    if (unchanged(d)) return COUNTING_REPORT_REUSED;
    report.requested = report.activity = now;
    report.started = report.invalid = false;
    const uint8_t request[2] = {1, 1};
    if (protocol_send(0x0D, request, sizeof(request)) < 0) {
        fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED;
    }
    report.stage = REPORT_SERIAL;
    return COUNTING_REPORT_IGNORED;
}

static void publish(counting_sim_t *d)
{
    /* Swap ownership, not a visible clear/repopulate sequence. The retired
     * strings are released once, after the complete snapshot is published. */
    char **strings = d->sn_str;
    int capacity = d->sn_capacity;
    d->sn_str = report.staging.sn_str;
    d->sn_capacity = report.staging.sn_capacity;
    report.staging.sn_str = strings;
    report.staging.sn_capacity = capacity;
    memcpy(d->denom_mix, report.staging.denom_mix, sizeof(d->denom_mix));
    counting_data_clear_serials(&report.staging);
    report.published_rows = report.rows;
    remember(d);
}

counting_report_result_t counting_report_serial(
    const counting_session_state_t *s, counting_sim_t *d,
    const uint8_t *buf, uint8_t len, uint32_t now)
{
    if (len != 25 || !counting_report_accept_serial(s)) return COUNTING_REPORT_IGNORED;
    report.activity = now;
    if (marker(buf, len, 0)) {
        if (report.started) { fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED; }
        counting_data_clear_serials(&report.staging);
        report.started = true;
        report.rows = 0;
        return COUNTING_REPORT_IGNORED;
    }
    if (marker(buf, len, 255)) {
        report.stage = REPORT_IDLE;
        if (!report.started || report.invalid) { fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED; }
        publish(d);
        return COUNTING_REPORT_READY;
    }
    if (!report.started || report.invalid) return COUNTING_REPORT_IGNORED;
    /* All 256 sequence bytes are valid for a data row. Only the entire
     * all-zero/all-FF payload denotes a boundary. Empty serial rows still
     * consume their original NO; equal serial text is never deduplicated. */
    if (report.rows && buf[4] == (uint8_t)report.rows &&
        !memcmp(buf + 4, report.last_payload, sizeof(report.last_payload)))
        return COUNTING_REPORT_IGNORED;
    if (buf[4] != (uint8_t)(report.rows + 1U) || report.rows >= COUNTING_DATA_MAX_ITEMS) {
        fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED;
    }
    memcpy(report.last_payload, buf + 4, sizeof(report.last_payload));
    unsigned slot = report.rows++;
    char denom[8], serial[13], *end;
    memcpy(denom, buf + 5, 7); denom[7] = 0;
    long value = strtol(denom, &end, 10);
    while (*end == ' ') ++end;
    if (end == denom || *end || value < 0 || value > 2147483647L) {
        fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED;
    }
    memcpy(serial, buf + 12, 12); serial[12] = 0;
    int n = 12;
    while (n && serial[n - 1] == ' ') serial[--n] = 0;
    char *text = serial;
    while (*text == ' ') ++text;
    if (!*text || value == 0) return COUNTING_REPORT_IGNORED;
    if (!counting_data_ensure_serial_capacity(&report.staging, (int)slot + 1)) {
        fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED;
    }
    size_t size = strlen(text) + 1U;
    char *copy = malloc(size);
    if (!copy) { fail(COUNTING_REPORT_FAILURE_SERIAL); return COUNTING_REPORT_FAILED; }
    memcpy(copy, text, size);
    report.staging.sn_str[slot] = copy;
    report.staging.denom_mix[slot] = (int)value;
    return COUNTING_REPORT_IGNORED;
}

int counting_report_live_slot(counting_sim_t *d, bool *cleared)
{
    *cleared = false;
    if (!report.enabled) return -1;
    if (!report.live_seen) {
        if (!report.add) {
            counting_data_clear_serials(d);
            report.published_rows = 0;
            *cleared = true;
        }
        report.live_next = (int)report.serial_base;
    }
    report.live_seen = true;
    report.cache.valid = false;
    if (report.live_next >= COUNTING_DATA_MAX_ITEMS) {
        if (!report.live_limit_reported) { fail(COUNTING_REPORT_FAILURE_CAPACITY); report.live_limit_reported = true; }
        return -1;
    }
    return report.live_next++;
}

counting_report_failure_t counting_report_take_failure(void)
{
    counting_report_failure_t failed = report.failure;
    report.failure = COUNTING_REPORT_FAILURE_NONE;
    return failed;
}
