#include "un260/app_service/cash_tools_service.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/storage/cash_journal_store.h"
#include "un260/storage/workspace_store.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_system/machine_time.h"
#include "un260/storage/cashbook_store.h"
#include "un260/counting/counting_data_store.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
static ui_history_store_t history;
static cash_journal_t journal;
static workspace_model_t operators;
static bool confirmed,ready=true;
bool workspace_store_ready(void){return true;}
bool workspace_store_busy(void){return false;}
const workspace_model_t *workspace_store_get(void){return &operators;}
const cash_journal_t *cash_journal_store_get(void){return &journal;}
cash_store_status_t cash_journal_store_status(void){return CASH_STORE_READY;}
void currency_state_get_effective_code(char code[4]){strcpy(code,"USD");}
uint8_t currency_state_count(void){return 3;}
bool currency_state_get_code(uint8_t index,char code[4]){strcpy(code,index==0?"USD":index==1?"MUL":"CNY");return true;}
bool ui_history_data_is_available(void){return ready;}
storage_job_status_t ui_history_data_status(void){return ready?STORAGE_JOB_SUCCEEDED:STORAGE_JOB_PENDING;}
const ui_history_store_t *ui_history_data_get(void){return &history;}
bool machine_time_is_confirmed(void){return confirmed;}
bool machine_time_is_valid(const machine_time_value_t *t){return t->year==2026;}
void machine_time_get(machine_time_value_t *t){*t=(machine_time_value_t){2026,10,6,12,30,0};}
static uint32_t current_id=128;
static bool running,add;
static counting_sim_t live;
const cashbook_t *cashbook_store_get(void){return NULL;}
uint32_t counting_history_current_record(void){return current_id;}
const counting_sim_t *counting_data_current(void){return &live;}
bool counting_data_monetary_result_supported(const counting_sim_t *s){return s!=NULL;}
bool app_command_runtime_count_start_busy(void){return running;}
bool app_command_runtime_result_pending(void){return false;}
bool machine_state_aging_running(void){return false;}
bool machine_state_add_enabled(void){return add;}
int main(void){
 workspace_defaults(&operators);operators.operators_enabled=1;char codes[64][4];assert(cash_tools_currencies(codes,64)==2);assert(!strcmp(codes[1],"CNY"));
 ui_history_record_t *r=&history.records[7];*r=(ui_history_record_t){.valid=true,.record_no=128,.pcs=24,.amount=1680,.currency="USD",.year=2026,.month=10,.day=6,.hour=10};strcpy(r->denom_text,"100 x 12\n50 x 8\n20 x 4");
 cash_snapshot_t s;assert(cash_tools_history_count()==1);assert(cash_tools_history(0,&s));assert(s.record==128&&s.amount==1680*CASH_SCALE&&s.count==3&&s.recorded_at==20261006100000ULL);
 s.denoms[0].quantity=0;assert(r->amount==1680&&!strcmp(r->denom_text,"100 x 12\n50 x 8\n20 x 4"));
 r->amount=1681;assert(!cash_tools_history_count());r->amount=1680;strcpy(r->denom_text,"100 x 12\nBAD");assert(!cash_tools_history_count());
 r->multi.enabled=true;r->multi.count=2;r->multi.currencies[0]=(history_multi_currency_t){.code="USD",.amount=200,.pcs=2,.complete=true,.count=1,.denoms={{100,2}}};r->multi.currencies[1]=(history_multi_currency_t){.code="CNY",.amount=50,.pcs=1,.complete=true,.count=1,.denoms={{50,1}}};
 assert(cash_tools_history_count()==2);assert(cash_tools_current_count()==2);assert(cash_tools_current(1,&s)&&!strcmp(s.code,"CNY"));current_id=999;assert(!cash_tools_current_count());current_id=128;running=true;assert(!cash_tools_current_count());running=false;assert(cash_tools_history(1,&s)&&!strcmp(s.code,"CNY"));r->multi.overflow=true;assert(cash_tools_history_count()==0);r->multi.overflow=false;
 ready=false;assert(!cash_tools_history_count());ready=true;
 cash_entry_t e={0};cash_tools_entry_identity(&e);assert(!e.time);confirmed=true;cash_tools_entry_identity(&e);assert(e.time>1577836800&&e.operator_id==operators.active_id);
 puts("PASS supported currencies, stable count IDs, independent snapshots, mismatched/truncated data rejection, MULTI groups, pending history, confirmed clock, operator snapshot");
}
