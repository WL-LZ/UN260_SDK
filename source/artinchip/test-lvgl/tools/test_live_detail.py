#!/usr/bin/env python3
"""Production 0B absolute live rows, atomic query catalogues, no query amplification."""
from pathlib import Path
import subprocess, tempfile, sys

root = Path(__file__).resolve().parents[1]
code = r'''
#include <stdlib.h>
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_report_sync.h"
#define TEST_REAL_REPORT_SYNC
#define main old_tests
#include "tools/test_denom_stream.c"
#undef main
void counting_history_current_invalidate(void) {}
static unsigned completed;
static void complete(void *ctx) {(void)ctx; completed++;}
static void release_result(void) {
 counting_data_clear_serials(&sim);counting_data_clear_errors(&sim);
 counting_report_shutdown();
}
static void fresh_count(void) {
 release_result();reset();serial_requests=completed=0;
 counting_report_begin(false,&sim);counting_denom_reply_begin_count(&d);
 s.phase=COUNTING_SESSION_ACTIVE;s.start_confirmed=true;
}
static bool contains(int value) {
 for(unsigned i=0;i<sim.denom_number;i++)if(sim.denom[i].value==value)return true;
 return false;
}
static void detail_frame(unsigned cmd,unsigned value) {
 uint8_t b[25]={0xFD,0xDF,25,0x0D};unsigned len=cmd==0x0C?7:25;
 b[2]=len;b[3]=cmd;memset(b+4,value,len-5);
 counting_reject_sn_reply_hooks_t h={0};h.on_detail_complete=complete;
 counting_reject_sn_reply_dispatch(cmd,&d,&s,&sim,b,len,&h);
}
/* Independent replay oracle: a valid row replaces the last quantity for that
 * face, BEGIN clears quantities while retaining faces, 49/0C/0D never edit A.
 * New faces beyond the fixed table capacity are rejected without discarding
 * already confirmed entries. */
static denom_t expected[COUNTING_DENOM_MAX_ITEMS];
static unsigned expected_count;
static bool expected_open;
static bool payload_is(const uint8_t *b,unsigned value) {
 for(unsigned i=4;i<15;i++)if(b[i]!=value)return false;
 return true;
}
static bool decimal(const uint8_t *p,unsigned length,unsigned *out) {
 char text[9];assert(length<sizeof(text));memcpy(text,p,length);text[length]=0;
 char *end;unsigned long n=strtoul(text,&end,10);
 if(end==text)return false;
 while(*end==' ')*end++=0;
 if(*end || n>99999999)return false;
 *out=(unsigned)n;return true;
}
static void replay_expected(const uint8_t *b,unsigned len) {
 if(b[3]!=0x0B || len!=16)return;
 if(payload_is(b,0)){
  expected_open=true;for(unsigned i=0;i<expected_count;i++)expected[i].pcs=0;
 }else if(payload_is(b,255))expected_open=false;
 else if(expected_open){
  unsigned face,count;
  if(!decimal(b+4,8,&face)||!face||!decimal(b+12,3,&count))return;
  unsigned i=0;while(i<expected_count && expected[i].value!=(int)face)i++;
  if(i==COUNTING_DENOM_MAX_ITEMS)return;
  if(i==expected_count)expected[expected_count++].value=(int)face;
  expected[i].pcs=(uint16_t)count;
 }
}
static void assert_replay_expected(void) {
 assert(sim.denom_number==expected_count);
 for(unsigned i=0;i<expected_count;i++){
  unsigned j=0;while(j<sim.denom_number && sim.denom[j].value!=expected[i].value)j++;
  assert(j<sim.denom_number && sim.denom[j].pcs==expected[i].pcs);
 }
}
int main(int argc,char **argv) {
 old_tests(1,argv);
 /* Directory queries retain their separate atomic commit contract. */
 release_result();reset();serial_requests=completed=0;
 sim.denom_number=1;sim.denom[0]=(denom_t){5,7,35};
 counting_denom_query_expect_push(&d,now,true);marker(0);row(100);
 assert(sim.denom_number==1 && sim.denom[0].value==5 && sim.denom[0].pcs==7);
 marker(255);assert(d.query_complete && sim.denom_number==1 && sim.denom[0].value==100);
 assert(rejects==0 && serial_requests==0);

 fresh_count();sim.denom_number=1;sim.denom[0]=(denom_t){5,7,35};
 marker(0);assert(sim.denom_number==1 && sim.denom[0].value==5 && sim.denom[0].pcs==0);
 row(100);assert(sim.denom_number==2 && contains(5) && contains(100));
 marker(255);assert(sim.denom_number==2 && rejects==0);
 marker(0);row(20);marker(0);row(10);marker(255);
 assert(sim.denom_number==4 && contains(5) && contains(100) && contains(20) && contains(10));

 fresh_count();marker(0);for(int i=1;i<=16;i++)row(i);marker(255);
 assert(sim.denom_number==15 && contains(1) && contains(15) && !contains(16));
 marker(255);assert(rejects==0);
 detail_frame(0x0C,255);assert(serial_requests==0);
 detail_frame(0x0D,255);assert(!s.history_record.end_seen && completed==0);
 /* A rejected 16th denomination does not suppress independent B/C retrieval. */
 s.phase=COUNTING_SESSION_FINISHED_WAIT_START;s.start_confirmed=false;
 counting_report_schedule(&s,now);assert(rejects==1);
 counting_report_schedule(&s,now);marker(255);assert(rejects==1);
 detail_frame(0x0C,0);detail_frame(0x0C,0);detail_frame(0x0C,255);
 assert(serial_requests==1);
 detail_frame(0x0D,0);detail_frame(0x0D,255);
 assert(completed==1 && s.history_record.end_seen && sim.denom_number==15);
 detail_frame(0x0C,255);detail_frame(0x0D,255);
 assert(completed==1 && rejects==1 && serial_requests==1);

 /* STOP before 0B END still sends exactly one follow-up query. */
 fresh_count();marker(0);row(5);
 s.phase=COUNTING_SESSION_FINISHED_WAIT_START;s.start_confirmed=false;
 counting_report_schedule(&s,now);assert(rejects==0);
 marker(255);assert(rejects==1);
 detail_frame(0x0C,0);detail_frame(0x0C,255);assert(serial_requests==1);
 detail_frame(0x0D,0);detail_frame(0x0D,255);
 assert(completed==1 && s.history_record.end_seen && contains(5));

 if(argc==2) {
  release_result();reset();s.phase=COUNTING_SESSION_ACTIVE;
  serial_requests=completed=0;expected_count=0;expected_open=false;
  FILE*f=fopen(argv[1],"r");assert(f);char line[2048];unsigned frames=0,ends=0,truncated=0;
  while(fgets(line,sizeof(line),f)) {
   char*p=strstr(line,"RX[");if(!p)continue;unsigned len;assert(sscanf(p,"RX[%u]",&len)==1);
   p=strchr(p,':');if(!p){truncated++;continue;}p++;uint8_t b[256];assert(len<=sizeof(b));
   bool complete_line=true;
   for(unsigned i=0;i<len;i++){unsigned v;int n;if(sscanf(p," %x%n",&v,&n)!=1){complete_line=false;break;}assert(v<=255);b[i]=v;p+=n;}
   if(!complete_line){truncated++;continue;}
   replay_expected(b,len);
   if(b[3]==0x0B){counting_denom_reply_handle(&d,&s,&sim,b,len,NULL);if(len==16 && payload_is(b,255))ends++;}
   else if(b[3]==0x0C||b[3]==0x0D||b[3]==0x49) {
    if(b[3]==0x0C)sim.err_expected=1;
    counting_reject_sn_reply_dispatch(b[3],&d,&s,&sim,b,len,NULL);
   }
   assert_replay_expected();frames++;
  }
  fclose(f);assert(frames>50 && ends>2 && rejects==0 && serial_requests==0);
  assert(!s.history_record.end_seen && expected_count>0);
  printf("PASS active-phase stress replay: %u complete logged frames, %u denomination terminators, %u truncated log lines excluded, %u retained catalogue faces; absolute A checked per frame, no follow-up query amplification\n",frames,ends,truncated,expected_count);
 }
 release_result();
 puts("PASS live absolute rows, catalogue retention, atomic query, capacity retention, orphan end, both final orders, independent B/C, no premature history completion");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-live-detail-') as tmp:
    tmp=Path(tmp)
    stub=tmp/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *fmt, ...);\n')
    src=tmp/'test.c';src.write_text(code)
    exe=tmp/'test'
    files=['counting_denom_reply.c','counting_denom_query_service.c','counting_reject_sn_reply.c','counting_data_store.c','counting_report_sync.c']
    subprocess.run(['cc','-std=c11','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-I'+str(tmp),'-I'+str(root),str(src),*[str(root/'un260/counting'/f) for f in files],'-o',str(exe)],check=True)
    subprocess.run([str(exe),*sys.argv[1:]],check=True)
