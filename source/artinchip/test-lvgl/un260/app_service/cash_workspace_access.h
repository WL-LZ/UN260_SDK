#ifndef CASH_WORKSPACE_ACCESS_H
#define CASH_WORKSPACE_ACCESS_H
#include "un260/workspace/workspace_model.h"
#include "un260/workspace/cash_tools.h"
typedef enum { CASH_ACCESS_ENTRY=1,CASH_ACCESS_WORKSPACE } cash_access_action_t;
bool cash_access_required(cash_access_action_t);
/* One action, one actor, short lifetime. Credentials are never copied or logged. */
bool cash_access_verify(cash_access_action_t,const char *pin);
void cash_access_cancel(void);
bool cash_access_consume(cash_access_action_t);
/* NULL permits removal; caller starts the asynchronous archive check explicitly. */
const char *cash_workspace_delete_blocker(uint32_t id);
bool cash_workspace_delete(uint32_t id);
bool cash_workspace_save(const workspace_model_t *,bool force_auth);
cash_error_t cash_workspace_submit(cash_entry_t *,uint32_t correction);
#endif
