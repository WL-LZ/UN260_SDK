#ifndef PAGE_01_MAIN_DETAIL_H
#define PAGE_01_MAIN_DETAIL_H

#include "page_01_main.h"

/* Main-owned projection of the counting store. Parent deletion also releases
 * its row pools and idle-paused motion timers. No protocol or persistent data. */
bool page_01_main_detail_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t width, lv_coord_t height);
void page_01_main_detail_refresh(page_01_detail_section_t section);
void page_01_main_detail_set_visible(bool visible);
void page_01_main_detail_reset(void);
void page_01_main_detail_destroy(void);
void page_01_main_detail_set_position(lv_coord_t x, lv_coord_t y);
lv_obj_t *page_01_main_detail_scroll_obj(void);
/* Apply the same tap-versus-drag contract to Main's title/card blank area.
 * Do not bind ABC buttons. Non-clickable decorative children need no binding. */
void page_01_main_detail_bind_tap(lv_obj_t *object);

#endif
