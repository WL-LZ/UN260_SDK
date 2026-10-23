from test_i18n_support import with_i18n
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
#include "un260/lv_system/ui_i18n.h"
#include "un260/app_service/app_setting_notice.h"
#include "un260/app_service/setting_service.h"
#include "un260/lv_core/lv_page_manager.h"
static ui_page_t page=UI_PAGE_MENU;
static bool currency_mode;
bool count_mode_service_currency_mode(void){return currency_mode;}
static setting_request_observer_t observer;
ui_page_t ui_manager_get_current_page(void){return page;}
void setting_service_set_request_observer(setting_request_observer_t value){observer=value;}
#include "un260/app_service/workspace_service.h"
static struct { bool active,waiting; unsigned step,confirmed; char message[160]; } apply;
static workspace_apply_result_t apply_result;
static ui_message_t apply_message_info;
static const char *const steps[]={"Speed","Sorting","Sound","Batch","ADD","Start method","Count mode"};
static unsigned notices;
static ui_notice_kind_t last;
static ui_notice_state_t queue;
static unsigned clears;
static char cleared_key[UI_NOTICE_KEY_CAPACITY];
/* This fixture exercises banner ownership with automatic fault guides disabled.
 * Fault recovery routing itself is covered by the dedicated integration suite. */
void fault_popup_resolve_setting_rejection(uint8_t command){(void)command;}
void app_fault_recovery_resolve_setting_rejection(uint8_t command){(void)command;}
bool app_fault_recovery_report_setting_rejection(uint8_t command){(void)command;return false;}
bool fault_popup_get_auto_enabled(void){return false;}
void fault_popup_report_setting_rejection(uint8_t command,uint8_t reason){(void)command;(void)reason;}
void ui_notice_clear(const char *key){
    assert(key);++clears;snprintf(cleared_key,sizeof(cleared_key),"%s",key);
    ui_notice_state_remove(&queue,key,false);
}
static void seed_progress(const char *key){
    ui_notice_config_t config={.kind=UI_NOTICE_PROGRESS,.key=key,.title="Working"};
    assert(ui_notice_state_post(&queue,&config));
}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail) {
    assert(key&&title&&detail);notices++;last=kind;
    ui_notice_config_t config={.kind=kind,.key=key,.title=title,.detail=detail};
    ui_notice_state_post(&queue,&config);
}
''' + owns + reject + r'''
int main(void) {
    app_setting_notice_init();
    page=UI_PAGE_MAIN;currency_mode=true;observer(0x04);currency_mode=false;
    app_setting_notice_result("settings.mode","Count mode",true);assert(notices==0);
    app_setting_notice_timeout("settings.mode","Count mode");assert(notices==1&&last==UI_NOTICE_WARNING);
    app_setting_notice_result("settings.mode","Count mode",false);assert(notices==2&&last==UI_NOTICE_ERROR);
    page=UI_PAGE_MENU;observer(0x04);app_setting_notice_result("settings.mode","Count mode",true);
    assert(notices==3&&last==UI_NOTICE_SUCCESS);notices=0;

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
    ui_message_render(&apply_message_info,apply.message,sizeof(apply.message));
    assert(strstr(apply.message,"Speed rejected")&&strstr(apply.message,"2 changes confirmed"));
    assert(!workspace_service_reject_command(0x16));
    apply.active=apply.waiting=true;apply_result=WORKSPACE_APPLY_PENDING;
    app_setting_notice_timeout("settings.speed","Speed");
    assert(notices==4&&apply.active&&apply_result==WORKSPACE_APPLY_PENDING);
    apply.waiting=false;
    app_setting_notice_timeout("settings.speed","Speed");assert(notices==5);
    apply.active=false;
    app_setting_notice_result("settings.speed","Speed",true);assert(notices==6);
    page=UI_PAGE_MAIN;observer(0x16);
    app_setting_notice_result("settings.speed","Speed",true);assert(notices==6);
    page=UI_PAGE_MENU;
    app_setting_notice_result("settings.speed","Speed",false);assert(notices==6);
    observer(0x16);
    app_setting_notice_result("settings.speed","Speed",true);assert(notices==7);
    page=UI_PAGE_PURE;app_setting_notice_timeout("settings.speed","Speed");assert(notices==7);
    page=UI_PAGE_DETAIL;app_setting_notice_timeout("settings.speed","Speed");assert(notices==8);
    page=UI_PAGE_MAIN;apply.active=apply.waiting=true;apply.step=0;
    app_setting_notice_result("settings.speed","Speed",false);
    assert(notices==8&&!apply.active&&apply_result==WORKSPACE_APPLY_FAILED);
    apply.active=apply.waiting=false;page=UI_PAGE_MENU;
    const struct { const char *key; uint8_t command; } settings[]={
        {"settings.mode",0x04},{"settings.speed",0x16},{"settings.work_mode",0x38},
        {"settings.add",0x39},{"settings.sorting",0x3A},{"settings.sound",0x15},
        {"settings.batch",0x06},{"settings.reject_pocket",0x08}};
    for(unsigned i=0;i<sizeof(settings)/sizeof(settings[0]);++i){
        unsigned before=notices;observer(settings[i].command);
        app_setting_notice_result(settings[i].key,"Setting",true);
        assert(notices==before+1&&last==UI_NOTICE_SUCCESS);
    }
    apply.active=apply.waiting=true;apply.step=5;
    unsigned before=notices;
    app_setting_notice_result("settings.work_mode","Start",true);
    assert(notices==before); /* Profile owns this result. */
    apply.active=apply.waiting=false;
    page=UI_PAGE_MAIN;observer(0x38);page=UI_PAGE_MENU;
    app_setting_notice_result("settings.work_mode","Start",true);
    assert(notices==before); /* Main-origin confirmation stays quiet. */
    observer(0x38);app_setting_notice_result("settings.work_mode","Start",true);
    assert(notices==before+1);
    /* A terminal result suppressed by page policy still ends only its own
     * progress lifetime. It cannot cancel unrelated queued operations. */
    ui_notice_state_init(&queue);page=UI_PAGE_MENU;observer(0x42);
    seed_progress("settings.flap");seed_progress("records.store");
    unsigned prior_posts=notices,prior_clears=clears;
    page=UI_PAGE_MAIN;app_setting_notice_result("settings.flap","Flap position",true);
    assert(notices==prior_posts&&clears==prior_clears+1&&!strcmp(cleared_key,"settings.flap"));
    assert(queue.has_active&&!strcmp(queue.active.key,"records.store")&&queue.active.kind==UI_NOTICE_PROGRESS);
    ui_notice_state_init(&queue);page=UI_PAGE_MENU;observer(0x08);
    seed_progress("settings.reject_pocket");seed_progress("records.store");
    prior_clears=clears;page=UI_PAGE_PURE;
    app_setting_notice_timeout("settings.reject_pocket","Reject capacity");
    assert(notices==prior_posts&&clears==prior_clears+1&&!strcmp(cleared_key,"settings.reject_pocket"));
    assert(queue.has_active&&!strcmp(queue.active.key,"records.store")&&queue.active.kind==UI_NOTICE_PROGRESS);
    /* Main-origin policy also retires stale presentation after entering Menu. */
    ui_notice_state_init(&queue);page=UI_PAGE_MAIN;observer(0x16);
    seed_progress("settings.speed");seed_progress("records.store");
    prior_clears=clears;page=UI_PAGE_MENU;
    app_setting_notice_result("settings.speed","Speed",true);
    assert(notices==prior_posts&&clears==prior_clears+1&&!strcmp(cleared_key,"settings.speed"));
    assert(queue.has_active&&!strcmp(queue.active.key,"records.store"));
    /* Profile-owned steps stay owned by the aggregate operation even on Home. */
    ui_notice_state_init(&queue);seed_progress("workspace.apply");
    apply.active=apply.waiting=true;apply.step=0;page=UI_PAGE_MAIN;prior_clears=clears;
    app_setting_notice_timeout("settings.speed","Speed");
    assert(notices==prior_posts&&clears==prior_clears&&queue.has_active&&!strcmp(queue.active.key,"workspace.apply"));
    puts("PASS profile ownership, unrelated results, unconfirmed timeouts; suppressed terminal results clear only matching progress");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-setting-notice-') as directory:
    work=Path(directory);source=work/'test.c';source.write_text(code)
    binary=work/('test.exe' if os.name=='nt' else 'test')
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
             '-I'+str(root),str(source),str(root/'un260/app_service/app_setting_notice.c'),str(root/'un260/lv_components/ui_notice_state.c'),'-o',str(binary)]
    command[1:1]=list(map(str,with_i18n([],root)))
    if os.name!='nt':command+=['-fsanitize=address,undefined','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(binary)],check=True)
