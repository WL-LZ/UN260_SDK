#include "un260/lv_components/smart_island/smart_island_internal.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_capsule_pagination.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_18_pure.h"
#include "un260/lv_system/ui_qr_data.h"
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_system/ui_text.h"

#define SMART_ISLAND_ACTION_BTN_W          221
#define SMART_ISLAND_ACTION_BTN_H          54
#define SMART_ISLAND_ACTION_BTN_X          20
#define SMART_ISLAND_ACTION_BTN_Y          29
#define SMART_ISLAND_PAGE_SLIDE_OFFSET     SMART_ISLAND_WIDTH

#define SMART_ISLAND_BTN_BG_TOP            0x1C1C1E
#define SMART_ISLAND_BTN_BG_BOT            0x111111
#define SMART_ISLAND_BTN_TEXT              0xFFFFFF
#define SMART_ISLAND_BTN_BORDER            0x2C2C2E
#define SMART_ISLAND_BTN_ARROW             0x8E8E93
#define SMART_ISLAND_BTN_SWITCH_ON_TOP      0x234A34
#define SMART_ISLAND_BTN_SWITCH_ON_BOT      0x1A3528
#define SMART_ISLAND_BTN_SWITCH_ON_TEXT     0xB8F5C7
#define SMART_ISLAND_BTN_SWITCH_OFF_TOP     0x2A2A2C
#define SMART_ISLAND_BTN_SWITCH_OFF_BOT     0x202022
#define SMART_ISLAND_BTN_SWITCH_OFF_TEXT    0xB0B0B2

static void smart_island_action_btn_touch_anim_cb(lv_event_t *event);
static void smart_island_action_btn_set_pressed_visual(lv_obj_t *btn, bool pressed);
static void smart_island_action_page_slide_anim_finish_cb(lv_anim_t *animation);
static void smart_island_page_slide_anim_finish_cb(lv_anim_t *animation);
static void smart_island_raise_compact_header(void);
static uint8_t smart_island_page_indicator_count_get(void);
static uint8_t smart_island_page_indicator_active_get(void);
static void smart_island_open_info_page_by_left_swipe(void);
static void smart_island_open_action_last_page_by_right_swipe(void);
static void smart_island_expand_if_needed(bool anim_en);
static void smart_island_swipe_cb(lv_event_t *event);
static void smart_island_action_btn_cb(lv_event_t *event);
static void smart_island_show_qr_error_toast(const char *text);
static void smart_island_show_qr_popup(void);
static void smart_island_page_slide_anim(smart_island_page_t old_page,
                                         smart_island_page_t new_page);
static void smart_island_page_apply_now(smart_island_page_t page);
static void smart_island_page_apply_anim(smart_island_page_t page);
static void smart_island_action_btn_style_apply(uint8_t index);
static void smart_island_action_item_apply(uint8_t index);
static void smart_island_action_page_slide_anim(uint8_t old_index, uint8_t new_index);
static void smart_island_action_page_set_index(uint8_t index, bool anim_en);

static void smart_island_action_btn_touch_anim_cb(lv_event_t *e)
{
    lv_obj_t *btn;
    lv_event_code_t code;

    if (e == NULL) {
        return;
    }

    btn = lv_event_get_target(e);
    if (btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }

    code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_translate_y(btn, 1, 0);
        smart_island_action_btn_set_pressed_visual(btn, true);
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_translate_y(btn, 0, 0);
        smart_island_action_btn_set_pressed_visual(btn, false);
    }
}

static void smart_island_action_btn_set_pressed_visual(lv_obj_t *btn, bool pressed)
{
    uint8_t i;
    lv_obj_t *label = NULL;
    lv_obj_t *arrow = NULL;

    if (btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }

    lv_obj_set_style_bg_opa(btn, pressed ? LV_OPA_70 : LV_OPA_COVER, 0);

    for (i = 0; i < SMART_ISLAND_ACTION_PAGE_COUNT; i++) {
        if (g_si_ctx.objects.action_buttons[i] == btn) {
            label = g_si_ctx.objects.action_labels[i];
            arrow = g_si_ctx.objects.action_arrows[i];
            break;
        }
    }

    if (label && lv_obj_is_valid(label)) {
        lv_obj_set_style_text_opa(label, pressed ? LV_OPA_70 : LV_OPA_COVER, 0);
    }

    if (arrow && lv_obj_is_valid(arrow)) {
        lv_obj_set_style_text_opa(arrow, pressed ? LV_OPA_70 : LV_OPA_COVER, 0);
    }
}

static void smart_island_action_page_slide_anim_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);
    g_si_ctx.view.anim_running = false;
    g_si_ctx.action.ignore_action_click_once = false;
}

static void smart_island_page_slide_anim_finish_cb(lv_anim_t *a)
{
    LV_UNUSED(a);

    smart_island_update_pages_visible();
    smart_island_reset_page_positions();

    if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) {
        lv_obj_set_style_translate_x(g_si_ctx.objects.time, 0, 0);
    }

    if (g_si_ctx.view.page == SMART_ISLAND_PAGE_INFO) {
        smart_island_reset_compact_header_position();
        smart_island_reset_time_position();

        if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) {
            smart_island_update_idle_time();
            lv_obj_clear_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) {
            lv_obj_add_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
            smart_island_reset_time_position();
        }
    }

    g_si_ctx.action.ignore_click_once = false;
    g_si_ctx.action.ignore_action_click_once = false;
    g_si_ctx.view.page_slide_dir = 0;
    g_si_ctx.view.anim_running = false;
}

static void smart_island_raise_compact_header(void)
{
    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) lv_obj_move_foreground(g_si_ctx.objects.dot);
    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) lv_obj_move_foreground(g_si_ctx.objects.title);
    if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) lv_obj_move_foreground(g_si_ctx.objects.time);
}

static uint8_t smart_island_page_indicator_count_get(void)
{
    return (uint8_t)(g_si_ctx.action.page_count + 1U);
}

static uint8_t smart_island_page_indicator_active_get(void)
{
    if (g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION) {
        return (uint8_t)(g_si_ctx.action.page_index + 1U);
    }

    return 0U;
}

void smart_island_page_indicator_sync(bool anim_en)
{
    uint8_t count;
    uint8_t active;

    if (g_si_ctx.objects.page_indicator == NULL || !lv_obj_is_valid(g_si_ctx.objects.page_indicator)) {
        return;
    }

    count = smart_island_page_indicator_count_get();
    active = smart_island_page_indicator_active_get();

    lv_capsule_pagination_set_count(g_si_ctx.objects.page_indicator, count);
    if (anim_en) {
        lv_capsule_pagination_set_active_page(g_si_ctx.objects.page_indicator, active);
    } else {
        lv_capsule_pagination_set_active_page_now(g_si_ctx.objects.page_indicator, active);
    }
}

static void smart_island_open_info_page_by_left_swipe(void)
{
    g_si_ctx.view.page_slide_dir = 1;
    smart_island_expand_if_needed(true);
    smart_island_set_page(SMART_ISLAND_PAGE_INFO, true);
}

static void smart_island_open_action_last_page_by_right_swipe(void)
{
    if (g_si_ctx.action.page_count == 0U) {
        return;
    }

    smart_island_action_page_set_index((uint8_t)(g_si_ctx.action.page_count - 1U), false);
    g_si_ctx.view.page_slide_dir = -1;
    smart_island_expand_if_needed(true);
    smart_island_set_page(SMART_ISLAND_PAGE_ACTION, true);
}

static void smart_island_expand_if_needed(bool anim_en)
{
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        return;
    }

    if (g_si_ctx.view.visual != SMART_ISLAND_VISUAL_EXPANDED) {
        smart_island_set_visual(SMART_ISLAND_VISUAL_EXPANDED, anim_en);
    }
}

static void smart_island_swipe_cb(lv_event_t *e)
{
    lv_event_code_t code;
    lv_indev_t *indev;
    lv_point_t pt;

    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING) return;
    if (g_si_ctx.view.visual != SMART_ISLAND_VISUAL_EXPANDED) return;

    indev = lv_event_get_indev(e);
    if (indev == NULL) {
        indev = lv_indev_get_act();
        if (indev == NULL) return;
    }

    lv_indev_get_point(indev, &pt);
    code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        g_si_ctx.view.swipe.pressed = true;
        g_si_ctx.view.swipe.swiped = false;
        g_si_ctx.view.swipe.start_pt = pt;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_coord_t dx;
        lv_coord_t dy;

        if (!g_si_ctx.view.swipe.pressed || g_si_ctx.view.swipe.swiped) return;

        dx = pt.x - g_si_ctx.view.swipe.start_pt.x;
        dy = pt.y - g_si_ctx.view.swipe.start_pt.y;

        if (LV_ABS(dx) > 10 && LV_ABS(dx) > LV_ABS(dy)) {
            g_si_ctx.action.ignore_click_once = true;
            g_si_ctx.action.ignore_action_click_once = true;
            g_si_ctx.view.swipe.swiped = true;

            if (dx < 0) {
                if (g_si_ctx.view.page == SMART_ISLAND_PAGE_INFO) {
                    smart_island_open_action_page();
                } else if (g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION) {
                    if (g_si_ctx.action.page_index + 1U < g_si_ctx.action.page_count) {
                        smart_island_action_page_set_index(g_si_ctx.action.page_index + 1U, true);
                    } else {
                        smart_island_open_info_page_by_left_swipe();
                    }
                }
            } else {
                if (g_si_ctx.view.page == SMART_ISLAND_PAGE_ACTION) {
                    if (g_si_ctx.action.page_index > 0U) {
                        smart_island_action_page_set_index(g_si_ctx.action.page_index - 1U, true);
                    } else {
                        smart_island_open_info_page();
                    }
                } else if (g_si_ctx.view.page == SMART_ISLAND_PAGE_INFO) {
                    smart_island_open_action_last_page_by_right_swipe();
                }
            }
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_si_ctx.view.swipe.pressed = false;
        g_si_ctx.view.swipe.swiped = false;
    }
}

void smart_island_enable_gesture_on_obj(lv_obj_t *obj)
{
    if (obj == NULL || !lv_obj_is_valid(obj)) return;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(obj, smart_island_swipe_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(obj, smart_island_swipe_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(obj, smart_island_swipe_cb, LV_EVENT_RELEASED, NULL);
}

void smart_island_modal_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    smart_island_close();
}

void smart_island_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING) {
        if (g_si_ctx.action.ignore_click_once) {
            g_si_ctx.action.ignore_click_once = false;
            return;
        }
        if (fault_popup_show_pending_now() || smart_island_warning_fault_show()) {
            smart_island_warning_stop();
        }
        return;
    }

    if (g_si_ctx.view.anim_running) return;

    if (g_si_ctx.action.ignore_click_once) {
        g_si_ctx.action.ignore_click_once = false;
        return;
    }

    /* 点钞完成态只保留绿色收起岛，不允许点开展开页 */
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        return;
    }

    if (g_si_ctx.view.visual == SMART_ISLAND_VISUAL_EXPANDED) {
        smart_island_close();
    } else {
        smart_island_open_info_page();
    }
}

static void smart_island_action_btn_cb(lv_event_t *e)
{
    uint8_t page_index = 0;
    uint8_t action_id = 0;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_si_ctx.action.ignore_action_click_once) {
        g_si_ctx.action.ignore_action_click_once = false;
        return;
    }

    page_index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (page_index >= SMART_ISLAND_ACTION_PAGE_COUNT) return;

    action_id = g_si_ctx.action.ids[page_index];

    if (action_id == SMART_ISLAND_ACTION_FUNC4) {
        bool enabled = smart_island_pure_count_is_enabled();
        smart_island_set_pure_count_enabled(!enabled);
        ui_state_save_pure_count_state();
        smart_island_action_item_apply(page_index);

        if (!enabled) {
            smart_island_close();
            ui_manager_push_page(UI_PAGE_PURE);
        } else {
            if (ui_manager_get_current_page() == UI_PAGE_PURE) {
                ui_page_18_pure_request_exit();
            } else {
                ui_manager_switch(UI_PAGE_MAIN);
            }
        }
        return;
    }

    if (action_id == SMART_ISLAND_ACTION_QR) {
        smart_island_show_qr_popup();
        return;
    }

    if (action_id == SMART_ISLAND_ACTION_FUNC3) {
        bool enabled = fault_popup_get_auto_enabled();
        fault_popup_set_auto_enabled(!enabled);
        ui_state_save_popup_auto_state();
        smart_island_action_item_apply(page_index);
        return;
    }

    if (g_si_ctx.action.callback) {
        g_si_ctx.action.callback(action_id);
    }
}

static void smart_island_show_qr_error_toast(const char *text)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 320;
    toast_cfg.h = 101;
    toast_cfg.text = smart_island_text_or_default(text, UI_TEXT_WIDGET_QR_POPUP_NO_DATA);
    toast_cfg.show_loader = true;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = lv_color_hex(0xC0392B);
    toast_cfg.auto_hide_ms = 2000;

    lv_print_toast_show_with_config(&toast_cfg);
}

static void smart_island_show_qr_popup(void)
{
    char qr_text[3072];
    if (!ui_qr_data_is_ready()) {
        smart_island_show_qr_error_toast(ui_text_get(UI_TEXT_WIDGET_QR_POPUP_NO_DATA));
        return;
    }
    if (!ui_qr_data_build(qr_text, sizeof(qr_text))) {
        smart_island_show_qr_error_toast(ui_text_get(UI_TEXT_WIDGET_QR_POPUP_DATA_TOO_LARGE));
        return;
    }
    if (!lv_qr_popup_show(qr_text)) {
        smart_island_show_qr_error_toast(ui_text_get(UI_TEXT_WIDGET_QR_POPUP_DATA_TOO_LARGE));
    }
}

static void smart_island_page_slide_anim(smart_island_page_t old_page, smart_island_page_t new_page)
{
    lv_anim_t a;
    lv_obj_t *old_obj = NULL, *new_obj = NULL;
    lv_coord_t start_old_x = 0, end_old_x = 0;
    lv_coord_t start_new_x = 0, end_new_x = 0;
    lv_coord_t delta_x = 0;
    bool slide_left = false;

    if (g_si_ctx.objects.page_info == NULL || g_si_ctx.objects.page_action == NULL) return;

    old_obj = (old_page == SMART_ISLAND_PAGE_INFO) ? g_si_ctx.objects.page_info : g_si_ctx.objects.page_action;
    new_obj = (new_page == SMART_ISLAND_PAGE_INFO) ? g_si_ctx.objects.page_info : g_si_ctx.objects.page_action;

    if (old_obj == new_obj) return;

    g_si_ctx.view.anim_running = true;

    smart_island_raise_compact_header();

    lv_obj_clear_flag(old_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(new_obj, LV_OBJ_FLAG_HIDDEN);

    if (g_si_ctx.view.page_slide_dir > 0) {
        slide_left = true;
    } else if (g_si_ctx.view.page_slide_dir < 0) {
        slide_left = false;
    } else {
        slide_left = (new_page == SMART_ISLAND_PAGE_ACTION);
    }

    if (slide_left) {
        start_old_x = 0;
        end_old_x = -SMART_ISLAND_PAGE_SLIDE_OFFSET;
        start_new_x = SMART_ISLAND_PAGE_SLIDE_OFFSET;
        end_new_x = 0;
    } else {
        start_old_x = 0;
        end_old_x = SMART_ISLAND_PAGE_SLIDE_OFFSET;
        start_new_x = -SMART_ISLAND_PAGE_SLIDE_OFFSET;
        end_new_x = 0;
    }

    delta_x = end_old_x - start_old_x;

    lv_obj_set_x(old_obj, start_old_x);
    lv_obj_set_x(new_obj, start_new_x);

    lv_anim_init(&a);
    lv_anim_set_var(&a, old_obj);
    lv_anim_set_exec_cb(&a, smart_island_anim_x_cb);
    lv_anim_set_values(&a, start_old_x, end_old_x);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);

    lv_anim_init(&a);
    lv_anim_set_var(&a, new_obj);
    lv_anim_set_exec_cb(&a, smart_island_anim_x_cb);
    lv_anim_set_values(&a, start_new_x, end_new_x);
    lv_anim_set_time(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a, smart_island_page_slide_anim_finish_cb);
    lv_anim_start(&a);

    if (g_si_ctx.objects.title && lv_obj_is_valid(g_si_ctx.objects.title)) {
        lv_anim_del(g_si_ctx.objects.title, smart_island_anim_translate_x_cb);
        lv_obj_clear_flag(g_si_ctx.objects.title, LV_OBJ_FLAG_HIDDEN);
        if (new_page == SMART_ISLAND_PAGE_ACTION) {
            lv_obj_set_style_translate_x(g_si_ctx.objects.title, 0, 0);
            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.title);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, 0, delta_x);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_translate_x(g_si_ctx.objects.title, -delta_x, 0);
            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.title);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, -delta_x, 0);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        }
    }

    if (g_si_ctx.objects.dot && lv_obj_is_valid(g_si_ctx.objects.dot)) {
        lv_anim_del(g_si_ctx.objects.dot, smart_island_anim_translate_x_cb);
        lv_obj_clear_flag(g_si_ctx.objects.dot, LV_OBJ_FLAG_HIDDEN);
        if (new_page == SMART_ISLAND_PAGE_ACTION) {
            lv_obj_set_style_translate_x(g_si_ctx.objects.dot, 0, 0);
            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.dot);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, 0, delta_x);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_translate_x(g_si_ctx.objects.dot, -delta_x, 0);
            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.dot);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, -delta_x, 0);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        }
    }

    if (g_si_ctx.objects.time && lv_obj_is_valid(g_si_ctx.objects.time)) {
        lv_anim_del(g_si_ctx.objects.time, smart_island_anim_translate_x_cb);

        if (new_page == SMART_ISLAND_PAGE_ACTION) {
            lv_obj_clear_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_translate_x(g_si_ctx.objects.time, 0, 0);

            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.time);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, 0, delta_x);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        } else {
            smart_island_reset_time_position();
            smart_island_update_idle_time();
            lv_obj_clear_flag(g_si_ctx.objects.time, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_translate_x(g_si_ctx.objects.time, -delta_x, 0);

            lv_anim_init(&a);
            lv_anim_set_var(&a, g_si_ctx.objects.time);
            lv_anim_set_exec_cb(&a, smart_island_anim_translate_x_cb);
            lv_anim_set_values(&a, -delta_x, 0);
            lv_anim_set_time(&a, 180);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
            lv_anim_start(&a);
        }
    }
}

static void smart_island_page_apply_now(smart_island_page_t page)
{
    g_si_ctx.view.page = page;
    smart_island_update_pages_visible();
    smart_island_reset_page_positions();
}

static void smart_island_page_apply_anim(smart_island_page_t page)
{
    smart_island_page_t old_page = g_si_ctx.view.page;
    g_si_ctx.view.page = page;
    smart_island_page_slide_anim(old_page, page);
}

static void smart_island_action_btn_style_apply(uint8_t index)
{
    lv_obj_t *btn = NULL;
    lv_obj_t *arrow = NULL;
    bool is_switch;
    bool enabled = false;

    if (index >= SMART_ISLAND_ACTION_PAGE_COUNT) {
        return;
    }

    btn = g_si_ctx.objects.action_buttons[index];
    arrow = g_si_ctx.objects.action_arrows[index];
    if (btn == NULL || !lv_obj_is_valid(btn)) {
        return;
    }

    is_switch = (g_si_ctx.action.ids[index] == SMART_ISLAND_ACTION_FUNC3 ||
        g_si_ctx.action.ids[index] == SMART_ISLAND_ACTION_FUNC4);

    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(SMART_ISLAND_BTN_BORDER), 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);

    if (!is_switch) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(SMART_ISLAND_BTN_BG_TOP), 0);
        lv_obj_set_style_bg_grad_color(btn, lv_color_hex(SMART_ISLAND_BTN_BG_BOT), 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);

        if (arrow && lv_obj_is_valid(arrow)) {
            lv_label_set_text(arrow, ">");
            lv_obj_set_style_text_color(arrow, lv_color_hex(SMART_ISLAND_BTN_ARROW), 0);
            lv_obj_set_style_text_font(arrow, &lv_font_instrument_sans_medium_18, 0);
            lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -16, 0);
        }
        return;
    }

    if (g_si_ctx.action.ids[index] == SMART_ISLAND_ACTION_FUNC4) {
        enabled = smart_island_pure_count_is_enabled();
    } else {
        enabled = fault_popup_get_auto_enabled();
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(enabled ? SMART_ISLAND_BTN_SWITCH_ON_TOP : SMART_ISLAND_BTN_SWITCH_OFF_TOP), 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(enabled ? SMART_ISLAND_BTN_SWITCH_ON_BOT : SMART_ISLAND_BTN_SWITCH_OFF_BOT), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);

    if (arrow && lv_obj_is_valid(arrow)) {
        lv_label_set_text(arrow, enabled ? "ON" : "OFF");
        lv_obj_set_style_text_color(arrow,
            lv_color_hex(enabled ? SMART_ISLAND_BTN_SWITCH_ON_TEXT : SMART_ISLAND_BTN_SWITCH_OFF_TEXT), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_instrument_sans_medium_12, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -12, 0);
    }
}

static void smart_island_action_item_apply(uint8_t index)
{
    lv_obj_t *btn = NULL, *label = NULL;
    lv_coord_t page_x = 0;

    if (index >= SMART_ISLAND_ACTION_PAGE_COUNT) return;

    btn = g_si_ctx.objects.action_buttons[index];
    label = g_si_ctx.objects.action_labels[index];

    if (btn == NULL || !lv_obj_is_valid(btn)) return;

    if (index >= g_si_ctx.action.page_count) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    page_x = (lv_coord_t)index * SMART_ISLAND_WIDTH;
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(btn, page_x + SMART_ISLAND_ACTION_BTN_X, SMART_ISLAND_ACTION_BTN_Y);

    if (label && lv_obj_is_valid(label)) {
        if (g_si_ctx.action.ids[index] == SMART_ISLAND_ACTION_FUNC3) {
            lv_label_set_text(label, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC3));
        } else if (g_si_ctx.action.ids[index] == SMART_ISLAND_ACTION_FUNC4) {
            lv_label_set_text(label, ui_text_get(UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC4));
        } else if (g_si_ctx.action.text_ids[index] < UI_TEXT_MAX) {
            lv_label_set_text(label, ui_text_get(g_si_ctx.action.text_ids[index]));
        } else if (g_si_ctx.action.texts[index][0] != '\0') {
            lv_label_set_text(label, g_si_ctx.action.texts[index]);
        } else {
            lv_label_set_text(label, "");
        }
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);
    }

    smart_island_action_btn_style_apply(index);
}

void smart_island_action_page_refresh_language_texts(void)
{
    for (uint8_t i = 0; i < g_si_ctx.action.page_count; i++) {
        if (g_si_ctx.action.text_ids[i] < UI_TEXT_MAX) {
            lv_snprintf(g_si_ctx.action.texts[i], sizeof(g_si_ctx.action.texts[i]), "%s",
                ui_text_get(g_si_ctx.action.text_ids[i]));
        }
        smart_island_action_item_apply(i);
    }
}

static void smart_island_action_page_slide_anim(uint8_t old_index, uint8_t new_index)
{
    lv_anim_t a;

    if (g_si_ctx.objects.action_track == NULL || !lv_obj_is_valid(g_si_ctx.objects.action_track)) return;

    if (old_index == new_index) {
        lv_obj_set_x(g_si_ctx.objects.action_track, -(lv_coord_t)new_index * SMART_ISLAND_WIDTH);
        return;
    }

    g_si_ctx.view.anim_running = true;
    lv_anim_del(g_si_ctx.objects.action_track, smart_island_anim_x_cb);

    lv_anim_init(&a);
    lv_anim_set_var(&a, g_si_ctx.objects.action_track);
    lv_anim_set_exec_cb(&a, smart_island_anim_x_cb);
    lv_anim_set_values(&a, -(lv_coord_t)old_index * SMART_ISLAND_WIDTH, -(lv_coord_t)new_index * SMART_ISLAND_WIDTH);
    lv_anim_set_time(&a, 160);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&a, smart_island_action_page_slide_anim_finish_cb);
    lv_anim_start(&a);
}

static void smart_island_action_page_set_index(uint8_t index, bool anim_en)
{
    uint8_t target_index = (index >= g_si_ctx.action.page_count && g_si_ctx.action.page_count > 0U)
        ? (uint8_t)(g_si_ctx.action.page_count - 1U)
        : index;
    uint8_t old_index = g_si_ctx.action.page_index;

    if (g_si_ctx.action.page_count == 0U) return;

    g_si_ctx.action.page_index = target_index;
    smart_island_page_indicator_sync(anim_en);

    if (g_si_ctx.objects.action_track == NULL || !lv_obj_is_valid(g_si_ctx.objects.action_track)) return;

    if (anim_en) {
        smart_island_action_page_slide_anim(old_index, target_index);
    } else {
        lv_anim_del(g_si_ctx.objects.action_track, smart_island_anim_x_cb);
        lv_obj_set_x(g_si_ctx.objects.action_track, -(lv_coord_t)target_index * SMART_ISLAND_WIDTH);
    }
}

bool smart_island_action_page_set_count(uint8_t count)
{
    uint8_t new_count = (count == 0U) ? 1U : (count > SMART_ISLAND_ACTION_PAGE_COUNT ? SMART_ISLAND_ACTION_PAGE_COUNT : count);

    g_si_ctx.action.page_count = new_count;

    if (g_si_ctx.action.page_index >= g_si_ctx.action.page_count) {
        g_si_ctx.action.page_index = (uint8_t)(g_si_ctx.action.page_count - 1U);
    }

    if (g_si_ctx.objects.action_track && lv_obj_is_valid(g_si_ctx.objects.action_track)) {
        lv_obj_set_size(g_si_ctx.objects.action_track,
                        (lv_coord_t)(SMART_ISLAND_WIDTH * g_si_ctx.action.page_count),
                        SMART_ISLAND_ACTION_EXPAND_H);
    }

    for (uint8_t i = 0; i < SMART_ISLAND_ACTION_PAGE_COUNT; i++) {
        smart_island_action_item_apply(i);
    }

    if (g_si_ctx.objects.action_track && lv_obj_is_valid(g_si_ctx.objects.action_track)) {
        lv_obj_set_x(g_si_ctx.objects.action_track,
                     -(lv_coord_t)g_si_ctx.action.page_index * SMART_ISLAND_WIDTH);
    }

    smart_island_page_indicator_sync(false);
    return true;
}

bool smart_island_action_page_set_lang_item(uint8_t index, uint8_t action_id, ui_text_id_t text_id)
{
    if (index >= SMART_ISLAND_ACTION_PAGE_COUNT ||
        (unsigned int)text_id >= (unsigned int)UI_TEXT_MAX) {
        return false;
    }
    g_si_ctx.action.ids[index] = action_id;
    g_si_ctx.action.text_ids[index] = text_id;
    g_si_ctx.action.texts[index][0] = '\0';
    if (index + 1U > g_si_ctx.action.page_count) smart_island_action_page_set_count(index + 1U);
    else smart_island_action_item_apply(index);
    return true;
}

bool smart_island_action_page_set_item(uint8_t index, uint8_t action_id, const char *text)
{
    if (index >= SMART_ISLAND_ACTION_PAGE_COUNT) return false;
    g_si_ctx.action.ids[index] = action_id;
    g_si_ctx.action.text_ids[index] = UI_TEXT_MAX;
    if (text && text[0] != '\0') lv_snprintf(g_si_ctx.action.texts[index], sizeof(g_si_ctx.action.texts[index]), "%s", text);
    else g_si_ctx.action.texts[index][0] = '\0';
    if (index + 1U > g_si_ctx.action.page_count) smart_island_action_page_set_count(index + 1U);
    else smart_island_action_item_apply(index);
    return true;
}

void smart_island_action_btn_create(void)
{
    static const uint8_t default_ids[SMART_ISLAND_ACTION_PAGE_COUNT] = {
        SMART_ISLAND_ACTION_FUNC4, SMART_ISLAND_ACTION_FUNC3,
        SMART_ISLAND_ACTION_TIME_SETTING, SMART_ISLAND_ACTION_QR
    };
    static const ui_text_id_t default_text_ids[SMART_ISLAND_ACTION_PAGE_COUNT] = {
        UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC4, UI_TEXT_WIDGET_SMART_ISLAND_ACTION_FUNC3,
        UI_TEXT_WIDGET_SMART_ISLAND_ACTION_TIME, UI_TEXT_WIDGET_SMART_ISLAND_ACTION_QR
    };
    lv_obj_t *btn = NULL, *label = NULL, *arrow = NULL;

    g_si_ctx.objects.action_track = lv_obj_create(g_si_ctx.objects.page_action);
    lv_obj_remove_style_all(g_si_ctx.objects.action_track);
    lv_obj_set_size(g_si_ctx.objects.action_track,
                    (lv_coord_t)(SMART_ISLAND_WIDTH * g_si_ctx.action.page_count),
                    SMART_ISLAND_ACTION_EXPAND_H);
    lv_obj_set_style_bg_opa(g_si_ctx.objects.action_track, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_si_ctx.objects.action_track, LV_OBJ_FLAG_SCROLLABLE);
    smart_island_enable_gesture_on_obj(g_si_ctx.objects.action_track);

    for (uint8_t i = 0; i < SMART_ISLAND_ACTION_PAGE_COUNT; i++) {
        btn = lv_obj_create(g_si_ctx.objects.action_track);
        lv_obj_remove_style_all(btn);
        lv_obj_set_size(btn, SMART_ISLAND_ACTION_BTN_W, SMART_ISLAND_ACTION_BTN_H);

        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_transform_zoom(btn, 256, 0);
        lv_obj_set_style_translate_y(btn, 0, 0);

        lv_obj_add_event_cb(btn, smart_island_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_add_event_cb(btn, smart_island_action_btn_touch_anim_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(btn, smart_island_action_btn_touch_anim_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(btn, smart_island_action_btn_touch_anim_cb, LV_EVENT_PRESS_LOST, NULL);
        smart_island_enable_gesture_on_obj(btn);

        label = lv_label_create(btn);
        lv_obj_set_style_text_color(label, lv_color_hex(SMART_ISLAND_BTN_TEXT), 0);
        lv_obj_set_style_text_font(label, &lv_font_instrument_sans_medium_14, 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 16, 0);

        arrow = lv_label_create(btn);
        lv_label_set_text(arrow, ">");
        lv_obj_set_style_text_color(arrow, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_instrument_sans_medium_18, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -16, 0);

        g_si_ctx.objects.action_buttons[i] = btn;
        g_si_ctx.objects.action_labels[i] = label;
        g_si_ctx.objects.action_arrows[i] = arrow;
        g_si_ctx.action.ids[i] = default_ids[i];
        g_si_ctx.action.text_ids[i] = default_text_ids[i];
        g_si_ctx.action.texts[i][0] = '\0';
    }

    smart_island_action_page_set_count(SMART_ISLAND_ACTION_PAGE_DEFAULT_COUNT);
    smart_island_action_page_refresh_language_texts();
    smart_island_action_page_set_index(0U, false);
}

void smart_island_set_page(smart_island_page_t page, bool anim_en)
{
    if ((unsigned int)page > (unsigned int)SMART_ISLAND_PAGE_ACTION) return;

    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
        smart_island_set_visual(SMART_ISLAND_VISUAL_COMPACT, false);
        return;
    }

    if (anim_en) smart_island_page_apply_anim(page);
    else smart_island_page_apply_now(page);
    smart_island_page_indicator_sync(anim_en);
}

smart_island_page_t smart_island_get_page(void) { return g_si_ctx.view.page; }

void smart_island_register_action_cb(smart_island_action_cb_t cb) { g_si_ctx.action.callback = cb; }

void smart_island_close(void)
{
    g_si_ctx.view.page = SMART_ISLAND_PAGE_INFO;
    smart_island_restore_idle();
    smart_island_action_page_set_index(0U, false);
    smart_island_page_indicator_sync(false);
    smart_island_reset_page_positions();
    smart_island_reset_compact_header_position();
    smart_island_reset_time_position();
    g_si_ctx.action.ignore_click_once = false;
    g_si_ctx.action.ignore_action_click_once = false;
}

void smart_island_open_info_page(void)
{
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        return;
    }

    smart_island_expand_if_needed(true);
    smart_island_set_page(SMART_ISLAND_PAGE_INFO, true);
}

void smart_island_open_action_page(void)
{
    if (g_si_ctx.view.scene == SMART_ISLAND_SCENE_RESULT) {
        return;
    }

    smart_island_action_page_set_index(0U, false);
    smart_island_expand_if_needed(true);
    smart_island_set_page(SMART_ISLAND_PAGE_ACTION, true);
}
