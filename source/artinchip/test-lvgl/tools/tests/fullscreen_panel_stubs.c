/* Explicit fullscreen-only view fixture boundary. The production facade and
 * all old page trees remain real; these tests never construct a panel tree.
 * Panel rendering and switching have their own real-component harnesses. */
#include <assert.h>
#include "un260/lv_system/ui_display_mode.h"
#include "un260/lv_core/panel_main.h"
#include "un260/lv_core/panel_pages.h"
#include "un260/lv_core/panel_entry.h"

ui_display_mode_t ui_display_mode_get(void) { return UI_DISPLAY_MODE_FULLSCREEN; }
bool ui_display_mode_is_panel(void) { return false; }
bool ui_display_mode_set(ui_display_mode_t mode)
{ assert(mode == UI_DISPLAY_MODE_FULLSCREEN); return mode == UI_DISPLAY_MODE_FULLSCREEN; }
bool panel_main_create(lv_obj_t *parent)
{ (void)parent; assert(!"fullscreen fixture attempted panel creation"); return false; }
void panel_main_destroy(void) {}
void panel_main_set_visible(bool visible) { (void)visible; }
void panel_main_refresh(uint32_t dirty) { (void)dirty; }
void panel_main_refresh_start_state(void) {}
lv_obj_t *panel_main_root(void) { return NULL; }
lv_obj_t *panel_main_scroll_obj(void) { return NULL; }
bool panel_entry_is_open(void) { return false; }
bool panel_choice_is_open(void) { return false; }
bool panel_pages_create(ui_page_t page, lv_obj_t *parent)
{ (void)page; (void)parent; assert(!"fullscreen fixture attempted panel creation"); return false; }
bool panel_pages_created(ui_page_t page) { (void)page; return false; }
bool panel_pages_visible(ui_page_t page) { (void)page; return false; }
bool panel_pages_resume(ui_page_t page) { (void)page; return false; }
void panel_pages_suspend(ui_page_t page) { (void)page; }
void panel_pages_destroy(ui_page_t page) { (void)page; }
void panel_pages_refresh(ui_page_t page, bool reset) { (void)page; (void)reset; }
void panel_pages_list_select(int currency, int tab) { (void)currency; (void)tab; }
bool panel_pages_settings_category(unsigned category) { (void)category; return false; }
bool panel_pages_settings_back(void) { return false; }
void panel_pages_settings_collection(void) {}
bool panel_pages_settings_is_collection(void) { return false; }
