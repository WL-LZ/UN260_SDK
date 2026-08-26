#ifndef PAGE_09_CIS_CALA_H
#define PAGE_09_CIS_CALA_H

#include "lvgl/lvgl.h"

void ui_page_cis_calib_create(lv_obj_t* parent);
void ui_page_cis_calib_destroy(void);
void cis_enter_btn_cb(lv_event_t* e);
void cis_calib_ui_refresh(void);

#endif
