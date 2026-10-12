#!/usr/bin/env python3
"""Real protocol/runtime recovery with the fourth-theme quiet prewarm fence."""
from pathlib import Path
s=Path(__file__).with_name('test_boot_recovery.py').read_text()
s=s.replace('#include <assert.h>', '#define UI_BOOT_ANIM_THEME 4\n#include <assert.h>')
s=s.replace('static bool intro,pure,timer_live;', 'static bool quiet,hold;static unsigned warms;static bool intro,pure,timer_live;')
s=s.replace('(void)p;return true;', 'if(warms==0)assert(p==UI_PAGE_MAIN);warms++;return true;')
s=s.replace('#define sim_data_init', '''static bool ui_page_08_curr_visual_is_quiet(void){return quiet;}
static bool ui_page_08_curr_ready_hold_complete(void){return hold;}
static void ui_page_08_curr_start_handoff(void){}
static unsigned preloads;static bool preload_should_fail;
static bool page_01_main_quick_prepare_preload(void){assert(page==UI_PAGE_MAIN||page==UI_PAGE_PURE);preloads++;return !preload_should_fail;}
static void lv_timer_set_period(lv_timer_t*t,uint32_t ms){assert(t==&timer_storage&&ms==20);}
#define sim_data_init''')
s=s.replace('pure=!strcmp(argv[1],"pure");app_boot_runtime_finish_timer_cb(&timer_storage);', '''
 if(page==UI_PAGE_BOOT){
  now+=2000;app_boot_runtime_poll(now,true);assert(warms==0);
  app_boot_runtime_finish_timer_cb(&timer_storage);assert(timer_live&&page==UI_PAGE_BOOT);
  quiet=true;
  if(!strcmp(argv[1],"fast")){now+=80;app_boot_runtime_poll(now,true);assert(warms==1);}
  else if(strcmp(argv[1],"deadline")){for(unsigned i=0;i<9;i++){now+=80;app_boot_runtime_poll(now,true);}assert(warms==7);}
  else now+=2001;
  app_boot_runtime_finish_timer_cb(&timer_storage);assert(timer_live&&page==UI_PAGE_BOOT);
  hold=true;
 }
 preload_should_fail=!strcmp(argv[1],"preload-failure");pure=!strcmp(argv[1],"pure");app_boot_runtime_finish_timer_cb(&timer_storage);''')
s=s.replace("('success','pure','failure','timeout','leave','recovery')", "('success','pure','failure','timeout','leave','recovery','deadline','fast','preload-failure')")
s=s.replace('assert(!timer_live);\n if(!strcmp(argv[1],"leave"))','assert(!timer_live);assert(preloads==(!strcmp(argv[1],"leave")?0U:1U));\n if(!strcmp(argv[1],"leave"))')
exec(compile(s,str(Path(__file__)), 'exec'))
