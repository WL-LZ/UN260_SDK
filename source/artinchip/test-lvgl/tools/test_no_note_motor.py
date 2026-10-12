#!/usr/bin/env python3
"""Exercise the production empty-feeder gate, without a timer-based unlock."""
from pathlib import Path
import subprocess
import tempfile
from test_main_view import function

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'un260/app_service/app_command_runtime.c').read_text()
actual = '\n'.join(function(source, name) for name in (
    'app_command_runtime_count_start_busy', 'app_command_runtime_handle_no_note_motion'))
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/counting/counting_session_state.h"
#include "un260/protocol/protocol_frame.h"
static counting_session_state_t g_counting_session;
static bool g_no_note_motor_active, pending, diagnostic, locked;
static unsigned resets, faults;
static bool counting_action_start_pending(void){return pending;}
static bool work_mode_service_diagnostic_active(void){return diagnostic;}
static void page_01_main_set_counting_locked(bool value){locked=value;}
static void smart_island_notify_count_reset(void){++resets;}
static void fault_popup_report_boot_result(uint8_t step,uint8_t result)
{assert(step==2 && result==2);++faults;}
'''
cases = r'''
int main(void){
 uint8_t no_note[]={0xFD,0xDF,7,0x0A,1,2,0x75};
 uint8_t motor[]={0xFD,0xDF,7,0x37,2,1,0xEB};
 assert(!app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(!locked && !resets && !app_command_runtime_count_start_busy());
 assert(!app_command_runtime_handle_no_note_motion(0x0A,no_note,7));
 assert(locked && g_no_note_motor_active && app_command_runtime_count_start_busy());
 motor[4]=1;assert(!app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(locked && !resets);
 motor[4]=2;motor[5]=0;assert(!app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(locked && !resets);
 motor[5]=1;assert(!app_command_runtime_handle_no_note_motion(0x37,motor,6));
 assert(locked && !resets);
 assert(app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(!locked && !app_command_runtime_count_start_busy() && resets==1);
 assert(!app_command_runtime_handle_no_note_motion(0x37,motor,7) && resets==1);
 app_command_runtime_handle_no_note_motion(0x0A,no_note,7);
 motor[5]=2;assert(app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(!locked && faults==1 && resets==2);
 diagnostic=true;app_command_runtime_handle_no_note_motion(0x0A,no_note,7);
 assert(!g_no_note_motor_active);diagnostic=false;
 app_command_runtime_handle_no_note_motion(0x0A,no_note,7);
 no_note[5]=1;app_command_runtime_handle_no_note_motion(0x0A,no_note,7);
 g_counting_session.start_confirmed=true;
 assert(!g_no_note_motor_active && locked && app_command_runtime_count_start_busy());
 motor[5]=1;assert(!app_command_runtime_handle_no_note_motion(0x37,motor,7));
 assert(locked && resets==2);
 puts("PASS empty-feeder gate: terminal motor reply only, malformed/duplicate/unrelated replies, failure, diagnostics and real-count isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-no-note-motor-') as directory:
    work = Path(directory)
    harness = work / 'test.c'
    harness.write_text(fixture + actual + cases)
    exe = work / 'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I' + str(ROOT), str(harness),
                    str(ROOT / 'un260/protocol/protocol_frame.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
