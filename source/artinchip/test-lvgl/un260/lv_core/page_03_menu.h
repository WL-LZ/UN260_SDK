#ifndef PAGE_03_MENU_H
#define PAGE_03_MENU_H
#include "lvgl/lvgl.h"
void ui_page_03_menu_create(lv_obj_t *parent);
void ui_page_03_menu_destroy(void);
bool ui_page_03_menu_resume(void);
void ui_page_03_menu_suspend(void);
bool page_03_menu_is_created(void);
bool page_03_menu_is_visible(void);
void ui_page_03_menu_refresh_data(uint32_t topics);
void page_03_menu_clear_batch_tip(void);
void page_03_menu_show_batch_saved_tip(void);
void page_03_menu_refresh_batch_number(void);
void page_03_menu_refresh_batch_mode(void);
void page_03_menu_open_batch(void);
#endif
