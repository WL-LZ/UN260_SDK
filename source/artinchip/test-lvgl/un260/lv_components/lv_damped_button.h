#ifndef LV_DAMPED_BUTTON_H
#define LV_DAMPED_BUTTON_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

typedef struct {
    uint32_t normal_color;
    uint32_t pressed_color;
    uint32_t disabled_color;
    uint32_t text_color;
    uint32_t disabled_text_color;
    lv_coord_t radius;
} lv_damped_button_style_t;

lv_obj_t *lv_damped_button_create(lv_obj_t *parent,
                                  const lv_damped_button_style_t *style,
                                  const char *text,
                                  const lv_font_t *font);
void lv_damped_button_set_text(lv_obj_t *button, const char *text);
void lv_damped_button_set_enabled(lv_obj_t *button, bool enabled);
bool lv_damped_button_is_enabled(lv_obj_t *button);
lv_obj_t *lv_damped_button_get_label(lv_obj_t *button);

#endif
