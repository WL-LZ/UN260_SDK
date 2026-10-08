#ifndef UN260_UI_LOCALE_FONT_BANK_H
#define UN260_UI_LOCALE_FONT_BANK_H
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif
#include "un260/lv_system/ui_lang.h"
/* UI-thread only. Preparation validates every asset before loading any font.
 * Failed loads release only new tentative fonts; published fonts live for
 * the process lifetime so cached/hidden labels never hold dangling pointers. */
bool ui_locale_font_prepare(language_t language);
/* Small previews/native-name lists prepare only the size they actually use.
 * This does not mark the complete locale ready for saving or switching. */
bool ui_locale_font_prepare_size(language_t language,unsigned pixels);
/* No allocation or file I/O. Exact pixel size, NULL until prepared. */
const lv_font_t *ui_locale_font_get(language_t language, unsigned pixels);
/* Stable composed descriptors, including fallback baseline/height. */
const lv_font_t *ui_locale_font_compose(const lv_font_t *base, unsigned pixels,
    language_t language);
#endif
