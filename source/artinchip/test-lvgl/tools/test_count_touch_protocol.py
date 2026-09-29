#!/usr/bin/env python3
"""Exact application start/end handlers with production control/info/MUL parsers.

Only unrelated UI/history/diagnostic side effects are stubbed. There is no
simulated timer unlock and no copied protocol-to-touch routing.
"""
from pathlib import Path
import subprocess, tempfile
from test_main_view import function

root = Path(__file__).resolve().parents[1]
source = (root/'un260/app_service/app_counting_runtime.c').read_text()
start = function(source, 'app_counting_runtime_on_start_success')
info = function(source, 'app_counting_runtime_handle_info')
command = (root/'un260/app_service/app_command_runtime.c').read_text()
preflight = command[command.index('    if (cmd == 0x0A && len >= 7'):command.index('    if (cmd == 0x03 && len >= 6')]
preflight = 'static bool delayed_start(uint8_t cmd,uint8_t *buf,uint8_t len) {\n'+preflight+'\nreturn true;\n}\n'
assert source.count('page_01_main_set_counting_locked(false)') == 1
assert source.count('page_01_main_set_counting_locked(true)') == 1
head = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_multi.h"
#include "un260/innovation/multi_pass_verification.h"
static counting_sim_t data;
static bool locked, multi;
static bool g_start_waiting_for_clear;
static unsigned locks,unlocks;
static void page_01_main_set_counting_locked(bool value){locked=value;if(value)++locks;else ++unlocks;}
bool counting_history_discard_pending(counting_session_state_t *s){(void)s;return false;}
void uart_debug_printf(const char *s,...){(void)s;}
#define counting_history_prepare_start(...) false
#define counting_data_current() (&data)
#define counting_data_mutable() (&data)
#define counting_data_mark_multi_result(p) ((p)->multi_currency_result=true)
#define currency_state_multi_selected() multi
#define UI_N_(s) s
#define UI_PAGE_PURE 1
#define UI_PAGE_MAIN 2
#define DATA_COLLECT_MODE_NONE 0
#define multi_pass_verification_is_active() false
#define machine_state_add_enabled() false
#define data_collection_state_mode() 0
#define ui_manager_get_current_page() UI_PAGE_MAIN
#define work_mode_service_diagnostic_active() false
#define app_counting_runtime_main_page_active() true
#define ui_history_total_notes_counted_get() 0
#define lv_tick_get() 100
'''
noops = '''app_fault_recovery_count_started app_fault_recovery_count_finished diagnostic_calibration_feed_started currency_state_begin_count_session
page_02_list_report_reset page_01_curr_img_refre fault_popup_clear_runtime counting_history_session_start
app_auto_qr_on_start multi_pass_verification_on_count_start multi_pass_verification_get_view
page_32_innovation_notify_verification_event data_collection_state_set_status page_06_data_collection_refresh
ui_manager_switch smart_island_notify_count_start app_counting_runtime_refresh_compact counting_history_append_frame
page_02_list_section_mark_dirty counting_history_capture_end smart_island_set_count_analysis
smart_island_update_counting smart_island_notify_count_end app_counting_runtime_try_history_commit
ui_refresh_main_page app_counting_runtime_schedule_auto_wave app_auto_qr_on_end'''.split()
head += '\n'.join(f'#define {name}(...) ((void)0)' for name in noops)+'\n'
test = r'''
int main(void){
 counting_session_state_t session={0};
 counting_control_reply_hooks_t hooks={.on_start_success=app_counting_runtime_on_start_success};
 uint8_t reply[]={0xFD,0xDF,7,0x0A,1,1,0};
 uint8_t report[16]={0xFD,0xDF,13,0x0E,0,0,0,100,0,10,0,1,0};
 /* A buffered start must lock before history finishes, without requiring
    model publication or a pending on-screen Start request. */
 assert(!delayed_start(0x0A,reply,7)&&locked);locked=false;
 /* Auto and physical Start enter through unsolicited 0A as well: no pending
    UI request is needed. Unknown/failed/truncated starts never acquire. */
 reply[5]=2;counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);assert(!locked);
 reply[5]=1;counting_control_reply_dispatch(0x0A,&session,reply,6,&hooks);assert(!locked);
 for(int mode=0;mode<2;++mode)for(int end=2;end<=5;++end){
  multi=mode;memset(&data,0,sizeof(data));memset(&session,0,sizeof(session));
  unsigned old_locks=locks,old_unlocks=unlocks;
  counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
  assert(locked&&session.start_confirmed&&locks==old_locks+1);
  report[11]=1;app_counting_runtime_handle_info(&session,&data,report,13);assert(locked);
  report[11]=6;app_counting_runtime_handle_info(&session,&data,report,13);assert(locked);
  report[11]=end;app_counting_runtime_handle_info(&session,&data,report,12);assert(locked);
  reply[4]=2;reply[5]=7;counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);assert(locked);
  uint8_t fault[]={0xFD,0xDF,6,0x0F,1,0};
  counting_control_reply_dispatch(0x0F,&session,fault,6,&hooks);assert(locked);
  fault[4]=0;counting_control_reply_dispatch(0x0F,&session,fault,6,&hooks);assert(locked);
  assert(unlocks==old_unlocks);
  app_counting_runtime_handle_info(&session,&data,report,13);
  assert(!locked&&!session.start_confirmed&&unlocks==old_unlocks+1);
  app_counting_runtime_handle_info(&session,&data,report,13);assert(unlocks==old_unlocks+1);
  reply[4]=reply[5]=1;
 }
 puts("PASS exact app start/end + control/info/MUL: unsolicited start, failures, invalid/truncated/live reports, fault/reset reports, all four terminal statuses, duplicate end");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-count-touch-protocol-') as temp:
    w=Path(temp);(w/'test.c').write_text(head+preflight+start+'\n'+info+'\n'+test)
    sources=['un260/counting/counting_control_reply.c','un260/counting/counting_info_reply.c','un260/counting/counting_multi.c']
    subprocess.run(['cc','-std=c11','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
                    '-ffunction-sections','-fdata-sections','-no-pie','-I'+str(root),str(w/'test.c'),
                    *[str(root/p) for p in sources],'-Wl,--gc-sections','-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test')],check=True)
