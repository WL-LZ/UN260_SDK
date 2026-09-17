#ifndef LV_NAV_BUTTON_H
#define LV_NAV_BUTTON_H
#include "lvgl/lvgl.h"
/* Shared neutral Back visual; callers keep their existing navigation callback. */
lv_obj_t *lv_nav_button_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
    lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *user_data);
/* Shared 27px raster home icon. Parent-owned, non-interactive; the caller
 * places it inside its navigation button and keeps the existing action. */
lv_obj_t *lv_nav_home_icon_create(lv_obj_t *parent);
typedef enum {
    LV_NAV_BACK_NONE, LV_NAV_BACK_HANDLED, LV_NAV_BACK_BLOCKED
} lv_nav_back_result_t;
/* Reuse the visible page's ESC action, including its busy guards and cleanup. */
lv_nav_back_result_t lv_nav_button_request_back(void);
#endif
