#ifndef PAGE_01_MULTI_H
#define PAGE_01_MULTI_H
#include "lvgl/lvgl.h"
lv_obj_t *page_01_multi_create(lv_obj_t *parent);
void page_01_multi_destroy(void);
void page_01_multi_refresh(void);
void page_01_multi_visible(bool visible);
bool page_01_multi_back(void);
lv_obj_t *page_01_multi_scroll(void);
#endif
