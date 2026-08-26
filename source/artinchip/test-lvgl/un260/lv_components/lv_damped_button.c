#include "lv_damped_button.h"

lv_obj_t *lv_damped_button_create(lv_obj_t *parent,
                                  const lv_damped_button_style_t *style,
                                  const char *text,
                                  const lv_font_t *font)
{
    lv_obj_t *button;
    lv_obj_t *label;

    if (parent == NULL || style == NULL) return NULL;
    button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->normal_color), 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->pressed_color), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(style->disabled_color), LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_STATE_DISABLED);
    lv_obj_set_style_opa(button, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_radius(button, style->radius, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(style->normal_color), 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
    lv_obj_set_style_border_opa(button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(button, LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(button);
    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(style->text_color), 0);
    lv_obj_set_style_text_color(label, lv_color_hex(style->disabled_text_color), LV_STATE_DISABLED);
    lv_obj_center(label);
    return button;
}

lv_obj_t *lv_damped_button_get_label(lv_obj_t *button)
{
    return (button != NULL && lv_obj_is_valid(button) && lv_obj_get_child_cnt(button) > 0)
        ? lv_obj_get_child(button, 0) : NULL;
}

void lv_damped_button_set_text(lv_obj_t *button, const char *text)
{
    lv_obj_t *label = lv_damped_button_get_label(button);
    if (label != NULL) lv_label_set_text(label, text != NULL ? text : "");
}

void lv_damped_button_set_enabled(lv_obj_t *button, bool enabled)
{
    lv_obj_t *label;
    if (button == NULL || !lv_obj_is_valid(button)) return;
    if (enabled) {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
    }
    label = lv_damped_button_get_label(button);
    if (label != NULL) {
        if (enabled) lv_obj_clear_state(label, LV_STATE_DISABLED);
        else lv_obj_add_state(label, LV_STATE_DISABLED);
    }
}

bool lv_damped_button_is_enabled(lv_obj_t *button)
{
    return button != NULL && lv_obj_is_valid(button) &&
           !lv_obj_has_state(button, LV_STATE_DISABLED);
}
