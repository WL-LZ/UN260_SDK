#include "app_boot_runtime.h"

#include <stddef.h>

#include "lvgl/lvgl.h"

#include "un260/app_service/app_counting_runtime.h"
#include "un260/boot/boot_reply.h"
#include "un260/boot/boot_service.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/counting_ui_runtime.h"

#define APP_BOOT_FINISH_DELAY_MS       2000
#define APP_BOOT_CURRENCY_LIST_CMD     0x56
#define APP_BOOT_CURRENCY_LIST_REQUEST 0x01

static lv_timer_t *g_boot_finish_timer = NULL;

static void app_boot_runtime_cancel_finish(void)
{
    if (g_boot_finish_timer == NULL) {
        return;
    }
    lv_timer_del(g_boot_finish_timer);
    g_boot_finish_timer = NULL;
}

static void app_boot_runtime_request_currency_list(void)
{
    const uint8_t request = APP_BOOT_CURRENCY_LIST_REQUEST;

    protocol_send(APP_BOOT_CURRENCY_LIST_CMD, &request, 1);
}

static void app_boot_runtime_send_handshake(uint32_t now_ms)
{
    const uint8_t payload = 0x01;

    boot_service_start(now_ms);
    boot_service_request_handshake(now_ms);
    protocol_send(0x01, &payload, 1);
}

static void app_boot_runtime_send_next_selftest(void)
{
    uint8_t protocol_step;

    boot_selftest_list_sync_step(boot_service_self_test_sequence_index());
    if (boot_service_next_self_test_protocol_step(&protocol_step)) {
        protocol_send(0x37, &protocol_step, 1);
    }
}

static void app_boot_runtime_finish(counting_session_state_t *counting_session)
{
    boot_selftest_list_finish();
    sim_data_init();
    app_counting_runtime_reset_session(counting_session, "boot finish");
    ui_manager_switch(ui_state_pure_count_is_enabled() ? UI_PAGE_PURE : UI_PAGE_MAIN);
}

static void app_boot_runtime_finish_timer_cb(lv_timer_t *timer)
{
    counting_session_state_t *counting_session;

    if (timer == NULL) {
        return;
    }
    counting_session = (counting_session_state_t *)timer->user_data;
    g_boot_finish_timer = NULL;
    app_boot_runtime_finish(counting_session);
    lv_timer_del(timer);
}

void app_boot_runtime_handle_reply(counting_session_state_t *counting_session,
                                   uint8_t cmd,
                                   const uint8_t *buf,
                                   uint8_t len)
{
    boot_reply_result_t reply = boot_reply_dispatch(cmd, buf, len);

    if (reply.kind == BOOT_REPLY_HANDSHAKE_ACCEPTED) {
        boot_progress_set(20);
        app_boot_runtime_send_next_selftest();
        return;
    }
    if (reply.kind != BOOT_REPLY_SELF_TEST_RECORDED) {
        return;
    }

    boot_selftest_list_set_result(reply.self_test_index, reply.self_test_result);
    boot_progress_set((uint8_t)(30 + reply.self_test_index * 10));

    if (reply.self_test_event == BOOT_SELF_TEST_EVENT_NONE) {
        app_boot_runtime_send_next_selftest();
    } else if (reply.self_test_event == BOOT_SELF_TEST_EVENT_SUCCESS) {
        boot_progress_set(100);
        if (g_boot_finish_timer != NULL) {
            return;
        }
        app_boot_runtime_request_currency_list();
        g_boot_finish_timer = lv_timer_create(app_boot_runtime_finish_timer_cb,
                                              APP_BOOT_FINISH_DELAY_MS,
                                              counting_session);
        if (g_boot_finish_timer == NULL) {
            app_boot_runtime_finish(counting_session);
        }
    } else if (reply.self_test_event == BOOT_SELF_TEST_EVENT_FAILURE) {
        app_boot_runtime_request_currency_list();
        show_boot_fault_popup(reply.first_failure_step, reply.first_failure_result);
    }
}

void app_boot_runtime_poll(uint32_t now_ms, bool boot_page_active)
{
    boot_service_action_t action;

    if (!boot_page_active) {
        app_boot_runtime_cancel_finish();
        return;
    }

    action = boot_service_poll(now_ms);
    if (action == BOOT_SERVICE_ACTION_SEND_HANDSHAKE) {
        app_boot_runtime_send_handshake(now_ms);
    } else if (action == BOOT_SERVICE_ACTION_HANDSHAKE_TIMEOUT) {
        show_boot_selftest_error_popup(
            "Controller handshake timeout.\nPress CONFIRM to enter sensor page.");
    } else if (action == BOOT_SERVICE_ACTION_SELF_TEST_TIMEOUT) {
        show_boot_selftest_error_popup(
            "Self-test timeout.\nPress CONFIRM to enter sensor page.");
    }
}
