#ifndef PAGE_14_MAIN_UPGRADE_H
#define PAGE_14_MAIN_UPGRADE_H

#include "lvgl/lvgl.h"
#include <stdint.h>

void ui_page_14_main_upgrade_create(lv_obj_t* parent);
void ui_page_14_main_upgrade_destroy(void);
void ui_page_14_main_upgrade_on_reply(uint8_t cmd_g, uint8_t res);

#endif
