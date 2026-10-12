#include "history_read_task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned slot,kind;size_t count;history_query_t query;ui_history_record_t records[]; } read_input_t;
static struct {storage_job_id_t id;atomic_bool cancelled;history_read_result_t result;} reads[STORAGE_WORKER_CAPACITY];
static void free_result(history_read_result_t *r)
{if(r->detail){history_record_detail_release(r->detail);free(r->detail);}memset(r,0,sizeof(*r));}
static bool run_read(const void *snapshot,size_t size)
{
    const read_input_t *in=snapshot;
    if(size<sizeof(*in) || in->slot>=STORAGE_WORKER_CAPACITY || in->count>UI_HISTORY_MAX_RECORDS ||
       size!=sizeof(*in)+in->count*sizeof(in->records[0]))return true;
    history_read_result_t *r=&reads[in->slot].result;r->count=in->count;r->ok=true;
    for(size_t i=0;i<in->count;i++){
        if(atomic_load(&reads[in->slot].cancelled)){r->ok=false;break;}
        const ui_history_record_t *rec=&in->records[i];
        history_record_detail_t *d=calloc(1,sizeof(*d));
        bool valid=d && ui_history_record_read_detail(rec,d);
        if(in->kind==0){if(valid)r->detail=d;else {if(d)free(d);r->ok=false;}break;}
        history_query_record_t row={.record_no=rec->record_no,.pcs=rec->pcs,.amount=rec->detail_bytes?rec->detail_amount:rec->amount,
            .year=rec->year,.month=rec->month,.day=rec->day,.hour=rec->hour,.minute=rec->minute,.second=rec->second,
            .valid=rec->valid,.detail=valid?d:NULL,.multi=rec->multi.enabled?&rec->multi:NULL};
        memcpy(row.currency,rec->currency,4);r->ids[i]=rec->record_no;
        r->matches[i]=(uint8_t)history_query_match_record(&in->query,&row);
        if(d){history_record_detail_release(d);free(d);}
    }
    return true; /* A read error is a completed result, not a failed write job. */
}
static storage_job_id_t request(const ui_history_record_t *records,size_t count,const history_query_t *query)
{
    history_read_poll_discarded();
    if(!records || !count || count>UI_HISTORY_MAX_RECORDS || !storage_worker_init())return 0;
    unsigned slot=0;while(slot<STORAGE_WORKER_CAPACITY && reads[slot].id)slot++;
    if(slot==STORAGE_WORKER_CAPACITY)return 0;
    size_t size=sizeof(read_input_t)+count*sizeof(*records);
    if(size>STORAGE_WORKER_MAX_JOB_BYTES)return 0;
    read_input_t *in=calloc(1,size);if(!in)return 0;
    in->slot=slot;in->kind=query?1:0;in->count=count;if(query)in->query=*query;
    memcpy(in->records,records,count*sizeof(*records));atomic_store(&reads[slot].cancelled,false);
    storage_job_id_t id=0;bool accepted=storage_worker_submit(run_read,in,size,&id);free(in);
    if(accepted)reads[slot].id=id;
    return id;
}
storage_job_id_t history_read_detail_request(const ui_history_record_t *record){return request(record,1,NULL);}
storage_job_id_t history_read_query_request(const ui_history_record_t *records,size_t count,const history_query_t *query)
{return query?request(records,count,query):0;}
bool history_read_take(storage_job_id_t id,history_read_result_t *out)
{
    if(!id || !out || storage_worker_status(id)!=STORAGE_JOB_SUCCEEDED)return false;
    for(unsigned i=0;i<STORAGE_WORKER_CAPACITY;i++)if(reads[i].id==id){
        *out=reads[i].result;memset(&reads[i].result,0,sizeof(reads[i].result));
        storage_worker_release(id);reads[i].id=0;return true;
    }
    return false;
}
void history_read_cancel(storage_job_id_t id)
{
    for(unsigned i=0;i<STORAGE_WORKER_CAPACITY;i++)if(id && reads[i].id==id)atomic_store(&reads[i].cancelled,true);
    history_read_poll_discarded();
}
void history_read_poll_discarded(void)
{
    for(unsigned i=0;i<STORAGE_WORKER_CAPACITY;i++)if(reads[i].id && atomic_load(&reads[i].cancelled) &&
        storage_worker_status(reads[i].id)==STORAGE_JOB_SUCCEEDED){
        free_result(&reads[i].result);storage_worker_release(reads[i].id);reads[i].id=0;
    }
}
