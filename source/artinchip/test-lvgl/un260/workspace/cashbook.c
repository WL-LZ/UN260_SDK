#include "cashbook.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
static bool fail(char *reason,unsigned n,const char *s){if(reason&&n)snprintf(reason,n,"%s",s);return false;}
bool cashbook_day_valid(uint32_t day)
{
    unsigned y=day/10000,m=day/100%100,d=day%100;
    static const unsigned days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2020||y>2099||m<1||m>12)return false;
    unsigned max=days[m]+(m==2&&y%4==0);
    return d>=1&&d<=max;
}
static bool code_valid(const char c[4])
{return !c[3]&&c[0]>='A'&&c[0]<='Z'&&c[1]>='A'&&c[1]<='Z'&&c[2]>='A'&&c[2]<='Z'&&strcmp(c,"MUL")&&strcmp(c,"AUT");}
static bool result_valid(const cashbook_result_t *r)
{
    if(!r->source||!r->pcs||r->currencies>CASHBOOK_CURRENCIES||r->sample_count>CASHBOOK_SAMPLES||
       !memchr(r->operator_name,0,sizeof(r->operator_name))||r->hour>23||r->minute>59||r->second>59)return false;
    uint64_t pcs=0;
    for(unsigned i=0;i<r->currencies;i++){
        if(!code_valid(r->money[i].code)||!r->money[i].pcs)return false;
        pcs+=r->money[i].pcs;
        for(unsigned j=0;j<i;j++)if(!strcmp(r->money[i].code,r->money[j].code))return false;
    }
    return pcs==r->pcs||!r->complete;
}
void cashbook_defaults(cashbook_t *b){memset(b,0,sizeof(*b));b->version=1;}
const cashbook_run_t *cashbook_run(const cashbook_t *b,uint32_t id)
{return b&&id&&id<=b->run_count&&b->runs[id-1].id==id?&b->runs[id-1]:NULL;}
const cashbook_group_t *cashbook_group(const cashbook_t *b,uint32_t id)
{return b&&id&&id<=b->group_count&&b->groups[id-1].id==id?&b->groups[id-1]:NULL;}
bool cashbook_closed(const cashbook_t *b,uint32_t day)
{for(unsigned i=0;i<b->close_count;i++)if(b->closes[i].day==day&&b->closes[i].current)return true;return false;}
unsigned cashbook_attempts(const cashbook_t *b,uint32_t group)
{unsigned n=0;for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].group==group)n++;return n;}
static bool money_add(cashbook_money_t *out,unsigned *n,const cashbook_money_t *v)
{
    unsigned i=0;for(;i<*n;i++)if(!strcmp(out[i].code,v->code))break;
    if(i==*n){if(*n>=CASHBOOK_CURRENCIES)return false;memset(&out[i],0,sizeof(out[i]));memcpy(out[i].code,v->code,4);(*n)++;}
    if(v->pcs>UINT32_MAX-out[i].pcs||v->amount>UINT64_MAX-out[i].amount)return false;
    out[i].pcs+=v->pcs;out[i].amount+=v->amount;return true;
}
bool cashbook_totals(const cashbook_t *b,uint32_t day,cashbook_money_t *out,unsigned *n,unsigned *confirmed,unsigned *pending)
{
    *n=*confirmed=*pending=0;memset(out,0,CASHBOOK_CURRENCIES*sizeof(*out));
    for(unsigned i=0;i<b->group_count;i++){
        const cashbook_group_t *g=&b->groups[i];if(g->day!=day||g->excluded)continue;
        if(!g->confirmed||g->review)(*pending)++;
        if(!g->confirmed)continue;
        const cashbook_run_t *r=cashbook_run(b,g->selected);if(!r)return false;
        (*confirmed)++;
        for(unsigned j=0;j<r->result.currencies;j++)if(!money_add(out,n,&r->result.money[j]))return false;
    }return true;
}
bool cashbook_valid(const cashbook_t *b)
{
    if(!b||b->version!=1||b->run_count>CASHBOOK_RUNS||b->group_count>CASHBOOK_RUNS||b->close_count>CASHBOOK_CLOSES)return false;
    for(unsigned i=0;i<b->run_count;i++){
        const cashbook_run_t *r=&b->runs[i];
        if(r->id!=i+1||!result_valid(&r->result)||!cashbook_group(b,r->group)||
           (r->candidate&&!cashbook_group(b,r->candidate)))return false;
    }
    for(unsigned i=0;i<b->group_count;i++){
        const cashbook_group_t *g=&b->groups[i];const cashbook_run_t *r=cashbook_run(b,g->selected);
        if(g->id!=i+1||(!g->excluded&&(!r||r->group!=g->id))||
           (g->confirmed&&(!r||!r->result.complete||!cashbook_day_valid(g->day))))return false;
    }
    for(unsigned i=0;i<b->close_count;i++){
        const cashbook_close_t *c=&b->closes[i];
        if(c->id!=i+1||!cashbook_day_valid(c->day)||!c->revision||c->groups>CASHBOOK_RUNS||c->currencies>CASHBOOK_CURRENCIES)return false;
        for(unsigned j=0;j<c->groups;j++)if(!cashbook_run(b,c->selected[j]))return false;
    }return true;
}
static unsigned overlap(const cashbook_result_t *a,const cashbook_result_t *b)
{
    bool used[CASHBOOK_SAMPLES]={0};unsigned n=0;
    for(unsigned i=0;i<a->sample_count;i++)for(unsigned j=0;j<b->sample_count;j++)
        if(!used[j]&&a->samples[i]==b->samples[j]){used[j]=true;n++;break;}
    return n;
}
bool cashbook_apply(cashbook_t *b,const cashbook_command_t *c,char *reason,unsigned capacity)
{
    if(!b||!c)return fail(reason,capacity,"Invalid operation.");
    cashbook_group_t *g=(cashbook_group_t *)cashbook_group(b,c->group);
    cashbook_run_t *r=(cashbook_run_t *)cashbook_run(b,c->run);
    if(c->operation==CASHBOOK_INGEST){
        if(!result_valid(&c->result))return fail(reason,capacity,"Incomplete or invalid result metadata.");
        const cashbook_run_t *previous=NULL;
        for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].result.source==c->result.source){
            previous=&b->runs[i];
            if(!memcmp(&previous->result,&c->result,sizeof(c->result)))return true;
        }
        if(b->run_count==CASHBOOK_RUNS||b->group_count==CASHBOOK_RUNS)return fail(reason,capacity,"Record storage is full. Export and archive closed days before more counts.");
        uint32_t day=b->business_day?b->business_day:c->result.day;
        if(previous){g=(cashbook_group_t *)cashbook_group(b,previous->group);day=g->day;}
        /* A closed day is frozen. Late data waits unassigned for an explicit decision. */
        if(cashbook_closed(b,day)){previous=NULL;day=0;}
        bool explicit_recount=false;
        if(!previous&&c->result.recount_group){
            cashbook_group_t *target=(cashbook_group_t *)cashbook_group(b,c->result.recount_group);
            const cashbook_run_t *selected=target?cashbook_run(b,target->selected):NULL;
            if(target&&!target->excluded&&!cashbook_closed(b,target->day)&&selected&&selected->result.currencies==1&&c->result.currencies==1&&
               !strcmp(selected->result.money[0].code,c->result.money[0].code)){
                g=target;explicit_recount=true;day=g->day;g->review=true;
            }
        }
        if(!previous&&!explicit_recount){g=&b->groups[b->group_count];*g=(cashbook_group_t){.id=b->group_count+1,.day=day};b->group_count++;}
        r=&b->runs[b->run_count];*r=(cashbook_run_t){.id=b->run_count+1,.group=g->id,.result=c->result};b->run_count++;
        if(!g->selected)g->selected=r->id;
        if(previous)g->review=true;
        if(r->result.sample_count>=3)for(unsigned i=0;i+1<b->run_count;i++){
            const cashbook_run_t *other=&b->runs[i];
            const cashbook_group_t *og=cashbook_group(b,other->group);
            if(other->group==r->group||og->day!=day||og->excluded||other->result.currencies!=1||r->result.currencies!=1||
               strcmp(other->result.money[0].code,r->result.money[0].code))continue;
            unsigned min=other->result.sample_count<r->result.sample_count?other->result.sample_count:r->result.sample_count;
            if(min>=3&&overlap(&other->result,&r->result)*100>=min*80){r->candidate=other->group;g->review=true;break;}
        }
    }else if(c->operation==CASHBOOK_SET_DAY){
        if(!cashbook_day_valid(c->day)||cashbook_closed(b,c->day))return fail(reason,capacity,"Choose a valid, open business date.");
        b->business_day=c->day;
        for(unsigned i=0;i<b->group_count;i++)if(!b->groups[i].day&&!b->groups[i].confirmed)b->groups[i].day=c->day;
    }else if(c->operation==CASHBOOK_CONFIRM_SINGLES){
        if(!cashbook_day_valid(c->day)||cashbook_closed(b,c->day))return fail(reason,capacity,"Choose a valid, open business date.");
        unsigned confirmed=0;uint16_t attempts[CASHBOOK_RUNS]={0};
        for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].group&&b->runs[i].group<=CASHBOOK_RUNS)attempts[b->runs[i].group-1]++;
        for(unsigned i=0;i<b->group_count;i++){
            cashbook_group_t *single=&b->groups[i];const cashbook_run_t *selected=cashbook_run(b,single->selected);
            if(single->day==c->day&&!single->excluded&&!single->confirmed&&!single->review&&attempts[i]==1&&selected&&selected->result.complete&&!selected->candidate){single->confirmed=true;confirmed++;}
        }
        if(!confirmed)return fail(reason,capacity,"No complete, unambiguous single counts need confirmation. Review recounts individually.");
    }else if(c->operation==CASHBOOK_CLOSE){
        if(!cashbook_day_valid(c->day)||cashbook_closed(b,c->day)||b->close_count==CASHBOOK_CLOSES)return fail(reason,capacity,"Check the business date and available close slots.");
        cashbook_money_t money[CASHBOOK_CURRENCIES];unsigned n,confirmed,pending;
        if(!cashbook_totals(b,c->day,money,&n,&confirmed,&pending)||pending||!confirmed)return fail(reason,capacity,"Resolve every pending count before closing this day.");
        cashbook_close_t *close=&b->closes[b->close_count];memset(close,0,sizeof(*close));
        close->id=b->close_count+1;close->day=c->day;close->operator_id=c->operator_id;close->revision=1;close->current=true;close->currencies=n;
        memcpy(close->money,money,sizeof(money));
        for(unsigned i=0;i<b->close_count;i++)if(b->closes[i].day==c->day&&b->closes[i].revision>=close->revision)close->revision=b->closes[i].revision+1;
        for(unsigned i=0;i<b->group_count;i++)if(b->groups[i].day==c->day&&!b->groups[i].excluded)close->selected[close->groups++]=b->groups[i].selected;
        b->close_count++;if(b->business_day==c->day)b->business_day=0;
    }else if(c->operation==CASHBOOK_REOPEN){
        bool found=false;for(unsigned i=0;i<b->close_count;i++)if(b->closes[i].day==c->day&&b->closes[i].current){b->closes[i].current=false;found=true;}
        if(!found)return fail(reason,capacity,"This day is not closed.");
    }else{
        if(!g||cashbook_closed(b,g->day))return fail(reason,capacity,"Reopen the closed day before changing a result.");
        switch(c->operation){
        case CASHBOOK_CONFIRM: case CASHBOOK_SELECT:
            if(!r||r->group!=g->id||!r->result.complete||!cashbook_day_valid(g->day))return fail(reason,capacity,"Choose a complete result and a valid business date first.");
            g->selected=r->id;g->confirmed=true;g->review=false;g->excluded=false;
            for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].group==g->id)b->runs[i].candidate=0;
            break;
        case CASHBOOK_MERGE:{
            if(!r||r->group==g->id||g->excluded)return fail(reason,capacity,"Choose a different, active destination group.");
            cashbook_group_t *source=(cashbook_group_t *)cashbook_group(b,r->group);
            if(source->day!=g->day||cashbook_closed(b,source->day)||source->confirmed)return fail(reason,capacity,"Only an unconfirmed group in this day can be attached.");
            for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].group==source->id){b->runs[i].group=g->id;b->runs[i].candidate=0;}
            source->excluded=true;source->review=false;g->review=true;
            for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].candidate==source->id)b->runs[i].candidate=g->id;
            break;}
        case CASHBOOK_SPLIT:
            if(!r||r->group!=g->id||cashbook_attempts(b,g->id)<2||b->group_count==CASHBOOK_RUNS)return fail(reason,capacity,"Choose one result from a recount group.");
            b->groups[b->group_count]=(cashbook_group_t){.id=b->group_count+1,.day=g->day,.selected=r->id};
            r->group=++b->group_count;r->candidate=0;
            if(g->selected==r->id){g->confirmed=false;for(unsigned i=0;i<b->run_count;i++)if(b->runs[i].group==g->id){g->selected=b->runs[i].id;break;}}
            g->review=true;break;
        case CASHBOOK_EXCLUDE:g->excluded=true;g->confirmed=false;g->review=false;break;
        case CASHBOOK_RESTORE:if(!cashbook_attempts(b,g->id))return fail(reason,capacity,"This group was merged. Split its result from the destination instead.");g->excluded=false;g->confirmed=false;g->review=true;break;
        case CASHBOOK_ASSIGN_DAY:
            if(g->confirmed||!cashbook_day_valid(c->day)||cashbook_closed(b,c->day))return fail(reason,capacity,"Choose an open date for an unconfirmed count.");
            g->day=c->day;g->review=true;break;
        default:return fail(reason,capacity,"Unsupported operation.");
        }
    }
    if(b->sequence==UINT32_MAX)return fail(reason,capacity,"Ledger sequence exhausted.");
    b->sequence++;if(reason&&capacity)snprintf(reason,capacity,"Saved. Counting results are unchanged.");return true;
}
