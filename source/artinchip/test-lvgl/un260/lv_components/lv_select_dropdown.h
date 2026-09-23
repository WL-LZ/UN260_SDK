#ifndef LV_SELECT_DROPDOWN_H
#define LV_SELECT_DROPDOWN_H
#include "lvgl/lvgl.h"

/* Styled native single-choice field. Use the normal lv_dropdown API for data,
 * selection and lifecycle. Only the expanded list is a scroll viewport. */
lv_obj_t *lv_select_dropdown_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                    lv_coord_t width, lv_coord_t height);
#endif
