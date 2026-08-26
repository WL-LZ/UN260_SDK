#ifndef PAGE_27_SET_CFD_LEVEL_H
#define PAGE_27_SET_CFD_LEVEL_H

#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>

void ui_page_27_set_cfd_level_create(lv_obj_t* parent);
void ui_page_27_set_cfd_level_destroy(void);
bool ui_page_27_set_cfd_level_query(void);
void ui_page_27_set_cfd_level_on_info(const uint8_t* data, uint16_t len);
void ui_page_27_set_cfd_level_on_request_failed(void);

#endif
