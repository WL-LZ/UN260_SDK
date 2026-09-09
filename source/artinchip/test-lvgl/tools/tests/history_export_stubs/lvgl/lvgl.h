#ifndef HISTORY_EXPORT_TEST_LVGL_H
#define HISTORY_EXPORT_TEST_LVGL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef int16_t lv_coord_t;
typedef uint32_t lv_color_t;
typedef struct lv_font_t lv_font_t;
typedef struct lv_timer_t { void (*callback)(struct lv_timer_t *); } lv_timer_t;
#define lv_snprintf snprintf
static inline lv_color_t lv_color_hex(uint32_t value) { return value; }
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), uint32_t period, void *user_data);
void lv_timer_del(lv_timer_t *timer);
void lv_timer_set_repeat_count(lv_timer_t *timer, int32_t count);

#endif
