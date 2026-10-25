#ifndef UN260_WORKSPACE_ACCESS_H
#define UN260_WORKSPACE_ACCESS_H
#include "un260/workspace/workspace_model.h"
typedef enum { WORKSPACE_ACCESS_WORKSPACE=1 } workspace_access_action_t;
bool workspace_access_required(workspace_access_action_t);
/* One action, one actor, short lifetime. Credentials are never copied or logged. */
bool workspace_access_verify(workspace_access_action_t,const char *pin);
void workspace_access_cancel(void);
/* Remaining retry delay; zero permits another attempt. */
uint32_t workspace_access_retry_delay_ms(void);
bool workspace_access_consume(workspace_access_action_t);
/* NULL permits removal; existing counting History retains operator identities. */
const char *workspace_access_delete_blocker(uint32_t id);
bool workspace_access_delete(uint32_t id);
bool workspace_access_save(const workspace_model_t *,bool force_auth);
#endif
