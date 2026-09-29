#include "ui_lang.h"
#include "i18n/generated/lv_i18n.h"
#include "un260/storage/ui_locale_store.h"
#include <string.h>
#include "i18n/generated/ui_locale_registry.inc"
static const ui_locale_t *current = &ui_locales[0];
static uint32_t generation;
static bool initialized;

void ui_lang_init(void)
{
    if (!initialized) {
        (void)lv_i18n_init(lv_i18n_language_pack);
        initialized = true;
    }
}
size_t ui_lang_count(void) { return sizeof(ui_locales) / sizeof(ui_locales[0]); }
const ui_locale_t *ui_lang_at(size_t index)
{ return index < ui_lang_count() ? &ui_locales[index] : NULL; }
const ui_locale_t *ui_lang_find(const char *tag)
{
    if (tag) for (size_t i = 0; i < ui_lang_count(); ++i)
        if (strcmp(tag, ui_locales[i].tag) == 0) return &ui_locales[i];
    return NULL;
}
bool ui_lang_is_available(const ui_locale_t *locale)
{
    return locale && locale->enabled && locale->translations_ready &&
           locale->font_ready && locale->shaping_ready;
}
static void select_locale(const ui_locale_t *locale)
{
    ui_lang_init();
    if (!locale || lv_i18n_set_locale(locale->tag) != 0) locale = &ui_locales[0];
    (void)lv_i18n_set_locale(locale->tag);
    if (current != locale) { current = locale; ++generation; }
}
language_t ui_lang_get(void) { return current->id; }
uint32_t ui_lang_generation(void) { return generation; }
const ui_locale_t *ui_lang_current(void) { return current; }
void ui_lang_set(language_t lang)
{
    for (size_t i = 0; i < ui_lang_count(); ++i)
        if (ui_locales[i].id == lang) { select_locale(&ui_locales[i]); return; }
    select_locale(&ui_locales[0]);
}
void ui_lang_restore(const char *tag)
{
    const ui_locale_t *locale = ui_lang_find(tag);
    select_locale(ui_lang_is_available(locale) ? locale : &ui_locales[0]);
}
bool ui_lang_save(language_t lang)
{
    for (size_t i = 0; i < ui_lang_count(); ++i) {
        if (ui_locales[i].id != lang) continue;
        if (!ui_lang_is_available(&ui_locales[i]) ||
            !ui_locale_store_write(ui_locales[i].tag)) return false;
        select_locale(&ui_locales[i]);
        return true;
    }
    return false;
}
