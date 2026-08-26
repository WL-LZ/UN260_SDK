#ifndef PAGE_28_GET_IMAGE_H
#define PAGE_28_GET_IMAGE_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void ui_page_28_get_image_create(lv_obj_t* parent);
void ui_page_28_get_image_destroy(void);
void ui_page_28_get_image_on_frame(const uint8_t* data, uint16_t len);
bool ui_page_28_get_image_poll(uint32_t now_ms);

#endif
