#include "history_record_snapshot.h"
#include <stdlib.h>
#include <string.h>

enum { HEADER=32, VERSION=1, COMPLETE_A=1, COMPLETE_B=2, COMPLETE_C=4 };
static void put32(unsigned char *p,uint32_t v)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static uint32_t get32(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
uint64_t history_record_snapshot_hash(const void *bytes,size_t size)
{
    const unsigned char *p=bytes;uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;i++)h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
void history_record_snapshot_release(history_record_snapshot_t *s)
{if(s){free(s->bytes);memset(s,0,sizeof(*s));}}
bool history_record_snapshot_capture(const counting_sim_t *sim,uint32_t pcs,
    bool reject_end_seen,history_record_snapshot_t *out)
{
    if(!sim || !out || sim->multi_currency_result || sim->sn_capacity<0 ||
       sim->sn_capacity>COUNTING_DATA_MAX_ITEMS || (sim->sn_capacity && !sim->sn_str) ||
       sim->denom_number>COUNTING_DENOM_MAX_ITEMS)return false;
    size_t size=HEADER;unsigned serials=0,denoms=0;
    if(sim->err_capacity<0 || sim->err_capacity>COUNTING_DATA_MAX_ITEMS ||
       sim->err_num>sim->err_capacity || (sim->err_num && (!sim->err_code || !sim->err_pcs)))return false;
    unsigned rejects=sim->err_num;
    uint64_t accepted=0,rejected=0,amount=0;
    const denom_t *denom=sim->denom;unsigned denom_count=sim->denom_number;
    for(unsigned i=0;i<sim->denom_number;i++)if(sim->denom[i].value>0){
        denoms++;size+=8;accepted+=sim->denom[i].pcs;
    }
    for(int i=0;i<sim->sn_capacity;i++)if(sim->sn_str[i]){
        size_t n=0;while(n<HISTORY_EXPORT_SN_TEXT_SIZE && sim->sn_str[i][n])n++;
        if(n>=HISTORY_EXPORT_SN_TEXT_SIZE || sim->denom_mix[i]<0)return false;
        serials++;size+=9+n;
    }
    /* The final B report identifies each accepted note. Reconcile stale or
     * partial A snapshots against that complete report, without changing the
     * live counting model. Keep A's existing order when its quantities agree. */
    denom_t reconciled[COUNTING_DENOM_MAX_ITEMS]={0};unsigned groups=0;
    bool can_reconcile=serials==pcs;
    if(can_reconcile)for(int i=0;i<sim->sn_capacity;i++)if(sim->sn_str[i]){
        int value=sim->denom_mix[i];if(value<=0){can_reconcile=false;break;}
        unsigned g=0;while(g<groups && reconciled[g].value!=value)g++;
        if(g==groups){if(groups==COUNTING_DENOM_MAX_ITEMS){can_reconcile=false;break;}reconciled[groups++].value=value;}
        reconciled[g].pcs++;
    }
    if(can_reconcile){
        bool same=accepted==pcs;
        for(unsigned g=0;g<groups;g++){
            uint64_t count=0;for(unsigned i=0;i<sim->denom_number;i++)if(sim->denom[i].value==reconciled[g].value)count+=sim->denom[i].pcs;
            if(count!=reconciled[g].pcs)same=false;
        }
        if(!same){
            size-=denoms*8U;denom=reconciled;denom_count=groups;denoms=groups;size+=groups*8U;accepted=pcs;
        }
    }
    for(unsigned i=0;i<denom_count;i++)if(denom[i].value>0)amount+=(uint64_t)denom[i].value*denom[i].pcs;
    for(unsigned i=0;i<rejects;i++)rejected+=sim->err_pcs[i];
    size+=rejects*5U;
    if(size>HISTORY_SNAPSHOT_MAX_BYTES)return false;
    unsigned char *data=calloc(1,size);if(!data)return false;
    memcpy(data,"ABC1",4);put32(data+4,VERSION);put32(data+8,pcs);
    put32(data+12,denoms);put32(data+16,serials);put32(data+20,rejects);
    put32(data+24,sim->err_expected);
    unsigned flags=(accepted==pcs?COMPLETE_A:0)|(serials==pcs?COMPLETE_B:0)|
        (reject_end_seen && rejected==sim->err_expected?COMPLETE_C:0);
    put32(data+28,flags);size_t pos=HEADER;
    for(unsigned i=0;i<denom_count;i++)if(denom[i].value>0){
        put32(data+pos,(uint32_t)denom[i].value);put32(data+pos+4,denom[i].pcs);pos+=8;
    }
    for(int i=0;i<sim->sn_capacity;i++)if(sim->sn_str[i]){
        size_t n=strlen(sim->sn_str[i]);put32(data+pos,(uint32_t)i+1);
        put32(data+pos+4,(uint32_t)sim->denom_mix[i]);data[pos+8]=(unsigned char)n;
        memcpy(data+pos+9,sim->sn_str[i],n);pos+=9+n;
    }
    for(unsigned i=0;i<rejects;i++){
        data[pos]=sim->err_code[i];put32(data+pos+1,sim->err_pcs[i]);pos+=5;
    }
    *out=(history_record_snapshot_t){.bytes=data,.size=size,.hash=history_record_snapshot_hash(data,size)};
    out->amount=amount;out->amount_complete=accepted==pcs;
    for(unsigned i=0;i<denom_count;i++)if(denom[i].value>0)out->denoms[out->denom_count++]=denom[i];
    for(unsigned i=0;i<rejects;i++)out->reject_codes[sim->err_code[i]/8]|=1U<<(sim->err_code[i]%8);
    return true;
}
bool history_record_snapshot_decode(const void *bytes,size_t size,uint64_t hash,
    uint32_t pcs,history_record_detail_t *out)
{
    const unsigned char *p=bytes;
    if(!p || !out || size<HEADER || size>HISTORY_SNAPSHOT_MAX_BYTES ||
       memcmp(p,"ABC1",4) || get32(p+4)!=VERSION || get32(p+8)!=pcs ||
       history_record_snapshot_hash(p,size)!=hash)return false;
    unsigned a=get32(p+12),b=get32(p+16),c=get32(p+20),flags=get32(p+28);
    if(a>COUNTING_DENOM_MAX_ITEMS || b>COUNTING_DATA_MAX_ITEMS || c>COUNTING_DATA_MAX_ITEMS || flags>7)return false;
    history_record_detail_t d={0};size_t pos=HEADER;
    if(b && !(d.serials=calloc(b,sizeof(*d.serials))))return false;
    if(c>HISTORY_DETAIL_MAX_REJECTS &&
       !(d.reject_extra=calloc(c-HISTORY_DETAIL_MAX_REJECTS,sizeof(*d.reject_extra))))goto failed;
    for(unsigned i=0;i<a;i++){
        if(size-pos<8)goto failed;
        uint32_t value=get32(p+pos),count=get32(p+pos+4);pos+=8;
        if(!value)goto failed;
        d.denoms[i]=(history_detail_denom_t){value,count,(uint64_t)value*count};
        d.denomination_pcs+=count;d.denomination_amount+=(uint64_t)value*count;
    }
    unsigned last_no=0;
    for(unsigned i=0;i<b;i++){
        if(size-pos<9)goto failed;
        unsigned no=get32(p+pos),value=get32(p+pos+4),n=p[pos+8];pos+=9;
        if(no<=last_no || no>COUNTING_DATA_MAX_ITEMS || n>=HISTORY_EXPORT_SN_TEXT_SIZE || size-pos<n || memchr(p+pos,0,n))goto failed;
        d.serials[i].no=no;d.serials[i].denom=value;
        memcpy(d.serials[i].sn,p+pos,n);pos+=n;last_no=no;
    }
    for(unsigned i=0;i<c;i++){
        if(size-pos<5)goto failed;
        history_detail_reject_t *r=i<HISTORY_DETAIL_MAX_REJECTS?&d.rejects[i]:&d.reject_extra[i-HISTORY_DETAIL_MAX_REJECTS];
        *r=(history_detail_reject_t){i+1,get32(p+pos+1),p[pos]};pos+=5;
        d.saved_reject_pcs+=r->pcs;
    }
    if(pos!=size || ((flags&COMPLETE_A) && d.denomination_pcs!=pcs) ||
       ((flags&COMPLETE_B) && b!=pcs) || ((flags&COMPLETE_C) && d.saved_reject_pcs!=get32(p+24)))goto failed;
    d.denom_count=a;d.serial_count=b;d.reject_count=c;
    d.denoms_complete=(flags&COMPLETE_A)!=0;d.serials_complete=(flags&COMPLETE_B)!=0;d.rejects_complete=(flags&COMPLETE_C)!=0;
    d.denoms_available=a || d.denoms_complete;d.serials_available=b || d.serials_complete;d.rejects_available=c || d.rejects_complete;
    *out=d;return true;
failed:
    history_record_detail_release(&d);return false;
}
