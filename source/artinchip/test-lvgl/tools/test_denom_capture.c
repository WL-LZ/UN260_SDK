/* External UART capture replay: no private log content is stored here. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"

static counting_sim_t data;
static counting_session_state_t session;
static counting_detail_state_t detail;
static denom_t expected[COUNTING_DENOM_MAX_ITEMS], stopped[COUNTING_DENOM_MAX_ITEMS];
static unsigned expected_count, stopped_count, run_id, run_line, failures;
static unsigned checked, excluded, counted, cnt_runs, clear_count, restart_count;
static unsigned expected_pcs, expected_amount, note_count, serial_completions;
static unsigned total_frames, stop_count, final_count;
static bool add, skip, have_stop, have_final;
static uint32_t now;

uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *fmt, ...) { (void)fmt; }
int protocol_send(uint8_t cmd, const uint8_t *payload, uint16_t len)
{ (void)cmd; assert(payload && len); return 0; }
bool counting_history_discard_pending(counting_session_state_t *state)
{ (void)state; return false; }

static unsigned pcs_for(const denom_t *rows, unsigned count, int value)
{
    unsigned pcs = 0;
    for (unsigned i = 0; i < count; ++i) if (rows[i].value == value) pcs += rows[i].pcs;
    return pcs;
}

static bool same_table(const denom_t *rows, unsigned count, const char *where)
{
    bool equal = true;
    for (unsigned side = 0; side < 2; ++side) {
        const denom_t *keys = side ? rows : expected;
        unsigned key_count = side ? count : expected_count;
        for (unsigned i = 0; i < key_count; ++i) {
            unsigned want = pcs_for(expected, expected_count, keys[i].value);
            unsigned got = pcs_for(rows, count, keys[i].value);
            if (want != got) {
                fprintf(stderr, "FAIL RUN %u line %u %s denomination=%d expected=%u actual=%u\n",
                        run_id, run_line, where, keys[i].value, want, got);
                equal = false;
            }
        }
    }
    return equal;
}

static void require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL RUN %u line %u %s\n", run_id, run_line, message);
        ++failures;
    }
}

static void consume_restart(void)
{
    if (counting_report_take_group_restart()) ++restart_count;
    require(restart_count <= 1, "more than one history group restart notification");
}

static void history_ready(void *context)
{
    (void)context;
    ++serial_completions;
    if (run_id && !skip && have_final)
        require(same_table(data.denom, data.denom_number, "at history-ready"),
                "A table changed before history completion");
}

static void clear_result(void)
{
    counting_report_reset();
    counting_data_clear_serials(&data);
    counting_data_clear_errors(&data);
    denom_t catalogue[COUNTING_DENOM_MAX_ITEMS];
    unsigned count = data.denom_number;
    memcpy(catalogue, data.denom, sizeof(catalogue));
    memset(&data, 0, sizeof(data));
    data.denom_number = count;
    for (unsigned i = 0; i < count; ++i) data.denom[i].value = catalogue[i].value;
    memset(&session, 0, sizeof(session));
    memset(&detail, 0, sizeof(detail));
    ++clear_count;
}

static bool payload_is(const uint8_t *b, unsigned len, uint8_t value)
{
    for (unsigned i = 4; i + 1 < len; ++i) if (b[i] != value) return false;
    return true;
}

static void finish_run(void)
{
    if (!run_id) return;
    if (skip) {
        ++excluded;
        printf("EXCLUDED RUN %u: initial retained baseline absent or incomplete STOP/0B; complete 0B is used only as next baseline\n", run_id);
    } else {
        require(have_stop, "missing accepted STOP");
        require(have_final, "missing complete final 0B");
        ++checked;
    }
}

static void receive(uint8_t *b, unsigned n, unsigned line)
{
    ++now; ++total_frames;
    if (b[3] == 0x3B && n == 6 && b[4] == 1) {
        clear_result();
        return;
    }
    if (!run_id) return;
    if (b[3] == 0x0A) {
        if (n == 7 && b[4] == 1 && b[5] == 1) {
            counting_report_begin(add, &data);
            detail.live_denom_started = detail.live_reject_started = false;
        }
        counting_control_reply_dispatch(b[3], &session, b, n, NULL);
        return;
    }
    if (b[3] == 0x0E && n == 13) {
        counting_info_reply_result_t result = counting_info_reply_handle(&session, &data, b, n, 0);
        if (result.kind == COUNTING_INFO_REPLY_LIVE) {
            counting_report_observe_total(&data,
                ((uint32_t)b[4] << 24) | ((uint32_t)b[5] << 16) | ((uint32_t)b[6] << 8) | b[7],
                ((unsigned)b[8] << 8) | b[9]);
            consume_restart();
        } else if (result.kind == COUNTING_INFO_REPLY_FINISHED) {
            have_stop = true; ++stop_count;
            stopped_count = data.denom_number;
            memcpy(stopped, data.denom, sizeof(stopped));
            if (!skip) {
                require(result.final_pcs == (int)expected_pcs, "STOP total pieces differs from capture");
                require(result.final_amount == (double)expected_amount, "STOP amount differs from capture");
            }
        }
        return;
    }
    if (b[3] == 0x0B && n == 16) {
        counting_denom_reply_result_t result = counting_denom_reply_handle(&detail, &session, &data, b, n, NULL);
        consume_restart();
        if (have_stop && payload_is(b, n, 255) && result == COUNTING_DENOM_REPLY_SESSION_END) {
            have_final = true; ++final_count;
            if (!skip) {
                bool cnt = note_count == 0 && expected_amount == 0;
                if (cnt) {
                    ++cnt_runs;
                    unsigned monetary_notes = 0;
                    for (unsigned i = 0; i < expected_count; ++i) monetary_notes += expected[i].pcs;
                    require(monetary_notes == 0, "CNT final 0B unexpectedly contains monetary counts");
                } else {
                    ++counted;
                    require(same_table(stopped, stopped_count, "before final 0B"), "live A differs from final controller table");
                }
                require(same_table(data.denom, data.denom_number, "after final 0B"), "published A differs from final controller table");
                printf("CHECK RUN %u line %u ADD=%u notes49=%u total=%u amount=%u restart=%u %s\n",
                       run_id, line, add, note_count, expected_pcs, expected_amount, restart_count,
                       cnt ? "CNT totals checked; no denomination stream" : "live/final denomination comparison completed");
            }
        }
        return;
    }
    if (b[3] == 0x49 || b[3] == 0x0C || b[3] == 0x0D) {
        counting_reject_sn_reply_hooks_t hooks = {.on_history_record_ready = history_ready};
        counting_detail_reply_result_t result = counting_reject_sn_reply_dispatch(
            b[3], &detail, &session, &data, b, n, &hooks);
        if (b[3] == 0x49 && n == 24 && result == COUNTING_DETAIL_REPLY_DATA) {
            ++note_count;
            /* In this captured, complete note stream, no intermediate row
             * may exceed its final controller count (including zero rows). */
            if (!skip) for (unsigned i = 0; i < data.denom_number; ++i)
                require(data.denom[i].pcs <= pcs_for(expected, expected_count, data.denom[i].value),
                        "intermediate A contains notes absent from final controller table");
        }
        consume_restart();
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *input = fopen(argv[1], "r"); assert(input);
    char line[2048];
    while (fgets(line, sizeof(line), input)) {
        if (line[0] == 'R') {
            finish_run();
            unsigned add_value, skip_value;
            assert(sscanf(line, "R %u %u %u %u %u %u", &run_id, &add_value, &skip_value,
                          &expected_pcs, &expected_amount, &run_line) == 6);
            add = add_value != 0; skip = skip_value != 0;
            expected_count = stopped_count = restart_count = note_count = 0;
            have_stop = have_final = false;
            memset(expected, 0, sizeof(expected));
        } else if (line[0] == 'X') {
            unsigned value, pcs;
            assert(sscanf(line, "X %u %u", &value, &pcs) == 2);
            assert(expected_count < COUNTING_DENOM_MAX_ITEMS && pcs <= UINT16_MAX);
            expected[expected_count++] = (denom_t){.value = (int)value, .pcs = pcs, .amount = (float)value * pcs};
        } else if (line[0] == 'F') {
            unsigned source_line; int used;
            assert(sscanf(line, "F %u %n", &source_line, &used) == 1);
            char *p = line + used; uint8_t b[256]; unsigned n = 0, value;
            while (sscanf(p, " %x%n", &value, &used) == 1) {
                assert(n < sizeof(b) && value <= 255); b[n++] = value; p += used;
            }
            assert(n >= 5 && n == b[2]); receive(b, n, source_line);
        }
    }
    finish_run(); fclose(input);
    counting_report_shutdown(); counting_data_clear_serials(&data); counting_data_clear_errors(&data);
    require(checked > 0, "no complete captured runs checked");
    printf("%s capture replay: runs=%u excluded=%u monetary=%u CNT=%u clear_acks=%u STOP=%u final0B=%u serial_completions=%u frames=%u failures=%u\n",
           failures ? "FAIL" : "PASS", checked, excluded, counted, cnt_runs, clear_count,
           stop_count, final_count, serial_completions, total_frames, failures);
    return failures ? 1 : 0;
}
void counting_history_current_invalidate(void) {}
