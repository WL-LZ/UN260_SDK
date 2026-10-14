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
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_detail_state.h"
#include "un260/protocol/protocol_frame.h"
static counting_session_state_t g_counting_session;
static counting_detail_state_t g_counting_detail_state;
static counting_sim_t data;
static bool g_no_note_motor_active, g_no_note_denom_pending, pending, diagnostic, locked, multi_query;
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
static bool counting_action_start_pending(void){return pending;}
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
#define counting_action_handle_reply(...) (pending=false)
#define app_fault_recovery_observe_setting_detail(...) false
#define app_protocol_runtime_handle_reply(...) false
#define app_setting_runtime_handle_reply(...) false
#define app_fault_recovery_stacker_cleared() (++stacker)
#define app_fault_recovery_handle_clear_result(...) ((void)0)
#define app_currency_runtime_handle_detected(...) ((void)0)
#define app_counting_runtime_handle_detail(...) ((void)0)
#define app_boot_runtime_handle_reply(...) ((void)0)
#define app_currency_runtime_handle_reply(...) ((void)0)
#define app_counting_runtime_handle_denom(...) (++denoms)
'''
cases = r'''
static void dispatch(uint8_t *f){assert(app_command_runtime_dispatch(f[3],f,f[2]));}
static void reset(void){
 memset(&g_counting_session,0,sizeof(g_counting_session));
 memset(&g_counting_detail_state,0,sizeof(g_counting_detail_state));
 g_no_note_motor_active=g_no_note_denom_pending=pending=diagnostic=locked=multi_query=multi=warning_visible=false;
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
 reset();pending=true;dispatch(no_note);assert(warnings==1&&locked&&!pending);
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
 reset();dispatch(start);dispatch(no_note);assert(!g_no_note_motor_active&&locked);
 /* Truncated / invalid headers cannot arm empty motion. */
 reset();assert(!app_command_runtime_handle_no_note_motion(0x0A,no_note,6));
 no_note[0]=0;assert(!app_command_runtime_handle_no_note_motion(0x0A,no_note,7));no_note[0]=0xFD;
 assert(!g_no_note_motor_active);
 /* Missing unsolicited detail must not steal an explicit query or next START. */
 dispatch(no_note);live[11]=2;dispatch(live);g_counting_detail_state.query_pending=true;
 dispatch(denom);assert(denoms==1&&!g_no_note_denom_pending);
 reset();dispatch(no_note);dispatch(live);multi_query=true;dispatch(denom);assert(denoms==1);
 reset();dispatch(no_note);dispatch(live);dispatch(start);assert(starts==1&&!g_no_note_denom_pending);
 puts("PASS full dispatcher: supplied empty-motion sequence, prior result preservation, no count hooks, legacy end/fault, malformed/duplicate replies, query ownership, diagnostics and next real count");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-no-note-motor-') as directory:
    work = Path(directory); harness = work/'test.c'; harness.write_text(fixture+actual+cases)
    exe=work/'test'
    sources=['un260/protocol/protocol_frame.c','un260/counting/counting_control_reply.c','un260/counting/counting_info_reply.c']
    subprocess.run(['cc','-std=c11','-g','-Wall','-Wextra','-Werror',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
        '-I'+str(ROOT),str(harness),*[str(ROOT/p) for p in sources],'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
