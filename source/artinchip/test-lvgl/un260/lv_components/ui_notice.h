#ifndef UI_NOTICE_H
#define UI_NOTICE_H

#include "ui_notice_state.h"

/* UI-thread API. Strings are copied, so stack/localized strings are safe. */
void ui_notice_init(void);
void ui_notice_deinit(void);
bool ui_notice_show(const ui_notice_config_t *config);
/* Queue pressure/duplicate suppression only affect presentation, never the
 * operation. Callers must use their service result as the business authority. */
void ui_notice_post(ui_notice_kind_t kind, const char *key, const char *title, const char *detail);
/* Tap/dismiss acknowledges the card only; it never cancels the associated task. */
void ui_notice_dismiss(const char *key);
void ui_notice_clear(const char *key);

enum {
    UI_NOTICE_SUSPEND_FAULT = 1u << 0,
    UI_NOTICE_SUSPEND_STANDBY = 1u << 1,
    UI_NOTICE_SUSPEND_MODAL = 1u << 2,
    UI_NOTICE_SUSPEND_DIALOG = 1u << 3
};
/* Independent blockers cannot accidentally resume one another. */
void ui_notice_set_suspended(uint32_t reason, bool suspended);
/* Decision dialogs acquire once on show and release once on hide/delete.
 * The shared count allows nested independent dialog owners. Not for keyboards. */
void ui_notice_dialog_acquire(void);
void ui_notice_dialog_release(void);
bool ui_notice_is_visible(void);

#endif
