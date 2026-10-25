#define main history_storage_base_main
#include "test_history_storage.c"
#undef main
static counting_session_state_t group_session;
static counting_sim_t group_data;
static storage_job_id_t checkpoint(unsigned pcs,bool add,bool settle) {
    counting_history_begin_group(add);counting_history_session_start(NULL,0);
    counting_data_clear_serials(&group_data);counting_data_clear_errors(&group_data);
    group_data.total_pcs=pcs;group_data.total_amount=pcs*50;
    group_data.denom_number=1;group_data.denom[0]=(denom_t){50,pcs,pcs*50};
    assert(counting_data_ensure_serial_capacity(&group_data,pcs));
    for(unsigned i=0;i<pcs;++i){group_data.sn_str[i]=strdup("GROUP0000001");assert(group_data.sn_str[i]);group_data.denom_mix[i]=50;}
    uint8_t zero[13]={0xFD,0xDF,13,0x0E};counting_history_append_frame("0x0E",zero,13);
    group_session.history_record=(counting_pending_history_record_t){.valid=true,.end_seen=true,.pcs=pcs,.amount=pcs*50};
    assert(counting_history_try_commit(&group_session,&group_data,0)==COUNTING_HISTORY_COMMIT_PENDING);
    storage_job_id_t id=ui_history_last_commit_id();
    if(settle){wait_saved(id,0);counting_history_poll_commit(&group_session,&group_data,0);}
    return id;
}
static void check(unsigned count,unsigned pcs,unsigned lifetime,uint32_t record) {
    const ui_history_store_t *s=ui_history_data_get();
    assert(s->record_count==count && s->total_notes_counted==lifetime);
    const ui_history_record_t *r=&s->records[0];
    assert(r->pcs==pcs && r->amount==pcs*50 && (!record || r->record_no==record));
    history_record_detail_t d={0};assert(ui_history_record_read_detail(r,&d));
    assert(d.serial_count==pcs && d.denomination_pcs==pcs && d.denomination_amount==pcs*50);
    assert(d.denoms_complete && d.serials_complete);
    for(unsigned i=0;i<pcs;++i)assert(d.serials[i].no==i+1 && d.serials[i].denom==50);
    history_record_detail_release(&d);
}
int main(int argc,char **argv) {
    assert(argc==2);ui_thread=pthread_self();ui_history_data_init();assert(ui_history_data_is_available());
    if(!strcmp(argv[1],"read")) {check(5,166,339,0);assert(storage_worker_shutdown());puts("PASS fresh-process grouped history and full details");return 0;}
    checkpoint(38,true,true);uint32_t first=ui_history_data_get()->records[0].record_no;check(1,38,38,first);
    operator_fixture.users[0].id=99;strcpy(operator_fixture.users[0].name,"Other");
    atomic_store(&fault,FAIL_OPEN);storage_job_id_t failed=checkpoint(75,true,false);
    assert(storage_worker_wait(failed)==STORAGE_JOB_FAILED);
    atomic_store(&fault,FAIL_NONE);assert(storage_worker_retry(failed));wait_saved(failed,0);
    counting_history_poll_commit(&group_session,&group_data,0);check(1,75,75,first);
    assert(ui_history_data_get()->records[0].operator_id==42);
    checkpoint(75,true,true);check(1,75,75,first);
    hold_worker(true);storage_job_id_t pending=checkpoint(80,true,false);
    counting_data_clear_serials(&group_data);group_data.denom[0].pcs=0;
    hold_worker(false);wait_saved(pending,0);counting_history_poll_commit(&group_session,&group_data,0);check(1,80,80,first);
    counting_history_current_invalidate();checkpoint(2,true,true);check(2,2,82,0);
    uint32_t deleted=ui_history_data_get()->records[0].record_no;
    assert(ui_history_record_delete_records(&deleted,1));wait_saved(ui_history_last_commit_id(),0);
    checkpoint(3,true,true);check(1,80,83,first); /* Deleted group stays deleted. */
    checkpoint(3,false,true);check(2,3,86,0);
    checkpoint(4,false,true);check(3,4,90,0);
    counting_history_current_invalidate();checkpoint(83,true,true);
    uint32_t old_group=ui_history_data_get()->records[0].record_no;
    check(4,83,173,old_group);
    counting_history_restart_group();checkpoint(83,true,true);
    uint32_t new_group=ui_history_data_get()->records[0].record_no;
    assert(new_group!=old_group);check(5,83,256,new_group);
    assert(ui_history_data_get()->records[1].record_no==old_group&&ui_history_data_get()->records[1].pcs==83);
    checkpoint(166,true,true);check(5,166,339,new_group);
    assert(ui_history_data_get()->records[1].record_no==old_group&&ui_history_data_get()->records[1].pcs==83);
    counting_data_clear_serials(&group_data);counting_data_clear_errors(&group_data);
    assert(storage_worker_shutdown());
    puts("PASS ADD 38->75 updates one full record, only +37 lifetime; empty pass, save failure/retry, immutable async snapshot, clear/new group, delete wins, non-ADD independent");
    return 0;
}
