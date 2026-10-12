#ifndef LV_CONTENT_PAGER_H
#define LV_CONTENT_PAGER_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

lv_obj_t *lv_content_pager_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t width, lv_coord_t height,
                                  uint8_t page_count);
lv_obj_t *lv_content_pager_get_page(lv_obj_t *pager, uint8_t index);
lv_obj_t *lv_content_pager_get_viewport(lv_obj_t *pager);
void lv_content_pager_set_indicator_position(lv_obj_t *pager,lv_coord_t x,lv_coord_t y);
bool lv_content_pager_set_active(lv_obj_t *pager, uint8_t index, bool animated);
uint8_t lv_content_pager_get_active(lv_obj_t *pager);
uint8_t lv_content_pager_get_count(lv_obj_t *pager);
/* Activate a non-empty prefix of the allocated pages. Settles motion and
 * clamps the current page; unused pages/dots cannot remain swipe targets. */
bool lv_content_pager_set_count(lv_obj_t *pager,uint8_t page_count);

/* Optional circular, externally driven paging. The default native pager is
 * unchanged. Enabling requires at least two pages; false means invalid input.
 * Circular active/dots commit only when settling completes or finish is used. */
bool lv_content_pager_set_circular(lv_obj_t *pager, bool enabled);
/* Begin preserves an interrupted transition's visible offset. Updates use the
 * total horizontal displacement since begin, in pixels (right is positive).
 * These three functions return whether the circular pager accepted the call. */
bool lv_content_pager_drag_begin(lv_obj_t *pager);
bool lv_content_pager_drag_update(lv_obj_t *pager, int32_t displacement);
bool lv_content_pager_drag_end(lv_obj_t *pager, bool cancelled);
/* True while circular dragging/settling or a partial offset is present. */
bool lv_content_pager_is_moving(lv_obj_t *pager);
/* Synchronously settle circular paging to the chosen target, or nearest page during drag.
 * Returns whether motion/geometry changed. Use before capturing a snapshot. */
bool lv_content_pager_finish(lv_obj_t *pager);

#endif
