#ifndef PAGE_19_HISTORY_H
#define PAGE_19_HISTORY_H

#include "lvgl/lvgl.h"

/* Open a stable record directly; Back returns to the caller. */
void ui_page_19_history_open_record(uint32_t record_no);
void ui_page_19_history_create(lv_obj_t *parent);
bool ui_page_19_history_resume(void);
void ui_page_19_history_suspend(void);
void ui_page_19_history_destroy(void);
void ui_page_19_history_refresh(void);

#endif
