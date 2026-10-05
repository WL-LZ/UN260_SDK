#ifndef PAGE_18_PURE_H
#define PAGE_18_PURE_H

#include "lvgl/lvgl.h"

void ui_page_18_pure_create(lv_obj_t* parent);
void ui_page_18_pure_destroy(void);
bool ui_page_18_pure_resume(void);
void ui_page_18_pure_suspend(void);
/* Switch between the counting themes; update preference only after navigation. */
bool ui_page_18_pure_toggle_theme(void);

#endif
