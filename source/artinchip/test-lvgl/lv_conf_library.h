#ifndef UN260_LV_CONF_LIBRARY_H
#define UN260_LV_CONF_LIBRARY_H

#include "lv_conf.h"

/* The application owns these Montserrat fonts. Keep the library's default
 * 14px font for standalone LVGL initialization, without a second bitmap copy
 * of every application size. All drawing and font ABI settings stay shared. */
#undef LV_FONT_MONTSERRAT_10
#define LV_FONT_MONTSERRAT_10 0
#undef LV_FONT_MONTSERRAT_12
#define LV_FONT_MONTSERRAT_12 0
#undef LV_FONT_MONTSERRAT_16
#define LV_FONT_MONTSERRAT_16 0
#undef LV_FONT_MONTSERRAT_18
#define LV_FONT_MONTSERRAT_18 0
#undef LV_FONT_MONTSERRAT_20
#define LV_FONT_MONTSERRAT_20 0
#undef LV_FONT_MONTSERRAT_22
#define LV_FONT_MONTSERRAT_22 0
#undef LV_FONT_MONTSERRAT_24
#define LV_FONT_MONTSERRAT_24 0
#undef LV_FONT_MONTSERRAT_26
#define LV_FONT_MONTSERRAT_26 0
#undef LV_FONT_MONTSERRAT_28
#define LV_FONT_MONTSERRAT_28 0
#undef LV_FONT_MONTSERRAT_30
#define LV_FONT_MONTSERRAT_30 0
#undef LV_FONT_MONTSERRAT_32
#define LV_FONT_MONTSERRAT_32 0
#undef LV_FONT_MONTSERRAT_40
#define LV_FONT_MONTSERRAT_40 0
#undef LV_FONT_MONTSERRAT_42
#define LV_FONT_MONTSERRAT_42 0
#undef LV_FONT_MONTSERRAT_44
#define LV_FONT_MONTSERRAT_44 0
#undef LV_FONT_MONTSERRAT_48
#define LV_FONT_MONTSERRAT_48 0

/* No firmware consumer uses the legacy full Siyuan font. Message CJK fonts
 * are owned by the application and keep their existing glyph coverage. */
#undef LV_FONT_SIYUANHEITI_NORMAL_24
#define LV_FONT_SIYUANHEITI_NORMAL_24 0

#endif
