#ifndef PAGE_40_SET_IMPELLER_H
#define PAGE_40_SET_IMPELLER_H
#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"
void ui_page_40_set_impeller_create(lv_obj_t *parent);
void ui_page_40_set_impeller_destroy(void);
void ui_page_40_set_impeller_on_update(const setting_impeller_result_t *result);
#endif
