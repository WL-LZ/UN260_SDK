#ifndef PAGE_07_CURR_VIEW_H
#define PAGE_07_CURR_VIEW_H

#include "lvgl/lvgl.h"

/* Currency-private image presentation shared by the card renderer, grid and
 * selected-currency summary. Does not own image objects or model state. */
void page07_curr_view_set_img_target_width(lv_obj_t *img, const char *code,
                                           int target_w);
void page07_curr_view_set_image_unselected_style(lv_obj_t *img);
void page07_curr_view_set_image_selected_style(lv_obj_t *img);

#endif
