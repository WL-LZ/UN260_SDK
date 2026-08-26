#ifndef PAGE_22_SET_DOUBLE_NOTE_H
#define PAGE_22_SET_DOUBLE_NOTE_H

#include "lvgl/lvgl.h"
#include "un260/app_service/setting_service.h"
#include <stdint.h>

void ui_page_22_set_double_note_create(lv_obj_t* parent);
void ui_page_22_set_double_note_destroy(void);
void ui_page_22_set_double_note_on_boot_setting(void);
void ui_page_22_set_double_note_on_reply(const setting_value_result_t* result);

#endif
