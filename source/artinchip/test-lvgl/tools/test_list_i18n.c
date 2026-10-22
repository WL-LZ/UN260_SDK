#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "un260/counting/counting_reject_reason.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_report_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "protocol_error_names.inc"

_Static_assert(sizeof(expected_names) / sizeof(expected_names[0]) == 0x32,
               "The protocol fixture must cover every defined reject code");

static void verify_names(void)
{
    for (unsigned code = 0; code <= 0xFF; ++code) {
        const char *expected = code < 0x32 ? expected_names[code] : "Unknown Error";
        assert(strcmp(counting_reject_reason_get((uint8_t)code), expected) == 0);
        assert(strcmp(ui_text_counting_reject_reason((uint8_t)code), expected) == 0);
        const counting_reject_guide_t *guide = counting_reject_guide_get((uint8_t)code);
        assert(guide != NULL && strcmp(guide->title, expected) == 0);
        assert(guide->meaning != NULL && guide->meaning[0] != '\0');
        assert(guide->causes != NULL && guide->causes[0] != '\0');
        assert(guide->action != NULL && guide->action[0] != '\0');
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(ui_lang_count() == 10);
    for (size_t index = 0; index < ui_lang_count(); ++index) {
        const ui_locale_t *locale = ui_lang_at(index);
        assert(locale != NULL && ui_lang_is_available(locale));
        ui_lang_set(locale->id);
        assert(ui_lang_get() == locale->id);
        verify_names();
        assert(ui_text_get(UI_TEXT_LIST_NO_SERIAL_NUMBERS)[0] != '\0');
        if (locale->id == LANGUAGE_CN || locale->id == LANGUAGE_KR) {
            assert(strcmp(ui_tr("Search"), "Search") != 0);
            assert(strcmp(ui_text_get(UI_TEXT_LIST_NO_SERIAL_NUMBERS), "No serial numbers") != 0);
        }

        /* Export uses its captured locale without changing the active UI locale. */
        ui_lang_set(locale->id == LANGUAGE_EN ? LANGUAGE_CN : LANGUAGE_EN);
        const language_t active_language = ui_lang_get();
        const uint32_t generation = ui_lang_generation();
        char path[1024];
        int length = snprintf(path, sizeof(path), "%s/report-%s.html", argv[1], locale->tag);
        assert(length > 0 && (size_t)length < sizeof(path));
        FILE *report = fopen(path, "wb");
        assert(report != NULL);
        ui_report_i18n_write(report, locale->id);
        assert(fclose(report) == 0);
        assert(ui_lang_get() == active_language && ui_lang_generation() == generation);
    }

    ui_lang_set(UI_LANGUAGE_INVALID);
    assert(ui_lang_get() == LANGUAGE_EN);
    verify_names();
    assert(strcmp(ui_text_get(UI_TEXT_LIST_DENOMINATIONS), "Denominations") == 0);
    puts("list i18n: 10 real locales, 50 protocol names, 206 unknown codes, guide titles and locale fallback passed");
    return 0;
}
