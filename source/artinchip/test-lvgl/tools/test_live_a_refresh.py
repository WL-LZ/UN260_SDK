#!/usr/bin/env python3
"""Exercise production live projection scheduling before any end/full-list frame."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
runtime=(root/'un260/app_service/app_counting_runtime.c').read_text()
preamble=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/ui_frame_commit.h"
#define lv_snprintf snprintf
#define PAGE_01_MAIN_DIRTY_COUNTING 1
#define PAGE_01_DETAIL_SECTION_A 0
#define PAGE_01_DETAIL_SECTION_B 1
#define PAGE_01_DETAIL_SECTION_C 2
#define PAGE_02_SECTION_A 0
#define PAGE_02_SECTION_C 2
typedef struct {int total_pcs; float total_amount; bool multi_currency_result;} counting_sim_t;
static counting_sim_t data;
static bool visible=true;
static unsigned section,rows,totals,dirty;
static int displayed_pcs;

static const counting_sim_t *counting_data_current(void){return &data;}
static bool page_01_main_is_visible(void){return visible;}
static void page_01_main_mark_dirty(unsigned f){dirty|=f;}
static void page_01_main_refresh_totals(int n,const char *s){(void)s;totals++;displayed_pcs=n;}
static unsigned page_01_detail_section_get(void){return section;}
static void page_01_main_detail_refresh_rows_only(void){rows++;displayed_pcs=data.total_pcs;}
static void page_02_list_section_mark_dirty(unsigned s){(void)s;}
static void smart_island_update_counting(int p,float a){(void)p;(void)a;}
static void flush(void){ui_frame_commit_flush();}
'''
main=r'''
int main(void){
 ui_frame_commit_begin_batch();
 /* No stop or full-list callback: a live total must publish A rows. */
 data.total_pcs=1;data.total_amount=50;
 app_counting_runtime_refresh_compact(&data);assert(rows==0);
 flush();assert(rows==1 && displayed_pcs==1 && totals==1);
 /* Several accepted frames coalesce into one latest-state refresh. */
 data.total_pcs=2;app_counting_runtime_refresh_compact(&data);
 data.total_pcs=3;app_counting_runtime_refresh_compact(&data);
 flush();assert(rows==2 && displayed_pcs==3);
 /* Totals must not redraw B/C with every accepted note. */
 section=1;app_counting_runtime_refresh_compact(&data);flush();assert(rows==2);
 section=2;app_counting_runtime_refresh_compact(&data);flush();assert(rows==2);
 section=0;data.multi_currency_result=true;
 app_counting_runtime_refresh_compact(&data);flush();assert(rows==2);
 data.multi_currency_result=false;visible=false;
 app_counting_runtime_refresh_compact(&data);flush();assert(rows==2 && dirty);
 visible=true;ui_frame_commit_end_batch();
 app_counting_runtime_refresh_compact(&data);assert(rows==3);
 /* Existing explicit row updates for B/C and full denomination remain. */
 section=1;app_counting_visual_commit(NULL,2);assert(rows==4);
 section=2;app_counting_visual_commit(NULL,8);assert(rows==5);
 app_counting_visual_commit(NULL,4);assert(rows==6);
 puts("PASS live A refresh before end; coalescing, hidden, B/C, MULTI, fallback");
}
'''
source=preamble+'\n'.join(function(runtime,n) for n in ['app_counting_runtime_format_amount','app_counting_visual_commit','app_counting_runtime_refresh_compact'])+main
with tempfile.TemporaryDirectory(prefix='un260-live-a-') as t:
 p=Path(t);(p/'test.c').write_text(source)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root),str(p/'test.c'),str(root/'un260/lv_core/ui_frame_commit.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
