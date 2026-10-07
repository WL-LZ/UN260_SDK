#!/usr/bin/env python3
"""Actual upgrade-hide functions on real LVGL, including mid-entry reversal."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'un260/lv_components/lv_upgrade_popup.c').read_text()
def function(name):
    m=re.search(r'^static [\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert m,name
    begin=source.index('{',m.start());depth=0
    for t in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',source[begin:],re.S):
        if t[0]=='{':depth+=1
        elif t[0]=='}':
            depth-=1
            if not depth:return source[m.start():begin+t.end()]
    raise AssertionError(name)
code=r'''
#include "lvgl/lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define UPGRADE_POPUP_HIDE_TIME_MS HIDE_DURATION
enum {UPGRADE_POPUP_STATE_PROMPT,UPGRADE_POPUP_STATE_PROGRESS,UPGRADE_POPUP_STATE_SUCCESS,UPGRADE_POPUP_STATE_RESTART,UPGRADE_POPUP_STATE_FAIL};
static struct {lv_obj_t *root,*prompt_card,*prompt_icon,*prompt_title_group,*prompt_content_group;
lv_obj_t *success_card,*success_icon_wrap,*success_title_group,*success_content_group;
lv_obj_t *fail_card,*fail_icon_wrap,*fail_title_group,*fail_content_group;int state;bool prompt_hiding;}g_upgrade_popup;
static unsigned destroyed,resets;static bool fail_alloc;
void *__real_lv_mem_alloc(size_t);lv_obj_t *__real_lv_obj_create(lv_obj_t *);
void *__wrap_lv_mem_alloc(size_t n){if(fail_alloc){fail_alloc=false;return NULL;}return __real_lv_mem_alloc(n);}
lv_obj_t *__wrap_lv_obj_create(lv_obj_t *p){return __real_lv_obj_create(p);}
static void ui_upgrade_service_reset(void){resets++;}
static void upgrade_popup_destroy(void){assert(g_upgrade_popup.root);lv_obj_del(g_upgrade_popup.root);g_upgrade_popup.root=NULL;destroyed++;}
static void upgrade_popup_prompt_hide_ready_cb(lv_anim_t *a){(void)a;upgrade_popup_destroy();}
static void upgrade_popup_result_hide_ready_cb(lv_anim_t *a){(void)a;upgrade_popup_destroy();}
'''
for name in ['upgrade_popup_anim_opa_cb','upgrade_popup_anim_card_opa_cb','upgrade_popup_anim_translate_y_cb','upgrade_popup_hide']:
    code+=function(name)+'\n'
code+=r'''
static void advance(unsigned ms){while(ms){unsigned step=ms>10?10:ms;lv_tick_inc(step);lv_timer_handler();ms-=step;}}
static lv_color_t pixels[1280*20];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){(void)a;(void)p;lv_disp_flush_ready(d);}
static void fixture(int state,lv_obj_t **parts){
    memset(&g_upgrade_popup,0,sizeof(g_upgrade_popup));g_upgrade_popup.state=state;
    g_upgrade_popup.root=lv_obj_create(lv_scr_act());
    for(unsigned i=0;i<4;i++){parts[i]=lv_obj_create(g_upgrade_popup.root);lv_obj_remove_style_all(parts[i]);}
    g_upgrade_popup.prompt_card=g_upgrade_popup.success_card=g_upgrade_popup.fail_card=parts[0];
    g_upgrade_popup.prompt_icon=g_upgrade_popup.success_icon_wrap=g_upgrade_popup.fail_icon_wrap=parts[1];
    g_upgrade_popup.prompt_title_group=g_upgrade_popup.success_title_group=g_upgrade_popup.fail_title_group=parts[2];
    g_upgrade_popup.prompt_content_group=g_upgrade_popup.success_content_group=g_upgrade_popup.fail_content_group=parts[3];
    for(unsigned i=0;i<4;i++){
        lv_anim_t a;lv_anim_init(&a);lv_anim_set_var(&a,parts[i]);
        lv_anim_set_exec_cb(&a,i?upgrade_popup_anim_opa_cb:upgrade_popup_anim_card_opa_cb);
        lv_anim_set_values(&a,0,255);lv_anim_set_time(&a,400);lv_anim_start(&a);
        lv_anim_set_exec_cb(&a,upgrade_popup_anim_translate_y_cb);lv_anim_set_values(&a,24,0);lv_anim_start(&a);
    }
}
int main(void){
    lv_init();static lv_disp_draw_buf_t buffer;static lv_disp_drv_t display;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*20);lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;assert(lv_disp_drv_register(&display));
    unsigned times[]={0,20,80,200,360,420};
    for(int state=0;state<=UPGRADE_POPUP_STATE_FAIL;state++){
        if(state==UPGRADE_POPUP_STATE_PROGRESS)continue;
        for(unsigned n=0;n<sizeof(times)/sizeof(times[0]);n++){
            lv_obj_t *parts[4];fixture(state,parts);advance(times[n]);
            unsigned opa[4];int y[4];for(unsigned i=0;i<4;i++){opa[i]=i?lv_obj_get_style_opa(parts[i],0):lv_obj_get_style_bg_opa(parts[i],0);y[i]=lv_obj_get_style_translate_y(parts[i],0);}
            unsigned old=destroyed,reset=resets;upgrade_popup_hide();
            for(unsigned i=0;i<4;i++){assert(opa[i]==(i?lv_obj_get_style_opa(parts[i],0):lv_obj_get_style_bg_opa(parts[i],0)));assert(y[i]==lv_obj_get_style_translate_y(parts[i],0));}
            upgrade_popup_hide();assert(resets==reset+1);
            advance(80);assert(destroyed==old);
            for(unsigned i=0;i<4;i++)assert((i?lv_obj_get_style_opa(parts[i],0):lv_obj_get_style_bg_opa(parts[i],0))<=opa[i]);
            advance(UPGRADE_POPUP_HIDE_TIME_MS);assert(destroyed==old+1 && lv_anim_count_running()==0);
        }
    }
    lv_obj_t *parts[4];fixture(UPGRADE_POPUP_STATE_PROMPT,parts);advance(80);fail_alloc=true;
    unsigned old=destroyed;upgrade_popup_hide();assert(destroyed==old+1 && lv_anim_count_running()==0);
    fixture(UPGRADE_POPUP_STATE_PROGRESS,parts);unsigned reset=resets;upgrade_popup_hide();assert(g_upgrade_popup.root&&resets==reset);upgrade_popup_destroy();
    advance(500);assert(lv_anim_count_running()==0);lv_deinit();
    puts("PASS real LVGL upgrade motion: 24 mid-entry/result reversals, repeated close, allocation failure, progress guard, no stale callbacks");
}
'''
code=code.replace('HIDE_DURATION',re.search(r'#define\s+UPGRADE_POPUP_HIDE_TIME_MS\s+(\d+)',source)[1])
with tempfile.TemporaryDirectory(prefix='un260-upgrade-motion-') as tmp:
    harness=Path(tmp)/'motion.c';harness.write_text(code)
    subprocess.run(['python3',str(root/'tools/test_content_pager.py'),'--harness',str(harness)],check=True)
