#include "cash_workspace_access.h"
#include "cash_tools_service.h"
#include "workspace_service.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/cash_journal_store.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/lv_system/app_clock.h"
#include <string.h>
#include <stdlib.h>
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/storage/cashbook_store.h"
#include "un260/counting/counting_history_service.h"
static struct {unsigned action,failures;uint32_t actor,deadline,blocked_until;} access;
static uint32_t actor(void){const workspace_user_t *u=workspace_active(workspace_store_get());return u?u->id:UINT32_MAX;}
bool cash_access_required(cash_access_action_t action)
{(void)action;return !workspace_store_ready()||workspace_store_get()->management_protected;}
void cash_access_cancel(void){access.action=0;access.deadline=0;}
bool cash_access_verify(cash_access_action_t action,const char *pin)
{
    cash_access_cancel();uint32_t now=app_clock_uptime_ms();
    if(access.blocked_until&&(int32_t)(now-access.blocked_until)<0)return false;
    const char *expected=user_cfg_password_get();
    if(!expected||strlen(expected)!=4||!pin||strlen(pin)!=4||strcmp(expected,pin)){
        if(++access.failures>=5){access.failures=0;access.blocked_until=now+30000;}return false;
    }
    access.failures=0;access.blocked_until=0;access.action=action;access.actor=actor();access.deadline=now+30000;return true;
}
bool cash_access_consume(cash_access_action_t action)
{
    bool valid=access.action==(unsigned)action&&access.actor==actor()&&(int32_t)(access.deadline-app_clock_uptime_ms())>=0;
    cash_access_cancel();return valid;
}
const char *cash_workspace_delete_blocker(uint32_t id)
{
    const workspace_model_t *m=workspace_store_get();
    if(!workspace_store_ready()||!m||!id||!workspace_find((workspace_model_t *)m,id))return UI_N_("The workspace is not available yet.");
    if(m->user_count<=1)return UI_N_("Keep at least one operator.");
    if(m->operators_enabled&&m->active_id==id)return UI_N_("Switch operator before deleting the current person.");
    const char *reason=workspace_service_switch_blocker();if(reason)return reason;
    if(!counting_history_is_idle()||!ui_history_data_is_available()||!ui_history_data_can_accept()||cash_journal_store_status()!=CASH_STORE_READY||!cashbook_store_ready()||cashbook_store_busy())return UI_N_("Wait until records are ready, then try again.");
    const ui_history_store_t *history=ui_history_data_get();
    if(!history)return UI_N_("Wait until records are ready, then try again.");
    for(unsigned i=0;i<history->record_count;++i)if(history->records[i].valid&&history->records[i].operator_id==id)return UI_N_("Linked records prevent deletion.");
    const cash_journal_t *journal=cash_journal_store_get();
    if(!journal)return UI_N_("Wait until records are ready, then try again.");
    for(unsigned i=0;i<journal->count;++i){const cash_entry_t *e=&journal->entries[i];if(e->operator_id==id||e->cash.operator_id==id||e->peer.operator_id==id)return UI_N_("Linked records prevent deletion.");}
    switch(cashbook_store_operator_reference(id)){
    case CASHBOOK_REF_CLEAR:return NULL;
    case CASHBOOK_REF_USED:return UI_N_("Linked records prevent deletion.");
    case CASHBOOK_REF_FAILED:return UI_N_("Records could not be verified. Retry before deleting.");
    default:return UI_N_("Checking operator records...");
    }
}
bool cash_workspace_delete(uint32_t id)
{
    if(cash_workspace_delete_blocker(id))return false;
    workspace_model_t *next=malloc(sizeof(*next));if(!next)return false;
    *next=*workspace_store_get();bool ok=workspace_remove_user(next,id);
    if(ok)ok=cash_workspace_save(next,false);
    free(next);return ok;
}
bool cash_workspace_save(const workspace_model_t *model,bool force_auth)
{
    if(!model||workspace_service_switch_blocker())return false;
    const workspace_model_t *old=workspace_store_get();
    for(unsigned i=0;i<old->user_count;++i)if(!workspace_find((workspace_model_t *)model,old->users[i].id)&&cash_workspace_delete_blocker(old->users[i].id))return false;
    if((force_auth||cash_access_required(CASH_ACCESS_WORKSPACE))&&!cash_access_consume(CASH_ACCESS_WORKSPACE))return false;
    return workspace_store_save(model);
}
cash_error_t cash_workspace_submit(cash_entry_t *entry,uint32_t correction)
{
    if(!entry||cash_tools_write_blocker())return CASH_INVALID;
    bool sensitive=correction||entry->kind==CASH_REVERSAL||entry->kind==CASH_OPENING;
    entry->approved=0;
    if(sensitive&&cash_access_required(CASH_ACCESS_ENTRY)){if(!cash_access_consume(CASH_ACCESS_ENTRY))return CASH_INVALID;entry->approved=1;}
    cash_tools_entry_identity(entry);
    return cash_journal_store_submit(entry,correction);
}
