/*
 * Customer-result tests: every accepted 0x49 frame is one physical note,
 * including equal serial text and blank serial text. 0x0E never identifies a
 * denomination. The final 0x0B snapshot is authoritative, not an increment.
 *
 * The runner extracts the production runtime INFO handler into an include.
 * Only unrelated UI/history persistence effects are stubbed; all note, totals,
 * denomination parsing, ADD baseline and session state changes are production.
 */
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
static unsigned restarts;
static uint32_t tick;
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
static unsigned money(void)
{
    unsigned amount = 0;
    for (unsigned i = 0; i < data.denom_number; ++i)
        amount += (unsigned)data.denom[i].value * data.denom[i].pcs;
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
    add_enabled = locked = false; restarts = 0; tick = 100; island_pcs = -1;
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
    detail.live_denom_started = false;
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
    counting_denom_reply_handle(&detail, &session, &data, b, sizeof(b), NULL);
}
static void final_row(unsigned value, unsigned pcs)
{
    uint8_t b[16] = {0xFD, 0xDF, 16, 0x0B};
    char fields[16];
    snprintf(fields, sizeof(fields), "%8u%3u", value, pcs);
    memcpy(b + 4, fields, 11);
    counting_denom_reply_handle(&detail, &session, &data, b, sizeof(b), NULL);
}
static void snapshot(unsigned ten, unsigned fifty, unsigned hundred)
{
    boundary(0); final_row(10, ten); final_row(50, fifty); final_row(100, hundred); boundary(255);
    /* Inspect the report-layer event here; production consumes it before history save. */
    if (counting_report_take_group_restart()) counting_history_restart_group();
}
static bool exact(unsigned ten, unsigned fifty, unsigned hundred)
{
    CHECK(quantity(10) == ten && quantity(50) == fifty && quantity(100) == hundred);
    CHECK(counted() == ten + fifty + hundred);
    CHECK(money() == 10 * ten + 50 * fifty + 100 * hundred);
    for (unsigned i = 0; i < data.denom_number; ++i)
        CHECK(data.denom[i].amount == (float)data.denom[i].value * data.denom[i].pcs);
    return true;
}

static bool duplicate_note(void)
{
    begin(false); info(0, 0, 1);
    note(50, "SAME12345678"); note(50, "SAME12345678");
    CHECK(exact(0, 2, 0));
    info(100, 2, 1); info(100, 2, 1); CHECK(exact(0, 2, 0));
    info(0, 0, 2); CHECK(!locked); snapshot(0, 2, 0); CHECK(exact(0, 2, 0));
    return true;
}
static bool blank_serial(void)
{
    begin(false); note(50, ""); note(10, NULL); note(50, "            ");
    CHECK(exact(1, 2, 0)); info(110, 3, 1); info(0, 0, 2);
    snapshot(1, 2, 0); CHECK(exact(1, 2, 0)); return true;
}
static bool count_first(void)
{
    begin(false); info(0, 0, 1);
    note(50, "A"); info(0, 1, 1); CHECK(exact(0, 1, 0));
    info(50, 1, 1); note(10, "B"); info(50, 2, 1); info(60, 2, 1);
    CHECK(exact(1, 1, 0)); info(0, 0, 2); snapshot(1, 1, 0); CHECK(exact(1, 1, 0));
    return true;
}
static bool amount_first(void)
{
    begin(false); info(0, 0, 1); note(50, "A"); info(50, 0, 1); info(50, 1, 1);
    note(50, "B"); info(100, 1, 1); CHECK(exact(0, 2, 0)); info(100, 2, 1);
    note(10, "C"); info(110, 2, 1); info(110, 3, 1);
    CHECK(exact(1, 2, 0)); info(0, 0, 2); snapshot(1, 2, 0); CHECK(exact(1, 2, 0));
    return true;
}
static bool end_amount(void)
{
    begin(false); info(0, 0, 1); note(50, "A"); info(50, 1, 1);
    note(50, "B"); info(50, 2, 1); info(100, 2, 2);
    CHECK(!locked && exact(0, 2, 0));
    CHECK(session.history_record.pcs == 2 && session.history_record.amount == 100);
    snapshot(0, 2, 0); CHECK(exact(0, 2, 0));
    return true;
}
static bool missing_info(void)
{
    begin(false);
    note(10, "A"); note(50, "B"); note(50, "C"); note(10, "D");
    CHECK(exact(2, 2, 0)); info(120, 4, 2);
    CHECK(!locked); snapshot(2, 2, 0); CHECK(exact(2, 2, 0)); return true;
}
static bool add_continue(void)
{
    begin(false); info(0, 0, 1); note(10, "A"); note(50, "B");
    info(60, 2, 1); info(0, 0, 2); snapshot(1, 1, 0);
    begin(true); info(0, 0, 1); CHECK(exact(1, 1, 0));
    info(60, 2, 1); /* delayed unchanged cumulative total is not reset */
    note(50, "C"); info(110, 3, 1); note(10, "D"); info(120, 4, 1);
    CHECK(exact(2, 2, 0)); CHECK(restarts == 0);
    info(0, 0, 2); CHECK(island_pcs == 2 && !locked); snapshot(2, 2, 0);
    CHECK(exact(2, 2, 0)); return true;
}
static bool restart_before_note(void)
{
    begin(false); note(50, "A"); note(50, "B"); info(100, 2, 1);
    info(0, 0, 2); snapshot(0, 2, 0);
    begin(true); info(0, 0, 1); info(10, 1, 1); note(10, "C");
    CHECK(exact(1, 0, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    info(0, 0, 2); CHECK(island_pcs == 1); snapshot(1, 0, 0); CHECK(exact(1, 0, 0)); return true;
}
static bool restart_after_note(void)
{
    begin(false); note(50, "A"); note(50, "B"); info(100, 2, 1);
    info(0, 0, 2); snapshot(0, 2, 0);
    begin(true); info(0, 0, 1); note(10, "C"); info(10, 1, 1);
    CHECK(exact(1, 0, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    info(0, 0, 2); CHECK(island_pcs == 1); snapshot(1, 0, 0); CHECK(exact(1, 0, 0)); return true;
}
static bool restart_equal(void)
{
    begin(false); note(50, "A"); info(50, 1, 1); info(0, 0, 2); snapshot(0, 1, 0);
    begin(true); info(0, 0, 1); note(10, "B"); info(10, 1, 1);
    /* Equal first total cannot distinguish a delayed old baseline from reset.
     * The complete final snapshot resolves that ambiguity without blocking. */
    info(0, 0, 2); CHECK(!locked); snapshot(1, 0, 0);
    CHECK(exact(1, 0, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    snapshot(1, 0, 0); CHECK(restarts == 1 && !counting_report_take_group_restart());
    return true;
}
static bool empty_add(void)
{
    begin(false); note(10, "A"); note(50, "B"); info(60, 2, 1);
    info(0, 0, 2); snapshot(1, 1, 0);
    begin(true); info(0, 0, 1); info(0, 0, 1); info(0, 0, 2);
    CHECK(exact(1, 1, 0)); CHECK(restarts == 0 && island_pcs == 0 && !locked);
    snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); return true;
}
static bool final_replace(void)
{
    begin(false); note(10, "A"); info(60, 2, 1); info(0, 0, 2);
    snapshot(1, 1, 0); CHECK(exact(1, 1, 0));
    snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); /* duplicate whole snapshot */
    begin(true); note(50, "B"); info(110, 3, 1); CHECK(exact(1, 2, 0));
    info(0, 0, 2); snapshot(1, 2, 0); CHECK(exact(1, 2, 0)); return true;
}
static bool live_snapshot(void)
{
    begin(false); info(0, 0, 1); note(50, "A"); snapshot(0, 1, 0);
    info(50, 1, 1); CHECK(exact(0, 1, 0));
    note(10, "B"); info(60, 2, 1); CHECK(exact(1, 1, 0));
    info(0, 0, 2); snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); return true;
}
static bool non_add_after_add(void)
{
    begin(false); note(10, "A"); note(50, "B"); info(60, 2, 1); info(0, 0, 2); snapshot(1, 1, 0);
    begin(true); note(50, "C"); info(110, 3, 1); info(0, 0, 2); snapshot(1, 2, 0);
    begin(false); CHECK(exact(0, 0, 0)); CHECK(data.denom_number == 3);
    info(0, 0, 1); note(10, "D"); info(10, 1, 1); CHECK(exact(1, 0, 0));
    info(0, 0, 2); snapshot(1, 0, 0); CHECK(exact(1, 0, 0)); return true;
}
static bool restart_missing_first(void)
{
    begin(false); note(50, "A"); note(50, "B"); info(100, 2, 1);
    info(0, 0, 2); snapshot(0, 2, 0);
    begin(true); note(10, "C"); note(10, "D"); note(10, "E");
    info(30, 3, 1); info(0, 0, 2); snapshot(3, 0, 0);
    CHECK(exact(3, 0, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    snapshot(3, 0, 0); CHECK(restarts == 1 && !counting_report_take_group_restart());
    return true;
}
static bool restart_equal_amount(void)
{
    begin(false); note(50, "SAME"); info(50, 1, 1); info(0, 0, 2); snapshot(0, 1, 0);
    begin(true); note(50, "SAME"); info(50, 1, 1);
    info(0, 0, 2); snapshot(0, 1, 0);
    CHECK(exact(0, 1, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    snapshot(0, 1, 0); CHECK(restarts == 1 && !counting_report_take_group_restart());
    return true;
}
static bool late_note_after_end(void)
{
    begin(false); note(50, "A"); info(50, 1, 1); info(0, 0, 2); snapshot(0, 1, 0);
    note(100, "LATE"); CHECK(exact(0, 1, 0));
    CHECK(!counting_report_observe_note(&data, 100)); CHECK(exact(0, 1, 0));
    begin(true); note(10, "B"); info(60, 2, 1); CHECK(exact(1, 1, 0));
    info(0, 0, 2); snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); return true;
}
static bool end_count(void)
{
    begin(false); note(50, "A"); info(50, 1, 1); note(50, "B");
    info(100, 2, 2); CHECK(!locked && exact(0, 2, 0));
    CHECK(session.history_record.pcs == 2 && session.history_record.amount == 100);
    snapshot(0, 2, 0); CHECK(exact(0, 2, 0)); return true;
}
static bool zero_end_retains(void)
{
    begin(false); note(50, "A"); info(50, 1, 1); note(10, "B"); info(60, 2, 1);
    info(0, 0, 2); CHECK(!locked && exact(1, 1, 0));
    CHECK(session.history_record.pcs == 2 && session.history_record.amount == 60);
    snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); return true;
}
static bool blank_serial_order(void)
{
    begin(false); note(50, ""); note(10, "VALID0000001"); note(50, ""); note(10, "VALID0000002");
    CHECK(exact(2, 2, 0)); CHECK(data.sn_capacity >= 4);
    CHECK(!data.sn_str[0] && data.sn_str[1] && !data.sn_str[2] && data.sn_str[3]);
    CHECK(!strcmp(data.sn_str[1], "VALID0000001") && !strcmp(data.sn_str[3], "VALID0000002"));
    CHECK(data.denom_mix[1] == 10 && data.denom_mix[3] == 10);
    info(120, 4, 1); info(0, 0, 2); snapshot(2, 2, 0);
    CHECK(exact(2, 2, 0)); return true;
}
static bool all_amounts_ahead(void)
{
    begin(false); info(170, 0, 1);
    note(50, "A"); note(10, "B"); note(100, "C"); note(10, "D");
    CHECK(exact(2, 1, 1)); info(170, 4, 1); info(0, 0, 2);
    snapshot(2, 1, 1); CHECK(exact(2, 1, 1)); return true;
}
static bool all_counts_ahead(void)
{
    begin(false); info(0, 4, 1);
    note(50, "A"); note(10, "B"); note(100, "C"); note(10, "D");
    CHECK(exact(2, 1, 1)); info(170, 4, 1); info(0, 0, 2);
    snapshot(2, 1, 1); CHECK(exact(2, 1, 1)); return true;
}
static bool ten_thousand_notes(void)
{
    begin(false);
    for (unsigned i = 0; i < 10000; ++i) note(i % 2 ? 50 : 10, "");
    CHECK(exact(5000, 5000, 0)); info(300000, 10000, 1); info(0, 0, 2);
    CHECK(session.history_record.pcs == 10000 && session.history_record.amount == 300000);
    /* This boundary validates explicit-note accumulation, not unspecified
     * controller chunking for 0B's three-character count field. */
    return true;
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
    begin(false); note(50, "OLD000000001"); note(10, ""); note(10, "");
    info(70, 3, 1); info(0, 0, 2); snapshot(2, 1, 0);
    detail_marker(0x0C, 0); detail_marker(0x0C, 255);
    detail_marker(0x0D, 0);
    serial_note(1, 50, "OLD000000001"); serial_note(2, 10, ""); serial_note(3, 10, "");
    detail_marker(0x0D, 255);
}
static bool add_tail_empty(void)
{
    seed_tail_empty(); CHECK(exact(2, 1, 0)); CHECK(data.sn_str && data.sn_str[0]);
    begin(true); info(0, 0, 1); note(50, "NEW000000001"); info(120, 4, 1);
    CHECK(exact(2, 2, 0)); CHECK(data.sn_capacity >= 4);
    CHECK(!strcmp(data.sn_str[0], "OLD000000001") && !data.sn_str[1] && !data.sn_str[2]);
    CHECK(data.sn_str[3] && !strcmp(data.sn_str[3], "NEW000000001"));
    info(0, 0, 2); snapshot(2, 2, 0); CHECK(restarts == 0); return true;
}
static bool restart_keeps_current_serials(void)
{
    seed_tail_empty(); begin(true); info(0, 0, 1);
    note(50, ""); note(10, "NEW000000001"); info(60, 2, 1);
    CHECK(exact(1, 1, 0)); CHECK(restarts == 1 && data.last_total_pcs == 0);
    CHECK(data.sn_str && !data.sn_str[0] && data.sn_str[1]);
    CHECK(!strcmp(data.sn_str[1], "NEW000000001") && data.denom_mix[1] == 10);
    CHECK(counting_data_serial_valid_count(&data) == 1);
    info(0, 0, 2); snapshot(1, 1, 0); CHECK(exact(1, 1, 0)); return true;
}
static bool wire_amount_limits(void)
{
    const uint32_t values[] = {UINT32_MAX, UINT32_MAX - 1u};
    const char *headers[] = {"4,294,967,295", "4,294,967,294"};
    for (unsigned i = 0; i < 2; ++i) {
        reset(); begin(false); info(values[i], 1, 1);
        CHECK(data.total_amount == (double)values[i]);
        char header[32];
        app_counting_runtime_format_amount(header, sizeof(header), data.total_amount);
        CHECK(!strcmp(header, headers[i]));
        info(0, 0, 2);
        CHECK(session.last_result.amount == (double)values[i]);
        CHECK(session.history_record.amount == (double)values[i]);
        CHECK(island_amount == (double)values[i]);
        begin(true); info(0, 0, 1);
        CHECK(data.total_amount == (double)values[i] && data.last_total_amount == (double)values[i]);
        info(0, 0, 2); CHECK(island_amount == (double)values[i]);
    }
    return true;
}
static bool wire_stop_amount_limits(void)
{
    const uint32_t values[] = {UINT32_MAX, UINT32_MAX - 1u};
    for (unsigned i = 0; i < 2; ++i) {
        reset(); begin(false); info(values[i] - 1u, 1, 1);
        info(values[i], 1, 2);
        CHECK(data.total_amount == (double)values[i]);
        CHECK(session.last_result.amount == (double)values[i]);
        CHECK(session.history_record.amount == (double)values[i]);
        CHECK(island_amount == (double)values[i]);
    }
    return true;
}
static uint32_t random_state;
static unsigned next_random(void)
{
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static bool randomized(void)
{
    unsigned checked_notes = 0;
    for (unsigned seed = 1; seed <= 80; ++seed) {
        reset(); random_state = seed;
        unsigned ten = 0, fifty = 0, pcs = 0, amount = 0;
        for (unsigned run = 0; run < 5; ++run) {
            begin(run != 0); info(0, 0, 1);
            unsigned notes = 1 + next_random() % 37;
            for (unsigned i = 0; i < notes; ++i) {
                unsigned value = next_random() % 3 == 0 ? 10 : 50;
                unsigned previous_amount = amount, previous_pcs = pcs;
                ++pcs; ++checked_notes; amount += value; ten += value == 10; fifty += value == 50;
                /* Repeated text is intentionally common; each note is distinct. */
                const char *serial = next_random() % 4 == 0 ? "" : "SAME12345678";
                switch (next_random() % 6) {
                case 0: note(value, serial); info(amount, pcs, 1); break;
                case 1: info(previous_amount, pcs, 1); note(value, serial); info(amount, pcs, 1); break;
                case 2: info(amount, previous_pcs, 1); note(value, serial); info(amount, pcs, 1); break;
                case 3: note(value, serial); break; /* omitted 0E */
                case 4: note(value, serial); info(amount, pcs, 1); info(amount, pcs, 1); break;
                default: info(amount, pcs, 1); note(value, serial); break;
                }
                if (!exact(ten, fifty, 0)) {
                    fprintf(stderr, "seed=%u run=%u note=%u\n", seed, run, i); return false;
                }
            }
            info(amount, pcs, 1); info(0, 0, 2);
            CHECK(!locked); CHECK(exact(ten, fifty, 0));
            snapshot(ten, fifty, 0); CHECK(exact(ten, fifty, 0));
            CHECK(restarts == 0);
        }
    }
    printf("PROPERTY 80 seeds x 5 runs, %u note prefixes: every live prefix and final 0B histogram match\n", checked_notes);
    return true;
}
static bool diagnostic_0b_rows(void)
{
    begin(false); note(50, "A"); info(50, 1, 1); info(0, 0, 2);
    boundary(0); final_row(50, 1); final_row(50, 1); boundary(255);
    printf("DIAGNOSTIC duplicate 0B row: A50=%u (wire has two identical rows; controller row semantics need confirmation)\n", quantity(50));
    snapshot(0, 1, 0); boundary(0); final_row(10, 0); boundary(255);
    printf("DIAGNOSTIC missing 0B row: A50=%u A10=%u (0B is authoritative; no invented reconstruction)\n", quantity(50), quantity(10));
    snapshot(0, 0, 1);
    printf("DIAGNOSTIC conflicting 0B vs 0E: A100=%u total=%d/%.0f\n", quantity(100), data.total_pcs, data.total_amount);
    return true;
}
struct test_case { const char *name; bool (*run)(void); };
static const struct test_case cases[] = {
    {"duplicate_note", duplicate_note}, {"blank_serial", blank_serial},
    {"count_first", count_first}, {"amount_first", amount_first},
    {"end_amount", end_amount}, {"end_count", end_count}, {"zero_end_retains", zero_end_retains},
    {"missing_info", missing_info}, {"blank_serial_order", blank_serial_order},
    {"all_amounts_ahead", all_amounts_ahead}, {"all_counts_ahead", all_counts_ahead},
    {"ten_thousand_notes", ten_thousand_notes},
    {"add_continue", add_continue}, {"restart_before_note", restart_before_note},
    {"restart_after_note", restart_after_note}, {"restart_equal", restart_equal},
    {"empty_add", empty_add}, {"final_replace", final_replace},
    {"live_snapshot", live_snapshot}, {"non_add_after_add", non_add_after_add},
    {"restart_missing_first", restart_missing_first}, {"restart_equal_amount", restart_equal_amount},
    {"late_note_after_end", late_note_after_end}, {"add_tail_empty", add_tail_empty},
    {"restart_keeps_current_serials", restart_keeps_current_serials},
    {"wire_amount_limits", wire_amount_limits}, {"wire_stop_amount_limits", wire_stop_amount_limits},
    {"randomized", randomized},
    {"diagnostic_0b_rows", diagnostic_0b_rows},
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
