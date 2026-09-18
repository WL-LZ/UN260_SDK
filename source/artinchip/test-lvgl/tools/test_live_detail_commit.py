#!/usr/bin/env python3
"""Exercise production live-denomination callback + frame coalescer."""
from pathlib import Path
import subprocess, tempfile
from test_main_top_gesture import function

root=Path(__file__).resolve().parents[1]
source=(root/'un260/app_service/app_counting_runtime.c').read_text()
code=r'''
#include <assert.h>
#include <stdio.h>
#include "un260/counting/counting_data_types.h"
#include "un260/lv_core/ui_frame_commit.h"
static counting_sim_t data;
static unsigned rows,totals,dirty;
static bool visible=true;
enum {PAGE_01_MAIN_DIRTY_COUNTING=1,PAGE_01_DETAIL_SECTION_B=1,PAGE_02_SECTION_A=0};
static const counting_sim_t *counting_data_current(void){return &data;}
static bool page_01_main_is_visible(void){return visible;}
static void page_01_main_mark_dirty(unsigned flag){assert(flag==1);dirty++;}
static void page_02_list_section_mark_dirty(unsigned id){assert(id==0);}
static void app_counting_runtime_format_amount(char *out,size_t size,float value){snprintf(out,size,"%.0f",value);}
static void page_01_main_refresh_totals(int pcs,const char *amount){(void)pcs;(void)amount;totals++;}
static int page_01_detail_section_get(void){return 0;}
static void page_01_main_detail_refresh_rows_only(void){rows++;}
'''
code+=function(source,'app_counting_visual_commit')
code+=function(source,'app_counting_runtime_on_live_denom_changed')
code+=r'''
int main(void){
 ui_frame_commit_begin_batch();
 for(unsigned i=0;i<100;i++)app_counting_runtime_on_live_denom_changed();
 assert(rows==0);ui_frame_commit_end_batch();ui_frame_commit_flush();assert(rows==1 && totals==0);
 visible=false;ui_frame_commit_begin_batch();
 for(unsigned i=0;i<100;i++)app_counting_runtime_on_live_denom_changed();
 ui_frame_commit_end_batch();ui_frame_commit_flush();assert(rows==1 && dirty==1);
 visible=true;app_counting_runtime_on_live_denom_changed();assert(rows==2);
 puts("PASS 100 live notifications coalesce to one rows-only refresh, hidden page stays dirty, non-batched fallback works");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-live-commit-') as tmp:
    tmp=Path(tmp);src=tmp/'test.c';src.write_text(code);exe=tmp/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root),str(src),str(root/'un260/lv_core/ui_frame_commit.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
