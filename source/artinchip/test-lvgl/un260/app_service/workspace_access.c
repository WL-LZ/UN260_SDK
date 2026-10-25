#include "workspace_access.h"
#include "workspace_service.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/app_clock.h"
#include <string.h>
#include <stdlib.h>
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/counting/counting_history_service.h"
static struct {unsigned action,failures;uint32_t actor,deadline,blocked_until;} access;
static uint32_t actor(void){const workspace_user_t *u=workspace_active(workspace_store_get());return u?u->id:UINT32_MAX;}
bool workspace_access_required(workspace_access_action_t action)
{(void)action;return !workspace_store_ready()||workspace_store_get()->management_protected;}
void workspace_access_cancel(void){access.action=0;access.deadline=0;}
uint32_t workspace_access_retry_delay_ms(void)
{
    int32_t remaining=(int32_t)(access.blocked_until-app_clock_uptime_ms());
    return access.blocked_until&&remaining>0?(uint32_t)remaining:0;
}
bool workspace_access_verify(workspace_access_action_t action,const char *pin)
{
    workspace_access_cancel();uint32_t now=app_clock_uptime_ms();
    if(workspace_access_retry_delay_ms())return false;
    const char *expected=user_cfg_password_get();
    if(!expected||strlen(expected)!=4||!pin||strlen(pin)!=4||strcmp(expected,pin)){
        if(++access.failures>=5){access.failures=0;access.blocked_until=now+30000;}return false;
    }
    access.failures=0;access.blocked_until=0;access.action=action;access.actor=actor();access.deadline=now+30000;return true;
}
bool workspace_access_consume(workspace_access_action_t action)
{
    bool valid=access.action==(unsigned)action&&access.actor==actor()&&(int32_t)(access.deadline-app_clock_uptime_ms())>=0;
    workspace_access_cancel();return valid;
}
const char *workspace_access_delete_blocker(uint32_t id)
{
    const workspace_model_t *m=workspace_store_get();
    if(!workspace_store_ready()||!m||!id||!workspace_find((workspace_model_t *)m,id))return UI_N_("The workspace is not available yet.");
    if(m->user_count<=1)return UI_N_("Keep at least one operator.");
    if(m->operators_enabled&&m->active_id==id)return UI_N_("Switch operator before deleting the current person.");
    const char *reason=workspace_service_switch_blocker();if(reason)return reason;
    if(!counting_history_is_idle()||!ui_history_data_is_available()||!ui_history_data_can_accept())return UI_N_("Wait until records are ready, then try again.");
    const ui_history_store_t *history=ui_history_data_get();
    if(!history)return UI_N_("Wait until records are ready, then try again.");
    for(unsigned i=0;i<history->record_count;++i)if(history->records[i].valid&&history->records[i].operator_id==id)return UI_N_("Linked records prevent deletion.");
    return NULL;
}
bool workspace_access_delete(uint32_t id)
{
    if(workspace_access_delete_blocker(id))return false;
    workspace_model_t *next=malloc(sizeof(*next));if(!next)return false;
    *next=*workspace_store_get();bool ok=workspace_remove_user(next,id);
    if(ok)ok=workspace_access_save(next,false);
    free(next);return ok;
}
bool workspace_access_save(const workspace_model_t *model,bool force_auth)
{
    if(!model||workspace_service_switch_blocker())return false;
    const workspace_model_t *old=workspace_store_get();
    for(unsigned i=0;i<old->user_count;++i)if(!workspace_find((workspace_model_t *)model,old->users[i].id)&&workspace_access_delete_blocker(old->users[i].id))return false;
    if((force_auth||workspace_access_required(WORKSPACE_ACCESS_WORKSPACE))&&!workspace_access_consume(WORKSPACE_ACCESS_WORKSPACE))return false;
    return workspace_store_save(model);
}
