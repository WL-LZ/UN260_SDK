#!/usr/bin/env python3
"""Production live detail parsers: atomic snapshots, no query amplification."""
from pathlib import Path
import subprocess, tempfile, sys

root = Path(__file__).resolve().parents[1]
code = r'''
#include <stdlib.h>
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_data_store.h"
#define main old_tests
#include "tools/test_denom_stream.c"
#undef main
static unsigned completed;
static void complete(void *ctx) {(void)ctx; completed++;}
static void detail_frame(unsigned cmd,unsigned value) {
 uint8_t b[25]={0xFD,0xDF,25,0x0D};unsigned len=cmd==0x0C?7:25;
 b[2]=len;b[3]=cmd;memset(b+4,value,len-5);
 counting_reject_sn_reply_hooks_t h={0};h.on_detail_complete=complete;
 counting_reject_sn_reply_dispatch(cmd,&d,&s,&sim,b,len,&h);
}
int main(int argc,char **argv) {
 old_tests(1,argv);reset();s.phase=COUNTING_SESSION_ACTIVE;
 sim.denom_number=1;sim.denom[0]=(denom_t){5,7,35};
 marker(0);row(100);assert(sim.denom[0].value==5 && sim.denom[0].pcs==7);
 marker(255);assert(sim.denom_number==1 && sim.denom[0].value==100 && rejects==0);
 marker(0);row(20);marker(0);row(10);marker(255);
 assert(sim.denom_number==1 && sim.denom[0].value==10);
 marker(0);for(int i=1;i<=16;i++)row(i);marker(255);
 assert(sim.denom_number==1 && sim.denom[0].value==10);
 marker(255);assert(rejects==0);
 d.wait_sn_after_reject_end=true;detail_frame(0x0C,255);
 assert(serial_requests==0 && !d.wait_sn_after_reject_end);
 detail_frame(0x0D,255);assert(!s.history_record.end_seen && completed==0);
 marker(0);row(5);s.phase=COUNTING_SESSION_FINISHED_WAIT_START;marker(255);
 assert(rejects==1 && d.wait_sn_after_reject_end);
 detail_frame(0x0C,255);assert(serial_requests==1);
 detail_frame(0x0D,255);assert(completed==1 && s.history_record.end_seen);
 if(argc==2) {
  reset();s.phase=COUNTING_SESSION_ACTIVE;serial_requests=0;completed=0;
  FILE*f=fopen(argv[1],"r");assert(f);char line[2048];unsigned frames=0,ends=0,truncated=0;
  while(fgets(line,sizeof(line),f)) {
   char*p=strstr(line,"RX[");if(!p)continue;unsigned len;assert(sscanf(p,"RX[%u]",&len)==1);
   p=strchr(p,':');if(!p){truncated++;continue;}p++;uint8_t b[256];assert(len<=sizeof(b));
   bool complete_line=true;
   for(unsigned i=0;i<len;i++){unsigned v;int n;if(sscanf(p," %x%n",&v,&n)!=1){complete_line=false;break;}b[i]=v;p+=n;}
   if(!complete_line){truncated++;continue;}
   if(b[3]==0x0B){counting_denom_reply_handle(&d,&s,&sim,b,len,NULL);if(b[4]==255)ends++;}
   else if(b[3]==0x0C||b[3]==0x0D||b[3]==0x49) {
    if(b[3]==0x0C)sim.err_expected=1;
    counting_reject_sn_reply_dispatch(b[3],&d,&s,&sim,b,len,NULL);
   }
   frames++;
  }
  fclose(f);assert(frames>50 && ends>2 && rejects==0 && serial_requests==0);
  assert(!s.history_record.end_seen && sim.denom_number==6);
  printf("PASS active-phase stress replay: %u complete logged frames, %u denomination terminators, %u truncated log lines excluded; no follow-up query amplification\n",frames,ends,truncated);
  counting_data_clear_serials(&sim);free(sim.sn_str);
  counting_data_clear_errors(&sim);free(sim.err_pcs);free(sim.err_code);
 }
 puts("PASS atomic live snapshot, replacement, overflow retention, orphan end, stop-straddling final chain, no premature history completion");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-live-detail-') as tmp:
    tmp=Path(tmp)
    stub=tmp/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *fmt, ...);\n')
    src=tmp/'test.c';src.write_text(code)
    exe=tmp/'test'
    files=['counting_denom_reply.c','counting_denom_query_service.c','counting_reject_sn_reply.c','counting_data_store.c']
    subprocess.run(['cc','-std=c11','-g','-O1','-fsanitize=address,undefined','-fno-sanitize-recover=all','-I'+str(tmp),'-I'+str(root),str(src),*[str(root/'un260/counting'/f) for f in files],'-o',str(exe)],check=True)
    subprocess.run([str(exe),*sys.argv[1:]],check=True)
