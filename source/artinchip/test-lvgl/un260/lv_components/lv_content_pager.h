#ifndef LV_CONTENT_PAGER_H
#define LV_CONTENT_PAGER_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

lv_obj_t *lv_content_pager_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t width, lv_coord_t height,
                                  uint8_t page_count);
lv_obj_t *lv_content_pager_get_page(lv_obj_t *pager, uint8_t index);
bool lv_content_pager_set_active(lv_obj_t *pager, uint8_t index, bool animated);
uint8_t lv_content_pager_get_active(lv_obj_t *pager);
uint8_t lv_content_pager_get_count(lv_obj_t *pager);

#endif
