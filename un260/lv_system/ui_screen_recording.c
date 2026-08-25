#include "ui_screen_recording.h"

#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/font/manrope_fonts.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/recording/screen_recording_service.h"

#include "lvgl/lvgl.h"

#include <stdbool.h>

#define UI_SCREEN_RECORDING_BUTTON_X 1204
#define UI_SCREEN_RECORDING_BUTTON_Y 3
#define UI_SCREEN_RECORDING_BUTTON_SIZE 44

static lv_obj_t *g_record_button;
static lv_obj_t *g_record_mark;
static lv_obj_t *g_fps_overlay;
static lv_obj_t *g_fps_panel;
static screen_recording_state_t g_last_state = SCREEN_RECORDING_IDLE;

static void ui_screen_recording_set_button_visible(bool visible)
{
    bool is_visible;

    if (g_record_button == NULL || !lv_obj_is_valid(g_record_button)) {
        return;
    }

    is_visible = !lv_obj_has_flag(g_record_button, LV_OBJ_FLAG_HIDDEN);
    if (is_visible == visible) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(g_record_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_record_button, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_screen_recording_fps_dialog_close(void)
{
    if (g_fps_overlay != NULL && lv_obj_is_valid(g_fps_overlay)) {
        lv_obj_del(g_fps_overlay);
    }
    g_fps_overlay = NULL;
    g_fps_panel = NULL;
}

static void ui_screen_recording_show_toast(const char *text, bool alarm)
{
    lv_print_toast_config_t toast_cfg = lv_print_toast_get_default_config();

    toast_cfg.w = 390;
    toast_cfg.h = 101;
    toast_cfg.text = text;
    toast_cfg.show_loader = false;
    toast_cfg.align_center = true;
    toast_cfg.use_text_area = false;
    toast_cfg.loader_color = alarm ? lv_color_hex(0xC0392B)
                                   : lv_color_hex(0x18A66A);
    toast_cfg.auto_hide_ms = 1800;
    lv_print_toast_show_with_config(&toast_cfg);
}

static void ui_screen_recording_apply_state(screen_recording_state_t state)
{
    bool stop_shape = state != SCREEN_RECORDING_IDLE;

    if (g_record_mark == NULL || !lv_obj_is_valid(g_record_mark)) {
        return;
    }
    lv_obj_set_size(g_record_mark, stop_shape ? 12 : 11,
                    stop_shape ? 12 : 11);
    lv_obj_set_style_radius(g_record_mark, stop_shape ? 2 : LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_record_mark, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_bg_opa(g_record_mark, LV_OPA_COVER, 0);
    lv_obj_center(g_record_mark);
    if (g_record_button != NULL && lv_obj_is_valid(g_record_button)) {
        if (state == SCREEN_RECORDING_STOPPING) {
            lv_obj_clear_flag(g_record_button, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_add_flag(g_record_button, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    g_last_state = state;
}

static void ui_screen_recording_start(uint32_t fps)
{
    screen_recording_start_result_t result;
    result = screen_recording_service_start(fps);
    if (result == SCREEN_RECORDING_START_OK) {
        ui_screen_recording_apply_state(SCREEN_RECORDING_STARTING);
        ui_screen_recording_show_toast(
            ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_STARTED), false);
    } else if (result == SCREEN_RECORDING_START_USB_NOT_READY) {
        ui_screen_recording_show_toast(
            ui_text_get(UI_TEXT_WIDGET_SCREENSHOT_INSERT_USB), true);
    } else if (result != SCREEN_RECORDING_START_BUSY) {
        ui_screen_recording_show_toast(
            ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FAILED), true);
    }
}

static void ui_screen_recording_fps_select_cb(lv_event_t *event)
{
    uint32_t fps = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    ui_screen_recording_fps_dialog_close();
    ui_screen_recording_start(fps);
}

static void ui_screen_recording_fps_cancel_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    ui_screen_recording_fps_dialog_close();
}

static lv_obj_t *ui_screen_recording_dialog_button(lv_obj_t *parent,
                                                    lv_coord_t x,
                                                    lv_coord_t y,
                                                    lv_coord_t width,
                                                    uint32_t color,
                                                    uint32_t pressed_color,
                                                    const char *text,
                                                    lv_event_cb_t callback,
                                                    void *user_data)
{
    lv_damped_button_style_t style = {
        .normal_color = color,
        .pressed_color = pressed_color,
        .disabled_color = 0xCDD4DA,
        .text_color = 0xFFFFFF,
        .disabled_text_color = 0xFFFFFF,
        .radius = 18,
    };
    lv_obj_t *button = lv_damped_button_create(parent, &style, text,
                                                &lv_font_instrument_sans_bold_14);
    if (button == NULL) return NULL;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, 48);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    return button;
}

static void ui_screen_recording_panel_y(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, (lv_coord_t)value);
}

static void ui_screen_recording_show_fps_dialog(void)
{
    lv_obj_t *label;
    lv_anim_t animation;

    ui_screen_recording_fps_dialog_close();
    g_fps_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_fps_overlay);
    lv_obj_set_size(g_fps_overlay, 1280, 400);
    lv_obj_set_style_bg_color(g_fps_overlay, lv_color_hex(0x15202A), 0);
    lv_obj_set_style_bg_opa(g_fps_overlay, LV_OPA_50, 0);
    lv_obj_add_flag(g_fps_overlay, LV_OBJ_FLAG_CLICKABLE);

    g_fps_panel = lv_obj_create(g_fps_overlay);
    lv_obj_remove_style_all(g_fps_panel);
    lv_obj_set_pos(g_fps_panel, 310, 410);
    lv_obj_set_size(g_fps_panel, 660, 210);
    lv_obj_set_style_bg_color(g_fps_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_fps_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_fps_panel, 24, 0);
    lv_obj_set_style_shadow_color(g_fps_panel, lv_color_hex(0x23313B), 0);
    lv_obj_set_style_shadow_width(g_fps_panel, 24, 0);
    lv_obj_set_style_shadow_opa(g_fps_panel, LV_OPA_20, 0);
    lv_obj_clear_flag(g_fps_panel, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(g_fps_panel);
    lv_label_set_text(label, ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_TITLE));
    lv_obj_set_style_text_font(label, &lv_font_instrument_sans_bold_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x25323D), 0);
    lv_obj_set_width(label, 600);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 30, 24);

    ui_screen_recording_dialog_button(g_fps_panel, 30, 70, 180,
        0x8D99A3, 0x3578F6,
        ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_8),
        ui_screen_recording_fps_select_cb, (void *)(uintptr_t)8U);
    ui_screen_recording_dialog_button(g_fps_panel, 240, 70, 180,
        0x8D99A3, 0x3578F6,
        ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_24),
        ui_screen_recording_fps_select_cb, (void *)(uintptr_t)24U);
    ui_screen_recording_dialog_button(g_fps_panel, 450, 70, 180,
        0x8D99A3, 0x3578F6,
        ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FPS_60),
        ui_screen_recording_fps_select_cb, (void *)(uintptr_t)60U);
    ui_screen_recording_dialog_button(g_fps_panel, 240, 142, 180,
        0x8D99A3, 0x74818B,
        ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_CANCEL),
        ui_screen_recording_fps_cancel_cb, NULL);

    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_fps_panel);
    lv_anim_set_values(&animation, 410, 95);
    lv_anim_set_time(&animation, 190);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, ui_screen_recording_panel_y);
    lv_anim_start(&animation);
}

static void ui_screen_recording_click_cb(lv_event_t *event)
{
    screen_recording_state_t state;

    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    state = screen_recording_service_state();
    if (state == SCREEN_RECORDING_STARTING || state == SCREEN_RECORDING_ACTIVE) {
        screen_recording_service_request_stop();
        ui_screen_recording_apply_state(SCREEN_RECORDING_STOPPING);
        ui_screen_recording_show_toast(
            ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_STOPPING), false);
    } else if (state == SCREEN_RECORDING_IDLE) {
        ui_screen_recording_show_fps_dialog();
    }
}

static void ui_screen_recording_create(void)
{
    if (g_record_button != NULL && lv_obj_is_valid(g_record_button)) {
        return;
    }
    g_record_button = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_record_button);
    lv_obj_set_pos(g_record_button, UI_SCREEN_RECORDING_BUTTON_X,
                   UI_SCREEN_RECORDING_BUTTON_Y);
    lv_obj_set_size(g_record_button, UI_SCREEN_RECORDING_BUTTON_SIZE,
                    UI_SCREEN_RECORDING_BUTTON_SIZE);
    lv_obj_set_style_bg_color(g_record_button, lv_color_hex(0xE8E8E8),
                              LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(g_record_button, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_set_style_radius(g_record_button, 10, 0);
    lv_obj_clear_flag(g_record_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_record_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_record_button, ui_screen_recording_click_cb,
                        LV_EVENT_CLICKED, NULL);

    g_record_mark = lv_obj_create(g_record_button);
    lv_obj_remove_style_all(g_record_mark);
    lv_obj_clear_flag(g_record_mark, LV_OBJ_FLAG_CLICKABLE);
    ui_screen_recording_apply_state(screen_recording_service_state());
}

void ui_screen_recording_indicator_poll(void)
{
    screen_recording_completion_info_t completion;
    screen_recording_state_t state = screen_recording_service_state();
    bool enabled = user_cfg_screen_recording_enabled();

    if (screen_recording_service_poll_completion(&completion)) {
        if (completion.result == SCREEN_RECORDING_COMPLETION_SAVED) {
            ui_screen_recording_show_toast(
                ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_SAVED), false);
        } else {
            ui_screen_recording_show_toast(
                ui_text_get(UI_TEXT_WIDGET_SCREEN_RECORDING_FAILED), true);
        }
        state = screen_recording_service_state();
    }

    if (!enabled && state == SCREEN_RECORDING_IDLE) {
        ui_screen_recording_fps_dialog_close();
        ui_screen_recording_set_button_visible(false);
        return;
    }
    ui_screen_recording_create();
    ui_screen_recording_set_button_visible(true);
    if (state != g_last_state) {
        ui_screen_recording_apply_state(state);
    }
}
