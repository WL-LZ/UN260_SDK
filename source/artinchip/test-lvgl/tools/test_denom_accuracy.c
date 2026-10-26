/* Controller 0B rows own A; 0E owns totals and completion. 49 owns B only.
 * The runtime INFO handler is production code; only UI/storage effects are
 * stubbed. Every 0B START clears quantities/amounts while retaining the
 * denomination catalogue; ADD totals are supplied entirely by the controller. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"

static counting_sim_t data;
static counting_session_state_t session;
static counting_detail_state_t detail;
static bool add_enabled, locked;
static unsigned restarts, row_changes;
static uint32_t tick;
static void changed(void) { ++row_changes; }
static const counting_denom_reply_hooks_t hooks = {.on_main_data_changed = changed};
static int island_pcs;
static double island_amount;
uint32_t app_clock_uptime_ms(void) { return ++tick; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t command, const uint8_t *payload, uint16_t length)
{ (void)command; (void)payload; (void)length; return 0; }
void counting_history_current_invalidate(void) {}
static void counting_history_restart_group(void) { ++restarts; }
static void page_01_main_set_counting_locked(bool value) { locked = value; }
static void smart_island_update_counting(int pcs, double amount)
{ island_amount = amount; island_pcs = pcs; }
#define ui_history_total_notes_counted_get() 0
#define ui_page_dynamic_capture_on_count_info(...) ((void)0)
#define currency_state_restore_detected_result() false
#define page_01_curr_img_refre() ((void)0)
#define smart_island_refresh_summary() ((void)0)
#define app_counting_runtime_refresh_compact(...) ((void)0)
#define counting_history_append_frame(...) ((void)0)
#define page_02_list_section_mark_dirty(...) ((void)0)
#define counting_history_capture_end(...) ((void)0)
#define machine_state_add_enabled() add_enabled
#define smart_island_set_count_analysis(...) ((void)0)
#define smart_island_notify_count_end(...) ((void)0)
#define app_counting_runtime_try_history_commit(...) ((void)0)
#define app_counting_runtime_main_page_active() false
#define ui_refresh_main_page() ((void)0)
#define app_auto_qr_on_end(...) ((void)0)
#define lv_tick_get() app_clock_uptime_ms()
#define lv_snprintf snprintf
#include "accuracy_runtime_info.inc"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s; A10=%u A50=%u A100=%u total=%d/%.0f last=%d restarts=%u\n", \
        __func__, __LINE__, #expr, quantity(10), quantity(50), quantity(100), \
        data.total_pcs, data.total_amount, data.last_total_pcs, restarts); return false; \
} } while (0)

static unsigned quantity(unsigned value)
{
    for (unsigned i = 0; i < data.denom_number; ++i)
        if (data.denom[i].value == (int)value) return data.denom[i].pcs;
    return 0;
}
static unsigned counted(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < data.denom_number; ++i) n += data.denom[i].pcs;
    return n;
}
static uint64_t money(void)
{
    uint64_t amount = 0;
    for (unsigned i = 0; i < data.denom_number; ++i)
        amount += counting_data_denom_amount(&data.denom[i]);
    return amount;
}
static void reset(void)
{
    counting_report_shutdown();
    counting_data_clear_serials(&data);
    counting_data_clear_errors(&data);
    memset(&data, 0, sizeof(data));
    memset(&session, 0, sizeof(session));
    memset(&detail, 0, sizeof(detail));
    data.denom_number = 3;
    data.denom[0].value = 10; data.denom[1].value = 50; data.denom[2].value = 100;
    add_enabled = locked = false; restarts = row_changes = 0; tick = 100; island_pcs = -1;
}
static void begin(bool add)
{
    add_enabled = add;
    counting_report_begin(add, &data);
    /* Same successful START state as counting_control_reply.c. */
    session.phase = COUNTING_SESSION_IDLE;
    session.start_confirmed = true;
    session.end_anim_wait_detail = false;
    session.expected_issue = 0;
    counting_denom_reply_begin_count(&detail);
    detail.live_reject_started = false;
    locked = true;
}
static void info(unsigned amount, unsigned pcs, unsigned status)
{
    uint8_t b[13] = {0xFD, 0xDF, 13, 0x0E};
    b[4] = amount >> 24; b[5] = amount >> 16; b[6] = amount >> 8; b[7] = amount;
    b[8] = pcs >> 8; b[9] = pcs; b[11] = status;
    app_counting_runtime_handle_info(&session, &data, b, sizeof(b));
}
static void note(unsigned value, const char *serial)
{
    uint8_t b[24] = {0xFD, 0xDF, 24, 0x49};
    char fields[24];
    snprintf(fields, sizeof(fields), "%7u%-12.12s", value, serial ? serial : "");
    memcpy(b + 4, fields, 19);
    counting_reject_sn_reply_dispatch(0x49, &detail, &session, &data, b, sizeof(b), NULL);
}
static void boundary(unsigned fill)
{
    uint8_t b[16] = {0xFD, 0xDF, 16, 0x0B};
    memset(b + 4, fill, 11);
    counting_denom_reply_handle(&detail, &session, &data, b, sizeof(b), &hooks);
}
static void row(unsigned value, unsigned pcs)
{
    uint8_t b[16] = {0xFD, 0xDF, 16, 0x0B};
    char fields[16];
    snprintf(fields, sizeof(fields), "%8u%3u", value, pcs);
    memcpy(b + 4, fields, 11);
    counting_denom_reply_handle(&detail, &session, &data, b, sizeof(b), &hooks);
}
static void rows(unsigned ten, unsigned fifty, unsigned hundred)
{ row(10, ten); row(50, fifty); row(100, hundred); }
static bool exact(unsigned ten, unsigned fifty, unsigned hundred)
{
    CHECK(quantity(10) == ten && quantity(50) == fifty && quantity(100) == hundred);
    CHECK(counted() == ten + fifty + hundred);
    CHECK(money() == 10 * ten + 50 * fifty + 100 * hundred);
    for (unsigned i = 0; i < data.denom_number; ++i)
        CHECK(counting_data_denom_amount(&data.denom[i]) ==
              (uint64_t)(unsigned)data.denom[i].value * data.denom[i].pcs);
    return true;
}


static bool absolute_rows(void)
{
    begin(false); boundary(0); row(100, 1); CHECK(exact(0, 0, 1));
    row(100, 2); CHECK(exact(0, 0, 2)); row(100, 3); CHECK(exact(0, 0, 3));
    unsigned changes = row_changes; row(100, 3); CHECK(row_changes == changes);
    row(100, 1); CHECK(exact(0, 0, 1)); row(100, 0); CHECK(exact(0, 0, 0));
    CHECK(data.denom_number == 3 && data.denom[2].value == 100);
    CHECK(data.total_pcs == 0 && data.total_amount == 0); boundary(255);
    return true;
}
static bool serials_do_not_write_a(void)
{
    begin(false); boundary(0); row(50, 2);
    note(100, "SAME12345678"); note(100, "SAME12345678"); note(10, "");
    CHECK(exact(0, 2, 0)); CHECK(data.sn_capacity >= 3);
    CHECK(data.sn_str[0] && data.sn_str[1] && !data.sn_str[2]);
    CHECK(!strcmp(data.sn_str[0], data.sn_str[1]));
    CHECK(data.denom_mix[0] == 100 && data.denom_mix[1] == 100);
    info(100, 2, 1); info(0, 0, 2); boundary(255);
    note(100, "AFTER-END"); CHECK(exact(0, 2, 0)); return true;
}
static bool start_before_first_info(void)
{
    begin(false); boundary(0); rows(1, 2, 0); CHECK(exact(1, 2, 0));
    info(0, 0, 0); CHECK(exact(1, 2, 0)); info(110, 3, 1);
    CHECK(exact(1, 2, 0)); info(0, 0, 2); boundary(255);
    CHECK(!locked && island_pcs == 3); return true;
}
static bool count_first(void)
{
    begin(false); boundary(0); info(0, 0, 1);
    info(0, 1, 1); row(50, 1); CHECK(exact(0, 1, 0));
    CHECK(data.total_pcs == 1 && data.total_amount == 0);
    info(50, 1, 1); info(50, 2, 1); row(10, 1); CHECK(exact(1, 1, 0));
    CHECK(data.total_pcs == 2 && data.total_amount == 50);
    info(60, 2, 1); CHECK(exact(1, 1, 0)); info(0, 0, 2); boundary(255);
    CHECK(session.last_result.amount == 60 && session.history_record.amount == 60);
    return true;
}
static bool amount_first(void)
{
    begin(false); boundary(0); info(50, 0, 1); row(50, 1);
    CHECK(exact(0, 1, 0) && data.total_pcs == 0 && data.total_amount == 50);
    info(50, 1, 1); info(100, 1, 1); row(50, 2); CHECK(exact(0, 2, 0));
    info(100, 2, 1); row(10, 1); info(110, 2, 1); info(110, 3, 1);
    info(0, 0, 2); boundary(255);
    CHECK(exact(1, 2, 0) && island_pcs == 3 && island_amount == 110);
    return true;
}
static bool positive_stop_final(void)
{
    begin(false); boundary(0); row(50, 2); info(50, 1, 1);
    info(100, 2, 2); CHECK(!locked && island_pcs == 2 && island_amount == 100);
    CHECK(session.history_record.pcs == 2 && session.history_record.amount == 100);
    boundary(255); CHECK(exact(0, 2, 0)); return true;
}
static bool zero_stop_retains_live(void)
{
    begin(false); boundary(0); row(100, 3); info(60, 2, 1); info(0, 0, 2);
    CHECK(!locked && island_pcs == 2 && island_amount == 60);
    boundary(255); CHECK(exact(0, 0, 3));
    CHECK(data.total_pcs == 2 && data.total_amount == 60);
    CHECK(session.last_result.pcs == 2 && session.last_result.amount == 60);
    CHECK(session.history_record.pcs == 2 && session.history_record.amount == 60);
    return true;
}
static bool no_live_positive_stop(void)
{
    begin(false); boundary(0); rows(2, 2, 0); info(120, 4, 2); boundary(255);
    CHECK(exact(2, 2, 0) && !locked && island_pcs == 4);
    CHECK(session.history_record.pcs == 4 && session.history_record.amount == 120);
    return true;
}
static bool empty_cnt(void)
{
    begin(false); boundary(0); info(0, 317, 1); info(0, 0, 2); boundary(255);
    CHECK(exact(0, 0, 0) && data.total_pcs == 317 && data.total_amount == 0);
    CHECK(!locked && island_pcs == 317 && session.history_record.pcs == 317);
    return true;
}
static bool start_clears_rows_for_both_add_states(void)
{
    for (unsigned add = 0; add < 2; ++add) {
        reset(); data.denom[0].pcs = 2; data.denom[0].amount = 20;
        begin(add != 0); CHECK(exact(2, 0, 0)); boundary(0);
        CHECK(exact(0, 0, 0));
        CHECK(data.denom_number == 3 && data.denom[0].value == 10);
        row(50, 1); CHECK(exact(0, 1, 0)); boundary(255);
    }
    return true;
}
static bool add_continue(void)
{
    begin(false); boundary(0); rows(1, 1, 0);
    info(60, 2, 1); info(0, 0, 2); boundary(255);
    begin(true); boundary(0); CHECK(exact(0, 0, 0)); rows(1, 1, 0);
    info(0, 0, 1); CHECK(exact(1, 1, 0));
    info(60, 2, 1); rows(2, 2, 0); info(120, 4, 1);
    CHECK(exact(2, 2, 0) && restarts == 0);
    info(0, 0, 2); boundary(255);
    CHECK(!locked && island_pcs == 2 && session.analysis_valid_pcs == 2);
    CHECK(data.last_total_pcs == 2 && session.history_record.pcs == 4);
    return true;
}
static bool empty_add(void)
{
    begin(false); boundary(0); rows(1, 1, 0);
    info(60, 2, 1); info(0, 0, 2); boundary(255);
    begin(true); boundary(0); rows(1, 1, 0);
    info(0, 0, 1); info(0, 0, 1); info(0, 0, 2); boundary(255);
    CHECK(exact(1, 1, 0) && restarts == 0 && island_pcs == 0 && !locked);
    return true;
}
static bool restart_a_is_independent(void)
{
    begin(false); boundary(0); rows(0, 2, 0);
    info(100, 2, 1); info(0, 0, 2); boundary(255);
    begin(true); boundary(0); rows(1, 2, 0); info(0, 0, 1);
    CHECK(exact(1, 2, 0)); info(10, 1, 1);
    CHECK(restarts == 1 && data.last_total_pcs == 0 && exact(1, 2, 0));
    /* The controller, not the scalar restart, clears the old denomination. */
    row(50, 0); CHECK(exact(1, 0, 0)); info(10, 1, 1);
    CHECK(restarts == 1 && !counting_report_take_group_restart());
    info(0, 0, 2); boundary(255); CHECK(!locked && island_pcs == 1);
    return true;
}
static bool a_cannot_guess_group_reset(void)
{
    begin(false); boundary(0); rows(0, 1, 0);
    info(50, 1, 1); info(0, 0, 2); boundary(255);
    begin(true); boundary(0); rows(1, 0, 0); info(10, 1, 1);
    info(0, 0, 2); boundary(255);
    CHECK(exact(1, 0, 0) && data.total_pcs == 1 && data.total_amount == 10);
    CHECK(restarts == 0 && data.last_total_pcs == 1);
    /* Equal counts cannot identify a reset; A is never repurposed as a reset
     * signal. This is a protocol ambiguity boundary, not reset detection. */
    puts("BOUNDARY equal first total has no reset evidence; no A-based group inference");
    return true;
}
static bool non_add_clear_after_add(void)
{
    begin(true); boundary(0); rows(1, 2, 0); info(110, 3, 1);
    info(0, 0, 2); boundary(255); begin(false);
    CHECK(exact(1, 2, 0)); boundary(0); CHECK(exact(0, 0, 0));
    CHECK(data.denom_number == 3); row(10, 1); info(10, 1, 1);
    info(0, 0, 2); boundary(255); CHECK(exact(1, 0, 0) && island_pcs == 1);
    return true;
}
static bool end_order_and_late_frames(void)
{
    begin(false); boundary(0); row(50, 2); boundary(255);
    CHECK(locked && exact(0, 2, 0)); row(100, 3); note(100, "LATE");
    CHECK(exact(0, 2, 0)); info(100, 2, 1); info(0, 0, 2);
    CHECK(!locked && island_pcs == 2 && exact(0, 2, 0));
    boundary(255); CHECK(session.history_record.amount == 100); return true;
}
static void detail_marker(unsigned command, unsigned fill)
{
    unsigned len = command == 0x0C ? 7 : 25;
    uint8_t b[25] = {0xFD, 0xDF}; b[2] = len; b[3] = command;
    memset(b + 4, fill, len - 5);
    counting_reject_sn_reply_dispatch(command, &detail, &session, &data, b, len, NULL);
}
static void serial_note(unsigned no, unsigned value, const char *serial)
{
    uint8_t b[25] = {0xFD, 0xDF, 25, 0x0D}; char text[24]; b[4] = no;
    snprintf(text, sizeof(text), "%7u%-12.12s", value, serial);
    memcpy(b + 5, text, 19);
    counting_reject_sn_reply_dispatch(0x0D, &detail, &session, &data, b, sizeof(b), NULL);
}
static void seed_tail_empty(void)
{
    begin(false); boundary(0); rows(2, 1, 0);
    note(50, "OLD000000001"); note(10, ""); note(10, "");
    info(70, 3, 1); info(0, 0, 2); boundary(255);
    detail_marker(0x0C, 0); detail_marker(0x0C, 255); detail_marker(0x0D, 0);
    serial_note(1, 50, "OLD000000001"); serial_note(2, 10, ""); serial_note(3, 10, "");
    detail_marker(0x0D, 255);
}
static bool add_tail_empty(void)
{
    seed_tail_empty(); CHECK(exact(2, 1, 0)); CHECK(data.sn_str && data.sn_str[0]);
    begin(true); boundary(0); info(0, 0, 1); note(50, "NEW000000001");
    CHECK(exact(0, 0, 0)); rows(2, 2, 0); info(120, 4, 1);
    CHECK(exact(2, 2, 0) && data.sn_capacity >= 4);
    CHECK(!strcmp(data.sn_str[0], "OLD000000001") && !data.sn_str[1] && !data.sn_str[2]);
    CHECK(data.sn_str[3] && !strcmp(data.sn_str[3], "NEW000000001"));
    info(0, 0, 2); boundary(255); CHECK(restarts == 0 && island_pcs == 1); return true;
}
static bool restart_before_serial(void)
{
    seed_tail_empty(); begin(true); boundary(0); rows(1, 1, 0);
    info(0, 0, 1); info(60, 2, 1); note(50, ""); note(10, "NEW000000001");
    CHECK(exact(1, 1, 0) && restarts == 1 && data.last_total_pcs == 0);
    CHECK(data.sn_str && !data.sn_str[0] && data.sn_str[1]);
    CHECK(!strcmp(data.sn_str[1], "NEW000000001") && data.denom_mix[1] == 10);
    info(0, 0, 2); boundary(255); CHECK(island_pcs == 2); return true;
}
static bool restart_keeps_current_serials(void)
{
    seed_tail_empty(); begin(true); boundary(0); rows(1, 1, 0);
    info(0, 0, 1); note(50, ""); note(10, "NEW000000001"); info(60, 2, 1);
    CHECK(exact(1, 1, 0) && restarts == 1 && data.last_total_pcs == 0);
    CHECK(data.sn_str && !data.sn_str[0] && data.sn_str[1]);
    CHECK(!strcmp(data.sn_str[1], "NEW000000001") && data.denom_mix[1] == 10);
    CHECK(counting_data_serial_valid_count(&data) == 1);
    info(0, 0, 2); boundary(255); CHECK(exact(1, 1, 0) && island_pcs == 2);
    return true;
}
static bool wire_amount_limits(void)
{
    const uint32_t values[] = {UINT32_MAX, UINT32_MAX - 1u};
    const char *headers[] = {"4,294,967,295", "4,294,967,294"};
    for (unsigned i = 0; i < 2; ++i) {
        reset(); begin(false); boundary(0); row(50, 1); info(values[i], 1, 1);
        CHECK(data.total_amount == (double)values[i]); char header[32];
        app_counting_runtime_format_amount(header, sizeof(header), data.total_amount);
        CHECK(!strcmp(header, headers[i])); info(0, 0, 2); boundary(255);
        CHECK(session.last_result.amount == (double)values[i]);
        CHECK(session.history_record.amount == (double)values[i]);
        CHECK(island_amount == (double)values[i]);
        begin(true); boundary(0); info(0, 0, 1);
        CHECK(data.total_amount == (double)values[i] && data.last_total_amount == (double)values[i]);
        info(0, 0, 2); boundary(255); CHECK(island_amount == (double)values[i]);
    }
    return true;
}
static bool wire_stop_amount_limits(void)
{
    const uint32_t values[] = {UINT32_MAX, UINT32_MAX - 1u};
    for (unsigned i = 0; i < 2; ++i) {
        reset(); begin(false); boundary(0); info(values[i] - 1u, 1, 1);
        info(values[i], 1, 2); boundary(255);
        CHECK(data.total_amount == (double)values[i]);
        CHECK(session.last_result.amount == (double)values[i]);
        CHECK(session.history_record.amount == (double)values[i]);
        CHECK(island_amount == (double)values[i]);
    }
    return true;
}
static bool row_field_limits(void)
{
    begin(false); boundary(0); row(99999999, 999);
    CHECK(data.denom_number == 4 && quantity(99999999) == 999);
    CHECK(counting_data_denom_amount(&data.denom[3]) == UINT64_C(99899999001));
    row(99999999, 0); CHECK(counting_data_denom_amount(&data.denom[3]) == 0);
    row(50000, 999); CHECK(money() == UINT64_C(49950000)); return true;
}
static uint32_t random_state;
static unsigned next_random(void)
{ random_state = random_state * 1664525u + 1013904223u; return random_state; }
static bool randomized(void)
{
    unsigned checked_rows = 0;
    const unsigned values[] = {10, 50, 100};
    for (unsigned seed = 1; seed <= 80; ++seed) {
        reset(); random_state = seed; unsigned q[3] = {0};
        for (unsigned run = 0; run < 5; ++run) {
            begin(run != 0); boundary(0);
            memset(q, 0, sizeof(q));
            CHECK(exact(q[0], q[1], q[2]));
            /* Controller rows alone establish this run; no local ADD base. */
            rows(q[0], q[1], q[2]); info(0, 0, 1);
            unsigned n = 20 + next_random() % 50;
            for (unsigned i = 0; i < n; ++i) {
                unsigned slot = next_random() % 3, previous_pcs = counted();
                unsigned previous_amount = (unsigned)money();
                q[slot] = next_random() % 1000;
                unsigned pcs = q[0] + q[1] + q[2], amount = 10*q[0] + 50*q[1] + 100*q[2];
                switch (next_random() % 4) {
                case 0: info(previous_amount, pcs, 1); row(values[slot], q[slot]); info(amount, pcs, 1); break;
                case 1: info(amount, previous_pcs, 1); row(values[slot], q[slot]); info(amount, pcs, 1); break;
                case 2: row(values[slot], q[slot]); break;
                default: row(values[slot], q[slot]); row(values[slot], q[slot]); info(amount, pcs, 1); break;
                }
                ++checked_rows;
                CHECK(exact(q[0], q[1], q[2]));
            }
            unsigned pcs = counted(), amount = (unsigned)money();
            info(amount, pcs, 1); info(amount, pcs, 2); boundary(255);
            CHECK(!locked && exact(q[0], q[1], q[2]));
            CHECK(data.total_pcs == (int)pcs && data.total_amount == amount);
        }
    }
    printf("PROPERTY 80 seeds x 5 runs: %u absolute row updates, every A prefix exact\n", checked_rows);
    return true;
}
struct test_case { const char *name; bool (*run)(void); };
static const struct test_case cases[] = {
    {"absolute_rows", absolute_rows}, {"serials_do_not_write_a", serials_do_not_write_a},
    {"start_before_first_info", start_before_first_info},
    {"count_first", count_first}, {"amount_first", amount_first},
    {"positive_stop_final", positive_stop_final}, {"zero_stop_retains_live", zero_stop_retains_live},
    {"no_live_positive_stop", no_live_positive_stop}, {"empty_cnt", empty_cnt},
    {"start_clears_rows_for_both_add_states", start_clears_rows_for_both_add_states}, {"add_continue", add_continue},
    {"empty_add", empty_add}, {"restart_a_is_independent", restart_a_is_independent},
    {"a_cannot_guess_group_reset", a_cannot_guess_group_reset},
    {"non_add_clear_after_add", non_add_clear_after_add},
    {"end_order_and_late_frames", end_order_and_late_frames},
    {"add_tail_empty", add_tail_empty}, {"restart_before_serial", restart_before_serial},
    {"restart_keeps_current_serials", restart_keeps_current_serials},
    {"wire_amount_limits", wire_amount_limits}, {"wire_stop_amount_limits", wire_stop_amount_limits},
    {"row_field_limits", row_field_limits}, {"randomized", randomized},
};
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        if (strcmp(argv[1], cases[i].name)) continue;
        reset(); bool ok = cases[i].run();
        counting_report_shutdown(); counting_data_clear_serials(&data); counting_data_clear_errors(&data);
        if (ok) printf("PASS %s\n", cases[i].name);
        return ok ? 0 : 1;
    }
    return 2;
}
