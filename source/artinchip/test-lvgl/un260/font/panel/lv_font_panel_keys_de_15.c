/*******************************************************************************
 * Size: 15 px
 * Bpp: 4
 * Opts: --no-compress --no-prefilter --bpp 4 --size 15 --font /tmp/un260-panel-key-fonts-z8bsos25/de.ttf -r 0xE4,0xF6,0xFC --format lvgl --lv-font-name lv_font_panel_keys_de_15 -o /home/pc/d213-100ask/d211/source/artinchip/test-lvgl/un260/font/panel/lv_font_panel_keys_de_15.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_FONT_PANEL_KEYS_DE_15
#define LV_FONT_PANEL_KEYS_DE_15 1
#endif

#if LV_FONT_PANEL_KEYS_DE_15

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+00E4 "ä" */
    0x0, 0x19, 0x52, 0x94, 0x0, 0x3, 0xf8, 0x4f,
    0x60, 0x0, 0x3, 0x10, 0x31, 0x0, 0x3, 0x56,
    0x53, 0x0, 0xa, 0xff, 0xff, 0xfc, 0x10, 0x99,
    0x65, 0x8f, 0xf9, 0x0, 0x2, 0x34, 0xcf, 0xd0,
    0x6e, 0xff, 0xff, 0xfe, 0x2f, 0xfc, 0x55, 0xcf,
    0xf4, 0xff, 0x50, 0xe, 0xff, 0x2f, 0xfd, 0x9d,
    0xff, 0xf0, 0x7f, 0xff, 0x9b, 0xff, 0x0, 0x2,
    0x10, 0x0, 0x0,

    /* U+00F6 "ö" */
    0x0, 0x19, 0x52, 0x94, 0x0, 0x0, 0x2f, 0x94,
    0xf7, 0x0, 0x0, 0x3, 0x10, 0x31, 0x0, 0x0,
    0x2, 0x56, 0x30, 0x0, 0x1, 0xbf, 0xff, 0xfe,
    0x30, 0xb, 0xff, 0xa8, 0xff, 0xe1, 0x2f, 0xfa,
    0x0, 0x5f, 0xf7, 0x5f, 0xf6, 0x0, 0x1f, 0xf9,
    0x5f, 0xf7, 0x0, 0x2f, 0xf9, 0x1f, 0xfc, 0x0,
    0x7f, 0xf6, 0x9, 0xff, 0xdb, 0xff, 0xd0, 0x0,
    0x8f, 0xff, 0xfb, 0x10, 0x0, 0x0, 0x23, 0x10,
    0x0,

    /* U+00FC "ü" */
    0x0, 0x97, 0x19, 0x50, 0x0, 0xf, 0xb1, 0xf9,
    0x0, 0x0, 0x32, 0x3, 0x10, 0x2, 0x32, 0x0,
    0x3, 0x31, 0xdf, 0xc0, 0x3, 0xff, 0x6d, 0xfc,
    0x0, 0x3f, 0xf6, 0xdf, 0xc0, 0x3, 0xff, 0x6d,
    0xfc, 0x0, 0x3f, 0xf6, 0xdf, 0xd0, 0x4, 0xff,
    0x6b, 0xff, 0x0, 0xaf, 0xf6, 0x8f, 0xfe, 0xdf,
    0xff, 0x61, 0xcf, 0xfd, 0x6f, 0xf6, 0x0, 0x12,
    0x0, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 162, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 59, .adv_w = 165, .box_w = 10, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 124, .adv_w = 171, .box_w = 9, .box_h = 13, .ofs_x = 1, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x12, 0x18
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 228, .range_length = 25, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 3, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t lv_font_panel_keys_de_15 = {
#else
lv_font_t lv_font_panel_keys_de_15 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if LV_FONT_PANEL_KEYS_DE_15*/

