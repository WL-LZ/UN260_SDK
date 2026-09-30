#include "ui_message_font.h"
#include "main_fonts.h"
#include "manrope_fonts.h"
#include <stddef.h>

LV_FONT_DECLARE(lv_font_message_cjk_12);
LV_FONT_DECLARE(lv_font_message_cjk_14);
LV_FONT_DECLARE(lv_font_message_cjk_16);
LV_FONT_DECLARE(lv_font_message_cjk_18);
LV_FONT_DECLARE(lv_font_message_cjk_20);
LV_FONT_DECLARE(lv_font_message_cjk_22);
LV_FONT_DECLARE(lv_font_message_cjk_24);
LV_FONT_DECLARE(lv_font_message_cjk_28);

static const struct {
    const lv_font_t *base;
    const lv_font_t *fallback;
} faces[] = {
    { &lv_font_instrument_sans_medium_12, &lv_font_message_cjk_12 },
    { &lv_font_instrument_sans_semibold_12, &lv_font_message_cjk_12 },
    { &lv_font_instrument_sans_medium_14, &lv_font_message_cjk_14 },
    { &lv_font_instrument_sans_semibold_14, &lv_font_message_cjk_14 },
    { &lv_font_instrument_sans_medium_16, &lv_font_message_cjk_16 },
    { &lv_font_instrument_sans_medium_18, &lv_font_message_cjk_18 },
    { &lv_font_instrument_sans_medium_24, &lv_font_message_cjk_24 },
    { &lv_font_instrument_sans_semibold_20, &lv_font_message_cjk_20 },
    { &lv_font_instrument_sans_semibold_22, &lv_font_message_cjk_22 },
    { &lv_font_instrument_sans_semibold_28, &lv_font_message_cjk_28 }
};

const lv_font_t *ui_message_font(const lv_font_t *base)
{
    static lv_font_t wrapped[sizeof(faces) / sizeof(faces[0])];
    static bool initialized;
    if (!initialized) {
        for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
            wrapped[i] = *faces[i].base;
            wrapped[i].fallback = faces[i].fallback;
        }
        initialized = true;
    }
    for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i)
        if (base == faces[i].base) return &wrapped[i];
    return base;
}
