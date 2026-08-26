#ifndef PAGE_30_SET_FACTORY_H
#define PAGE_30_SET_FACTORY_H

#include "lvgl/lvgl.h"
#include <stdint.h>

void ui_page_30_set_factory_create(lv_obj_t* parent);
void ui_page_30_set_factory_destroy(void);
void ui_page_30_set_factory_on_reply(uint8_t res);

#endif
