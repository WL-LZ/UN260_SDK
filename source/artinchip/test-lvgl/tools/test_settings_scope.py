#!/usr/bin/env python3
"""Compile the production navigation scope and count-start routing decision."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
manager = (root / 'un260/lv_core/lv_page_manager.c').read_text()
runtime = (root / 'un260/app_service/app_counting_runtime.c').read_text()

def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[start:], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[match.start():start + token.end()]
    raise AssertionError(name)

scope = function(manager, 'ui_manager_update_diagnostic_scope')
for name in ('ui_manager_switch', 'ui_manager_adopt_precreated_page'):
    assert 'ui_manager_update_diagnostic_scope(page)' in function(manager, name)
start = function(runtime, 'app_counting_runtime_on_start_success')
decision = start[start.index('    if (data_collection_state_mode()'):start.index('    smart_island_notify_count_start();')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "un260/lv_core/lv_page_manager.h"
static bool scope_active, collection_page;
static int collection_mode, switches, refreshes;
static ui_page_t current;
#define DATA_COLLECT_MODE_NONE 0
static void work_mode_service_set_diagnostic(bool active){scope_active=active;}
static bool work_mode_service_diagnostic_active(void){return scope_active;}
static bool page_06_settings_is_collection(void){return collection_page;}
static int data_collection_state_mode(void){return collection_mode;}
static void data_collection_state_set_status(const char *s){assert(s&&s[0]);}
static void page_06_data_collection_refresh(void){refreshes++;}
ui_page_t ui_manager_get_current_page(void){return current;}
void ui_manager_switch(ui_page_t page){assert(page==UI_PAGE_MAIN);switches++;current=page;}
'''
code += scope + '\nstatic void count_started(void){\n' + decision + '\n}\n'
code += r'''
int main(void){
 for(int page=0;page<UI_PAGE_COUNT;page++){
  bool expected=page==UI_PAGE_CIS_CALIB||page==UI_PAGE_DEBUG||page==UI_PAGE_SENSOR||
   page==UI_PAGE_IMAGE_GET||page==UI_PAGE_WAVE_GET||page==UI_PAGE_MOTOR_TEST||page==UI_PAGE_AGING_SETTING;
  current=page;collection_page=false;collection_mode=0;switches=0;
  ui_manager_update_diagnostic_scope(page);assert(scope_active==expected);
  count_started();assert(switches==(expected||page==UI_PAGE_PURE?0:1));
 }
 current=UI_PAGE_SETTING;collection_page=true;switches=0;
 ui_manager_update_diagnostic_scope(current);assert(scope_active);
 count_started();assert(switches==0); /* Before choosing a collection mode. */
 collection_mode=1;count_started();assert(refreshes==1&&switches==0);
 collection_page=false;collection_mode=0;ui_manager_update_diagnostic_scope(UI_PAGE_SETTING);
 assert(!scope_active);count_started();assert(switches==1);
 puts("PASS diagnostic page whitelist before operation, collection subpage, normal/Pure routing and exit reset");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-settings-scope-') as temp:
    path=Path(temp);(path/'test.c').write_text(code)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root),str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
