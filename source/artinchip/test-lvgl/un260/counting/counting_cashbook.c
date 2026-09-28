#include "counting_cashbook.h"
#include "counting_data_store.h"
#include "counting_history_service.h"
#include "un260/innovation/multi_pass_verification.h"
#include "un260/machine_state/machine_state.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/storage/cashbook_store.h"
#include <stdio.h>
#include <string.h>
static uint32_t armed_group,active_group;
bool counting_cashbook_arm_verify(uint32_t group)
{
    const cashbook_t *book=cashbook_store_get();const cashbook_group_t *g=cashbook_group(book,group);
    if(multi_pass_verification_is_active()||!cashbook_store_ready()||cashbook_store_busy()||!g||g->excluded||cashbook_closed(book,g->day)||
       app_command_runtime_count_start_busy()||!counting_history_is_idle()||machine_state_add_enabled()||counting_data_current()->multi_currency_result)return false;
    const cashbook_run_t *r=cashbook_run(book,g->selected);
    if(!r||r->result.currencies!=1)return false;
    armed_group=group;return true;
}
uint32_t counting_cashbook_verify_group(void){return armed_group?armed_group:active_group;}
void counting_cashbook_cancel_verify(void){armed_group=active_group=0;}
void counting_cashbook_on_start(void)
{
    /* Not persisted: restarting must never silently attach an unrelated count. */
    if(armed_group)active_group=armed_group;
    armed_group=0;
    if(machine_state_add_enabled()||counting_data_current()->multi_currency_result)active_group=0;
}
static bool currency_valid(const char *c)
{return c[0]>='A'&&c[0]<='Z'&&c[1]>='A'&&c[1]<='Z'&&c[2]>='A'&&c[2]<='Z'&&!c[3]&&strcmp(c,"MUL")&&strcmp(c,"AUT");}
void counting_cashbook_capture(cashbook_result_t *out,const ui_history_record_t *r,const counting_sim_t *sim)
{
    memset(out,0,sizeof(*out));out->source=r->record_no;out->pcs=r->pcs;out->recount_group=active_group;if(r->pcs)active_group=0;
    uint32_t day=r->year*10000U+r->month*100U+r->day;
    out->day=cashbook_day_valid(day)?day:0;
    out->hour=r->hour;out->minute=r->minute;out->second=r->second;
    out->operator_id=r->operator_id;snprintf(out->operator_name,sizeof(out->operator_name),"%s",r->operator_name);
    uint64_t pcs=0;
    if(r->multi.enabled){
        out->cumulative=true;out->complete=!r->multi.overflow;
        for(unsigned i=0;i<r->multi.count&&i<CASHBOOK_CURRENCIES;i++){
            const history_multi_currency_t *v=&r->multi.currencies[i];
            if(!v->pcs)continue;
            if(!currency_valid(v->code)){out->complete=false;continue;}
            cashbook_money_t *m=&out->money[out->currencies++];memcpy(m->code,v->code,4);m->pcs=v->pcs;m->amount=v->amount;pcs+=v->pcs;
        }
        out->complete=out->complete&&pcs==r->pcs;
    }else if(currency_valid(r->currency)){
        out->currencies=1;memcpy(out->money[0].code,r->currency,4);out->money[0].pcs=r->pcs;out->money[0].amount=r->amount;
        out->complete=r->amount>0;
    }
    /* Evidence only. No amount is changed by these bounded serial samples.
     * An ADD cumulative serial list does not describe this incremental result. */
    if(!sim||r->multi.enabled||sim->total_pcs!=(int)r->pcs||!sim->sn_str||!sim->denom_mix)return;
    int limit=counting_data_serial_scan_limit(sim);
    for(int i=0;i<limit&&out->sample_count<CASHBOOK_SAMPLES;i++){
        const char *s=sim->sn_str[i];if(!s||!*s||sim->denom_mix[i]<=0)continue;
        uint64_t hash=1469598103934665603ULL;unsigned valid=0;
        for(unsigned j=0;s[j]&&j<64;j++){
            unsigned char ch=s[j];if(ch==' ')continue;
            if(!((ch>='0'&&ch<='9')||(ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z'))){valid=0;break;}
            hash=(hash^ch)*1099511628211ULL;valid++;
        }
        if(valid<4)continue;
        hash=(hash^(unsigned)sim->denom_mix[i])*1099511628211ULL;
        for(unsigned j=0;j<3;j++)hash=(hash^(unsigned char)r->currency[j])*1099511628211ULL;
        out->samples[out->sample_count++]=hash;
    }
}
bool counting_cashbook_commit(const cashbook_result_t *result)
{
    if(!result->pcs)return true; /* Empty motor starts are not business records. */
    cashbook_store_init();
    const cashbook_t *book=cashbook_store_get();
    for(unsigned i=0;i<book->run_count;i++)if(!memcmp(&book->runs[i].result,result,sizeof(*result)))return true;
    if(cashbook_store_ready()&&!cashbook_store_busy()){
        cashbook_command_t command={.operation=CASHBOOK_INGEST,.result=*result};
        if(cashbook_store_submit(&command)){
            book=cashbook_store_get();
            for(unsigned i=0;i<book->run_count;i++)if(!memcmp(&book->runs[i].result,result,sizeof(*result)))return true;
        }
    }
    return false;
}
