#ifndef UN260_PAGE_DYNAMIC_CAPTURE_H
#define UN260_PAGE_DYNAMIC_CAPTURE_H
#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>
void ui_page_dynamic_capture_create(lv_obj_t *parent);
void ui_page_dynamic_capture_destroy(void);
/* Accepted counting events; ownership and freshness stay private to this page. */
void ui_page_dynamic_capture_on_count_start(uint32_t valid, uint16_t issue, bool cumulative);
void ui_page_dynamic_capture_on_count_info(uint16_t valid, uint8_t issue, uint8_t status);
void ui_page_dynamic_capture_on_frame(uint8_t command, const uint8_t *payload, uint16_t length);
bool ui_page_dynamic_capture_is_capturing(void);
bool ui_page_dynamic_capture_poll(uint32_t now_ms);
#endif
