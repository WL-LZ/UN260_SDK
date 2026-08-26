#ifndef PAGE_24_SET_REJECT_POCKET_H
#define PAGE_24_SET_REJECT_POCKET_H

#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"
#include <stdint.h>

void ui_page_24_set_reject_pocket_create(lv_obj_t* parent);
void ui_page_24_set_reject_pocket_destroy(void);
void ui_page_24_set_reject_pocket_on_boot_setting(void);
void ui_page_24_set_reject_pocket_on_reply(const setting_value_result_t* result);

#endif
