"""Actual 0x06 dispatch branch + actual page result handler, no legacy widget."""
from pathlib import Path
import os,subprocess,tempfile
from test_multi_result_safety import function
root=Path(__file__).resolve().parents[1]
event=(root/'un260/lv_core/lv_page_event.c').read_text()
reply=(root/'un260/app_service/app_setting_reply_basic.c').read_text()
branch=reply[reply.index('    case 0x06:'):reply.index('    case 0x39:')]
actual=function(event,'page_03_batch_set_result')
assert 'batch_switch_on_0x06_result' not in reply
assert 'set_batch_switch_state' not in event
harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "un260/app_service/setting_service.h"
#include "un260/machine_state/machine_fault.h"
#define UI_N_(s) s
static bool visible,pending,enabled;
static unsigned animations,refreshes,failures,successes,confirmed,preset_reports,preset_popups;
static uint8_t number;
static setting_batch_result_t wanted;
void uart_debug_printf(const char *fmt,...){(void)fmt;}
void app_fault_recovery_report(machine_fault_key_t key){assert(key.source==MACHINE_FAULT_PRESET&&key.type==0&&key.code==4);preset_reports++;}
void fault_popup_report_preset_full(void){preset_popups++;}
void page_03_menu_clear_batch_tip(void){}
void page_03_menu_refresh_batch_number(void){assert(visible);refreshes++;}
void page_03_menu_show_batch_saved_tip(void){assert(visible);}
bool page_03_menu_is_visible(void){return visible;}
void app_setting_notice_result(const char *key,const char *title,bool ok){assert(key&&title);if(ok)successes++;else failures++;}
void machine_state_confirm_batch(bool on,uint8_t n){enabled=on;number=n;confirmed++;}
uint8_t machine_state_batch_num(void){return number;}
void page_01_bottom_c_refresh_batch(bool animate){assert(animate);animations++;}
void page_01_batch_refre(void){}
void smart_island_refresh_summary(void){}
bool setting_service_batch_take_result(uint8_t status,setting_batch_result_t *result){(void)status;if(!pending)return false;pending=false;*result=wanted;return true;}
'''
main=r'''
int main(void){
 uint8_t frame[]={0xFD,0xDF,6,6,1,0};
 for(unsigned lifecycle=0;lifecycle<4;lifecycle++){
  visible=lifecycle==1; /* never created, visible, suspended, destroyed */
  for(unsigned type=SETTING_BATCH_REQUEST_NUMBER;type<=SETTING_BATCH_REQUEST_SWITCH;type++){
   pending=true;wanted=(setting_batch_result_t){.type=type,.target={true,50}};
   unsigned old=confirmed,notices=successes,reports=preset_reports;
   frame[4]=4;dispatch(frame,5);assert(preset_reports==reports&&pending);
   dispatch(frame,sizeof(frame));assert(preset_reports==reports+1&&preset_popups==preset_reports&&pending&&confirmed==old&&successes==notices);
   /* Unsolicited Batch full never consumes the outstanding setting ACK. */
   frame[4]=1;dispatch(frame,sizeof(frame));assert(confirmed==old+1&&enabled&&number==50&&!pending&&successes==notices+1);
   old=confirmed;notices=successes;dispatch(frame,sizeof(frame));assert(confirmed==old&&successes==notices); /* duplicate ACK */
   notices=failures;pending=true;frame[4]=2;
   frame[2]=7;dispatch(frame,sizeof(frame));assert(pending&&failures==notices);
   frame[2]=6;frame[3]=0x39;dispatch(frame,sizeof(frame));assert(pending&&failures==notices);
   frame[3]=6;dispatch(frame,sizeof(frame));assert(confirmed==old&&failures==notices+1);
   dispatch(frame,sizeof(frame));assert(failures==notices+1); /* duplicate reject */
   frame[4]=1;
  }
 }
 assert(animations==8&&refreshes==2);puts("PASS actual Batch ACK: both request types, never-created/visible/hidden/destroyed Menu, Batch full separate from ACK, malformed/truncated report, failure and duplicate reply");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-batch-ack-') as tmp:
    tmp=Path(tmp);source=tmp/'test.c';exe=tmp/('test.exe' if os.name=='nt' else 'test')
    source.write_text(harness+'\n'+actual+'\nstatic void dispatch(const uint8_t *buf,uint8_t len){const uint8_t cmd=6;switch(cmd){'+branch+'}}\n'+main)
    command=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror',f'-I{root}',str(source),'-o',str(exe)]
    if os.name!='nt':command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(exe)],check=True)
