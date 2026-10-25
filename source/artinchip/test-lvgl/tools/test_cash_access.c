#include "un260/app_service/workspace_access.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_system/ui_history_data.h"
static ui_history_store_t history;
static bool records_ready=true;
bool counting_history_is_idle(void){return records_ready;}
bool ui_history_data_is_available(void){return records_ready;}
bool ui_history_data_can_accept(void){return records_ready;}
const ui_history_store_t *ui_history_data_get(void){return &history;}
static workspace_model_t model;
static uint32_t now;
static bool busy;
const workspace_model_t *workspace_store_get(void){return &model;}
bool workspace_store_ready(void){return true;}
const char *user_cfg_password_get(void){return "2468";}
uint32_t app_clock_uptime_ms(void){return now;}
const char *workspace_service_switch_blocker(void){return busy?"busy":NULL;}
const char *calculator_import_write_blocker(void){return busy?"busy":NULL;}
bool workspace_store_save(const workspace_model_t *m){if(!m)return false;model=*m;return true;}
int main(void)
{
    workspace_defaults(&model);model.operators_enabled=1;model.management_protected=1;
    assert(workspace_access_required(WORKSPACE_ACCESS_WORKSPACE));
    for(unsigned i=0;i<5;++i)assert(!workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"0000"));
    assert(workspace_access_retry_delay_ms()==30000);
    assert(!workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));now=10000;assert(workspace_access_retry_delay_ms()==20000);now=30001;assert(!workspace_access_retry_delay_ms());
    assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));assert(workspace_access_consume(WORKSPACE_ACCESS_WORKSPACE));assert(!workspace_access_consume(WORKSPACE_ACCESS_WORKSPACE));
    assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));now+=30001;assert(!workspace_access_consume(WORKSPACE_ACCESS_WORKSPACE));
    assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));uint32_t id;assert(workspace_add_user(&model,"Second",&id));model.active_id=id;assert(!workspace_access_consume(WORKSPACE_ACCESS_WORKSPACE));
    assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));workspace_access_cancel();assert(!workspace_access_save(&model,true));
    model.management_protected=0;model.active_id=1;
    assert(!workspace_access_delete_blocker(id));
    records_ready=false;assert(workspace_access_delete_blocker(id));records_ready=true;
    history.record_count=1;history.records[0].valid=true;history.records[0].operator_id=id;assert(workspace_access_delete_blocker(id));history.record_count=0;
    busy=true;assert(!workspace_access_delete(id));busy=false;
    model.management_protected=1;assert(!workspace_access_delete(id));assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));
    assert(workspace_access_verify(WORKSPACE_ACCESS_WORKSPACE,"2468"));assert(workspace_access_delete(id));assert(model.user_count==1&&model.next_id==3);assert(workspace_access_delete_blocker(1));
    puts("PASS authorization: retry lock, expiry, actor/action scope, single use, cancellation and busy blockers");
}
