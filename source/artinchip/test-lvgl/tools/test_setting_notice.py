#!/usr/bin/env python3
"""A profile owns only its current command; unrelated results must remain visible."""
from pathlib import Path
import os, subprocess, tempfile
from test_main_view import function

root = Path(__file__).resolve().parents[1]
service = (root/'un260/app_service/workspace_service.c').read_text()
owns = function(service, 'workspace_service_owns_command')
reject = function(service, 'workspace_service_reject_command')
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_components/ui_notice.h"
#include "un260/app_service/app_setting_notice.h"
#include "un260/app_service/workspace_service.h"
static struct { bool active,waiting; unsigned step,confirmed; char message[160]; } apply;
static workspace_apply_result_t apply_result;
static const char *const steps[]={"Speed","Sorting","Sound","Batch","ADD","Start method","Count mode"};
static unsigned notices;
static ui_notice_kind_t last;
void ui_notice_post(ui_notice_kind_t kind,const char *key,const char *title,const char *detail) {
    assert(key&&title&&detail);notices++;last=kind;
}
''' + owns + reject + r'''
int main(void) {
    apply.active=apply.waiting=true;apply.step=0;
    apply.confirmed=2;apply_result=WORKSPACE_APPLY_PENDING;
    app_setting_notice_result("settings.speed","Speed",true);assert(!notices);
    app_setting_notice_result("settings.receipt","Receipt settings",true);
    assert(notices==1&&last==UI_NOTICE_SUCCESS);
    app_setting_notice_timeout("settings.receipt","Receipt settings");
    assert(notices==2&&last==UI_NOTICE_WARNING);
    app_setting_notice_result("settings.reject_pocket","Reject capacity",false);
    assert(notices==3&&last==UI_NOTICE_ERROR);
    app_setting_notice_result("settings.mode","Mode",true);assert(notices==4);
    assert(apply.active&&apply_result==WORKSPACE_APPLY_PENDING);
    /* Capture ownership before finishing so no child ERROR is emitted. */
    app_setting_notice_result("settings.speed","Speed",false);
    assert(notices==4&&!apply.active&&!apply.waiting&&apply_result==WORKSPACE_APPLY_FAILED);
    assert(strstr(apply.message,"Speed rejected")&&strstr(apply.message,"2 changes confirmed"));
    assert(!workspace_service_reject_command(0x16));
    apply.active=apply.waiting=true;apply_result=WORKSPACE_APPLY_PENDING;
    app_setting_notice_timeout("settings.speed","Speed");
    assert(notices==4&&apply.active&&apply_result==WORKSPACE_APPLY_PENDING);
    apply.waiting=false;
    app_setting_notice_timeout("settings.speed","Speed");assert(notices==5);
    apply.active=false;
    app_setting_notice_result("settings.speed","Speed",true);assert(notices==6);
    puts("PASS profile matched reject ends once without child notice; unrelated results visible; timeout remains unconfirmed");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-setting-notice-') as directory:
    work=Path(directory);source=work/'test.c';source.write_text(code)
    binary=work/('test.exe' if os.name=='nt' else 'test')
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
             '-I'+str(root),str(source),str(root/'un260/app_service/app_setting_notice.c'),'-o',str(binary)]
    if os.name!='nt':command+=['-fsanitize=address,undefined','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(binary)],check=True)
