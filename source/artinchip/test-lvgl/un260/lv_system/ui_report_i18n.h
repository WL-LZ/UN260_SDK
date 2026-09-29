#ifndef UI_REPORT_I18N_H
#define UI_REPORT_I18N_H
#include "ui_lang.h"
#include <stdio.h>
/* Snapshot locale is chosen before an export. Never translates report data,
 * serial numbers, currency codes, or the stable CSV/schema representation. */
void ui_report_i18n_write(FILE *file, language_t locale);
#endif
