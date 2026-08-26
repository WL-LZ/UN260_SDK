#ifndef PAGE_23_SET_FLAP_H
#define PAGE_23_SET_FLAP_H

#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"
#include <stdint.h>

void ui_page_23_set_flap_create(lv_obj_t* parent);
void ui_page_23_set_flap_destroy(void);
void ui_page_23_set_flap_on_reply(const setting_value_result_t* result);

#endif
