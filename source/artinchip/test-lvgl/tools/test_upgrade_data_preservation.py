#!/usr/bin/env python3
"""Exercise the production nonblocking upgrade gate at its owner boundaries."""
from pathlib import Path
import subprocess
import tempfile
from test_main_view import function

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/app_service/app_command_runtime.c').read_text()
body = '\n'.join(function(source, name) for name in (
    'app_command_runtime_count_start_busy',
    'app_command_runtime_prepare_ui_upgrade',
    'app_command_runtime_request_count_start'))
head = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/app_service/app_command_runtime.h"
#include "un260/counting/counting_session_state.h"
#include "un260/storage/storage_worker.h"
static counting_session_state_t g_counting_session;
static bool start_pending, clear_pending, g_deferred_frame_valid, frames, query;
static bool initialized=true, available=true, cash_busy, workspace_busy, standby_busy;
static bool standby_ready=true;
static bool ready=true, saved=true, captured=true, idle=true;
static unsigned captures, starts, refreshes;
static int upgrade_owner;
static struct {bool counting;} multi;
static struct {unsigned pcs, record_no;} data={10,123};
static storage_job_status_t mock_history=STORAGE_JOB_SUCCEEDED, mock_writes=STORAGE_JOB_SUCCEEDED;
#define counting_action_start_pending() start_pending
#define counting_action_clear_pending() clear_pending
#define counting_multi_current() (&multi)
#define protocol_frame_queue_has_pending() frames
#define counting_multi_query_busy() query
#define counting_multi_extra_busy() false
#define ui_history_data_is_initialized() initialized
#define ui_history_data_is_available() available
#define app_clock_uptime_ms() 42U
#define cashbook_store_poll() false
#define workspace_store_poll() false
#define cashbook_store_busy() cash_busy
#define workspace_store_busy() workspace_busy
#define standby_store_busy() standby_busy
#define standby_store_storage_ready() standby_ready
#define cashbook_store_ready() ready
#define workspace_store_ready() ready
#define cashbook_store_last_success() saved
#define workspace_store_last_success() saved
#define counting_data_mutable() (&data)
#define counting_history_poll_commit(...) 0
#define ui_history_data_status() mock_history
#define storage_worker_pending_status() mock_writes
#define counting_history_is_idle() idle
#define upgrade_session_owner() upgrade_owner
#define UPGRADE_SESSION_NONE 0
#define counting_history_can_start() true
#define app_fault_recovery_prepare_start() true
#define counting_action_request_start() (++starts,true)
#define page_01_main_refresh_start_state() (++refreshes)
#define uart_debug_printf(...) ((void)0)
static bool capture(counting_session_state_t *s,const void *d,uint32_t now){
 assert(s==&g_counting_session && d==&data && now==42U);
 assert(data.pcs==10 && data.record_no==123);
 if(!captured)return false;
 if(s->history_record.valid){++captures;s->history_record.valid=false;}
 return true;
}
#define counting_history_prepare_reset capture
'''
test = r'''
int main(void){
 g_counting_session.history_record.valid=true;
 start_pending=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_BUSY);
 start_pending=false;multi.counting=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_BUSY);
 multi.counting=false;g_deferred_frame_valid=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 g_deferred_frame_valid=false;frames=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 frames=false;initialized=false;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 initialized=true;available=false;g_deferred_frame_valid=true;
 assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_FAILED);
 g_deferred_frame_valid=false;
 available=true;cash_busy=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 cash_busy=false;standby_busy=true;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 standby_busy=false;standby_ready=false;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_FAILED);
 standby_ready=true;ready=false;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_FAILED);
 ready=true;saved=false;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_READY);
 assert(captures==1);
 captures=0;g_counting_session.history_record.valid=true;
 saved=true;captured=false;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 assert(captures==0 && g_counting_session.history_record.valid);
 captured=true;idle=false;mock_history=STORAGE_JOB_PENDING;
 assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT && captures==1);
 for(int i=0;i<3;++i)assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 assert(captures==1);idle=true;mock_history=STORAGE_JOB_SUCCEEDED;mock_writes=STORAGE_JOB_FAILED;
 assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_FAILED);
 mock_writes=STORAGE_JOB_PENDING;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_WAIT);
 mock_writes=STORAGE_JOB_SUCCEEDED;assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_READY);
 assert(app_command_runtime_prepare_ui_upgrade()==APP_UI_UPGRADE_READY && captures==1);
 upgrade_owner=1;assert(!app_command_runtime_request_count_start() && starts==0);
 upgrade_owner=0;assert(app_command_runtime_request_count_start() && starts==1 && refreshes==1);
 assert(data.pcs==10 && data.record_no==123);
 puts("PASS upgrade preservation: freeze once, wait durability, keep records, failed writes block, upgrade lease blocks new UI Start");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-upgrade-preservation-') as directory:
    unit = Path(directory) / 'gate.c'
    unit.write_text(head + body + test)
    binary = Path(directory) / 'gate'
    subprocess.run(['cc', '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-I' + str(root), str(unit), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
