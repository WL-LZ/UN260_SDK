#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_data_store.h"
#include "un260/storage/cashbook_store.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/ui_i18n.h"
#include "cash_tools_service.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/currency/currency_state.h"
#include "un260/storage/workspace_store.h"
#include "un260/storage/cash_journal_store.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "un260/lv_system/machine_time.h"
static void add_code(char (*codes)[4],unsigned *n,unsigned cap,const char *code)
{
    if(*n>=cap||!code||code[3]||cash_currency_decimals(code)<0)return;
    for(unsigned i=0;i<*n;++i)if(!strcmp(codes[i],code))return;
    memcpy(codes[(*n)++],code,4);
}
unsigned cash_tools_currencies(char (*codes)[4],unsigned cap)
{
    unsigned n=0;char code[4];currency_state_get_effective_code(code);add_code(codes,&n,cap,code);
    for(unsigned i=0;i<currency_state_count();++i)if(currency_state_get_code(i,code))add_code(codes,&n,cap,code);
    const cash_journal_t *j=cash_journal_store_get();if(j&&cash_journal_store_status()!=CASH_STORE_LOADING&&cash_journal_store_status()!=CASH_STORE_CORRUPT)
        for(unsigned i=0;i<j->count;++i)add_code(codes,&n,cap,j->entries[i].cash.code);
    return n;
}
bool cash_tools_history_ready(void){return ui_history_data_is_available()&&ui_history_data_status()==STORAGE_JOB_SUCCEEDED;}
static bool snapshot(const ui_history_record_t *r,unsigned group,cash_snapshot_t *s)
{
    memset(s,0,sizeof(*s));if(!r->valid||!r->record_no||r->year<2020||r->month<1||r->month>12||r->day<1||r->day>31)return false;
    s->record=r->record_no;
    s->recorded_at=(((((uint64_t)r->year*100+r->month)*100+r->day)*100+r->hour)*100+r->minute)*100+r->second;
    uint64_t amount,pcs=0,expected;
    if(r->multi.enabled){
        if(r->multi.overflow||group>=r->multi.count||group>=HISTORY_MULTI_CURRENCIES)return false;
        const history_multi_currency_t *c=&r->multi.currencies[group];if(!c->complete||c->count>CASH_DENOMS)return false;
        memcpy(s->code,c->code,4);amount=c->amount;expected=c->pcs;s->count=c->count;
        for(unsigned i=0;i<s->count;++i){s->denoms[i].value=(int64_t)c->denoms[i].value*CASH_SCALE;s->denoms[i].quantity=c->denoms[i].pcs;pcs+=c->denoms[i].pcs;}
    }else{
        memcpy(s->code,r->currency,4);amount=r->amount;expected=r->pcs;
        const char *p=r->denom_text,*end=r->denom_text+sizeof(r->denom_text);
        if(!memchr(p,0,sizeof(r->denom_text)))return false;
        while(*p){unsigned value,count;int n=0;if(s->count>=CASH_DENOMS||sscanf(p,"%u x %u%n",&value,&count,&n)!=2||n<=0||p+n>=end)return false;
            s->denoms[s->count++]=(cash_denom_t){(int64_t)value*CASH_SCALE,count};pcs+=count;p+=n;
            if(*p=='\n')++p;else if(*p)return false;
        }
    }
    s->operator_id=r->operator_id;memcpy(s->operator_name,r->operator_name,sizeof(s->operator_name));s->original_amount=(int64_t)amount*CASH_SCALE;
    const cashbook_t *book=cashbook_store_get();
    if(book)for(unsigned i=0;i<book->run_count;++i)if(book->runs[i].result.source==r->record_no){s->group=book->runs[i].group;s->day=book->runs[i].result.day;break;}
    return s->count&&pcs==expected&&cash_snapshot_total(s)==CASH_OK&&s->amount==(int64_t)amount*CASH_SCALE&&s->amount>0;
}
unsigned cash_tools_history_count(void)
{
    if(!cash_tools_history_ready())return 0;
    const ui_history_store_t *h=ui_history_data_get();unsigned n=0;cash_snapshot_t s;
    for(unsigned i=0;i<UI_HISTORY_MAX_RECORDS;++i){const ui_history_record_t *r=&h->records[i];unsigned count=r->multi.enabled?r->multi.count:1;if(count>HISTORY_MULTI_CURRENCIES)continue;
        for(unsigned g=0;g<count;++g)if(snapshot(r,g,&s))++n;}
    return n;
}
bool cash_tools_history(unsigned index,cash_snapshot_t *out)
{
    if(!out||!cash_tools_history_ready())return false;
    const ui_history_store_t *h=ui_history_data_get();unsigned n=0;cash_snapshot_t s;
    /* Reverse stable ID order, independent of ring position and deletions. */
    uint32_t before=UINT32_MAX;
    for(unsigned row=0;row<UI_HISTORY_MAX_RECORDS;++row){const ui_history_record_t *r=NULL;
        for(unsigned i=0;i<UI_HISTORY_MAX_RECORDS;++i)if(h->records[i].valid&&h->records[i].record_no<before&&(!r||h->records[i].record_no>r->record_no))r=&h->records[i];
        if(!r)break;
        before=r->record_no;unsigned count=r->multi.enabled?r->multi.count:1;if(count>HISTORY_MULTI_CURRENCIES)continue;
        for(unsigned g=0;g<count;++g)if(snapshot(r,g,&s)){if(n++==index){*out=s;return true;}}
    }return false;
}
void cash_tools_entry_identity(cash_entry_t *entry)
{
    machine_time_value_t t;machine_time_get(&t);entry->time=0;
    if(machine_time_is_confirmed()&&machine_time_is_valid(&t)&&t.year>=2020&&t.year<=2099){
        struct tm date={.tm_year=t.year-1900,.tm_mon=t.month-1,.tm_mday=t.day,.tm_hour=t.hour,.tm_min=t.minute,.tm_sec=t.second,.tm_isdst=-1};
        time_t now=mktime(&date);if(now>0)entry->time=(uint64_t)now;
    }
    const workspace_model_t *m=workspace_store_get();const workspace_user_t *u=m?workspace_active(m):NULL;
    entry->operator_id=u?u->id:0;snprintf(entry->operator_name,sizeof(entry->operator_name),"%s",u?u->name:"");
}

const char *cash_tools_write_blocker(void)
{
    if(!workspace_store_ready()||workspace_store_busy())return UI_N_("Wait for the workspace save to finish.");
    if(app_command_runtime_count_start_busy()||machine_state_aging_running())return UI_N_("Finish counting before confirming cash.");
    if(app_command_runtime_result_pending())return UI_N_("Wait for the counting result to finish.");
    return NULL;
}
const char *cash_tools_current_blocker(void)
{
    const char *reason=cash_tools_write_blocker();if(reason)return reason;
    const counting_sim_t *data=counting_data_current();
    if(!data||!counting_data_monetary_result_supported(data))return UI_N_("CNT has note counts only, not a cash amount.");
    if(machine_state_add_enabled()&&data->total_pcs>0)return UI_N_("Finish ADD before importing the complete result.");
    if(!cash_tools_history_ready()||!counting_history_current_record())return UI_N_("The current count is not saved or has been cleared.");
    return NULL;
}
unsigned cash_tools_current_count(void)
{
    if(cash_tools_current_blocker())return 0;
    uint32_t id=counting_history_current_record();const ui_history_store_t *h=ui_history_data_get();cash_snapshot_t s;
    for(unsigned i=0;i<UI_HISTORY_MAX_RECORDS;++i)if(h->records[i].valid&&h->records[i].record_no==id){const ui_history_record_t *r=&h->records[i];unsigned n=0;
        for(unsigned g=0;g<(r->multi.enabled?r->multi.count:1);++g)if(snapshot(r,g,&s))++n;
        return n;}
    return 0;
}
bool cash_tools_current(unsigned index,cash_snapshot_t *out)
{
    if(!out||cash_tools_current_blocker())return false;
    uint32_t id=counting_history_current_record();const ui_history_store_t *h=ui_history_data_get();cash_snapshot_t s;
    for(unsigned i=0;i<UI_HISTORY_MAX_RECORDS;++i)if(h->records[i].valid&&h->records[i].record_no==id){const ui_history_record_t *r=&h->records[i];unsigned n=0;
        for(unsigned g=0;g<(r->multi.enabled?r->multi.count:1);++g)if(snapshot(r,g,&s)&&n++==index){*out=s;return true;}
        break;}
    return false;
}
