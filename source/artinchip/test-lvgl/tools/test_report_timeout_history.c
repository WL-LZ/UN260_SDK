/* Included after the established real-storage fixture by the Python runner. */
static void assert_current_a(unsigned ten, unsigned fifty)
{
 unsigned a=0,b=0;
 for(unsigned i=0;i<note_data.denom_number;i++){
  if(note_data.denom[i].value==10)a=note_data.denom[i].pcs;
  if(note_data.denom[i].value==50)b=note_data.denom[i].pcs;
 }
 assert(a==ten&&b==fifty);
}
static void verify_store(bool add)
{
 const ui_history_store_t *store=ui_history_data_get();
 assert(store->record_count==(add?1u:2u)&&store->total_notes_counted==3);
 check_record(0,store->records[0].record_no,1,add?2:0,add?"OLDLIVE00001":"NEWLIVE00001");
 if(!add)check_record(1,store->records[1].record_no,0,2,"OLDLIVE00001");
 for(unsigned i=0;i<store->record_count;i++){
  history_record_detail_t detail={0};assert(ui_history_record_read_detail(&store->records[i],&detail));
  for(unsigned n=0;n<detail.serial_count;n++){
   assert(strncmp(detail.serials[n].sn,"OLDPART",7));
   assert(strncmp(detail.serials[n].sn,"STALE",5));
  }
  history_record_detail_release(&detail);
 }
}
static void stale_serials(bool include_end)
{
 serial_row(2,50,"STALE0000001");
 if(include_end)marker(0x0D,255);
}
int main(int argc,char **argv)
{
 assert(argc==4);bool add=atoi(argv[2])!=0,tail=atoi(argv[3])!=0;
 ui_thread=pthread_self();ui_history_data_init();assert(ui_history_data_is_available());
 if(!strcmp(argv[1],"read")){
  verify_store(add);assert(storage_worker_shutdown());
  printf("PASS fresh-disk history add=%d tail=%d\n",add,tail);return 0;
 }
 note_data.denom_number=2;note_data.denom[0].value=10;note_data.denom[1].value=50;
 begin(add);denom_boundary(0);denom_row(50,2);denom_row(10,0);
 note(50,"OLDLIVE00001");note(50,"OLDLIVE00002");info(100,2,1);info(0,0,2);denom_boundary(255);
 assert(sent[0x0C]==1&&sent[0x0D]==0);
 marker(0x0C,0);marker(0x0C,255);assert(sent[0x0D]==1);
 marker(0x0D,0);serial_row(1,50,"OLDPART00001");
 assert(note_data.sn_str[0]&&!strcmp(note_data.sn_str[0],"OLDLIVE00001"));
 assert(!note_session.history_record.end_seen);assert_current_a(0,2);
 clock_tick+=3001;counting_report_poll(&note_session,clock_tick);
 assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_SERIAL);
 assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_NONE);
 for(unsigned i=0;i<3;i++)counting_report_poll(&note_session,++clock_tick);
 assert(sent[0x0C]==1&&sent[0x0D]==1);assert_current_a(0,2);
 if(tail)stale_serials(true);
 assert(note_data.sn_str[0]&&!strcmp(note_data.sn_str[0],"OLDLIVE00001"));
 /* Unsolicited old serials have no query owner after timeout. */
 marker(0x0D,0);stale_serials(tail);assert_current_a(0,2);
 begin(add);storage_job_id_t previous=ui_history_last_commit_id();assert(previous);
 wait_saved(previous,app_clock_uptime_ms());counting_history_poll_commit(&note_session,&note_data,app_clock_uptime_ms());
 const ui_history_store_t *first=ui_history_data_get();assert(first->record_count==1);
 check_record(0,first->records[0].record_no,0,2,"OLDLIVE00001");
 denom_boundary(0);denom_row(10,1);denom_row(50,add?2:0);
 note(10,"NEWLIVE00001");info(add?110:10,add?3:1,1);
 stale_serials(tail);assert_current_a(1,add?2:0);
 if(add){denom_boundary(255);info(0,0,2);}else{info(0,0,2);denom_boundary(255);}
 if(sent[0x0C]!=2)fprintf(stderr,"RECOVERY FAILED: next automatic C requests=%u (expected 2), D=%u add=%d tail=%d\n",sent[0x0C],sent[0x0D],add,tail);
 assert(sent[0x0C]==2&&sent[0x0D]==1);
 for(unsigned i=0;i<4;i++)counting_report_poll(&note_session,++clock_tick);
 assert(sent[0x0C]==2);stale_serials(true);
 marker(0x0C,0);marker(0x0C,255);assert(sent[0x0D]==2);
 /* A leftover END before the new query's START does not finish that query. */
 stale_serials(true);assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_NONE);
 marker(0x0D,0);
 if(add){serial_row(1,50,"OLDLIVE00001");serial_row(2,50,"OLDLIVE00002");serial_row(3,10,"NEWLIVE00001");}
 else serial_row(1,10,"NEWLIVE00001");
 assert_current_a(1,add?2:0);marker(0x0D,255);
 assert(observed_commit==COUNTING_HISTORY_COMMIT_PENDING);
 storage_job_id_t current=ui_history_last_commit_id();assert(current&&current!=previous);
 wait_saved(current,app_clock_uptime_ms());counting_history_poll_commit(&note_session,&note_data,app_clock_uptime_ms());
 assert_current_a(1,add?2:0);verify_store(add);assert(sent[0x0C]==2&&sent[0x0D]==2);
 counting_report_poll(&note_session,clock_tick+5000);assert(sent[0x0C]==2&&sent[0x0D]==2);
 assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_NONE);
 counting_report_shutdown();counting_data_clear_serials(&note_data);counting_data_clear_errors(&note_data);
 assert(storage_worker_shutdown());
 printf("PASS timeout recovery add=%d old_tail=%d C=2 D=2 A unchanged, no partial/stale serials, lifetime notes=3\n",add,tail);return 0;
}
