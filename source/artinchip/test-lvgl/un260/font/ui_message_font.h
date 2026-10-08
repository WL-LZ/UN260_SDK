#ifndef UI_MESSAGE_FONT_H
#define UI_MESSAGE_FONT_H
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_lang.h"

/* Register once on the UI thread before creating UI. The provider must return
 * stable descriptors and perform no I/O or allocation during font lookup. */
typedef const lv_font_t *(*ui_message_font_provider_t)(const lv_font_t *base,
    unsigned pixels, language_t language);
void ui_message_font_set_provider(ui_message_font_provider_t provider);
const lv_font_t *ui_message_font_for(const lv_font_t *base, language_t language);
/* Existing message roles: English metrics stay unchanged. CN/KR descriptors
 * include the fallback glyph extents in their baseline/line height. This SC
 * subset is not a full-page Japanese or Arabic font provider. */
const lv_font_t *ui_message_font(const lv_font_t *base);
/* Explicit size for legacy typefaces outside the named message role table. */
const lv_font_t *ui_message_font_sized(const lv_font_t *base, unsigned pixels);
#endif
