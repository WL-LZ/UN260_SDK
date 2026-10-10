#!/usr/bin/env python3
"""Host-test production detail callbacks with the real frame coalescer.

View calls are observed, not rendered. List activation and recycled-row binding
are covered by the separate LVGL view tests; this test owns callback scheduling.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from test_multi_result_safety import function

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "un260/app_service/app_counting_runtime.c").read_text(encoding="utf-8")
names = (
    "app_counting_visual_commit",
    "app_counting_runtime_main_page_active",
    "app_counting_runtime_refresh_compact",
    "app_counting_runtime_on_live_denom_changed",
    "app_counting_runtime_on_reject_report_changed",
    "app_counting_runtime_on_serial_item_changed",
)
parts = [function(source, name) for name in names]
for name, part in zip(names, parts):
    for forbidden in ("protocol_send", "uart_send", "ui_manager_switch",
                      "page_02_list_report_reset"):
        assert forbidden not in part, (name, forbidden)

code = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_data_types.h"
#include "un260/lv_core/ui_frame_commit.h"
enum {
 PAGE_01_MAIN_DIRTY_COUNTING=1U<<9,
 PAGE_01_DETAIL_SECTION_A=0,PAGE_01_DETAIL_SECTION_B=1,PAGE_01_DETAIL_SECTION_C=2,
 PAGE_02_SECTION_A=0,PAGE_02_SECTION_B=1,PAGE_02_SECTION_C=2,
 UI_PAGE_MAIN=1
};
static counting_sim_t data;
static uint8_t reject_code[1]={0x1c},reject_pcs[1];
static unsigned rows,totals,dirty,list_dirty[3],island_updates;
static unsigned shown_err_num,shown_err_pcs;
static int active_section;
static bool visible=true;
static const counting_sim_t *counting_data_current(void){return &data;}
static bool page_01_main_is_visible(void){return visible;}
static bool page_01_main_is_created(void){return true;}
static int ui_manager_get_current_page(void){return visible?UI_PAGE_MAIN:0;}
static void page_01_main_mark_dirty(unsigned flag){assert(flag==PAGE_01_MAIN_DIRTY_COUNTING);dirty++;}
static void page_02_list_section_mark_dirty(unsigned id){assert(id<3);list_dirty[id]++;}
static void page_02_list_section_data_ready(unsigned id){assert(id==PAGE_02_SECTION_C);list_dirty[id]++;}
static void app_counting_runtime_format_amount(char *out,size_t size,float value){snprintf(out,size,"%.0f",value);}
static void page_01_main_refresh_totals(int pcs,const char *amount){assert(pcs==data.total_pcs);assert(strcmp(amount,"123")==0);totals++;}
static void smart_island_update_counting(int pcs,float amount){assert(pcs==data.total_pcs && amount==data.total_amount);island_updates++;}
static int page_01_detail_section_get(void){return active_section;}
static void page_01_main_detail_refresh_rows_only(void){rows++;shown_err_num=data.err_num;shown_err_pcs=data.err_pcs[0];}
"""
code += "\n\n".join(parts)
code += r"""
static void reset(int section,bool show){
 assert(!ui_frame_commit_pending());
 ui_frame_commit_take_stats(NULL);
 rows=totals=dirty=island_updates=shown_err_num=shown_err_pcs=0;
 memset(list_dirty,0,sizeof(list_dirty));
 visible=show;active_section=section;
 memset(&data,0,sizeof(data));
 data.total_amount=123;data.total_pcs=9;
 data.err_num=1;data.err_pcs=reject_pcs;data.err_code=reject_code;
 reject_pcs[0]=1;
}
static void flush_batch(unsigned requests){
 ui_frame_commit_end_batch();
 assert(rows==0 && totals==0);
 ui_frame_commit_flush();
 assert(!ui_frame_commit_pending());
 ui_frame_commit_stats_t stats;
 ui_frame_commit_take_stats(&stats);
 assert(stats.requests==requests && stats.merged==requests-1);
 assert(stats.callbacks==1 && stats.full==0);
}
static void test_reject_batch(void){
 reset(PAGE_01_DETAIL_SECTION_C,true);
 ui_frame_commit_begin_batch();
 for(unsigned i=1;i<=100;i++){
  reject_pcs[0]=(uint8_t)i;
  app_counting_runtime_on_reject_report_changed(NULL);
 }
 flush_batch(100);
 assert(rows==1 && totals==0 && dirty==0 && island_updates==0);
 assert(list_dirty[2]==100 && list_dirty[0]==0 && list_dirty[1]==0);
 assert(shown_err_num==1 && shown_err_pcs==100);
 puts("PASS 100 reject notifications coalesce once and consume latest quantity");
}
static void test_same_reason_count(void){
 reset(PAGE_01_DETAIL_SECTION_C,true);
 for(unsigned pcs=1;pcs<=3;pcs++){
  reject_pcs[0]=(uint8_t)pcs;
  app_counting_runtime_on_reject_report_changed(NULL);
  assert(rows==pcs && totals==0 && shown_err_num==1 && shown_err_pcs==pcs);
 }
 assert(!ui_frame_commit_pending());
 puts("PASS unchanged err_num with quantity 1/2/3 still schedules each refresh");
}
static void test_inactive_reject_section(void){
 for(int section=PAGE_01_DETAIL_SECTION_A;section<=PAGE_01_DETAIL_SECTION_B;section++){
  reset(section,true);
  ui_frame_commit_begin_batch();
  reject_pcs[0]=3;
  app_counting_runtime_on_reject_report_changed(NULL);
  flush_batch(1);
  assert(rows==0 && totals==0 && list_dirty[2]==1);
  /* Activation may happen after the notification, before the next frame.
     The production commit must query current section/data at commit time. */
  ui_frame_commit_begin_batch();
  reject_pcs[0]=42;
  app_counting_runtime_on_reject_report_changed(NULL);
  active_section=PAGE_01_DETAIL_SECTION_C;
  flush_batch(1);
  assert(rows==1 && totals==0 && shown_err_pcs==42);
 }
 puts("PASS reject changes do not redraw A/B; next frame after selecting C sees latest data");
}
static void test_hidden(void){
 reset(PAGE_01_DETAIL_SECTION_C,false);
 ui_frame_commit_begin_batch();
 for(unsigned i=1;i<=100;i++){
  reject_pcs[0]=(uint8_t)i;
  app_counting_runtime_on_reject_report_changed(NULL);
 }
 flush_batch(100);
 assert(rows==0 && totals==0 && dirty==1 && list_dirty[2]==100);
 visible=true;
 app_counting_runtime_on_reject_report_changed(NULL);
 assert(rows==1 && totals==0 && shown_err_pcs==100);
 /* A page hidden between receipt and commit must also avoid rendering. */
 reset(PAGE_01_DETAIL_SECTION_C,true);
 ui_frame_commit_begin_batch();
 app_counting_runtime_on_reject_report_changed(NULL);
 visible=false;
 flush_batch(1);
 assert(rows==0 && totals==0 && dirty==1);
 puts("PASS hidden pages only mark COUNTING dirty; non-batched visible fallback works");
}
static void test_mixed_callbacks(void){
 reset(PAGE_01_DETAIL_SECTION_B,true);
 ui_frame_commit_begin_batch();
 for(unsigned i=0;i<100;i++){
  app_counting_runtime_refresh_compact(&data);             /* 1 */
  app_counting_runtime_on_serial_item_changed(NULL);       /* 2 */
  app_counting_runtime_on_live_denom_changed();            /* 4 */
  app_counting_runtime_on_reject_report_changed(NULL);     /* 8 */
 }
 active_section=PAGE_01_DETAIL_SECTION_C;
 flush_batch(400);
 assert(rows==1 && totals==1 && dirty==0 && island_updates==100);
 assert(list_dirty[0]==200 && list_dirty[1]==100 && list_dirty[2]==200);
 puts("PASS mixed 1/2/4/8 callbacks merge once while preserving totals and List invalidations");
}
static void test_original_section_flags(void){
 for(int section=0;section<3;section++){
  reset(section,true);
  ui_frame_commit_begin_batch();
  for(unsigned i=0;i<100;i++)app_counting_runtime_on_live_denom_changed();
  flush_batch(100);
  assert(rows==1 && totals==0 && list_dirty[0]==100);
  reset(section,true);
  app_counting_runtime_on_serial_item_changed(NULL);
  assert(rows==(section==PAGE_01_DETAIL_SECTION_B) && totals==0 && list_dirty[1]==1);
  reset(section,true);
  app_counting_runtime_refresh_compact(&data);
  assert(rows==0 && totals==1 && list_dirty[0]==1 && list_dirty[2]==1);
 }
 /* Flag 2 queued on B must not accidentally redraw C when it alone is pending. */
 reset(PAGE_01_DETAIL_SECTION_B,true);
 ui_frame_commit_begin_batch();
 app_counting_runtime_on_serial_item_changed(NULL);
 active_section=PAGE_01_DETAIL_SECTION_C;
 flush_batch(1);
 assert(rows==0 && totals==0);
 puts("PASS existing totals-only, active-B serial, and unconditional denomination-row semantics");
}
int main(void){
 test_reject_batch();test_same_reason_count();test_inactive_reject_section();
 test_hidden();test_mixed_callbacks();test_original_section_flags();
 puts("PASS production callbacks + real frame coalescer; no protocol/navigation stubs or rendering claims");
}
"""
with tempfile.TemporaryDirectory(prefix="un260-live-commit-") as directory:
    work = Path(directory)
    src = work / "test.c"
    src.write_text(code, encoding="utf-8")
    exe = work / "test"
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-I" + str(ROOT), str(src), str(ROOT / "un260/lv_core/ui_frame_commit.c"),
        "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
