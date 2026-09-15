#ifndef UI_PAGE_BACKGROUND_H
#define UI_PAGE_BACKGROUND_H
#include "lvgl/lvgl.h"

#define UI_USER_BACKGROUND_SRC "L:/usr/local/share/lvgl_data/backgrounds/user.png"
#define UI_SETTINGS_BACKGROUND_SRC UI_USER_BACKGROUND_SRC

typedef enum { UI_BACKGROUND_USER, UI_BACKGROUND_SETTINGS } ui_background_t;

/* Static root decoration only: no objects, timers, I/O or private cache owner.
 * All pages share the existing bounded image decoder cache by source path. */
static inline void ui_page_background_apply(lv_obj_t *root, ui_background_t theme)
{
    if (!root) return;
    lv_obj_set_style_bg_color(root, lv_color_hex(0xF2F5F7), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_img_src(root, theme == UI_BACKGROUND_SETTINGS ?
        UI_SETTINGS_BACKGROUND_SRC : UI_USER_BACKGROUND_SRC, 0);
    lv_obj_set_style_bg_img_opa(root, LV_OPA_COVER, 0);
}
#endif
