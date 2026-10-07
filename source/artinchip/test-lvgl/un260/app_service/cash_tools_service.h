#ifndef UN260_CASH_TOOLS_SERVICE_H
#define UN260_CASH_TOOLS_SERVICE_H
#include "un260/workspace/cash_tools.h"
unsigned cash_tools_currencies(char (*codes)[4],unsigned capacity);
/* Index addresses a currency group, never a reusable history slot. Returned
 * snapshot has no pointers and is validated against its own saved totals. */
bool cash_tools_history(unsigned index,cash_snapshot_t *out);
unsigned cash_tools_history_count(void);
bool cash_tools_history_ready(void);
void cash_tools_entry_identity(cash_entry_t *entry);
const char *cash_tools_current_blocker(void);
unsigned cash_tools_current_count(void);
bool cash_tools_current(unsigned index,cash_snapshot_t *out);
const char *cash_tools_write_blocker(void);
#endif
