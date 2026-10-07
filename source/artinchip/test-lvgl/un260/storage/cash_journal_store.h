#ifndef UN260_CASH_JOURNAL_STORE_H
#define UN260_CASH_JOURNAL_STORE_H
#include "un260/workspace/cash_tools.h"
typedef enum { CASH_STORE_LOADING, CASH_STORE_READY, CASH_STORE_SAVING, CASH_STORE_FAILED, CASH_STORE_CORRUPT } cash_store_status_t;
void cash_journal_store_init(void);
bool cash_journal_store_poll(void);
cash_store_status_t cash_journal_store_status(void);
const cash_journal_t *cash_journal_store_get(void);
cash_error_t cash_journal_store_submit(const cash_entry_t *entry,uint32_t correction);
bool cash_journal_store_retry(void);
unsigned cash_journal_store_revision(void);
#endif
