#ifndef UN260_PANEL_KEY_FONTS_H
#define UN260_PANEL_KEY_FONTS_H
#include "lvgl/lvgl.h"
LV_FONT_DECLARE(lv_font_panel_keys_zh_Hans_14);
LV_FONT_DECLARE(lv_font_panel_keys_zh_Hans_15);
LV_FONT_DECLARE(lv_font_panel_keys_zh_Hans_17);
LV_FONT_DECLARE(lv_font_panel_keys_zh_Hans_18);
LV_FONT_DECLARE(lv_font_panel_keys_ko_14);
LV_FONT_DECLARE(lv_font_panel_keys_ko_15);
LV_FONT_DECLARE(lv_font_panel_keys_ko_17);
LV_FONT_DECLARE(lv_font_panel_keys_ko_18);
LV_FONT_DECLARE(lv_font_panel_keys_ja_14);
LV_FONT_DECLARE(lv_font_panel_keys_ja_15);
LV_FONT_DECLARE(lv_font_panel_keys_ja_17);
LV_FONT_DECLARE(lv_font_panel_keys_ja_18);
LV_FONT_DECLARE(lv_font_panel_keys_ar_14);
LV_FONT_DECLARE(lv_font_panel_keys_ar_15);
LV_FONT_DECLARE(lv_font_panel_keys_ar_17);
LV_FONT_DECLARE(lv_font_panel_keys_ar_18);
LV_FONT_DECLARE(lv_font_panel_keys_tr_14);
LV_FONT_DECLARE(lv_font_panel_keys_tr_15);
LV_FONT_DECLARE(lv_font_panel_keys_tr_17);
LV_FONT_DECLARE(lv_font_panel_keys_tr_18);
LV_FONT_DECLARE(lv_font_panel_keys_fr_14);
LV_FONT_DECLARE(lv_font_panel_keys_fr_15);
LV_FONT_DECLARE(lv_font_panel_keys_fr_17);
LV_FONT_DECLARE(lv_font_panel_keys_fr_18);
LV_FONT_DECLARE(lv_font_panel_keys_pl_14);
LV_FONT_DECLARE(lv_font_panel_keys_pl_15);
LV_FONT_DECLARE(lv_font_panel_keys_pl_17);
LV_FONT_DECLARE(lv_font_panel_keys_pl_18);
LV_FONT_DECLARE(lv_font_panel_keys_ru_14);
LV_FONT_DECLARE(lv_font_panel_keys_ru_15);
LV_FONT_DECLARE(lv_font_panel_keys_ru_17);
LV_FONT_DECLARE(lv_font_panel_keys_ru_18);
LV_FONT_DECLARE(lv_font_panel_keys_de_14);
LV_FONT_DECLARE(lv_font_panel_keys_de_15);
LV_FONT_DECLARE(lv_font_panel_keys_de_17);
LV_FONT_DECLARE(lv_font_panel_keys_de_18);
#define PANEL_KEY_FONT_COUNT 36
static inline const lv_font_t *panel_key_bold_font(unsigned language,unsigned px){
 static const struct {unsigned language,px;const lv_font_t *font;} fonts[]={{1,14,&lv_font_panel_keys_zh_Hans_14},{1,15,&lv_font_panel_keys_zh_Hans_15},{1,17,&lv_font_panel_keys_zh_Hans_17},{1,18,&lv_font_panel_keys_zh_Hans_18},{2,14,&lv_font_panel_keys_ko_14},{2,15,&lv_font_panel_keys_ko_15},{2,17,&lv_font_panel_keys_ko_17},{2,18,&lv_font_panel_keys_ko_18},{3,14,&lv_font_panel_keys_ja_14},{3,15,&lv_font_panel_keys_ja_15},{3,17,&lv_font_panel_keys_ja_17},{3,18,&lv_font_panel_keys_ja_18},{4,14,&lv_font_panel_keys_ar_14},{4,15,&lv_font_panel_keys_ar_15},{4,17,&lv_font_panel_keys_ar_17},{4,18,&lv_font_panel_keys_ar_18},{5,14,&lv_font_panel_keys_tr_14},{5,15,&lv_font_panel_keys_tr_15},{5,17,&lv_font_panel_keys_tr_17},{5,18,&lv_font_panel_keys_tr_18},{6,14,&lv_font_panel_keys_fr_14},{6,15,&lv_font_panel_keys_fr_15},{6,17,&lv_font_panel_keys_fr_17},{6,18,&lv_font_panel_keys_fr_18},{7,14,&lv_font_panel_keys_pl_14},{7,15,&lv_font_panel_keys_pl_15},{7,17,&lv_font_panel_keys_pl_17},{7,18,&lv_font_panel_keys_pl_18},{8,14,&lv_font_panel_keys_ru_14},{8,15,&lv_font_panel_keys_ru_15},{8,17,&lv_font_panel_keys_ru_17},{8,18,&lv_font_panel_keys_ru_18},{9,14,&lv_font_panel_keys_de_14},{9,15,&lv_font_panel_keys_de_15},{9,17,&lv_font_panel_keys_de_17},{9,18,&lv_font_panel_keys_de_18}};
 for(unsigned i=0;i<PANEL_KEY_FONT_COUNT;i++)if(fonts[i].language==language&&fonts[i].px==px)return fonts[i].font;
 return NULL;
}
#endif
