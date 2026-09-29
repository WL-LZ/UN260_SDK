#ifndef UI_I18N_H
#define UI_I18N_H
#include <stdint.h>
#include "ui_lang.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Marker for static catalogue literals; translate only at the display boundary. */
#define UI_N_(literal) literal
/* UI-thread lookups. Unknown keys fall back to the English source text.
 * Data such as serial numbers, filenames, currency/protocol codes is not a key. */
const char *ui_tr(const char *msgid);
/* Immutable catalogue lookup: suitable for an export's captured locale. */
const char *ui_tr_for(language_t language, const char *msgid);
const char *ui_trc(const char *context, const char *msgid);
const char *ui_trn(const char *msgid, int32_t count);
#ifdef __cplusplus
}
#endif
#endif
