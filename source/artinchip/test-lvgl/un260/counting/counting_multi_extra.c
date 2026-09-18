#include "counting_multi_extra.h"
#include "un260/protocol/protocol_send.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static multi_serial_cache_t serial[COUNTING_MULTI_MAX];
static multi_reject_cache_t rejects;
static uint32_t generation, revision;
static struct {
    uint8_t cmd;
    unsigned index;
    uint32_t generation, sent, activity;
    bool started, invalid, expired, abandoned;
    uint16_t count;
    multi_serial_t rows[MULTI_SERIAL_MAX];
    multi_reject_t errors[254];
} pending;

static void sync_generation(void)
{
    uint32_t next=counting_multi_current()->generation;
    if(generation==next)return;
    for(unsigned i=0;i<COUNTING_MULTI_MAX;i++){free(serial[i].rows);memset(&serial[i],0,sizeof(serial[i]));}
    memset(&rejects,0,sizeof(rejects));
    if(pending.cmd)pending.abandoned=true;
    generation=next;revision++;
}
const multi_serial_cache_t *counting_multi_serials(unsigned i)
{sync_generation();return i<counting_multi_current()->count?&serial[i]:NULL;}
const multi_reject_cache_t *counting_multi_rejects(void){sync_generation();return &rejects;}
uint32_t counting_multi_extra_revision(void){sync_generation();return revision;}
bool counting_multi_extra_busy(void){sync_generation();return pending.cmd!=0;}
static multi_detail_status_t *status(void)
{return pending.cmd==0x0d?&serial[pending.index].status:&rejects.status;}
static bool request(unsigned index,uint8_t cmd,uint32_t now)
{
    sync_generation();
    const counting_multi_t *m=counting_multi_current();
    if(m->counting||pending.cmd||counting_multi_query_busy()||
       (cmd==0x0d&&index>=m->count))return false;
    memset(&pending,0,sizeof(pending));pending.cmd=cmd;pending.index=index;
    pending.generation=generation;pending.sent=pending.activity=now;
    uint8_t bytes[4]={0};uint16_t len=1;
    if(cmd==0x0d){memcpy(bytes+1,m->currencies[index].code,3);len=4;}
    else bytes[0]=1;
    if(protocol_send(cmd,bytes,len)<0){*status()=MULTI_DETAIL_INVALID;pending.cmd=0;revision++;return false;}
    *status()=MULTI_DETAIL_LOADING;revision++;return true;
}
bool counting_multi_serial_request(unsigned i,uint32_t now){return request(i,0x0d,now);}
bool counting_multi_reject_request(uint32_t now){return request(0,0x0c,now);}
static bool uniform(const uint8_t *f,unsigned len,uint8_t value)
{for(unsigned i=4;i+1<len;i++)if(f[i]!=value)return false;return true;}
bool counting_multi_extra_reply(uint8_t cmd,const uint8_t *f,uint8_t len,uint32_t now)
{
    sync_generation();
    if(!pending.cmd||cmd!=pending.cmd)return false;
    if(!f||(cmd==0x0d?len!=25:len!=7)||f[3]!=cmd)return true;
    pending.activity=now;
    if(uniform(f,len,255)){
        if(!pending.abandoned&&pending.generation==generation){
            multi_detail_status_t result=pending.expired?MULTI_DETAIL_TIMEOUT:
                (!pending.started||pending.invalid)?MULTI_DETAIL_INVALID:
                pending.count?MULTI_DETAIL_READY:MULTI_DETAIL_EMPTY;
            if(cmd==0x0d && result==MULTI_DETAIL_READY){
                multi_serial_t *rows=malloc(pending.count*sizeof(*rows));
                if(!rows)result=MULTI_DETAIL_INVALID;
                else{memcpy(rows,pending.rows,pending.count*sizeof(*rows));
                    free(serial[pending.index].rows);serial[pending.index].rows=rows;
                    serial[pending.index].count=pending.count;}
            }else if(cmd==0x0c && result==MULTI_DETAIL_READY){
                memcpy(rejects.rows,pending.errors,pending.count*sizeof(*rejects.rows));rejects.count=pending.count;
            }
            if(result==MULTI_DETAIL_EMPTY){if(cmd==0x0d)serial[pending.index].count=0;else rejects.count=0;}
            *status()=result;revision++;
        }
        memset(&pending,0,sizeof(pending));return true;
    }
    if(pending.abandoned||pending.expired)return true;
    if(uniform(f,len,0)){if(pending.count)pending.invalid=true;pending.started=true;return true;}
    if(!pending.started){pending.invalid=true;return true;}
    if(cmd==0x0c){
        for(unsigned i=0;i<pending.count;i++)if(pending.errors[i].code==f[4]){
            if(pending.errors[i].pcs!=f[5])pending.invalid=true;return true;}
        if(pending.count>=254){pending.invalid=true;return true;}
        pending.errors[pending.count++]=(multi_reject_t){f[4],f[5]};return true;
    }
    unsigned no=f[4];if(!no||no==255){pending.invalid=true;return true;}
    multi_serial_t row={0};row.number=no;
    bool digits=false,ended=false;
    for(unsigned i=5;i<12;i++){
        if(f[i]>='0'&&f[i]<='9'){if(ended){pending.invalid=true;return true;}row.value=row.value*10+f[i]-'0';digits=true;}
        else if(f[i]==' '){if(digits)ended=true;}else{pending.invalid=true;return true;}
    }
    unsigned start=12,end=24;while(start<end&&f[start]==' ')start++;while(end>start&&f[end-1]==' ')end--;
    if(!digits||start==end){pending.invalid=true;return true;}
    for(unsigned i=start;i<end;i++)if(f[i]<32||f[i]>126){pending.invalid=true;return true;}
    memcpy(row.text,f+start,end-start);
    for(unsigned i=0;i<pending.count;i++)if(pending.rows[i].number==no){
        if(pending.rows[i].value!=row.value||strcmp(pending.rows[i].text,row.text))pending.invalid=true;return true;}
    if(pending.count==MULTI_SERIAL_MAX){pending.invalid=true;return true;}
    unsigned pos=pending.count++;
    while(pos&&pending.rows[pos-1].number>no){pending.rows[pos]=pending.rows[pos-1];pos--;}
    pending.rows[pos]=row;return true;
}
void counting_multi_extra_poll(uint32_t now)
{
    sync_generation();if(!pending.cmd||pending.expired)return;
    if((int32_t)(now-pending.activity)<2500&&(int32_t)(now-pending.sent)<30000)return;
    pending.expired=true;if(!pending.abandoned){*status()=MULTI_DETAIL_TIMEOUT;revision++;}
}
