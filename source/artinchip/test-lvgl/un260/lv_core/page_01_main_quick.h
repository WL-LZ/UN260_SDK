#ifndef PAGE_01_MAIN_QUICK_H
#define PAGE_01_MAIN_QUICK_H
#include "lvgl/lvgl.h"
/* Main-owned transient overlay. No counting data, page stack or protocol
 * parsing belongs to this view. Host services remain authoritative. */
void page_01_main_quick_attach(lv_obj_t *main);
void page_01_main_quick_detach(void);
void page_01_main_quick_suspend(void);
void page_01_main_quick_schedule_preload(void);
void page_01_main_quick_refresh_data(uint32_t topics);
bool page_01_main_quick_request_back(void);
bool page_01_main_quick_is_open(void);
bool page_01_main_quick_pointer(lv_indev_t *,lv_event_code_t,const lv_point_t *,uint8_t);
#endif
