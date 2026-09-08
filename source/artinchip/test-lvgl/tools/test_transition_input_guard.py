#!/usr/bin/env python3
"""Compile actual manager guard functions; nested owners may not unlock input."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root/'un260/lv_core/lv_page_manager.c').read_text(encoding='utf-8')
def function(name):
    m = re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{', source, re.M)
    assert m
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[m.end()-1:], re.S):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return source[m.start():m.end()+token.end()-1]
code = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#define LV_UNUSED(x) (void)(x)
typedef int ui_page_t;
enum {UI_PAGE_BOOT_ANIM=0,UI_PAGE_COUNT=34};
typedef int lv_timer_t;
typedef int lv_indev_t;
static uint64_t g_transition_input_owners;
static bool g_page_switch_committing, enabled=true;
static lv_timer_t *g_page_input_unlock_timer;
static lv_indev_t device;
static lv_indev_t *lv_indev_get_next(lv_indev_t *p) {return p ? NULL : &device;}
static void lv_indev_enable(lv_indev_t *p,bool value) {assert(p==&device);enabled=value;}
static void lv_timer_del(lv_timer_t *timer) {(void)timer;}
'''
code += '\n'.join(function(n) for n in ('ui_manager_set_input_enabled','ui_manager_hold_transition_input','ui_manager_input_unlock_cb'))
code += r'''
int main(void) {
    lv_timer_t timer=0;
    ui_manager_hold_transition_input(32,true); assert(!enabled);
    ui_manager_hold_transition_input(32,true); /* same owner idempotent */
    ui_manager_hold_transition_input(2,true);
    ui_manager_hold_transition_input(32,false); assert(!enabled);
    ui_manager_hold_transition_input(2,false); assert(enabled);
    ui_manager_hold_transition_input(32,true);
    g_page_switch_committing=true;
    ui_manager_hold_transition_input(32,false); assert(!enabled);
    g_page_switch_committing=false;g_page_input_unlock_timer=&timer;
    ui_manager_hold_transition_input(32,true);
    ui_manager_input_unlock_cb(&timer);assert(!enabled && !g_page_input_unlock_timer);
    ui_manager_hold_transition_input(32,false);assert(enabled);
    ui_manager_hold_transition_input(-1,true);assert(enabled);
    ui_manager_hold_transition_input(UI_PAGE_COUNT,true);assert(enabled);
    puts("Transition input guard: PASS owner overlap, timer ordering, invalid owners");
}
'''
cc = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
with tempfile.TemporaryDirectory(prefix='un260-guard-') as work:
    path = Path(work)/'test.c'; path.write_text(code, encoding='utf-8')
    exe = Path(work)/('test.exe' if os.name == 'nt' else 'test')
    subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
