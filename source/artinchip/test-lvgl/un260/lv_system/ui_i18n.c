#include "ui_i18n.h"
#include "ui_lang.h"
#include "i18n/generated/lv_i18n.h"
#include <stddef.h>
#include <string.h>

const char *ui_tr(const char *msgid)
{
    if (!msgid) return "";
    ui_lang_init();
    return lv_i18n_get_text(msgid);
}
const char *ui_tr_for(language_t language, const char *msgid)
{
    const char *tag = "en";
    for (size_t i = 0; i < ui_lang_count(); ++i) {
        const ui_locale_t *locale = ui_lang_at(i);
        if (locale->id == language) { tag = locale->tag; break; }
    }
    return lv_i18n_get_text_from_pack(lv_i18n_language_pack, tag, msgid);
}
const char *ui_trc(const char *context, const char *msgid)
{
    if (!msgid) return "";
    if (!context || !*context) return ui_tr(msgid);
    char key[384];
    size_t a = strlen(context), b = strlen(msgid);
    if (a + b + 2 > sizeof(key)) return ui_tr(msgid);
    memcpy(key, context, a); key[a] = '\004';
    memcpy(key + a + 1, msgid, b + 1);
    const char *translated = ui_tr(key);
    /* Never return stack storage for an unknown contextual key. */
    return translated == key ? ui_tr(msgid) : translated;
}
const char *ui_trn(const char *msgid, int32_t count)
{
    if (!msgid) return "";
    ui_lang_init();
    return lv_i18n_get_text_plural(msgid, count);
}
