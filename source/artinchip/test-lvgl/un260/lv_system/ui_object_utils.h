#ifndef UI_OBJECT_UTILS_H
#define UI_OBJECT_UTILS_H

#include "lvgl/lvgl.h"
#include "un260/lv_resources/lv_img_init.h"

lv_obj_t *find_obj_by_name(const char *name, ui_element_t *page_cfg_obj,
                           int len);
void update_label_by_name(ui_element_t *page_cfg_obj, int len,
                          const char *name, const char *fmt, ...);

#endif
