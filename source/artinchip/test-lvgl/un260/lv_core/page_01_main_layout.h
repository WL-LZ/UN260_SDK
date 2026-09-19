#ifndef PAGE_01_MAIN_LAYOUT_H
#define PAGE_01_MAIN_LAYOUT_H
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_main_layout.h"
#ifndef UI_MAIN_LAYOUT_EDIT_DEFAULT_ENABLED
#define UI_MAIN_LAYOUT_EDIT_DEFAULT_ENABLED 1
#endif
#define PAGE_01_MAIN_LAYOUT_ITEM_COUNT 11
/* Identity order: four left actions, three right actions, summary, detail,
 * left footer group, right footer group. */
void page_01_main_layout_attach(lv_obj_t *root, lv_obj_t *const items[PAGE_01_MAIN_LAYOUT_ITEM_COUNT],
                                void (*apply)(const ui_main_layout_t *));
/* UI-thread policy only; no visible switch or persistence. Disabling cancels
 * an active draft, drains its touch, and retains the last saved positions.
 * This policy survives page suspend/rebuild; the macro sets its boot default. */
void page_01_main_layout_set_enabled(bool enabled);
bool page_01_main_layout_is_enabled(void);
bool page_01_main_layout_is_editing(void);
/* Main owns arbitration between its quick drawer and layout editor. */
bool page_01_main_layout_pointer(lv_indev_t *, lv_event_code_t, const lv_point_t *, uint8_t);
void page_01_main_layout_suspend(void);
void page_01_main_layout_resume(void);
void page_01_main_layout_detach(void);
#endif
