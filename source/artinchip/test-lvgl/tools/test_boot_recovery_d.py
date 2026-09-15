#!/usr/bin/env python3
"""Real protocol/runtime recovery with the fourth-theme quiet prewarm fence."""
from pathlib import Path
s=Path(__file__).with_name('test_boot_recovery.py').read_text()
s=s.replace('#include <assert.h>', '#define UI_BOOT_ANIM_THEME 4\n#include <assert.h>')
s=s.replace('static bool intro,pure,timer_live;', 'static bool quiet;static unsigned warms;static bool intro,pure,timer_live;')
s=s.replace('(void)p;return true;', '(void)p;warms++;return true;')
s=s.replace('#define FAULT_CONFIRM_GOTO_SENSOR', '''static bool ui_page_08_curr_visual_is_quiet(void){return quiet;}
static void ui_page_08_curr_start_handoff(void){}
static void lv_timer_set_period(lv_timer_t*t,uint32_t ms){assert(t==&timer_storage&&ms==20);}
#define FAULT_CONFIRM_GOTO_SENSOR''')
s=s.replace('pure=!strcmp(argv[1],"pure");app_boot_runtime_finish_timer_cb(&timer_storage);', '''
 if(page==UI_PAGE_BOOT){
  now+=2000;app_boot_runtime_poll(now,true);assert(warms==0);
  app_boot_runtime_finish_timer_cb(&timer_storage);assert(timer_live&&page==UI_PAGE_BOOT);
  quiet=true;
  if(strcmp(argv[1],"deadline")){for(unsigned i=0;i<9;i++){now+=80;app_boot_runtime_poll(now,true);}assert(warms==8);}
  else now+=2001;
 }
 pure=!strcmp(argv[1],"pure");app_boot_runtime_finish_timer_cb(&timer_storage);''')
s=s.replace("('success','pure','failure','timeout','leave','sensor')", "('success','pure','failure','timeout','leave','sensor','deadline')")
exec(compile(s,str(Path(__file__)), 'exec'))
