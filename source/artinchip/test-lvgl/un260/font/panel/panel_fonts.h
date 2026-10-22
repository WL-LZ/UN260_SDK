#ifndef UN260_PANEL_FONTS_H
#define UN260_PANEL_FONTS_H
#include "lvgl/lvgl.h"
#include <stdbool.h>
LV_FONT_DECLARE(lv_font_panel_medium_11);
LV_FONT_DECLARE(lv_font_panel_semibold_11);
LV_FONT_DECLARE(lv_font_panel_medium_12);
LV_FONT_DECLARE(lv_font_panel_semibold_12);
LV_FONT_DECLARE(lv_font_panel_medium_13);
LV_FONT_DECLARE(lv_font_panel_semibold_13);
LV_FONT_DECLARE(lv_font_panel_medium_14);
LV_FONT_DECLARE(lv_font_panel_semibold_14);
LV_FONT_DECLARE(lv_font_panel_medium_15);
LV_FONT_DECLARE(lv_font_panel_semibold_15);
LV_FONT_DECLARE(lv_font_panel_medium_16);
LV_FONT_DECLARE(lv_font_panel_semibold_16);
LV_FONT_DECLARE(lv_font_panel_medium_17);
LV_FONT_DECLARE(lv_font_panel_semibold_17);
LV_FONT_DECLARE(lv_font_panel_medium_18);
LV_FONT_DECLARE(lv_font_panel_semibold_18);
LV_FONT_DECLARE(lv_font_panel_medium_19);
LV_FONT_DECLARE(lv_font_panel_semibold_19);
LV_FONT_DECLARE(lv_font_panel_medium_20);
LV_FONT_DECLARE(lv_font_panel_semibold_20);
LV_FONT_DECLARE(lv_font_panel_medium_21);
LV_FONT_DECLARE(lv_font_panel_semibold_21);
LV_FONT_DECLARE(lv_font_panel_medium_22);
LV_FONT_DECLARE(lv_font_panel_semibold_22);
LV_FONT_DECLARE(lv_font_panel_medium_31);
LV_FONT_DECLARE(lv_font_panel_semibold_31);
LV_FONT_DECLARE(lv_font_panel_medium_35);
LV_FONT_DECLARE(lv_font_panel_semibold_35);
LV_FONT_DECLARE(lv_font_panel_medium_59);
LV_FONT_DECLARE(lv_font_panel_semibold_59);
LV_FONT_DECLARE(lv_font_panel_medium_62);
LV_FONT_DECLARE(lv_font_panel_semibold_62);
LV_FONT_DECLARE(lv_font_panel_medium_66);
LV_FONT_DECLARE(lv_font_panel_semibold_66);
LV_FONT_DECLARE(lv_font_panel_medium_76);
LV_FONT_DECLARE(lv_font_panel_semibold_76);
static inline const lv_font_t *panel_base_font(unsigned px,bool bold){
 static const struct {unsigned px;const lv_font_t *normal,*bold;} f[]={{11,&lv_font_panel_medium_11,&lv_font_panel_semibold_11},{12,&lv_font_panel_medium_12,&lv_font_panel_semibold_12},{13,&lv_font_panel_medium_13,&lv_font_panel_semibold_13},{14,&lv_font_panel_medium_14,&lv_font_panel_semibold_14},{15,&lv_font_panel_medium_15,&lv_font_panel_semibold_15},{16,&lv_font_panel_medium_16,&lv_font_panel_semibold_16},{17,&lv_font_panel_medium_17,&lv_font_panel_semibold_17},{18,&lv_font_panel_medium_18,&lv_font_panel_semibold_18},{19,&lv_font_panel_medium_19,&lv_font_panel_semibold_19},{20,&lv_font_panel_medium_20,&lv_font_panel_semibold_20},{21,&lv_font_panel_medium_21,&lv_font_panel_semibold_21},{22,&lv_font_panel_medium_22,&lv_font_panel_semibold_22},{31,&lv_font_panel_medium_31,&lv_font_panel_semibold_31},{35,&lv_font_panel_medium_35,&lv_font_panel_semibold_35},{59,&lv_font_panel_medium_59,&lv_font_panel_semibold_59},{62,&lv_font_panel_medium_62,&lv_font_panel_semibold_62},{66,&lv_font_panel_medium_66,&lv_font_panel_semibold_66},{76,&lv_font_panel_medium_76,&lv_font_panel_semibold_76}};
 for(unsigned i=0;i<sizeof(f)/sizeof(f[0]);i++)if(f[i].px==px)return bold?f[i].bold:f[i].normal;
 return bold?&lv_font_panel_semibold_16:&lv_font_panel_medium_16;
}
#endif
