#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_reply.h"
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_action_service.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/machine_state/machine_state.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/protocol/mode_codec.h"
#include "un260/innovation/multi_pass_verification.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); abort(); \
} } while (0)

static counting_session_state_t g_counting_session;
static unsigned images, summaries, starts, ends, resets, locks, unlocks, failures;
static unsigned fault_clears, report_resets, clear_sends, navigations;
static bool locked, main_visible = true, history_ready = true;
static bool transport_ready = true, transport_fault, send_failure;
static uint64_t tick = 100;
static char main_code[4], island_code[4];
static uint8_t last_command, last_payload[8];
static uint16_t last_length;

uint64_t app_clock_monotonic_ms(void) { return tick; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    CHECK(len <= sizeof(last_payload));
    last_command = cmd; last_length = len; memcpy(last_payload, payload, len);
    if (cmd == 0x3B) ++clear_sends;
    return send_failure ? -1 : (int)len;
}
static bool protocol_send_is_ready(void) { return transport_ready; }
static bool runtime_guard_has_fault(void) { return transport_fault; }
bool counting_history_discard_pending(counting_session_state_t *session)
{ (void)session; return false; }
bool counting_history_can_start(void) { return true; }
void counting_history_current_invalidate(void) {}
static bool counting_history_prepare_reset(counting_session_state_t *session,
    const counting_sim_t *data, uint32_t now)
{ (void)session; (void)data; (void)now; return history_ready; }
static uint32_t lv_tick_get(void) { return (uint32_t)tick; }
static uint32_t ui_history_total_notes_counted_get(void) { return 50; }
static void page_01_curr_img_refre(void)
{ ++images; currency_state_get_effective_code(main_code); }
static void smart_island_refresh_summary(void)
{ ++summaries; currency_state_get_effective_code(island_code); }
static void page_01_main_set_counting_locked(bool value)
{ locked = value; if (value) ++locks; else ++unlocks; }
static bool app_counting_runtime_main_page_active(void) { return main_visible; }
static void smart_island_notify_count_start(void) { ++starts; }
static void smart_island_notify_count_end(const char *result) { (void)result; ++ends; }
static void smart_island_notify_count_reset(void) { ++resets; }
static void app_fault_recovery_stacker_cleared(void) { ++fault_clears; }
static void page_02_list_report_reset(void) { ++report_resets; }
static void ui_manager_switch(int page) { (void)page; ++navigations; }
static void failure_hook(uint8_t type, uint8_t code)
{ CHECK(type == 2 && code == 4); ++failures; }
static void counting_history_session_start(const uint8_t *buf, uint8_t len)
{ CHECK(buf && len == 7); }
static void counting_history_capture_end(const uint8_t *buf, uint8_t len)
{ CHECK(buf && len == 13); }
static void counting_history_append_frame(const char *tag, const uint8_t *buf, uint8_t len)
{ CHECK(tag && buf && len == 13); }
static void app_counting_runtime_refresh_compact(const counting_sim_t *data) { CHECK(data); }
static void app_counting_runtime_try_history_commit(counting_session_state_t *session,
    const counting_sim_t *data, uint32_t now)
{ CHECK(session && data); (void)now; }
static void smart_island_update_counting(int pcs, float amount) { (void)pcs; (void)amount; }
static void smart_island_set_count_analysis(int accepted, int suspect, int damaged)
{ (void)accepted; (void)suspect; (void)damaged; }
static void page_02_list_section_mark_dirty(int section) { (void)section; }
static void smart_island_notify_warning_level(const char *text, int severity)
{ CHECK(text); (void)severity; }
static void app_counting_runtime_on_reject_report_changed(void *context) { (void)context; }
static void app_setting_notice_result(const char *key, const char *text, bool success)
{ CHECK(key && text && success); }
static void page_01_bottom_a_refresh_mode(bool animate) { (void)animate; }
static void ui_manager_publish_data_changed(uint32_t topics) { (void)topics; }
bool multi_pass_verification_on_count_start(bool add) { (void)add; return false; }
void multi_pass_verification_get_view(multi_pass_verify_view_t *view) { memset(view, 0, sizeof(*view)); }
static void page_32_innovation_notify_verification_event(const multi_pass_capture_event_t *event) { CHECK(event); }
static void data_collection_state_set_status(const char *text) { CHECK(text); }
static void app_auto_qr_on_end(uint32_t now) { (void)now; }
static void ui_page_dynamic_capture_on_count_start(uint32_t valid, uint16_t issue, bool cumulative)
{ (void)valid; (void)issue; (void)cumulative; }
static void ui_page_dynamic_capture_on_count_info(uint16_t valid, uint8_t issue, uint8_t status)
{ (void)valid; (void)issue; (void)status; }

#define UI_N_(text) text
#define UI_PAGE_MAIN 1
#define UI_PAGE_PURE 2
#define PAGE_02_SECTION_A 1
#define PAGE_02_SECTION_C 3
#define UI_DATA_TOPIC_MACHINE_SETTINGS 1U
#define SMART_ISLAND_WARNING_LEVEL_ERROR 2
#define DATA_COLLECT_MODE_NONE 0
#define data_collection_state_mode() 0
#define ui_manager_get_current_page() UI_PAGE_MAIN
#define work_mode_service_diagnostic_active() false
#define smart_island_pure_count_is_enabled() false
#define multi_pass_verification_is_active() false
#define app_fault_recovery_clear() ((void)0)
#define diagnostic_calibration_feed_started() ((void)0)
#define fault_popup_clear_runtime() ((void)0)
#define fault_popup_clear_preset() ((void)0)
#define app_auto_qr_on_start() ((void)0)
#define app_auto_qr_cancel() ((void)0)
#define counting_report_reset() ((void)0)
#define ui_count_end_anim_cancel() ((void)0)
#define smart_island_clear_count_analysis() ((void)0)
#define page_01_main_scroll_reset() ((void)0)
#define ui_refresh_main_page() ((void)0)
#define page_06_data_collection_refresh() ((void)0)
#define app_setting_runtime_cancel_mode_clear() ((void)0)
#define stop_counting_sim() ((void)0)
#define page_01_main_refresh_start_state() ((void)0)
#define page_01_mode_switch_refre() ((void)0)

#include "auto_currency_runtime_under_test.inc"

static const counting_control_reply_hooks_t control_hooks = {
    .on_start_success = app_counting_runtime_on_start_success,
    .on_start_failure = failure_hook,
};

/* Transport has already checked the wire CRC. Preserve the real application
 * routing and validate frame length/header with the production frame helper. */
static bool receive(const uint8_t *frame, uint8_t len)
{
    if (!protocol_frame_is_valid(frame, len)) return false;
    switch (frame[3]) {
    case 0x51: return app_command_runtime_handle_stacker_clear(frame, len);
    case 0x50:
    case 0x49: app_currency_runtime_handle_detected(frame, len); return true;
    case 0x0A:
    case 0x0F:
        counting_control_reply_dispatch(frame[3], &g_counting_session, frame, len, &control_hooks);
        return true;
    case 0x0E:
        app_counting_runtime_handle_info(&g_counting_session, counting_data_mutable(), frame, len);
        return true;
    default: return false;
    }
}

/* Selected frames from the user's 16:47:49--16:48:10 log. The second genuine
 * run has no 0x50 currency report; accepting its first note must restore CNY. */
static const uint8_t start_ok[] = {0xFD,0xDF,7,0x0A,1,1,0x26};
static const uint8_t detected_cny[] = {0xFD,0xDF,8,0x50,'C','N','Y',0x1E};
static const uint8_t pocket_clear[] = {0xFD,0xDF,6,0x51,1,0x24};
static const uint8_t zero_live[] = {0xFD,0xDF,13,0x0E,0,0,0,0,0,0,0,0,0x48};
static const uint8_t reject_live[] = {0xFD,0xDF,13,0x0E,0,0,0,0,0,0,6,1,0x23};
static const uint8_t zero_end[] = {0xFD,0xDF,13,0x0E,0,0,0,0,0,0,0,2,0x2A};
static const uint8_t good_live[] = {0xFD,0xDF,13,0x0E,0,0,0,10,0,1,2,1,0x4F};
static const uint8_t full_live[] = {0xFD,0xDF,13,0x0E,0,0,1,84,0,22,5,1,0xFA};
static const uint8_t reject_start[] = {0xFD,0xDF,7,0x0A,2,4,0xFE};
static const uint8_t mode_ack[] = {0xFD,0xDF,6,0x04,1,0xB4};

static void effective(const char *expected)
{
    char code[4], selected[4];
    currency_state_get_effective_code(code);
    currency_state_get_selected_code(selected);
    CHECK(strcmp(code, expected) == 0);
    if (currency_state_auto_selected()) CHECK(strcmp(selected, "AUT") == 0);
}
static void publish(const char *expected)
{
    effective(expected);
    CHECK(strcmp(main_code, expected) == 0);
    CHECK(strcmp(island_code, expected) == 0);
}
static void receive_info(uint32_t amount, uint16_t qty, uint8_t issue, uint8_t status)
{
    uint8_t payload[] = {(uint8_t)(amount >> 24), (uint8_t)(amount >> 16),
        (uint8_t)(amount >> 8), (uint8_t)amount, (uint8_t)(qty >> 8),
        (uint8_t)qty, issue, status};
    uint8_t frame[13];
    CHECK(protocol_frame_build(frame, sizeof(frame), 0x0E, payload, sizeof(payload)) == 13);
    CHECK(receive(frame, sizeof(frame)));
}
static void initialize_auto(bool detected)
{
    counting_action_cancel_all();
    setting_service_cancel_mode_request();
    counting_data_clear_serials(counting_data_mutable());
    counting_data_clear_errors(counting_data_mutable());
    memset(counting_data_mutable(), 0, sizeof(*counting_data_mutable()));
    memset(&g_counting_session, 0, sizeof(g_counting_session));
    counting_multi_reset(); currency_state_reset();
    CHECK(currency_state_confirm_auto_selection());
    machine_state_confirm_mode(MODE_MDC); machine_state_confirm_add(false);
    locked = false; main_visible = history_ready = transport_ready = true;
    transport_fault = send_failure = false;
    images = summaries = starts = ends = resets = locks = unlocks = failures = 0;
    fault_clears = report_resets = clear_sends = navigations = 0;
    page_01_curr_img_refre(); smart_island_refresh_summary();
    if (detected) CHECK(receive(detected_cny, sizeof(detected_cny)));
    publish(detected ? "CNY" : "AUT");
}
static void hide_detected(void)
{
    CHECK(receive(pocket_clear, sizeof(pocket_clear)));
    effective("AUT");
}

static void reported_regression(void)
{
    initialize_auto(false);
    CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(receive(zero_live, sizeof(zero_live)));
    CHECK(receive(detected_cny, sizeof(detected_cny))); publish("CNY");
    CHECK(receive(full_live, sizeof(full_live)));
    CHECK(receive(zero_end, sizeof(zero_end))); CHECK(!locked);
    CHECK(counting_data_current()->total_pcs == 22);
    hide_detected(); hide_detected();
    CHECK(counting_data_current()->total_pcs == 22);
    CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(receive(zero_live, sizeof(zero_live)));
    CHECK(receive(reject_live, sizeof(reject_live))); effective("AUT");
    CHECK(receive(zero_end, sizeof(zero_end))); effective("AUT");
    CHECK(setting_service_request_mode(MODE_SDC));
    CHECK(last_command == 0x04 && last_length == 1 && last_payload[0] == 4);
    CHECK(mode_reply(0x04, mode_ack, sizeof(mode_ack)) & APP_SETTING_REPLY_ACTION_SCHEDULE_MODE_CLEAR);
    CHECK(machine_state_mode() == MODE_SDC);
    CHECK(app_command_runtime_clear_counting_data("mode change")); effective("AUT");
    CHECK(counting_data_current()->total_pcs == 0 && counting_data_current()->err_expected == 0);
    CHECK(clear_sends == 1);
    for (unsigned i = 0; i < 7; ++i) {
        hide_detected(); CHECK(receive(reject_start, sizeof(reject_start))); effective("AUT");
    }
    CHECK(failures == 7 && !g_counting_session.start_confirmed && !locked);
    CHECK(receive(start_ok, sizeof(start_ok))); effective("AUT");
    CHECK(receive(zero_live, sizeof(zero_live))); effective("AUT");
    CHECK(receive(reject_live, sizeof(reject_live))); effective("AUT");
    unsigned before_images = images, before_summaries = summaries;
    CHECK(receive(good_live, sizeof(good_live)));
    publish("CNY"); /* Pre-fix implementation fails here. */
    CHECK(images == before_images + 1 && summaries == before_summaries + 1);
    CHECK(machine_state_mode() == MODE_SDC && !machine_state_add_enabled());
    receive_info(30, 3, 19, 1); publish("CNY");
    CHECK(images == before_images + 1 && summaries == before_summaries + 1);
    CHECK(receive(zero_end, sizeof(zero_end)));
    CHECK(!locked && starts == 3 && ends == 3 && locks == 3 && unlocks == 3);
    CHECK(g_counting_session.analysis_valid_pcs == 3);
    CHECK(counting_data_current()->total_pcs == 3);
    puts("PASS reported AUTO -> CNY -> pocket clear -> all rejects -> SDC clear -> reject start failures -> genuine CNY without 0x50");
}

static void guard_cases(void)
{
    initialize_auto(false);
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live)));
    effective("AUT"); /* Manual fallback CNY must never become detection. */
    for (unsigned status = 0; status <= 5; ++status) {
        initialize_auto(true); hide_detected();
        receive_info(10, 1, 0, status); effective("AUT"); /* No successful START. */
    }
    initialize_auto(true); hide_detected(); CHECK(receive(start_ok, sizeof(start_ok)));
    uint8_t invalid[13]; memcpy(invalid, good_live, sizeof(invalid));
    invalid[0] = 0; CHECK(!receive(invalid, sizeof(invalid))); effective("AUT");
    memcpy(invalid, good_live, sizeof(invalid)); invalid[2] = 12;
    CHECK(!receive(invalid, sizeof(invalid))); effective("AUT");
    CHECK(!receive(NULL, 13)); effective("AUT");
    invalid[2] = 12; CHECK(receive(invalid, 12)); effective("AUT");
    receive_info(10, 1, 0, 6); effective("AUT");
    CHECK(g_counting_session.start_confirmed && locked);
    receive_info(0, 0, 3, 1); effective("AUT");
    receive_info(0, 0, 3, 2); effective("AUT");
    receive_info(10, 1, 0, 2); effective("AUT"); /* Duplicate END remains ignored. */

    initialize_auto(true); CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(receive(good_live, sizeof(good_live))); CHECK(receive(zero_end, sizeof(zero_end)));
    CHECK(counting_data_current()->total_pcs == 1); hide_detected();
    CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(receive(zero_end, sizeof(zero_end))); effective("AUT");
    CHECK(g_counting_session.last_result.pcs == 1); /* Retained final model is not new evidence. */

    for (unsigned status = 0; status <= 5; ++status) {
        initialize_auto(true); hide_detected(); CHECK(receive(start_ok, sizeof(start_ok)));
        unsigned before_images = images, before_summaries = summaries;
        receive_info(10, 1, 0, status); publish("CNY");
        CHECK(images == before_images + 1 && summaries == before_summaries + 1);
        CHECK(locked == (status <= 1));
    }
    initialize_auto(true); CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(receive(pocket_clear, sizeof(pocket_clear))); effective("CNY");
    CHECK(receive(good_live, sizeof(good_live)));
    CHECK(receive(pocket_clear, sizeof(pocket_clear))); effective("CNY");
    CHECK(receive(zero_end, sizeof(zero_end))); hide_detected();
    puts("PASS AUTO guards: no recognized currency, no start, invalid/truncated/unknown reports, all rejects, retained totals, duplicate END, out-of-order pocket clears");
}

static void reset_and_catalog_cases(void)
{
    initialize_auto(true);
    history_ready = false;
    CHECK(!app_command_runtime_clear_counting_data("blocked history")); publish("CNY");
    CHECK(clear_sends == 0);
    history_ready = true; transport_ready = false;
    CHECK(!app_command_runtime_clear_counting_data("transport unavailable")); publish("CNY");
    transport_ready = true; transport_fault = true;
    CHECK(!app_command_runtime_clear_counting_data("transport fault")); publish("CNY");
    transport_fault = false; send_failure = true;
    CHECK(!app_command_runtime_clear_counting_data("send failed")); effective("AUT");
    send_failure = false; counting_action_cancel_all();
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); publish("CNY");
    CHECK(receive(zero_end, sizeof(zero_end)));
    CHECK(app_command_runtime_clear_counting_data("explicit clear")); effective("AUT");
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); publish("CNY");
    uint8_t detected_usd[] = {0xFD,0xDF,8,0x50,'U','S','D',0};
    CHECK(receive(detected_usd, sizeof(detected_usd))); publish("USD");
    CHECK(receive(zero_end, sizeof(zero_end))); hide_detected();
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); publish("USD");

    initialize_auto(true); hide_detected();
    currency_state_begin_list_sync();
    CHECK(currency_state_append_list_code(1, "USD"));
    CHECK(currency_state_append_list_code(3, "EUR"));
    CHECK(!currency_state_finish_list_sync()); /* Invalid sync preserves cache. */
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); publish("CNY");
    CHECK(receive(zero_end, sizeof(zero_end))); hide_detected();
    currency_state_begin_list_sync(); CHECK(currency_state_append_list_code(1, "USD"));
    CHECK(currency_state_finish_list_sync()); effective("AUT");
    currency_state_begin_list_sync(); CHECK(currency_state_append_list_code(1, "USD"));
    CHECK(currency_state_append_list_code(2, "CNY")); CHECK(currency_state_finish_list_sync());
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); effective("AUT");
    CHECK(receive(detected_cny, sizeof(detected_cny))); publish("CNY");

    initialize_auto(true); hide_detected(); currency_state_reset();
    CHECK(currency_state_confirm_auto_selection());
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); effective("AUT");
    puts("PASS clear/backpressure/send failure, new detected code replacement, invalid catalog retention, supported catalog removal/re-add, state reset");
}

static void selection_and_lifecycle_cases(void)
{
    for (unsigned multi = 0; multi < 2; ++multi) {
        initialize_auto(true); hide_detected();
        if (multi) CHECK(currency_state_confirm_multi_selection());
        else { uint8_t index; CHECK(currency_state_find_code("USD", &index));
               CHECK(currency_state_confirm_active_selection(index, "USD")); }
        CHECK(currency_state_confirm_auto_selection());
        CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); effective("AUT");
    }
    initialize_auto(true); hide_detected(); CHECK(currency_state_leave_auto_selection());
    CHECK(currency_state_confirm_auto_selection());
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); effective("AUT");
    for (unsigned add = 0; add < 2; ++add) for (unsigned visible = 0; visible < 2; ++visible) {
        initialize_auto(true); machine_state_confirm_add(add != 0); main_visible = visible != 0;
        counting_data_mutable()->last_total_pcs = 2;
        hide_detected(); CHECK(receive(start_ok, sizeof(start_ok)));
        CHECK(locked && g_counting_session.start_confirmed && starts == 1);
        CHECK(receive(good_live, sizeof(good_live))); publish("CNY");
        receive_info(30, 3, 2, 1);
        receive_info(30, 3, 2, 2); publish("CNY");
        CHECK(!locked && ends == 1 && g_counting_session.analysis_valid_pcs == (add ? 1 : 3));
        CHECK(machine_state_mode() == MODE_MDC && machine_state_add_enabled() == (add != 0));
    }
    initialize_auto(true); CHECK(currency_state_confirm_multi_selection());
    CHECK(receive(start_ok, sizeof(start_ok)));
    CHECK(counting_data_current()->multi_currency_result);
    CHECK(receive(good_live, sizeof(good_live))); effective("MUL");
    CHECK(receive(zero_end, sizeof(zero_end))); effective("MUL");
    CHECK(!locked && starts == 1 && ends == 1);
    CHECK(receive(pocket_clear, sizeof(pocket_clear)));
    CHECK(counting_data_current()->multi_currency_result && counting_data_current()->total_pcs == 0);
    initialize_auto(true); CHECK(currency_state_leave_auto_selection());
    CHECK(receive(start_ok, sizeof(start_ok))); CHECK(receive(good_live, sizeof(good_live))); effective("CNY");
    CHECK(receive(zero_end, sizeof(zero_end)));
    CHECK(app_command_runtime_clear_counting_data("manual clear")); effective("CNY");
    puts("PASS AUTO/manual/MULTI transitions forget cached detection; ADD, visible/hidden Main, lock and island start/end/reset lifecycles preserved");
}

int main(void)
{
    reported_regression();
    guard_cases();
    reset_and_catalog_cases();
    selection_and_lifecycle_cases();
    counting_data_clear_serials(counting_data_mutable());
    counting_data_clear_errors(counting_data_mutable());
    puts("PASS production AUTO currency result integration");
    return 0;
}
