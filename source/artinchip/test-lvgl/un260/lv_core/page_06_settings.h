#ifndef PAGE_06_SETTINGS_H
#define PAGE_06_SETTINGS_H
#include <stdio.h>
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "un260/data_collection/data_collection.h"
#include "un260/lv_resources/lv_img_init.h" 

typedef enum {
    PAGE_06_SETTINGS_MENU_SYSTEM = 0,
    PAGE_06_SETTINGS_MENU_MAINTENANCE,
    PAGE_06_SETTINGS_MENU_USER,
    PAGE_06_SETTINGS_MENU_VERSION,
    PAGE_06_SETTINGS_MENU_DATA_COLLECTION,
    PAGE_06_SETTINGS_MENU_COUNT
} page_06_settings_menu_t;

typedef enum {
    PAGE_06_SETTINGS_SUB_NONE = 0,
    PAGE_06_SETTINGS_SUB_UPGRADE,
} page_06_settings_sub_page_t;

void ui_page_06_settings_create(lv_obj_t* parent);
void ui_page_06_settings_destroy(void);
bool ui_page_06_settings_resume(void);
void ui_page_06_settings_suspend(void);
/* End a Settings navigation session without discarding its cached widgets. */
void ui_page_06_settings_reset_navigation(void);
void ui_page_06_settings_refresh_data(uint32_t topics);

/* 设置页一级菜单索引，用于切换左侧大类或在指定大类下添加选项。 */
bool page_06_settings_switch_menu(page_06_settings_menu_t menu);
lv_obj_t* page_06_settings_get_menu_page(page_06_settings_menu_t menu);

/* 创建与设置页现有样式一致的设置项。col 为 0/1，row 从 0 开始。 */
lv_obj_t* page_06_settings_create_option(lv_obj_t* parent, int col, int row,
                                         const char* title, const char* value,
                                         bool accent, lv_event_cb_t cb,
                                         void* user_data);

/* 在指定一级菜单下创建设置项；设置页未创建时返回 NULL。 */
lv_obj_t* page_06_settings_create_menu_option(page_06_settings_menu_t menu,
                                              int col, int row,
                                              const char* title,
                                              const char* value,
                                              bool accent,
                                              lv_event_cb_t cb,
                                              void* user_data);

/* 创建点击后进入 ui_manager 页面栈的设置项，page 参数传 UI_PAGE_*。 */
lv_obj_t* page_06_settings_create_page_option(page_06_settings_menu_t menu,
                                              int col, int row,
                                              const char* title,
                                              int page,
                                              bool accent);

/* 创建设置页内部子页面；sub_page 用稳定 ID，不依赖显示文案。 */
lv_obj_t* page_06_settings_create_sub_page(page_06_settings_menu_t menu,
                                           page_06_settings_sub_page_t sub_page,
                                           const char* title);
lv_obj_t* page_06_settings_create_sub_page_link(lv_obj_t* parent,
                                                int col, int row,
                                                const char* title,
                                                lv_obj_t* target_page,
                                                bool accent);
bool page_06_settings_open_sub_page(lv_obj_t* sub_page);
bool page_06_settings_back_sub_page(void);

void page_06_settings_set_status(const char* text, lv_color_t color);
void page_06_data_collection_refresh(void);
void page_06_data_collection_on_reply(data_collection_reply_result_t result);
#endif // !PAGE_06_SETTINGS_H
