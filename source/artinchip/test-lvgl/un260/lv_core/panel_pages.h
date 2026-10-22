#ifndef UN260_PANEL_PAGES_H
#define UN260_PANEL_PAGES_H
#include "lvgl/lvgl.h"
#include "lv_page_manager.h"
/* Page-owned projections: keep the existing navigation IDs and service routes. */
bool panel_pages_create(ui_page_t page, lv_obj_t *parent);
bool panel_pages_created(ui_page_t page);
bool panel_pages_visible(ui_page_t page);
bool panel_pages_resume(ui_page_t page);
void panel_pages_suspend(ui_page_t page);
void panel_pages_destroy(ui_page_t page);
void panel_pages_refresh(ui_page_t page, bool reset);
void panel_pages_list_select(int currency, int tab);
bool panel_pages_settings_category(unsigned category);
/* Page-private bridge for destinations with no approved panel design. */
void page_03_menu_open_detail(unsigned category, unsigned subcategory);
void page_06_settings_open_detail(const char *id);
#endif
