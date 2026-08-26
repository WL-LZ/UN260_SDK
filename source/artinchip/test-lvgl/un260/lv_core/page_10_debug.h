#ifndef PAGE_10_DEBUG_H
#define PAGE_10_DEBUG_H

#include "lvgl/lvgl.h"
#include "un260/lv_resources/lv_img_init.h" 
void ui_page_10_debug_create(void);
void ui_page_10_debug_destroy(void);
bool debug_page_rx_log_is_active(void);
void debug_append_rx_log(const char *data);

#endif // PAGE_10_DEBUG_H
