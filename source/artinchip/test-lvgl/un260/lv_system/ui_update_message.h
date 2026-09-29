#ifndef UI_UPDATE_MESSAGE_H
#define UI_UPDATE_MESSAGE_H
#include "ui_lang.h"
#include <stddef.h>
/* UI/update boundary only: the status file and raw logs remain unchanged.
 * The caller freezes locale for an operation. Unknown diagnostics are literal. */
void ui_update_message_render(language_t locale, const char *raw, char *out, size_t capacity);
#endif
