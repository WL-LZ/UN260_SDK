#include "gesture_guide.h"

#include "un260/font/manrope_fonts.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_system/ui_text.h"

#define GUIDE_BLUE 0x3478F6

static lv_obj_t *g_overlay;
static lv_obj_t *g_panel;

static lv_obj_t *guide_label(lv_obj_t *parent, const char *text,
                             const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void guide_puck_y(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, (lv_coord_t)value);
}

static void guide_add_motion(lv_obj_t *page, const gesture_definition_t *definition)
{
    lv_obj_t *track;
    int i;

    track = lv_obj_create(page);
    lv_obj_remove_style_all(track);
    lv_obj_set_pos(track, 78, 18);
    lv_obj_set_size(track, 200, 132);
    lv_obj_set_style_bg_color(track, lv_color_hex(0xEDF3FA), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(track, 24, 0);
    for(i = 0; i < definition->finger_count; i++) {
        lv_anim_t anim;
        lv_obj_t *puck = lv_obj_create(track);
        lv_obj_remove_style_all(puck);
        lv_obj_set_size(puck, 24, 24);
        lv_obj_set_pos(puck, 72 + i * 30, 92);
        lv_obj_set_style_radius(puck, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(puck, lv_color_hex(GUIDE_BLUE), 0);
        lv_obj_set_style_bg_opa(puck, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_color(puck, lv_color_hex(GUIDE_BLUE), 0);
        lv_obj_set_style_shadow_width(puck, 8, 0);
        lv_obj_set_style_shadow_opa(puck, LV_OPA_20, 0);
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, puck);
        lv_anim_set_values(&anim, 92, 22);
        lv_anim_set_time(&anim, 760);
        lv_anim_set_playback_time(&anim, 180);
        lv_anim_set_repeat_delay(&anim, 360);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&anim, guide_puck_y);
        lv_anim_start(&anim);
    }
}

static void guide_panel_y(void *object, int32_t value)
{
    lv_obj_set_y((lv_obj_t *)object, (lv_coord_t)value);
}

static void guide_delete_ready(lv_anim_t *animation)
{
    LV_UNUSED(animation);
    if(g_overlay != NULL && lv_obj_is_valid(g_overlay)) lv_obj_del(g_overlay);
    g_overlay = NULL;
    g_panel = NULL;
}

static void guide_close_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    gesture_guide_close(true);
}

bool gesture_guide_is_open(void)
{
    return g_overlay != NULL && lv_obj_is_valid(g_overlay);
}

void gesture_guide_close(bool animated)
{
    lv_anim_t anim;
    if(!gesture_guide_is_open()) return;
    if(!animated || g_panel == NULL || !lv_obj_is_valid(g_panel)) {
        guide_delete_ready(NULL);
        return;
    }
    lv_anim_del(g_panel, guide_panel_y);
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, g_panel);
    lv_anim_set_values(&anim, lv_obj_get_y(g_panel), 410);
    lv_anim_set_time(&anim, 150);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&anim, guide_panel_y);
    lv_anim_set_ready_cb(&anim, guide_delete_ready);
    lv_anim_start(&anim);
}

void gesture_guide_show(void)
{
    lv_obj_t *pager;
    lv_obj_t *button;
    lv_damped_button_style_t button_style = {
        .radius = 18,
        .normal_color = GUIDE_BLUE,
        .pressed_color = 0x245BC4,
        .disabled_color = 0x96A1AA,
        .text_color = 0xFFFFFF,
        .disabled_text_color = 0xE1E6EA,
    };
    lv_anim_t anim;
    size_t i;

    gesture_guide_close(false);
    g_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(g_overlay);
    lv_obj_set_size(g_overlay, 1280, 400);
    lv_obj_set_style_bg_color(g_overlay, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(g_overlay, LV_OPA_50, 0);
    lv_obj_add_flag(g_overlay, LV_OBJ_FLAG_CLICKABLE);

    g_panel = lv_obj_create(g_overlay);
    lv_obj_remove_style_all(g_panel);
    lv_obj_set_pos(g_panel, 250, 410);
    lv_obj_set_size(g_panel, 780, 320);
    lv_obj_set_style_bg_color(g_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(g_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_panel, 26, 0);
    lv_obj_set_style_shadow_width(g_panel, 26, 0);
    lv_obj_set_style_shadow_opa(g_panel, LV_OPA_20, 0);

    lv_obj_t *title = guide_label(g_panel, ui_text_get(UI_TEXT_GESTURE_GUIDE_TITLE),
                                  &lv_font_instrument_sans_bold_20, 0x24313D);
    lv_obj_set_pos(title, 28, 18);
    lv_obj_set_width(title, 580);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, 0);
    pager = lv_content_pager_create(g_panel, 18, 56, 744, 202,
                                    (uint8_t)gesture_service_definition_count());
    for(i = 0; i < gesture_service_definition_count(); i++) {
        const gesture_definition_t *definition = gesture_service_definition(i);
        lv_obj_t *page = lv_content_pager_get_page(pager, (uint8_t)i);
        lv_obj_t *label;
        guide_add_motion(page, definition);
        label = guide_label(page, ui_text_get(definition->title_text),
                            &lv_font_instrument_sans_bold_18, 0x24313D);
        lv_obj_set_pos(label, 310, 28);
        lv_obj_set_width(label, 400);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
        label = guide_label(page, ui_text_get(definition->body_text),
                            &lv_font_instrument_sans_medium_14, 0x66737E);
        lv_obj_set_pos(label, 310, 70);
        lv_obj_set_size(label, 400, 70);
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    }
    button = lv_damped_button_create(g_panel, &button_style,
                                     ui_text_get(UI_TEXT_GESTURE_GOT_IT),
                                     &lv_font_instrument_sans_bold_14);
    lv_obj_set_pos(button, 622, 268);
    lv_obj_set_size(button, 130, 40);
    lv_obj_add_event_cb(button, guide_close_cb, LV_EVENT_CLICKED, NULL);

    lv_anim_init(&anim);
    lv_anim_set_var(&anim, g_panel);
    lv_anim_set_values(&anim, 410, 40);
    lv_anim_set_time(&anim, 220);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&anim, guide_panel_y);
    lv_anim_start(&anim);
}
