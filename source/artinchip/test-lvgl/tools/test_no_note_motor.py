#!/usr/bin/env python3
"""Replay empty START through the production dispatcher, control/info parsers.

Unrelated device/UI services are stubs; counters detect accidental start,
history/report mutation, finished animation, and denomination publication.
"""
from pathlib import Path
import subprocess, tempfile
from test_main_view import function
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'un260/app_service/app_command_runtime.c').read_text()
actual = '\n'.join(function(source, name) for name in (
    'app_command_runtime_count_start_busy', 'app_command_runtime_handle_stacker_clear',
    'app_command_runtime_handle_no_note_motion', 'app_command_runtime_dispatch'))
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_action_service.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_detail_state.h"
#include "un260/protocol/protocol_frame.h"
static counting_session_state_t g_counting_session;
static counting_detail_state_t g_counting_detail_state;
static counting_sim_t data;
static bool g_no_note_motor_active, g_no_note_denom_pending, diagnostic, locked, multi_query;
static bool g_add_result_cleared;
static bool add_commit_deferred;
static bool app_add_runtime_before_count_start(void){return !add_commit_deferred;}
static void app_add_runtime_poll(void){}
static bool counting_denom_query_complete(counting_detail_state_t *state)
{assert(state==&g_counting_detail_state);state->query_pending=false;return true;}

static uint32_t g_count_end_generation;
static bool multi, warning_visible;
static struct { bool counting; } multi_model;
typedef struct { int domain, type, code; } machine_fault_key_t;
#define MACHINE_FAULT_START 1
#define currency_state_auto_selected() false
#define currency_state_multi_selected() multi
#define currency_state_clear_detected_result() ((void)0)
#define page_01_curr_img_refre() ((void)0)
#define counting_multi_current() (&multi_model)
#define app_counting_runtime_reset_session(...) (true)
#define sim_reset_counting_result(...) ((void)0)
#define counting_data_mark_multi_result(...) ((void)0)

static unsigned resets, faults, warnings, starts, ends, prepares, reports, denoms, stacker;
uint64_t app_clock_monotonic_ms(void){return 100;}
bool counting_history_can_start(void){return true;}
int protocol_send(uint8_t cmd,const uint8_t *payload,uint16_t len){
 assert(cmd==0x0A&&len==1&&payload[0]==1);return 6;
}
static bool work_mode_service_diagnostic_active(void){return diagnostic;}
static bool counting_multi_query_busy(void){return multi_query;}
static void page_01_main_set_counting_locked(bool v){locked=v;}
static void smart_island_notify_count_reset(void){++resets;}
static void fault_popup_report_boot_result(uint8_t step,uint8_t result){assert(step==2 && result==2);++faults;}
bool counting_history_discard_pending(counting_session_state_t *s){(void)s;return false;}
void uart_debug_printf(const char *s,...){(void)s;}
static void started(const uint8_t *b,uint8_t l){(void)b;(void)l;++starts;locked=true;}
static void failed(uint8_t t,uint8_t c){if(t==1&&c==2){++warnings;warning_visible=true;}}
static void app_counting_runtime_handle_control(uint8_t cmd,counting_session_state_t *s,const uint8_t *b,uint8_t l){
 counting_control_reply_hooks_t h={.on_start_success=started,.on_start_failure=failed};
 counting_control_reply_dispatch(cmd,s,b,l,&h);
}
static void app_counting_runtime_handle_info(counting_session_state_t *s,counting_sim_t *d,const uint8_t *b,uint8_t l){
 if(l!=13)return;
 counting_info_reply_result_t r=counting_info_reply_handle(s,d,b,l,0);
 if(r.kind==COUNTING_INFO_REPLY_FINISHED){++ends;locked=false;}
}
#define counting_data_mutable() (&data)
#define app_clock_uptime_ms() 100
#define machine_state_add_enabled() false
#define counting_history_prepare_start(...) (++prepares,true)
#define counting_history_prepare_reset(...) true
#define counting_report_begin(...) (++reports)
#define counting_report_discard_stale(...) false
#define app_fault_recovery_observe_setting_detail(...) false
#define app_protocol_runtime_handle_reply(...) false
#define app_setting_runtime_handle_reply(...) false
#define app_fault_recovery_stacker_cleared() (++stacker)
#define app_fault_recovery_handle_clear_result(...) ((void)0)
#define app_currency_runtime_handle_detected(...) ((void)0)
#define app_counting_runtime_handle_detail(...) ((void)0)
#define app_boot_runtime_handle_reply(...) ((void)0)
static bool accepted_currency;
static void app_currency_runtime_handle_reply(counting_detail_state_t *detail,
    counting_session_state_t *session,const uint8_t *buf,uint8_t len)
{
    (void)session;(void)buf;(void)len;
    if(accepted_currency){detail->query_wait_push=true;detail->query_expired=false;}
}
#define app_counting_runtime_handle_denom(...) (++denoms)
'''
cases = r'''
static void dispatch(uint8_t *f){assert(app_command_runtime_dispatch(f[3],f,f[2]));}
static void reset(void){
 counting_action_cancel_all();
 memset(&g_counting_session,0,sizeof(g_counting_session));
 memset(&g_counting_detail_state,0,sizeof(g_counting_detail_state));
 g_no_note_motor_active=g_no_note_denom_pending=diagnostic=locked=multi_query=multi=warning_visible=g_add_result_cleared=false;
 add_commit_deferred=accepted_currency=false;
 resets=faults=warnings=starts=ends=prepares=reports=denoms=stacker=g_count_end_generation=0;
 data.total_pcs=23;data.total_amount=2300;data.denom_number=1;data.denom[0].pcs=23;
}
int main(void){
 uint8_t no_note[]={0xFD,0xDF,7,0x0A,1,2,0x75};
 uint8_t start[]={0xFD,0xDF,7,0x0A,1,1,0x26};
 uint8_t clear[]={0xFD,0xDF,6,0x51,1,0x24};
 uint8_t live[]={0xFD,0xDF,13,0x0E,0,0,0,0,0,0,0,0,0x48};
 uint8_t motor[]={0xFD,0xDF,7,0x37,2,1,0xEB};
 uint8_t denom[]={0xFD,0xDF,16,0x0B,0,0,0,0,0,0,0,0,0,0,0,5};
 reset();assert(counting_action_request_start());dispatch(no_note);
 assert(warnings==1&&locked&&!counting_action_start_pending());
 dispatch(clear);dispatch(clear);assert(stacker==2&&locked);
 dispatch(start);dispatch(start);assert(locked&&app_command_runtime_count_start_busy());
 assert(!starts&&!prepares&&!reports&&!g_counting_session.start_confirmed);
 dispatch(live);live[11]=1;dispatch(live);assert(locked&&resets==0);
 live[11]=2;dispatch(live);assert(!locked&&!app_command_runtime_count_start_busy()&&resets==1);
 dispatch(live);assert(resets==1&&!ends&&!g_count_end_generation);
 dispatch(denom);memcpy(denom+4,"     100  0",11);dispatch(denom);
 memset(denom+4,255,11);dispatch(denom);
 assert(!denoms&&!g_no_note_denom_pending&&!starts&&!ends);
 assert(data.total_pcs==23&&data.total_amount==2300&&data.denom[0].pcs==23);
 /* NO NOTE followed by an explicit START refusal aborts empty motion.
  * Confirm only acknowledges the fault: no motor-end frame is guaranteed. */
 for(unsigned type=1;type<=2;++type){
  reset();assert(counting_action_request_start());dispatch(no_note);
  uint8_t fault[]={0xFD,0xDF,7,0x0A,type,type==1?3:1,0};
  dispatch(fault);
  assert(!locked&&!app_command_runtime_count_start_busy()&&resets==1);
  assert(!starts&&!prepares&&!reports&&!g_counting_session.start_confirmed);
  dispatch(fault);dispatch(clear);assert(!locked&&resets==1);
  /* Late empty snapshots cannot replace the previous result. */
  live[11]=2;dispatch(live);dispatch(denom);
  assert(!denoms&&data.total_pcs==23&&data.total_amount==2300);
 }
 /* Replay the supplied upper-channel failure, then the next hardware run. */
 reset();assert(counting_action_request_start());dispatch(no_note);
 uint8_t upper[]={0xFD,0xDF,7,0x0A,2,1,0x0B};dispatch(upper);
 assert(!locked&&!app_command_runtime_count_start_busy());
 dispatch(clear);dispatch(clear);
 /* Next genuine run must still start, count, and finish once. */
 dispatch(start);assert(starts==1&&prepares==1&&reports==1&&locked);
 live[9]=3;live[11]=1;dispatch(live);assert(data.total_pcs==3&&locked);
 live[11]=2;dispatch(live);dispatch(live);assert(ends==1&&!locked&&g_count_end_generation==1);
 /* MULTI clear keeps its data transition without hiding the no-note warning. */
 reset();multi=true;dispatch(no_note);dispatch(clear);dispatch(clear);
 assert(stacker==2&&locked&&warning_visible);
 dispatch(start);assert(!starts&&warning_visible);live[11]=2;dispatch(live);
 assert(!locked&&warning_visible&&!ends);
 /* Legacy end, failure, duplicates, malformed/unknown frames. */
 reset();dispatch(no_note);motor[4]=1;dispatch(motor);assert(locked);
 motor[4]=2;motor[5]=0;dispatch(motor);assert(locked);
 assert(!app_command_runtime_handle_no_note_motion(0x37,motor,6)&&locked);
 live[11]=6;dispatch(live);assert(locked);
 motor[5]=1;dispatch(motor);dispatch(motor);assert(!locked&&resets==1);
 dispatch(no_note);motor[5]=2;dispatch(motor);assert(!locked&&faults==1&&resets==2);
 /* Each legal 13-byte terminal status releases only the motion gate. */
 for(unsigned status=2;status<=5;++status){reset();dispatch(no_note);dispatch(start);
  live[11]=status;dispatch(live);assert(!locked&&resets==1&&!ends&&!starts);}
 /* MULTI currency live frames are not motor terminal reports. */
 reset();dispatch(no_note);uint8_t mul[16]={0xFD,0xDF,16,0x0E,'U','S','D'};
 mul[14]=1;dispatch(mul);mul[14]=2;dispatch(mul);assert(locked);
 /* Diagnostics and real running sessions keep their existing handlers. */
 reset();diagnostic=true;dispatch(no_note);dispatch(start);assert(starts==1&&!g_no_note_motor_active);
 reset();dispatch(start);dispatch(no_note);dispatch(upper);
 assert(!g_no_note_motor_active&&locked&&g_counting_session.start_confirmed);
 /* Unknown or truncated failures do not prove empty motion was aborted. */
 reset();dispatch(no_note);upper[4]=3;dispatch(upper);
 assert(locked&&g_no_note_motor_active&&!resets);upper[4]=2;
 assert(!app_command_runtime_handle_no_note_motion(0x0A,upper,6));
 assert(locked&&g_no_note_motor_active);
 /* Truncated / invalid headers cannot arm empty motion. */
 reset();assert(!app_command_runtime_handle_no_note_motion(0x0A,no_note,6));
 no_note[0]=0;assert(!app_command_runtime_handle_no_note_motion(0x0A,no_note,7));no_note[0]=0xFD;
 assert(!g_no_note_motor_active);
 /* Missing unsolicited detail must not steal an explicit query or next START. */
 dispatch(no_note);live[11]=2;dispatch(live);g_counting_detail_state.query_pending=true;
 dispatch(denom);assert(denoms==1&&!g_no_note_denom_pending);
 reset();dispatch(no_note);dispatch(live);multi_query=true;dispatch(denom);assert(denoms==1);
 reset();dispatch(no_note);dispatch(live);dispatch(start);assert(starts==1&&!g_no_note_denom_pending);
 /* All defined mechanical START failures, both currency layouts, direct
  * refusal or after NO NOTE. The real action/request owners must settle. */
 for(unsigned layout=0;layout<2;++layout)
 for(unsigned empty=0;empty<2;++empty)
 for(unsigned code=1;code<=13;++code){
  reset();multi=layout;assert(counting_action_request_start());locked=true;
  if(empty)dispatch(no_note);
  uint8_t fault[]={0xFD,0xDF,7,0x0A,2,code,0};dispatch(fault);
  assert(!locked&&!counting_action_start_pending()&&!app_command_runtime_count_start_busy());
  assert(resets==empty&&!starts&&!prepares&&!reports&&!ends);
  assert(data.total_pcs==23&&data.total_amount==2300);
  dispatch(fault);assert(resets==empty&&!locked);
  /* A genuine subsequent START must own the run, including after a fault. */
  assert(counting_action_request_start());dispatch(start);
  assert(locked&&starts==1&&g_counting_session.start_confirmed&&!g_no_note_motor_active);
  live[9]=3;live[11]=1;dispatch(live);
  assert(locked&&g_counting_session.phase==COUNTING_SESSION_ACTIVE);
  /* A late START refusal cannot release a real running session's lock. */
  dispatch(fault);assert(locked&&app_command_runtime_count_start_busy());
  live[11]=2;dispatch(live);dispatch(live);
  assert(!locked&&!app_command_runtime_count_start_busy()&&ends==1&&g_count_end_generation==1);
 }
 /* ADD clearing quarantines prior streams until a genuine new owner exists.
    Failed/unsolicited currency replies and idle AUTO detection are not owners. */
 reset();g_add_result_cleared=true;
 uint8_t currency_failed[]={0xFD,0xDF,6,0x03,2,0};
 uint8_t currency_success[]={0xFD,0xDF,6,0x03,1,0};
 uint8_t detected[]={0xFD,0xDF,8,0x50,'U','S','D',0};
 uint8_t old_denom[16]={0xFD,0xDF,16,0x0B};
 uint8_t old_serial[24]={0xFD,0xDF,24,0x49};
 dispatch(currency_failed);assert(g_add_result_cleared);
 dispatch(currency_success);assert(g_add_result_cleared);
 dispatch(detected);assert(g_add_result_cleared);
 detected[3]=0x49;dispatch(detected);assert(g_add_result_cleared);
 dispatch(old_denom);dispatch(old_serial);dispatch(live);
 assert(!denoms&&!ends&&data.total_pcs==23&&g_add_result_cleared);
 g_counting_detail_state.query_pending=g_counting_detail_state.query_expired=true;
 memset(old_denom+4,0xFF,11);dispatch(old_denom);
 assert(!g_counting_detail_state.query_pending&&g_add_result_cleared&&!denoms);
 accepted_currency=true;dispatch(currency_success);assert(!g_add_result_cleared);
 dispatch(old_denom);assert(denoms==1);
 /* A confirmed CLEAR waiting for its history commit retains the exact START
    frame. Replaying once ready opens the new generation, never an old frame. */
 reset();g_add_result_cleared=true;add_commit_deferred=true;
 assert(!app_command_runtime_dispatch(0x0A,start,sizeof(start)));
 assert(!starts&&!reports&&!prepares&&g_add_result_cleared);
 add_commit_deferred=false;dispatch(start);
 assert(starts==1&&reports==1&&prepares==1&&!g_add_result_cleared);
 puts("PASS ADD clear dispatcher: failed/unsolicited03 and idle AUTO detection retain old-frame quarantine; late 0B/49/0E ignored, query END drains, accepted currency transition/new START reopen, commit backpressure preserves START order");
 puts("PASS full dispatcher + real START request owner: 52 fault/layout/order cases, active-session protection, supplied empty-motion sequence, prior result preservation, legacy end/fault, malformed/duplicate replies, query ownership, diagnostics and next real count");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-no-note-motor-') as directory:
    work = Path(directory); harness = work/'test.c'; harness.write_text(fixture+actual+cases)
    exe=work/'test'
    sources=['un260/protocol/protocol_frame.c','un260/counting/counting_control_reply.c','un260/counting/counting_info_reply.c',
             'un260/counting/counting_action_service.c','un260/protocol/protocol_request.c']
    subprocess.run(['cc','-std=c11','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
        '-I'+str(ROOT),str(harness),*[str(ROOT/p) for p in sources],'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
