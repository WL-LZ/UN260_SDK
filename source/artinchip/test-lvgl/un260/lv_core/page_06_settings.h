#ifndef PAGE_06_SETTINGS_H
#define PAGE_06_SETTINGS_H
#include "lvgl/lvgl.h"
#include "un260/data_collection/data_collection.h"
/* Stable legacy values; visible order is owned by settings_catalog. */
typedef enum {
 PAGE_06_SETTINGS_MENU_SYSTEM=0, PAGE_06_SETTINGS_MENU_MAINTENANCE,
 PAGE_06_SETTINGS_MENU_USER, PAGE_06_SETTINGS_MENU_VERSION,
 PAGE_06_SETTINGS_MENU_DATA_COLLECTION, PAGE_06_SETTINGS_MENU_COUNT
} page_06_settings_menu_t;
void ui_page_06_settings_create(lv_obj_t *parent);
void ui_page_06_settings_destroy(void);
bool ui_page_06_settings_resume(void);
void ui_page_06_settings_suspend(void);
void ui_page_06_settings_reset_navigation(void);
void ui_page_06_settings_refresh_data(uint32_t topics);
bool page_06_settings_switch_menu(page_06_settings_menu_t menu);
bool page_06_settings_back_sub_page(void);
bool page_06_settings_is_collection(void);
void page_06_data_collection_refresh(void);
void page_06_data_collection_on_reply(data_collection_reply_result_t result);
/* Add/remove/reorder stable-ID definitions in settings_catalog.c.
 * lv_settings_grid/item provide automatic layout; frame_create provides detail chrome.
 * Device callbacks and drafts remain owned by the destination page. */
#endif
