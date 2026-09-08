#!/usr/bin/env python3
"""Exercise the real history functions with rendering isolated behind a stub."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'un260/lv_core/lv_page_manager.c').read_text()
def function(name):
    start=source.index(name+'(')
    start=source.rfind('\n',0,start)+1
    brace=source.index('{',start)
    depth=1
    end=brace+1
    while depth:
        if source[end]=='{': depth+=1
        if source[end]=='}': depth-=1
        end+=1
    return source[start:end]

# Both normal transitions and precreated-page adoption must commit root history.
assert source.count('ui_manager_history_on_commit(page);\n    g_page_manager.current = page;')==2
events=(root/'un260/lv_core/lv_page_event.c').read_text()
assert 'ui_manager_push_page(UI_PAGE_MAIN)' not in events
assert 'ui_manager_push_page(UI_PAGE_SETTING)' not in events
code=r'''
#include "un260/lv_core/lv_page_manager.h"
#include <assert.h>
#include <stdio.h>
#define UI_PAGE_STACK_CAPACITY 10
#define UI_PAGE_INVALID ((ui_page_t)-1)
static struct { ui_page_t current,stack[10];int stack_top; } g_page_manager={.current=UI_PAGE_MAIN,.stack_top=-1};
static bool g_page_switch_committing;
static unsigned session_resets;
static void ui_manager_reset_navigation_sessions(void){session_resets++;}
#define uart_debug_printf(...) ((void)0)
static bool ui_manager_page_is_registered(ui_page_t p){return p>=0 && p<UI_PAGE_COUNT;}
'''
# Keep diagnostic arguments evaluated in the stub build for -Werror.
code=code.replace('#define uart_debug_printf(...) ((void)0)',
                  'static void uart_debug_printf(const char *fmt,...){(void)fmt;}')
code+=function('ui_manager_history_on_commit')
code+=r'''
void ui_manager_switch(ui_page_t p){if(g_page_switch_committing || !ui_manager_page_is_registered(p))return;
ui_manager_history_on_commit(p);g_page_manager.current=p;}
'''
for name in ('ui_manager_push_page','ui_manager_pop_page','ui_manager_clear_stack'):
    code+=function(name)
code+=r'''
int main(void){
    /* Reproduce the old List -> Main -> Settings -> Print -> back bug. */
    ui_manager_push_page(UI_PAGE_LIST);
    assert(g_page_manager.stack_top==0);
    ui_manager_push_page(UI_PAGE_MAIN); /* protect even old callers */
    assert(g_page_manager.current==UI_PAGE_MAIN && g_page_manager.stack_top==-1);
    ui_manager_switch(UI_PAGE_SET_PASSAGE);ui_manager_switch(UI_PAGE_SETTING);
    unsigned before_back=session_resets;
    ui_manager_push_page(UI_PAGE_PRINT_SETTING);
    assert(ui_manager_pop_page() && g_page_manager.current==UI_PAGE_SETTING);
    assert(session_resets==before_back); /* Back retains Settings context. */
    assert(ui_manager_pop_page() && g_page_manager.current==UI_PAGE_MAIN);
    assert(session_resets==before_back+1); /* Home ends that session. */
    assert(!ui_manager_pop_page());
    /* Nested detail pages keep LIFO order until the root is reached. */
    ui_manager_push_page(UI_PAGE_SETTING);ui_manager_push_page(UI_PAGE_PRINT_SETTING);
    ui_manager_push_page(UI_PAGE_DEBUG);
    assert(ui_manager_pop_page() && g_page_manager.current==UI_PAGE_PRINT_SETTING);
    assert(ui_manager_pop_page() && g_page_manager.current==UI_PAGE_SETTING);
    assert(ui_manager_pop_page() && g_page_manager.current==UI_PAGE_MAIN);
    /* A direct Home/ESC discards all history, not just one entry. */
    ui_manager_push_page(UI_PAGE_MENU);ui_manager_push_page(UI_PAGE_CURR);
    ui_manager_switch(UI_PAGE_MAIN);assert(g_page_manager.stack_top==-1);
    g_page_manager.stack[0]=UI_PAGE_LIST;g_page_manager.stack_top=0;
    assert(!ui_manager_pop_page() && g_page_manager.stack_top==-1);
    /* Reentrant/invalid navigation never corrupts the stack. */
    g_page_switch_committing=true;ui_manager_push_page(UI_PAGE_MENU);
    assert(!ui_manager_pop_page() && g_page_manager.stack_top==-1);
    g_page_switch_committing=false;ui_manager_push_page((ui_page_t)999);
    assert(g_page_manager.current==UI_PAGE_MAIN && g_page_manager.stack_top==-1);
    puts("PASS: Main root invariant, List/Settings/Print reproduction, nested LIFO, direct Home, guards");
}
'''
work=Path(tempfile.mkdtemp(prefix='un260-navigation-test-'))
(work/'test.c').write_text(code)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',
               '-I'+str(root),str(work/'test.c'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Test artifacts:',work)
