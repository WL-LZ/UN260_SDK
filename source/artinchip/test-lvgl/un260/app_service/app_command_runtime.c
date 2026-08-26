#include "app_command_runtime.h"

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
#include "un260/counting/counting_denom_query_service.h"
#include "un260/counting/counting_session_state.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_07_curr.h"
#include "un260/lv_core/page_10_debug.h"
#include "un260/lv_drivers/lv_drivers.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/protocol/protocol_frame.h"
#include "un260/protocol/protocol_frame_queue.h"

#define APP_COMMAND_MAX_FRAMES_PER_TICK 64

static counting_detail_state_t g_counting_detail_state;
static counting_session_state_t g_counting_session;

static bool app_command_runtime_main_page_active(void)
{
    return ui_manager_get_current_page() == UI_PAGE_MAIN &&
           page_01_main_is_created();
}

bool app_command_runtime_request_count_start(void)
{
    if (!counting_action_request_start()) {
        uart_debug_printf("count start request rejected or send failed\n");
        return false;
    }
    return true;
}

bool app_command_runtime_clear_counting_data(const char *reason)
{
    app_setting_runtime_cancel_mode_clear();
    stop_counting_sim();
    app_counting_runtime_reset_session(&g_counting_session, reason);
    g_counting_detail_state.wait_sn_after_reject_end = false;
    sim_reset_counting_result(counting_data_mutable());
    currency_state_begin_count_session();
    page_01_curr_img_refre();
    if (!counting_action_request_clear()) {
        uart_debug_printf("count clear request rejected or send failed reason=%s\n",
                    reason != NULL ? reason : "unknown");
        return false;
    }
    return true;
}

static void app_command_runtime_dispatch(uint8_t cmd,
                                         uint8_t *buf,
                                         uint8_t len)
{
    counting_action_handle_reply(cmd, buf, len);

    /* 0x49/0x18 is the controller's live serial-number frame. */
    if (cmd == 0x49 && len == 0x18) {
        app_counting_runtime_handle_detail(cmd,
                                           &g_counting_detail_state,
                                           &g_counting_session,
                                           counting_data_mutable(),
                                           buf,
                                           len);
        return;
    }

    if (app_setting_runtime_handle_reply(cmd, buf, len) ||
        app_protocol_runtime_handle_reply(cmd, buf, len)) {
        return;
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
}

void app_command_runtime_process_frames(void)
{
    protocol_frame_t frame;
    int processed = 0;

    while (processed < APP_COMMAND_MAX_FRAMES_PER_TICK &&
           protocol_frame_queue_pop(&frame)) {
        uint8_t *buf = frame.data;
        uint8_t len = frame.len;

        processed++;
        if (debug_page_rx_log_is_active()) {
            char hex_log[256];

            protocol_frame_format_hex(buf, len, hex_log, sizeof(hex_log));
            debug_append_rx_log(hex_log);
        }

        if (len < PROTOCOL_FRAME_MIN_SIZE) {
            uart_debug_printf("Queued frame dropped: invalid len=%u\n", len);
            continue;
        }

        app_command_runtime_dispatch(buf[3], buf, len);
    }
}

void app_command_runtime_poll(uint32_t now_ms)
{
    boot_stage_t stage = boot_service_get_stage();
    uint32_t action_timeouts = counting_action_take_timeouts();

    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_START) != 0U) {
        uart_debug_printf("count start request timeout\n");
    }
    if ((action_timeouts & COUNTING_ACTION_TIMEOUT_CLEAR) != 0U) {
        uart_debug_printf("count clear request timeout\n");
        page_07_curr_cancel_pending_selection();
    }

    if (app_setting_runtime_take_mode_clear()) {
        if (!app_command_runtime_clear_counting_data("mode change")) {
            page_07_curr_cancel_pending_selection();
        }
    }
    page_07_curr_poll_selection();
    app_counting_runtime_poll_history(&g_counting_session, counting_data_mutable(), now_ms);
    counting_denom_query_poll(&g_counting_detail_state,
                              now_ms,
                              stage == BOOT_STAGE_DONE || stage == BOOT_STAGE_FAIL,
                              app_command_runtime_main_page_active());
}
