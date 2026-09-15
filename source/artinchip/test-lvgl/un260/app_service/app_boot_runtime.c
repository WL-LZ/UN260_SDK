#include "app_boot_runtime.h"

#include <stddef.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "un260/app_service/app_counting_runtime.h"
#include "un260/boot/boot_reply.h"
#include "un260/boot/boot_service.h"
#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_08_boot.h"
#include "un260/lv_core/page_00_boot_anim.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/counting_ui_runtime.h"

#define APP_BOOT_FINISH_DELAY_MS       2000
#define APP_BOOT_PREWARM_PERIOD_MS       80
#define APP_BOOT_CURRENCY_LIST_CMD     0x56
#define APP_BOOT_CURRENCY_LIST_REQUEST 0x01

static lv_timer_t *g_boot_finish_timer = NULL;
static counting_session_state_t *g_deferred_boot_finish;
static bool g_boot_runtime_active;
static bool g_boot_prewarm_active = false;
static size_t g_boot_prewarm_cursor = 0;
static uint32_t g_boot_prewarm_due_ms = 0;
#if defined(UI_BOOT_ANIM_THEME) && UI_BOOT_ANIM_THEME == 4
static uint32_t g_boot_prewarm_deadline_ms;
#endif

static const ui_page_t g_boot_prewarm_pages[] = {
    UI_PAGE_MENU,
    UI_PAGE_LIST,
    UI_PAGE_HISTORY,
    UI_PAGE_SETTING,
    UI_PAGE_PURE,
    UI_PAGE_INNOVATION_CENTER,
    /* Currency should precede MAIN when its controller-provided catalog is
     * ready.  The scheduler below skips a temporarily unavailable page and
     * retries it later, so this dependency can never block MAIN prewarm. */
    UI_PAGE_CURR,
    /* MAIN has the largest remaining cold-create peak.  Build its retained
     * object tree and decode the full-screen background while SELF_TEST is
     * still visible, then activate it through the normal resume path. */
    UI_PAGE_MAIN,
};

static bool g_boot_prewarm_done[
    sizeof(g_boot_prewarm_pages) / sizeof(g_boot_prewarm_pages[0])];

static void app_boot_runtime_cancel_prewarm(void)
{
    g_boot_prewarm_active = false;
}

static bool app_boot_runtime_time_reached(uint32_t now_ms,
                                          uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void app_boot_runtime_start_prewarm(void)
{
    app_boot_runtime_cancel_prewarm();
    memset(g_boot_prewarm_done, 0, sizeof(g_boot_prewarm_done));
    g_boot_prewarm_cursor = 0;
    g_boot_prewarm_due_ms = lv_tick_get() + APP_BOOT_PREWARM_PERIOD_MS;
    g_boot_prewarm_active = true;
#if defined(UI_BOOT_ANIM_THEME) && UI_BOOT_ANIM_THEME == 4
    g_boot_prewarm_deadline_ms = lv_tick_get() + 4000U;
#endif
}

static void app_boot_runtime_poll_prewarm(uint32_t now_ms)
{
    const size_t page_count =
        sizeof(g_boot_prewarm_pages) / sizeof(g_boot_prewarm_pages[0]);
    size_t checked = 0;
    size_t completed = 0;

    if (!g_boot_prewarm_active ||
        !app_boot_runtime_time_reached(now_ms, g_boot_prewarm_due_ms)) {
        return;
    }

    /* A controller-backed page can be temporarily unavailable.  Scan past it
     * instead of blocking every independent page behind it, but still perform
     * at most one expensive page construction per application loop. */
#if defined(UI_BOOT_ANIM_THEME) && UI_BOOT_ANIM_THEME == 4
    /* Cold page construction must not compete with the Ready choreography.
     * Keep protocol polling live, then warm pages during the static hold. */
    if (!ui_page_08_curr_visual_is_quiet()) return;
#endif
    while (checked < page_count) {
        size_t index = g_boot_prewarm_cursor;

        g_boot_prewarm_cursor = (g_boot_prewarm_cursor + 1U) % page_count;
        checked++;
        if (g_boot_prewarm_done[index]) {
            continue;
        }
        if (ui_manager_prewarm_page(g_boot_prewarm_pages[index])) {
            g_boot_prewarm_done[index] = true;
            break;
        }
    }

    for (size_t i = 0; i < page_count; i++) {
        if (g_boot_prewarm_done[i]) {
            completed++;
        }
    }
    if (completed >= page_count) {
        app_boot_runtime_cancel_prewarm();
        return;
    }

    /* Rebase after the work itself, not from the timestamp captured before
     * it, so an expensive page can never make the next page immediately due. */
    g_boot_prewarm_due_ms = lv_tick_get() + APP_BOOT_PREWARM_PERIOD_MS;
}

static void app_boot_runtime_cancel_finish(void)
{
    g_deferred_boot_finish = NULL;
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

static bool app_boot_runtime_finish(counting_session_state_t *counting_session)
{
    if (!app_counting_runtime_reset_session(counting_session, "boot finish")) {
        g_deferred_boot_finish = counting_session;
        return false;
    }
    g_deferred_boot_finish = NULL;
    app_boot_runtime_cancel_prewarm();
    boot_selftest_list_finish();
    /* The controller may have completed (or still be sending) its denomination
     * query during self-test. Boot handoff must not replace that data with the
     * simulator's local currency table. Session reset above owns only session state. */
#if defined(UI_BOOT_ANIM_THEME) && UI_BOOT_ANIM_THEME == 4
    ui_page_08_curr_start_handoff();
#endif
    ui_manager_switch(ui_state_pure_count_is_enabled() ? UI_PAGE_PURE : UI_PAGE_MAIN);
    return true;
}

static void app_boot_runtime_finish_timer_cb(lv_timer_t *timer)
{
    counting_session_state_t *counting_session;

    if (timer == NULL) {
        return;
    }
    if (ui_manager_get_current_page() != UI_PAGE_BOOT ||
        boot_service_get_stage() != BOOT_STAGE_DONE) {
        g_boot_finish_timer = NULL;
        g_deferred_boot_finish = NULL;
        app_boot_runtime_cancel_prewarm();
        lv_timer_del(timer);
        return;
    }
    counting_session = (counting_session_state_t *)timer->user_data;
#if defined(UI_BOOT_ANIM_THEME) && UI_BOOT_ANIM_THEME == 4
    if (g_boot_prewarm_active &&
        !app_boot_runtime_time_reached(lv_tick_get(), g_boot_prewarm_deadline_ms)) {
        lv_timer_set_period(timer, 20);
        return;
    }
#endif
    if (!app_boot_runtime_finish(counting_session)) return;
    g_boot_finish_timer = NULL;
    lv_timer_del(timer);
}

void app_boot_runtime_handle_reply(counting_session_state_t *counting_session,
                                   uint8_t cmd,
                                   const uint8_t *buf,
                                   uint8_t len)
{
    if (ui_manager_get_current_page() != UI_PAGE_BOOT ||
        ui_page_00_boot_anim_is_active() ||
        !boot_service_reply_window_open(app_clock_uptime_ms())) return;
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
        app_boot_runtime_start_prewarm();
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

    if (!boot_page_active ||
        ui_manager_get_current_page() != UI_PAGE_BOOT) {
        if (g_boot_runtime_active) boot_service_cancel();
        g_boot_runtime_active = false;
        app_boot_runtime_cancel_finish();
        app_boot_runtime_cancel_prewarm();
        return;
    }
    g_boot_runtime_active = true;

    if (g_deferred_boot_finish != NULL && g_boot_finish_timer == NULL)
        (void)app_boot_runtime_finish(g_deferred_boot_finish);

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

    app_boot_runtime_poll_prewarm(now_ms);
}
