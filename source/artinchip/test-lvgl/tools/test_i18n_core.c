#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "i18n/generated/lv_i18n.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool store_ok;
static unsigned writes;
static char stored[32];
bool ui_locale_store_write(const char *tag)
{
    ++writes;
    if (!store_ok) return false;
    snprintf(stored, sizeof(stored), "%s", tag);
    return true;
}

int main(void)
{
    ui_lang_init();
    assert(ui_lang_get() == LANGUAGE_EN);
    assert(ui_lang_count() >= 1);
    assert(ui_lang_is_available(ui_lang_find("en")));
    assert(!ui_lang_is_available(ui_lang_find("zh-Hans")));
    assert(!ui_lang_is_available(ui_lang_find("ko")));
    assert(!ui_lang_at(ui_lang_count()));
    assert(!ui_lang_find(NULL));
    assert(!ui_lang_find("missing"));
    assert(!strcmp(ui_tr(NULL), ""));
    assert(!strcmp(ui_tr("Unregistered original text"), "Unregistered original text"));
    assert(!strcmp(ui_trc("missing context", "Unregistered original text"), "Unregistered original text"));
    assert(!strcmp(ui_text_get((ui_text_id_t)-1), ""));
    assert(!strcmp(ui_text_get((ui_text_id_t)UI_TEXT_MAX), ""));
    assert(!strcmp(ui_text_get((ui_text_id_t)INT_MAX), ""));

    size_t phrases = 0;
    for (size_t locale_index = 0; locale_index < ui_lang_count(); ++locale_index) {
        const ui_locale_t *locale = ui_lang_at(locale_index);
        ui_lang_set(locale->id);
        const lv_i18n_lang_t *pack = NULL;
        for (size_t i = 0; lv_i18n_language_pack[i]; ++i)
            if (!strcmp(lv_i18n_language_pack[i]->locale_name, locale->tag)) pack = lv_i18n_language_pack[i];
        assert(pack);
        for (uint16_t i = 0; i < pack->singular_count; ++i) {
            const lv_i18n_phrase_t *entry = &pack->singulars[i];
            if (i) assert(strcmp(pack->singulars[i - 1].msg_id, entry->msg_id) < 0);
            assert(!strcmp(ui_tr(entry->msg_id), entry->translation));
            assert(!strcmp(ui_tr_for(locale->id, entry->msg_id), entry->translation));
            ++phrases;
        }
        for (unsigned form = 0; form < _LV_I18N_PLURAL_TYPE_NUM; ++form)
            for (uint16_t i = 1; i < pack->plural_count[form]; ++i)
                assert(strcmp(pack->plurals[form][i - 1].msg_id, pack->plurals[form][i].msg_id) < 0);
    }
    ui_lang_set(LANGUAGE_CN);
    assert(strcmp(ui_text_get(UI_TEXT_NOTICE_NOW), "Now"));
    uint32_t before = ui_lang_generation();
    assert(!strcmp(ui_tr_for(LANGUAGE_EN, "UI_TEXT_NOTICE_NOW"), "Now"));
    assert(ui_lang_get() == LANGUAGE_CN && ui_lang_generation() == before);
    ui_lang_set(LANGUAGE_CN);
    assert(ui_lang_generation() == before);
    assert(!ui_lang_save(LANGUAGE_CN) && writes == 0);
    assert(!ui_lang_save(LANGUAGE_EN) && writes == 1);
    assert(ui_lang_get() == LANGUAGE_CN && ui_lang_generation() == before);
    store_ok = true;
    assert(ui_lang_save(LANGUAGE_EN) && !strcmp(stored, "en"));
    assert(ui_lang_get() == LANGUAGE_EN && ui_lang_generation() == before + 1);
    ui_lang_set(LANGUAGE_CN); ui_lang_restore("zh-Hans"); assert(ui_lang_get() == LANGUAGE_EN);
    ui_lang_set(LANGUAGE_CN); ui_lang_restore("deleted-locale"); assert(ui_lang_get() == LANGUAGE_EN);
    ui_lang_set((language_t)65535); assert(ui_lang_get() == LANGUAGE_EN);
    assert(!strcmp(ui_trn("%u notes", 1), "%u note"));
    assert(!strcmp(ui_trn("%u notes", 2), "%u notes"));
    assert(!strcmp(ui_trn("%u notes", INT32_MIN), "%u notes"));
    printf("I18n core PASS: %zu translated phrases, registry gates, fallback, plural, frozen locale, persistence transaction\n", phrases);
    return 0;
}
