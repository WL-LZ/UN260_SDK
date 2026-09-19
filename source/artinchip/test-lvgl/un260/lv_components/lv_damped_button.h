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

/*
 * Register an existing LVGL button (or any clickable object) with the shared
 * UN260 tactile interaction.  Visual roles keep their own normal color while
 * the shared engine derives the pressed shade as approximately 8% darker. The
 * pressed_color argument is kept for source compatibility and is normalized
 * by the implementation.
 */
void lv_damped_button_register(lv_obj_t *button,
                               lv_color_t normal_color,
                               lv_color_t pressed_color);
void lv_damped_button_set_palette(lv_obj_t *button,
                                  lv_color_t normal_color,
                                  lv_color_t pressed_color);
/* Opt-in design palette; preserves the shared timing and lifecycle while using
 * the supplied shade verbatim. Legacy callers retain derived feedback. */
void lv_damped_button_set_exact_palette(lv_obj_t *button,
                                        lv_color_t normal_color,
                                        lv_color_t pressed_color);
/* Reuse the button palette for manually pressed list rows, without animation. */
lv_color_t lv_damped_button_pressed_color(lv_color_t normal_color);

lv_obj_t *lv_damped_button_create(lv_obj_t *parent,
                                  const lv_damped_button_style_t *style,
                                  const char *text,
                                  const lv_font_t *font);
void lv_damped_button_set_text(lv_obj_t *button, const char *text);
void lv_damped_button_set_enabled(lv_obj_t *button, bool enabled);
bool lv_damped_button_is_enabled(lv_obj_t *button);
lv_obj_t *lv_damped_button_get_label(lv_obj_t *button);

#endif
