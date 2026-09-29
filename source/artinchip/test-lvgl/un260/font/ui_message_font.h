#ifndef UI_MESSAGE_FONT_H
#define UI_MESSAGE_FONT_H
#include "lvgl/lvgl.h"

/* Message/fault-only wrapper. Original fonts and their English metrics remain
 * unchanged; missing Chinese/Korean/punctuation glyphs use the Noto subset. */
const lv_font_t *ui_message_font(const lv_font_t *base);
#endif
