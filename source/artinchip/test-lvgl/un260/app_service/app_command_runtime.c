#include "un260/lv_system/ui_i18n.h"
#include "app_command_runtime.h"
#include "app_add_runtime.h"
#include "un260/lv_components/ui_notice.h"
#include "workspace_service.h"
#include "runtime_guard.h"
#include "ui_factory_reset_service.h"
#include "touch_calibration_service.h"
#include "app_fault_recovery.h"
#include "motor_test_service.h"
#include "work_mode_service.h"
#include "upgrade_session.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/machine_state/machine_state.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_multi_extra.h"
#include "app_standby_runtime.h"

#include <stdbool.h>
#include <stddef.h>

#include "lvgl/lvgl.h"

#include "un260/app_service/app_boot_runtime.h"
#include "un260/app_service/app_counting_runtime.h"
#include "un260/app_service/app_currency_runtime.h"
#include "un260/app_service/count_mode_service.h"
#include "un260/app_service/app_protocol_runtime.h"
#include "un260/app_service/app_setting_runtime.h"
#include "un260/boot/boot_service.h"
#include "un260/counting/counting_action_service.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_denom_query_service.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_session_state.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_10_debug.h"
#include "un260/lv_core/page_dynamic_capture.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/app_clock.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"
#include "un260/protocol/protocol_request.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/standby_store.h"

#define APP_COMMAND_MAX_FRAMES_PER_TICK 64
#define APP_COMMAND_FRAME_BUDGET_US 2000U

static counting_detail_state_t g_counting_detail_state;
static counting_session_state_t g_counting_session;
static uint32_t g_count_end_generation;
/* Empty-feeder motion owns the input lock, never a counting session.
 * New controllers finish with 0x0E; older controllers use 0x37. */
static bool g_no_note_motor_active;
static bool g_no_note_denom_pending;
/* After an acknowledged explicit group reset, idle late data cannot publish.
 * Currency refresh or the next real START establishes the next result scope. */
static bool g_add_result_cleared;
/* One indivisible transition frame retained ahead of the RX queue when its
 * predecessor's history cannot yet be preserved. Never dequeue past it. */
static protocol_frame_t g_deferred_frame;
static bool g_deferred_frame_valid;
static bool g_deferred_frame_blocked;
static bool g_deferred_frame_warning_reported;

static bool app_command_runtime_main_page_active(void)
{
    return ui_manager_get_current_page() == UI_PAGE_MAIN &&
           page_01_main_is_created();
}

static const char *diagnostic_operation_blocker(bool allow_calibration_feed)
{
    if(ui_page_dynamic_capture_is_capturing())
        return UI_N_("The previous capture is still being received. Please wait.");
    if(!work_mode_service_diagnostic_ready())return work_mode_service_status_msgid();
    if(!protocol_send_is_ready())return UI_N_("Controller connection is unavailable.");
    if(upgrade_session_owner()!=UPGRADE_SESSION_NONE)return UI_N_("Wait for the update to finish.");
    if(app_command_runtime_count_start_busy())return UI_N_("A count is already in progress.");
    if(app_add_runtime_busy())return UI_N_("Finish the current operation, then try again.");
    calibration_state_snapshot_t calibration;diagnostic_calibration_get_snapshot(&calibration);
    if(calibration.session_active &&
       !(allow_calibration_feed && diagnostic_calibration_allows_feed()))
        return UI_N_("Wait for calibration to finish.");
    if(machine_state_aging_running()||motor_test_service_busy())return UI_N_("Stop the motor test before running banknotes.");
    return NULL;
}
const char *app_command_runtime_calibration_blocker(void)
{
    return diagnostic_operation_blocker(false);
}
const char *app_command_runtime_diagnostic_run_blocker(void)
{
    const char *reason=diagnostic_operation_blocker(ui_manager_get_current_page()==UI_PAGE_CIS_CALIB);
    if(reason)return reason;
    /* Failed self-test enters service recovery; calibration itself must not
     * depend on ordinary count-ready sensors (the CIS bar occupies the path). */
    boot_stage_t boot=boot_service_get_stage();
    if(boot!=BOOT_STAGE_DONE&&boot!=BOOT_STAGE_FAIL)return UI_N_("Wait for the self-check to finish.");
    if(fault_popup_is_showing())return UI_N_("Close the machine guide before running.");
    return NULL;
}
bool app_command_runtime_request_diagnostic_run(void)
{
    return !app_command_runtime_diagnostic_run_blocker()&&app_command_runtime_request_count_start();
}
bool app_command_runtime_request_count_start(void)
{
    if (ui_factory_reset_blocks_navigation() || touch_calibration_busy()) return false;
    if (count_mode_service_busy()) return false;
    if (app_add_runtime_busy()) {
        ui_notice_post_text(UI_NOTICE_WARNING, "count.add-reset", UI_N_("Clear"),
                            UI_N_("Please try again."));
        return false;
    }
    if (upgrade_session_owner() != UPGRADE_SESSION_NONE ||
        app_command_runtime_count_start_busy() || counting_action_clear_pending() ||
        !counting_history_can_start()) return false;
    if (!app_fault_recovery_prepare_start()) return false;
    if (!counting_action_request_start()) {
        uart_debug_printf("count start request rejected or send failed\n");
        return false;
    }
    page_01_main_set_counting_locked(true);
    page_01_main_refresh_start_state();
    return true;
}

bool app_command_runtime_result_pending(void)
{
    return g_counting_session.history_record.valid;
}

uint32_t app_command_runtime_count_end_generation(void)
{
    return g_count_end_generation;
}

bool app_command_runtime_counting_confirmed(void)
{
    return g_counting_session.start_confirmed;
}

bool app_command_runtime_count_start_busy(void)
{
    return g_no_note_motor_active || counting_action_start_pending() || g_counting_session.start_confirmed ||
        g_counting_session.phase == COUNTING_SESSION_ACTIVE;
}

app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void)
{
    if (ui_factory_reset_blocks_navigation() || touch_calibration_busy()) return APP_UI_UPGRADE_BUSY;
    if (app_command_runtime_count_start_busy() || counting_action_clear_pending() ||
        app_add_runtime_busy() || counting_multi_current()->counting)
        return APP_UI_UPGRADE_BUSY;
    if (ui_history_data_is_initialized() && !ui_history_data_is_available())
        return APP_UI_UPGRADE_FAILED;
    if (g_deferred_frame_valid || protocol_frame_queue_has_pending() ||
        counting_multi_query_busy() || counting_multi_extra_busy())
        return APP_UI_UPGRADE_WAIT;
    if (!ui_history_data_is_initialized()) return APP_UI_UPGRADE_WAIT;

    uint32_t now_ms = app_clock_uptime_ms();
    if (workspace_store_busy() || standby_store_busy())
        return APP_UI_UPGRADE_WAIT;
    if (!workspace_store_ready() ||
        !standby_store_storage_ready())
        return APP_UI_UPGRADE_FAILED;

    bool captured = counting_history_prepare_reset(&g_counting_session,
                                                   counting_data_mutable(), now_ms);
    (void)counting_history_poll_commit(&g_counting_session,
                                       counting_data_mutable(), now_ms);
    storage_job_status_t history = ui_history_data_status();
    storage_job_status_t writes = storage_worker_pending_status();
    if (history == STORAGE_JOB_FAILED || writes == STORAGE_JOB_FAILED)
        return APP_UI_UPGRADE_FAILED;
    if (!captured || !counting_history_is_idle() || history == STORAGE_JOB_PENDING ||
        writes == STORAGE_JOB_PENDING || workspace_store_busy() ||
        standby_store_busy())
        return APP_UI_UPGRADE_WAIT;
    return APP_UI_UPGRADE_READY;
}

bool app_command_runtime_add_clear_allowed(void)
{
    return !app_command_runtime_count_start_busy() &&
        !counting_action_clear_pending() && protocol_send_is_ready() &&
        !runtime_guard_has_fault() && !ui_factory_reset_blocks_navigation() &&
        !touch_calibration_busy() && !workspace_service_applying() &&
        upgrade_session_owner() == UPGRADE_SESSION_NONE &&
        !work_mode_service_diagnostic_active() &&
        !machine_state_aging_running() && !motor_test_service_busy() &&
        /* A completed self-test failure remains available for diagnosis;
         * it does not make an idle ADD setting permanently unavailable. */
        (boot_service_get_stage() == BOOT_STAGE_DONE ||
         boot_service_get_stage() == BOOT_STAGE_FAIL) &&
        !g_deferred_frame_blocked &&
        /* A queued START must settle before clearing its count. Unrelated
         * replies (including touch sound ACKs) do not own the ADD operation. */
        !protocol_frame_queue_has_command_between(0x0A, 0, UINT64_MAX) &&
        !counting_multi_query_busy() && !counting_multi_extra_busy();
}

bool app_command_runtime_prepare_add_reset(void)
{
    if (!app_command_runtime_add_clear_allowed()) {
        uart_debug_printf("ADD admission blocked: count=%d clear=%d wire=%d fault=%d reset=%d touch=%d profile=%d upgrade=%d diagnostic=%d aging=%d motor=%d boot=%d deferred=%d start_rx=%d multi=%d extra=%d\n",
            app_command_runtime_count_start_busy(), counting_action_clear_pending(),
            protocol_send_is_ready(), runtime_guard_has_fault(),
            ui_factory_reset_blocks_navigation(), touch_calibration_busy(),
            workspace_service_applying(), upgrade_session_owner(),
            work_mode_service_diagnostic_active(), machine_state_aging_running(),
            motor_test_service_busy(), boot_service_get_stage(), g_deferred_frame_blocked,
            protocol_frame_queue_has_command_between(0x0A, 0, UINT64_MAX),
            counting_multi_query_busy(), counting_multi_extra_busy());
        return false;
    }
    bool ready = counting_history_prepare_reset(&g_counting_session,
        counting_data_current(), app_clock_uptime_ms());
    if (!ready) uart_debug_printf("ADD admission blocked: pending history snapshot could not be preserved\n");
    return ready;
}

/* Revoke publication, while allowing an already-open untagged query to drain. */
static void app_command_runtime_revoke_result_replies(void)
{
    /* Keep an old untagged 0B query's drain ownership, but revoke publication. */
    const bool drain_query = g_counting_detail_state.query_pending;
    const uint32_t query_tick = g_counting_detail_state.query_tick;
    g_counting_detail_state = (counting_detail_state_t){0};
    g_counting_detail_state.query_pending = drain_query;
    g_counting_detail_state.query_expired = drain_query;
    g_counting_detail_state.query_failed = true;
    g_counting_detail_state.query_tick = query_tick;
    g_add_result_cleared = true;
}

bool app_command_runtime_commit_add_reset(void)
{
    if (app_command_runtime_count_start_busy()) return false;
    if (!app_counting_runtime_reset_session(&g_counting_session, "ADD group change"))
        return false;
    app_setting_runtime_cancel_mode_clear();
    stop_counting_sim();
    app_command_runtime_revoke_result_replies();
    sim_reset_counting_group(counting_data_mutable());
    currency_state_clear_detected_result();
    if (currency_state_multi_selected())
        counting_data_mark_multi_result(counting_data_mutable());
    page_01_curr_img_refre();
    page_01_main_refresh_start_state();
    return true;
}

bool app_command_runtime_clear_counting_data(const char *reason)
{
    if (app_add_runtime_busy()) return app_add_runtime_retry_clear();
    /* Preserve the current model when the controller channel is uncertain. */
    if (g_no_note_motor_active || !protocol_send_is_ready() || runtime_guard_has_fault()) return false;
    if (!app_counting_runtime_reset_session(&g_counting_session, reason)) {
        smart_island_notify_warning_level(UI_N_("History full: clear deferred"),
                                           SMART_ISLAND_WARNING_LEVEL_ERROR);
        return false;
    }
    app_setting_runtime_cancel_mode_clear();
    stop_counting_sim();
    app_command_runtime_revoke_result_replies();
    sim_reset_counting_result(counting_data_mutable());
    currency_state_clear_detected_result();
    page_01_curr_img_refre();
    const bool clear_requested = counting_action_request_clear();
    page_01_main_set_counting_locked(false);
    /* CLEAR cancels a pending START even when its own send fails. Reflect the
     * settled request state, not the pre-clear state seen by the data reset. */
    page_01_main_refresh_start_state();
    if (!clear_requested) {
        uart_debug_printf("count clear request rejected or send failed reason=%s\n",
                    reason != NULL ? reason : "unknown");
        return false;
    }
    return true;
}

/* RX 0x51/6: controller confirms the genuine-note pocket was cleared.
 * Never echo CLEAR: this is a notification, not a UI clear request. */
static bool app_command_runtime_handle_stacker_clear(const uint8_t *buf, uint8_t len)
{
    if (!protocol_frame_is_valid(buf, len) || len != 6 ||
        buf[3] != 0x51 || buf[4] != 0x01) return true;
    app_fault_recovery_stacker_cleared();
    if (currency_state_auto_selected() &&
        !g_counting_session.start_confirmed &&
        g_counting_session.phase != COUNTING_SESSION_ACTIVE) {
        currency_state_clear_detected_result();
        page_01_curr_img_refre();
    }
    if (!currency_state_multi_selected()) return true;
    /* The controller must notify before START. Do not block the RX queue
     * behind an out-of-order event, which would also block the end frame. */
    if (g_counting_session.start_confirmed ||
        g_counting_session.phase == COUNTING_SESSION_ACTIVE ||
        counting_multi_current()->counting) {
        uart_debug_printf("MULTI pocket clear ignored: received after START\n");
        return true;
    }
    if (!app_counting_runtime_reset_session(&g_counting_session, "MULTI pocket clear"))
        return false;
    sim_reset_counting_result(counting_data_mutable());
    counting_data_mark_multi_result(counting_data_mutable());
    return true;
}

static bool app_command_runtime_handle_no_note_motion(uint8_t cmd,const uint8_t *buf,uint8_t len)
{
    if (!protocol_frame_is_valid(buf,len) || buf[3] != cmd) return false;
    if (cmd == 0x0A && len == 7 && !work_mode_service_diagnostic_active()) {
        if (buf[4] == 1 && buf[5] == 2 &&
            !g_counting_session.start_confirmed &&
            g_counting_session.phase != COUNTING_SESSION_ACTIVE) {
            g_no_note_motor_active = true;
            g_no_note_denom_pending = true;
            page_01_main_set_counting_locked(true);
            /* Let the normal failure hook display the no-note warning. */
        } else if (buf[4] == 1 && buf[5] == 1) {
            /* After NO NOTE this acknowledges empty motor motion. Consume it
             * before history/report/start hooks can create a counted result. */
            if (g_no_note_motor_active) return true;
            g_no_note_denom_pending = false;
        } else if ((!g_no_note_motor_active || buf[4] == 1 || buf[4] == 2) &&
                   !g_counting_session.start_confirmed &&
                   g_counting_session.phase != COUNTING_SESSION_ACTIVE) {
            /* A subsequent START refusal aborts the empty-motion attempt;
             * no motor-end report is guaranteed after a channel fault.
             * Keep draining any empty denomination tail until the next START. */
            if (g_no_note_motor_active) {
                g_no_note_motor_active = false;
                smart_island_notify_count_reset();
            }
            page_01_main_set_counting_locked(false);
        }
    }
    if (g_no_note_motor_active && cmd == 0x0E && (len == 13 || len == 16)) {
        uint8_t status = buf[len == 16 ? 14 : 11];
        if (len == 13 && status >= 2 && status <= 5) {
            g_no_note_motor_active = false;
            page_01_main_set_counting_locked(false);
            smart_island_notify_count_reset();
        }
        return true;
    }
    /* The controller sends a zero denomination snapshot after empty motion.
     * Drain through END without replacing the previous result or scheduling
     * reject/serial queries. A subsequent genuine START owns a new stream. */
    if (g_no_note_denom_pending && cmd == 0x0B && len == 16) {
        /* Explicit queries keep their existing wire ownership. */
        if (g_counting_detail_state.query_pending || counting_multi_query_busy()) {
            g_no_note_denom_pending = false;
            return false;
        }
        bool end = true;
        for (unsigned i = 4; i < 15; ++i) end = end && buf[i] == 0xFF;
        if (end) g_no_note_denom_pending = false;
        return true;
    }
    if (g_no_note_motor_active && cmd == 0x37 && len == 7 && buf[4] == 2 &&
        (buf[5] == 1 || buf[5] == 2)) {
        g_no_note_motor_active = false;
        g_no_note_denom_pending = false;
        page_01_main_set_counting_locked(false);
        smart_island_notify_count_reset();
        if (buf[5] == 2) fault_popup_report_boot_result(2,2);
        return true;
    }
    return false;
}

static bool app_command_runtime_dispatch(uint8_t cmd,
                                         uint8_t *buf,
                                         uint16_t frame_len)
{
    if (app_fault_recovery_observe_setting_detail(
            cmd, buf, frame_len, &g_counting_session)) return true;
    if (cmd == 0x47 || cmd == 0x48)
        return app_protocol_runtime_handle_reply(cmd, buf, frame_len);
    /* Every other controller reply keeps its original one-byte length ABI. */
    if (frame_len > PROTOCOL_FRAME_MAX_SIZE) return true;
    uint8_t len = (uint8_t)frame_len;
    /* Keep the transition ahead of generic reply side effects. Returning
     * false retains this frame and holds the next START until history is safe. */
    if (cmd == 0x51) return app_command_runtime_handle_stacker_clear(buf, len);
    if (cmd == 0x3D) {
        if (protocol_frame_is_valid(buf, len) && len == 6 &&
            (buf[4] == 0x01 || buf[4] == 0x02))
            app_fault_recovery_handle_clear_result(buf[4]);
        return true;
    }
    if (app_command_runtime_handle_no_note_motion(cmd,buf,len)) return true;
    if (cmd == 0x0A && len == 7 && buf[4] == 0x01 && buf[5] == 0x01) {
        count_mode_service_count_started();
        app_setting_runtime_cancel_mode_clear();
    }
    /* Preflight BEFORE taking request results or invoking any dispatcher. A
     * retried frame therefore cannot duplicate protocol/UI side effects. */
    if (cmd == 0x0A && len == 7 && buf[4] == 0x01 && buf[5] == 0x01 &&
        !app_add_runtime_before_count_start()) return false;
    if (cmd == 0x0A && len >= 7 && buf[4] == 0x01 && buf[5] == 0x01 &&
        !counting_history_prepare_start(&g_counting_session, counting_data_mutable(),
                                        app_clock_uptime_ms())) {
        /* The controller is already running even while history delays model
         * publication. Protect Main now; replay will enter the normal start
         * hook and only an accepted end report can release the gate. */
        page_01_main_set_counting_locked(true);
        return false;
    }
    if (cmd == 0x03 && len >= 6 &&
        (buf[4] == 0x01 || (buf[4] == 0x03 && len >= 9)) &&
        !counting_history_prepare_reset(&g_counting_session, counting_data_mutable(),
                                        app_clock_uptime_ms())) return false;
    if (cmd == 0x0A && len >= 7 && buf[4] == 1 && buf[5] == 1) {
        g_add_result_cleared = false;
        counting_report_begin(machine_state_add_enabled(), counting_data_mutable());
        /* Interrupted snapshots cannot publish into a new run. The report
         * owner still drains any already-open untagged reply through END. */
        counting_denom_reply_begin_count(&g_counting_detail_state);
        g_counting_detail_state.live_reject_started = false;
    }
    if (counting_report_discard_stale(cmd, buf, len)) {
        if (cmd == 0x0C) g_counting_detail_state.live_reject_started = false;
        return true;
    }
    counting_action_handle_reply(cmd, buf, len);
    /* Settle CLEAR before the next queued hardware START can own a new run. */
    if (cmd == 0x3B) {
        app_add_runtime_poll();
        return true;
    }
    if (g_add_result_cleared &&
        (cmd == 0x0B || cmd == 0x0C || cmd == 0x0D || cmd == 0x0E ||
         (cmd == 0x49 && len == 0x18))) {
        if (cmd == 0x0B && g_counting_detail_state.query_pending) {
            bool end = len == 16;
            for (unsigned i = 4; end && i < 15; ++i) end = buf[i] == 0xFF;
            if (end) (void)counting_denom_query_complete(&g_counting_detail_state);
        }
        return true;
    }

    /* New 0x49/8 currency detection shares a command with 0x49/24 serials. */
    if (cmd == 0x49 && len == 8) {
        app_currency_runtime_handle_detected(&g_counting_detail_state, buf, len);
        return true;
    }
    /* 0x49/0x18 is the controller's live serial-number frame. */
    if (cmd == 0x49 && len == 0x18) {
        app_counting_runtime_handle_detail(cmd,
                                           &g_counting_detail_state,
                                           &g_counting_session,
                                           counting_data_mutable(),
                                           buf,
                                           len);
        return true;
    }

    if (app_setting_runtime_handle_reply(cmd, buf, len) ||
        app_protocol_runtime_handle_reply(cmd, buf, len)) {
        return true;
    }

    switch (cmd) {
    case 0x01:
    case 0x37:
        app_boot_runtime_handle_reply(&g_counting_session, cmd, buf, len);
        break;
    case 0x03:
        app_currency_runtime_handle_reply(&g_counting_detail_state,
                                          &g_counting_session,
                                          buf,
                                          len);
        /* Only an accepted currency transition owns a new denomination push.
         * A failure/unsolicited ACK or idle AUTO detection does not reopen it. */
        if (g_add_result_cleared && g_counting_detail_state.query_wait_push &&
            !g_counting_detail_state.query_expired)
            g_add_result_cleared = false;
        break;
    case 0x50:
        app_currency_runtime_handle_detected(&g_counting_detail_state, buf, len);
        break;
    case 0x0E: {
        const bool armed = g_counting_session.start_confirmed;
        const counting_session_phase_t previous = g_counting_session.phase;
        app_counting_runtime_handle_info(&g_counting_session, counting_data_mutable(), buf, len);
        if (armed && previous != COUNTING_SESSION_FINISHED_WAIT_START &&
            !g_counting_session.start_confirmed &&
            g_counting_session.phase == COUNTING_SESSION_FINISHED_WAIT_START)
            ++g_count_end_generation;
        break;
    }
    case 0x0F:
    case 0x0A:
        app_counting_runtime_handle_control(cmd,
                                            &g_counting_session,
                                            buf,
                                            len);
        break;
    case 0x0B:
        app_counting_runtime_handle_denom(&g_counting_detail_state,
                                          &g_counting_session,
                                          counting_data_mutable(),
                                          buf,
                                          len);
        break;
    case 0x0D:
    case 0x0C:
        app_counting_runtime_handle_detail(cmd,
                                           &g_counting_detail_state,
                                           &g_counting_session,
                                           counting_data_mutable(),
                                           buf,
                                           len);
        break;
    default:
        uart_debug_printf("Unknown command 0x%02X\n", cmd);
        break;
    }
    return true;
}

void app_command_runtime_process_frames(void)
{
    (void)app_command_runtime_process_frames_budget(APP_COMMAND_FRAME_BUDGET_US);
}

bool app_command_runtime_has_command_between(uint8_t command,
                                              uint64_t started_ms,
                                              uint64_t deadline_ms)
{
    return g_deferred_frame_valid && started_ms < deadline_ms &&
           g_deferred_frame.data[3] == command &&
           g_deferred_frame.received_ms >= started_ms &&
           g_deferred_frame.received_ms < deadline_ms;
}

bool app_command_runtime_frames_pending(void)
{
    /* Blocked work retries on the existing <=10ms loop ceiling; do not turn a
     * disk failure into a busy loop merely because later UART frames exist. */
    if (g_deferred_frame_valid) return !g_deferred_frame_blocked;
    return protocol_frame_queue_has_pending();
}

uint32_t app_command_runtime_process_frames_budget(uint32_t budget_us)
{
    uint32_t processed = 0;
    uint64_t started_us = app_clock_monotonic_us();

    while (processed < APP_COMMAND_MAX_FRAMES_PER_TICK &&
           (processed == 0U ||
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()) < budget_us)) {
        uint8_t *buf;
        uint16_t len;
        bool fresh = !g_deferred_frame_valid;
        if (fresh) {
            if (!protocol_frame_queue_pop(&g_deferred_frame)) break;
            g_deferred_frame_valid = true;
        }
        buf = g_deferred_frame.data;
        len = g_deferred_frame.len;
        if (fresh && debug_page_rx_log_is_active()) {
            char hex_log[256];

            protocol_frame_format_hex(buf, len, hex_log, sizeof(hex_log));
            debug_append_rx_log(hex_log);
        }

        if (len < PROTOCOL_FRAME_MIN_SIZE) {
            uart_debug_printf("Queued frame dropped: invalid len=%u\n", len);
            g_deferred_frame_valid = false;
            g_deferred_frame_warning_reported = false;
            processed++;
            continue;
        }

        if (fresh) app_standby_runtime_protocol_activity();
        protocol_request_result_context_begin(g_deferred_frame.received_ms);
        bool dispatched = app_command_runtime_dispatch(buf[3], buf, len);
        protocol_request_result_context_end();
        if (!dispatched) {
            /* Initial history loading is normal backpressure, not a storage
             * fault. Keep the frame and the bounded wait, but leave its warning
             * unconsumed so a failed load/full queue can report after loading. */
            if (!g_deferred_frame_warning_reported && ui_history_data_is_initialized()) {
                const bool available = ui_history_data_is_available();
                uart_debug_printf("RX transition paused: history %s; retaining frame and session\n",
                                  available ? "full" : "unavailable");
                smart_island_notify_warning_level(available ? UI_N_("History full: receiving paused") :
                                                   UI_N_("History unavailable: receiving paused"),
                                                   SMART_ISLAND_WARNING_LEVEL_ERROR);
                g_deferred_frame_warning_reported = true;
            }
            g_deferred_frame_blocked = true;
            break;
        }
        const counting_sim_t *count = counting_data_current();
        runtime_guard_count(count->total_pcs, count->total_amount,
                            g_counting_session.phase,
                            app_command_runtime_count_start_busy(),
                            app_command_runtime_result_pending(), count->multi_currency_result);
        g_deferred_frame_valid = false;
        g_deferred_frame_blocked = false;
        g_deferred_frame_warning_reported = false;
        processed++;
    }
    if (processed) page_01_main_refresh_start_state();
    return processed;
}

void app_command_runtime_poll(uint32_t now_ms)
{
    app_counting_runtime_poll_reports(&g_counting_session, now_ms);
    motor_test_service_poll(now_ms);
    app_fault_recovery_poll();
    boot_stage_t stage = boot_service_get_stage();
    uint32_t action_timeouts = counting_action_take_timeouts();

    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_START) != 0U) {
        uart_debug_printf("count start request timeout\n");
        if (!app_command_runtime_count_start_busy()) page_01_main_set_counting_locked(false);
        page_01_main_refresh_start_state();
    }
    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_CLEAR) != 0U) {
        uart_debug_printf("count clear request timeout\n");
    }

    const bool add_was_busy = app_add_runtime_busy();
    app_add_runtime_poll();
    if (add_was_busy != app_add_runtime_busy()) page_01_main_refresh_start_state();
    if (!app_add_runtime_busy() && app_setting_runtime_take_mode_clear()) {
        (void)app_command_runtime_clear_counting_data("mode change");
    }
    count_mode_service_poll();
    app_counting_runtime_poll_history(&g_counting_session, counting_data_mutable(), now_ms);
    uint32_t multi_revision = counting_multi_current()->revision;
    counting_multi_poll(now_ms);
    counting_multi_extra_poll(now_ms);
    if (multi_revision != counting_multi_current()->revision)
        ui_refresh_main_page();
    if (app_add_runtime_busy() || g_add_result_cleared ||
        currency_state_multi_selected() || counting_data_current()->multi_currency_result ||
        counting_multi_query_busy() || counting_multi_extra_busy()) return;
    counting_denom_query_poll(&g_counting_detail_state,
                              now_ms,
                              stage == BOOT_STAGE_DONE || stage == BOOT_STAGE_FAIL,
                              app_command_runtime_main_page_active());
}
