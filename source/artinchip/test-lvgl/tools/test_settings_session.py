#!/usr/bin/env python3
"""Run production Settings reset and manager dispatch with LVGL isolated.

Cache allocation is deliberately not reset: this tests navigation state,
including the saved model and retained tiles/sidebar, not application data.
"""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
settings = (root / 'un260/lv_core/page_06_settings.c').read_text()
manager = (root / 'un260/lv_core/lv_page_manager.c').read_text()

def function(source, name):
    match = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = match.start()
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        if source[end] == '{': depth += 1
        if source[end] == '}': depth -= 1
        end += 1
    return source[start:end]

registry = manager[manager.index('static const ui_page_registration_t g_page_registry'):]
setting_entry = registry[registry.index('[UI_PAGE_SETTING]'):registry.index('[UI_PAGE_SET_PASSAGE]')]
assert '.reset_navigation = ui_page_06_settings_reset_navigation' in setting_entry
for name in ('ui_manager_switch', 'ui_manager_adopt_precreated_page'):
    assert 'ui_manager_history_on_commit(page)' in function(manager, name)
assert 'reset_navigation' not in function(settings, 'ui_page_06_settings_suspend')

code = r'''
#include <stdbool.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include "un260/lv_core/lv_page_manager.h"
enum { PAGE_06_SETTINGS_MENU_SYSTEM=0, SETTINGS_MENU_SYSTEM=0, PAGE_06_SETTINGS_SUB_NONE=0 };
static struct object { int valid; } object = {1};
static struct object *settings_page;
static int saved_menu_index, saved_content_sub_page, saved_option_menu, saved_option_context_sub_page;
static bool saved_content_internal, saved_option_internal;
static int saved_option_col, saved_option_row, nav_stack_depth;
static char saved_content_title[40];
static unsigned option_refreshes, sidebar_refreshes, content_refreshes;
static int shown_menu, shown_content;
static bool lv_obj_is_valid(struct object *o){return o && o->valid;}
static void page_06_refresh_option_state(void){
    assert(saved_option_col==-1 && saved_option_row==-1); option_refreshes++;
}
static void page_06_update_menu_state(int menu){shown_menu=menu;sidebar_refreshes++;}
static void page_06_switch_sub_page(int menu){shown_content=menu;content_refreshes++;}
'''
code += function(settings, 'page_06_reset_saved_navigation')
code += function(settings, 'ui_page_06_settings_reset_navigation')
code += r'''
static unsigned other_resets;
static void reset_other(void){other_resets++;}
static const struct { void (*reset_navigation)(void); } g_page_registry[UI_PAGE_COUNT]={
    [UI_PAGE_SETTING]={ui_page_06_settings_reset_navigation},
    [UI_PAGE_MENU]={reset_other},
};
'''
code += function(manager, 'ui_manager_reset_navigation_sessions')
code += r'''
static void dirty_navigation(void){
    saved_menu_index=2;saved_content_internal=true;saved_content_sub_page=1;
    saved_content_title[0]='U';saved_option_menu=2;saved_option_internal=true;
    saved_option_context_sub_page=1;saved_option_col=1;saved_option_row=2;
    nav_stack_depth=3;shown_menu=2;shown_content=3;
}
static void assert_reset_model(void){
    assert(saved_menu_index==0 && !saved_content_internal && saved_content_sub_page==0);
    assert(saved_content_title[0]==0 && saved_option_menu==0 && !saved_option_internal);
    assert(saved_option_context_sub_page==0 && saved_option_col==-1 && saved_option_row==-1);
    assert(nav_stack_depth==0);
}
int main(void){
    /* No widget/cache yet: still clear saved state safely. */
    dirty_navigation();ui_manager_reset_navigation_sessions();assert_reset_model();
    assert(!option_refreshes && other_resets==1);
    /* Retained page: model AND live widgets must change, without destroy/create. */
    settings_page=&object;dirty_navigation();ui_manager_reset_navigation_sessions();
    assert_reset_model();assert(shown_menu==0 && shown_content==0);
    assert(option_refreshes==1 && sidebar_refreshes==1 && content_refreshes==1);
    assert(settings_page==&object && other_resets==2);
    /* Invalid/deleted cache and repeated Home remain safe. */
    object.valid=0;dirty_navigation();ui_manager_reset_navigation_sessions();assert_reset_model();
    assert(option_refreshes==1);
    object.valid=1;ui_manager_reset_navigation_sessions();assert_reset_model();
    assert(option_refreshes==2 && other_resets==4);
    puts("PASS: cached Settings model/view reset, no-cache reset, generic dispatch, cache retained");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-settings-session-') as temporary:
    work = Path(temporary)
    (work / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-I'+str(root), str(work/'test.c'),
                    '-o', str(work/'test')], check=True)
    subprocess.run([str(work/'test')], check=True)
