#ifndef PAGE_26_SET_AGING_H
#define PAGE_26_SET_AGING_H

#include "lvgl/lvgl.h"
#include <stdint.h>

void ui_page_26_set_aging_create(lv_obj_t* parent);
void ui_page_26_set_aging_destroy(void);
void ui_page_26_set_aging_on_reply(uint8_t res);

#endif
