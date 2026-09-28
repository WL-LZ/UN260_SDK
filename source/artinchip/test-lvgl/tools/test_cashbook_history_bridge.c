/* History fault tests use the real Cashbook adapter/model with an immediate
 * store boundary. Worker/durability faults are exercised by test_cashbook_store. */
#include "un260/storage/cashbook_store.h"
#include <assert.h>
#include <stdio.h>
static cashbook_t book;
void cashbook_store_init(void){if(!book.version)cashbook_defaults(&book);}
const cashbook_t *cashbook_store_get(void){return &book;}
bool cashbook_store_ready(void){return true;}
bool cashbook_store_busy(void){return false;}
bool cashbook_store_submit(const cashbook_command_t *c)
{char reason[160];bool ok=cashbook_apply(&book,c,reason,sizeof(reason));if(!ok)fprintf(stderr,"Cashbook rejected fixture: %s\n",reason);return ok;}
bool app_command_runtime_count_start_busy(void){return false;}
bool machine_state_add_enabled(void){return false;}
bool multi_pass_verification_is_active(void){return false;}
