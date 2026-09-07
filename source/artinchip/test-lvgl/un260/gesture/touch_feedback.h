#ifndef UN260_TOUCH_FEEDBACK_H
#define UN260_TOUCH_FEEDBACK_H
#include "lvgl/lvgl.h"
void touch_feedback_init(void);
void touch_feedback_sample(const lv_point_t *point, uint8_t count);
void touch_feedback_edge_hint(int side, int distance, int y);
bool touch_feedback_edge_hint_is_returning(void);
bool touch_feedback_enabled(void);
bool touch_feedback_set_enabled(bool enabled);
#endif
