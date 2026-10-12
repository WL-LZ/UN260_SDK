#!/usr/bin/env python3
"""Production Batch preference persistence, offline commit and Main routing.

Run fresh processes against the real atomic state writer. Only live UI accessors
and the controller transport are stubbed; the original workspace cycle and its
guards are compiled directly for the Main callback qualification.
"""
from pathlib import Path
import hashlib
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def actual_function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{.*?^\}", source, re.M | re.S)
    assert match, name
    return match.group()


STATE_TEST = r'''
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/app_service/app_local_setting.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_core/page_01_main.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static bool fail_sync,fail_rename,top_popup=true,popup_auto=true,pure_count=true;
static unsigned notices;
static ui_notice_kind_t notice_kind;
static const char *notice_detail;
int __real_fsync(int fd);
int __wrap_fsync(int fd){if(fail_sync){errno=EIO;return -1;}return __real_fsync(fd);}
int __real_rename(const char *old,const char *next);
int __wrap_rename(const char *old,const char *next){if(fail_rename){errno=EIO;return -1;}return __real_rename(old,next);}
page_01_detail_section_t page_01_detail_section_get(void){return PAGE_01_DETAIL_SECTION_C;}
void page_01_detail_section_set(page_01_detail_section_t s,bool save){(void)s;(void)save;}
bool fault_popup_get_auto_enabled(void){return popup_auto;}
void fault_popup_set_auto_enabled(bool v){popup_auto=v;}
bool ui_notice_get_enabled(void){return top_popup;}
void ui_notice_set_enabled(bool v){top_popup=v;}
bool smart_island_pure_count_is_enabled(void){return pure_count;}
void smart_island_set_pure_count_enabled(bool v){pure_count=v;}
bool app_setting_notice_page_allowed(void){return true;}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail){
 assert(!strcmp(key,"settings.main_batch_action"));assert(!strcmp(title,"Main Batch button"));
 notices++;notice_kind=kind;notice_detail=detail;
}
void page_01_main_layout_set_enabled(bool v){(void)v;}
bool page_01_main_layout_is_enabled(void){return true;}
bool gesture_service_set_enabled(bool v){(void)v;return true;}
bool touch_feedback_set_enabled(bool v){(void)v;return true;}
static size_t file_bytes(char data[4096]){
 FILE *fp=fopen(UI_STATE_STORE_PATH,"rb");assert(fp);
 size_t size=fread(data,1,4096,fp);assert(!ferror(fp)&&feof(fp));assert(!fclose(fp));return size;
}
static void unchanged(const char *original,size_t size){
 char current[4096];assert(file_bytes(current)==size&&!memcmp(original,current,size));
 assert(ui_state_main_batch_action_get()==UI_MAIN_BATCH_SETTINGS);
}
static void unrelated(const ui_persist_state_t *disk){
 const uint8_t left[4]={3,2,1,0},right[3]={2,1,0};
 const uint8_t order[13]={8,7,6,5,4,3,2,1,0,9,10,11,12};
 assert(!memcmp(disk->page01.layout.left,left,4)&&!memcmp(disk->page01.layout.right,right,3));
 assert(disk->page01.layout.mirrored==1&&disk->page01.layout.footer_swapped==1);
 assert(!memcmp(disk->page01.quick_order,order,13)&&disk->page01.quick_visible==7);
 assert(disk->page07.view_mode==1&&disk->page07.fav_only==1&&disk->page07.fav_count==1);
 assert(!strcmp(disk->page07.fav_codes[0],"EUR"));
 assert(disk->page06.reserved06_enable==1&&disk->page18.reserved18_enable==1);
}
int main(int argc,char **argv){
 assert(argc==3);ui_main_batch_action_t expected=(ui_main_batch_action_t)atoi(argv[2]);
 assert(ui_state_main_batch_action_get()==expected);
 if(!strcmp(argv[1],"get"))return 0;
 if(!strcmp(argv[1],"direct-default")){
  ui_persist_state_t disk={0};disk.page01.main_batch_action=UI_MAIN_BATCH_CYCLE;
  assert(ui_state_store_load(&disk)&&disk.page01.main_batch_action==UI_MAIN_BATCH_SETTINGS);return 0;
 }
 if(!strcmp(argv[1],"open-failure")){
  char original[4096];size_t size=file_bytes(original);
  assert(!app_local_setting_apply(APP_LOCAL_MAIN_BATCH_ACTION,UI_MAIN_BATCH_CYCLE,0));
  assert(notice_kind==UI_NOTICE_ERROR&&!strcmp(notice_detail,"Could not save. Try again."));
  unchanged(original,size);return 0;
 }
 if(!strcmp(argv[1],"failures")){
  char original[4096];size_t size=file_bytes(original);
  assert(expected==UI_MAIN_BATCH_SETTINGS);
  assert(!ui_state_main_batch_action_set((ui_main_batch_action_t)-1));
  assert(!ui_state_main_batch_action_set((ui_main_batch_action_t)2));unchanged(original,size);
  fail_sync=true;
  assert(ui_state_main_batch_action_set(UI_MAIN_BATCH_SETTINGS)); // A repeated choice performs no write.
  assert(!app_local_setting_apply(APP_LOCAL_MAIN_BATCH_ACTION,UI_MAIN_BATCH_CYCLE,0));fail_sync=false;
  assert(notice_kind==UI_NOTICE_ERROR);unchanged(original,size);
  assert(access(UI_STATE_STORE_PATH ".tmp",F_OK)!=0);
  fail_rename=true;assert(!app_local_setting_apply(APP_LOCAL_MAIN_BATCH_ACTION,UI_MAIN_BATCH_CYCLE,0));fail_rename=false;
  unchanged(original,size);assert(access(UI_STATE_STORE_PATH ".tmp",F_OK)!=0);
  assert(notices==2);return 0;
 }
 unsigned target=!strcmp(argv[1],"cycle")?UI_MAIN_BATCH_CYCLE:UI_MAIN_BATCH_SETTINGS;
 assert(target!=expected);top_popup=false;
 assert(app_local_setting_apply(APP_LOCAL_MAIN_BATCH_ACTION,target,0));
 assert(ui_state_main_batch_action_get()==(ui_main_batch_action_t)target);
 assert(notice_kind==UI_NOTICE_SUCCESS&&!strcmp(notice_detail,"Setting saved."));
 ui_persist_state_t disk={0};assert(ui_state_store_load(&disk));
 assert(disk.page01.main_batch_action==(ui_main_batch_action_t)target);unrelated(&disk);
 assert(!disk.top_popup_enabled&&disk.page01.detail_section==PAGE_01_DETAIL_SECTION_C);
 // A different persisted option must not erase the selected Batch behavior.
 assert(ui_state_quick_visible_set(3,true));assert(ui_state_store_load(&disk));
 assert(disk.page01.main_batch_action==(ui_main_batch_action_t)target&&disk.page01.quick_visible==15);
 assert(ui_state_quick_visible_set(3,false));
 puts("PASS actual local Batch commit: offline save; write-before-live update; unrelated state retained");
 return 0;
}
'''

HEADERS = {
    'un260/lv_system/ui_i18n.h': '#define UI_N_(s) s\n',
    'un260/lv_components/lv_fault_popup.h': '#include <stdbool.h>\nbool fault_popup_get_auto_enabled(void);\nvoid fault_popup_set_auto_enabled(bool);\n',
    'un260/lv_components/ui_notice.h': '#include <stdbool.h>\ntypedef enum {UI_NOTICE_SUCCESS,UI_NOTICE_ERROR} ui_notice_kind_t;\nbool ui_notice_get_enabled(void);\nvoid ui_notice_set_enabled(bool);\nvoid ui_notice_post_text(ui_notice_kind_t,const char*,const char*,const char*);\n',
    'un260/lv_components/smart_island.h': '#include <stdbool.h>\nbool smart_island_pure_count_is_enabled(void);\nvoid smart_island_set_pure_count_enabled(bool);\n',
    'un260/lv_core/page_01_main.h': '#include <stdbool.h>\ntypedef enum {PAGE_01_DETAIL_SECTION_A,PAGE_01_DETAIL_SECTION_B,PAGE_01_DETAIL_SECTION_C} page_01_detail_section_t;\npage_01_detail_section_t page_01_detail_section_get(void);\nvoid page_01_detail_section_set(page_01_detail_section_t,bool);\n',
    'un260/lv_core/page_01_main_layout.h': '#include <stdbool.h>\nvoid page_01_main_layout_set_enabled(bool);\nbool page_01_main_layout_is_enabled(void);\n',
    'un260/gesture/gesture_service.h': '#include <stdbool.h>\nbool gesture_service_set_enabled(bool);\n',
    'un260/gesture/touch_feedback.h': '#include <stdbool.h>\nbool touch_feedback_set_enabled(bool);\n',
}

CALLBACK_TEST = r'''
#include "un260/lv_system/ui_state_store.h"
typedef struct {unsigned code;} lv_event_t;
enum {LV_EVENT_PRESSED=1,LV_EVENT_LONG_PRESSED,LV_EVENT_RELEASED,LV_EVENT_CLICKED};
static ui_main_batch_action_t preference=UI_MAIN_BATCH_SETTINGS;
static unsigned opens;
static unsigned lv_event_get_code(lv_event_t *e){return e->code;}
ui_main_batch_action_t ui_state_main_batch_action_get(void){return preference;}
void page_03_menu_open_batch(void){opens++;}
'''

CALLBACK_CASES = r'''
int main(void){
 workspace_existing_tests();
 workspace_service_cancel_apply();batch_save.pending=false;async_save=false;
 ready=true;busy=running=diagnostic=false;capacity=true;connected=false;fail_send=false;
 workspace_defaults(&model);model.operators_enabled=1;memset(&count,0,sizeof(count));
 actuals[3]=0;actuals[4]=0;
 lv_event_t event={LV_EVENT_PRESSED};unsigned before=commands;
 page_01_bottom_batch_btn_event_cb(&event);event.code=LV_EVENT_LONG_PRESSED;
 page_01_bottom_batch_btn_event_cb(&event);event.code=LV_EVENT_RELEASED;
 page_01_bottom_batch_btn_event_cb(&event);assert(opens==0&&commands==before);
 // Settings works offline and routes to the Count / Batch presets helper only.
 event.code=LV_EVENT_CLICKED;page_01_bottom_batch_btn_event_cb(&event);
 assert(opens==1&&commands==before&&actuals[3]==0);
 preference=UI_MAIN_BATCH_CYCLE;page_01_bottom_batch_btn_event_cb(&event);
 assert(opens==1&&commands==before); // Disconnect guard remains intact.
 connected=true;
 page_01_bottom_batch_btn_event_cb(&event);
 assert(commands==before+1&&last_step==3&&last_value==10&&actuals[3]==0);
 // Sending does not manufacture the controller ACK. Rapid retry retains old actual value.
 page_01_bottom_batch_btn_event_cb(&event);assert(last_value==10&&actuals[3]==0);
 actuals[3]=10;page_01_bottom_batch_btn_event_cb(&event);assert(last_value==50&&actuals[3]==10);
 actuals[3]=150;page_01_bottom_batch_btn_event_cb(&event);assert(last_value==0&&actuals[3]==150);
 before=commands;
 running=true;page_01_bottom_batch_btn_event_cb(&event);running=false;assert(commands==before);
 diagnostic=true;page_01_bottom_batch_btn_event_cb(&event);diagnostic=false;assert(commands==before);
 capacity=false;page_01_bottom_batch_btn_event_cb(&event);capacity=true;assert(commands==before);
 ready=false;page_01_bottom_batch_btn_event_cb(&event);ready=true;assert(commands==before);
 busy=true;page_01_bottom_batch_btn_event_cb(&event);busy=false;assert(commands==before);
 batch_save.pending=true;page_01_bottom_batch_btn_event_cb(&event);batch_save.pending=false;assert(commands==before);
 apply.active=true;page_01_bottom_batch_btn_event_cb(&event);apply.active=false;assert(commands==before);
 // Idle ADD behavior, ownership and totals are the existing cycle semantics.
 actuals[3]=0;actuals[4]=1;count.total_pcs=50;
 page_01_bottom_batch_btn_event_cb(&event);assert(commands==before+1&&last_value==10&&count.total_pcs==50);
 puts("PASS actual Main Batch callback + original cycle service: settings route, non-click exclusion, offline and busy guards, ACK ownership, OFF/numeric cycle, idle ADD");
 return 0;
}
'''


def main():
    source_names = ['un260/lv_system/ui_state_runtime.c', 'un260/lv_system/ui_state_store.c',
                    'un260/lv_system/ui_state_store.h', 'un260/lv_system/ui_main_layout.c',
                    'un260/app_service/app_local_setting.c', 'un260/lv_core/lv_page_event.c',
                    'un260/app_service/workspace_service.c', 'un260/workspace/workspace_model.c']
    before = {name: hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in source_names}
    fixture = '''magic=1430868820
version=1
p01_detail_section=1
p01_layout=3,2,1,0,2,1,0,1
p01_layout_footer=1
p01_quick_order_v3=8,7,6,5,4,3,2,1,0,9,10,11,12
p01_quick_visible=7
p07_view_mode=1
p07_fav_only=1
p07_fav_count=1
p07_fav0=EUR
p06_reserved=1
p18_reserved=1
'''
    compiler = os.environ.get('CC', 'cc')
    for optimization in ('-O0', '-O2'):
        with tempfile.TemporaryDirectory(prefix='un260-main-batch-') as directory:
            work = Path(directory)
            for name, value in HEADERS.items():
                path = work/name;path.parent.mkdir(parents=True, exist_ok=True);path.write_text(value)
            state = work/'state.cfg'
            test = work/'test.c';test.write_text(STATE_TEST)
            binary = work/'state-test'
            command = [compiler, '-std=gnu11', optimization, '-g', '-Wall', '-Wextra', '-Werror',
                       '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
                       '-I'+str(work), '-I'+str(ROOT), '-DUI_STATE_STORE_PATH="'+str(state)+'"',
                       str(test), *(str(ROOT/name) for name in source_names[:2]),
                       str(ROOT/'un260/lv_system/ui_main_layout.c'), str(ROOT/'un260/app_service/app_local_setting.c'),
                       '-Wl,--wrap=fsync', '-Wl,--wrap=rename', '-o', str(binary)]
            subprocess.run(command, check=True)

            def run(mode, expected):
                subprocess.run([str(binary), mode, str(expected)], check=True)

            run('get', 0)  # Missing file.
            assert not state.exists()
            state.write_text(fixture)
            original = state.read_bytes();run('get', 0);run('direct-default', 0)
            assert state.read_bytes() == original  # Loading never rewrites old config.
            for value in ('-1', '2', '255', 'x', '1 trailing', '4294967295',
                          '4294967296', '4294967297', '18446744073709551616'):
                state.write_text(fixture+'p01_main_batch_action='+value+'\n')
                original=state.read_bytes();run('get', 0);assert state.read_bytes()==original
            state.write_text(fixture+'p01_main_batch_action=1\n');run('get', 1)
            state.write_text(fixture+'p01_main_batch_action=0\n');run('get', 0)
            state.write_text(fixture);run('failures', 0)
            (work/'state.cfg.tmp').mkdir();run('open-failure', 0);(work/'state.cfg.tmp').rmdir()
            run('cycle', 0);run('get', 1)  # Separate process, true reload.
            run('settings', 1);run('get', 0)
            for invalid in ('magic=0\nversion=1\n', 'magic=1430868820\nversion=2\n'):
                state.write_text(invalid+'p01_main_batch_action=1\n');run('get', 0)
        # Use the original service harness, compile its production service and
        # the exact Main callback, and append behavior-specific cases.
        from test_i18n_support import with_i18n
        with tempfile.TemporaryDirectory(prefix='un260-main-batch-route-') as directory:
            work=Path(directory);test=work/'test.c'
            original=(ROOT/'tools/test_workspace_service.c').read_text()
            assert original.count('int main(void)')==1
            original=original.replace('int main(void)', 'static void workspace_existing_tests(void)', 1)
            callback=actual_function((ROOT/'un260/lv_core/lv_page_event.c').read_text(), 'page_01_bottom_batch_btn_event_cb')
            test.write_text(original+CALLBACK_TEST+callback+CALLBACK_CASES)
            command=[compiler,'-std=gnu11',optimization,'-g','-Wall','-Wextra','-Werror',
                     '-Wno-misleading-indentation','-fsanitize=address,undefined',
                     '-fno-sanitize-recover=all','-no-pie','-I'+str(ROOT),str(test),
                     str(ROOT/'un260/workspace/workspace_model.c'),
                     *(str(p) for p in with_i18n([],ROOT)), '-o',str(work/'route-test')]
            subprocess.run(command,check=True);subprocess.run([str(work/'route-test')],check=True)
    after={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in source_names}
    assert before==after, 'Production input changed during qualification'
    print('PASS Main Batch preference O0/O2 ASan+UBSan: default/legacy/invalid, offline local service, atomic failures, fresh-process cycle/settings reload, original Main cycle protocol guards, stable production inputs')


if __name__ == '__main__':
    main()
