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
    note_detail.live_denom_started = note_detail.live_reject_started = false;
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
static void check_big_record(unsigned index)
{
    const ui_history_record_t *record = &ui_history_data_get()->records[index];
    assert(record->pcs == 9999 && record->amount == 499950000u);
    assert(record->detail_amount == 499950000u);
    history_record_detail_t detail = {0}; assert(ui_history_record_read_detail(record, &detail));
    assert(detail.denoms_complete && detail.serials_complete);
    assert(detail.denomination_pcs == 9999 && detail.denomination_amount == 499950000u);
    assert(saved_denom(&detail, 50000) == 9999 && detail.serial_count == 9999);
    for (unsigned i = 0; i < 9999; ++i)
        assert(detail.serials[i].no == i + 1 && detail.serials[i].denom == 50000);
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
    assert(store->record_count == 11 && store->total_notes_counted == 10578);
    check_wire_fallback(0, UINT32_MAX); check_wire_fallback(1, UINT32_MAX - 1u);
    check_wire_full(2); check_big_record(3);
    check_record(4, store->records[4].record_no, 1, 2, "ONLY49000001");
    /* CNT has an accepted count but no denomination/serial protocol. */
    assert(store->records[5].pcs == 3 && store->records[5].amount == 0);
    check_record(6, store->records[6].record_no, 0, 2, "QTYFIX000001");
    check_record(7, store->records[7].record_no, 0, 2, "DELAY0000001");
    check_record(8, store->records[8].record_no, 1, 0, "NONADD000001");
    check_record(9, store->records[9].record_no, 1, 1, "RESET0000001");
    check_record(10, store->records[10].record_no, 0, 1, "OLD000000001");
}
int main(int argc, char **argv)
{
    assert(argc == 2); ui_thread = pthread_self();
    ui_history_data_init(); assert(ui_history_data_is_available());
    if (!strcmp(argv[1], "read")) {
        read_final_store(); assert(storage_worker_shutdown());
        puts("PASS fresh process: eleven records: UINT32 edge fallback/full snapshots, 499950000, 49-only ADD, CNT; lifetime 10578"); return 0;
    }
    note_data.denom_number = 2; note_data.denom[0].value = 10; note_data.denom[1].value = 50;
    unsigned one50[] = {50}, one10[] = {10}, mixed[] = {50, 10};
    const char *old_serial[] = {"OLD000000001"};
    const char *reset_serial[] = {"RESET0000001"};
    const char *mixed_serials[] = {"RESET0000001", "ADDED0000001"};
    const char *nonadd_serial[] = {"NONADD000001"};
    unsigned delayed_values[] = {50, 50};
    const char *delayed_serials[] = {"DELAY0000001", "DELAY0000002"};
    const char *qty_serials[] = {"QTYFIX000001", "QTYFIX000002"};
    const char *only_serials[] = {"ONLY49000001", "ONLY49000002"};
    unsigned only_add_values[] = {50, 10, 50};
    const char *only_add_serials[] = {"ONLY49000001", "ONLY49000002", "ONLYADD00001"};

    begin(true); info(0, 0, 1); note(50, old_serial[0]); info(50, 1, 1); info(0, 0, 2);
    assert(ui_history_data_get()->record_count == 0); final_a(0, 1);
    assert(ui_history_data_get()->record_count == 0);
    finish_details(1, one50, old_serial);
    uint32_t old_id = ui_history_data_get()->records[0].record_no;
    check_record(0, old_id, 0, 1, old_serial[0]);

    begin(true); info(0, 0, 1); note(50, reset_serial[0]); info(50, 1, 1); info(0, 0, 2);
    assert(note_data.last_total_pcs == 1); /* Equal total is still ambiguous. */
    check_record(0, old_id, 0, 1, old_serial[0]);
    final_a(0, 1); assert(note_data.last_total_pcs == 0);
    assert(!counting_report_take_group_restart()); /* Actual runtime consumed it. */
    check_record(0, old_id, 0, 1, old_serial[0]);
    finish_details(1, one50, reset_serial);
    const ui_history_store_t *store = ui_history_data_get();
    assert(store->record_count == 2 && store->total_notes_counted == 2);
    uint32_t new_id = store->records[0].record_no; assert(new_id != old_id);
    check_record(0, new_id, 0, 1, reset_serial[0]); check_record(1, old_id, 0, 1, old_serial[0]);

    begin(true); info(0, 0, 1); info(50, 1, 1); note(10, mixed_serials[1]); info(60, 2, 1);
    info(0, 0, 2); final_a(1, 1); finish_details(2, mixed, mixed_serials);
    store = ui_history_data_get(); assert(store->record_count == 2 && store->total_notes_counted == 3);
    check_record(0, new_id, 1, 1, reset_serial[0]); check_record(1, old_id, 0, 1, old_serial[0]);

    begin(true); info(0, 0, 1); info(0, 0, 2); final_a(1, 1);
    marker(0x0C, 0); marker(0x0C, 255);
    assert(!note_session.history_record.valid);
    assert(ui_history_data_get()->record_count == 2 && ui_history_data_get()->total_notes_counted == 3);

    begin(false); info(0, 0, 1); note(10, nonadd_serial[0]); info(10, 1, 1); info(0, 0, 2);
    final_a(1, 0); finish_details(1, one10, nonadd_serial);

    /* The last LIVE pair is split and STOP is a zero marker. Only complete
     * final A supplies the missing amount; saved summary must agree with it. */
    begin(false); info(0, 0, 1); note(50, delayed_serials[0]); info(50, 1, 1);
    note(50, delayed_serials[1]); info(50, 2, 1); info(0, 0, 2);
    final_a(0, 2); finish_details(2, delayed_values, delayed_serials);
    check_record(0, ui_history_data_get()->records[0].record_no, 0, 2, delayed_serials[0]);

    /* Final A also owns a missing last accepted count, not only its amount. */
    begin(false); info(0, 0, 1); note(50, qty_serials[0]); info(50, 1, 1);
    note(50, qty_serials[1]); info(0, 0, 2); final_a(0, 2);
    finish_details(2, delayed_values, qty_serials);
    check_record(0, ui_history_data_get()->records[0].record_no, 0, 2, qty_serials[0]);

    /* CNT: an empty A protocol cannot erase the authoritative total count. */
    begin(false); info(0, 3, 1); info(0, 0, 2);
    denom_boundary(0); denom_boundary(255); finish_details(0, NULL, NULL);

    /* Start from an empty display and deliver no live 0E at all. A zero STOP
     * initially has no history candidate; final A must establish one. */
    note_data.total_pcs = 0; note_data.total_amount = 0;
    begin(true); note(50, only_serials[0]); note(10, only_serials[1]); info(0, 0, 2);
    final_a(1, 1); finish_details(2, mixed, only_serials);
    check_record(0, ui_history_data_get()->records[0].record_no, 1, 1, only_serials[0]);
    uint32_t only_id = ui_history_data_get()->records[0].record_no;
    /* Normal cumulative next run also omits every live 0E. The previous
     * result must become the baseline before this zero STOP is processed. */
    begin(true); note(50, only_add_serials[2]); info(0, 0, 2); final_a(1, 2);
    assert(note_session.analysis_valid_pcs == 1 && note_data.last_total_pcs == 2);
    finish_details(3, only_add_values, only_add_serials);
    check_record(0, only_id, 1, 2, only_serials[0]);

    /* 50000 * 9999 cannot survive a float round trip exactly. All stored
     * money fields must preserve the integer denomination multiplication. */
    begin(false); info(0, 0, 1);
    for (unsigned i = 0; i < 9999; ++i) note(50000, "BIG000000001");
    info(499950000u, 9999, 2);
    denom_boundary(0);
    for (unsigned i = 0; i < 10; ++i) denom_row(50000, 999);
    denom_row(50000, 9); denom_boundary(255);
    unsigned *big_values = calloc(9999, sizeof(*big_values));
    const char **big_serials = calloc(9999, sizeof(*big_serials));
    assert(big_values && big_serials);
    for (unsigned i = 0; i < 9999; ++i) { big_values[i] = 50000; big_serials[i] = "BIG000000001"; }
    finish_details(9999, big_values, big_serials);
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
    puts("PASS actual 49 -> runtime 0E -> runtime 0B -> 0C/0D -> storage: late reset adds group, continuation updates same group, empty ADD saves nothing, non-ADD independent");
    return 0;
}
