#!/usr/bin/env python3
"""Host state/mirror checks of actual edge-hint code; not a screen-render test."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/gesture/touch_feedback.c').read_text()
assert 'lv_obj_set_style_opa(' not in source, 'Touch hints must not allocate an opacity layer'
edge = source
edge = '\n'.join(line for line in edge.splitlines() if not line.startswith('#include'))
stub = r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef int16_t lv_coord_t;
typedef struct { int x, y; bool hidden; unsigned bg_opa, border_opa; } lv_obj_t;
typedef struct { int16_t x1,y1,x2,y2; } lv_area_t;
typedef struct { const lv_area_t *clip_area; } lv_draw_ctx_t;
typedef struct { lv_obj_t *target; lv_draw_ctx_t *ctx; } lv_event_t;
typedef struct { int bg_color,bg_opa,radius; } lv_draw_rect_dsc_t;
typedef struct { int color,width; } lv_draw_line_dsc_t;
typedef struct { int16_t x,y; } lv_point_t;
typedef unsigned char lv_opa_t;
typedef struct lv_timer_t { bool paused; } lv_timer_t;
static lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),unsigned period,void *data) {
    (void)cb;(void)period;(void)data;return calloc(1,sizeof(lv_timer_t));
}
static void lv_timer_pause(lv_timer_t *t){t->paused=true;}
static void lv_timer_resume(lv_timer_t *t){t->paused=false;}
static void lv_timer_reset(lv_timer_t *t){(void)t;}
static bool enabled=true;
static void user_cfg_touch_feedback_load(void){}
static bool user_cfg_touch_feedback_enabled(void){return enabled;}
static bool user_cfg_touch_feedback_save(bool e){enabled=e;return true;}
static uint8_t lv_port_indev_touch_points(lv_point_t *p,int32_t *ids,uint8_t n){(void)p;(void)ids;(void)n;return 0;}
static lv_obj_t layer;
static unsigned tick;
static unsigned char raster[2][72*48];
typedef struct { const lv_area_t *blend_area,*mask_area; unsigned char *mask_buf; int mask_res,color,opa; } lv_draw_sw_blend_dsc_t;
#define LV_DRAW_MASK_RES_CHANGED 1
static void lv_draw_sw_blend(lv_draw_ctx_t *c,const lv_draw_sw_blend_dsc_t *d){
    (void)c;
    assert(d->blend_area->x1>=0 && d->blend_area->x2<=1279);
    assert(d->mask_res==LV_DRAW_MASK_RES_CHANGED);
    memcpy(raster[d->color==0xffffff],d->mask_buf,72*48);
}
#define LV_OBJ_FLAG_CLICKABLE 1
#define LV_OBJ_FLAG_SCROLLABLE 2
#define LV_OBJ_FLAG_HIDDEN 4
#define LV_OBJ_FLAG_FLOATING 8
#define LV_OBJ_FLAG_IGNORE_LAYOUT 16
#define LV_EVENT_DRAW_MAIN 1
#define LV_OPA_80 204
#define LV_OPA_COVER 255
#define LV_OPA_TRANSP 0
#define LV_RADIUS_CIRCLE 32767
#define LV_UNUSED(x) ((void)(x))
#define LV_SYMBOL_RIGHT ">"
#define LV_SYMBOL_LEFT "<"
static lv_obj_t *lv_layer_sys(void) { return &layer; }
static lv_obj_t *lv_obj_create(lv_obj_t *p) { (void)p; return calloc(1,sizeof(lv_obj_t)); }
#define lv_label_create lv_obj_create
static void lv_obj_set_x(lv_obj_t *o, int x) { o->x=x; }
static void lv_obj_set_y(lv_obj_t *o, int y) { o->y=y; }
static void lv_obj_set_pos(lv_obj_t *o,int x,int y){o->x=x;o->y=y;}
static void lv_obj_set_style_bg_opa(lv_obj_t *o,unsigned opa,int s){(void)s;o->bg_opa=opa;}
static void lv_obj_set_style_border_opa(lv_obj_t *o,unsigned opa,int s){(void)s;o->border_opa=opa;}
static void lv_obj_add_flag(lv_obj_t *o,int f) { if(f & 4)o->hidden=true; }
static void lv_obj_clear_flag(lv_obj_t *o,int f) { if(f & 4)o->hidden=false; }
static bool lv_obj_has_flag(lv_obj_t *o,int f) { return f==4 && o->hidden; }
static int lv_disp_get_hor_res(void *p) { (void)p; return 1280; }
static unsigned lv_tick_get(void) { return tick; }
static unsigned lv_tick_elaps(unsigned t) { return tick-t; }
static void lv_obj_invalidate(lv_obj_t *o){(void)o;}
static void lv_obj_get_coords(lv_obj_t *o,lv_area_t *a){*a=(lv_area_t){o->x,o->y,o->x+47,o->y+71};}
static lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t *e){return e->ctx;}
static lv_obj_t *lv_event_get_target(lv_event_t *e){return e->target;}
static bool _lv_area_intersect(lv_area_t *r,const lv_area_t *a,const lv_area_t *b){
    *r=(lv_area_t){a->x1>b->x1?a->x1:b->x1,a->y1>b->y1?a->y1:b->y1,
        a->x2<b->x2?a->x2:b->x2,a->y2<b->y2?a->y2:b->y2};
    return r->x1<=r->x2 && r->y1<=r->y2;
}
static int lv_color_hex(int c){return c;}
static int lv_color_white(void){return 0xffffff;}
#define lv_obj_add_event_cb(o,cb,e,u) ((void)(cb))
#define lv_obj_remove_style_all(...) ((void)0)
#define lv_obj_set_size(...) ((void)0)
#define lv_obj_set_style_radius(...) ((void)0)
#define lv_obj_set_style_bg_color(...) ((void)0)
#define lv_obj_set_style_border_color(...) ((void)0)
#define lv_obj_set_style_border_width(...) ((void)0)
#define lv_obj_set_style_text_font(o,f,s) ((void)(f))
#define lv_obj_set_style_text_color(...) ((void)0)
#define lv_label_set_text(...) ((void)0)
#define lv_obj_center(...) ((void)0)
#define lv_obj_move_foreground(...) ((void)0)
#define uart_debug_printf(...) ((void)0)
'''
test = r'''
int main(void) {
    unsigned char left[16][2][72*48];
    lv_area_t screen={0,0,1279,399};
    lv_draw_ctx_t ctx={&screen};
    for(int distance=18; distance<=160; distance+=7) {
        for(int side=1;side>=-1;side-=2) {
            touch_feedback_edge_hint(side,distance,200);
            int shown=g_edge_extent;
            touch_feedback_edge_hint(0,0,0);
            assert(g_edge_extent==shown && !g_edge_hint->hidden);
            for(int i=0;i<20;i++) touch_feedback_edge_hint(0,0,0);
            assert(g_edge_elapsed==0);
            for(int i=0;i<16;i++) {
                lv_event_t event={g_edge_hint,&ctx};
                edge_hint_draw(&event);
                assert(ctx.clip_area==&screen);
                if(g_edge_extent>0) {
                    if(side==1)memcpy(left[i],raster,sizeof(raster));
                    else for(int p=0;p<2;p++) for(int y=0;y<72;y++) for(int x=0;x<48;x++)
                        assert(left[i][p][y*48+x]==raster[p][y*48+47-x]);
                }
                int before=g_edge_extent;
                tick+=20;edge_hint_return_step(g_edge_timer);
                assert(g_edge_extent<=before && g_edge_extent>=0);
                assert(g_edge_hint->x==(side>0?0:1280-48));
            }
            assert(g_edge_hint->hidden && !g_edge_returning && g_edge_timer->paused);
        }
    }
    touch_feedback_edge_hint(1,120,200);
    touch_feedback_edge_hint(0,0,0);
    tick+=400;edge_hint_return_step(g_edge_timer);
    assert(g_edge_elapsed==32 && !g_edge_hint->hidden && g_edge_returning);
    tick+=100;edge_hint_return_step(g_edge_timer);
    assert(g_edge_elapsed==32); /* No timer-only completion without a drawn frame. */
    touch_feedback_edge_hint(-1,100,200);
    assert(g_edge_timer->paused && !g_edge_returning && !g_edge_hint->hidden);
    assert(g_edge_extent==44 && g_edge_hint->x==1280-48);
    /* A single pixel of visible capsule must survive at BOTH display edges. */
    for(int extent=1;extent<=48;extent++) {
        unsigned char mask_left[72*48];
        for(int side=1;side>=-1;side-=2) {
            touch_feedback_edge_hint(side,100,200);
            g_edge_extent=extent;
            lv_event_t event={g_edge_hint,&ctx};edge_hint_draw(&event);
            unsigned sum=0;
            for(int i=0;i<72*48;i++)sum+=raster[0][i];
            assert(sum>0);
            if(side==1)memcpy(mask_left,raster[0],sizeof(mask_left));
            else for(int y=0;y<72;y++)for(int x=0;x<48;x++)
                assert(mask_left[y*48+x]==raster[0][y*48+47-x]);
        }
    }
    lv_point_t point={100,200};
    tick=1000;touch_feedback_init();touch_feedback_sample(&point,1);
    touch_feedback_sample(&point,0);
    tick=1099;fade(g_timer);assert(!g_contacts[0].ring->hidden && g_contacts[0].ring->bg_opa==255);
    tick=1100;fade(g_timer);assert(!g_contacts[0].ring->hidden && g_contacts[0].ring->border_opa==255);
    for(tick=1120;tick<=1220;tick+=20)fade(g_timer);
    assert(g_contacts[0].ring->bg_opa==127 && g_contacts[0].ring->border_opa==127);
    for(tick=1240;tick<=1340;tick+=20)fade(g_timer);
    assert(g_contacts[0].ring->hidden && g_timer->paused);
    tick=2000;touch_feedback_sample(&point,1);touch_feedback_sample(&point,0);
    tick=2400;fade(g_timer);
    assert(!g_contacts[0].ring->hidden && g_contacts[0].fade_elapsed==32);
    assert(g_contacts[0].ring->bg_opa>0 && g_contacts[0].ring->bg_opa<255);
    touch_feedback_sample(&point,1);
    assert(!g_contacts[0].fading && g_contacts[0].ring->bg_opa==255 && g_contacts[0].ring->border_opa==255);
    touch_feedback_sample(&point,2);assert(g_contacts[0].ring->hidden);
    touch_feedback_sample(&point,0);touch_feedback_sample(&point,1);
    touch_feedback_set_enabled(false);assert(g_contacts[0].ring->hidden && g_timer->paused);
    /* Unsigned tick wrap must preserve hold and primitive fade. */
    enabled=true;tick=UINT32_MAX-50;touch_feedback_sample(&point,1);touch_feedback_sample(&point,0);
    tick+=99;fade(g_timer);assert(g_contacts[0].ring->bg_opa==255);
    tick+=21;fade(g_timer);assert(g_contacts[0].ring->bg_opa<255 && !g_contacts[0].ring->hidden);
    puts("PASS: mirrored edge, primitive bg/border fade, 100+240ms, stalls, retouch, multitouch/disable, tick wrap");
}
'''
work = Path(tempfile.mkdtemp(prefix='un260-edge-hint-test-'))
(work/'test.c').write_text(stub+'\n'+edge+'\n'+test)
subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                str(work/'test.c'),'-o',str(work/'test')],check=True)
subprocess.run([str(work/'test')],check=True)
print('Test artifacts:', work)
