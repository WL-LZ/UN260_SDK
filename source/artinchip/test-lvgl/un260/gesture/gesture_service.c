#include "gesture_service.h"

#include "lv_port_indev.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_system/user_cfg.h"

#include <string.h>

#define GESTURE_BOTTOM_START_Y 340
#define GESTURE_HOME_DISTANCE  82
#define GESTURE_EXIT_DISTANCE  68
#define GESTURE_MAX_HORIZONTAL 210

typedef struct {
    bool tracking;
    bool triggered;
    bool home_armed;
    bool exit_armed;
    lv_point_t start;
} gesture_runtime_t;

static gesture_runtime_t g_runtime;

static const gesture_definition_t g_definitions[] = {
    {
        .finger_count = 1,
        .starts_from_bottom = true,
        .action = GESTURE_ACTION_HOME,
        .title_text = UI_TEXT_GESTURE_HOME_TITLE,
        .body_text = UI_TEXT_GESTURE_HOME_BODY,
    },
    {
        .finger_count = 3,
        .starts_from_bottom = false,
        .action = GESTURE_ACTION_EXIT_PAGE,
        .title_text = UI_TEXT_GESTURE_EXIT_TITLE,
        .body_text = UI_TEXT_GESTURE_EXIT_BODY,
    },
};

static bool gesture_page_is_safe(ui_page_t page)
{
    return page != UI_PAGE_BOOT_ANIM && page != UI_PAGE_BOOT &&
           page != UI_PAGE_UPGRADE && page != UI_PAGE_MAIN_UPGRADE &&
           page != UI_PAGE_IMAGE_UPGRADE && page != UI_PAGE_UI_UPGRADE;
}

static void gesture_navigate_async(void *user_data)
{
    gesture_action_t action = (gesture_action_t)(uintptr_t)user_data;
    ui_page_t page = ui_manager_get_current_page();

    if(!gesture_page_is_safe(page)) return;
    gesture_guide_close(false);
    if(action == GESTURE_ACTION_HOME) {
        if(page == UI_PAGE_MAIN) return;
        ui_manager_clear_stack();
        ui_manager_switch(UI_PAGE_MAIN);
    } else if(page != UI_PAGE_MAIN) {
        if(!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
    }
}

static void gesture_trigger(lv_indev_t *indev, gesture_action_t action)
{
    if(g_runtime.triggered) return;
    g_runtime.triggered = true;
    lv_indev_wait_release(indev);
    lv_async_call(gesture_navigate_async, (void *)(uintptr_t)action);
}

static void gesture_pointer_event(lv_indev_t *indev,
                                  lv_event_code_t event_code,
                                  const lv_point_t *point,
                                  uint8_t touch_count,
                                  void *user_data)
{
    int dx;
    int upward;

    LV_UNUSED(user_data);
    if(!gesture_service_enabled() || point == NULL ||
       !gesture_page_is_safe(ui_manager_get_current_page()) ||
       gesture_guide_is_open()) {
        return;
    }

    if(event_code == LV_EVENT_PRESSED) {
        memset(&g_runtime, 0, sizeof(g_runtime));
        g_runtime.tracking = true;
        g_runtime.start = *point;
        g_runtime.home_armed = touch_count == 1 &&
                               point->y >= GESTURE_BOTTOM_START_Y;
        return;
    }
    if(!g_runtime.tracking) return;
    if(event_code == LV_EVENT_RELEASED || event_code == LV_EVENT_PRESS_LOST) {
        memset(&g_runtime, 0, sizeof(g_runtime));
        return;
    }
    if(event_code != LV_EVENT_PRESSING || g_runtime.triggered) return;

    if(touch_count >= 3 && !g_runtime.exit_armed) {
        g_runtime.exit_armed = true;
        g_runtime.home_armed = false;
        g_runtime.start = *point;
        return;
    }
    if(touch_count != 1) g_runtime.home_armed = false;

    dx = point->x - g_runtime.start.x;
    if(dx < 0) dx = -dx;
    upward = g_runtime.start.y - point->y;
    if(dx > GESTURE_MAX_HORIZONTAL) return;

    if(g_runtime.exit_armed && touch_count >= 3 &&
       upward >= GESTURE_EXIT_DISTANCE) {
        gesture_trigger(indev, GESTURE_ACTION_EXIT_PAGE);
    } else if(g_runtime.home_armed && touch_count == 1 &&
              upward >= GESTURE_HOME_DISTANCE) {
        gesture_trigger(indev, GESTURE_ACTION_HOME);
    }
}

void gesture_service_init(void)
{
    memset(&g_runtime, 0, sizeof(g_runtime));
    lv_port_indev_set_pointer_observer(gesture_pointer_event, NULL);
}

bool gesture_service_enabled(void)
{
    return user_cfg_gesture_enabled();
}

bool gesture_service_set_enabled(bool enabled)
{
    memset(&g_runtime, 0, sizeof(g_runtime));
    return user_cfg_gesture_save(enabled);
}

size_t gesture_service_definition_count(void)
{
    return sizeof(g_definitions) / sizeof(g_definitions[0]);
}

const gesture_definition_t *gesture_service_definition(size_t index)
{
    return index < gesture_service_definition_count() ?
           &g_definitions[index] : NULL;
}
