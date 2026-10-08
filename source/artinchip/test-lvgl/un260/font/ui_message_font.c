#include "ui_message_font.h"
#include "main_fonts.h"
#include "manrope_fonts.h"
#include "ui_font_fallback.h"
#include "un260/lv_system/ui_lang.h"
#include <stddef.h>

LV_FONT_DECLARE(lv_font_instrument_sans_semibold_25);

LV_FONT_DECLARE(lv_font_message_cjk_12);
LV_FONT_DECLARE(lv_font_message_cjk_14);
LV_FONT_DECLARE(lv_font_message_cjk_16);
LV_FONT_DECLARE(lv_font_message_cjk_18);
LV_FONT_DECLARE(lv_font_message_cjk_20);
LV_FONT_DECLARE(lv_font_message_cjk_22);
LV_FONT_DECLARE(lv_font_message_cjk_24);
LV_FONT_DECLARE(lv_font_message_cjk_28);

LV_FONT_DECLARE(lv_font_instrument_sans_medium_15);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_17);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_22);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_10);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_26);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_28);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_32);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_36);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_26);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_32);
LV_FONT_DECLARE(lv_font_instrument_sans_bold_14);
LV_FONT_DECLARE(lv_font_instrument_sans_bold_16);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_18);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_24);
LV_FONT_DECLARE(lv_font_instrument_sans_menumedium_14);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_19);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_17);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_19);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_21);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_29);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_37);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_50);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_55);

static const struct {
    const lv_font_t *base;
    const lv_font_t *fallback;
    unsigned pixels;
} faces[] = {
    { &lv_font_instrument_sans_semibold_37, NULL, 37 },
    { &lv_font_instrument_sans_semibold_50, NULL, 50 },
    { &lv_font_instrument_sans_semibold_55, NULL, 55 },
    { &lv_font_instrument_sans_menumedium_14, &lv_font_message_cjk_14, 14 },
    { &lv_font_instrument_sans_medium_19, &lv_font_message_cjk_20, 20 },
    { &lv_font_instrument_sans_semibold_17, &lv_font_message_cjk_18, 18 },
    { &lv_font_instrument_sans_semibold_19, &lv_font_message_cjk_20, 20 },
    { &lv_font_instrument_sans_semibold_21, &lv_font_message_cjk_22, 22 },
    { &lv_font_instrument_sans_semibold_29, &lv_font_message_cjk_28, 28 },
    { &lv_font_instrument_sans_medium_10, NULL, 10 },
    { &lv_font_instrument_sans_medium_26, NULL, 26 },
    { &lv_font_instrument_sans_medium_28, &lv_font_message_cjk_28, 28 },
    { &lv_font_instrument_sans_medium_32, NULL, 32 },
    { &lv_font_instrument_sans_medium_36, NULL, 36 },
    { &lv_font_instrument_sans_semibold_26, NULL, 26 },
    { &lv_font_instrument_sans_semibold_32, NULL, 32 },
    { &lv_font_instrument_sans_bold_14, &lv_font_message_cjk_14, 14 },
    { &lv_font_instrument_sans_bold_16, &lv_font_message_cjk_16, 16 },
    { &lv_font_instrument_sans_medium_15, &lv_font_message_cjk_16, 16 },
    { &lv_font_instrument_sans_medium_17, &lv_font_message_cjk_18, 18 },
    { &lv_font_instrument_sans_semibold_18, &lv_font_message_cjk_18, 18 },
    { &lv_font_instrument_sans_semibold_24, &lv_font_message_cjk_24, 24 },
    { &lv_font_instrument_sans_medium_12, &lv_font_message_cjk_12, 12 },
    { &lv_font_instrument_sans_semibold_12, &lv_font_message_cjk_12, 12 },
    { &lv_font_instrument_sans_medium_13, &lv_font_message_cjk_14, 14 },
    { &lv_font_instrument_sans_medium_14, &lv_font_message_cjk_14, 14 },
    { &lv_font_instrument_sans_semibold_14, &lv_font_message_cjk_14, 14 },
    { &lv_font_instrument_sans_semibold_16, &lv_font_message_cjk_16, 16 },
    { &lv_font_instrument_sans_medium_16, &lv_font_message_cjk_16, 16 },
    { &lv_font_instrument_sans_medium_18, &lv_font_message_cjk_18, 18 },
    { &lv_font_instrument_sans_medium_20, &lv_font_message_cjk_20, 20 },
    { &lv_font_instrument_sans_medium_22, &lv_font_message_cjk_22, 22 },
    { &lv_font_instrument_sans_medium_24, &lv_font_message_cjk_24, 24 },
    { &lv_font_instrument_sans_semibold_20, &lv_font_message_cjk_20, 20 },
    { &lv_font_instrument_sans_semibold_22, &lv_font_message_cjk_22, 22 },
    { &lv_font_instrument_sans_semibold_25, &lv_font_message_cjk_24, 24 },
    { &lv_font_instrument_sans_semibold_28, &lv_font_message_cjk_28, 28 }
};

static ui_message_font_provider_t locale_provider;
void ui_message_font_set_provider(ui_message_font_provider_t provider)
{ locale_provider=provider; }
const lv_font_t *ui_message_font_for(const lv_font_t *base, language_t language)
{
    static lv_font_t wrapped[sizeof(faces) / sizeof(faces[0])];
    static lv_font_t localized[sizeof(faces) / sizeof(faces[0])];
    static bool localized_ready[sizeof(faces) / sizeof(faces[0])];
    static bool initialized;
    if (!initialized) {
        for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
            wrapped[i] = *faces[i].base;
            wrapped[i].fallback = faces[i].fallback;
        }
        initialized = true;
    }
    for (size_t i = 0; i < sizeof(faces) / sizeof(faces[0]); ++i) {
        if (base != faces[i].base) continue;
        if (language == LANGUAGE_EN) return &wrapped[i];
        if (locale_provider) {
            const lv_font_t *font=locale_provider(base,faces[i].pixels,language);
            if (font) return font;
        }
        /* The legacy SC fallback cannot substitute Japanese or Arabic. */
        if ((language != LANGUAGE_CN && language != LANGUAGE_KR) || !faces[i].fallback) return base;
        if (!localized_ready[i]) {
            ui_font_compose_fallback(&localized[i], faces[i].base, faces[i].fallback);
            localized_ready[i] = true;
        }
        return &localized[i];
    }
    return base;
}

const lv_font_t *ui_message_font(const lv_font_t *base)
{ return ui_message_font_for(base,ui_lang_get()); }

const lv_font_t *ui_message_font_sized(const lv_font_t *base,unsigned pixels)
{
    if(ui_lang_get()!=LANGUAGE_EN && locale_provider) {
        const lv_font_t *resolved=locale_provider(base,pixels,ui_lang_get());
        if(resolved) return resolved;
    }
    return base;
}
