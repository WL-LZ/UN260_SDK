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
static bool visible,pending,enabled;
static unsigned animations,refreshes,failures,successes,confirmed;
static uint8_t number;
static setting_batch_result_t wanted;
void uart_debug_printf(const char *fmt,...){(void)fmt;}
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
   unsigned old=confirmed,notices=successes;dispatch(frame,sizeof(frame));assert(confirmed==old+1&&enabled&&number==50&&!pending&&successes==notices+1);
   old=confirmed;notices=successes;dispatch(frame,sizeof(frame));assert(confirmed==old&&successes==notices); /* duplicate ACK */
   notices=failures;pending=true;frame[4]=2;dispatch(frame,sizeof(frame));assert(confirmed==old&&failures==notices+1);
   dispatch(frame,sizeof(frame));assert(failures==notices+1); /* duplicate reject */
   frame[4]=1;
  }
 }
 assert(animations==8&&refreshes==2);puts("PASS actual Batch ACK: both request types, never-created/visible/hidden/destroyed Menu, failure and duplicate reply");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-batch-ack-') as tmp:
    tmp=Path(tmp);source=tmp/'test.c';exe=tmp/('test.exe' if os.name=='nt' else 'test')
    source.write_text(harness+'\n'+actual+'\nstatic void dispatch(const uint8_t *buf,uint8_t len){switch(6){'+branch+'}}\n'+main)
    command=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror',f'-I{root}',str(source),'-o',str(exe)]
    if os.name!='nt':command+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(exe)],check=True)
