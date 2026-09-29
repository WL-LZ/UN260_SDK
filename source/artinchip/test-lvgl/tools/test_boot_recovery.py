#!/usr/bin/env python3
"""Run boot lifecycle and per-item fault projection against real protocol state."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
runtime=re.sub(r'^#include[^\n]*\n','',(root/'un260/app_service/app_boot_runtime.c').read_text(),flags=re.M)
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "un260/boot/boot_reply.h"
#include "un260/machine_state/machine_fault.h"
typedef int counting_session_state_t;
typedef enum {UI_PAGE_BOOT,UI_PAGE_MENU,UI_PAGE_LIST,UI_PAGE_HISTORY,UI_PAGE_SETTING,UI_PAGE_PURE,
UI_PAGE_INNOVATION_CENTER,UI_PAGE_CURR,UI_PAGE_MAIN,UI_PAGE_SENSOR} ui_page_t;
typedef struct {void *user_data;} lv_timer_t;
static uint32_t now;static ui_page_t page=UI_PAGE_BOOT;
static bool intro,pure,timer_live;static unsigned sends,faults,switches;
static unsigned recorded;
static uint8_t last_cmd,last_value;static lv_timer_t timer_storage;
static uint32_t app_clock_uptime_ms(void){return now;}
static uint32_t lv_tick_get(void){return now;}
static ui_page_t ui_manager_get_current_page(void){return page;}
static bool ui_manager_prewarm_page(ui_page_t p){(void)p;return true;}
static bool ui_page_00_boot_anim_is_active(void){return intro;}
static void protocol_send(uint8_t cmd,const uint8_t *p,unsigned len){assert(p && len==1);sends++;last_cmd=cmd;last_value=*p;}
static bool app_counting_runtime_reset_session(counting_session_state_t *s,const char *why){assert(s&&why);return true;}
#define sim_data_init() assert(!"Simulator initialization in production boot")
static void boot_progress_set(uint8_t v){(void)v;}
static void boot_selftest_list_sync_step(uint8_t v){(void)v;}
static void boot_selftest_list_set_result(uint8_t i,uint8_t v){(void)i;(void)v;}
static void boot_selftest_list_finish(void){}
static bool ui_state_pure_count_is_enabled(void){return pure;}
static void ui_manager_switch(ui_page_t p){page=p;switches++;}
static void fault_popup_record_boot_result(uint8_t step,uint8_t result){
 recorded++;machine_fault_clear_code(MACHINE_FAULT_BOOT,step);
 if(result!=1)machine_fault_report((machine_fault_key_t){MACHINE_FAULT_BOOT,result,step});
}
static bool fault_popup_show_pending_now(void){assert(machine_fault_count());faults++;return true;}
static void show_boot_selftest_error_popup(const char *text){assert(text);faults++;}
static lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t*),uint32_t ms,void *data)
{assert(cb && ms==2000 && !timer_live);timer_live=true;timer_storage.user_data=data;return &timer_storage;}
static void lv_timer_del(lv_timer_t *t){assert(t==&timer_storage && timer_live);timer_live=false;}
'''
main=r'''
int main(int argc,char **argv){
 assert(argc==2);counting_session_state_t session=1;
 if(!strcmp(argv[1],"recovery")){
 machine_fault_report((machine_fault_key_t){MACHINE_FAULT_BOOT,2,1});
 machine_fault_report((machine_fault_key_t){MACHINE_FAULT_BOOT,2,3});}
 uint8_t h[]={0xfd,0xdf,6,1,1,0};uint8_t r[]={0xfd,0xdf,7,0x37,4,1,0};
 intro=true;app_boot_runtime_handle_reply(&session,1,h,6);assert(sends==0);
 intro=false;now=8200;app_boot_runtime_poll(now,true);assert(sends==1 && last_cmd==1);
 if(!strcmp(argv[1],"timeout")){now+=60000;app_boot_runtime_handle_reply(&session,1,h,6);
 assert(sends==1);app_boot_runtime_poll(now,true);assert(faults==1);puts("PASS late timeout reply");return 0;}
 app_boot_runtime_handle_reply(&session,1,h,6);assert(last_cmd==0x37 && last_value==4);
 unsigned before=sends;app_boot_runtime_handle_reply(&session,1,h,6);assert(sends==before);
 const uint8_t order[]={4,1,2,3,5};
 for(int i=0;i<5;i++){now+=100;r[4]=order[i];r[5]=(!strcmp(argv[1],"failure") && (i==1||i==3))?2:1;
 app_boot_runtime_handle_reply(&session,0x37,r,7);assert(recorded==(unsigned)i+1);
 if(i<4)assert(faults==0);
 app_boot_runtime_handle_reply(&session,0x37,r,7);assert(recorded==(unsigned)i+1);}
 assert(last_cmd==0x56);
 if(!strcmp(argv[1],"failure")){assert(faults==1&&!timer_live&&machine_fault_count()==2);puts("PASS independent selftest failures, deferred until sequence end");return 0;}
 assert(machine_fault_count()==0);
 assert(timer_live);
 if(!strcmp(argv[1],"leave"))page=UI_PAGE_SENSOR;
 pure=!strcmp(argv[1],"pure");app_boot_runtime_finish_timer_cb(&timer_storage);
 assert(!timer_live);
 if(!strcmp(argv[1],"leave"))assert(page==UI_PAGE_SENSOR && switches==0);
 else assert(page==(pure?UI_PAGE_PURE:UI_PAGE_MAIN) && switches==1);
 before=sends;app_boot_runtime_handle_reply(&session,1,h,6);assert(sends==before);
 puts("PASS restored boot handoff/lifecycle");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-boot-recovery-') as tmp:
    src=Path(tmp)/'runtime.c';src.write_text(fixture+runtime+main)
    exe=Path(tmp)/'runtime'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root),str(src),str(root/'un260/boot/boot_reply.c'),str(root/'un260/boot/boot_service.c'),str(root/'un260/machine_state/machine_fault.c'),'-o',str(exe)],check=True)
    for scenario in ('success','pure','failure','timeout','leave','recovery'):
        subprocess.run([str(exe),scenario],check=True)
