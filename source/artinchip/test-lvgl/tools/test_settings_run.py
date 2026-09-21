#!/usr/bin/env python3
"""Production diagnostic RUN guard, with only hardware state edges stubbed."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'un260/app_service/app_command_runtime.c').read_text()
def function(name):
    match=re.search(r'^(?:const char \*|bool )'+name+r'\([^;]*?\)\n\{.*?^\}',source,re.M|re.S)
    assert match,name
    return match.group()
code=r'''
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#define BOOT_STAGE_DONE 3
#define BOOT_STAGE_FAIL 4
typedef int boot_stage_t;
#define UPGRADE_SESSION_NONE 0
typedef struct {bool session_active;} calibration_state_snapshot_t;
static bool mode=true,link=true,fault,busy,calibration,motor,aging;
static int boot=3,upgrade,starts;
bool work_mode_service_diagnostic_ready(void){return mode;}
const char *work_mode_service_status_text(void){return "Manual mode not confirmed";}
bool protocol_send_is_ready(void){return link;}
int boot_service_get_stage(void){return boot;}
int upgrade_session_owner(void){return upgrade;}
bool fault_popup_is_showing(void){return fault;}
bool fault_popup_get_pending_fault(void*a,void*b,void*c){(void)a;(void)b;(void)c;return fault;}
bool app_command_runtime_count_start_busy(void){return busy;}
void diagnostic_calibration_get_snapshot(calibration_state_snapshot_t*s){s->session_active=calibration;}
bool machine_state_aging_running(void){return aging;}
bool motor_test_service_busy(void){return motor;}
bool app_command_runtime_request_count_start(void){starts++;return true;}
'''
code+=function('app_command_runtime_calibration_blocker')+'\n'+function('app_command_runtime_diagnostic_run_blocker')+'\n'+function('app_command_runtime_request_diagnostic_run')
code+=r'''
int main(void){
 assert(app_command_runtime_request_diagnostic_run()&&starts==1);
 bool *gates[]={&fault,&busy,&calibration,&motor,&aging};
 for(unsigned i=0;i<5;i++){*gates[i]=true;assert(!app_command_runtime_request_diagnostic_run());*gates[i]=false;}
 mode=false;assert(!app_command_runtime_request_diagnostic_run());mode=true;
 link=false;assert(!app_command_runtime_request_diagnostic_run());link=true;
 boot=0;assert(!app_command_runtime_request_diagnostic_run());boot=3;
 upgrade=1;assert(!app_command_runtime_request_diagnostic_run());upgrade=0;
 assert(starts==1&&app_command_runtime_request_diagnostic_run()&&starts==2);
 boot=BOOT_STAGE_FAIL;assert(app_command_runtime_request_diagnostic_run()&&starts==3);
 fault=true;assert(!app_command_runtime_calibration_blocker());assert(app_command_runtime_diagnostic_run_blocker());
 puts("PASS diagnostic RUN: mode, link, boot, upgrade, fault, count, calibration, motor and aging gates");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'run.c').write_text(code)
    for opt in ('-O0','-O2'):
        subprocess.run(['cc',opt,'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'run.c'),'-o',str(p/'run')],check=True)
        subprocess.run([str(p/'run')],check=True)
