#!/usr/bin/env python3
"""Real 0B parser -> runtime callbacks -> frame coalescing, before END.
UI endpoint spies complement the actual Main/List LVGL label/lifecycle tests.
The denomination/query/report/data modules and runtime functions stay real.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile
from test_main_view import function

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path,default=Path('/dev/shm/un260-controller-denom-live/live-a'))
out=parser.parse_args().output_dir
out.mkdir(parents=True,exist_ok=True)
runtime=(root/'un260/app_service/app_counting_runtime.c').read_text()
preamble=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/ui_frame_commit.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"
#define lv_snprintf snprintf
#define PAGE_01_MAIN_DIRTY_COUNTING 1
#define PAGE_01_DETAIL_SECTION_A 0
#define PAGE_01_DETAIL_SECTION_B 1
#define PAGE_01_DETAIL_SECTION_C 2
#define PAGE_02_SECTION_A 0
#define PAGE_02_SECTION_C 2
#define data (*counting_data_mutable())
static counting_session_state_t session;
static counting_detail_state_t detail;
static bool visible=true;
static unsigned section,rows,totals,dirty,list_dirty,requests,history_frames;
static unsigned displayed_pcs;
static uint64_t displayed_amount;

uint32_t app_clock_uptime_ms(void){static uint32_t n=100;return ++n;}
void uart_debug_printf(const char *s,...){(void)s;}
void counting_history_current_invalidate(void){}
int protocol_send(uint8_t c,const uint8_t *b,uint16_t n){(void)c;(void)b;(void)n;requests++;return 0;}
static bool page_01_main_is_visible(void){return visible;}
static void page_01_main_mark_dirty(unsigned f){dirty|=f;}
static void page_01_main_refresh_totals(int n,const char *s){(void)n;(void)s;totals++;}
static unsigned page_01_detail_section_get(void){return section;}
static void page_01_main_detail_refresh_rows_only(void){
 rows++; displayed_pcs=data.denom[0].pcs;
 displayed_amount=counting_data_denom_amount(&data.denom[0]);
}
static void page_02_list_section_mark_dirty(unsigned s){list_dirty|=1U<<s;}
static void smart_island_update_counting(int p,double a){(void)p;(void)a;}
static void history(const char *tag,const uint8_t *b,uint8_t n){assert(!strcmp(tag,"0x0B"));(void)b;(void)n;history_frames++;}
static void flush(void){ui_frame_commit_flush();}
static void app_counting_runtime_on_main_data_changed(void);
static void app_counting_runtime_on_live_denom_changed(void);
static const counting_denom_reply_hooks_t g_counting_denom_hooks={
 .on_history_frame=history,.on_main_data_changed=app_counting_runtime_on_main_data_changed
};
/* Finished-report storage and island animation are exercised in the history
 * and native tests. These edges are not needed to assert live A scheduling. */
#define counting_multi_query_busy() false
#define currency_state_multi_selected() false
#define counting_multi_denom(...) ((void)0)
static void ui_refresh_main_page(void){page_01_main_detail_refresh_rows_only();}
'''
main=r'''
static void wire(unsigned value,unsigned pcs,int marker){
 uint8_t b[16]={0xFD,0xDF,16,0x0B};char text[16];
 if(marker>=0)memset(b+4,marker,11);
 else {assert(pcs<=999);snprintf(text,sizeof(text),"%8u%3u",value,pcs);memcpy(b+4,text,11);}
 app_counting_runtime_handle_denom(&detail,&session,&data,b,sizeof(b));
}
static void serial(void){
 uint8_t b[24]={0xFD,0xDF,24,0x49};char text[24];
 snprintf(text,sizeof(text),"%7u%-12.12s",50U,"SAME00000001");memcpy(b+4,text,19);
 counting_reject_sn_reply_dispatch(0x49,&detail,&session,&data,b,sizeof(b),NULL);
}
int main(void){
 counting_report_begin(false,&data);
 counting_denom_reply_begin_count(&detail);
 data.denom_number=2;data.denom[0]=(denom_t){.value=5};data.denom[1]=(denom_t){.value=10};
 /* START acknowledgement precedes the first 0E ACTIVE status. */
 session.start_confirmed=true;session.phase=COUNTING_SESSION_IDLE;
 ui_frame_commit_begin_batch();wire(0,0,0);flush();unsigned before=rows;
 wire(5,1,-1);assert(data.denom[0].pcs==1 && rows==before);
 wire(5,2,-1);assert(data.denom[0].pcs==2 && rows==before);
 assert(list_dirty&1);flush();assert(rows==before+1 && displayed_pcs==2 && displayed_amount==10);
 /* A repeated absolute row cannot add a second copy; 49 belongs only to B. */
 wire(5,2,-1);serial();serial();flush();
 assert(data.denom[0].pcs==2 && displayed_pcs==2);
 assert(counting_data_serial_valid_count(&data)==2);
 session.phase=COUNTING_SESSION_ACTIVE;before=rows;
 wire(5,3,-1);wire(5,4,-1);assert(rows==before);flush();
 assert(rows==before+1 && displayed_pcs==4);
 /* Total information may lag or lead; it never determines A face values. */
 data.total_pcs=99;data.total_amount=12345;list_dirty=0;
 app_counting_runtime_refresh_compact(&data);flush();
 assert((list_dirty&1)==0);
 assert(data.denom[0].pcs==4 && displayed_pcs==4 && totals);
 section=1;before=rows;wire(5,5,-1);flush();assert(rows==before && (list_dirty&1));
 section=2;wire(5,6,-1);flush();assert(rows==before);
 section=0;visible=false;wire(5,7,-1);flush();assert(rows==before && dirty);
 visible=true;app_counting_runtime_on_live_denom_changed();flush();
 assert(rows==before+1 && displayed_pcs==7);
 /* 0B END during an active pass does not fire detail queries or count-end. */
 wire(0,0,255);flush();assert(data.denom[0].pcs==7 && requests==0);
 ui_frame_commit_end_batch();
 before=rows;app_counting_runtime_on_live_denom_changed();assert(rows==before+1);
 /* Existing explicit B/C and full refresh routes retain their section rules. */
 section=1;app_counting_visual_commit(NULL,2);assert(rows==before+2);
 section=2;app_counting_visual_commit(NULL,8);assert(rows==before+3);
 app_counting_visual_commit(NULL,4);assert(rows==before+4);
 data.multi_currency_result=true;section=0;before=rows;list_dirty=0;
 app_counting_runtime_refresh_compact(&data);assert(rows==before && (list_dirty&1));
 assert(history_frames>=9);
 counting_report_shutdown();counting_data_clear_serials(&data);counting_data_clear_errors(&data);
 puts("PASS real 0B -> runtime -> A frame scheduling: START-before-0E, absolute/repeated row, 49 independence, totals independence, merge, hidden, B/C, MULTI and no extra request");
}
'''
names=['app_counting_runtime_format_amount','app_counting_visual_commit',
       'app_counting_runtime_refresh_compact','app_counting_runtime_on_main_data_changed',
       'app_counting_runtime_on_live_denom_changed','app_counting_runtime_handle_denom']
source=preamble+'\n'.join(function(runtime,n) for n in names)+main
with tempfile.TemporaryDirectory(prefix='live-a-',dir=out) as t:
 p=Path(t);(p/'test.c').write_text(source)
 stub=p/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True)
 stub.write_text('void uart_debug_printf(const char *, ...);\n')
 modules=['un260/lv_core/ui_frame_commit.c','un260/counting/counting_denom_reply.c',
          'un260/counting/counting_denom_query_service.c','un260/counting/counting_report_sync.c',
          'un260/counting/counting_data_store.c','un260/counting/counting_reject_sn_reply.c',
          'un260/counting/counting_serial_text.c']
 command=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-O1',
          '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
          '-I'+str(p),'-I'+str(root),str(p/'test.c'),
          *[str(root/x) for x in modules],'-o',str(p/'test')]
 subprocess.run(command,check=True)
 subprocess.run([str(p/'test')],check=True)
