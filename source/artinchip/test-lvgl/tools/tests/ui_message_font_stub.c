#include "un260/font/ui_message_font.h"
const lv_font_t *ui_message_font(const lv_font_t *font){return font;}

const lv_font_t *ui_message_font_for(const lv_font_t *font,language_t language){(void)language;return font;}
void ui_message_font_set_provider(ui_message_font_provider_t provider){(void)provider;}
