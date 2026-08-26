#ifndef LV_PRINT_TOAST_H
#define LV_PRINT_TOAST_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

#define LV_PRINT_TOAST_DEFAULT_X            502
#define LV_PRINT_TOAST_DEFAULT_Y            31
#define LV_PRINT_TOAST_DEFAULT_W            277
#define LV_PRINT_TOAST_DEFAULT_H            101
#define LV_PRINT_TOAST_DEFAULT_TEXT         "Printing..."
#define LV_PRINT_TOAST_DEFAULT_SHOW_LOADER  true
#define LV_PRINT_TOAST_DEFAULT_ALIGN_CENTER true
#define LV_PRINT_TOAST_DEFAULT_USE_TEXT_AREA false
#define LV_PRINT_TOAST_DEFAULT_TEXT_X       0
#define LV_PRINT_TOAST_DEFAULT_TEXT_Y       0
#define LV_PRINT_TOAST_DEFAULT_TEXT_W       0
#define LV_PRINT_TOAST_DEFAULT_TEXT_H       0
#define LV_PRINT_TOAST_DEFAULT_TEXT_FONT    (&lv_font_instrument_sans_medium_24)
#define LV_PRINT_TOAST_DEFAULT_BG_COLOR     lv_color_hex(0xFFFFFF)
#define LV_PRINT_TOAST_DEFAULT_TEXT_COLOR   lv_color_hex(0x2F3542)
#define LV_PRINT_TOAST_DEFAULT_LOADER_COLOR lv_color_hex(0x2F7CF6)
#define LV_PRINT_TOAST_DEFAULT_AUTO_HIDE_MS 5000

typedef struct {
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    lv_coord_t text_x;
    lv_coord_t text_y;
    lv_coord_t text_w;
    lv_coord_t text_h;
    lv_color_t bg_color;
    lv_color_t text_color;
    lv_color_t loader_color;
    const char *text;
    const lv_font_t *text_font;
    uint32_t auto_hide_ms;
    bool show_loader;
    bool align_center;
    bool use_text_area;
} lv_print_toast_config_t;

#define LV_PRINT_TOAST_CONFIG(_x, _y, _w, _h, _text, _show_loader) \
    {                                                               \
        .x = (_x),                                                  \
        .y = (_y),                                                  \
        .w = (_w),                                                  \
        .h = (_h),                                                  \
        .text_x = LV_PRINT_TOAST_DEFAULT_TEXT_X,                    \
        .text_y = LV_PRINT_TOAST_DEFAULT_TEXT_Y,                    \
        .text_w = LV_PRINT_TOAST_DEFAULT_TEXT_W,                    \
        .text_h = LV_PRINT_TOAST_DEFAULT_TEXT_H,                    \
        .bg_color = LV_PRINT_TOAST_DEFAULT_BG_COLOR,                \
        .text_color = LV_PRINT_TOAST_DEFAULT_TEXT_COLOR,            \
        .loader_color = LV_PRINT_TOAST_DEFAULT_LOADER_COLOR,        \
        .text = (_text),                                            \
        .text_font = LV_PRINT_TOAST_DEFAULT_TEXT_FONT,              \
        .auto_hide_ms = LV_PRINT_TOAST_DEFAULT_AUTO_HIDE_MS,        \
        .show_loader = (_show_loader),                              \
        .align_center = false,                                      \
        .use_text_area = LV_PRINT_TOAST_DEFAULT_USE_TEXT_AREA,      \
    }

#define LV_PRINT_TOAST_CONFIG_EX(_x, _y, _w, _h, _text_x, _text_y, _text_w, _text_h,      \
                                 _bg_color, _text_color, _loader_color, _text, _font,      \
                                 _auto_hide_ms, _show_loader, _align_center, _use_text_area) \
    {                                                                                       \
        .x = (_x),                                                                          \
        .y = (_y),                                                                          \
        .w = (_w),                                                                          \
        .h = (_h),                                                                          \
        .text_x = (_text_x),                                                                \
        .text_y = (_text_y),                                                                \
        .text_w = (_text_w),                                                                \
        .text_h = (_text_h),                                                                \
        .bg_color = (_bg_color),                                                            \
        .text_color = (_text_color),                                                        \
        .loader_color = (_loader_color),                                                    \
        .text = (_text),                                                                    \
        .text_font = (_font),                                                               \
        .auto_hide_ms = (_auto_hide_ms),                                                    \
        .show_loader = (_show_loader),                                                      \
        .align_center = (_align_center),                                                    \
        .use_text_area = (_use_text_area),                                                  \
    }

void lv_print_toast_create(void);
void lv_print_toast_create_with_config(const lv_print_toast_config_t *config);
void lv_print_toast_show(const char *text);
void lv_print_toast_show_with_config(const lv_print_toast_config_t *config);
void lv_print_toast_set_text(const char *text);
void lv_print_toast_set_loader_color(lv_color_t color);
void lv_print_toast_hide(void);
lv_print_toast_config_t lv_print_toast_get_default_config(void); //获取默认打印提示框配置

#endif
