#ifndef UI_LANG_H
#define UI_LANG_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Stable compatibility IDs, never persist enum order. New locales are declared
 * in i18n/locales.json; all runtime selection is registry based. */
typedef uint16_t language_t;
#define LANGUAGE_EN ((language_t)0)
#define LANGUAGE_CN ((language_t)1)
#define LANGUAGE_KR ((language_t)2)
#define UI_LANGUAGE_INVALID ((language_t)UINT16_MAX)

typedef enum { UI_LANG_LTR, UI_LANG_RTL } ui_lang_direction_t;
typedef struct {
    language_t id;
    const char *tag, *code, *name;
    ui_lang_direction_t direction;
    bool enabled, translations_ready, font_ready, shaping_ready;
    const char *font_profile;
} ui_locale_t;

/* All selection and lookup is on the UI thread. init is idempotent and does no I/O. */
void ui_lang_init(void);
language_t ui_lang_get(void);
uint32_t ui_lang_generation(void);
const ui_locale_t *ui_lang_current(void);
size_t ui_lang_count(void);
const ui_locale_t *ui_lang_at(size_t index);
const ui_locale_t *ui_lang_find(const char *tag);
bool ui_lang_is_available(const ui_locale_t *locale);
/* Low-level non-persistent draft/test selection. Production settings use save;
 * background workers must only read a persisted tag, then apply on the UI thread. */
void ui_lang_set(language_t lang);
/* Restore only a released, resource-ready locale; unknown/removed tags use English. */
void ui_lang_restore(const char *tag);
/* Store the stable tag atomically, then select. Failure leaves language unchanged. */
bool ui_lang_save(language_t lang);
#endif
