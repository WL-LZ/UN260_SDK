/* Actual counting history -> storage thread -> fresh-process reader -> query
 * and CSV/HTML exporter. Reuse existing clock/USB/notice test boundaries. */
#define main history_storage_base_main
#include "test_history_storage.c"
#undef main
#include "test_notice_sink.h"
#include "un260/lv_system/ui_history_export_data.c"
#include "un260/counting/counting_info_reply.h"
static lv_timer_t export_timer;
lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),uint32_t period,void *data)
{(void)period;(void)data;export_timer.callback=cb;return &export_timer;}
void lv_timer_del(lv_timer_t *t){(void)t;}
void lv_timer_set_repeat_count(lv_timer_t *t,int32_t n){(void)t;(void)n;}
bool usb_storage_prepare(void){return true;}
bool usb_storage_make_unique_file_pair(const char *base,const char *a,char *pa,size_t na,const char *b,char *pb,size_t nb)
{snprintf(pa,na,UI_HISTORY_STORE_DIR "/%s%s",base,a);snprintf(pb,nb,UI_HISTORY_STORE_DIR "/%s%s",base,b);return true;}
bool usb_storage_commit_file_pair(const char *a,const char *fa,const char *b,const char *fb)
{return rename(a,fa)==0 && rename(b,fb)==0;}
static void verify_controller_a(const ui_history_record_t *rec)
{
    assert(rec->pcs==2 && rec->amount==20 && rec->detail_amount==20);
    history_record_detail_t d={0};assert(ui_history_record_read_detail(rec,&d));
    assert(d.denoms_complete && d.serials_complete && d.rejects_complete);
    assert(d.denom_count==1 && d.denoms[0].value==10 && d.denoms[0].pcs==2);
    assert(d.denomination_pcs==2 && d.denomination_amount==20);
    assert(d.serial_count==2 && d.serials[0].denom==50 && d.serials[1].denom==50);
    assert(!strcmp(d.serials[0].sn,"A-AUTH-00001") && !strcmp(d.serials[1].sn,"A-AUTH-00002"));
    history_record_detail_release(&d);
}
static void controller_a_is_not_inferred(void)
{
    char *serials[]={"A-AUTH-00001","A-AUTH-00002"};
    counting_sim_t sim={.denom_number=1,.denom={{10,2,20}},.sn_capacity=2,.sn_str=serials};
    sim.denom_mix[0]=sim.denom_mix[1]=50;
    for(unsigned reported=0;reported<=2;reported++){
        sim.denom[0].pcs=reported;
        history_record_snapshot_t snapshot={0};
        assert(history_record_snapshot_capture(&sim,2,true,&snapshot));
        history_record_detail_t detail={0};
        assert(history_record_snapshot_decode(snapshot.bytes,snapshot.size,snapshot.hash,2,&detail));
        assert(detail.denom_count==1 && detail.denoms[0].value==10 && detail.denoms[0].pcs==reported);
        assert(detail.denomination_amount==(uint64_t)reported*10);
        assert(detail.denoms_complete==(reported==2));
        assert(detail.serials_complete && detail.serials[0].denom==50 && detail.serials[1].denom==50);
        history_record_detail_release(&detail);history_record_snapshot_release(&snapshot);
    }
    puts("PASS controller A stays authoritative over conflicting complete B, including partial/missing counts");
}
static void verify_record(const ui_history_record_t *rec)
{
    if(rec->pcs==2){verify_controller_a(rec);return;}
    history_record_detail_t d={0};assert(ui_history_record_read_detail(rec,&d));
    assert(d.denomination_pcs==rec->pcs && d.denomination_amount==rec->detail_amount);
    assert(d.serial_count==rec->pcs && d.denoms_complete && d.serials_complete && d.rejects_complete);
    unsigned expected=rec->pcs?300:1;
    assert(d.reject_count==expected && d.saved_reject_pcs==(rec->pcs?300:5));
    for(size_t i=0;i<d.serial_count;i++){
        assert(d.serials[i].no==i+1 && d.serials[i].denom==(rec->pcs==10000?1000000:10*(i%15+1)));
        if(i==2)assert(!*d.serials[i].sn); /* Reported empty slot is retained. */
        else assert(!strcmp(d.serials[i].sn,i%2?"001122334455":"000000000001"));
    }
    for(size_t i=0;i<d.reject_count;i++){
        const history_detail_reject_t *r=history_record_detail_reject_at(&d,i);
        assert(r && r->code==(rec->pcs?i%49+1:7) && r->pcs==(rec->pcs?1:5));
    }
    history_record_detail_release(&d);
    storage_job_id_t job=history_read_detail_request(rec);assert(job);
    assert(storage_worker_wait(job)==STORAGE_JOB_SUCCEEDED);
    history_read_result_t result={0};assert(history_read_take(job,&result) && result.ok);
    assert(result.detail->serial_count==rec->pcs);history_record_detail_release(result.detail);free(result.detail);
    history_query_input_t input={0};strcpy(input.serial,"000000000001");strcpy(input.currency,"USD");input.serial_match=HISTORY_TEXT_EXACT;
    if(rec->pcs==10000)strcpy(input.amount,"10000000000");
    input.denominations[0]=rec->pcs==10000?1000000:10;input.denomination_count=1;
    history_query_t query;assert(history_query_compile(&input,&query)==HISTORY_QUERY_OK);
    job=history_read_query_request(rec,1,&query);assert(job);assert(storage_worker_wait(job)==STORAGE_JOB_SUCCEEDED);
    assert(history_read_take(job,&result) && result.ok && result.matches[0]==(rec->pcs?HISTORY_QUERY_MATCH:HISTORY_QUERY_NO_MATCH));
    uint32_t id=rec->record_no;
    assert(ui_history_export_data_request_records(&id,1));
    assert(storage_worker_wait(g_history_export_job)==STORAGE_JOB_SUCCEEDED);ui_history_export_data_poll();assert(g_history_export_succeeded);
    char base[160],path[256],line[4096];history_export_build_name_for_record(base,sizeof(base),rec);
    snprintf(path,sizeof(path),UI_HISTORY_STORE_DIR "/%s.csv",base);FILE *fp=fopen(path,"r");assert(fp);
    unsigned serials=0,rejects=0;bool b=false,c=false,money=rec->pcs!=10000;
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"Total Amount,10000000000"))money=true;
        if(!strcmp(line,"SERIAL NUMBER LIST\n")){b=true;c=false;continue;}
        if(!strcmp(line,"REJECT REPORT\n")){b=false;c=true;continue;}
        unsigned no;if(sscanf(line,"%u,",&no)==1){if(b)serials++;if(c)rejects++;}
    }
    fclose(fp);assert(serials==rec->pcs && rejects==expected && money);
    snprintf(path,sizeof(path),UI_HISTORY_STORE_DIR "/%s.html",base);fp=fopen(path,"r");assert(fp);
    unsigned rows=0,events=0;bool complete=false;money=rec->pcs!=10000;
    while(fgets(line,sizeof(line),fp)){
        if(strstr(line,"data-target=\"10000000000\""))money=true;
        const char *p=line;while((p=strstr(p,"data-sn="))){rows++;p+=8;}
        p=line;while((p=strstr(p,"{no:"))){events++;p+=4;}
        if(strstr(line,"aComplete:true,bComplete:true,cComplete:true"))complete=true;
    }
    fclose(fp);assert(rows==rec->pcs && events==expected && complete && money);
    history_export_unlock_timer_cb(&export_timer);
}
int main(int argc,char **argv)
{
    assert(argc==2);ui_thread=pthread_self();ui_history_data_init();assert(ui_history_data_is_available());
    if(!strcmp(argv[1],"read")){
        assert(ui_history_data_get()->record_count==8);
        for(unsigned i=0;i<8;i++)verify_record(&ui_history_data_get()->records[i]);
        assert(storage_worker_shutdown());puts("PASS fresh process: complete ABC, async detail/query and CSV/HTML exact row counts");return 0;
    }
    const unsigned sizes[]={1,150,256,257,1000,10000};
    for(unsigned round=0;round<6;round++){
        counting_sim_t *sim=calloc(1,sizeof(*sim));assert(sim);
        assert(counting_data_ensure_serial_capacity(sim,sizes[round]));assert(counting_data_ensure_error_capacity(sim,300));
        sim->denom_number=15;
        for(unsigned i=0;i<15;i++)sim->denom[i].value=10*(i+1);
        uint64_t amount=0;
        for(unsigned i=0;i<sizes[round];i++){
            sim->sn_str[i]=strdup(i==2?"":i%2?"001122334455":"000000000001");assert(sim->sn_str[i]);
            sim->denom_mix[i]=round==5?1000000:10*(i%15+1);sim->denom[i%15].pcs++;amount+=sim->denom_mix[i];
        }
        /* The controller owns this exact large A table; B is independent. */
        if(round==5){sim->denom_number=1;sim->denom[0].value=1000000;sim->denom[0].pcs=10000;}
        sim->err_num=300;sim->err_expected=300;
        for(unsigned i=0;i<300;i++){sim->err_code[i]=i%49+1;sim->err_pcs[i]=1;}
        sim->total_pcs=sizes[round];sim->total_amount=(double)amount;
        counting_session_state_t session={0};counting_history_session_start(NULL,0);
        /* Saturate the optional diagnostic log before final reject details. */
        uint8_t live[]={0xFD,0xDF,13,0x0E,0,0,0,0,0,1,0,1,0x0A};
        for(unsigned i=0;i<300;i++)counting_history_append_frame("0x0E",live,sizeof(live));
        uint8_t begin[]={0xFD,0xDF,7,0x0C,0,0,0x0A},end[]={0xFD,0xDF,7,0x0C,255,255,0x0A};
        counting_history_append_frame("0x0C",begin,7);counting_history_append_frame("0x0C",end,7);
        session.history_record.valid=true;session.history_record.end_seen=true;session.history_record.pcs=sizes[round];session.history_record.amount=(double)amount;
        assert(counting_history_try_commit(&session,sim,0)==COUNTING_HISTORY_COMMIT_PENDING);
        counting_data_clear_serials(sim);counting_data_clear_errors(sim);free(sim);
        storage_job_id_t id=ui_history_last_commit_id();wait_saved(id,0);
        (void)counting_history_poll_commit(&session,NULL,0);
        verify_record(&ui_history_data_get()->records[0]);
    }
    controller_a_is_not_inferred();
    /* Persist conflicting but equally complete A/B, then verify this same
     * record again in the separate reader process. Never infer A from B. */
    counting_sim_t authoritative={.denom_number=1,.denom={{10,2,20}},.total_pcs=2,.total_amount=20};
    assert(counting_data_ensure_serial_capacity(&authoritative,2));
    authoritative.sn_str[0]=strdup("A-AUTH-00001");authoritative.sn_str[1]=strdup("A-AUTH-00002");
    assert(authoritative.sn_str[0] && authoritative.sn_str[1]);
    authoritative.denom_mix[0]=authoritative.denom_mix[1]=50;
    counting_session_state_t authoritative_session={0};counting_history_session_start(NULL,0);
    uint8_t a_begin[]={0xFD,0xDF,7,0x0C,0,0,0x0A},a_end[]={0xFD,0xDF,7,0x0C,255,255,0x0A};
    counting_history_append_frame("0x0C",a_begin,7);counting_history_append_frame("0x0C",a_end,7);
    authoritative_session.history_record.valid=true;authoritative_session.history_record.end_seen=true;
    authoritative_session.history_record.pcs=2;authoritative_session.history_record.amount=20;
    assert(counting_history_try_commit(&authoritative_session,&authoritative,0)==COUNTING_HISTORY_COMMIT_PENDING);
    counting_data_clear_serials(&authoritative);
    wait_saved(ui_history_last_commit_id(),0);(void)counting_history_poll_commit(&authoritative_session,NULL,0);
    verify_controller_a(&ui_history_data_get()->records[0]);
    /* Genuine reject-only END is history, while no-note motor spins are not. */
    counting_sim_t pure={0};counting_session_state_t session={.start_confirmed=true,.phase=COUNTING_SESSION_ACTIVE};
    uint8_t final[]={0xFD,0xDF,13,0x0E,0,0,0,0,0,0,5,2,0x0A};
    counting_info_reply_result_t reply=counting_info_reply_handle(&session,&pure,final,13,ui_history_total_notes_counted_get());
    assert(reply.kind==COUNTING_INFO_REPLY_FINISHED && session.history_record.valid && !session.history_record.pcs);
    uint8_t code=7,pcs=5;pure.err_code=&code;pure.err_pcs=&pcs;pure.err_capacity=1;pure.err_num=1;
    ui_history_record_t rec;assert(ui_history_record_build_from_session(&pure,0,0,"","","","",&rec));
    history_record_snapshot_t snapshot={0};assert(history_record_snapshot_capture(&pure,0,true,&snapshot));
    uint32_t total=ui_history_total_notes_counted_get();unsigned count=ui_history_data_get()->record_count;
    atomic_store(&fail_detail_open,true);
    assert(ui_history_record_append_full_snapshot(&rec,total,&snapshot));storage_job_id_t job=ui_history_last_commit_id();
    assert(storage_worker_wait(job)==STORAGE_JOB_FAILED);ui_history_data_poll(0);
    assert(ui_history_data_get()->record_count==count && ui_history_total_notes_counted_get()==total);
    atomic_store(&fail_detail_open,false);ui_history_data_poll(1000);wait_saved(job,1000);
    assert(ui_history_total_notes_counted_get()==total);verify_record(&ui_history_data_get()->records[0]);
    history_record_snapshot_release(&snapshot);
    /* Full-file corruption cannot silently fall back to a legacy preview. */
    rec=ui_history_data_get()->records[1];char path[256],backup[272];
    snprintf(path,sizeof(path),UI_HISTORY_STORE_DIR "/detail-%u-%016" PRIx64 ".abc",rec.record_no,rec.detail_hash);
    FILE *fp=fopen(path,"rb");assert(fp);unsigned char *bytes=malloc(rec.detail_bytes);assert(bytes);
    assert(fread(bytes,1,rec.detail_bytes,fp)==rec.detail_bytes);fclose(fp);
    fp=fopen(path,"r+b");assert(fp);assert(fputc('!',fp)!=EOF);fclose(fp);
    history_record_detail_t detail={0};assert(!ui_history_record_read_detail(&rec,&detail));
    uint32_t bad_id=rec.record_no;assert(ui_history_export_data_request_records(&bad_id,1));
    assert(storage_worker_wait(g_history_export_job)==STORAGE_JOB_SUCCEEDED);ui_history_export_data_poll();assert(!g_history_export_succeeded);
    history_export_unlock_timer_cb(&export_timer);
    fp=fopen(path,"wb");assert(fp);assert(fwrite(bytes,1,rec.detail_bytes,fp)==rec.detail_bytes);fclose(fp);free(bytes);
    snprintf(backup,sizeof(backup),"%s.saved",path);assert(!rename(path,backup));
    assert(!ui_history_record_read_detail(&rec,&detail));assert(!rename(backup,path));
    history_query_input_t input={0};strcpy(input.serial,"000000000001");history_query_t query;
    assert(history_query_compile(&input,&query)==HISTORY_QUERY_OK);
    job=history_read_query_request(&rec,1,&query);assert(job);history_read_cancel(job);
    for(unsigned i=0;i<5000 && storage_worker_pending_status()==STORAGE_JOB_PENDING;i++)usleep(1000);
    history_read_poll_discarded();
    assert(storage_worker_shutdown());puts("PASS production counting spool: 1/150/256/257/1000/10000 SN, 15 denominations, 300 C rows, duplicate/empty SN, saturated log and destroyed input; reject-only, failed detail write/retry, corrupt/missing files and cancelled query");
}
