#!/usr/bin/env python3
"""Execute production 0x51 adapter with real frame validation."""
from pathlib import Path
import subprocess
import tempfile
from test_multi_result_safety import function
root = Path(__file__).resolve().parents[1]
source = (root/'un260/app_service/app_command_runtime.c').read_text()
body = function(source, 'app_command_runtime_handle_stacker_clear')
dispatch = function(source, 'app_command_runtime_dispatch')
assert dispatch.index('if (cmd == 0x51) return app_command_runtime_handle_stacker_clear(buf, len);') < dispatch.index('counting_action_handle_reply(')
code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "un260/counting/counting_session_state.h"
#include "un260/counting/counting_multi.h"
#include "un260/counting/counting_data_types.h"
#include "un260/protocol/protocol_frame.h"
static counting_session_state_t g_counting_session;
static struct {bool wait_sn_after_reject_end;} g_counting_detail_state;
static counting_sim_t sim;
static counting_multi_t model;
static bool multi,save_ok=true;
static unsigned resets,clears,warnings;
static bool currency_state_multi_selected(void){return multi;}
const counting_multi_t *counting_multi_current(void){return &model;}
static bool app_counting_runtime_reset_session(counting_session_state_t*s,const char*r){
 (void)r;if(!save_ok)return false;memset(s,0,sizeof(*s));resets++;return true;
}
static counting_sim_t *counting_data_mutable(void){return &sim;}
static void sim_reset_counting_result(counting_sim_t*s){memset(s,0,sizeof(*s));clears++;}
static void counting_data_mark_multi_result(counting_sim_t*s){s->multi_currency_result=true;}
static void uart_debug_printf(const char *s){assert(s);warnings++;}
''' + body + r'''
int main(void){
 uint8_t frame[]={0xFD,0xDF,6,0x51,1,0};
 sim.total_pcs=28;
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 assert(!resets && sim.total_pcs==28);
 multi=true;
 assert(app_command_runtime_handle_stacker_clear(NULL,6));
 assert(app_command_runtime_handle_stacker_clear(frame,5));
 frame[2]=7;assert(app_command_runtime_handle_stacker_clear(frame,6));frame[2]=6;
 frame[3]=0x50;assert(app_command_runtime_handle_stacker_clear(frame,6));frame[3]=0x51;
 frame[4]=0;assert(app_command_runtime_handle_stacker_clear(frame,6));frame[4]=1;
 assert(!resets&&!clears&&sim.total_pcs==28);
 save_ok=false;assert(!app_command_runtime_handle_stacker_clear(frame,6));
 assert(sim.total_pcs==28&&!resets&&!clears);
 save_ok=true;g_counting_session.start_confirmed=true;
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 g_counting_session.start_confirmed=false;model.counting=true;
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 model.counting=false;g_counting_session.phase=COUNTING_SESSION_ACTIVE;
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 assert(warnings==3&&!resets&&sim.total_pcs==28);
 g_counting_session.phase=COUNTING_SESSION_FINISHED_WAIT_START;
 g_counting_detail_state.wait_sn_after_reject_end=true;
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 assert(resets==1&&clears==1&&sim.total_pcs==0&&sim.multi_currency_result);
 assert(!g_counting_detail_state.wait_sn_after_reject_end);
 assert(app_command_runtime_handle_stacker_clear(frame,6));
 assert(sim.total_pcs==0&&sim.multi_currency_result);
 puts("PASS 0x51/6: length/value/mode guards, history backpressure, out-of-order guard, clear and duplicate notification");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-stacker-wire-') as directory:
    work = Path(directory)
    src = work/'event.c'
    src.write_text(code)
    for opt in ('-O0', '-O2'):
        binary = work/'test'
        subprocess.run(['cc','-std=c11',opt,'-Wall','-Wextra','-Werror','-fsanitize=undefined',
                        '-I'+str(root),str(src),str(root/'un260/protocol/protocol_frame.c'),
                        '-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
