#!/usr/bin/env python3
"""Exercise the production quick-order runtime and atomic state-file writer.

Only unrelated live UI accessors are stubbed. Reloads use a fresh process;
file failures are injected at real fsync/rename boundaries, not a fake store.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
default = '0,1,3,4,2,5,6,7,8'
legacy = '0,1,2,3,4,5,6,7'
custom = '5,2,7,0,1,6,3,4'
custom_v2 = '8,5,2,7,0,1,6,3,4'
saved = '8,7,6,5,4,3,2,1,0'
header = 'magic=1430868820\nversion=1\n'
fixture = header + '''p01_detail_section=1
p01_layout=3,2,1,0,2,1,0,1
p01_layout_footer=1
p07_view_mode=1
p07_fav_only=1
p07_fav_count=1
p07_fav0=EUR
p06_reserved=1
p18_reserved=1
'''
code = r'''
#include "un260/lv_system/ui_state_runtime.h"
#include "un260/lv_core/page_01_main.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static bool fail_sync,fail_rename;
static bool popup_auto=true,top_popup=true,pure_count=true;
#define QUICK_COUNT UI_STATE_QUICK_CONTROL_COUNT
_Static_assert(QUICK_COUNT==13,"All quick tools must participate in the saved order");
int __real_fsync(int fd);
int __wrap_fsync(int fd){if(fail_sync){errno=EIO;return -1;}return __real_fsync(fd);}
int __real_rename(const char *old,const char *new);
int __wrap_rename(const char *old,const char *new){if(fail_rename){errno=EIO;return -1;}return __real_rename(old,new);}
page_01_detail_section_t page_01_detail_section_get(void){return PAGE_01_DETAIL_SECTION_C;}
void page_01_detail_section_set(page_01_detail_section_t s,bool save){(void)s;(void)save;}
bool fault_popup_get_auto_enabled(void){return popup_auto;}
void fault_popup_set_auto_enabled(bool v){popup_auto=v;}
bool ui_notice_get_enabled(void){return top_popup;}
void ui_notice_set_enabled(bool v){top_popup=v;}
bool smart_island_pure_count_is_enabled(void){return pure_count;}
void smart_island_set_pure_count_enabled(bool v){pure_count=v;}
static void get_order(const uint8_t expected[QUICK_COUNT]){
 uint8_t actual[QUICK_COUNT]={0};ui_state_quick_order_get(actual);assert(!memcmp(actual,expected,sizeof(actual)));
}
static void disk_order(const uint8_t expected[QUICK_COUNT]){
 ui_persist_state_t disk={0};assert(ui_state_store_load(&disk));
 assert(!memcmp(disk.page01.quick_order,expected,sizeof(disk.page01.quick_order)));
}
static size_t file_bytes(char data[4096]){
 FILE *fp=fopen(UI_STATE_STORE_PATH,"rb");assert(fp);
 size_t size=fread(data,1,4096,fp);assert(!ferror(fp)&&feof(fp));assert(!fclose(fp));return size;
}
static void unchanged(const uint8_t expected[QUICK_COUNT],const char *original,size_t size){
 char current[4096];get_order(expected);disk_order(expected);
 size_t actual=file_bytes(current);assert(actual==size&&!memcmp(current,original,size));
}
int main(int argc,char **argv){
 assert(argc==3);unsigned values[QUICK_COUNT];uint8_t expected[QUICK_COUNT];
 char input[128];snprintf(input,sizeof(input),"%s",argv[2]);char *part=strtok(input,",");unsigned count=0;while(part&&count<QUICK_COUNT){values[count++]=strtoul(part,NULL,10);part=strtok(NULL,",");}assert(count==QUICK_COUNT);
 for(unsigned i=0;i<QUICK_COUNT;i++)expected[i]=values[i];
 get_order(expected);ui_state_quick_order_get(NULL);
 if(!strncmp(argv[1],"visible-",8)){
  uint16_t mask=ui_state_quick_visible_get();
  if(!strcmp(argv[1],"visible-default")){assert(mask==UI_STATE_QUICK_VISIBLE_ALL);return 0;}
  if(!strcmp(argv[1],"visible-empty")){assert(mask==0);return 0;}
  assert(mask==UI_STATE_QUICK_VISIBLE_ALL);
  char original[4096];size_t size=file_bytes(original);
  fail_sync=true;assert(!ui_state_quick_visible_set(QUICK_COUNT-1,false));fail_sync=false;
  assert(ui_state_quick_visible_get()==mask);unchanged(expected,original,size);
  fail_rename=true;assert(!ui_state_quick_visible_set(0,false));fail_rename=false;
  assert(ui_state_quick_visible_get()==mask);unchanged(expected,original,size);
  assert(!ui_state_quick_visible_set(QUICK_COUNT,false));
  for(unsigned i=0;i<QUICK_COUNT;i++)assert(ui_state_quick_visible_set(i,false));
  assert(ui_state_quick_visible_get()==0);get_order(expected);disk_order(expected);
  ui_persist_state_t disk={0};assert(ui_state_store_load(&disk)&&disk.page01.quick_visible==0);
  fail_sync=true;assert(ui_state_quick_visible_set(0,false));fail_sync=false;
  uint8_t reordered[QUICK_COUNT];memcpy(reordered,expected,sizeof(reordered));
  uint8_t tmp=reordered[0];reordered[0]=reordered[QUICK_COUNT-1];reordered[QUICK_COUNT-1]=tmp;
  assert(ui_state_quick_order_save(reordered));
  assert(ui_state_store_load(&disk)&&disk.page01.quick_visible==0);
  return 0;
 }
 if(!strncmp(argv[1],"top-",4)){
  ui_state_apply_common_runtime();
  if(!strcmp(argv[1],"top-default")){assert(top_popup&&!popup_auto);return 0;}
  if(!strcmp(argv[1],"top-off")){assert(!top_popup&&!popup_auto);return 0;}
  assert(!strcmp(argv[1],"top-save")&&top_popup&&!popup_auto);
  char original[4096];size_t size=file_bytes(original);
  fail_sync=true;assert(!ui_state_top_popup_set_enabled(false));fail_sync=false;
  assert(top_popup&&!popup_auto);unchanged(expected,original,size);
  fail_rename=true;assert(!ui_state_top_popup_set_enabled(false));fail_rename=false;
  assert(top_popup&&!popup_auto);unchanged(expected,original,size);
  assert(ui_state_top_popup_set_enabled(false)&&!top_popup&&!popup_auto);
  ui_persist_state_t disk={0};assert(ui_state_store_load(&disk));
  assert(!disk.top_popup_enabled&&disk.page06.reserved06_enable==0);
  get_order(expected);disk_order(expected);
  fault_popup_set_auto_enabled(true);ui_state_save_popup_auto_state();
  assert(!top_popup&&popup_auto);assert(ui_state_store_load(&disk));
  assert(!disk.top_popup_enabled&&disk.page06.reserved06_enable==1);
  fault_popup_set_auto_enabled(false);ui_state_save_popup_auto_state();
  return 0;
 }
 if(!strcmp(argv[1],"get"))return 0;
 const uint8_t next[QUICK_COUNT]={8,7,6,5,4,3,2,1,0,9,10,11,12};
 char original[4096];size_t size=file_bytes(original);
 if(!strcmp(argv[1],"invalid")){
  uint8_t bad[QUICK_COUNT]={0,1,2,3,4,5,6,7,7};assert(!ui_state_quick_order_save(NULL));
  assert(!ui_state_quick_order_save(bad));bad[8]=9;assert(!ui_state_quick_order_save(bad));
  unchanged(expected,original,size);return 0;
 }
 if(!strcmp(argv[1],"open-failure")){
  assert(!ui_state_quick_order_save(next));unchanged(expected,original,size);return 0;
 }
 if(!strcmp(argv[1],"failure")){
  fail_sync=true;assert(!ui_state_quick_order_save(next));fail_sync=false;
  unchanged(expected,original,size);assert(access(UI_STATE_STORE_PATH ".tmp",F_OK)!=0);
  fail_rename=true;assert(!ui_state_quick_order_save(next));fail_rename=false;
  unchanged(expected,original,size);assert(access(UI_STATE_STORE_PATH ".tmp",F_OK)!=0);
 }
 assert(ui_state_quick_order_save(next));get_order(next);disk_order(next);
 ui_persist_state_t disk={0};assert(ui_state_store_load(&disk));
 assert(disk.page01.detail_section==PAGE_01_DETAIL_SECTION_C);
 assert(disk.page06.reserved06_enable==1&&disk.page18.reserved18_enable==1);
 assert(disk.page07.view_mode==1&&disk.page07.fav_only==1&&disk.page07.fav_count==1);
 assert(!strcmp(disk.page07.fav_codes[0],"EUR"));
 const uint8_t left[4]={3,2,1,0},right[3]={2,1,0};
 assert(!memcmp(disk.page01.layout.left,left,4)&&!memcmp(disk.page01.layout.right,right,3));
 assert(disk.page01.layout.mirrored==1&&disk.page01.layout.footer_swapped==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-quick-order-') as temporary:
    work = Path(temporary)
    headers = {
        'un260/lv_components/lv_fault_popup.h': '#include <stdbool.h>\nbool fault_popup_get_auto_enabled(void);\nvoid fault_popup_set_auto_enabled(bool);\n',
        'un260/lv_components/ui_notice.h': '#include <stdbool.h>\nbool ui_notice_get_enabled(void);\nvoid ui_notice_set_enabled(bool);\n',
        'un260/lv_components/smart_island.h': '#include <stdbool.h>\nbool smart_island_pure_count_is_enabled(void);\nvoid smart_island_set_pure_count_enabled(bool);\n',
        'un260/lv_core/page_01_main.h': '#include <stdbool.h>\ntypedef enum {PAGE_01_DETAIL_SECTION_A,PAGE_01_DETAIL_SECTION_B,PAGE_01_DETAIL_SECTION_C} page_01_detail_section_t;\npage_01_detail_section_t page_01_detail_section_get(void);\nvoid page_01_detail_section_set(page_01_detail_section_t,bool);\n',
    }
    for name, text in headers.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    state = work / 'state.cfg'
    binary = work / 'test'
    (work / 'test.c').write_text(code)
    sources = [root / ('un260/lv_system/' + name) for name in
               ('ui_state_runtime.c', 'ui_state_store.c', 'ui_main_layout.c')]
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-g',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
                    '-I' + str(work), '-I' + str(root),
                    '-DUI_STATE_STORE_PATH="' + str(state) + '"', str(work / 'test.c'),
                    *map(str, sources), '-Wl,--wrap=fsync', '-Wl,--wrap=rename', '-o', str(binary)], check=True)
    def run(mode, expected):
        if len(expected.split(','))==9:expected+=',9,10,11,12'
        subprocess.run([str(binary), mode, expected], check=True)

    def check_written_order(expected):
        fields = dict(line.split('=', 1) for line in state.read_text().splitlines())
        if len(expected.split(','))==9:expected+=',9,10,11,12'
        projection = ','.join(value for value in expected.split(',') if int(value)<8)
        v2 = ','.join(value for value in expected.split(',') if int(value)<9)
        assert fields['p01_quick_order'] == projection
        assert fields['p01_quick_order_v2'] == v2
        assert fields['p01_quick_order_v3'] == expected
        run('get', expected)
        # A file retaining only the legacy projection still contains all eight
        # original controls in the same relative order. New UI appends Top.
        original = state.read_bytes()
        state.write_text('\n'.join(line for line in state.read_text().splitlines()
                                   if not line.startswith(('p01_quick_order_v2=','p01_quick_order_v3='))) + '\n')
        run('get', projection + ',8')
        state.write_bytes(original)

    run('get', default)
    run('visible-default',default)
    state.write_text(fixture)
    for value in ('-1','8192','x','1 trailing'):
        state.write_text(fixture+'p01_quick_visible='+value+'\n')
        run('visible-default',default)
    state.write_text(fixture+'p01_quick_visible=0\n')
    run('visible-empty',default)
    state.write_text(fixture+'p01_quick_order_v3='+default+',9,10,11,12\n')
    run('visible-save',default)
    run('visible-empty','12,1,3,4,2,5,6,7,8,9,10,11,0')
    state.write_text(fixture)
    run('get', default)  # Existing v1 file predating the quick-order field.
    for order in (legacy, custom):
        state.write_text(fixture + 'p01_quick_order=' + order + '\n')
        original = state.read_bytes()
        run('get', order + ',8')  # Preserve all existing positions; append Top.
        assert state.read_bytes() == original
    corrupt_orders = ('0,1,2,3,4,5,6,6', '0,1,2,3,4,5,6,8', '0,1,2',
                      '0,1,2,3,4,5,6,-1', 'x,1,2,3,4,5,6,7',
                      legacy + ',8,9', legacy + ' trailing',
                      '0,1,2,3,4,5,6,7,7', '0,1,2,3,4,5,6,7,9')
    for order in corrupt_orders:
        state.write_text(fixture + 'p01_quick_order=' + order + '\n')
        original = state.read_bytes()
        run('get', default)
        assert state.read_bytes() == original  # Loading is read-only.
    old_line = 'p01_quick_order=' + custom + '\n'
    new_line = 'p01_quick_order_v2=' + custom_v2 + '\n'
    for lines in (old_line + new_line, new_line + old_line, new_line):
        state.write_text(fixture + lines)
        original = state.read_bytes()
        run('get', custom_v2)  # v2 wins regardless of key order in the file.
        assert state.read_bytes() == original
    for order in corrupt_orders:
        invalid_v2 = 'p01_quick_order_v2=' + order + '\n'
        for lines in (old_line + invalid_v2, invalid_v2 + old_line):
            state.write_text(fixture + lines)
            original = state.read_bytes()
            run('get', custom + ',8')
            assert state.read_bytes() == original
        state.write_text(fixture + invalid_v2)
        run('get', default)
        state.write_text(fixture + new_line + invalid_v2 + old_line)
        run('get', custom_v2)  # An invalid later entry never replaces a valid v2.
    for invalid in ('magic=0\nversion=1\n', 'magic=1430868820\nversion=2\n', 'version=1\n'):
        state.write_text(invalid + 'p01_quick_order=' + custom + '\n')
        run('get', default)
    legacy_popup_off = fixture.replace('p06_reserved=1', 'p06_reserved=0') + 'p01_quick_order=' + custom + '\n'
    state.write_text(legacy_popup_off)
    run('top-default', custom + ',8')  # Missing Top preference remains independent of old Popup.
    for value in ('2', '-1', 'x', '1 trailing'):
        state.write_text(legacy_popup_off + 'top_popup_enabled=' + value + '\n')
        run('top-default', custom + ',8')
    state.write_text(legacy_popup_off + 'top_popup_enabled=0\n')
    run('top-off', custom + ',8')
    state.write_text(legacy_popup_off)
    run('top-save', custom + ',8')
    check_written_order(custom + ',8')
    run('top-off', custom + ',8')  # New process applies both independent saved switches.
    for mode in ('invalid', 'failure', 'save', 'open-failure'):
        state.write_text(fixture + 'p01_quick_order=' + custom + '\n')
        if mode == 'open-failure':
            state.with_suffix('.cfg.tmp').mkdir()
        run(mode, custom + ',8')
        if mode == 'open-failure':
            state.with_suffix('.cfg.tmp').rmdir()
        if mode in ('failure', 'save'):
            check_written_order(saved)
        else:
            run('get', custom + ',8')
    # Repeat a full nine-control save from an existing v2 file, including
    # placing Top at the first slot and checking both persisted representations.
    state.write_text(fixture + old_line + new_line)
    run('failure', custom_v2)
    check_written_order(saved)
print('PASS quick order and popup preferences: nine-control defaults, legacy eight-control append, v2 priority and corrupt fallback, Top save/swap, legacy projection, atomic reload and open/fsync/rename rollback, independent Popup/Top popup and unrelated state preservation')
