#include "counting_multi.h"
#include <string.h>
#include "un260/protocol/protocol_send.h"

static counting_multi_t model;
static int latest_index = -1;
static bool prefetch_pending;
static bool prefetch_settling;
static uint32_t prefetch_after;
static uint8_t prefetch_attempts[COUNTING_MULTI_MAX];
static uint32_t retry_after[COUNTING_MULTI_MAX];
#define MULTI_SETTLE_MS 250U
#define MULTI_PREFETCH_ATTEMPTS 3U
/* 0B has no currency / transaction id. This ownership outlives all pages,
 * mode switches and clears. Never reassign an unfinished response. */
static struct {
    bool busy, started, invalid, abandoned, expired;
    unsigned index;
    uint32_t generation, activity, sent;
    uint8_t count;
    multi_denom_t items[COUNTING_MULTI_DENOMS];
} query;

const counting_multi_t *counting_multi_current(void) { return &model; }
const multi_currency_t *counting_multi_latest(void)
{
    return latest_index>=0 && (unsigned)latest_index<model.count ? &model.currencies[latest_index] : NULL;
}
bool counting_multi_query_busy(void) { return query.busy; }
void counting_multi_drain_legacy(void)
{
    if (query.busy) return;
    memset(&query, 0, sizeof(query));
    query.busy = query.abandoned = true;
}
void counting_multi_reset(void)
{
    latest_index = -1;
    prefetch_pending = false;
    prefetch_settling = false;
    memset(prefetch_attempts, 0, sizeof(prefetch_attempts));
    memset(retry_after, 0, sizeof(retry_after));
    uint32_t revision = model.revision + 1, generation = model.generation + 1;
    memset(&model, 0, sizeof(model));
    model.revision = revision;
    model.generation = generation;
    model.group_generation = generation;
    if (query.busy) query.abandoned = true;
}
void counting_multi_begin(bool add)
{
    if (!model.group_generation) counting_multi_reset();
    latest_index = -1;
    prefetch_pending = false;
    prefetch_settling = false;
    memset(prefetch_attempts, 0, sizeof(prefetch_attempts));
    memset(retry_after, 0, sizeof(retry_after));
    /* A pass is not a new stacker batch. Only an explicit result reset
     * (confirmed removal, CLEAR or currency change) closes the group.
     * ADD is controller metadata, never permission to discard currencies. */
    {
        model.generation++;
        if (query.busy) query.abandoned = true;
        for (unsigned i = 0; i < model.count; ++i) {
            /* Preserve verified details of currencies not updated this pass.
             * An unfinished old request cannot own the next generation. */
            if (model.currencies[i].status == MULTI_DETAIL_LOADING)
                model.currencies[i].status = MULTI_DETAIL_NONE;
        }
    }
    model.counting = true;
    model.add = add;
    if (model.passes < UINT32_MAX) model.passes++;
    model.revision++;
}
bool counting_multi_info(const uint8_t *f, uint8_t len)
{
    if (!f || (len != 13 && len != 16) || !model.counting || f[3] != 0x0e) return false;
    const unsigned p = len == 16 ? 7 : 4;
    const uint8_t status = f[p + 7];
    if (status > 5) return false;
    if (len == 16) {
        if (status != 1) return false;
        for (unsigned i = 4; i < 7; ++i) if (f[i] < 'A' || f[i] > 'Z') return false;
        unsigned index;
        for (index = 0; index < model.count; ++index)
            if (!memcmp(model.currencies[index].code, f + 4, 3)) break;
        if (index == COUNTING_MULTI_MAX) { model.overflow = true; model.revision++; return false; }
        multi_currency_t *c = &model.currencies[index];
        if (index == model.count) { memcpy(c->code, f + 4, 3); c->code[3] = 0; model.count++; }
        c->amount = ((uint32_t)f[7] << 24) | ((uint32_t)f[8] << 16) | ((uint32_t)f[9] << 8) | f[10];
        c->pcs = ((uint16_t)f[11] << 8) | f[12];
        latest_index = (int)index;
        c->status = MULTI_DETAIL_NONE;
        c->denom_count = 0;
        model.total_pcs = 0;
        for (unsigned i = 0; i < model.count; ++i) model.total_pcs += model.currencies[i].pcs;
        model.reject = f[13];
    } else if (status == 1) {
        /* Global final summary has no currency or amount in MULTI. */
        model.total_pcs = ((uint16_t)f[8] << 8) | f[9];
        model.reject = f[10];
    } else if (status >= 2) {
        model.counting = false;
        prefetch_pending = true;
        prefetch_settling = true;
    }
    /* The status-0 zero frame is a delimiter, not CLEAR, including in ADD. */
    model.revision++;
    return true;
}
bool counting_multi_request(unsigned index, uint32_t now)
{
    if (model.counting || query.busy || index >= model.count) return false;
    uint8_t payload[4] = {0};
    memcpy(payload + 1, model.currencies[index].code, 3);
    memset(&query, 0, sizeof(query));
    query.index = index; query.generation = model.generation;
    query.activity = query.sent = now;
    multi_currency_t *c = &model.currencies[index];
    if (prefetch_attempts[index] < MULTI_PREFETCH_ATTEMPTS) prefetch_attempts[index]++;
    retry_after[index] = now + 1000U * prefetch_attempts[index];
    c->denom_count = 0;
    if (protocol_send(0x0b, payload, sizeof(payload)) < 0) {
        c->status = MULTI_DETAIL_INVALID; model.revision++; return false;
    }
    query.busy = true; c->status = MULTI_DETAIL_LOADING; model.revision++;
    return true;
}
static bool uniform(const uint8_t *f, uint8_t value)
{
    for (unsigned i = 4; i < 15; ++i) if (f[i] != value) return false;
    return true;
}
static bool decimal(const uint8_t *p, unsigned n, uint32_t *value)
{
    bool digit = false, padded = false;
    *value = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (p[i] >= '0' && p[i] <= '9') {
            if (padded) return false;
            *value = *value * 10 + p[i] - '0'; digit = true;
        } else if (p[i] == ' ' || p[i] == 0) { if (digit) padded = true; }
        else return false;
    }
    return digit;
}
void counting_multi_denom(const uint8_t *f, uint8_t len, uint32_t now)
{
    if (!query.busy || !f || len != 16 || f[3] != 0x0b) return;
    if (uniform(f, 0xff)) {
        if (!query.abandoned && query.generation == model.generation) {
            multi_currency_t *c = &model.currencies[query.index];
            /* The controller can finish a zero-valued catalog before its
             * counted breakdown is ready. Do not exhaust retries in <1s. */
            retry_after[query.index] = now + 1000U * prefetch_attempts[query.index];
            uint64_t amount = 0; uint32_t pcs = 0;
            for (unsigned i = 0; i < query.count; ++i) {
                pcs += query.items[i].pcs;
                amount += (uint64_t)query.items[i].value * query.items[i].pcs;
            }
            /* Cross-check only: never replace authoritative 0E totals. */
            if (query.expired) c->status = MULTI_DETAIL_TIMEOUT;
            else if (!query.started || query.invalid) c->status = MULTI_DETAIL_INVALID;
            else if (!query.count) c->status = MULTI_DETAIL_EMPTY;
            else if (pcs != c->pcs || amount != c->amount) c->status = MULTI_DETAIL_INVALID;
            else {
                c->status = MULTI_DETAIL_READY; c->denom_count = query.count;
                memcpy(c->denom, query.items, sizeof(c->denom));
                for (unsigned i = 1; i < c->denom_count; ++i)
                    for (unsigned j = i; j > 0 && c->denom[j].value > c->denom[j-1].value; --j) {
                        multi_denom_t tmp = c->denom[j]; c->denom[j] = c->denom[j-1]; c->denom[j-1] = tmp;
                    }
            }
        }
        memset(&query, 0, sizeof(query));
        prefetch_after = now + MULTI_SETTLE_MS;
        model.revision++; return;
    }
    if (query.abandoned || query.expired) return;
    query.activity = now;
    if (uniform(f, 0)) {
        /* Repeated empty start delimiters are harmless; restarting after data
         * would merge two streams, so retain the consistency failure then. */
        if (query.started && query.count) query.invalid = true;
        query.started = true; return;
    }
    if (!query.started) { query.invalid = true; return; }
    uint32_t value, pcs;
    if (!decimal(f + 4, 8, &value) || !decimal(f + 12, 3, &pcs) || !value) {
        query.invalid = true; return;
    }
    for (unsigned i = 0; i < query.count; ++i) if (query.items[i].value == value) {
        /* Identical retransmissions are idempotent; conflicting duplicates fail. */
        if (query.items[i].pcs != pcs) query.invalid = true;
        return;
    }
    if (query.count == COUNTING_MULTI_DENOMS) { query.invalid = true; return; }
    query.items[query.count++] = (multi_denom_t){value, (uint16_t)pcs};
}
void counting_multi_poll(uint32_t now)
{
    if (!query.busy || query.expired) return;
    /* Poll may have sampled its tick just before the UI sent this request. */
    if ((int32_t)(now - query.activity) < 2500 && (int32_t)(now - query.sent) < 30000) return;
    query.expired = true;
    if (!query.abandoned && query.generation == model.generation)
        model.currencies[query.index].status = MULTI_DETAIL_TIMEOUT;
    model.revision++;
}

void counting_multi_prefetch(uint32_t now)
{
    if (!prefetch_pending || model.counting || query.busy) return;
    /* The end-of-count notification precedes controller detail preparation.
     * Wait before the first query, then retry only fully closed failed replies.
     * A missing FF still quarantines ownership: no blind timeout reassignment. */
    if (prefetch_settling) {
        prefetch_settling = false;
        prefetch_after = now + MULTI_SETTLE_MS;
        return;
    }
    if ((int32_t)(now - prefetch_after) < 0) return;
    for (unsigned i = 0; i < model.count; ++i) {
        if (model.currencies[i].status == MULTI_DETAIL_NONE) {
            counting_multi_request(i, now);
            prefetch_after = now + MULTI_SETTLE_MS;
            return;
        }
    }
    bool waiting_retry = false;
    for (unsigned i = 0; i < model.count; ++i) {
        multi_currency_t *c = &model.currencies[i];
        if (c->status != MULTI_DETAIL_READY &&
            !(c->status == MULTI_DETAIL_EMPTY && !c->pcs && !c->amount) &&
            prefetch_attempts[i] < MULTI_PREFETCH_ATTEMPTS) {
            waiting_retry = true;
            if ((int32_t)(now - retry_after[i]) < 0) continue;
            counting_multi_request(i, now);
            prefetch_after = now + MULTI_SETTLE_MS;
            return;
        }
    }
    prefetch_pending = waiting_retry;
}
