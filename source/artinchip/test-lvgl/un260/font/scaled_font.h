#ifndef UN260_SCALED_FONT_H
#define UN260_SCALED_FONT_H
#include "lvgl/lvgl.h"
/* Caller-owned scratch. No allocation, layers or display transparency needed. */
typedef struct {
    lv_font_t font;
    const lv_font_t *base;
    uint8_t *pixels;
    uint32_t capacity;
    unsigned percent;
} scaled_font_t;
bool scaled_font_init(scaled_font_t *font,const lv_font_t *base,unsigned percent,uint8_t *pixels,uint32_t capacity);
#endif
