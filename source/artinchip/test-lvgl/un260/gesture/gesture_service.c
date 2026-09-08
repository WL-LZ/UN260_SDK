#include "gesture_service.h"
#include "touch_feedback.h"
#include "lv_port_indev.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/innovation/page_32_innovation.h"
#include "un260/lv_core/page_06_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/lv_drivers/uart_io.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    bool started, captured, triggered, cancelled, releasing;
    uint8_t fingers;
    int side;
    lv_point_t start, contacts[2];
    int32_t ids[2];
    uint32_t tick;
    ui_page_t origin;
    gesture_action_t action;
} gesture_runtime_t;
static gesture_runtime_t g_runtime;
static ui_page_t g_pending_origin;
static bool g_pending;
static uint32_t g_pending_tick;
static const gesture_definition_t g_definitions[] = {
    {1, false, GESTURE_ACTION_EXIT_PAGE, UI_TEXT_GESTURE_EXIT_TITLE, UI_TEXT_GESTURE_EXIT_BODY},
    {2, false, GESTURE_ACTION_EXPORT, UI_TEXT_GESTURE_HOME_TITLE, UI_TEXT_GESTURE_HOME_BODY},
    {2, false, GESTURE_ACTION_HOME, UI_TEXT_GESTURE_HOME_TITLE, UI_TEXT_GESTURE_HOME_BODY},
};
static bool gesture_page_is_safe(ui_page_t page)
{
    return page != UI_PAGE_BOOT_ANIM && page != UI_PAGE_BOOT &&
           page != UI_PAGE_CIS_CALIB && page != UI_PAGE_MOTOR_TEST &&
           page != UI_PAGE_AGING_SETTING && page != UI_PAGE_FACTORY_SETTING &&
           page != UI_PAGE_UPGRADE && page != UI_PAGE_MAIN_UPGRADE &&
           page != UI_PAGE_IMAGE_UPGRADE && page != UI_PAGE_UI_UPGRADE;
}
static bool gesture_action_allowed(ui_page_t page, gesture_action_t action)
{
    if(action == GESTURE_ACTION_EXIT_PAGE)
        return page != UI_PAGE_BOOT_ANIM && page != UI_PAGE_BOOT;
    /* Global HOME/export must not bypass a maintenance page's ESC cleanup. */
    return gesture_page_is_safe(page);
}
static void gesture_navigate_async(void *user_data)
{
    gesture_action_t action = (gesture_action_t)(uintptr_t)user_data;
    ui_page_t page = ui_manager_get_current_page();
    uint32_t started = lv_tick_get();
    g_pending = false;
    if(!gesture_service_enabled() || page != g_pending_origin || !gesture_action_allowed(page, action)) return;
    if(action == GESTURE_ACTION_HOME) {
        if(page == UI_PAGE_MAIN) return;
        ui_manager_clear_stack();
        ui_manager_switch(UI_PAGE_MAIN);
    } else if(action == GESTURE_ACTION_EXPORT) {
        /* Reuse the existing USB export validation, filenames and error UI. */
        ui_export_data_request();
    } else if(page_32_innovation_request_back()) {
        return;
    } else if(page != UI_PAGE_MAIN) {
        lv_nav_back_result_t result = lv_nav_button_request_back();
        if(result != LV_NAV_BACK_NONE || !gesture_page_is_safe(page)) {
            /* If a maintenance page has no usable ESC, fail closed; never
             * silently replace its safety logic with a raw stack pop. */
            uart_debug_printf("GESTURE_NAV from=%u route=ESC result=%u wait_ms=%lu commit_ms=%lu\n",
                (unsigned)page, (unsigned)result,
                (unsigned long)(started-g_pending_tick), (unsigned long)lv_tick_elaps(started));
            return;
        }
        if(page == UI_PAGE_SETTING && page_06_settings_back_sub_page()) {
            uart_debug_printf("GESTURE_NAV from=%u local_back=1 wait_ms=%lu commit_ms=%lu\n",
                (unsigned)page, (unsigned long)(started-g_pending_tick),
                (unsigned long)lv_tick_elaps(started));
            return;
        }
        if(!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
    }
    uart_debug_printf("GESTURE_NAV from=%u to=%u wait_ms=%lu commit_ms=%lu\n",
        (unsigned)page, (unsigned)ui_manager_get_current_page(),
        (unsigned long)(started-g_pending_tick), (unsigned long)lv_tick_elaps(started));
}

static void gesture_queue(void)
{
    if(g_pending || g_runtime.origin != ui_manager_get_current_page()) return;
    g_pending_origin = g_runtime.origin;
    g_pending_tick = lv_tick_get();
    void *data = (void *)(uintptr_t)g_runtime.action;
    /* Leave the input callback safely, but never wait for the visual return.
     * The system-layer hint lives independently of the outgoing page. */
    g_pending = lv_async_call(gesture_navigate_async, data) == LV_RES_OK;
    printf("GESTURE action=%s fingers=%u edge=%d queued=%d\n",
        g_runtime.action == GESTURE_ACTION_HOME ? "HOME" :
        g_runtime.action == GESTURE_ACTION_EXPORT ? "EXPORT" : "BACK",
        g_runtime.fingers, g_runtime.side, g_pending);
    fflush(stdout);
}
static bool gesture_pointer_event(lv_indev_t *indev, lv_event_code_t event,
                                  const lv_point_t *point, uint8_t count, void *user_data)
{
    LV_UNUSED(indev); LV_UNUSED(user_data);
    touch_feedback_sample(point, count);
    if(event == LV_EVENT_RELEASED || count == 0) {
        bool captured = g_runtime.captured;
        touch_feedback_edge_hint(0, 0, 0);
        if(g_runtime.triggered && !g_runtime.cancelled) gesture_queue();
        memset(&g_runtime, 0, sizeof(g_runtime));
        return captured;
    }
    if(!gesture_service_enabled() || !point ||
       !gesture_action_allowed(ui_manager_get_current_page(), GESTURE_ACTION_EXIT_PAGE) ||
       gesture_guide_is_open()) return g_runtime.captured;
    if(count > 1 && !gesture_page_is_safe(ui_manager_get_current_page())) {
        g_runtime.cancelled = true;
        g_runtime.triggered = false;
        touch_feedback_edge_hint(0, 0, 0);
        return g_runtime.captured;
    }
    if(g_runtime.cancelled) return g_runtime.captured;
    if(count > 2) {
        g_runtime.captured = g_runtime.cancelled = true;
        touch_feedback_edge_hint(0, 0, 0);
        return true; /* No three-finger action; suppress the whole contact sequence. */
    }
    if(!g_runtime.started) {
        g_runtime.started = true;
        g_runtime.start = *point;
        g_runtime.tick = lv_tick_get();
        g_runtime.origin = ui_manager_get_current_page();
        g_runtime.fingers = 1;
        g_runtime.side = point->x <= 24 ? 1 : point->x >= 1255 ? -1 : 0;
    }
    if(g_pending || g_runtime.origin != ui_manager_get_current_page() ||
       lv_tick_elaps(g_runtime.tick) > 1800) {
        g_runtime.cancelled = true;
        touch_feedback_edge_hint(0, 0, 0);
        return g_runtime.captured;
    }
    if(count == 2 && g_runtime.fingers != 2) {
        g_runtime.captured = true; g_runtime.fingers = 2; g_runtime.side = 0;
        g_runtime.triggered = false;
        g_runtime.start = *point; g_runtime.tick = lv_tick_get();
        touch_feedback_edge_hint(0, 0, 0);
        if(lv_port_indev_touch_points(g_runtime.contacts, g_runtime.ids, 2) != 2)
            g_runtime.cancelled = true;
        return true;
    }
    if(g_runtime.fingers == 2 && count == 1) {
        /* Lifting fingers need not be simultaneous. Keep the recognized result
           but never interpret the remaining finger as a new edge gesture. */
        g_runtime.releasing = true;
        return true;
    }
    if(g_runtime.releasing && count == 2) {
        g_runtime.cancelled = true;
        return true; /* A new contact is not part of the original swipe. */
    }
    int dx = point->x - g_runtime.start.x;
    int dy = point->y - g_runtime.start.y;
    if(count == 1) {
        if(!g_runtime.side) return false;
        int inward = dx * g_runtime.side;
        if(abs(dy) > 64 || (abs(dy) > 18 && abs(dy) > abs(dx))) {
            g_runtime.cancelled = true;
            touch_feedback_edge_hint(0, 0, 0);
            return g_runtime.captured;
        }
        if(inward < 18 && !g_runtime.captured) return false;
        g_runtime.captured = true;
        g_runtime.triggered = inward >= 96 && lv_tick_elaps(g_runtime.tick) >= 60;
        g_runtime.action = GESTURE_ACTION_EXIT_PAGE;
        touch_feedback_edge_hint(g_runtime.side, inward, point->y);
        return true;
    }
    if(g_runtime.triggered) return true;
    if(abs(dy) < 64 || abs(dy)*2 < abs(dx)*3 || lv_tick_elaps(g_runtime.tick) < 60) return true;
    lv_point_t points[2]; int32_t ids[2];
    if(lv_port_indev_touch_points(points, ids, 2) != 2) return true;
    for(unsigned i=0; i<2; ++i) {
        unsigned j;
        for(j=0; j<2; ++j) if(ids[i] == g_runtime.ids[j]) break;
        if(j == 2) { g_runtime.cancelled = true; return true; }
        int fy = points[i].y - g_runtime.contacts[j].y;
        int fx = points[i].x - g_runtime.contacts[j].x;
        if(abs(fy) < 48 || (fy < 0) != (dy < 0) || abs(fy)*2 < abs(fx)*3) return true;
    }
    g_runtime.action = dy < 0 ? GESTURE_ACTION_EXPORT : GESTURE_ACTION_HOME;
    g_runtime.triggered = true;
    return true;
}
void gesture_service_init(void)
{
    memset(&g_runtime, 0, sizeof(g_runtime));
    touch_feedback_init();
    lv_port_indev_set_pointer_observer(gesture_pointer_event, NULL);
}
bool gesture_service_enabled(void) { return user_cfg_gesture_enabled(); }
bool gesture_service_set_enabled(bool enabled)
{
    bool ok = user_cfg_gesture_save(enabled);
    if(ok) {
        /* Keep a captured contact consumed until release; do not turn the
           rest of an in-flight gesture into an ordinary click. */
        if(g_runtime.captured) { g_runtime.cancelled = true; g_runtime.triggered = false; }
        else memset(&g_runtime, 0, sizeof(g_runtime));
        touch_feedback_edge_hint(0, 0, 0);
    }
    return ok;
}
size_t gesture_service_definition_count(void) { return sizeof(g_definitions) / sizeof(g_definitions[0]); }
const gesture_definition_t *gesture_service_definition(size_t index)
{
    return index < gesture_service_definition_count() ? &g_definitions[index] : NULL;
}
