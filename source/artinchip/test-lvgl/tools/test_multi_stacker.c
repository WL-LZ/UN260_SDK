#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_history_service.h"
#include "un260/lv_system/ui_history_data.h"
static ui_history_store_t store={.next_record_no=1};
static unsigned jobs,appends,updates;
static bool available=true;
static counting_session_state_t session;
static counting_sim_t sim={.multi_currency_result=true};
int protocol_send(uint8_t c,const uint8_t*p,uint16_t n){(void)c;(void)p;return n;}
bool ui_history_data_can_accept(void){return available;}
const ui_history_store_t *ui_history_data_get(void){return &store;}
uint32_t ui_history_total_notes_counted_get(void){return store.total_notes_counted;}
void ui_history_total_notes_counted_set(uint32_t n){store.total_notes_counted=n;}
storage_job_id_t ui_history_last_commit_id(void){return jobs;}
storage_job_status_t ui_history_commit_status(storage_job_id_t id){assert(id);return STORAGE_JOB_SUCCEEDED;}
storage_job_status_t ui_history_data_status(void){return available?STORAGE_JOB_SUCCEEDED:STORAGE_JOB_FAILED;}
bool ui_history_record_append_snapshot(const ui_history_record_t*r,uint32_t total){
 assert(available);ui_history_record_t*d=&store.records[store.record_count++];*d=*r;d->record_no=store.next_record_no++;
 store.total_notes_counted=total;appends++;jobs++;return true;
}
bool ui_history_record_update_snapshot(const ui_history_record_t*r,uint32_t total){
 assert(available);for(unsigned i=0;i<store.record_count;i++)if(store.records[i].record_no==r->record_no){
 store.records[i]=*r;store.total_notes_counted=total;updates++;jobs++;return true;}assert(0);return false;
}
bool ui_history_record_build_multi_base(const counting_sim_t*s,uint32_t p,const char*e,const char*a,const char*b,const char*l,ui_history_record_t*r){
 (void)s;(void)e;(void)a;(void)b;(void)l;memset(r,0,sizeof(*r));r->valid=true;r->pcs=p;return true;
}
bool ui_history_record_build_from_session(const counting_sim_t*s,uint32_t p,float m,const char*e,const char*a,const char*b,const char*l,ui_history_record_t*r){
 (void)m;return ui_history_record_build_multi_base(s,p,e,a,b,l,r);
}
static void live(const char*c,unsigned amount,unsigned pcs){uint8_t f[16]={0xfd,0xdf,16,14};memcpy(f+4,c,3);f[7]=amount>>24;f[8]=amount>>16;f[9]=amount>>8;f[10]=amount;f[11]=pcs>>8;f[12]=pcs;f[14]=1;assert(counting_multi_info(f,16));}
static void global(unsigned pcs,unsigned status){uint8_t f[13]={0xfd,0xdf,13,14};f[8]=pcs>>8;f[9]=pcs;f[11]=status;assert(counting_multi_info(f,13));}
static void start(bool add){assert(counting_history_prepare_start(&session,&sim,0));counting_multi_begin(add);global(0,0);}
static void finish(unsigned pcs){global(pcs,1);global(0,2);counting_history_try_commit(&session,&sim,0);counting_history_poll_commit(&session,&sim,1);}
int main(void){
 const counting_multi_t*m=counting_multi_current();
 start(false);live("CNY",65,13);finish(13);assert(appends==1&&store.total_notes_counted==13);
 uint32_t group=m->group_generation,id=store.records[0].record_no;
 start(false);assert(m->count==1&&m->total_pcs==13&&m->group_generation==group);live("USD",425,15);live("USD",425,15);
 assert(m->count==2&&m->total_pcs==28&&m->currencies[0].amount==65);finish(28);
 assert(appends==1&&updates==1&&store.record_count==1&&store.records[0].record_no==id);
 assert(store.records[0].pcs==28&&store.records[0].multi.count==2&&store.records[0].multi.passes==2&&store.total_notes_counted==28);
 counting_history_try_commit(&session,&sim,2);assert(updates==1);
 start(true);live("CNY",75,15);finish(30);assert(m->currencies[0].pcs==15&&m->currencies[0].amount==75);
 assert(appends==1&&store.total_notes_counted==30&&store.records[0].multi.passes==3);
 /* Removed: preserve prior history, start a separate group even with ADD on. */
 assert(counting_history_prepare_reset(&session,&sim,0));counting_multi_reset();start(true);live("USD",10,2);finish(2);
 assert(appends==2&&store.record_count==2&&store.records[0].pcs==30&&store.records[1].pcs==2&&store.total_notes_counted==32);
 /* Storage unavailable: completed result remains in bounded snapshot spool. */
 start(false);live("CNY",5,1);available=false;finish(3);assert(!counting_history_can_start());
 assert(store.record_count==2&&store.records[1].pcs==2);available=true;
 counting_history_poll_commit(&session,&sim,4);counting_history_poll_commit(&session,&sim,5);
 assert(store.record_count==2&&store.records[1].pcs==3&&store.total_notes_counted==33);
 puts("PASS: CNY13 + USD15 without ADD = one 28-PCS history; overwrite not add; ADD toggle; explicit batch boundary; duplicate frames; storage retry");
}
