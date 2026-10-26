#!/usr/bin/env python3
"""Exercise the real history functions with rendering isolated behind a stub."""
from pathlib import Path
import subprocess
import tempfile
import re

root=Path(__file__).resolve().parents[1]
source=(root/'un260/lv_core/lv_page_manager.c').read_text()
def function(name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    start=match.start()
    brace=source.index('{',start)
    depth=1
    end=brace+1
    while depth:
        if source[end]=='{': depth+=1
        if source[end]=='}': depth-=1
        end+=1
    return source[start:end]

for owner in ('ui_manager_switch','ui_manager_adopt_precreated_page'):
    body=function(owner);opening=body.index('{')
    first=body[opening+1:].lstrip().splitlines()[0]
    assert first.startswith('if (ui_factory_reset_blocks_navigation())'), owner

# Both normal transitions and precreated-page adoption must commit root history.
assert source.count('ui_manager_history_on_commit(page);\n    g_page_manager.current = page;')==2
events=(root/'un260/lv_core/lv_page_event.c').read_text()
assert 'ui_manager_push_page(UI_PAGE_MAIN)' not in events
assert 'ui_manager_push_page(UI_PAGE_SETTING)' not in events
code=r'''
#include "un260/lv_core/lv_page_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tools/test_maintenance_state.h"
#include "un260/lv_system/ui_interaction_trace.h"
#define UI_PAGE_STACK_CAPACITY 10
#define UI_PAGE_INVALID ((ui_page_t)-1)
typedef struct { ui_page_t current,stack[10];int stack_top; } ui_page_manager_context_t;
static ui_page_manager_context_t g_page_manager={.current=UI_PAGE_MAIN,.stack_top=-1},g_home_bookmark;
static bool g_home_bookmark_valid,g_temporary_home,g_restoring_home;
static bool g_page_switch_committing;
static uint64_t g_navigation_owners;
static unsigned session_resets;
static void ui_manager_reset_navigation_sessions(void){session_resets++;}
#define uart_debug_printf(...) ((void)0)
static bool ui_manager_page_is_registered(ui_page_t p){return p>=0 && p<UI_PAGE_COUNT;}
typedef struct {bool navigation_root;} ui_page_registration_t;
'''
roots=[]
registry=source[source.index('static const ui_page_registration_t g_page_registry'):]
for match in re.finditer(r'\[(UI_PAGE_\w+)\]\s*=\s*\{([^}]+)\}',registry):
    if '.navigation_root = true' in match.group(2): roots.append(match.group(1))
assert set(roots)=={'UI_PAGE_MAIN','UI_PAGE_PURE'}
code+='static const ui_page_registration_t g_page_registry[UI_PAGE_COUNT]={'+','.join('['+page+']={true}' for page in roots)+'};\n'
# Keep diagnostic arguments evaluated in the stub build for -Werror.
code=code.replace('#define uart_debug_printf(...) ((void)0)',
                  'void uart_debug_printf(const char *fmt,...){(void)fmt;}')
code+=function('ui_manager_canonical_page')
code+=function('ui_manager_canonicalize_context')
code+=function('ui_manager_navigation_is_blocked')
code+=function('ui_manager_hold_navigation')
code+=function('ui_manager_is_navigation_root')
code+=function('ui_manager_history_on_commit')
code+=function('ui_manager_navigation_on_leave')
code+=r'''
void ui_manager_switch(ui_page_t p){if(ui_factory_reset_blocks_navigation() || ui_manager_navigation_is_blocked() || g_page_switch_committing)return;
p=ui_manager_canonical_page(p);ui_manager_canonicalize_context(&g_page_manager,false);if(!ui_manager_page_is_registered(p))return;
ui_manager_navigation_on_leave(g_page_manager.current,p);ui_manager_history_on_commit(p);g_page_manager.current=p;}
'''
for name in ('ui_manager_push_page','ui_manager_pop_page','ui_manager_clear_stack','ui_manager_suspend_to_home','ui_manager_restore_from_home'):
    code+=function(name)
code+=r'''
int main(void){
    /* Old direct/push/history/Home bookmarks resolve to the single stable owner. */
    ui_manager_switch(UI_PAGE_WAVE_GET);assert(g_page_manager.current==UI_PAGE_IMAGE_GET);
    ui_manager_push_page(UI_PAGE_WAVE_GET);assert(g_page_manager.stack_top==-1);
    ui_manager_push_page(UI_PAGE_DEBUG);
    assert(g_page_manager.current==UI_PAGE_DEBUG&&g_page_manager.stack_top==0&&g_page_manager.stack[0]==UI_PAGE_IMAGE_GET);
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_IMAGE_GET);
    ui_manager_switch(UI_PAGE_MAIN);ui_manager_push_page(UI_PAGE_SETTING);ui_manager_push_page(UI_PAGE_WAVE_GET);
    assert(g_page_manager.current==UI_PAGE_IMAGE_GET&&g_page_manager.stack_top==1);
    assert(ui_manager_suspend_to_home());g_home_bookmark.current=UI_PAGE_WAVE_GET;
    assert(ui_manager_restore_from_home()&&g_page_manager.current==UI_PAGE_IMAGE_GET);
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_SETTING);
    ui_manager_switch(UI_PAGE_DEBUG);g_page_manager.stack[0]=UI_PAGE_WAVE_GET;g_page_manager.stack_top=0;
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_IMAGE_GET);
    g_page_manager.stack[0]=UI_PAGE_SETTING;g_page_manager.stack[1]=UI_PAGE_IMAGE_GET;
    g_page_manager.stack[2]=UI_PAGE_WAVE_GET;g_page_manager.stack_top=2;
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_SETTING&&g_page_manager.stack_top==-1);
    ui_manager_switch(UI_PAGE_MAIN);
    puts("PASS legacy magnetic direct/push/history/Home routes normalize to single capture owner without duplicate Back entries");
    for(unsigned owner=UI_PAGE_IMAGE_GET;owner<=UI_PAGE_WAVE_GET;owner+=UI_PAGE_WAVE_GET-UI_PAGE_IMAGE_GET){
        ui_manager_switch(UI_PAGE_SETTING);ui_manager_push_page((ui_page_t)owner);
        g_home_bookmark=g_page_manager;g_home_bookmark_valid=true;
        ui_page_manager_context_t frozen_capture=g_page_manager, frozen_bookmark=g_home_bookmark;
        ui_manager_hold_navigation((ui_page_t)owner,true);ui_manager_hold_navigation((ui_page_t)owner,true);
        ui_manager_hold_navigation(UI_PAGE_MENU,true);ui_manager_hold_navigation(UI_PAGE_MENU,false);
        for(unsigned target=0;target<UI_PAGE_COUNT;target++){
            ui_manager_push_page((ui_page_t)target);ui_manager_switch((ui_page_t)target);
            assert(!memcmp(&g_page_manager,&frozen_capture,sizeof(frozen_capture)));
        }
        assert(!ui_manager_pop_page()&&!ui_manager_suspend_to_home()&&!ui_manager_restore_from_home());
        ui_manager_clear_stack();assert(!memcmp(&g_page_manager,&frozen_capture,sizeof(frozen_capture)));
        assert(!memcmp(&g_home_bookmark,&frozen_bookmark,sizeof(frozen_bookmark))&&g_home_bookmark_valid);
        ui_manager_hold_navigation((ui_page_t)owner,false);assert(!ui_manager_navigation_is_blocked());
        assert(ui_manager_pop_page());ui_manager_switch(UI_PAGE_MAIN);
    }
    ui_manager_hold_navigation((ui_page_t)-1,true);ui_manager_hold_navigation(UI_PAGE_COUNT,true);
    assert(!ui_manager_navigation_is_blocked());
    puts("PASS capture owner navigation: all direct/push targets, Back, Home, Return and stack clear preserve state; owner-specific idempotent unlock resumes routes");
    /* While reset is pending, successful, or failed, its owner exposes the
     * same navigation lock. All real history helpers leave both chains intact. */
    ui_manager_push_page(UI_PAGE_SETTING);ui_manager_push_page(UI_PAGE_FACTORY_SETTING);
    g_home_bookmark=g_page_manager;g_home_bookmark_valid=true;
    ui_page_manager_context_t frozen=g_page_manager,bookmark=g_home_bookmark;
    unsigned resets=session_resets;
    host_factory_reset_blocked=true;
    for(unsigned page=0;page<UI_PAGE_COUNT;page++){
        ui_manager_push_page((ui_page_t)page);ui_manager_switch((ui_page_t)page);
        assert(!memcmp(&g_page_manager,&frozen,sizeof(frozen)));
    }
    assert(!ui_manager_pop_page());assert(!ui_manager_suspend_to_home());
    assert(!ui_manager_restore_from_home());
    assert(!memcmp(&g_page_manager,&frozen,sizeof(frozen)));
    assert(!memcmp(&g_home_bookmark,&bookmark,sizeof(bookmark))&&g_home_bookmark_valid);
    assert(session_resets==resets&&!g_temporary_home&&!g_restoring_home);
    host_factory_reset_blocked=false;ui_manager_switch(UI_PAGE_MAIN);
    assert(g_page_manager.stack_top==-1&&!g_home_bookmark_valid);
    puts("PASS factory-reset navigation lock: every push/direct route, Back/Home/Return preserve current page, stacks and bookmark; unlocked routes resume");
    /* Reproduce the old List -> Main -> Settings -> Print -> back bug. */
    ui_manager_push_page(UI_PAGE_LIST);
    assert(g_page_manager.stack_top==0);
    ui_manager_push_page(UI_PAGE_MAIN); /* protect even old callers */
    assert(g_page_manager.current==UI_PAGE_MAIN && g_page_manager.stack_top==-1);
    /* A temporary Home owns an independent full chain, not a Main push. */
    ui_manager_switch(UI_PAGE_SETTING);ui_manager_push_page(UI_PAGE_PRINT_SETTING);
    ui_manager_push_page(UI_PAGE_BRIGHTNESS_SETTING);
    unsigned before_suspend=session_resets;
    assert(ui_manager_suspend_to_home());
    assert(g_page_manager.current==UI_PAGE_MAIN&&g_page_manager.stack_top==-1);
    assert(session_resets==before_suspend&&g_home_bookmark_valid);
    assert(!ui_manager_suspend_to_home()); /* Repeated down keeps bookmark. */
    assert(ui_manager_restore_from_home());
    assert(g_page_manager.current==UI_PAGE_BRIGHTNESS_SETTING&&g_page_manager.stack_top==1);
    assert(session_resets==before_suspend&&!g_home_bookmark_valid);
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_PRINT_SETTING);
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_SETTING);
    assert(ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_MAIN);
    assert(!ui_manager_restore_from_home()); /* Ordinary Back is not undoable. */
    ui_manager_push_page(UI_PAGE_SETTING);assert(ui_manager_suspend_to_home());
    ui_manager_push_page(UI_PAGE_MENU);assert(!g_home_bookmark_valid);
    ui_manager_switch(UI_PAGE_MAIN);assert(!ui_manager_restore_from_home());
    for(unsigned i=0;i<20;i++){
        ui_manager_push_page(UI_PAGE_SETTING);ui_manager_push_page(UI_PAGE_CFD_LEVEL_SETTING);
        assert(ui_manager_suspend_to_home());assert(ui_manager_restore_from_home());
        assert(g_page_manager.stack_top==1);ui_manager_switch(UI_PAGE_MAIN);
    }
    puts("PASS temporary Home/Return: full chain, local session, normal ESC, unrelated navigation, repeated lifecycle");
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
    /* Selecting another theme drops the old theme's stack and temporary
     * Home bookmark. Pure cannot be popped or suspended back into Main. */
    ui_manager_push_page(UI_PAGE_SETTING);assert(ui_manager_suspend_to_home());
    assert(g_home_bookmark_valid);
    ui_manager_push_page(UI_PAGE_PURE);
    assert(g_page_manager.current==UI_PAGE_PURE&&g_page_manager.stack_top==-1);
    assert(!g_home_bookmark_valid&&!ui_manager_restore_from_home());
    assert(!ui_manager_pop_page()&&g_page_manager.current==UI_PAGE_PURE);
    assert(!ui_manager_suspend_to_home()&&g_page_manager.current==UI_PAGE_PURE);
    for(unsigned i=0;i<20;i++) {
        ui_manager_push_page(UI_PAGE_STANDBY);
        assert(g_page_manager.stack_top==0);
        if(i%2)assert(ui_manager_pop_page()); /* Touch wake. */
        else ui_manager_switch(UI_PAGE_PURE); /* Hardware Start wake. */
        assert(g_page_manager.current==UI_PAGE_PURE&&g_page_manager.stack_top==-1);
    }
    ui_manager_switch(UI_PAGE_MAIN);
    assert(!g_home_bookmark_valid&&g_page_manager.stack_top==-1);
    assert(!ui_manager_restore_from_home());
    puts("PASS independent Pure root: old history/bookmark discarded, no Back/Home exit, repeated Standby/Start wake bounded");
}
'''
for name in ('ui_manager_switch','ui_manager_adopt_precreated_page'):
    body=function(name)
    assert body.index('ui_manager_navigation_is_blocked()') < body.index('if (g_page_switch_committing)') < body.index('ui_manager_canonical_page(page)') < body.index('ui_manager_notify_page_switch'), name
assert '[UI_PAGE_WAVE_GET] =' not in registry
assert 'ui_page_dynamic_capture_create, ui_page_dynamic_capture_destroy' in registry

work=Path(tempfile.mkdtemp(prefix='un260-navigation-test-'))
(work/'test.c').write_text(code)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',
               '-I'+str(root),str(work/'test.c'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Test artifacts:',work)
