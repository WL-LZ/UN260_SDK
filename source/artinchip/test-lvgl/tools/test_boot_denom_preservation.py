#!/usr/bin/env python3
"""Real denomination parser + production boot-finish and session-reset bodies."""
from pathlib import Path
import subprocess, tempfile
from test_main_top_gesture import function
root=Path(__file__).resolve().parents[1]
reset=function((root/'un260/app_service/app_counting_runtime.c').read_text(),'app_counting_runtime_reset_session')
finish=function((root/'un260/app_service/app_boot_runtime.c').read_text(),'app_boot_runtime_finish')
fixture=r'''
#define main stream_regression_main
#include "tools/test_denom_stream.c"
#undef main
#define UI_BOOT_ANIM_THEME 4
static counting_session_state_t *g_deferred_boot_finish;
static unsigned switches;
static bool reset_allowed=true;
static const counting_sim_t *counting_data_current(void){return &sim;}
static uint32_t lv_tick_get(void){return now;}
static bool counting_history_prepare_reset(counting_session_state_t *session,const counting_sim_t *data,uint32_t tick){(void)session;(void)data;(void)tick;return reset_allowed;}
static void ui_count_end_anim_cancel(void){}
static void smart_island_notify_count_reset(void){}
static void app_boot_runtime_cancel_prewarm(void){}
static void boot_selftest_list_finish(void){}
static void ui_page_08_curr_start_handoff(void){}
static bool ui_state_pure_count_is_enabled(void){return false;}
enum {UI_PAGE_MAIN,UI_PAGE_PURE};
static void ui_manager_switch(int page){assert(page==UI_PAGE_MAIN);switches++;}
#define sim_data_init() assert(!"Boot must never populate simulation denominations")
'''
main=r'''
int main(void){
 reset();assert(app_boot_runtime_finish(&s));assert(sim.denom_number==0);
 counting_denom_query_expect_push(&d,now,true);marker(0);row(500);row(200);row(100);marker(255);
 counting_sim_t before=sim;
 assert(app_boot_runtime_finish(&s));assert(!memcmp(&before,&sim,sizeof(sim)));
 assert(sim.denom_number==3&&sim.denom[0].value==500);
 counting_denom_query_expect_push(&d,now,true);marker(0);row(1000);
 counting_detail_state_t pending=d;
 assert(app_boot_runtime_finish(&s));assert(!memcmp(&pending,&d,sizeof(d)));
 row(25);marker(255);assert(sim.denom_number==2&&sim.denom[0].value==1000&&sim.denom[1].value==25);
 before=sim;reset_allowed=false;unsigned previous=switches;
 assert(!app_boot_runtime_finish(&s));assert(switches==previous&&g_deferred_boot_finish==&s);
 assert(!memcmp(&before,&sim,sizeof(sim)));
 reset_allowed=true;assert(app_boot_runtime_finish(&s));assert(!memcmp(&before,&sim,sizeof(sim)));
 puts("PASS boot denominations: absent stays empty, received preserved, in-flight query completes, deferred history reset preserves data");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-boot-denom-') as tmp:
    tmp=Path(tmp);stub=tmp/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True)
    stub.write_text('void uart_debug_printf(const char *fmt, ...);\n')
    src=tmp/'test.c';src.write_text(fixture+reset+finish+main)
    exe=tmp/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-error=return-type','-Wno-error=sign-compare','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(tmp),'-I'+str(root),str(src),str(root/'un260/counting/counting_denom_reply.c'),str(root/'un260/counting/counting_denom_query_service.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
