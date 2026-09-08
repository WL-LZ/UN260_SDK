#!/usr/bin/env python3
"""Host tests of production memory broker and widget retry ownership."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
work=Path(tempfile.mkdtemp(prefix='un260-image-services-'))
manager=(root/'aic_ui/image_memory.c').read_text().replace('#include "image_memory.h"',(root/'aic_ui/image_memory.h').read_text())
recovery=(root/'aic_ui/image_recovery.c').read_text()
recovery='\n'.join(l for l in recovery.splitlines() if not l.startswith('#include "'))
mock=r'''
#include <assert.h>
#include <string.h>
typedef struct lv_obj_t {const char *src;bool visible;void *user;void (*cb)(void*);} lv_obj_t;
typedef struct {lv_obj_t *obj;void *user;} lv_event_t;
typedef struct {bool paused;} lv_timer_t;
#define LV_IMG_SRC_FILE 1
#define LV_EVENT_DELETE 2
static uint32_t tick,invalidations,headers;
static lv_timer_t mock_timer;
static int lv_img_src_get_type(const void*s){return s?LV_IMG_SRC_FILE:0;}
static const void *lv_img_get_src(lv_obj_t*o){return o->src;}
static void *lv_event_get_user_data(lv_event_t*e){return e->user;}
static lv_obj_t *lv_event_get_target(lv_event_t*e){return e->obj;}
static void lv_obj_remove_event_cb_with_user_data(lv_obj_t*o,void(*cb)(lv_event_t*),void*u){(void)cb;assert(o->user==u);o->user=NULL;}
static void lv_obj_add_event_cb(lv_obj_t*o,void(*cb)(lv_event_t*),int code,void*u){(void)cb;assert(code==2);assert(!o->user);o->user=u;}
static void lv_timer_resume(lv_timer_t*t){t->paused=false;}
static void lv_timer_pause(lv_timer_t*t){t->paused=true;}
static lv_timer_t *lv_timer_create(void(*cb)(lv_timer_t*),int ms,void*u){(void)cb;(void)u;assert(ms==100);return &mock_timer;}
static void lv_img_set_draw_status_cb(void(*cb)(lv_obj_t*,bool)){(void)cb;}
static void lv_img_set_draw_retry_cb(bool(*cb)(lv_obj_t*)){(void)cb;}
static bool lv_obj_is_visible(lv_obj_t*o){return o->visible;}
static uint32_t lv_tick_get(void){return tick;}
static uint32_t lv_tick_elaps(uint32_t t){return tick-t;}
static void lv_img_set_src(lv_obj_t*o,const void*s){assert(o->src==s);headers++;}
static void lv_obj_invalidate(lv_obj_t*o){assert(o->visible);invalidations++;}
'''
tests=r'''
static unsigned evictions;
static void free_scale(uint32_t bytes){(void)bytes;evictions++;image_mem_release(IMAGE_MEM_SCALE,1024*1024);}
int main(void){
    assert(image_mem_acquire(IMAGE_MEM_IMAGE,8*1024*1024));
    assert(image_mem_acquire(IMAGE_MEM_SNAPSHOT,3*1024*1024));
    assert(!image_mem_prewarm_allowed());
    assert(image_mem_acquire(IMAGE_MEM_SCALE,1024*1024));
    assert(!image_mem_acquire(IMAGE_MEM_DECODE,1));
    image_mem_register(IMAGE_MEM_SCALE,free_scale);
    assert(image_mem_acquire(IMAGE_MEM_DECODE,1024*1024) && evictions==1);
    assert(image_mem_acquire(IMAGE_MEM_CPU,4*1024*1024));
    assert(!image_mem_acquire(IMAGE_MEM_CPU,1));
    assert(!image_mem_acquire(IMAGE_MEM_IMAGE,UINT32_MAX));
    image_mem_release(IMAGE_MEM_CPU,4*1024*1024);
    image_mem_release(IMAGE_MEM_DECODE,1024*1024);
    image_mem_release(IMAGE_MEM_IMAGE,8*1024*1024);
    image_mem_release(IMAGE_MEM_SNAPSHOT,3*1024*1024);
    assert(image_mem_used()==0 && image_mem_prewarm_allowed());
    image_mem_register(IMAGE_MEM_SCALE,NULL);
    for(unsigned i=0;i<10000;i++){assert(image_mem_acquire(IMAGE_MEM_IMAGE,5));assert(image_mem_used()==4096);image_mem_release(IMAGE_MEM_IMAGE,5);}
    assert(image_mem_used()==0);
    image_recovery_init();assert(timer->paused);
    lv_obj_t a={.src="L:/a.png",.visible=true};
    status(&a,false);assert(a.user && !timer->paused);
    tick=1499;retry_tick(timer);assert(!invalidations);
    tick=1500;retry_tick(timer);assert(invalidations==1);
    tick=4500;retry_tick(timer);assert(invalidations==2);
    tick=10500;retry_tick(timer);assert(invalidations==3);
    assert(may_draw(&a));status(&a,false); /* last scheduled draw completed */
    tick=999999;retry_tick(timer);assert(invalidations==3 && !may_draw(&a));
    status(&a,true);retry_tick(timer);assert(!a.user);
    status(&a,false);lv_event_t e={.obj=&a,.user=a.user};deleted(&e);a.user=NULL;
    tick+=99999;retry_tick(timer);assert(invalidations==3);
    status(&a,false);a.visible=false;retry_tick(timer);assert(!a.user);
    a.visible=true;status(&a,false);a.src="L:/other.png";retry_tick(timer);assert(!a.user);
    assert(headers==3 && image_mem_used()==0);
    puts("PASS memory shared cap/temp claims/CPU separation/overflow/reclaim/10000 cycles; retries 3/delete/hidden/source change/success");
}
'''
c=work/'services.c';c.write_text(manager+mock+recovery+tests)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(work/'services')],check=True)
subprocess.run([str(work/'services')],check=True)
print('Artifacts:',work)
