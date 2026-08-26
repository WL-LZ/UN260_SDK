#ifndef LV_DEBUG_OVERLAY_H
#define LV_DEBUG_OVERLAY_H

#include <stdbool.h>

void lv_debug_overlay_init(void);
void lv_debug_overlay_set_enabled(bool enabled);
bool lv_debug_overlay_is_enabled(void);

#endif
