#ifndef UN260_PANEL_MAIN_H
#define UN260_PANEL_MAIN_H
#include "lvgl/lvgl.h"
#include <stdbool.h>
#include <stdint.h>
bool panel_main_create(lv_obj_t *parent);
void panel_main_destroy(void);
void panel_main_set_visible(bool visible);
void panel_main_refresh(uint32_t dirty_flags);
void panel_main_refresh_start_state(void);
lv_obj_t *panel_main_root(void);
lv_obj_t *panel_main_scroll_obj(void);
#endif
