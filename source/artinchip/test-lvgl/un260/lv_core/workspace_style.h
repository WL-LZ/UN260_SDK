#ifndef UN260_WORKSPACE_STYLE_H
#define UN260_WORKSPACE_STYLE_H
/* Page-private visual contract: design_workspace_access_20261006, 1280 x 400. */
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#define CW_INK 0x20313B
#define CW_MUTED 0x627583
#define CW_BLUE 0x1462CC
#define CW_PANEL 0xF2F5F8
#define CW_SOFT 0xEAF2FF
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_52);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_37);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_15);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_17);
lv_obj_t *cw_box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius);
lv_obj_t *cw_label(lv_obj_t *p,int x,int y,int w,const char *s,const lv_font_t *font,uint32_t color);
lv_obj_t *cw_button(lv_obj_t *p,int x,int y,int w,int h,const char *s,uint32_t bg,uint32_t fg,lv_event_cb_t cb,void *data);
lv_obj_t *cw_root(lv_obj_t *parent);
#endif
