#!/usr/bin/env python3
"""Production batch reply branch: async full is separate from setting results."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'un260/app_service/app_setting_reply_basic.c').read_text()
branch=source[source.index('    case 0x06:'):source.index('    case 0x39:')]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include "un260/app_service/setting_service.h"
#include "un260/machine_state/machine_fault.h"
static bool pending=true;
static unsigned taken,full,recovery,setting_notice,boot_value,results;
#define UI_N_(s) s
bool setting_service_batch_take_result(uint8_t status,setting_batch_result_t *out){(void)out;assert(status==1||status==2);if(!pending)return false;pending=false;++taken;return true;}
void page_03_batch_set_result(bool success,const setting_batch_result_t *out){(void)success;(void)out;++results;}
void app_setting_notice_result(const char *k,const char *t,bool success){(void)k;(void)t;(void)success;++setting_notice;}
void uart_debug_printf(const char *f,...){(void)f;}
void smart_island_refresh_summary(void){}
void machine_state_confirm_batch(bool enabled,uint8_t n){(void)enabled;boot_value=n;}
int machine_state_batch_num(void){return boot_value;}
bool page_03_menu_is_visible(void){return false;}
void page_03_menu_refresh_batch_number(void){}
void page_01_batch_refre(void){}
void app_fault_recovery_report(machine_fault_key_t k){assert(k.source==MACHINE_FAULT_BATCH&&k.code==4);++recovery;}
void fault_popup_report_batch_full(void){++full;}
static void handle(const uint8_t *buf,uint8_t len){switch(0x06){
'''+branch+r'''
}}
int main(void){
 uint8_t b[]={0xFD,0xDF,6,6,4,0,0};
 handle(b,6);assert(full==1&&recovery==1&&pending&&!taken&&!results&&!setting_notice&&!boot_value);
 handle(b,6);assert(full==2&&pending);handle(b,5);handle(b,7);assert(full==2);
 b[4]=1;handle(b,6);assert(!pending&&taken==1&&results==1&&setting_notice==1);
 handle(b,6);assert(taken==1);
 pending=true;b[4]=4;handle(b,6);assert(full==3&&pending);
 b[4]=2;handle(b,6);assert(!pending&&taken==2&&results==2&&setting_notice==2);
 b[4]=3;b[5]=100;handle(b,7);assert(boot_value==100&&full==3);
 puts("PASS actual 06 branch: 04 full notification cannot consume pending settings, duplicate/new event, exact length, 01/02 results and 03 boot value preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-batch-full-') as temp:
    w=Path(temp);(w/'test.c').write_text(code)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',
                    '-I'+str(root),str(w/'test.c'),'-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test')],check=True)
