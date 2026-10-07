#include "un260/app_service/cash_workspace_access.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_system/ui_history_data.h"
#include "un260/storage/cashbook_store.h"
#include "un260/storage/cash_journal_store.h"
static ui_history_store_t history;
static cash_journal_t journal;
static bool records_ready=true;
static cashbook_reference_t reference=CASHBOOK_REF_CLEAR;
bool counting_history_is_idle(void){return records_ready;}
bool ui_history_data_is_available(void){return records_ready;}
bool ui_history_data_can_accept(void){return records_ready;}
const ui_history_store_t *ui_history_data_get(void){return &history;}
bool cashbook_store_ready(void){return records_ready;}
bool cashbook_store_busy(void){return false;}
cashbook_reference_t cashbook_store_operator_reference(uint32_t id){(void)id;return reference;}
cash_store_status_t cash_journal_store_status(void){return CASH_STORE_READY;}
const cash_journal_t *cash_journal_store_get(void){return &journal;}
static workspace_model_t model;
static uint32_t now;
static bool busy;
const workspace_model_t *workspace_store_get(void){return &model;}
bool workspace_store_ready(void){return true;}
const char *user_cfg_password_get(void){return "2468";}
uint32_t app_clock_uptime_ms(void){return now;}
const char *workspace_service_switch_blocker(void){return busy?"busy":NULL;}
const char *cash_tools_write_blocker(void){return busy?"busy":NULL;}
bool workspace_store_save(const workspace_model_t *m){if(!m)return false;model=*m;return true;}
void cash_tools_entry_identity(cash_entry_t *e){e->operator_id=workspace_active(&model)->id;}
cash_error_t cash_journal_store_submit(const cash_entry_t *e,uint32_t correction){(void)e;(void)correction;return CASH_OK;}
int main(void)
{
    workspace_defaults(&model);model.operators_enabled=1;model.management_protected=1;
    assert(cash_access_required(CASH_ACCESS_ENTRY));
    for(unsigned i=0;i<5;++i)assert(!cash_access_verify(CASH_ACCESS_ENTRY,"0000"));
    assert(!cash_access_verify(CASH_ACCESS_ENTRY,"2468"));now=30001;
    assert(cash_access_verify(CASH_ACCESS_ENTRY,"2468"));assert(cash_access_consume(CASH_ACCESS_ENTRY));assert(!cash_access_consume(CASH_ACCESS_ENTRY));
    assert(cash_access_verify(CASH_ACCESS_ENTRY,"2468"));now+=30001;assert(!cash_access_consume(CASH_ACCESS_ENTRY));
    assert(cash_access_verify(CASH_ACCESS_ENTRY,"2468"));uint32_t id;assert(workspace_add_user(&model,"Second",&id));model.active_id=id;assert(!cash_access_consume(CASH_ACCESS_ENTRY));
    assert(cash_access_verify(CASH_ACCESS_ENTRY,"2468"));assert(!cash_access_consume(CASH_ACCESS_WORKSPACE));assert(!cash_access_consume(CASH_ACCESS_ENTRY));
    cash_entry_t e={.kind=CASH_OPENING};assert(cash_workspace_submit(&e,0)==CASH_INVALID);assert(cash_access_verify(CASH_ACCESS_ENTRY,"2468"));assert(cash_workspace_submit(&e,0)==CASH_OK&&e.approved&&e.operator_id==id);
    e.kind=CASH_RECEIPT;busy=true;assert(cash_workspace_submit(&e,0)==CASH_INVALID);busy=false;assert(cash_workspace_submit(&e,0)==CASH_OK&&!e.approved);
    assert(cash_access_verify(CASH_ACCESS_WORKSPACE,"2468"));cash_access_cancel();assert(!cash_workspace_save(&model,true));
    model.management_protected=0;model.active_id=1;
    assert(!cash_workspace_delete_blocker(id));
    records_ready=false;assert(cash_workspace_delete_blocker(id));records_ready=true;
    reference=CASHBOOK_REF_FAILED;assert(cash_workspace_delete_blocker(id));reference=CASHBOOK_REF_USED;assert(cash_workspace_delete_blocker(id));reference=CASHBOOK_REF_CLEAR;
    history.record_count=1;history.records[0].valid=true;history.records[0].operator_id=id;assert(cash_workspace_delete_blocker(id));history.record_count=0;
    journal.count=1;journal.entries[0].cash.operator_id=id;assert(cash_workspace_delete_blocker(id));assert(!cash_workspace_delete(id));journal.count=0;
    model.management_protected=1;assert(!cash_workspace_delete(id));assert(cash_access_verify(CASH_ACCESS_WORKSPACE,"2468"));
    journal.count=1;journal.entries[0].operator_id=id;assert(!cash_workspace_delete(id));journal.count=0;
    assert(cash_access_verify(CASH_ACCESS_WORKSPACE,"2468"));assert(cash_workspace_delete(id));assert(model.user_count==1&&model.next_id==3);assert(cash_workspace_delete_blocker(1));
    puts("PASS authorization: retry lock, expiry, actor/action scope, single use, cancellation and busy blockers");
}
