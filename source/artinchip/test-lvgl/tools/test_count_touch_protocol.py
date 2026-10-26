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
end_generation = function(command, 'app_command_runtime_count_end_generation')
end_case = command[command.index('    case 0x0E: {'):command.index('    case 0x0F:',command.index('    case 0x0E: {'))]
end_dispatch = 'static void runtime_info(counting_session_state_t *session,counting_sim_t *sim_data,const uint8_t *buf,uint8_t len) {\nassert(session==&g_counting_session&&sim_data==&data);switch(0x0E) {\n'+end_case+'default:assert(false);}}\n'
assert source.count('page_01_main_set_counting_locked(false)') == 1
assert source.count('page_01_main_set_counting_locked(true)') == 1
head = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_multi.h"
#include "un260/innovation/multi_pass_verification.h"
static counting_sim_t data;
static counting_session_state_t g_counting_session;
static uint32_t g_count_end_generation;
static bool locked, multi, add;
static unsigned capture_starts,capture_infos;
static uint32_t capture_baseline;static uint16_t capture_issue_baseline,capture_valid;static uint8_t capture_issue,capture_status;static bool capture_add;
static void ui_page_dynamic_capture_on_count_start(uint32_t valid,uint16_t issue,bool cumulative){++capture_starts;capture_baseline=valid;capture_issue_baseline=issue;capture_add=cumulative;}
static void ui_page_dynamic_capture_on_count_info(uint16_t valid,uint8_t issue,uint8_t status){++capture_infos;capture_valid=valid;capture_issue=issue;capture_status=status;}
static unsigned locks,unlocks,restarted_groups;
static int island_pcs;
static void counting_history_restart_group(void){++restarted_groups;}
static void smart_island_update_counting(int pcs,double amount){(void)amount;island_pcs=pcs;}
/* Observe the UI publication without simulating protocol or lock events. */
static unsigned reject_notifications;
static void app_counting_runtime_on_reject_report_changed(void *context){(void)context;++reject_notifications;}
static void page_01_main_set_counting_locked(bool value){locked=value;if(value)++locks;else ++unlocks;}
bool counting_history_discard_pending(counting_session_state_t *s){(void)s;return false;}
void uart_debug_printf(const char *s,...){(void)s;}
uint32_t app_clock_uptime_ms(void){return 100;}
int protocol_send(uint8_t cmd,const uint8_t *payload,uint16_t len)
{(void)cmd;assert(payload&&len);return 0;}
#define counting_history_prepare_start(...) false
#define counting_data_current() (&data)
#define counting_data_mutable() (&data)
#define counting_data_mark_multi_result(p) ((p)->multi_currency_result=true)
#define currency_state_multi_selected() multi
#define currency_state_restore_detected_result() false
#define UI_N_(s) s
#define UI_PAGE_PURE 1
#define UI_PAGE_MAIN 2
#define DATA_COLLECT_MODE_NONE 0
#define multi_pass_verification_is_active() false
#define machine_state_add_enabled() add
#define data_collection_state_mode() 0
#define ui_manager_get_current_page() UI_PAGE_MAIN
#define work_mode_service_diagnostic_active() false
#define smart_island_pure_count_is_enabled() false
#define app_counting_runtime_main_page_active() true
#define ui_history_total_notes_counted_get() 0
#define lv_tick_get() 100
'''
noops = '''app_fault_recovery_clear diagnostic_calibration_feed_started
counting_history_begin_group page_02_list_report_reset page_01_curr_img_refre fault_popup_clear_runtime fault_popup_clear_preset counting_history_session_start
app_auto_qr_on_start multi_pass_verification_on_count_start multi_pass_verification_get_view
page_32_innovation_notify_verification_event data_collection_state_set_status page_06_data_collection_refresh
ui_manager_switch smart_island_notify_count_start app_counting_runtime_refresh_compact counting_history_append_frame
page_02_list_section_mark_dirty counting_history_capture_end smart_island_set_count_analysis
smart_island_notify_count_end app_counting_runtime_try_history_commit
ui_refresh_main_page app_auto_qr_on_end smart_island_refresh_summary'''.split()
head += '\n'.join(f'#define {name}(...) ((void)0)' for name in noops)+'\n'
test = r'''
#define session g_counting_session
int main(void){
 memset(&session,0,sizeof(session));
 counting_control_reply_hooks_t hooks={.on_start_success=app_counting_runtime_on_start_success};
 uint8_t reply[]={0xFD,0xDF,7,0x0A,1,1,0};
 uint8_t report[16]={0xFD,0xDF,13,0x0E,0,0,0,100,0,10,0,1,0};
 /* Boot and unarmed end reports never represent a completed run. */
 report[11]=2;runtime_info(&session,&data,report,13);assert(app_command_runtime_count_end_generation()==0);
 report[11]=6;runtime_info(&session,&data,report,13);assert(app_command_runtime_count_end_generation()==0);
 report[11]=1;
 /* A buffered start must lock before history finishes, without requiring
    model publication or a pending on-screen Start request. */
 assert(!delayed_start(0x0A,reply,7)&&locked);locked=false;assert(!capture_starts&&!capture_infos);
 /* Auto and physical Start enter through unsolicited 0A as well: no pending
    UI request is needed. Unknown/failed/truncated starts never acquire. */
 reply[5]=2;counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);assert(!locked);
 reply[5]=1;counting_control_reply_dispatch(0x0A,&session,reply,6,&hooks);assert(!locked);
 for(int mode=0;mode<2;++mode)for(int end=2;end<=5;++end){
  multi=mode;memset(&data,0,sizeof(data));memset(&session,0,sizeof(session));
  unsigned old_locks=locks,old_unlocks=unlocks;uint32_t old_end=app_command_runtime_count_end_generation();
  unsigned old_reject_notifications=reject_notifications,old_capture_starts=capture_starts,old_capture_infos=capture_infos;
  counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
  assert(locked&&session.start_confirmed&&locks==old_locks+1);
  assert(reject_notifications==old_reject_notifications+1);assert(capture_starts==old_capture_starts+1&&capture_baseline==0&&capture_issue_baseline==0&&!capture_add);
  report[11]=1;runtime_info(&session,&data,report,13);assert(locked);assert(capture_infos==old_capture_infos+1&&capture_valid==10&&capture_status==1);
  report[11]=6;runtime_info(&session,&data,report,13);assert(locked);
  report[11]=end;runtime_info(&session,&data,report,12);assert(locked);
  reply[4]=2;reply[5]=7;counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);assert(locked);
  uint8_t fault[]={0xFD,0xDF,6,0x0F,1,0};
  counting_control_reply_dispatch(0x0F,&session,fault,6,&hooks);assert(locked);
  fault[4]=0;counting_control_reply_dispatch(0x0F,&session,fault,6,&hooks);assert(locked);
  assert(unlocks==old_unlocks&&app_command_runtime_count_end_generation()==old_end);assert(capture_infos==old_capture_infos+1);
  runtime_info(&session,&data,report,13);
  assert(!locked&&!session.start_confirmed&&unlocks==old_unlocks+1&&app_command_runtime_count_end_generation()==old_end+1);assert(capture_infos==old_capture_infos+2&&capture_status==end);
  runtime_info(&session,&data,report,13);assert(unlocks==old_unlocks+1&&app_command_runtime_count_end_generation()==old_end+1);
  reply[4]=reply[5]=1;
 }
 uint32_t entry=app_command_runtime_count_end_generation();
 memset(&session,0,sizeof(session));memset(&data,0,sizeof(data));multi=false;report[11]=2;
 runtime_info(&session,&data,report,13);assert(app_command_runtime_count_end_generation()==entry);
 /* An unsolicited AUTO start/end in one consumer tick is visible via epoch. */
 counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
 runtime_info(&session,&data,report,13);assert(app_command_runtime_count_end_generation()==entry+1);
 runtime_info(&session,&data,report,13);assert(app_command_runtime_count_end_generation()==entry+1);
 memset(&session,0,sizeof(session));assert(app_command_runtime_count_end_generation()==entry+1);
 /* Capture gets the raw accepted per-frame status/counts, not FINISHED's
    retained previous total. Its ADD baseline is published before mutations. */
 memset(&session,0,sizeof(session));memset(&data,0,sizeof(data));multi=false;add=true;
 data.total_pcs=100;data.err_expected=2;
 counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
 assert(capture_baseline==100&&capture_issue_baseline==2&&capture_add);
 report[8]=report[9]=report[10]=report[11]=0;runtime_info(&session,&data,report,13);
 assert(capture_valid==0&&capture_issue==0&&capture_status==0);
 report[9]=101;report[10]=2;report[11]=1;runtime_info(&session,&data,report,13);
 assert(capture_valid==101&&capture_issue==2&&capture_status==1);
 report[9]=report[10]=0;report[11]=2;runtime_info(&session,&data,report,13);
 assert(capture_valid==0&&capture_issue==0&&capture_status==2&&data.total_pcs==101);
 unsigned end_infos=capture_infos;runtime_info(&session,&data,report,13);assert(capture_infos==end_infos);
 /* Captured ADD group restart, then normal cumulative continuation. */
 memset(&session,0,sizeof(session));memset(&data,0,sizeof(data));multi=false;add=true;
 data.total_pcs=83;data.total_amount=4150;
 session.last_result=(counting_pending_result_t){.valid=true,.pcs=83,.amount=4150};
 counting_report_begin(true,&data);
 counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
 memset(report+4,0,8);report[11]=1;runtime_info(&session,&data,report,13);
 assert(data.total_pcs==83&&data.last_total_pcs==83&&!restarted_groups);
 for(unsigned n=1;n<=83;++n){unsigned amount=n*50;report[6]=amount>>8;report[7]=amount;report[9]=n;runtime_info(&session,&data,report,13);}
 assert(restarted_groups==1&&data.last_total_pcs==0);
 memset(report+4,0,8);report[11]=2;runtime_info(&session,&data,report,13);
 assert(island_pcs==83&&data.total_pcs==83&&session.history_record.pcs==83);
 counting_report_begin(true,&data);
 counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
 report[11]=1;runtime_info(&session,&data,report,13);
 for(unsigned n=84;n<=166;++n){unsigned amount=n*50;report[6]=amount>>8;report[7]=amount;report[9]=n;runtime_info(&session,&data,report,13);}
 assert(restarted_groups==1&&data.last_total_pcs==83);
 memset(report+4,0,8);report[11]=2;runtime_info(&session,&data,report,13);
 assert(island_pcs==83&&data.total_pcs==166&&session.history_record.pcs==166);
 counting_report_begin(true,&data);counting_control_reply_dispatch(0x0A,&session,reply,7,&hooks);
 report[11]=1;runtime_info(&session,&data,report,13);report[11]=2;runtime_info(&session,&data,report,13);
 assert(island_pcs==0&&data.total_pcs==166&&restarted_groups==1);
 puts("PASS actual runtime ADD: controller 1->83 restart shows83, 84->166 continuation shows83, empty pass shows0; cumulative history totals preserved");
 puts("PASS exact accepted capture notifications: successful START only, raw LIVE/END status/values, ADD baseline before reset, invalid/unarmed/duplicate END ignored");
 puts("PASS accepted count-end generation: boot/duplicate/unknown ignored, AUTO start/end in one tick and reset retain epoch");
 puts("PASS exact app start/end + control/info/MUL: unsolicited start, failures, invalid/truncated/live reports, fault/reset reports, all four terminal statuses, duplicate end");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-count-touch-protocol-') as temp:
    w=Path(temp);stub=w/'un260/lv_drivers/lv_drivers.h';stub.parent.mkdir(parents=True);stub.write_text('void uart_debug_printf(const char *, ...);\n');(w/'test.c').write_text(head+preflight+start+'\n'+info+'\n'+end_generation+'\n'+end_dispatch+'\n'+test)
    sources=['un260/counting/counting_control_reply.c','un260/counting/counting_info_reply.c','un260/counting/counting_multi.c','un260/counting/counting_report_sync.c','un260/counting/counting_data_store.c']
    subprocess.run(['cc','-std=c11','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all',
                    '-ffunction-sections','-fdata-sections','-no-pie','-I'+str(w),'-I'+str(root),str(w/'test.c'),
                    *[str(root/p) for p in sources],'-Wl,--gc-sections','-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test')],check=True)
