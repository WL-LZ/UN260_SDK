#ifndef PAGE_20_SET_PRINT_H
#define PAGE_20_SET_PRINT_H

#include "lvgl/lvgl.h"
#include "un260/print/print_config.h"
#include <stdint.h>

void ui_page_20_set_print_create(lv_obj_t* parent);
void ui_page_20_set_print_destroy(void);
void ui_page_20_set_print_on_boot_setting(const uint8_t* data, uint16_t len);
void ui_page_20_set_print_on_reply(const print_config_request_result_t* result);

#endif
