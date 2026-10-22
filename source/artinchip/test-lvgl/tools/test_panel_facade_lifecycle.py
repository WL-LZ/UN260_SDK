#!/usr/bin/env python3
"""Check actual detail-return facade functions with observable page boundaries.

The batch route also executes the production push function with an observable
page-switch boundary. Rendering and hardware require their separate suites.
"""
from pathlib import Path
import re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def function(source,name):
    match=re.search(r'^(?:static\s+)?(?:void|bool)\s+'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    start=source.index('{',match.start());depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',source[start:],re.S):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[match.start():start+token.end()]
    raise AssertionError(name)
COMMON=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int lv_obj_t; typedef int lv_timer_t;
#define UI_PAGE_MAIN 1
#define UI_PAGE_MENU 2
#define UI_PAGE_SETTING 3
#define LV_OBJ_FLAG_HIDDEN 1
#define UI_DATA_TOPIC_DEVICE_VERSION 1
static int root,timer_object;
static bool mode,panel_created,fail_panel,panel_detail,panel_rebuild_pending;
static unsigned creates,destroys,renders,resumes,cancels,pops,switches,selected_category;
static bool ui_display_mode_is_panel(void){return mode;}
static bool panel_pages_created(int page){(void)page;return panel_created;}
static bool panel_pages_resume(int page){(void)page;resumes++;return panel_created;}
static bool panel_pages_settings_category(unsigned c){selected_category=c;return panel_created;}
static lv_obj_t *lv_scr_act(void){return &root;}
static bool lv_obj_is_valid(lv_obj_t *o){return o!=NULL;}
static void lv_obj_clear_flag(lv_obj_t *o,int flags){assert(o);(void)flags;}
static void lv_obj_move_foreground(lv_obj_t *o){assert(o);}
static void lv_timer_resume(lv_timer_t *o){assert(o);resumes++;}
static void render(void){renders++;}
'''
MENU=r'''
static struct {lv_obj_t *root;lv_timer_t *timer;bool dirty,batch_dirty,user_edit,saving;} menu;
static void reset_batch(void){}
static void workspace_service_cancel_apply(void){cancels++;}
static bool ui_manager_pop_page(void){pops++;return true;}
static void ui_manager_switch(int id){assert(id==UI_PAGE_MAIN);switches++;}
static void ui_page_03_menu_destroy(void){destroys++;memset(&menu,0,sizeof(menu));panel_created=false;panel_detail=panel_rebuild_pending=false;}
static void ui_page_03_menu_create(lv_obj_t *p){assert(p);creates++;panel_created=mode&&!fail_panel;if(!panel_created){menu.root=&root;menu.timer=&timer_object;}}
static bool page_03_menu_is_created(void){return panel_created||menu.root;}
static bool menu_return_to_panel(void);
'''
MENU_TEST=r'''
int main(void){
 menu.root=&root;menu.timer=&timer_object;confirmed_back(NULL);assert(pops==1&&!creates);
 mode=panel_detail=true;menu.batch_dirty=menu.user_edit=true;confirmed_back(NULL);
 assert(panel_created&&!panel_detail&&!panel_rebuild_pending&&creates==1&&destroys==1&&pops==1);
 panel_created=false;panel_detail=true;menu.root=&root;menu.timer=&timer_object;
 unsigned n=creates;assert(ui_page_03_menu_resume()&&creates==n&&panel_detail); /* child return */
 n=renders;ui_page_03_menu_reset_navigation();assert(panel_rebuild_pending&&renders==n&&creates==1);
 assert(ui_page_03_menu_resume()&&panel_created&&creates==2&&destroys==2&&!panel_rebuild_pending);
 panel_created=false;panel_detail=true;menu.root=&root;menu.timer=&timer_object;fail_panel=true;
 assert(menu_return_to_panel()&&menu.root&&!panel_created&&!panel_detail);
 puts("PASS Menu: confirmed Back, child return, hidden root reset, deferred rebuild and allocation fallback");
}
'''
SETTINGS=r'''
#define SETTINGS_CATEGORY 0
#define SETTINGS_DIRECTORY 1
#define SETTINGS_DETAIL 2
typedef struct {const char *id,*parent;int kind;} settings_node_t;
static const settings_node_t nodes[]={
 {"device",NULL,0},{"maintenance",NULL,0},{"calibration","maintenance",1},
 {"touch","calibration",2},{"about",NULL,0},{"versions","about",2}};
static const settings_node_t *scope;
static int scroll_positions[8];
static lv_obj_t *settings_page;
static lv_timer_t *settings_timer;
static const settings_node_t *find(const char *id){for(unsigned i=0;i<6;i++)if(!strcmp(nodes[i].id,id))return nodes+i;return NULL;}
static const settings_node_t *default_scope(void){return nodes;}
static void capture_scroll(void){}
static void page_06_data_collection_refresh(void){}
static void ui_page_06_settings_refresh_data(int flags){(void)flags;}
static void settings_poll(void *p){(void)p;}
static void ui_page_06_settings_destroy(void){destroys++;settings_page=NULL;settings_timer=NULL;panel_created=false;panel_detail=panel_rebuild_pending=false;}
static void ui_page_06_settings_create(lv_obj_t *p){assert(p);creates++;panel_created=mode&&!fail_panel;if(!panel_created){settings_page=&root;settings_timer=&timer_object;}}
'''
SETTINGS_TEST=r'''
int main(void){
 settings_page=&root;settings_timer=&timer_object;scope=find("touch");
 assert(page_06_settings_back_sub_page()&&scope==find("calibration")&&!creates);
 mode=panel_detail=true;scope=find("touch");unsigned n=renders;
 assert(page_06_settings_back_sub_page()&&scope==find("calibration")&&renders==n+1&&!creates);
 assert(page_06_settings_back_sub_page()&&panel_created&&selected_category==2&&creates==1&&!panel_detail);
 panel_created=false;panel_detail=true;settings_page=&root;settings_timer=&timer_object;scope=find("versions");
 n=creates;assert(ui_page_06_settings_resume()&&scope==find("versions")&&creates==n); /* child return */
 n=renders;ui_page_06_settings_reset_navigation();assert(panel_rebuild_pending&&scope==find("versions")&&renders==n);
 assert(ui_page_06_settings_resume()&&panel_created&&scope==default_scope()&&!panel_rebuild_pending);
 panel_created=false;panel_detail=true;settings_page=&root;settings_timer=&timer_object;scope=find("versions");
 assert(page_06_settings_back_sub_page()&&panel_created&&selected_category==4);
 panel_created=false;panel_detail=true;settings_page=&root;settings_timer=&timer_object;scope=find("calibration");fail_panel=true;
 assert(page_06_settings_back_sub_page()&&!panel_created&&settings_page&&scope==find("maintenance"));
 puts("PASS Set: exact child parent, category return, hidden root reset, deferred rebuild and allocation fallback");
}
'''
# Exercise the shared batch route with the production push implementation and
# the exact Operators Preferences branch; rendering remains a boundary stub.
BATCH_ROUTE=r'''
typedef int ui_page_t;
#define UI_PAGE_INVALID 0
#define UI_PAGE_OPERATORS 4
#define UI_PAGE_STACK_CAPACITY 8
#define UI_N_(text) (text)
static struct {int current,stack[UI_PAGE_STACK_CAPACITY],stack_top;} g_page_manager;
static struct {lv_obj_t *root;lv_timer_t *timer;bool dirty,batch_dirty,user_edit;unsigned tab,sub;} menu;
static bool blocked,g_page_switch_committing;
static unsigned notes,closes;
static bool ui_factory_reset_blocks_navigation(void){return false;}
static bool ui_manager_navigation_is_blocked(void){return blocked;}
static int ui_manager_canonical_page(int page){return page;}
#define ui_manager_canonicalize_context(context,stack) ((void)(context),(void)(stack))
static bool ui_manager_page_is_registered(int page){return page>=UI_PAGE_MAIN&&page<=UI_PAGE_OPERATORS;}
static bool ui_manager_is_navigation_root(int page){return page==UI_PAGE_MAIN;}
static int ui_manager_get_current_page(void){return g_page_manager.current;}
static void ui_page_03_menu_destroy(void){destroys++;memset(&menu,0,sizeof(menu));panel_created=false;panel_detail=panel_rebuild_pending=false;}
static void ui_page_03_menu_create(lv_obj_t *parent){assert(parent);creates++;panel_created=mode&&!panel_detail&&!fail_panel;if(!panel_created){menu.root=&root;menu.timer=&timer_object;}}
static bool page_03_menu_is_created(void){return panel_created||menu.root;}
static void panel_pages_destroy(int page){assert(page==UI_PAGE_MENU);panel_created=false;}
static void reset_batch(void){}
static void workspace_service_cancel_apply(void){cancels++;}
static bool ui_manager_pop_page(void){pops++;if(g_page_manager.stack_top<0)return false;g_page_manager.current=g_page_manager.stack[g_page_manager.stack_top--];return true;}
static void ui_manager_switch(int page){
 if(blocked||g_page_switch_committing)return;
 switches++;
 if(page==UI_PAGE_MENU&&!page_03_menu_is_created())ui_page_03_menu_create(lv_scr_act());
 g_page_manager.current=page;
}
typedef struct {unsigned id;} workspace_user_t;
typedef struct {workspace_user_t active;} workspace_model_t;
static const workspace_user_t *workspace_active(const workspace_model_t *m){return m?&m->active:NULL;}
static void close_overlay(void){closes++;}
static void note(const char *text){assert(text);notes++;}
static bool menu_return_to_panel(void);
void page_03_menu_open_detail(unsigned category,unsigned subcategory);
'''
BATCH_TEST=r'''
static void reset_route(bool panel,int from){
 memset(&menu,0,sizeof(menu));mode=panel;panel_created=panel_detail=panel_rebuild_pending=fail_panel=blocked=g_page_switch_committing=false;
 creates=destroys=renders=cancels=pops=switches=notes=closes=0;
 g_page_manager.current=from;g_page_manager.stack_top=0;g_page_manager.stack[0]=91;
}
int main(void){
 const workspace_model_t model={{7}};const workspace_user_t active={7},other={8};
 for(unsigned panel=0;panel<2;panel++)for(unsigned operators=0;operators<2;operators++){
  int from=operators?UI_PAGE_OPERATORS:UI_PAGE_MAIN;reset_route(panel,from);
  if(operators)operators_preferences(&active,&model);else page_03_menu_open_batch();
  assert(g_page_manager.current==UI_PAGE_MENU&&switches==1);
  assert(g_page_manager.stack_top==1&&g_page_manager.stack[0]==91&&g_page_manager.stack[1]==from);
  assert(menu.root&&menu.tab==1&&menu.sub==0&&!panel_created&&panel_detail==(bool)panel);
  assert(!notes&&closes==operators);
  confirmed_back(NULL);
  if(panel){
   assert(panel_created&&!panel_detail&&g_page_manager.current==UI_PAGE_MENU&&!pops&&switches==1);
   assert(g_page_manager.stack_top==1&&g_page_manager.stack[1]==from); /* detail Back retains caller */
  }else assert(pops==1&&g_page_manager.current==from&&g_page_manager.stack_top==0);
 }
 reset_route(true,UI_PAGE_OPERATORS);operators_preferences(&other,&model);
 assert(notes==1&&!closes&&!switches&&!creates&&g_page_manager.stack_top==0);
 reset_route(true,UI_PAGE_OPERATORS);blocked=true;operators_preferences(&active,&model);
 assert(g_page_manager.current==UI_PAGE_OPERATORS&&!switches&&!creates&&!panel_detail&&g_page_manager.stack_top==0);
 reset_route(true,UI_PAGE_MAIN);fail_panel=true;page_03_menu_open_batch();
 assert(menu.root&&menu.tab==1&&!panel_created&&panel_detail&&g_page_manager.stack[1]==UI_PAGE_MAIN);
 puts("PASS batch route: both display modes, Main/Operators Preferences, original stack, guarded user, blocked navigation, fallback, Back to panel Menu");
}
'''

with tempfile.TemporaryDirectory(prefix='un260-facade-lifecycle-') as temp:
    work=Path(temp)
    for page,prefix,names,suffix in (
        ('page_03_menu',MENU,['confirmed_back','menu_return_to_panel','ui_page_03_menu_reset_navigation','ui_page_03_menu_resume'],MENU_TEST),
        ('page_06_settings',SETTINGS,['settings_return_to_panel','page_06_settings_back_sub_page','ui_page_06_settings_reset_navigation','ui_page_06_settings_resume'],SETTINGS_TEST)):
        source=(ROOT/'un260/lv_core'/f'{page}.c').read_text()
        unit=work/f'{page}.c';exe=work/page
        unit.write_text(COMMON+prefix+'\n'.join(function(source,name) for name in names)+suffix)
        subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable','-Werror','-fsanitize=undefined','-fno-sanitize-recover=undefined',str(unit),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    menu_source=(ROOT/'un260/lv_core/page_03_menu.c').read_text()
    manager_source=(ROOT/'un260/lv_core/lv_page_manager.c').read_text()
    operator_source=(ROOT/'un260/lv_core/page_operators.c').read_text()
    branch=next(line.strip() for line in operator_source.splitlines() if line.strip().startswith('if(id==OP_PREFERENCES){'))
    assert branch.endswith('}')
    operator_branch='static void operators_preferences(const workspace_user_t *u,const workspace_model_t *m){'+branch[branch.index('{')+1:-1]+'}'
    unit=work/'batch_route.c';exe=work/'batch_route'
    unit.write_text(COMMON+BATCH_ROUTE+function(manager_source,'ui_manager_push_page')+'\n'+
        '\n'.join(function(menu_source,name) for name in ['confirmed_back','menu_return_to_panel','page_03_menu_open_batch','page_03_menu_open_detail'])+
        operator_branch+BATCH_TEST)
    subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Wno-unused-function','-Wno-unused-variable','-Werror','-fsanitize=undefined','-fno-sanitize-recover=undefined',str(unit),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
manager=(ROOT/'un260/lv_core/lv_page_manager.c').read_text()
assert '.reset_navigation = ui_page_03_menu_reset_navigation' in manager
assert '.reset_navigation = ui_page_06_settings_reset_navigation' in manager
print('PASS both retained facade roots register navigation-session resets')
