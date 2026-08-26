#ifndef PAGE_15_IMAGE_UPGRADE_H
#define PAGE_15_IMAGE_UPGRADE_H

#include "lvgl/lvgl.h"
#include <stdint.h>

void ui_page_15_image_upgrade_create(lv_obj_t* parent);
void ui_page_15_image_upgrade_destroy(void);
void ui_page_15_image_upgrade_on_reply(uint8_t cmd_g, uint8_t res);

#endif
