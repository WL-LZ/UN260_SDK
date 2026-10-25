#!/usr/bin/env python3
"""Exercise the production auto-upgrade poll across maintenance ownership.

The detector and presentation sink are observable peripherals. The actual
poll owns the interval, page exclusions, and factory/calibration storage gates.
"""
from pathlib import Path
import re
import subprocess
import tempfile
from test_main_view import function

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/app_service/app_ui_runtime.c').read_text()
interval = re.search(r'^#define APP_UI_UPGRADE_DETECT_INTERVAL_MS\s+(.+)$', source, re.M)
assert interval
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "tools/test_maintenance_state.h"
static ui_page_t current_page=UI_PAGE_MAIN;
static bool standby_busy,workspace_busy;
static uint32_t g_upgrade_detect_tick;
static unsigned detections,presentations;
ui_page_t ui_manager_get_current_page(void){return current_page;}
bool standby_store_busy(void){return standby_busy;}
bool workspace_store_busy(void){return workspace_busy;}
void ui_upgrade_service_detect(ui_upgrade_detect_info_t *info){
 ++detections;*info=(ui_upgrade_detect_info_t){true,true,true,UI_UPGRADE_PACKAGE_HASH_DIFFERENT};
}
void lv_upgrade_popup_process_detect(const ui_upgrade_detect_info_t *info){
 assert(info&&info->usb_present&&info->package_found&&info->package_hash_status==UI_UPGRADE_PACKAGE_HASH_DIFFERENT);
 ++presentations;
}
'''
code += '#define APP_UI_UPGRADE_DETECT_INTERVAL_MS ' + interval.group(1) + '\n'
code += function(source, 'app_ui_runtime_poll_upgrade')
code += r'''
int main(void){
 bool *guards[]={&host_factory_reset_blocked,&host_touch_calibration_busy,&standby_busy,&workspace_busy};
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++){
  g_upgrade_detect_tick=0;unsigned calls=detections;
  *guards[i]=true;app_ui_runtime_poll_upgrade(5000);
  assert(detections==calls&&presentations==calls&&g_upgrade_detect_tick==0);
  *guards[i]=false;app_ui_runtime_poll_upgrade(5000);
  assert(detections==calls+1&&presentations==calls+1&&g_upgrade_detect_tick==5000);
 }
 ui_page_t excluded[]={UI_PAGE_BOOT_ANIM,UI_PAGE_BOOT,UI_PAGE_UI_UPGRADE};
 for(unsigned i=0;i<sizeof(excluded)/sizeof(excluded[0]);i++){
  current_page=excluded[i];unsigned calls=detections;g_upgrade_detect_tick=0;
  app_ui_runtime_poll_upgrade(5000);assert(detections==calls&&g_upgrade_detect_tick==0);
 }
 current_page=UI_PAGE_MAIN;g_upgrade_detect_tick=0;unsigned calls=detections;
 app_ui_runtime_poll_upgrade(499);assert(detections==calls);
 app_ui_runtime_poll_upgrade(500);assert(detections==calls+1&&g_upgrade_detect_tick==500);
 app_ui_runtime_poll_upgrade(999);assert(detections==calls+1);
 app_ui_runtime_poll_upgrade(1000);assert(detections==calls+2);
 g_upgrade_detect_tick=UINT32_MAX-99;calls=detections;
 app_ui_runtime_poll_upgrade(399);assert(detections==calls);
 app_ui_runtime_poll_upgrade(400);assert(detections==calls+1);
 puts("PASS production auto-upgrade poll: factory/calibration and all storage owners suppress discovery/popup without consuming interval; boot/update exclusions, exact interval and uptime wrap preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-maintenance-upgrade-') as directory:
 work=Path(directory);unit=work/'poll.c';unit.write_text(code);exe=work/'poll'
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),str(unit),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
