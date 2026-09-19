#ifndef PAGE_17_MOTOR_TEST_H
#define PAGE_17_MOTOR_TEST_H

#include "lvgl/lvgl.h"

void ui_page_17_motor_test_create(lv_obj_t* parent);
void ui_page_17_motor_test_destroy(void);
void ui_page_17_motor_test_on_reply(uint8_t command, uint8_t result);

#endif
