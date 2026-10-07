#include "cash_tools.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t magnitude(int64_t v){return v<0?(uint64_t)(-(v+1))+1:(uint64_t)v;}
bool cash_parse(const char *s,int64_t *out)
{
    if(!s||!out||!*s)return false;
    bool negative=*s=='-';if(negative)++s;
    uint64_t n=0;unsigned digits=0,fraction=0;bool dot=false;
    for(;*s;++s){
        if(*s=='.'&&!dot){dot=true;continue;}
        if(*s<'0'||*s>'9'||(dot&&fraction==6))return false;
        unsigned d=(unsigned)(*s-'0');
        if(n>((uint64_t)CASH_LIMIT-d)/10)return false;
        n=n*10+d;++digits;if(dot)++fraction;
    }
    if(!digits)return false;
    while(fraction++<6){if(n>(uint64_t)CASH_LIMIT/10)return false;n*=10;}
    *out=negative?-(int64_t)n:(int64_t)n;return true;
}
void cash_format(int64_t value,unsigned decimals,char *out,size_t size)
{
    if(decimals>6)decimals=6;
    uint64_t n=magnitude(value),fraction=n%CASH_SCALE;
    char tail[8];snprintf(tail,sizeof(tail),"%06u",(unsigned)fraction);tail[decimals]=0;
    snprintf(out,size,decimals?"%s%llu.%s":"%s%llu",value<0?"-":"",(unsigned long long)(n/CASH_SCALE),tail);
}
static bool muldiv(uint64_t a,uint64_t b,uint64_t d,uint64_t *out)
{
    if(!d)return false;
    uint64_t q=0,r=0;
    for(int bit=63;bit>=0;--bit){
        r=r*2+((a>>bit)&1U)*b;
        uint64_t add=r/d;r%=d;
        if(add>(uint64_t)CASH_LIMIT||q>((uint64_t)CASH_LIMIT-add)/2)return false;
        q=q*2+add;
    }
    if(r>=(d+1)/2){if(q==(uint64_t)CASH_LIMIT)return false;++q;}
    *out=q;return true;
}
cash_error_t cash_calculate(int64_t a,char op,int64_t b,int64_t *out)
{
    if(!out||magnitude(a)>CASH_LIMIT||magnitude(b)>CASH_LIMIT)return CASH_INVALID;
    int64_t result;
    if(op=='+'||op=='-')result=op=='+'?a+b:a-b;
    else if(op=='*'||op=='/'){
        if(op=='/'&&!b)return CASH_DIV_ZERO;
        uint64_t n;
        if(!muldiv(magnitude(a),op=='*'?magnitude(b):CASH_SCALE,
                   op=='*'?CASH_SCALE:magnitude(b),&n))return CASH_OVERFLOW;
        result=(a<0)!=(b<0)?-(int64_t)n:(int64_t)n;
    }else return CASH_INVALID;
    if(magnitude(result)>CASH_LIMIT)return CASH_OVERFLOW;
    *out=result;return CASH_OK;
}
int cash_currency_decimals(const char *code)
{
    if(!code||strlen(code)!=3)return -1;
    static const char *const zero[]={"JPY","KRW","ISK","XOF","XAF"};
    static const char *const three[]={"OMR","IQD","BHD","KWD","JOD","TND","LYD"};
    static const char *const two[]={"EUR","USD","CNY","RUB","TRY","GBP","MXN","CAD","ILS","AED","SAR","IRR","HKD","AUD","SGD","CHF","ZAR","IDR","MOP","PKR","QAR","THB","TJS","UAH","AMD","AZN","LBP","EGP","PHP","INR","SOS","MAD","DZD","PLN"};
    for(unsigned i=0;i<sizeof(zero)/sizeof(*zero);++i)if(!strcmp(code,zero[i]))return 0;
    for(unsigned i=0;i<sizeof(three)/sizeof(*three);++i)if(!strcmp(code,three[i]))return 3;
    for(unsigned i=0;i<sizeof(two)/sizeof(*two);++i)if(!strcmp(code,two[i]))return 2;
    return -1;
}
bool cash_amount_precision(const char code[4],int64_t amount)
{
    if(!code||code[3])return false;
    int decimals=cash_currency_decimals(code);if(decimals<0)return false;
    int64_t unit=1;for(int i=decimals;i<6;++i)unit*=10;
    return amount>=0&&amount<=CASH_LIMIT&&amount%unit==0;
}
static bool precise(const cash_snapshot_t *s){return cash_amount_precision(s->code,s->amount);}
static bool snapshot_equal(const cash_snapshot_t *a,const cash_snapshot_t *b)
{
    if(strcmp(a->code,b->code)||a->record!=b->record||a->recorded_at!=b->recorded_at||a->amount!=b->amount||a->count!=b->count||a->group!=b->group||a->day!=b->day||a->operator_id!=b->operator_id||a->original_amount!=b->original_amount||memcmp(a->operator_name,b->operator_name,sizeof(a->operator_name)))return false;
    for(unsigned i=0;i<a->count;++i)if(a->denoms[i].value!=b->denoms[i].value||a->denoms[i].quantity!=b->denoms[i].quantity)return false;
    return true;
}
cash_error_t cash_snapshot_total(cash_snapshot_t *s)
{
    if(!s||s->count>CASH_DENOMS)return CASH_INVALID;
    int64_t total=0;
    for(unsigned i=0;i<s->count;++i){
        const cash_denom_t *d=&s->denoms[i];
        if(d->value<=0||d->value>CASH_LIMIT||d->quantity>99999)return CASH_INVALID;
        cash_snapshot_t face={0};memcpy(face.code,s->code,4);face.amount=d->value;
        if(!precise(&face))return CASH_PRECISION;
        for(unsigned j=0;j<i;++j)if(s->denoms[j].value==d->value)return CASH_INVALID;
        if(d->quantity&&(d->value>(CASH_LIMIT-total)/d->quantity))return CASH_OVERFLOW;
        total+=d->value*d->quantity;
    }
    s->amount=total;return precise(s)?CASH_OK:CASH_PRECISION;
}
cash_error_t cash_convert(int64_t amount,int64_t rate,const char *target,int64_t *out)
{
    int decimals=cash_currency_decimals(target);if(!out||amount<=0||rate<=0||rate>999999999999LL||decimals<0)return CASH_INVALID;
    uint64_t unit=1,n;for(int i=decimals;i<6;++i)unit*=10;
    /* Round only once, to the target currency quantum, not twice via micros. */
    if(!muldiv((uint64_t)amount,(uint64_t)rate,CASH_SCALE*unit,&n)||n>(uint64_t)CASH_LIMIT/unit)return CASH_OVERFLOW;
    *out=(int64_t)(n*unit);return *out>0?CASH_OK:CASH_PRECISION;
}
bool cash_journal_opened(const cash_journal_t *j,const char *code)
{if(!j)return false;for(unsigned i=0;i<j->count;++i)if(j->entries[i].kind==CASH_OPENING&&!strcmp(j->entries[i].cash.code,code))return true;return false;}
int64_t cash_entry_delta(const cash_journal_t *j,const cash_entry_t *e,const char *code)
{
    if(e->kind==CASH_AUDIT)return 0;
    if(e->kind==CASH_REVERSAL){if(!e->reverse_of||e->reverse_of>j->count||j->entries[e->reverse_of-1].kind==CASH_REVERSAL)return 0;return -cash_entry_delta(j,&j->entries[e->reverse_of-1],code);}
    int64_t n=!strcmp(e->cash.code,code)?e->cash.amount:0;
    if(e->kind==CASH_PAYMENT||e->kind==CASH_EXCHANGE)n=-n;
    if(e->kind==CASH_EXCHANGE&&!strcmp(e->peer.code,code))n+=e->peer.amount;
    return n;
}
int64_t cash_journal_balance(const cash_journal_t *j,const char *code)
{int64_t n=0;if(j)for(unsigned i=0;i<j->count;++i)n+=cash_entry_delta(j,&j->entries[i],code);return n;}
bool cash_journal_reversed(const cash_journal_t *j,uint32_t id)
{if(j)for(unsigned i=0;i<j->count;++i)if(j->entries[i].kind==CASH_REVERSAL&&j->entries[i].reverse_of==id)return true;return false;}
unsigned cash_journal_entry_actions(const cash_journal_t *j,uint32_t id)
{
    if(!j||!id||id>j->count||cash_journal_reversed(j,id))return 0;
    unsigned kind=j->entries[id-1].kind;
    if(kind==CASH_REVERSAL)return 0;
    if(kind==CASH_OPENING)return CASH_ACTION_CORRECT;
    if(kind==CASH_AUDIT)return CASH_ACTION_VOID;
    return kind==CASH_RECEIPT||kind==CASH_PAYMENT||kind==CASH_EXCHANGE?CASH_ACTION_CORRECT|CASH_ACTION_VOID:0;
}
bool cash_entry_is_audit(const cash_journal_t *j,const cash_entry_t *e)
{return e&&(e->kind==CASH_AUDIT||(e->kind==CASH_REVERSAL&&j&&e->reverse_of&&e->reverse_of<=j->count&&j->entries[e->reverse_of-1].kind==CASH_AUDIT));}
static bool same_source(const cash_snapshot_t *a,const cash_snapshot_t *b)
{
    if(strcmp(a->code,b->code))return false;
    if(a->group&&b->group)return a->group==b->group&&a->day==b->day;
    return a->record&&a->record==b->record&&a->recorded_at==b->recorded_at;
}
bool cash_journal_source_used(const cash_journal_t *j,const cash_snapshot_t *s)
{if(!j||!s->record)return false;for(unsigned i=0;i<j->count;++i){const cash_entry_t *e=&j->entries[i];if(e->kind!=CASH_REVERSAL&&e->kind!=CASH_AUDIT&&same_source(&e->cash,s)&&!cash_journal_reversed(j,e->id))return true;}return false;}
static cash_error_t entry_valid(const cash_entry_t *e)
{
    if(e->kind<CASH_OPENING||e->kind>CASH_AUDIT||e->time<1577836800ULL||e->note[64]||e->operator_name[24]||e->cash.operator_name[24]||e->approved>1)return CASH_INVALID;
    if(e->peer.code[3]||e->peer.operator_name[24]||e->peer.count>CASH_DENOMS)return CASH_INVALID;
    if(!precise(&e->cash))return CASH_PRECISION;
    if(e->cash.count){cash_snapshot_t s=e->cash;if(cash_snapshot_total(&s)||s.amount!=e->cash.amount)return CASH_INVALID;}
    if(!e->cash.amount&&e->kind!=CASH_OPENING&&e->kind!=CASH_AUDIT&&e->kind!=CASH_REVERSAL)return CASH_INVALID;
    if(e->kind==CASH_EXCHANGE){if(!precise(&e->peer)||!e->peer.amount||!strcmp(e->cash.code,e->peer.code)||e->peer.record||e->peer.count||e->rate<=0)return CASH_INVALID;}
    if(e->kind==CASH_AUDIT&&(!precise(&e->peer)||strcmp(e->cash.code,e->peer.code)||e->reverse_of||e->corrects))return CASH_INVALID;
    return CASH_OK;
}
static cash_error_t final_balance(const cash_journal_t *j,const cash_entry_t *e)
{
    const char *codes[]={e->cash.code,e->peer.code};
    for(unsigned i=0;i<2;++i){if(!codes[i][0])continue;int64_t n=cash_journal_balance(j,codes[i]);if(n<0)return CASH_BALANCE;if(n>CASH_LIMIT)return CASH_OVERFLOW;}
    return CASH_OK;
}
cash_error_t cash_journal_append(cash_journal_t *j,const cash_entry_t *entry)
{
    if(!j||!entry||j->count>=CASH_ENTRIES)return CASH_FULL;
    cash_entry_t e=*entry;e.id=j->count+1;
    cash_error_t err=entry_valid(&e);if(err)return err;
    if(e.corrects)return CASH_INVALID;
    if(e.kind==CASH_OPENING){if(cash_journal_opened(j,e.cash.code))return CASH_ALREADY_OPEN;if(e.reverse_of||e.cash.record)return CASH_INVALID;}
    else if(!cash_journal_opened(j,e.cash.code))return CASH_NEEDS_OPENING;
    if(e.kind==CASH_EXCHANGE&&!cash_journal_opened(j,e.peer.code))return CASH_NEEDS_OPENING;
    if(e.kind==CASH_REVERSAL){
        if(!e.reverse_of||e.reverse_of>j->count)return CASH_INVALID;
        const cash_entry_t *old=&j->entries[e.reverse_of-1];
        if(!(cash_journal_entry_actions(j,old->id)&CASH_ACTION_VOID))return cash_journal_reversed(j,old->id)?CASH_REVERSED:CASH_INVALID;
        if(cash_journal_reversed(j,old->id))return CASH_REVERSED;
        if(!snapshot_equal(&e.cash,&old->cash)||!snapshot_equal(&e.peer,&old->peer))return CASH_INVALID;
    }else{
        if(e.reverse_of)return CASH_INVALID;
        if(e.kind!=CASH_AUDIT&&cash_journal_source_used(j,&e.cash))return CASH_DUPLICATE;
        if(j->count>=CASH_NEW_LIMIT)return CASH_FULL;
    }
    j->entries[j->count++]=e;err=final_balance(j,&e);
    if(err){memset(&j->entries[--j->count],0,sizeof(e));return err;}
    j->version=2;return CASH_OK;
}
cash_error_t cash_journal_correct(cash_journal_t *j,uint32_t id,const cash_entry_t *replacement,uint64_t time)
{
    if(!j||!replacement||!id||id>j->count||j->count>CASH_ENTRIES-2)return CASH_INVALID;
    const cash_entry_t *old=&j->entries[id-1];
    if(old->kind==CASH_REVERSAL||old->kind==CASH_AUDIT)return CASH_INVALID;
    if(cash_journal_reversed(j,id))return CASH_REVERSED;
    cash_error_t error=entry_valid(replacement);if(error)return error;
    if(replacement->kind!=old->kind||strcmp(replacement->cash.code,old->cash.code)||strcmp(replacement->peer.code,old->peer.code)||replacement->cash.record!=old->cash.record||replacement->cash.recorded_at!=old->cash.recorded_at||replacement->cash.group!=old->cash.group||replacement->cash.day!=old->cash.day)return CASH_INVALID;
    cash_journal_t *copy=malloc(sizeof(*copy));if(!copy)return CASH_INVALID;*copy=*j;
    cash_entry_t reverse=*old;reverse.id=copy->count+1;reverse.kind=CASH_REVERSAL;reverse.reverse_of=id;reverse.corrects=0;reverse.time=time;
    reverse.operator_id=replacement->operator_id;memcpy(reverse.operator_name,replacement->operator_name,sizeof(reverse.operator_name));reverse.approved=replacement->approved;
    snprintf(reverse.note,sizeof(reverse.note),"#%u: %.48s",id,replacement->note);copy->entries[copy->count++]=reverse;
    cash_entry_t e=*replacement;e.id=copy->count+1;e.reverse_of=0;e.corrects=id;
    if(cash_journal_source_used(copy,&e.cash))error=CASH_DUPLICATE;
    else{copy->entries[copy->count++]=e;error=final_balance(copy,&e);}
    if(!error){copy->version=2;*j=*copy;}free(copy);return error;
}
bool cash_journal_valid(const cash_journal_t *j)
{
    if(!j||j->version!=2||j->count>CASH_ENTRIES)return false;
    for(unsigned i=0;i<j->count;++i)if(j->entries[i].id!=i+1||entry_valid(&j->entries[i]))return false;
    for(unsigned i=0;i<j->count;++i){const cash_entry_t *e=&j->entries[i];
        if(e->kind==CASH_REVERSAL){
            if(!e->reverse_of||e->reverse_of>=e->id)return false;
            const cash_entry_t *o=&j->entries[e->reverse_of-1];
            if(o->kind==CASH_REVERSAL||!snapshot_equal(&o->cash,&e->cash)||!snapshot_equal(&o->peer,&e->peer))return false;
            if(o->kind==CASH_OPENING&&(i+1>=j->count||j->entries[i+1].corrects!=o->id))return false;
        }else if(e->reverse_of)return false;
        if(e->corrects){if(e->kind==CASH_REVERSAL||!i||e->corrects>=e->id||j->entries[i-1].kind!=CASH_REVERSAL||j->entries[i-1].reverse_of!=e->corrects)return false;const cash_entry_t *o=&j->entries[e->corrects-1];if(o->kind!=e->kind||strcmp(o->cash.code,e->cash.code)||strcmp(o->peer.code,e->peer.code)||o->cash.record!=e->cash.record||o->cash.recorded_at!=e->cash.recorded_at)return false;}
        if(!cash_journal_opened(j,e->cash.code))return false;
        if(e->kind==CASH_EXCHANGE&&!cash_journal_opened(j,e->peer.code))return false;
        for(unsigned k=0;k<i;++k){const cash_entry_t *p=&j->entries[k];
            if(e->kind==CASH_REVERSAL&&p->kind==CASH_REVERSAL&&e->reverse_of==p->reverse_of)return false;
            if(e->kind==CASH_OPENING&&p->kind==CASH_OPENING&&!strcmp(e->cash.code,p->cash.code)&&!e->corrects)return false;
            if(e->kind!=CASH_REVERSAL&&e->kind!=CASH_AUDIT&&p->kind!=CASH_REVERSAL&&p->kind!=CASH_AUDIT&&same_source(&e->cash,&p->cash)&&!cash_journal_reversed(j,e->id)&&!cash_journal_reversed(j,p->id))return false;
        }
    }
    for(unsigned i=0;i<j->count;++i)if(final_balance(j,&j->entries[i]))return false;
    return true;
}
