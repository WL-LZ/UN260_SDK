#!/usr/bin/env python3
"""Execute production runtime-fault routing; presentation sinks are spies."""
from pathlib import Path
import subprocess
import tempfile
from test_main_view import function

root=Path(__file__).resolve().parents[1]
parts=[('un260/machine_state/machine_state.c','machine_runtime_error_desc'),
       ('un260/lv_components/lv_components.c','get_system_error_desc'),
       ('un260/app_service/app_counting_runtime.c','app_counting_runtime_on_runtime_fault')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum {SMART_ISLAND_WARNING_LEVEL_ERROR};
static unsigned popups,notices,clears,restores;static uint8_t pending;
static uint32_t now;static char message[80];
static void fault_popup_clear_runtime(void){pending=0;++clears;}
static void smart_island_restore_idle(void){++restores;}
static void fault_popup_record_runtime_notice(uint8_t c){pending=c;}
static void fault_popup_report_runtime_fault(uint8_t c){pending=c;++popups;}
static void uart_debug_printf(const char *s,...){(void)s;}
static uint32_t app_clock_uptime_ms(void){return now;}
static void app_counting_runtime_notice(const char *s){++notices;snprintf(message,sizeof(message),"%s",s);}
'''
code+='\n'.join(function((root/path).read_text(),name) for path,name in parts)
code+=r'''
int main(void){
 for(unsigned c=0;c<=7;++c)assert(machine_runtime_error_desc(c));
 for(unsigned c=8;c<256;++c)assert(!machine_runtime_error_desc(c));
 app_counting_runtime_on_runtime_fault(8);
 assert(pending==8 && notices==1 && !popups);
 assert(!strcmp(message,"Controller report 0x08"));
 now=100;app_counting_runtime_on_runtime_fault(8);assert(notices==1);
 now=30000;app_counting_runtime_on_runtime_fault(8);assert(notices==2);
 app_counting_runtime_on_runtime_fault(9);assert(notices==3 && pending==9);
 app_counting_runtime_on_runtime_fault(1);assert(popups==1 && pending==1);
 app_counting_runtime_on_runtime_fault(0);assert(!pending && clears==1 && restores==1);
 app_counting_runtime_on_runtime_fault(8);assert(notices==4);
 puts("PASS runtime faults: known machine guide retained; unmapped raw code retained, throttled non-modal notice, no island takeover, clear/reoccurrence");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-runtime-fault-') as directory:
    work=Path(directory);source=work/'test.c';source.write_text(code);binary=work/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-no-pie',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
