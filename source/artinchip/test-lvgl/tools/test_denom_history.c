/* Real storage integration. Reuse the established filesystem fixture; only
 * UI effects are stubs. INFO, DENOM, try-commit and history-ready handlers are
 * extracted verbatim from runtime. The detail dispatcher receives that same
 * history-ready callback; no mock save, synthetic end_seen, or fake record. */
#define main history_storage_fixture_main
#define counting_report_reset history_fixture_report_reset
#define uart_debug_printf history_fixture_uart_debug
#include "test_history_storage.c"
#undef uart_debug_printf
#undef counting_report_reset
#undef main
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/history/history_record_detail.h"

static counting_sim_t note_data;
static counting_session_state_t note_session;
static counting_detail_state_t note_detail;
static bool note_add;
static uint32_t clock_tick = 100;
static counting_history_commit_result_t observed_commit;
uint32_t app_clock_uptime_ms(void) { return ++clock_tick; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t command, const uint8_t *payload, uint16_t length)
{ (void)command; (void)payload; (void)length; return 0; }

typedef struct {
    counting_session_state_t *session;
    counting_sim_t *sim_data;
} app_counting_detail_context_t;
static void app_counting_runtime_report_history_commit(
    counting_session_state_t *s, counting_history_commit_result_t result, uint8_t attempts)
{ (void)s; (void)attempts; observed_commit = result; }
static void app_counting_runtime_on_main_data_changed(void) {}
static void app_counting_runtime_on_live_denom_changed(void) {}
static const counting_denom_reply_hooks_t g_counting_denom_hooks = {
    .on_history_frame = counting_history_append_frame
};
#define counting_multi_info(...) false
#define counting_multi_query_busy() false
#define counting_multi_denom(...) ((void)0)
#define currency_state_multi_selected() false
#define ui_page_dynamic_capture_on_count_info(...) ((void)0)
#define currency_state_restore_detected_result() false
#define page_01_curr_img_refre() ((void)0)
#define smart_island_refresh_summary() ((void)0)
#define app_counting_runtime_refresh_compact(...) ((void)0)
#define page_01_main_set_counting_locked(...) ((void)0)
#define page_02_list_section_mark_dirty(...) ((void)0)
#define machine_state_add_enabled() note_add
#define smart_island_set_count_analysis(...) ((void)0)
#define smart_island_update_counting(...) ((void)0)
#define smart_island_notify_count_end(...) ((void)0)
#define app_counting_runtime_main_page_active() false
#define ui_refresh_main_page() ((void)0)
#define app_auto_qr_on_end(...) ((void)0)
#include "denom_history_runtime.inc"

static void history_frame(void *context, const char *tag, const uint8_t *buf, uint8_t len)
{ (void)context; counting_history_append_frame(tag, buf, len); }
static void dispatch_detail(uint8_t *buf, unsigned len)
{
    app_counting_detail_context_t context = {&note_session, &note_data};
    counting_reject_sn_reply_hooks_t hooks = {
        .context = &context, .on_history_frame = history_frame,
        .on_history_record_ready = app_counting_runtime_on_history_record
    };
    counting_reject_sn_reply_dispatch(buf[3], &note_detail, &note_session,
        &note_data, buf, (uint8_t)len, &hooks);
}
static void start_history(const uint8_t *buf, uint8_t len)
{
    counting_history_begin_group(note_add);
    counting_history_session_start(buf, len);
}
static void begin(bool add)
{
    assert(counting_history_prepare_start(&note_session, &note_data, app_clock_uptime_ms()));
    note_add = add;
    counting_report_begin(add, &note_data);
    /* Match the real command-runtime START ownership initialization. */
    counting_denom_reply_begin_count(&note_detail);
    note_detail.live_reject_started = false;
    uint8_t b[7] = {0xFD, 0xDF, 7, 0x0A, 1, 1, 0};
    counting_control_reply_hooks_t hooks = {.on_start_success = start_history};
    counting_control_reply_dispatch(0x0A, &note_session, b, sizeof(b), &hooks);
    assert(note_session.start_confirmed);
}
static void info(unsigned amount, unsigned pcs, unsigned status)
{
    uint8_t b[13] = {0xFD, 0xDF, 13, 0x0E};
    b[4] = amount >> 24; b[5] = amount >> 16; b[6] = amount >> 8; b[7] = amount;
    b[8] = pcs >> 8; b[9] = pcs; b[11] = status;
    app_counting_runtime_handle_info(&note_session, &note_data, b, sizeof(b));
}
static void note(unsigned value, const char *serial)
{
    uint8_t b[24] = {0xFD, 0xDF, 24, 0x49}; char text[24];
    snprintf(text, sizeof(text), "%7u%-12.12s", value, serial);
    memcpy(b + 4, text, 19); dispatch_detail(b, sizeof(b));
}
static void denom_boundary(unsigned fill)
{
    uint8_t b[16] = {0xFD, 0xDF, 16, 0x0B}; memset(b + 4, fill, 11);
    app_counting_runtime_handle_denom(&note_detail, &note_session, &note_data, b, sizeof(b));
}
static void denom_row(unsigned value, unsigned pcs)
{
    assert(value>0 && value<=99999999U && pcs<=999U);
    uint8_t b[16] = {0xFD, 0xDF, 16, 0x0B}; char text[16];
    snprintf(text, sizeof(text), "%8u%3u", value, pcs); memcpy(b + 4, text, 11);
    app_counting_runtime_handle_denom(&note_detail, &note_session, &note_data, b, sizeof(b));
}
static void final_a(unsigned ten, unsigned fifty)
{
    denom_boundary(0); denom_row(10, ten); denom_row(50, fifty); denom_boundary(255);
}
static void marker(unsigned cmd, unsigned fill)
{
    unsigned len = cmd == 0x0C ? 7 : 25;
    uint8_t b[25] = {0xFD, 0xDF}; b[2] = len; b[3] = cmd;
    memset(b + 4, fill, len - 5); dispatch_detail(b, len);
}
static void serial_row(unsigned no, unsigned value, const char *serial)
{
    uint8_t b[25] = {0xFD, 0xDF, 25, 0x0D}; char text[24]; b[4] = no;
    snprintf(text, sizeof(text), "%7u%-12.12s", value, serial);
    memcpy(b + 5, text, 19); dispatch_detail(b, sizeof(b));
}
static void finish_details(unsigned rows, const unsigned *values, const char **serials)
{
    assert(note_session.history_record.valid && !note_session.history_record.end_seen);
    marker(0x0C, 0); marker(0x0C, 255);
    marker(0x0D, 0);
    for (unsigned i = 0; i < rows; ++i) serial_row(i + 1, values[i], serials[i]);
    assert(!note_session.history_record.end_seen);
    marker(0x0D, 255);
    assert(observed_commit == COUNTING_HISTORY_COMMIT_PENDING);
    storage_job_id_t id = ui_history_last_commit_id(); assert(id);
    /* The next model mutation cannot alter the queued immutable snapshot. */
    wait_saved(id, app_clock_uptime_ms());
    counting_history_poll_commit(&note_session, &note_data, app_clock_uptime_ms());
}
static unsigned saved_denom(const history_record_detail_t *detail, unsigned value)
{
    for (size_t i = 0; i < detail->denom_count; ++i)
        if (detail->denoms[i].value == value) return detail->denoms[i].pcs;
    return 0;
}
static void check_record(unsigned index, uint32_t id, unsigned ten, unsigned fifty,
                         const char *first_serial)
{
    const ui_history_store_t *store = ui_history_data_get();
    assert(index < store->record_count);
    const ui_history_record_t *record = &store->records[index];
    unsigned pcs = ten + fifty, amount = ten * 10 + fifty * 50;
    if (record->record_no != id || record->pcs != pcs || record->amount != amount)
        fprintf(stderr, "HISTORY MISMATCH index=%u expected id=%u pcs=%u amount=%u; actual id=%u pcs=%u amount=%u\n",
            index, id, pcs, amount, record->record_no, record->pcs, record->amount);
    assert(record->record_no == id && record->pcs == pcs && record->amount == amount);
    assert(record->detail_amount == amount);
    history_record_detail_t detail = {0};
    assert(ui_history_record_read_detail(record, &detail));
    assert(detail.denoms_complete && detail.serials_complete);
    assert(detail.denomination_pcs == pcs && detail.denomination_amount == amount);
    assert(saved_denom(&detail, 10) == ten && saved_denom(&detail, 50) == fifty);
    assert(detail.serial_count == pcs);
    for (unsigned i = 0; i < pcs; ++i) assert(detail.serials[i].no == i + 1);
    if (first_serial) assert(!strcmp(detail.serials[0].sn, first_serial));
    history_record_detail_release(&detail);
}
static void check_unmatched_qty(unsigned index)
{
    const ui_history_record_t *record=&ui_history_data_get()->records[index];
    assert(record->pcs==1 && record->amount==50 && record->detail_amount==50);
    history_record_detail_t detail={0};assert(ui_history_record_read_detail(record,&detail));
    assert(!detail.denoms_complete && !detail.serials_complete);
    assert(detail.denomination_pcs==2 && detail.denomination_amount==100);
    assert(saved_denom(&detail,50)==2 && detail.serial_count==2);
    assert(!strcmp(detail.serials[0].sn,"QTYFIX000001"));
    history_record_detail_release(&detail);
}
static void check_controller_a(unsigned index)
{
    const ui_history_record_t *record=&ui_history_data_get()->records[index];
    check_record(index,record->record_no,2,0,"AOWN00000001");
    history_record_detail_t detail={0};assert(ui_history_record_read_detail(record,&detail));
    assert(detail.serial_count==2 && detail.serials[0].denom==50 && detail.serials[1].denom==50);
    history_record_detail_release(&detail);
}
static void check_big_record(unsigned index)
{
    const ui_history_record_t *record = &ui_history_data_get()->records[index];
    assert(record->pcs == 999 && record->amount == 499500999u);
    assert(record->detail_amount == 499500999u);
    history_record_detail_t detail = {0}; assert(ui_history_record_read_detail(record, &detail));
    assert(detail.denoms_complete && detail.serials_complete);
    assert(detail.denomination_pcs == 999 && detail.denomination_amount == 499500999u);
    assert(saved_denom(&detail, 500001) == 999 && detail.serial_count == 999);
    for (unsigned i = 0; i < 999; ++i)
        assert(detail.serials[i].no == i + 1 && detail.serials[i].denom == 500001);
    history_record_detail_release(&detail);
}
static void check_wire_fallback(unsigned index, uint32_t amount)
{
    const ui_history_record_t *record = &ui_history_data_get()->records[index];
    assert(record->pcs == 1 && record->amount == amount && record->detail_amount == amount);
    history_record_detail_t detail = {0}; assert(ui_history_record_read_detail(record, &detail));
    assert(detail.denomination_pcs == 0 && !detail.denoms_complete);
    history_record_detail_release(&detail);
}
static void check_wire_full(unsigned index)
{
    const ui_history_record_t *record = &ui_history_data_get()->records[index];
    assert(record->pcs == 563 && record->amount == 4294967294u && record->detail_amount == 4294967294u);
    history_record_detail_t detail = {0}; assert(ui_history_record_read_detail(record, &detail));
    assert(detail.denoms_complete && detail.serials_complete && detail.serial_count == 563);
    assert(saved_denom(&detail, 8388607) == 512 && saved_denom(&detail, 10) == 51);
    assert(detail.denomination_pcs == 563 && detail.denomination_amount == 4294967294u);
    for (unsigned i = 0; i < 563; ++i)
        assert(detail.serials[i].no == i + 1 && detail.serials[i].denom == (i < 512 ? 8388607u : 10u));
    history_record_detail_release(&detail);
}
static void read_final_store(void)
{
    const ui_history_store_t *store = ui_history_data_get();
    assert(store->record_count == 12 && store->total_notes_counted == 1580);
    check_wire_fallback(0, UINT32_MAX); check_wire_fallback(1, UINT32_MAX - 1u);
    check_wire_full(2); check_big_record(3); check_controller_a(4);
    check_record(5, store->records[5].record_no, 1, 2, "ONLY49000001");
    /* CNT has an accepted count but no denomination/serial protocol. */
    assert(store->records[6].pcs == 3 && store->records[6].amount == 0);
    check_unmatched_qty(7);
    check_record(8, store->records[8].record_no, 0, 2, "DELAY0000001");
    check_record(9, store->records[9].record_no, 1, 0, "NONADD000001");
    check_record(10, store->records[10].record_no, 1, 1, "RESET0000001");
    check_record(11, store->records[11].record_no, 0, 2, "OLD000000001");
}
int main(int argc, char **argv)
{
    assert(argc == 2); ui_thread = pthread_self();
    ui_history_data_init(); assert(ui_history_data_is_available());
    if (!strcmp(argv[1], "read")) {
        read_final_store(); assert(storage_worker_shutdown());
        puts("PASS fresh process: twelve records: authoritative A, UINT32 edge fallback/full snapshots, 499500999, no-live-0E ADD, CNT; lifetime 1580"); return 0;
    }
    note_data.denom_number = 2; note_data.denom[0].value = 10; note_data.denom[1].value = 50;
    unsigned one50[] = {50}, one10[] = {10}, mixed[] = {50, 10};
    const char *old_serial[] = {"OLD000000001", "OLD000000002"};
    const char *reset_serial[] = {"RESET0000001"};
    const char *mixed_serials[] = {"RESET0000001", "ADDED0000001"};
    const char *nonadd_serial[] = {"NONADD000001"};
    unsigned delayed_values[] = {50, 50};
    const char *delayed_serials[] = {"DELAY0000001", "DELAY0000002"};
    const char *qty_serials[] = {"QTYFIX000001", "QTYFIX000002"};
    const char *only_serials[] = {"ONLY49000001", "ONLY49000002"};
    unsigned only_add_values[] = {50, 10, 50};
    const char *only_add_serials[] = {"ONLY49000001", "ONLY49000002", "ONLYADD00001"};

    begin(true); info(0, 0, 1); note(50, old_serial[0]); note(50, old_serial[1]); info(100, 2, 1); info(0, 0, 2);
    assert(ui_history_data_get()->record_count == 0); final_a(0, 2);
    assert(ui_history_data_get()->record_count == 0);
    finish_details(2, delayed_values, old_serial);
    uint32_t old_id = ui_history_data_get()->records[0].record_no;
    check_record(0, old_id, 0, 2, old_serial[0]);

    begin(true); info(0, 0, 1); note(50, reset_serial[0]); info(50, 1, 1); info(0, 0, 2);
    assert(note_data.last_total_pcs == 0); /* 0E drops from two to one: confirmed restart. */
    check_record(0, old_id, 0, 2, old_serial[0]);
    final_a(0, 1); assert(note_data.last_total_pcs == 0);
    assert(!counting_report_take_group_restart()); /* Actual runtime consumed it. */
    check_record(0, old_id, 0, 2, old_serial[0]);
    finish_details(1, one50, reset_serial);
    const ui_history_store_t *store = ui_history_data_get();
    assert(store->record_count == 2 && store->total_notes_counted == 3);
    uint32_t new_id = store->records[0].record_no; assert(new_id != old_id);
    check_record(0, new_id, 0, 1, reset_serial[0]); check_record(1, old_id, 0, 2, old_serial[0]);

    begin(true); info(0, 0, 1); info(50, 1, 1); note(10, mixed_serials[1]); info(60, 2, 1);
    info(0, 0, 2); final_a(1, 1); finish_details(2, mixed, mixed_serials);
    store = ui_history_data_get(); assert(store->record_count == 2 && store->total_notes_counted == 4);
    check_record(0, new_id, 1, 1, reset_serial[0]); check_record(1, old_id, 0, 2, old_serial[0]);

    begin(true); info(0, 0, 1); info(0, 0, 2); final_a(1, 1);
    marker(0x0C, 0); marker(0x0C, 255);
    assert(!note_session.history_record.valid);
    assert(ui_history_data_get()->record_count == 2 && ui_history_data_get()->total_notes_counted == 4);

    begin(false); info(0, 0, 1); note(10, nonadd_serial[0]); info(10, 1, 1); info(0, 0, 2);
    final_a(1, 0); finish_details(1, one10, nonadd_serial);

    /* Final A stays independent of a lagging LIVE amount and zero STOP.
     * Existing complete-A history summaries remain; the live 0E total does not change. */
    begin(false); info(0, 0, 1); note(50, delayed_serials[0]); info(50, 1, 1);
    note(50, delayed_serials[1]); info(50, 2, 1); info(0, 0, 2);
    final_a(0, 2); assert(note_data.total_pcs==2 && note_data.total_amount==50);
    finish_details(2, delayed_values, delayed_serials);
    check_record(0, ui_history_data_get()->records[0].record_no, 0, 2, delayed_serials[0]);

    /* A may disagree with the last 0E count. Preserve both without synthesizing
     * a new total or rebuilding A from the complete B report. */
    begin(false); info(0, 0, 1); note(50, qty_serials[0]); info(50, 1, 1);
    note(50, qty_serials[1]); info(0, 0, 2); final_a(0, 2);
    finish_details(2, delayed_values, qty_serials);
    assert(note_data.total_pcs==1 && note_data.total_amount==50);
    check_unmatched_qty(0);

    /* CNT: an empty A protocol cannot erase the authoritative total count. */
    begin(false); info(0, 3, 1); info(0, 0, 2);
    denom_boundary(0); denom_boundary(255); finish_details(0, NULL, NULL);

    /* No live 0E is required when the authoritative STOP carries its totals. */
    note_data.total_pcs = 0; note_data.total_amount = 0;
    begin(true); note(50, only_serials[0]); note(10, only_serials[1]); info(60, 2, 2);
    final_a(1, 1); finish_details(2, mixed, only_serials);
    check_record(0, ui_history_data_get()->records[0].record_no, 1, 1, only_serials[0]);
    uint32_t only_id = ui_history_data_get()->records[0].record_no;
    /* Normal cumulative next run also omits live 0E; STOP supplies the total.
     * The previous result becomes the baseline before STOP is processed. */
    begin(true); note(50, only_add_serials[2]); info(110, 3, 2); final_a(1, 2);
    assert(note_session.analysis_valid_pcs == 1 && note_data.last_total_pcs == 2);
    finish_details(3, only_add_values, only_add_serials);
    check_record(0, only_id, 1, 2, only_serials[0]);

    /* A and B can have the same number of notes but disagree on face value.
     * The authoritative controller A must survive the full history chain. */
    const char *a_owned_serials[]={"AOWN00000001","AOWN00000002"};
    begin(false); info(0,0,1);
    note(50,a_owned_serials[0]); note(50,a_owned_serials[1]);
    info(100,2,1); info(0,0,2); final_a(2,0);
    assert(note_data.total_pcs==2 && note_data.total_amount==100);
    finish_details(2,delayed_values,a_owned_serials); check_controller_a(0);

    /* 500001 * 999 cannot survive a float round trip exactly. All stored
     * money fields must preserve the integer denomination multiplication. */
    begin(false); info(0, 0, 1);
    for (unsigned i = 0; i < 999; ++i) note(500001, "BIG000000001");
    info(499500999u, 999, 2);
    denom_boundary(0); denom_row(500001, 999); denom_boundary(255);
    unsigned *big_values = calloc(999, sizeof(*big_values));
    const char **big_serials = calloc(999, sizeof(*big_serials));
    assert(big_values && big_serials);
    for (unsigned i = 0; i < 999; ++i) { big_values[i] = 500001; big_serials[i] = "BIG000000001"; }
    finish_details(999, big_values, big_serials);
    free(big_values); free(big_serials);
    check_big_record(0);

    /* A complete monetary report close to the full 0E 32-bit wire range. */
    begin(false); info(0, 0, 1);
    unsigned boundary_values[563]; const char *boundary_serials[563];
    for (unsigned i = 0; i < 563; ++i) {
        boundary_values[i] = i < 512 ? 8388607u : 10u;
        boundary_serials[i] = "MAXFULL00001";
        note(boundary_values[i], boundary_serials[i]);
    }
    info(4294967294u, 563, 1); info(0, 0, 2);
    denom_boundary(0); denom_row(8388607, 512); denom_row(10, 51); denom_boundary(255);
    finish_details(563, boundary_values, boundary_serials); check_wire_full(0);

    /* Without a denomination report, history must preserve the exact wire
     * amount as well; it cannot depend on the integer full-A repair path. */
    begin(false); info(UINT32_MAX - 1u, 1, 1); info(0, 0, 2);
    assert(note_data.total_amount == (double)(UINT32_MAX - 1u));
    denom_boundary(0); denom_boundary(255); finish_details(0, NULL, NULL);
    check_wire_fallback(0, UINT32_MAX - 1u);
    begin(false); info(UINT32_MAX - 1u, 1, 1); info(UINT32_MAX, 1, 2);
    assert(note_data.total_amount == (double)UINT32_MAX);
    denom_boundary(0); denom_boundary(255); finish_details(0, NULL, NULL);
    check_wire_fallback(0, UINT32_MAX);
    read_final_store();
    counting_report_shutdown(); counting_data_clear_serials(&note_data); counting_data_clear_errors(&note_data);
    assert(storage_worker_shutdown());
    puts("PASS actual 49 -> runtime 0E -> runtime 0B -> 0C/0D -> storage: 0E restart adds group, continuation updates same group, empty ADD saves nothing, non-ADD independent");
    return 0;
}
