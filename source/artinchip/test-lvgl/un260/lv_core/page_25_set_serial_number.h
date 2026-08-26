#ifndef PAGE_25_SET_SERIAL_NUMBER_H
#define PAGE_25_SET_SERIAL_NUMBER_H

#include "lvgl/lvgl.h"
#include <stdint.h>

void ui_page_25_set_serial_number_create(lv_obj_t* parent);
void ui_page_25_set_serial_number_destroy(void);
void ui_page_25_set_serial_number_on_boot_setting(uint8_t level);
void ui_page_25_set_serial_number_on_reply(uint8_t level, uint8_t res);

#endif
