#ifndef UN260_PAGE_RECEIPTS_H
#define UN260_PAGE_RECEIPTS_H
#include "lvgl/lvgl.h"
void ui_receipts_create(lv_obj_t *parent);
void ui_receipts_destroy(void);
void ui_receipts_suspend(void);
bool ui_receipts_resume(void);
#endif
