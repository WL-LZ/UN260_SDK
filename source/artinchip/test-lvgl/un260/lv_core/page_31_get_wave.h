#ifndef PAGE_31_GET_WAVE_H
#define PAGE_31_GET_WAVE_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void ui_page_31_get_wave_create(lv_obj_t* parent);
void ui_page_31_get_wave_destroy(void);
bool ui_page_31_get_wave_request(void);
void ui_page_31_get_wave_on_frame(const uint8_t* data, uint16_t len);
bool ui_page_31_get_wave_poll(uint32_t now_ms);

#endif
