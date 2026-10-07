#ifndef CASH_JOURNAL_EXPORT_H
#define CASH_JOURNAL_EXPORT_H
#include <stdbool.h>
#include <stdint.h>
/* Export a snapshot of selected stable entry IDs; no borrowed page state. */
bool cash_journal_export_start(const uint32_t *ids,unsigned count);
void cash_journal_export_poll(void);
#endif
