#ifndef LV_LOADING_ORBIT_H
#define LV_LOADING_ORBIT_H

#include "lvgl/lvgl.h"

lv_obj_t *lv_loading_orbit_create(lv_obj_t *parent);
lv_obj_t *lv_loading_orbit_create_sized(lv_obj_t *parent, lv_coord_t size);
void lv_loading_orbit_set_opa(lv_obj_t *orbit, lv_opa_t opa);
void lv_loading_orbit_set_indicator_color(lv_obj_t *orbit, lv_color_t color); //设置旋转环高亮颜色

#endif
