#include "app_command_runtime.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_multi_extra.h"
#include "app_standby_runtime.h"

#include <stdbool.h>
#include <stddef.h>

#include "lvgl/lvgl.h"

#include "un260/app_service/app_boot_runtime.h"
#include "un260/app_service/app_counting_runtime.h"
#include "un260/app_service/app_currency_runtime.h"
#include "un260/app_service/app_protocol_runtime.h"
#include "un260/app_service/app_setting_runtime.h"
#include "un260/boot/boot_service.h"
#include "un260/counting/counting_action_service.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_denom_query_service.h"
#include "un260/counting/counting_session_state.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_10_debug.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/app_clock.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"

#define APP_COMMAND_MAX_FRAMES_PER_TICK 64
#define APP_COMMAND_FRAME_BUDGET_US 2000U

static counting_detail_state_t g_counting_detail_state;
static counting_session_state_t g_counting_session;
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

bool app_command_runtime_request_count_start(void)
{
    if (app_command_runtime_count_start_busy()) return false;
    if (!counting_action_request_start()) {
        uart_debug_printf("count start request rejected or send failed\n");
        return false;
    }
    page_01_main_refresh_start_state();
    return true;
}

bool app_command_runtime_count_start_busy(void)
{
    return counting_action_start_pending() || g_counting_session.start_confirmed ||
        g_counting_session.phase == COUNTING_SESSION_ACTIVE;
}

bool app_command_runtime_clear_counting_data(const char *reason)
{
    if (!app_counting_runtime_reset_session(&g_counting_session, reason)) {
        smart_island_notify_warning_level("History full: clear deferred",
                                           SMART_ISLAND_WARNING_LEVEL_ERROR);
        return false;
    }
    app_setting_runtime_cancel_mode_clear();
    stop_counting_sim();
    g_counting_detail_state.wait_sn_after_reject_end = false;
    sim_reset_counting_result(counting_data_mutable());
    currency_state_begin_count_session();
    page_01_curr_img_refre();
    const bool clear_requested = counting_action_request_clear();
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

static bool app_command_runtime_dispatch(uint8_t cmd,
                                         uint8_t *buf,
                                         uint8_t len)
{
    /* Preflight BEFORE taking request results or invoking any dispatcher. A
     * retried frame therefore cannot duplicate protocol/UI side effects. */
    if (cmd == 0x0A && len >= 7 && buf[4] == 0x01 && buf[5] == 0x01 &&
        !counting_history_prepare_start(&g_counting_session, counting_data_mutable(),
                                        app_clock_uptime_ms())) return false;
    if (cmd == 0x03 && len >= 6 &&
        (buf[4] == 0x01 || (buf[4] == 0x03 && len >= 9)) &&
        !counting_history_prepare_reset(&g_counting_session, counting_data_mutable(),
                                        app_clock_uptime_ms())) return false;
    counting_action_handle_reply(cmd, buf, len);

    /* New 0x49/8 currency detection shares a command with 0x49/24 serials. */
    if (cmd == 0x49 && len == 8) {
        app_currency_runtime_handle_detected(buf, len);
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
        break;
    case 0x50:
        app_currency_runtime_handle_detected(buf, len);
        break;
    case 0x0E:
        app_counting_runtime_handle_info(&g_counting_session, counting_data_mutable(), buf, len);
        break;
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
        uint8_t len;
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
        if (!app_command_runtime_dispatch(buf[3], buf, len)) {
            /* Initial history loading is normal backpressure, not a storage
             * fault. Keep the frame and the bounded wait, but leave its warning
             * unconsumed so a failed load/full queue can report after loading. */
            if (!g_deferred_frame_warning_reported && ui_history_data_is_initialized()) {
                const bool available = ui_history_data_is_available();
                uart_debug_printf("RX transition paused: history %s; retaining frame and session\n",
                                  available ? "full" : "unavailable");
                smart_island_notify_warning_level(available ? "History full: receiving paused" :
                                                   "History unavailable: receiving paused",
                                                   SMART_ISLAND_WARNING_LEVEL_ERROR);
                g_deferred_frame_warning_reported = true;
            }
            g_deferred_frame_blocked = true;
            break;
        }
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
    boot_stage_t stage = boot_service_get_stage();
    uint32_t action_timeouts = counting_action_take_timeouts();

    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_START) != 0U) {
        uart_debug_printf("count start request timeout\n");
        page_01_main_refresh_start_state();
    }
    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_CLEAR) != 0U) {
        uart_debug_printf("count clear request timeout\n");
    }

    if (app_setting_runtime_take_mode_clear()) {
        (void)app_command_runtime_clear_counting_data("mode change");
    }
    app_counting_runtime_poll_history(&g_counting_session, counting_data_mutable(), now_ms);
    uint32_t multi_revision = counting_multi_current()->revision;
    counting_multi_poll(now_ms);
    counting_multi_extra_poll(now_ms);
    if (multi_revision != counting_multi_current()->revision)
        ui_refresh_main_page();
    if (currency_state_multi_selected() || counting_data_current()->multi_currency_result ||
        counting_multi_query_busy() || counting_multi_extra_busy()) return;
    counting_denom_query_poll(&g_counting_detail_state,
                              now_ms,
                              stage == BOOT_STAGE_DONE || stage == BOOT_STAGE_FAIL,
                              app_command_runtime_main_page_active());
}
