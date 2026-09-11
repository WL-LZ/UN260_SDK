#!/usr/bin/env python3
"""Host regression of the real Currency adapter, physics and view projection.

The small LVGL event model below exercises bubbling/press ownership and timer
lifetime, not pixel rasterization or the board touch/display drivers.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def without_includes(source):
    return "\n".join(line for line in source.splitlines()
                     if not line.startswith("#include"))


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, name
    opening = source.index("{", signature.start())
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[signature.start():token.end()]
    raise AssertionError(name)


page = (ROOT / "un260/lv_core/page_07_curr.c").read_text(encoding="utf-8")
adapter_h = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_carousel.h").read_text(encoding="utf-8")
adapter = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_carousel.c").read_text(encoding="utf-8")
physics_h = (ROOT / "un260/lv_components/ui_scroll_physics.h").read_text(encoding="utf-8")
physics = (ROOT / "un260/lv_components/ui_scroll_physics.c").read_text(encoding="utf-8")
surface_h = (ROOT / "un260/lv_components/lv_card_surface.h").read_text(encoding="utf-8")
surface = (ROOT / "un260/lv_components/lv_card_surface.c").read_text(encoding="utf-8")
internal = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_internal.h").read_text(encoding="utf-8")
layout = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_layout.h").read_text(encoding="utf-8")
view = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_view.c").read_text(encoding="utf-8")
renderer_h = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_card_render.h").read_text(encoding="utf-8")
renderer = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_card_render.c").read_text(encoding="utf-8")
cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
card_layout = re.search(r"typedef struct \{[^{}]*\} page07_curr_card_t;", internal).group()
geometry = without_includes(layout)

# Architecture is part of the regression: host stubs must not hide a new
# dependency on an unrelated, uncommitted display or application subsystem.
physics_code = re.sub(r"/\*.*?\*/|//[^\n]*", "", physics_h + physics, flags=re.S)
for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', physics_code, re.M):
    assert include in {"stdbool.h", "stdint.h", "math.h", "string.h", "ui_scroll_physics.h"}, include
assert not re.search(r"\b(?:lv_\w+|page\w+|g_page\w+|printf|fprintf|fopen|fwrite|open|write|read|malloc|calloc|free)\s*\(", physics_code)
assert "lv_port_disp" not in adapter + adapter_h
assert "fbdev_present_sequence" not in adapter + adapter_h
assert "perf_profile_frame_sequence()" in adapter
for implementation in ("curr_set_card_render_state", "curr_card_snapshot_key",
                       "curr_acquire_card_snapshot", "curr_card_cache_wanted"):
    assert implementation not in page, implementation
    assert function(renderer, implementation)
assert "lv_dma_snapshot_cache_acquire" not in page
assert "lv_dma_snapshot_cache_release" not in page
for module in ("un260/lv_components/ui_scroll_physics.c",
               "un260/lv_components/lv_card_surface.c",
               "un260/lv_core/page_07_curr/page_07_curr_carousel.c",
               "un260/lv_core/page_07_curr/page_07_curr_card_render.c",
               "un260/lv_core/page_07_curr/page_07_curr_view.c"):
    assert module in cmake, module

# The replaced mechanisms must not survive as a second position owner.
for obsolete in ("curr_scroll_x_abs", "curr_right_drag_cb", "curr_start_snap_timer",
                 "curr_animate_overscroll_back", "CURR_SEL_NEXT_EXTRA_GAP",
                 "CURR_CARD_PAD_RIGHT", "lv_obj_scroll_to_x", "selected_cache;"):
    assert obsolete not in page + renderer + adapter + layout, obsolete
projection = function(page, "curr_project_carousel")
for forbidden in ("lv_img_set_src", "lv_img_set_zoom", "lv_dma_snapshot_cache",
                  "curr_set_left_info_by_abs", "model.selected_abs_idx =",
                  "model.selected_visible_idx =", "page07_curr_model_save"):
    assert forbidden not in projection, forbidden
assert "CURR_CAROUSEL_V4_" in renderer
assert "CURR_CAROUSEL_V3_" not in page + renderer
assert "lv_card_surface_focus_mark_apply(card->focus_mark, focused)" in function(renderer, "curr_set_card_render_state")
cached_apply = function(renderer, "page07_curr_card_render_apply")
assert "!card->has_scaled_flag" in cached_apply
assert "card->using_cache = false" in cached_apply
assert "lv_obj_create" not in function(surface, "lv_card_surface_focus_mark_apply")
assert "CURR_CARD_UNSELECTED_BG_PATH" not in page
assert "page07_curr_card_render_sync_snapshots" in projection
assert "acquire_or_create" not in function(renderer, "page07_curr_card_render_sync_snapshots")
assert "uart_debug_printf" in function(adapter, "trace")
assert "page07_curr_carousel_destroy" in function(page, "curr_refresh_right_views")
assert "page07_curr_carousel_destroy" in function(page, "page_07_curr_img_reset")
assert "page07_curr_carousel_enable(&g_page07_curr.carousel, false)" in function(page, "ui_page_07_curr_suspend")
assert "page07_curr_carousel_busy" in function(page, "curr_snapshot_prewarm_timer_cb")
assert "page07_curr_carousel_busy" in function(page, "ui_page_07_curr_prepare_static_step")
assert "page07_curr_carousel_bind_child(g_page07_curr.cards[i].fav_btn)" in page
assert "page07_curr_carousel_bind_child(g_page07_curr.objects.list)" in page
assert "g_page07_curr.cards[i].has_scaled_flag = true" in page

stubs = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
typedef struct { int x,y; } lv_point_t;
typedef struct lv_obj lv_obj_t;
typedef struct lv_event lv_event_t;
typedef int lv_event_code_t;
typedef void (*event_cb_t)(lv_event_t *);
struct lv_obj { int x,y,w,h,border; unsigned flags; lv_obj_t *parent;
    unsigned background,bg_opa,border_color,radius,text_color,img_opa,recolor_opa;
    unsigned pad,shadow,outline; int align,align_x,align_y;
    unsigned y_writes,pos_writes; bool favorite_ui,focus_marker; const void *source;
    struct { event_cb_t cb; int code; void *data; } events[6]; unsigned event_count;
};
struct lv_event { int code; lv_obj_t *target,*current; void *data; };
typedef struct lv_timer { void (*cb)(struct lv_timer *); void *user_data;
    bool paused,dead; unsigned period; } lv_timer_t;
typedef struct { int unused; } lv_indev_t;
enum { LV_EVENT_ALL,LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED,
       LV_EVENT_PRESS_LOST,LV_EVENT_CLICKED,LV_EVENT_DELETE };
enum { LV_OBJ_FLAG_SCROLLABLE=1,LV_OBJ_FLAG_EVENT_BUBBLE=2,
       LV_OBJ_FLAG_CLICKABLE=4,LV_OBJ_FLAG_HIDDEN=8,TEST_PRESS_LOCK=16 };
static lv_timer_t timers[128];
static unsigned timer_count,timer_live,timer_deleted;
static bool fail_timer,has_indev=true,profile_enabled;
static lv_indev_t indev;
static lv_point_t pointer;
static uint32_t now_ms,presents;
static unsigned project_count,x_sets,y_sets,border_sets,trace_count,source_sets,palette_sets;
static unsigned pos_sets,size_sets,favorite_source_sets,favorite_palette_sets;
static unsigned marker_writes,object_creates;
static lv_obj_t created_objects[34];
static uint64_t fake_us; static unsigned clock_reads;
static char last_trace[512];
static uint32_t lv_tick_get(void) { return now_ms; }
static bool perf_profile_is_enabled(void) { return profile_enabled; }
static uint32_t perf_profile_frame_sequence(void) { return presents; }
static int trace_printf(const char *format,...) {
    va_list args; va_start(args,format);
    int result=vsnprintf(last_trace,sizeof(last_trace),format,args);
    va_end(args);trace_count++;return result;
}
#define uart_debug_printf trace_printf
static uint64_t app_clock_monotonic_us(void) { clock_reads++;fake_us+=7;return fake_us; }
static uint32_t app_clock_elapsed_us32(uint64_t begin,uint64_t end) { return (uint32_t)(end-begin); }
static lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),unsigned period,void *data) {
    if(fail_timer) return NULL;
    assert(timer_count<128);lv_timer_t *t=&timers[timer_count++];
    *t=(lv_timer_t){cb,data,false,false,period};timer_live++;return t;
}
static void lv_timer_del(lv_timer_t *t) { assert(t && !t->dead);t->dead=true;timer_live--;timer_deleted++; }
static void lv_timer_pause(lv_timer_t *t) { assert(t && !t->dead);t->paused=true; }
static void lv_timer_resume(lv_timer_t *t) { assert(t && !t->dead);t->paused=false; }
static void lv_timer_reset(lv_timer_t *t) { assert(t && !t->dead); }
static void marker_write(lv_obj_t *o) { assert(o);if(o->focus_marker)marker_writes++; }
lv_obj_t *lv_obj_create(lv_obj_t *parent) {
    assert(object_creates<34);lv_obj_t *o=&created_objects[object_creates++];
    *o=(lv_obj_t){.parent=parent,.flags=LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE};return o;
}
static void lv_obj_remove_style_all(lv_obj_t *o) { marker_write(o); }
static void lv_obj_clear_flag(lv_obj_t *o,unsigned flags) { marker_write(o);o->flags&=~flags; }
static void lv_obj_add_flag(lv_obj_t *o,unsigned flags) { marker_write(o);o->flags|=flags; }
static void lv_port_indev_set_drag_obj(lv_obj_t *o,bool drag) { if(drag)o->flags|=TEST_PRESS_LOCK; }
static void lv_obj_add_event_cb(lv_obj_t *o,event_cb_t cb,int code,void *data) {
    assert(o->event_count<6);unsigned i=o->event_count++;
    o->events[i].cb=cb;o->events[i].code=code;o->events[i].data=data;
}
static void *lv_event_get_user_data(lv_event_t *e) { return e->data; }
static int lv_event_get_code(lv_event_t *e) { return e->code; }
static lv_obj_t *lv_event_get_target(lv_event_t *e) { return e->target; }
static lv_indev_t *lv_indev_get_act(void) { return has_indev?&indev:NULL; }
static void lv_indev_get_point(lv_indev_t *i,lv_point_t *p) { assert(i);*p=pointer; }
static int lv_obj_get_x(lv_obj_t *o) { return o->x; }
static void lv_obj_set_x(lv_obj_t *o,int x) { marker_write(o);o->x=x;x_sets++; }
static void lv_obj_set_y(lv_obj_t *o,int y) { marker_write(o);o->y=y;y_sets++;o->y_writes++; }
static void lv_obj_set_pos(lv_obj_t *o,int x,int y) { marker_write(o);o->x=x;o->y=y;pos_sets++;o->pos_writes++; }
static void lv_obj_set_size(lv_obj_t *o,int w,int h) { marker_write(o);o->w=w;o->h=h;size_sets++; }
static void lv_obj_align(lv_obj_t *o,int align,int x,int y) {
    marker_write(o);o->align=align;o->align_x=x;o->align_y=y;
}
static void lv_obj_set_style_border_width(lv_obj_t *o,int width,int selector) {
    (void)selector;marker_write(o);o->border=width;border_sets++;
}
static unsigned lv_color_hex(unsigned value) { return value; }
static void lv_obj_set_style_bg_color(lv_obj_t *o,unsigned color,int selector) {
    (void)selector;marker_write(o);o->background=color;palette_sets++;if(o->favorite_ui)favorite_palette_sets++;
}
static void lv_obj_set_style_bg_opa(lv_obj_t *o,unsigned opa,int selector) {
    (void)selector;marker_write(o);o->bg_opa=opa;palette_sets++;if(o->favorite_ui)favorite_palette_sets++;
}
static void lv_img_set_src(lv_obj_t *o,const void *source) {
    marker_write(o);o->source=source;source_sets++;if(o->favorite_ui)favorite_source_sets++;
}
static const void *lv_img_get_src(lv_obj_t *o) { assert(o);return o->source; }
static bool lv_obj_has_flag(lv_obj_t *o,unsigned flag) { assert(o);return (o->flags&flag)!=0; }
static bool lv_obj_is_valid(lv_obj_t *o) { return o!=NULL; }
typedef unsigned lv_style_selector_t;
enum { LV_PART_MAIN=0,LV_STATE_DEFAULT=0,LV_GRAD_DIR_NONE=0,LV_BORDER_SIDE_FULL=15,
       LV_OPA_COVER=255,LV_OPA_TRANSP=0,LV_OPA_0=0,LV_OPA_40=102,
       LV_RADIUS_CIRCLE=0x7FFF,LV_ALIGN_BOTTOM_MID=8 };
#define STORE_STYLE(name,field) static void lv_obj_set_style_##name(lv_obj_t *o,unsigned value,unsigned s) { \
    (void)s;marker_write(o);o->field=value;palette_sets++; }
STORE_STYLE(radius,radius)
STORE_STYLE(border_color,border_color)
STORE_STYLE(text_color,text_color)
STORE_STYLE(img_opa,img_opa)
STORE_STYLE(img_recolor_opa,recolor_opa)
STORE_STYLE(pad_all,pad)
STORE_STYLE(shadow_width,shadow)
STORE_STYLE(outline_width,outline)
#define COUNT_STYLE(name) static void lv_obj_set_style_##name(lv_obj_t *o,unsigned value,unsigned s) { \
    marker_write(o);(void)value;(void)s;palette_sets++; }
COUNT_STYLE(bg_grad_dir)
COUNT_STYLE(border_opa)
COUNT_STYLE(border_side)
COUNT_STYLE(border_post)
COUNT_STYLE(shadow_opa)
COUNT_STYLE(img_recolor)
static void emit(lv_obj_t *target,int code,int x,int y,unsigned advance) {
    now_ms+=advance;pointer=(lv_point_t){x,y};
    for(lv_obj_t *o=target;o;) {
        for(unsigned i=0;i<o->event_count;i++) if(o->events[i].code==LV_EVENT_ALL || o->events[i].code==code) {
            lv_event_t e={code,target,o,o->events[i].data};o->events[i].cb(&e);
        }
        o=(o->flags&LV_OBJ_FLAG_EVENT_BUBBLE)?o->parent:NULL;
    }
}
'''

view_stubs = r'''
#define PAGE07_CURR_MAX_ITEMS 34
#define PAGE07_CURR_VIEW_CARD 0
#define PAGE07_CURR_VIEW_GRID 1
typedef struct { unsigned identity; } lv_img_dsc_t;
typedef struct lv_dma_snapshot { lv_img_dsc_t image; char key[48]; unsigned refs;
    bool available,focus_mark_visible; } lv_dma_snapshot_t;
/* ACTUAL_CARD_LAYOUT */
static struct {
    page07_curr_carousel_t carousel;
    struct { int visible_count,visible_indices[34],selected_abs_idx,selected_visible_idx,view_mode; bool favorite_only; } model;
    struct { lv_obj_t *list,*card_layer,*thumb; } objects;
    page07_curr_card_t cards[34];
} g_page07_curr;
static lv_obj_t *curr_page;
static int g_curr_track_x=-1,g_curr_track_w=-1,g_curr_card_styled_visible_idx=-1,g_curr_cache_focus_idx=-1;
static unsigned commits,favorite_toggles,refreshes;
static int commit_index;
static bool is_favorite;
static void curr_select_and_exit_abs(int i) { commits++;commit_index=i; }
static void page07_curr_model_toggle_favorite(int i) { (void)i;favorite_toggles++;is_favorite=!is_favorite; }
static bool page07_curr_model_is_favorite(int i) { (void)i;return is_favorite; }
static bool page07_curr_model_is_fixed(int i) { (void)i;return false; }
static void curr_refresh_right_views(void) { refreshes++;page07_curr_carousel_destroy(&g_page07_curr.carousel); }
static lv_dma_snapshot_t cache[68];
static unsigned cache_count,cache_acquires,cache_captures,cache_releases;
static bool seed_cache=true;
static lv_obj_t *release_observer;
static lv_dma_snapshot_t *release_observed_image;
static unsigned detach_verified;
static bool currency_state_get_code(uint8_t index,char code[4]) {
    if(index>=64)return false;
    snprintf(code,4,"%03u",index);return true;
}
static int currency_state_count(void) { return g_page07_curr.model.visible_count; }
static const lv_img_dsc_t *lv_dma_snapshot_image(lv_dma_snapshot_t *snapshot) {
    assert(snapshot);return &snapshot->image;
}
static lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire(const char *key) {
    cache_acquires++;
    for(unsigned i=0;i<cache_count;i++)if(cache[i].available && !strcmp(cache[i].key,key)) {
        cache[i].refs++;return &cache[i];
    }
    return NULL;
}
static lv_dma_snapshot_t *lv_dma_snapshot_cache_acquire_or_create(lv_obj_t *root,const char *key) {
    assert(root);cache_captures++;
    assert(!g_page07_curr.carousel.active && g_page07_curr.carousel.motion.phase==UI_SCROLL_IDLE);
    bool focused=key[strlen(key)-1]=='F',found_root=false;
    for(int i=0;i<g_page07_curr.model.visible_count;i++)if(g_page07_curr.cards[i].render_root==root) {
        lv_obj_t *mark=g_page07_curr.cards[i].focus_mark;
        assert(mark && mark->parent==root);
        assert(lv_obj_has_flag(mark,LV_OBJ_FLAG_HIDDEN)!=focused);found_root=true;
    }
    assert(found_root); /* The captured face includes the real marker state. */
    for(unsigned i=0;i<cache_count;i++)if(!strcmp(cache[i].key,key)) {
        cache[i].available=true;cache[i].refs++;cache[i].focus_mark_visible=focused;return &cache[i];
    }
    assert(cache_count<68);
    lv_dma_snapshot_t *s=&cache[cache_count++];
    *s=(lv_dma_snapshot_t){.refs=1,.available=true,.focus_mark_visible=focused};
    snprintf(s->key,sizeof(s->key),"%s",key);return s;
}
static void lv_dma_snapshot_cache_release(lv_dma_snapshot_t *snapshot) {
    if(!snapshot)return;
    assert(snapshot->refs>0);snapshot->refs--;cache_releases++;
    if(snapshot==release_observed_image) {
        assert(release_observer && release_observer->source!=&snapshot->image);
        assert(lv_obj_has_flag(release_observer,LV_OBJ_FLAG_HIDDEN));detach_verified++;
    }
}
'''
view_stubs = view_stubs.replace("/* ACTUAL_CARD_LAYOUT */", card_layout)

tests = r'''
static lv_obj_t viewport,strip,card,favorite,thumb,renderers[34],images[34],hits[34],fav_buttons[34],fav_icons[34];
static lv_obj_t flag_images[34],names[34],numbers[34];
static lv_obj_t *focus_marks[34];
static void project_counted(void) { project_count++;curr_project_carousel(); }
static void fixture(unsigned count,unsigned index) {
    page07_curr_carousel_destroy(&g_page07_curr.carousel);
    memset(&g_page07_curr,0,sizeof(g_page07_curr));
    memset(&viewport,0,sizeof(viewport));memset(&strip,0,sizeof(strip));
    memset(&card,0,sizeof(card));memset(&favorite,0,sizeof(favorite));
    memset(&thumb,0,sizeof(thumb));memset(hits,0,sizeof(hits));
    memset(renderers,0,sizeof(renderers));memset(images,0,sizeof(images));
    memset(fav_buttons,0,sizeof(fav_buttons));memset(fav_icons,0,sizeof(fav_icons));
    memset(flag_images,0,sizeof(flag_images));memset(names,0,sizeof(names));memset(numbers,0,sizeof(numbers));
    memset(focus_marks,0,sizeof(focus_marks));object_creates=0;
    memset(cache,0,sizeof(cache));cache_count=0;release_observer=NULL;release_observed_image=NULL;
    g_curr_cache_focus_idx=g_curr_card_styled_visible_idx=g_curr_track_x=g_curr_track_w=-1;
    viewport.w=992;viewport.h=400;curr_page=&viewport;
    strip.parent=&viewport;strip.y=CURR_CARD_STRIP_Y;strip.h=CURR_CARD_SCROLL_H;
    card.parent=&strip;favorite.parent=&card;
    g_page07_curr.objects.list=&strip;g_page07_curr.objects.card_layer=&viewport;g_page07_curr.objects.thumb=&thumb;
    g_page07_curr.model.visible_count=(int)count;g_page07_curr.model.selected_abs_idx=3;g_page07_curr.model.selected_visible_idx=3;
    is_favorite=false;
    for(unsigned i=0;i<count;i++) {
        g_page07_curr.model.visible_indices[i]=(int)i+10;
        focus_marks[i]=lv_obj_create(&renderers[i]);focus_marks[i]->focus_marker=true;
        lv_obj_remove_style_all(focus_marks[i]);
        g_page07_curr.cards[i]=(page07_curr_card_t){
            .render_root=&renderers[i],.composite=&images[i],.card=&hits[i],
            .img=&flag_images[i],.name=&names[i],.no=&numbers[i],.focus_mark=focus_marks[i],
            .base_x=244+(int)i*228,.base_y=CURR_CARD_LOCAL_Y,.drawn_y=CURR_CARD_LOCAL_Y,
            .abs_idx=(int)i+10,.fav_btn=&fav_buttons[i],.fav_icon=&fav_icons[i]};
        fav_buttons[i].favorite_ui=fav_icons[i].favorite_ui=true;
        curr_update_card_fav_content((int)i); /* same eager binding as real builder */
        curr_set_card_render_state((int)i,244+(int)i*228,CURR_CARD_LOCAL_Y,false);
        if(seed_cache)for(unsigned state=0;state<2;state++) {
            lv_dma_snapshot_t *s=&cache[cache_count++];
            assert(curr_card_snapshot_key((int)i,state!=0,s->key));
            s->available=true;s->focus_mark_visible=state!=0;
        }
    }
    page07_curr_carousel_init(&g_page07_curr.carousel,&viewport,count,228,index,project_counted);
    page07_curr_carousel_bind_child(&strip);page07_curr_carousel_bind_child(&card);page07_curr_carousel_bind_child(&favorite);
    lv_obj_add_event_cb(&card,curr_card_click_cb,LV_EVENT_CLICKED,(void *)(intptr_t)index);
    lv_obj_add_event_cb(&favorite,curr_fav_icon_click_cb,LV_EVENT_CLICKED,(void *)(intptr_t)index);
    if(index<count) g_page07_curr.cards[index].card=&card;
    project_counted();
    project_count=x_sets=y_sets=border_sets=commits=favorite_toggles=refreshes=source_sets=palette_sets=0;
    favorite_source_sets=favorite_palette_sets=pos_sets=size_sets=cache_captures=cache_acquires=cache_releases=0;
    marker_writes=0;assert(object_creates==count);
    clock_reads=0;
    now_ms=1000;has_indev=true;
}
static void tick(unsigned ms) {
    now_ms+=ms;presents++;
    lv_timer_t *t=g_page07_curr.carousel.timer;
    if(t && !t->paused && !t->dead) t->cb(t);
}
static void settle(void) {
    for(unsigned n=0;page07_curr_carousel_busy(&g_page07_curr.carousel) && n<300;n++) tick(16);
    assert(!page07_curr_carousel_busy(&g_page07_curr.carousel));
    assert(!g_page07_curr.carousel.timer || g_page07_curr.carousel.timer->paused);
}
static void drag(lv_obj_t *target) {
    emit(target,LV_EVENT_PRESSED,700,120,0);
    emit(target,LV_EVENT_PRESSING,680,120,16);
    emit(target,LV_EVENT_PRESSING,620,120,16);
    emit(target,LV_EVENT_PRESSING,540,120,16);
    emit(target,LV_EVENT_RELEASED,540,120,8);
}
static void assert_focus_mark(unsigned i,bool focused) {
    lv_obj_t *mark=focus_marks[i];
    assert(mark && g_page07_curr.cards[i].focus_mark==mark && mark->parent==&renderers[i]);
    assert(lv_obj_has_flag(mark,LV_OBJ_FLAG_HIDDEN)!=focused);
    assert(mark->w==28 && mark->h==3 && mark->radius==LV_RADIUS_CIRCLE);
    assert(mark->background==0x4D5965 && mark->bg_opa==LV_OPA_COVER);
    assert(mark->border==0 && mark->pad==0 && mark->shadow==0 && mark->outline==0);
    assert(mark->align==LV_ALIGN_BOTTOM_MID && mark->align_x==0 && mark->align_y==-11);
    assert(!lv_obj_has_flag(mark,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
}
int main(void) {
    fixture(10,2);
    emit(&card,LV_EVENT_PRESSED,700,120,0);
    assert(g_page07_curr.carousel.active && !page07_curr_carousel_click_allowed(&g_page07_curr.carousel));
    emit(&card,LV_EVENT_RELEASED,700,120,10);
    emit(&card,LV_EVENT_CLICKED,700,120,0);
    assert(commits==1 && commit_index==12);
    assert(g_page07_curr.model.selected_abs_idx==3); /* independent focus */

    fixture(10,2);drag(&card);
    assert(g_page07_curr.carousel.motion.velocity>0 && !g_page07_curr.carousel.active);
    emit(&card,LV_EVENT_CLICKED,540,120,0);assert(commits==0);
    unsigned old_projects=project_count;tick(32);assert(project_count==old_projects+1);
    float held=g_page07_curr.carousel.motion.position;
    emit(&card,LV_EVENT_PRESSED,500,120,1);
    assert(fabsf(g_page07_curr.carousel.motion.position-held)<0.01f);
    assert(g_page07_curr.carousel.timer->paused);
    emit(&card,LV_EVENT_RELEASED,500,120,10);
    emit(&card,LV_EVENT_CLICKED,500,120,0);assert(commits==0);
    settle();
    emit(&card,LV_EVENT_PRESSED,500,120,10);emit(&card,LV_EVENT_RELEASED,500,120,10);
    emit(&card,LV_EVENT_CLICKED,500,120,0);assert(commits==1);

    fixture(10,2);drag(&favorite);
    emit(&favorite,LV_EVENT_CLICKED,540,120,0);assert(!commits && !favorite_toggles);
    settle();
    assert(!favorite_source_sets && !favorite_palette_sets); /* focus faces can change, favorite sources cannot */
    emit(&favorite,LV_EVENT_PRESSED,500,120,10);emit(&favorite,LV_EVENT_RELEASED,500,120,10);
    emit(&favorite,LV_EVENT_CLICKED,500,120,0);assert(favorite_toggles==1 && commits==0);
    assert(favorite_source_sets==1 && favorite_palette_sets==2);
    curr_apply_selected_style();assert(favorite_source_sets==1 && favorite_palette_sets==2);

    fixture(10,2);drag(&strip);assert(g_page07_curr.carousel.motion.position>456);settle();
    fixture(10,2);drag(&viewport);assert(g_page07_curr.carousel.motion.position>456);settle();
    fixture(10,0);
    emit(&card,LV_EVENT_PRESSED,500,120,0);emit(&card,LV_EVENT_PRESSING,800,120,16);
    assert(g_page07_curr.carousel.motion.position<0 && strip.x>0);
    emit(&card,LV_EVENT_RELEASED,800,120,16);settle();assert(strip.x==0);

    fixture(10,2);
    emit(&favorite,LV_EVENT_PRESSED,500,120,0);emit(&favorite,LV_EVENT_PRESSING,430,120,16);
    has_indev=false;emit(&favorite,LV_EVENT_PRESS_LOST,430,120,16);
    assert(!g_page07_curr.carousel.active && !g_page07_curr.carousel.dragging);
    assert(g_page07_curr.carousel.motion.velocity==0 && !page07_curr_carousel_click_allowed(&g_page07_curr.carousel));
    settle();has_indev=true;

    fixture(10,2);
    emit(&card,LV_EVENT_PRESSED,500,120,0);emit(&card,LV_EVENT_PRESSING,497,160,16);
    emit(&card,LV_EVENT_RELEASED,497,160,16);emit(&card,LV_EVENT_CLICKED,497,160,0);
    assert(!commits && !g_page07_curr.carousel.active);settle();
    fixture(10,2);
    emit(&card,LV_EVENT_PRESSED,500,120,0);emit(&card,LV_EVENT_PRESSED,500,120,1);
    emit(&card,LV_EVENT_RELEASED,500,120,16);emit(&card,LV_EVENT_CLICKED,500,120,0);
    assert(!commits);

    fixture(10,2);drag(&card);
    page07_curr_carousel_enable(&g_page07_curr.carousel,false);
    assert(!page07_curr_carousel_busy(&g_page07_curr.carousel));
    old_projects=project_count;tick(100);assert(project_count==old_projects);
    emit(&card,LV_EVENT_PRESSED,500,120,0);assert(!g_page07_curr.carousel.active);
    page07_curr_carousel_enable(&g_page07_curr.carousel,true);
    emit(&card,LV_EVENT_PRESSED,500,120,0);assert(g_page07_curr.carousel.active);
    page07_curr_carousel_destroy(&g_page07_curr.carousel);assert(timer_live==0);
    old_projects=project_count;tick(100);assert(project_count==old_projects);

    fixture(10,2);
    emit(&card,LV_EVENT_DELETE,0,0,0);assert(timer_live==1); /* bubbled child delete is not owner deletion */
    emit(&viewport,LV_EVENT_DELETE,0,0,0);assert(timer_live==0 && !g_page07_curr.carousel.viewport);
    page07_curr_carousel_destroy(&g_page07_curr.carousel);assert(timer_live==0);

    fail_timer=true;fixture(10,2);drag(&card);
    assert(!page07_curr_carousel_busy(&g_page07_curr.carousel) && !g_page07_curr.carousel.timer);
    fail_timer=false;
    fixture(0,0);drag(&viewport);settle();assert(strip.x==0);
    fixture(1,0);drag(&card);settle();assert(strip.x==0);
    fixture(34,33);assert(strip.x==-7524); /* bounds retain the full supported catalog */

    fixture(10,2);
    assert(strip.x==-456);
    assert(strip.y==60 && strip.h==274);
    assert(g_page07_curr.cards[2].drawn_y==0 && card.border==0 && renderers[2].border==1);
    unsigned old_objects=object_creates;
    for(unsigned repeat=0;repeat<16;repeat++)curr_project_carousel();
    assert(!x_sets && !y_sets && !border_sets && !pos_sets && !source_sets && !palette_sets);
    assert(!marker_writes && object_creates==old_objects);
    unsigned hidden_root_writes=renderers[2].y_writes+renderers[2].pos_writes;
    unsigned visible_y_writes=images[2].y_writes;
    g_page07_curr.carousel.motion.position+=64; /* enough to change integer lift without changing focus */
    curr_project_carousel();assert(x_sets==1 && y_sets<=6 && !border_sets);
    assert(!marker_writes && object_creates==old_objects);
    assert(renderers[2].y_writes+renderers[2].pos_writes==hidden_root_writes);
    assert(images[2].y_writes>visible_y_writes && images[2].y>0);
    assert(g_page07_curr.model.selected_abs_idx==3 && g_page07_curr.model.selected_visible_idx==3);
    assert(thumb.x>0); /* thumb follows real fractional offset, not destination */

    /* Execute the production render state and shared card surface, not a
     * pre-painted mock: normal and focus must share the same inside border. */
    fixture(10,2);
    assert(CURR_CARD_NORMAL_BORDER==0xDEDFE1);
    assert(CURR_CARD_FOCUS_BORDER==0xBFC7CF);
    assert_focus_mark(2,false);
    focus_marks[2]->border=focus_marks[2]->pad=focus_marks[2]->shadow=focus_marks[2]->outline=9;
    lv_obj_add_flag(focus_marks[2],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    curr_set_card_render_state(2,700,0,true);
    assert(renderers[2].w==200 && renderers[2].h==265 && renderers[2].radius==14);
    assert(renderers[2].border==1 && renderers[2].border_color==0xBFC7CF);
    assert_focus_mark(2,true);
    assert(renderers[2].background==0xFFFFFF && renderers[2].bg_opa==LV_OPA_COVER);
    assert(names[2].text_color==0x16181B && numbers[2].text_color==0x16181B);
    assert(flag_images[2].img_opa==LV_OPA_COVER && flag_images[2].recolor_opa==0);
    unsigned old_palette=palette_sets,old_sources=source_sets,old_borders=border_sets;
    unsigned old_marker_writes=marker_writes;old_objects=object_creates;
    curr_set_card_render_state(2,700,0,true);
    assert(palette_sets==old_palette && source_sets==old_sources && border_sets==old_borders);
    assert(marker_writes==old_marker_writes && object_creates==old_objects);
    curr_set_card_render_state(2,700,8,false);
    assert(renderers[2].background==0xF7F8FA && renderers[2].border_color==0xDEDFE1);
    assert(renderers[2].border==1 && renderers[2].radius==14 && flag_images[2].img_opa==LV_OPA_40);
    assert_focus_mark(2,false);
    assert(card.border==0); /* hit testing is not a second rectangle decoration */

    /* Actual lookup/window functions hold at most 7N+3F, while changing
     * focus only reads already-produced surfaces, never allocates them. */
    fixture(34,15);
    for(unsigned round=0;round<2;round++) {
        int focus=round?20:15;
        g_page07_curr.carousel.motion.position=focus*228.0f;curr_project_carousel();
        unsigned pins=0;
        for(unsigned i=0;i<cache_count;i++)pins+=cache[i].refs;
        assert(pins==10 && cache_captures==0);
        for(int i=0;i<34;i++)for(unsigned state=0;state<2;state++)
            assert((g_page07_curr.cards[i].surface_cache[state]!=NULL)==curr_card_cache_wanted(i,state!=0,focus));
        assert(images[focus].source==lv_dma_snapshot_image(g_page07_curr.cards[focus].surface_cache[1]));
        assert(g_page07_curr.cards[focus].surface_cache[1]->focus_mark_visible);
        assert(!g_page07_curr.cards[focus+1].surface_cache[0]->focus_mark_visible);
    }
    char normal_key[48],focus_key[48];
    assert(curr_card_snapshot_key(20,false,normal_key) && curr_card_snapshot_key(20,true,focus_key));
    assert(strcmp(normal_key,focus_key)!=0 && strstr(focus_key,"CURR_CAROUSEL_V4_")!=NULL);
    assert(!curr_card_snapshot_key(-1,false,normal_key) && !curr_card_snapshot_key(34,false,normal_key));

    /* Production currency cards carry a width-scaled external flag.  Even
     * with a warm snapshot available they must retain their live render tree
     * when motion settles, otherwise the captured flag is re-sampled. */
    fixture(10,2);
    g_page07_curr.cards[2].has_scaled_flag=true;
    page07_curr_card_render_apply(2,g_page07_curr.cards[2].base_x,0);
    assert(!g_page07_curr.cards[2].using_cache);
    assert(!lv_obj_has_flag(&renderers[2],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(&images[2],LV_OBJ_FLAG_HIDDEN));

    /* Scaled flags must never produce or pin faces that apply() cannot use. */
    for(int i=0;i<10;i++)g_page07_curr.cards[i].has_scaled_flag=true;
    unsigned unused_captures=cache_captures;
    for(int i=0;i<10;i++)page07_curr_card_render_sync_snapshots(i,2);
    assert(!page07_curr_card_render_prewarm_step());
    assert(cache_captures==unused_captures);
    for(int i=0;i<10;i++)for(unsigned state=0;state<2;state++)
        assert(g_page07_curr.cards[i].surface_cache[state]==NULL);

    /* Make the currently bound focus face evictable. The fake cache's
     * release callback checks detach/hide ordering before dropping the pin. */
    fixture(10,2);detach_verified=0;
    release_observer=&images[2];release_observed_image=g_page07_curr.cards[2].surface_cache[1];
    assert(release_observed_image && images[2].source==lv_dma_snapshot_image(release_observed_image));
    assert(page07_curr_card_render_sync_snapshots(2,7));
    assert(detach_verified==1 && images[2].source==NULL && !g_page07_curr.cards[2].using_cache);
    assert(!g_page07_curr.cards[2].surface_cache[0] && !g_page07_curr.cards[2].surface_cache[1]);
    page07_curr_card_render_apply(2,g_page07_curr.cards[2].base_x,0);
    assert(!g_page07_curr.cards[2].using_cache && !lv_obj_has_flag(&renderers[2],LV_OBJ_FLAG_HIDDEN));
    assert(renderers[2].background==0xFFFFFF && lv_obj_has_flag(&images[2],LV_OBJ_FLAG_HIDDEN));
    assert_focus_mark(2,true);

    /* A FOCUS-only hit cannot substitute for missing NORMAL pixels. A live
     * fallback hides the captured marker when this card is not focused. */
    seed_cache=false;fixture(10,2);
    assert(curr_acquire_card_snapshot(3,true,true));
    assert(!g_page07_curr.cards[3].surface_cache[0] && g_page07_curr.cards[3].surface_cache[1]);
    assert(g_page07_curr.cards[3].surface_cache[1]->focus_mark_visible);
    assert_focus_mark(3,true);
    page07_curr_card_render_apply(3,g_page07_curr.cards[3].base_x,CURR_CARD_LOCAL_Y);
    assert(!g_page07_curr.cards[3].using_cache && renderers[3].background==0xF7F8FA);
    assert(!lv_obj_has_flag(&renderers[3],LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(&images[3],LV_OBJ_FLAG_HIDDEN));
    assert_focus_mark(3,false);assert_focus_mark(2,true);
    old_marker_writes=marker_writes;old_objects=object_creates;
    for(unsigned repeat=0;repeat<16;repeat++)curr_project_carousel();
    g_page07_curr.carousel.motion.position+=64;curr_project_carousel();
    assert(marker_writes==old_marker_writes && object_creates==old_objects);
    assert_focus_mark(3,false);assert_focus_mark(2,true);

    /* A NORMAL-only cache hit cannot substitute for missing FOCUS pixels.
     * Capturing a normal face must also restore the current live appearance. */
    seed_cache=false;fixture(10,2);
    assert(curr_acquire_card_snapshot(2,false,true));
    assert(g_page07_curr.cards[2].surface_cache[0] && !g_page07_curr.cards[2].surface_cache[1]);
    assert(!g_page07_curr.cards[2].surface_cache[0]->focus_mark_visible);
    assert_focus_mark(2,false);
    page07_curr_card_render_apply(2,g_page07_curr.cards[2].base_x,0);
    assert(!g_page07_curr.cards[2].using_cache && renderers[2].background==0xFFFFFF);
    assert(flag_images[2].img_opa==255 && names[2].text_color==0x16181B);
    assert(!lv_obj_has_flag(&renderers[2],LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(&images[2],LV_OBJ_FLAG_HIDDEN));
    assert_focus_mark(2,true);
    old_marker_writes=marker_writes;old_objects=object_creates;
    for(unsigned repeat=0;repeat<16;repeat++)curr_project_carousel();
    assert(marker_writes==old_marker_writes && object_creates==old_objects);
    unsigned old_capture=cache_captures;
    drag(&card);assert(cache_captures==old_capture);
    assert(!ui_page_07_curr_prepare_static_step() && cache_captures==old_capture);
    settle();assert(cache_captures==old_capture);
    assert(ui_page_07_curr_prepare_static_step() && cache_captures==old_capture+1);
    assert(g_page07_curr.cards[ui_scroll_physics_nearest(&g_page07_curr.carousel.motion)].using_cache);
    old_capture=cache_captures;
    lv_obj_add_flag(curr_page,LV_OBJ_FLAG_HIDDEN);
    assert(!ui_page_07_curr_prepare_static_step());lv_obj_clear_flag(curr_page,LV_OBJ_FLAG_HIDDEN);
    g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
    assert(!ui_page_07_curr_prepare_static_step());g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_CARD;
    g_page07_curr.carousel.active=true;assert(!ui_page_07_curr_prepare_static_step());g_page07_curr.carousel.active=false;
    assert(cache_captures==old_capture);seed_cache=true;

    profile_enabled=true;trace_count=0;fixture(10,2);drag(&card);settle();
    assert(trace_count==2 && strstr(last_trace,"event=SETTLED") && strstr(last_trace,"reason=drag"));
    assert(strstr(last_trace,"project_calls=") && strstr(last_trace,"project_us="));
    assert(strstr(last_trace,"flush_frames="));
    assert(g_page07_curr.carousel.profile_project_calls>=4);
    assert(g_page07_curr.carousel.profile_project_total_us==7ULL*g_page07_curr.carousel.profile_project_calls);
    assert(g_page07_curr.carousel.profile_project_max_us==7);
    old_projects=trace_count;tick(100);assert(trace_count==old_projects);
    fixture(10,2);drag(&card);page07_curr_carousel_stop(&g_page07_curr.carousel);
    assert(strstr(last_trace,"event=CANCEL") && strstr(last_trace,"reason=owner_stop"));
    fixture(10,2);emit(&card,LV_EVENT_PRESSED,700,120,0);emit(&card,LV_EVENT_PRESSING,620,120,16);
    page07_curr_carousel_destroy(&g_page07_curr.carousel);
    assert(strstr(last_trace,"event=CANCEL") && strstr(last_trace,"reason=owner_destroy"));
    old_projects=trace_count;page07_curr_carousel_destroy(&g_page07_curr.carousel);assert(trace_count==old_projects);
    profile_enabled=false;page07_curr_carousel_destroy(&g_page07_curr.carousel);
    fixture(10,2);drag(&card);settle();assert(clock_reads==0);
    page07_curr_carousel_destroy(&g_page07_curr.carousel);
    page07_curr_carousel_init(NULL,NULL,0,228,0,NULL);
    page07_curr_carousel_init(&g_page07_curr.carousel,NULL,0,228,0,NULL);
    assert(timer_live==0);
    puts("Currency carousel: PASS (ownership, cancel, taps, favorites, geometry, real render state, focus marker/no allocations, cache window/detach/miss, idle capture, lifetime, UART trace)");
    return 0;
}
'''

parts = [stubs, without_includes(physics_h), without_includes(physics),
         without_includes(adapter_h), without_includes(adapter), geometry,
         without_includes(surface_h), without_includes(surface), view_stubs]
for name in ("page07_curr_view_set_image_unselected_style",
             "page07_curr_view_set_image_selected_style"):
    parts.append(function(view, name))
# Compile the actual renderer body and its public header, including all cache
# ownership operations. Only LVGL/cache/transport edges above are simulated.
parts.extend([without_includes(renderer_h), without_includes(renderer)])
for name in ("ui_page_07_curr_prepare_static_step",
             "curr_update_card_fav_content", "curr_update_card_fav_ui",
             "curr_update_track_by_scroll", "curr_apply_selected_style",
             "curr_project_carousel", "curr_card_click_cb", "curr_fav_icon_click_cb"):
    parts.append(function(page, name))
parts.append(tests)
with tempfile.TemporaryDirectory(prefix="currency_carousel_") as directory:
    source = Path(directory) / "test.c"
    binary = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    source.write_text("\n".join(parts), encoding="utf-8")
    for optimization in ("-O0", "-O2"):
        subprocess.run([os.environ.get("CC", "cc"), "-std=c11", optimization,
                        "-Wall", "-Wextra", "-Werror", str(source),
                        "-o", str(binary), "-lm"], check=True)
        subprocess.run([str(binary)], check=True)
