#include "app_counting_runtime.h"
#include "un260/counting/counting_multi_extra.h"
#include "un260/counting/counting_multi.h"
#include "un260/lv_system/app_clock.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_reject_analysis_service.h"
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/currency/currency_state.h"
#include "un260/data_collection/data_collection.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_frame_commit.h"
#include "un260/lv_core/page_01_main_detail.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_02_list.h"
#include "un260/lv_core/page_06_settings.h"
#include "un260/lv_core/page_19_history.h"
#include "un260/lv_core/page_31_get_wave.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/ui_text.h"
#include "un260/innovation/page_32_innovation.h"
#include "un260/innovation/multi_pass_verification.h"

typedef struct {
    counting_session_state_t *session;
    counting_sim_t *sim_data;
} app_counting_detail_context_t;

static void app_counting_runtime_format_amount(char *dest,
                                               size_t dest_size,
                                               float amount)
{
    char temp[32];
    int len;
    int dest_index = 0;
    int i;

    if (dest == NULL || dest_size == 0U) {
        return;
    }

    lv_snprintf(temp, sizeof(temp), "%.0f", amount);
    len = (int)strlen(temp);
    if (len <= 3) {
        lv_snprintf(dest, dest_size, "%s", temp);
        return;
    }

    for (i = 0; i < len && dest_index < (int)dest_size - 1; i++) {
        dest[dest_index++] = temp[i];
        if (i < len - 1 && ((len - i - 1) % 3) == 0 &&
            dest_index < (int)dest_size - 1) {
            dest[dest_index++] = ',';
        }
    }
    dest[dest_index] = '\0';
}

static void app_counting_visual_commit(void *context, uint32_t flags)
{
    char amount_buf[32];
    const counting_sim_t *sim_data = counting_data_current();
    (void)context;
    if (!page_01_main_is_visible()) {
        page_01_main_mark_dirty(PAGE_01_MAIN_DIRTY_COUNTING);
        return;
    }
    if ((flags & 1U) && sim_data) {
        app_counting_runtime_format_amount(amount_buf,
                                       sizeof(amount_buf),
                                       sim_data->total_amount);
        page_01_main_refresh_totals(sim_data->total_pcs, amount_buf);
    }
    if ((flags & 4U) || ((flags & 2U) && page_01_detail_section_get() == PAGE_01_DETAIL_SECTION_B))
        page_01_main_detail_refresh_rows_only();
}

/* Protocol/model and island start/end messages remain ordered. Only the
 * visible projection of totals/rows is last-state-wins within a UI frame. */
static void app_counting_runtime_refresh_compact(const counting_sim_t *sim_data)
{
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    page_02_list_section_mark_dirty(PAGE_02_SECTION_C);
    if (!ui_frame_commit_defer(app_counting_visual_commit, NULL, 1U))
        app_counting_visual_commit(NULL, 1U);
    smart_island_update_counting(sim_data->total_pcs,
                                 sim_data->total_amount);
}

static bool app_counting_runtime_main_page_active(void)
{
    return ui_manager_get_current_page() == UI_PAGE_MAIN &&
           page_01_main_is_created();
}

static bool app_counting_runtime_should_keep_current_page(void)
{
    ui_page_t page = ui_manager_get_current_page();

    return page == UI_PAGE_DEBUG ||
           page == UI_PAGE_IMAGE_GET ||
           page == UI_PAGE_WAVE_GET ||
           page == UI_PAGE_SENSOR;
}

static bool app_counting_runtime_cb_calibration_active(void)
{
    calibration_state_snapshot_t calibration;

    diagnostic_calibration_get_snapshot(&calibration);
    return calibration.session_active &&
           calibration.target == CALIB_TARGET_CB;
}

static void app_counting_runtime_on_start_success(const uint8_t *buf, uint8_t len)
{
    const bool previous_multi = counting_data_current()->multi_currency_result;
    currency_state_begin_count_session();
    if (currency_state_multi_selected()) {
        if(!previous_multi)counting_multi_reset();
        counting_multi_begin(machine_state_add_enabled());
    }
    else counting_multi_reset();
    if (currency_state_multi_selected())
        counting_data_mark_multi_result(counting_data_mutable());
    page_02_list_report_reset();
    page_01_curr_img_refre();
    hide_counting_error_popup();
    fault_popup_clear_pending();
    fault_popup_reset_auto_retry();
    counting_history_session_start(buf, len);
    if (multi_pass_verification_is_active()) {
        if (machine_state_add_enabled()) {
            multi_pass_capture_event_t verification_event = { 0 };
            multi_pass_verify_view_t verification_view;

            multi_pass_verification_on_count_start(true);
            multi_pass_verification_get_view(&verification_view);
            verification_event.kind = MULTI_PASS_CAPTURE_ADD_REQUIRED;
            verification_event.captured_passes = verification_view.captured_passes;
            verification_event.target_passes = verification_view.target_passes;
            page_32_innovation_notify_verification_event(&verification_event);
        } else {
            multi_pass_verification_on_count_start(false);
        }
    }

    if (data_collection_state_mode() != DATA_COLLECT_MODE_NONE) {
        data_collection_state_set_status("Counting started...");
        page_06_data_collection_refresh();
    } else if (!app_counting_runtime_cb_calibration_active() &&
               ui_manager_get_current_page() != UI_PAGE_PURE &&
               !app_counting_runtime_should_keep_current_page()) {
        ui_manager_switch(UI_PAGE_MAIN);
    }
    smart_island_notify_count_start();
}

static void app_counting_runtime_on_error_frame(const char *tag,
                                                const uint8_t *buf,
                                                uint8_t len)
{
    counting_history_capture_error(tag, buf, len);
}

static const char *app_counting_runtime_start_ui_error_desc(uint8_t code)
{
    const char *description;

    if (code == 0x00) {
        return ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN);
    }
    description = machine_start_error_desc(code);
    if (description != NULL) {
        return description;
    }
    return ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR);
}

static void app_counting_runtime_on_start_failure(uint8_t type, uint8_t code)
{
    char status[160];
    const char *description;

    if (type == 0x01 && code == 0x02) {
        description = "No banknotes detected";
        fault_popup_report_start_no_note();
        uart_debug_printf("0x0A start fail (no note)\n");
        smart_island_notify_warning_level(
            ui_text_get(UI_TEXT_WIDGET_FAULT_NO_NOTE_MAIN),
            SMART_ISLAND_WARNING_LEVEL_WARNING);
    } else if (type == 0x01 || type == 0x02) {
        description = get_counting_error_desc(type, code);
        fault_popup_report_start_fault(type, code);
        uart_debug_printf(type == 0x01
                              ? "0x0A start fail (normal): val=%02X desc=%s\n"
                              : "0x0A start fail (fault): code=%02X desc=%s\n",
                          code, description);
        smart_island_notify_warning_level(
            app_counting_runtime_start_ui_error_desc(code),
            SMART_ISLAND_WARNING_LEVEL_ERROR);
    } else {
        smart_island_notify_warning_level(
            ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_COUNT_ERROR),
            SMART_ISLAND_WARNING_LEVEL_ERROR);
        return;
    }

    if (data_collection_state_mode() == DATA_COLLECT_MODE_NONE) {
        return;
    }

    snprintf(status,
             sizeof(status),
             "Start failed: %s",
             description);
    data_collection_state_set_status(status);
    page_06_data_collection_refresh();
}

static void app_counting_runtime_on_runtime_fault(uint8_t code)
{
    if (code == 0x00) {
        hide_fault_popup();
        fault_popup_clear_pending();
        fault_popup_reset_auto_retry();
        system_error_state_reset();
        smart_island_restore_idle();
        return;
    }

    fault_popup_report_runtime_fault(code);
    uart_debug_printf("0x0F fault=0x%02X %s\n",
                      code, get_system_error_desc(code));
    smart_island_notify_warning_level(get_system_error_desc(code),
                                      SMART_ISLAND_WARNING_LEVEL_ERROR);
}

static const counting_control_reply_hooks_t g_counting_control_hooks = {
    .on_start_success = app_counting_runtime_on_start_success,
    .on_error_frame = app_counting_runtime_on_error_frame,
    .on_start_failure = app_counting_runtime_on_start_failure,
    .on_runtime_fault = app_counting_runtime_on_runtime_fault,
};

static void app_counting_runtime_on_main_data_changed(void)
{
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    ui_refresh_main_page();
}

static void app_counting_runtime_on_live_denom_changed(void)
{
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    if (!ui_frame_commit_defer(app_counting_visual_commit, NULL, 4U))
        app_counting_visual_commit(NULL, 4U);
}

static const counting_denom_reply_hooks_t g_counting_denom_hooks = {
    .on_history_frame = counting_history_append_frame,
    .on_main_data_changed = app_counting_runtime_on_main_data_changed,
};

bool app_counting_runtime_reset_session(counting_session_state_t *session,
                                        const char *reason)
{
    if (session == NULL) {
        return false;
    }
    if (!counting_history_prepare_reset(session, counting_data_current(), lv_tick_get())) {
        uart_debug_printf("session reset deferred: unsaved history retained reason=%s\n",
                          reason != NULL ? reason : "unknown");
        return false;
    }
    counting_multi_reset();
    memset(session, 0, sizeof(*session));
    ui_count_end_anim_cancel();
    /* Late 0x0E frames are now rejected by the reset session. Its visual
     * owner must stop here as well, rather than waiting for that end frame. */
    smart_island_notify_count_reset();
    return true;
}

static void app_counting_runtime_on_detail_history(void *context,
                                                   const char *tag,
                                                   const uint8_t *buf,
                                                   uint8_t len)
{
    (void)context;
    counting_history_append_frame(tag, buf, len);
}

static void app_counting_runtime_on_reject_report_changed(void *context)
{
    (void)context;
    page_02_list_section_data_ready(PAGE_02_SECTION_C);
}

static void app_counting_runtime_on_summary_changed(void *context,
                                                    bool refresh_main)
{
    (void)context;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
    page_02_list_section_mark_dirty(PAGE_02_SECTION_C);
    smart_island_refresh_summary();
    if (refresh_main && app_counting_runtime_main_page_active()) {
        app_counting_detail_context_t *detail_context = context;
        if (detail_context && detail_context->session->phase == COUNTING_SESSION_ACTIVE) {
            if (!ui_frame_commit_defer(app_counting_visual_commit, NULL, 4U))
                app_counting_visual_commit(NULL, 4U);
        } else ui_refresh_main_page();
    }
}

static void app_counting_runtime_on_serial_data_started(void *context)
{
    (void)context;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    app_counting_detail_context_t *detail_context = context;
    if (!detail_context || detail_context->session->phase != COUNTING_SESSION_ACTIVE)
        page_01_main_scroll_reset();
}

static void app_counting_runtime_on_serial_report_ready(void *context)
{
    (void)context;
    page_02_list_section_data_ready(PAGE_02_SECTION_B);
}

static void app_counting_runtime_on_serial_ui_complete(void *context,
                                                       bool begin_end_anim)
{
    (void)context;
    if (app_counting_runtime_main_page_active()) {
        ui_refresh_main_page();
    }
    if (begin_end_anim) {
        ui_count_end_anim_begin(NULL);
    }
}

static void app_counting_runtime_on_serial_item_changed(void *context)
{
    (void)context;
    page_02_list_section_mark_dirty(PAGE_02_SECTION_B);
    if (app_counting_runtime_main_page_active() &&
        page_01_detail_section_get() == PAGE_01_DETAIL_SECTION_B) {
        if (!ui_frame_commit_defer(app_counting_visual_commit, NULL, 2U))
            app_counting_visual_commit(NULL, 2U);
    }
}

static void app_counting_runtime_on_live_serial_received(
    void *context,
    int denomination,
    const char *serial_number)
{
    (void)context;
    smart_island_notify_serial_number(denomination, serial_number);
}

static void app_counting_runtime_on_reject_analysis(void *context)
{
    app_counting_detail_context_t *detail_context = context;
    counting_reject_analysis_result_t result;

    if (detail_context == NULL ||
        !counting_reject_analysis_update(detail_context->session,
                                         detail_context->sim_data,
                                         &result)) {
        return;
    }

    smart_island_set_count_analysis(detail_context->session->analysis_valid_pcs,
                                    result.suspect_pcs,
                                    result.damaged_pcs);
    uart_debug_printf(                "count analysis valid=%d expected=%d current=%u delta=%u source=%s suspect=%d damaged=%d\n",
                detail_context->session->analysis_valid_pcs,
                result.expected_issue,
                result.current_total,
                result.delta_total,
                result.source == COUNTING_REJECT_ANALYSIS_SOURCE_DELTA ?
                    "delta" : "current",
                result.suspect_pcs,
                result.damaged_pcs);
}

static void app_counting_runtime_report_history_commit(
    const counting_session_state_t *session,
    counting_history_commit_result_t result,
    uint8_t previous_attempts)
{
    if (session == NULL) {
        return;
    }

    if (result == COUNTING_HISTORY_COMMIT_RETRY_PENDING) {
        uart_debug_printf("history save failed, retry scheduled attempt=%u\n",
                    session->history_record.save_attempts);
    } else if (result == COUNTING_HISTORY_COMMIT_FAILED) {
        uart_debug_printf("history storage failed or full; pending records retained, new starts blocked\n");
        smart_island_notify_warning_level("History save pending: storage unavailable",
                                           SMART_ISLAND_WARNING_LEVEL_ERROR);
    } else if (result == COUNTING_HISTORY_COMMIT_SAVED && previous_attempts > 0) {
        uart_debug_printf("history save recovered after %u retries\n",
                    previous_attempts);
    }
    if (result == COUNTING_HISTORY_COMMIT_SAVED &&
        ui_manager_get_current_page() == UI_PAGE_HISTORY) {
        ui_page_19_history_refresh();
    }
}

static void app_counting_runtime_try_history_commit(
    counting_session_state_t *session,
    const counting_sim_t *sim_data,
    uint32_t now_ms)
{
    counting_history_commit_result_t result;
    uint8_t previous_attempts;

    if (session == NULL || sim_data == NULL) {
        return;
    }

    previous_attempts = session->history_record.save_attempts;
    result = counting_history_try_commit(session, sim_data, now_ms);
    app_counting_runtime_report_history_commit(session, result,
                                               previous_attempts);
}

static void app_counting_runtime_on_history_record(void *context)
{
    app_counting_detail_context_t *detail_context = context;

    if (detail_context != NULL) {
        app_counting_runtime_try_history_commit(detail_context->session,
                                                detail_context->sim_data,
                                                lv_tick_get());
    }
}

static void app_counting_runtime_on_detail_complete(void *context)
{
    app_counting_detail_context_t *detail_context = context;

    if (detail_context != NULL) {
        app_counting_runtime_handle_detail_complete(detail_context->session);
    }
}

static void app_counting_runtime_schedule_auto_wave(counting_session_state_t *session)
{
    session->auto_wave_pending = false;
    if (machine_state_work_mode() != 0 ||
        ui_manager_get_current_page() != UI_PAGE_WAVE_GET) {
        return;
    }

    session->auto_wave_pending = true;
    uart_debug_printf("auto wave scheduled after count\n");
}

void app_counting_runtime_handle_info(counting_session_state_t *session,
                                      counting_sim_t *sim_data,
                                      const uint8_t *buf,
                                      uint8_t len)
{
    counting_info_reply_result_t result;

    if (session == NULL || sim_data == NULL) {
        return;
    }

    uint8_t normalized[13] = {0};
    const bool multi = sim_data->multi_currency_result;
    if (multi) {
        if (!session->start_confirmed || !counting_multi_info(buf, len)) return;
        const counting_multi_t *m = counting_multi_current();
        memcpy(normalized, buf, 4); normalized[2] = 13;
        normalized[8] = (uint8_t)(m->total_pcs >> 8);
        normalized[9] = (uint8_t)m->total_pcs;
        normalized[10] = m->reject;
        normalized[11] = buf[len == 16 ? 14 : 11] >= 2 ? 2 : 1;
        /* MULTI has no meaningful aggregate monetary amount. */
        sim_data->total_amount = 0;
        sim_data->total_pcs = (int)m->total_pcs;
    } else if (len != 13) {
        return;
    }
    result = counting_info_reply_handle(session, sim_data, multi ? normalized : buf, multi ? 13 : len,
                                        ui_history_total_notes_counted_get());
    if (result.kind == COUNTING_INFO_REPLY_LIVE) {
        app_counting_runtime_refresh_compact(sim_data);
        counting_history_append_frame("0x0E", buf, len);
    } else if (result.kind == COUNTING_INFO_REPLY_FINISHED) {
        int current_pcs = result.final_pcs;

        page_02_list_section_mark_dirty(PAGE_02_SECTION_A);
        page_02_list_section_mark_dirty(PAGE_02_SECTION_C);
        uart_debug_printf("Count finished\n");
        counting_history_capture_end(buf, len);
        if (!multi && machine_state_add_enabled() &&
            sim_data->last_total_pcs > 0 &&
            current_pcs >= sim_data->last_total_pcs) {
            current_pcs -= sim_data->last_total_pcs;
        }
        /* 0x0E qty is the accepted-note count; issue is reported separately. */
        session->analysis_valid_pcs = current_pcs;
        smart_island_set_count_analysis(session->analysis_valid_pcs,
                                        result.final_issue,
                                        0);
        /* The transport status, not the optional detail stream, owns the
         * island lifecycle.  Enter COMPLETE as soon as 0x0E says finished;
         * serial/reject details may continue to settle in the background. */
        smart_island_update_counting(current_pcs, result.final_amount);
        smart_island_notify_count_end(NULL);
        session->end_anim_wait_detail = false;
        app_counting_runtime_try_history_commit(session, sim_data,
                                                lv_tick_get());
        if (app_counting_runtime_main_page_active()) {
            ui_refresh_main_page();
        }
        app_counting_runtime_schedule_auto_wave(session);
    }
}

void app_counting_runtime_handle_control(uint8_t cmd,
                                         counting_session_state_t *session,
                                         const uint8_t *buf,
                                         uint8_t len)
{
    if (session == NULL) {
        return;
    }

    counting_control_reply_dispatch(cmd,
                                    session,
                                    buf,
                                    len,
                                    &g_counting_control_hooks);
}

void app_counting_runtime_handle_denom(counting_detail_state_t *detail_state,
                                       counting_session_state_t *session,
                                       counting_sim_t *sim_data,
                                       const uint8_t *buf,
                                       uint8_t len)
{
    if (detail_state == NULL || session == NULL || sim_data == NULL) {
        return;
    }

    if (sim_data->multi_currency_result || currency_state_multi_selected() ||
        counting_multi_query_busy()) {
        counting_multi_denom(buf, len, app_clock_uptime_ms());
        app_counting_runtime_on_main_data_changed();
        return;
    }
    counting_denom_reply_hooks_t hooks = g_counting_denom_hooks;
    if (session->phase == COUNTING_SESSION_ACTIVE)
        hooks.on_main_data_changed = app_counting_runtime_on_live_denom_changed;
    counting_denom_reply_handle(detail_state,
                                session,
                                sim_data,
                                buf,
                                len,
                                &hooks);
}

void app_counting_runtime_handle_detail(uint8_t cmd,
                                        counting_detail_state_t *detail_state,
                                        counting_session_state_t *session,
                                        counting_sim_t *sim_data,
                                        const uint8_t *buf,
                                        uint8_t len)
{
    app_counting_detail_context_t context;
    counting_reject_sn_reply_hooks_t hooks = { 0 };
    counting_detail_reply_result_t result;

    if (detail_state == NULL || session == NULL || sim_data == NULL) {
        return;
    }

    if (counting_multi_extra_reply(cmd,buf,len,app_clock_uptime_ms())) {
        app_counting_runtime_on_main_data_changed();
        return;
    }
    context.session = session;
    /* MULTI serial/reject detail frames have no active currency owner here.
     * Do not run the single-currency 0C -> 0D query chain or publish them as
     * single-currency report rows. The 0E global reject count remains valid. */
    if (sim_data->multi_currency_result || currency_state_multi_selected()) return;
    context.sim_data = sim_data;
    hooks.context = &context;
    hooks.on_history_frame = app_counting_runtime_on_detail_history;
    hooks.on_reject_analysis_ready = app_counting_runtime_on_reject_analysis;
    hooks.on_reject_report_changed = app_counting_runtime_on_reject_report_changed;
    hooks.on_summary_changed = app_counting_runtime_on_summary_changed;
    hooks.on_serial_data_started = app_counting_runtime_on_serial_data_started;
    hooks.on_serial_report_ready = app_counting_runtime_on_serial_report_ready;
    hooks.on_serial_ui_complete = app_counting_runtime_on_serial_ui_complete;
    hooks.on_serial_item_changed = app_counting_runtime_on_serial_item_changed;
    hooks.on_live_serial_received = app_counting_runtime_on_live_serial_received;
    hooks.on_history_record_ready = app_counting_runtime_on_history_record;
    hooks.on_detail_complete = app_counting_runtime_on_detail_complete;

    result = counting_reject_sn_reply_dispatch(cmd,
                                               detail_state,
                                               session,
                                               sim_data,
                                               buf,
                                               len,
                                               &hooks);
    if (cmd == 0x0C && result == COUNTING_DETAIL_REPLY_START) {
        page_02_list_section_mark_dirty(PAGE_02_SECTION_C);
    }
}

void app_counting_runtime_handle_detail_complete(counting_session_state_t *session)
{
    multi_pass_capture_event_t verification_event;
    multi_pass_capture_kind_t verification_result;
    bool sent;

    if (session == NULL) {
        return;
    }

    verification_result = multi_pass_verification_capture(
        session, counting_data_current(), &verification_event);
    if (verification_result != MULTI_PASS_CAPTURE_IGNORED) {
        page_32_innovation_notify_verification_event(&verification_event);
    }

    if (!session->auto_wave_pending) return;
    session->auto_wave_pending = false;

    if (machine_state_work_mode() != 0 ||
        ui_manager_get_current_page() != UI_PAGE_WAVE_GET) {
        return;
    }

    sent = ui_page_31_get_wave_request();
    uart_debug_printf("auto wave after count: sent=%d\n", sent ? 1 : 0);
}

void app_counting_runtime_poll_history(counting_session_state_t *session,
                                       const counting_sim_t *sim_data,
                                       uint32_t now_ms)
{
    counting_history_commit_result_t result;
    uint8_t previous_attempts;

    /* Completion/rollback publication is on the UI thread; the storage worker
     * never touches LVGL. Also handles history-page edits without an active count. */
    if (ui_history_data_poll(now_ms) && ui_manager_get_current_page() == UI_PAGE_HISTORY)
        ui_page_19_history_refresh();
    if (session == NULL || sim_data == NULL) {
        return;
    }

    previous_attempts = session->history_record.save_attempts;
    result = counting_history_poll_commit(session, sim_data, now_ms);
    app_counting_runtime_report_history_commit(session, result,
                                               previous_attempts);
}
