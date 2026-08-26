#ifndef LV_LOADING_GRID_H
#define LV_LOADING_GRID_H

#include "lvgl/lvgl.h"

lv_obj_t *lv_loading_grid_create(lv_obj_t *parent);
lv_obj_t *lv_loading_grid_create_sized(lv_obj_t *parent, lv_coord_t size);
void lv_loading_grid_set_opa(lv_obj_t *grid, lv_opa_t opa);

#endif
