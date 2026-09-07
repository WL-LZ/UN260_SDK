#!/usr/bin/env python3
"""Run the actual shared ESC dispatcher against lifetime/visibility fixtures."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'un260/lv_components/lv_nav_button.c').read_text()
assert 'lv_obj_add_event_cb(button, nav_back_marker, LV_EVENT_DELETE, button)' in src
src=src[:src.index('lv_obj_t *lv_nav_button_create')]
src='\n'.join(s for s in src.splitlines() if not s.startswith('#include'))
stub=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#define LV_UNUSED(x) ((void)(x))
#define LV_STATE_DISABLED 1
#define LV_EVENT_CLICKED 2
typedef struct {} lv_event_t;
typedef struct obj {bool visible,marked,disabled,delete_on_click;unsigned count;struct obj *child[4];} lv_obj_t;
typedef enum {LV_NAV_BACK_NONE,LV_NAV_BACK_HANDLED,LV_NAV_BACK_BLOCKED} lv_nav_back_result_t;
static lv_obj_t screen={.visible=true},top={.visible=true};
static lv_obj_t *clicked;
static unsigned clicks;
static bool lv_obj_is_visible(lv_obj_t *o){return o->visible;}
static unsigned lv_obj_get_child_cnt(lv_obj_t *o){return o->count;}
static lv_obj_t *lv_obj_get_child(lv_obj_t *o,int n){assert(n>=0 && (unsigned)n<o->count);return o->child[n];}
static void *lv_obj_get_event_user_data(lv_obj_t *o,void(*cb)(lv_event_t*)){(void)cb;return o->marked?o:NULL;}
static bool lv_obj_has_state(lv_obj_t *o,int s){assert(s==1);return o->disabled;}
static lv_obj_t *lv_layer_top(void){return &top;}
static lv_obj_t *lv_scr_act(void){return &screen;}
static void lv_event_send(lv_obj_t *o,int code,void *p){assert(code==2 && !p);clicks++;clicked=o;if(o->delete_on_click)free(o);}
'''
test=r'''
int main(void){
 lv_obj_t a={.visible=true,.marked=true},b=a,c=a;
 screen.count=2;screen.child[0]=&a;screen.child[1]=&b;
 assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED && clicked==&b);
 b.visible=false;assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED && clicked==&a);
 b.visible=true;b.disabled=true;unsigned before=clicks;
 assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED && clicks==before);
 top.count=1;top.child[0]=&c;
 assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED && clicked==&c);
 top.visible=false;assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED);
 screen.visible=false;assert(lv_nav_button_request_back()==LV_NAV_BACK_NONE);
 screen.visible=true;screen.count=1;lv_obj_t *heap=malloc(sizeof(*heap));*heap=a;heap->delete_on_click=true;
 screen.child[0]=heap;assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
 screen.count=0;assert(lv_nav_button_request_back()==LV_NAV_BACK_NONE);
 puts("nav ESC: PASS frontmost/hidden/disabled/layer/lifetime; no raw pop fallback");
}
'''
work=Path(tempfile.mkdtemp(prefix='un260-nav-test-'))
(work/'test.c').write_text(stub+src+test)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(work/'test.c'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
