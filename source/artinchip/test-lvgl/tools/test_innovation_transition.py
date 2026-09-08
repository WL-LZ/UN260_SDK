#!/usr/bin/env python3
"""Compile production Innovation transition functions against host UI stubs.

Checks navigation ownership, presentation-gated mirror motion and failure /
lifetime cleanup. This is not a substitute for board pixel and touch QA.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile
from lvgl_timer_fixture import code as lvgl_timer_code


ROOT = Path(__file__).resolve().parents[1]
PAGE = (ROOT / "un260/innovation/page_32_innovation.c").read_text(encoding="utf-8")


def function(name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name)
                          + r"\([^;]*?\)\s*\{", PAGE, re.M)
    assert signature, "Missing production function: " + name
    start = signature.start()
    opening = PAGE.index("{", start)
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(PAGE, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return PAGE[start:token.end()]
    raise AssertionError("Unterminated source block")


constants = "\n".join(re.findall(r"^#define INNOVATION_TRANSITION_.*$", PAGE, re.M))
assert "INNOVATION_TRANSITION_SETTLE_MS" in function("innovation_handle_drag_finish")
assert "INNOVATION_TRANSITION_SETTLE_MS" in function("innovation_back_first_frame")
assert "lv_anim_path_ease_in_out" not in function("innovation_back_first_frame")
assert "lv_event_get_code" not in function("innovation_back_first_frame")
assert "innovation_back_cb" in function("ui_page_32_innovation_create")
assert "page_32_innovation_request_back" in function("innovation_back_cb")

names = [
    "innovation_set_hidden", "innovation_set_y_if_changed",
    "innovation_refresh_pause", "innovation_refresh_resume",
    "innovation_transition_set_y", "innovation_transition_target",
    "innovation_transition_snapshot_release", "innovation_transition_prepare",
    "innovation_transition_commit_async", "innovation_transition_cancel_async",
    "innovation_transition_back_async", "innovation_transition_open_ready",
    "innovation_transition_cancel_ready", "innovation_transition_back_ready",
    "innovation_transition_animate", "innovation_back_stop",
    "innovation_transition_stop", "innovation_back_first_frame",
    "page_32_innovation_request_back", "innovation_back_cb",
    "ui_page_32_innovation_resume", "ui_page_32_innovation_suspend",
    "ui_page_32_innovation_destroy",
]
production = "\n\n".join(function(name) for name in names)
deferred_header = (ROOT / "un260/lv_core/ui_deferred_action.h").read_text(encoding="utf-8")
deferred_source = (ROOT / "un260/lv_core/ui_deferred_action.c").read_text(encoding="utf-8")
deferred = re.sub(r'^#include[^\n]*', '', deferred_header + '\n' + deferred_source, flags=re.M)

stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define LV_RES_OK 0
#define LV_OBJ_FLAG_HIDDEN 1
#define MULTI_PASS_VERIFY_MIN_PASSES 2
typedef int lv_coord_t;
typedef int ui_page_t;
enum {UI_PAGE_MAIN, UI_PAGE_INNOVATION_CENTER, UI_PAGE_OTHER};
typedef struct {bool valid,hidden; int y; unsigned opacity;} lv_obj_t;
typedef struct {int unused;} lv_event_t;
typedef struct lv_anim lv_anim_t;
typedef void (*lv_anim_ready_cb_t)(lv_anim_t *);
typedef int (*lv_anim_path_cb_t)(const lv_anim_t *);
struct lv_anim {
    lv_obj_t *var; int start,end; uint32_t duration;
    lv_anim_path_cb_t path; lv_anim_ready_cb_t ready;
    void (*exec)(void *,int32_t);
};
typedef struct {lv_obj_t *root; lv_timer_t *refresh_timer; unsigned target_passes;} page_t;
typedef struct {lv_obj_t *image,*source; void *snapshot;} surface_t;
static page_t g_page;
static surface_t g_transition_snapshot;
static struct {bool pressed,opened,preview_active;} g_handle_gesture;
static bool g_transition_snapshot_valid,g_transition_snapshot_dirty;
static bool g_transition_prepare_failed,g_page_transitioning;
static uint32_t g_transition_failure_tick,g_back_tick,g_back_present_sequence;
static lv_timer_t *g_back_first_frame_timer;
static int g_prompt;
static lv_obj_t live_root,image,screen;
static lv_anim_t running_animation;
static bool animation_running,fail_animation,fail_capture,profile,fail_adopt,destroy_on_pop;
static uint32_t present_sequence;
static ui_page_t current;
static unsigned captures,animations,pops,adopts,reveals,invalidates,releases;
static ui_deferred_action_t g_transition_action;
#define queued (g_transition_action.timer != NULL)
static void innovation_back_stop(void);
static void innovation_transition_stop(void);
static void innovation_transition_commit_async(void *);
static void innovation_transition_cancel_async(void *);
static void innovation_transition_back_async(void *);
static void ui_page_32_innovation_suspend(void);
static void ui_page_32_innovation_destroy(void);
static bool ui_page_32_innovation_resume(void);
static bool lv_obj_is_valid(lv_obj_t *o) {return o && o->valid;}
static bool lv_obj_has_flag(lv_obj_t *o,int f) {assert(f==1);return o->hidden;}
static void lv_obj_add_flag(lv_obj_t *o,int f) {assert(f==1);o->hidden=true;}
static void lv_obj_clear_flag(lv_obj_t *o,int f) {assert(f==1);o->hidden=false;}
static int lv_obj_get_y(lv_obj_t *o) {assert(lv_obj_is_valid(o));return o->y;}
static void lv_obj_set_y(lv_obj_t *o,int y) {assert(lv_obj_is_valid(o));o->y=y;}
static void lv_obj_move_foreground(lv_obj_t *o) {assert(lv_obj_is_valid(o));}
static void lv_obj_del(lv_obj_t *o) {assert(lv_obj_is_valid(o));o->valid=false;}
static void lv_obj_invalidate(lv_obj_t *o) {assert(lv_obj_is_valid(o));invalidates++;}
static lv_obj_t *lv_scr_act(void) {return &screen;}
static int lv_anim_path_ease_out(const lv_anim_t *a) {(void)a;return 0;}
static void lv_anim_del(lv_obj_t *o,void(*exec)(void *,int32_t)) {
    if(animation_running && running_animation.var==o && running_animation.exec==exec)
        animation_running=false;
}
static void lv_anim_init(lv_anim_t *a) {memset(a,0,sizeof(*a));}
static void lv_anim_set_var(lv_anim_t *a,lv_obj_t *o) {a->var=o;}
static void lv_anim_set_values(lv_anim_t *a,int start,int end) {a->start=start;a->end=end;}
static void lv_anim_set_time(lv_anim_t *a,uint32_t duration) {a->duration=duration;}
static void lv_anim_set_path_cb(lv_anim_t *a,lv_anim_path_cb_t path) {a->path=path;}
static void lv_anim_set_exec_cb(lv_anim_t *a,void(*exec)(void *,int32_t)) {a->exec=exec;}
static void lv_anim_set_ready_cb(lv_anim_t *a,lv_anim_ready_cb_t ready) {a->ready=ready;}
static lv_anim_t *lv_anim_start(lv_anim_t *a) {
    if(fail_animation) return NULL;
    assert(a->var==&image && image.valid && !animation_running);
    running_animation=*a;animation_running=true;animations++;
    a->exec(a->var,a->start);return &running_animation;
}
static uint32_t fbdev_present_sequence(void) {return present_sequence;}
static uint64_t app_clock_monotonic_us(void) {return (uint64_t)tick*1000;}
static uint32_t app_clock_elapsed_us32(uint64_t from,uint64_t to) {return (uint32_t)(to-from);}
static bool perf_profile_is_enabled(void) {return profile;}
static void uart_debug_printf(const char *fmt,...) {(void)fmt;}
static void perf_profile_report_event_us(const char *page,const char *event,uint32_t us) {
    (void)page;(void)event;(void)us;
}
static ui_page_t ui_manager_get_current_page(void) {return current;}
static bool input_held;
static void ui_manager_hold_transition_input(ui_page_t owner, bool hold) {assert(owner==UI_PAGE_INNOVATION_CENTER); input_held=hold;}
static bool ui_manager_pop_page(void) {
    pops++;
    if(destroy_on_pop) ui_page_32_innovation_destroy();
    else ui_page_32_innovation_suspend();
    current=UI_PAGE_MAIN;return true;
}
static void ui_manager_switch(ui_page_t page) {current=page;}
static bool ui_manager_adopt_precreated_page(ui_page_t page) {
    adopts++;if(fail_adopt) return false;current=page;return true;
}
static void ui_manager_push_page(ui_page_t page) {
    current=page;assert(ui_page_32_innovation_resume());
}
static void page_01_main_reveal_for_transition(void) {reveals++;}
static void innovation_prompt_close(void) {}
static void lv_modal_dialog_destroy(int *prompt) {(void)prompt;}
static void gesture_guide_close(bool save) {assert(!save);}
static bool innovation_page_refresh(void) {return false;}
static bool lv_dma_transition_surface_capture(surface_t *s,lv_obj_t *obj,const char *name) {
    assert(obj==&live_root && obj->valid && !obj->hidden && obj->y==0 && name);
    captures++;
    if(fail_capture) return false;
    image.valid=true;s->image=&image;s->source=obj;s->snapshot=&image;return true;
}
static unsigned lv_dma_snapshot_size(void *snapshot) {return snapshot ? 2048000U:0U;}
static void lv_dma_static_surface_release(surface_t *s) {
    releases++;
    if(lv_obj_is_valid(s->source)) s->source->hidden=false;
    if(lv_obj_is_valid(s->image)) lv_obj_del(s->image);
    *s=(surface_t){0};
}
'''

test = r'''
static void fixture(void) {
    test_lvgl_reset();
    live_root=(lv_obj_t){.valid=true,.opacity=255};
    image=(lv_obj_t){.valid=true,.hidden=true,.y=-400,.opacity=255};
    screen=(lv_obj_t){.valid=true,.opacity=255};
    g_back_first_frame_timer=NULL;
    g_transition_action=(ui_deferred_action_t){0};
    g_page=(page_t){.root=&live_root,.refresh_timer=lv_timer_create(NULL,1000,NULL)};
    g_transition_snapshot=(surface_t){.image=&image,.source=&live_root,.snapshot=&image};
    memset(&g_handle_gesture,0,sizeof(g_handle_gesture));
    g_transition_snapshot_valid=true;g_transition_snapshot_dirty=true;
    g_transition_prepare_failed=g_page_transitioning=false;
    animation_running=fail_animation=fail_async=fail_timer=fail_capture=profile=false;
    fail_adopt=destroy_on_pop=false;
    current=UI_PAGE_INNOVATION_CENTER;tick=100;present_sequence=10;
    input_held=false;
    captures=animations=pops=adopts=reveals=invalidates=releases=0;
}
static void drain(void) {
    lv_timer_handler();
    assert(!queued && !double_frees);
}
static void finish_animation(void) {
    assert(animation_running);
    lv_anim_t done=running_animation;animation_running=false;
    done.exec(done.var,done.end);done.ready(&done);
}
static void first_present(void) {
    present_sequence++;tick+=16;
    innovation_back_first_frame(g_back_first_frame_timer);
}
static void assert_returned(void) {
    assert(current==UI_PAGE_MAIN && pops==1 && !g_page_transitioning);
    assert(live_root.hidden && image.hidden && image.y==-400);
    assert(!g_back_first_frame_timer && !animation_running && !queued);
    assert(!input_held);
}
static void gesture_back_async(void *unused) {
    (void)unused;assert(page_32_innovation_request_back());
}
int main(void) {
    fixture();innovation_back_cb(NULL);
    assert(captures==1 && reveals==1 && g_page_transitioning);
    assert(input_held);
    assert(live_root.hidden && live_root.y==-400 && !image.hidden && image.y==0);
    assert(image.opacity==255 && g_page.refresh_timer->paused && !animation_running);
    tick+=16;innovation_back_first_frame(g_back_first_frame_timer);
    assert(!animation_running && !pops); /* no successful present yet */
    first_present(); /* profiles and DRAW_POST_END intentionally absent */
    assert(animation_running && running_animation.start==0 && running_animation.end==-400);
    assert(running_animation.duration==INNOVATION_TRANSITION_SETTLE_MS);
    assert(running_animation.path==lv_anim_path_ease_out && !g_back_first_frame_timer);
    assert(page_32_innovation_request_back() && captures==1 && animations==1);
    running_animation.exec(running_animation.var,-200);
    assert(image.y==-200 && image.opacity==255 && live_root.hidden && live_root.y==-400);
    finish_animation();assert(queued==1 && !pops);drain();assert_returned();
    assert(!page_32_innovation_request_back()); /* idle Main is not owned */

    /* Side gesture enters from a different real LVGL async callback. The
     * deferred back handoff then suspends the page from inside dispatch. */
    fixture();assert(lv_async_call(gesture_back_async,NULL)==LV_RES_OK);
    lv_timer_handler();assert(g_page_transitioning && !animation_running);
    first_present();finish_animation();drain();assert_returned();

    /* The same dispatch is safe when the manager destroys, rather than
     * retains, the outgoing page; there must be no post-callback owner use. */
    fixture();destroy_on_pop=true;innovation_back_cb(NULL);
    first_present();finish_animation();drain();
    assert(current==UI_PAGE_MAIN && pops==1 && !g_page.root && !image.valid);

    fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;
    g_handle_gesture.preview_active=true;live_root.hidden=true;
    assert(innovation_transition_animate(0,INNOVATION_TRANSITION_SETTLE_MS,
                                         innovation_transition_open_ready));
    assert(running_animation.start==-400 && running_animation.end==0);
    assert(running_animation.duration==180 && running_animation.path==lv_anim_path_ease_out);
    finish_animation();drain();assert(current==UI_PAGE_INNOVATION_CENTER && adopts==1);
    assert(!live_root.hidden && image.hidden && !g_page_transitioning);

    /* Adoption fallback resumes the page and cancels transition ownership
     * from inside the OPEN callback, not only the BACK callback. */
    fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;
    g_handle_gesture.preview_active=true;fail_adopt=true;
    innovation_transition_open_ready(NULL);drain();
    assert(current==UI_PAGE_INNOVATION_CENTER && adopts==1 && !live_root.hidden);

    fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;
    g_handle_gesture.preview_active=true;innovation_transition_cancel_ready(NULL);
    drain();assert(!g_page_transitioning && image.hidden && live_root.hidden);

    /* Exit during the uncommitted entrance cancels motion and pending commit. */
    for(unsigned queued_commit=0;queued_commit<2;queued_commit++) {
        fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;
        g_handle_gesture.preview_active=true;g_handle_gesture.pressed=true;
        live_root.hidden=true;image.hidden=false;
        assert(innovation_transition_animate(0,180,innovation_transition_open_ready));
        if(queued_commit) finish_animation();
        assert(page_32_innovation_request_back());drain();
        assert(current==UI_PAGE_MAIN && !adopts && !pops && !queued && !animation_running);
        assert(!g_page_transitioning && !g_handle_gesture.preview_active && !g_handle_gesture.pressed);
        assert(live_root.hidden && image.hidden && image.y==-400);
    }
    fixture();current=UI_PAGE_MAIN;g_handle_gesture.pressed=true;
    assert(page_32_innovation_request_back() && !g_handle_gesture.pressed && !pops);
    fixture();current=UI_PAGE_OTHER;g_handle_gesture.preview_active=true;
    assert(!page_32_innovation_request_back() && !captures && !pops);

    fixture();fail_capture=true;assert(page_32_innovation_request_back());
    assert(!animation_running && queued==1);drain();assert_returned();
    fixture();fail_capture=fail_async=true;assert(page_32_innovation_request_back());assert_returned();
    fixture();fail_timer=true;assert(page_32_innovation_request_back());assert_returned();
    fixture();assert(page_32_innovation_request_back());fail_animation=true;
    first_present();assert_returned();
    fixture();assert(page_32_innovation_request_back());first_present();fail_async=true;
    finish_animation();assert_returned();
    fixture();g_page.root=NULL;assert(page_32_innovation_request_back());
    assert(current==UI_PAGE_MAIN && pops==1 && !g_page_transitioning);

    fixture();assert(page_32_innovation_request_back());tick=g_back_tick+699;
    innovation_back_first_frame(g_back_first_frame_timer);assert(!pops);
    tick++;innovation_back_first_frame(g_back_first_frame_timer);assert_returned();
    fixture();present_sequence=UINT32_MAX;assert(page_32_innovation_request_back());
    first_present();assert(animation_running);finish_animation();drain();assert_returned();

    /* Lifecycle cleanup at each phase prevents stale ready/commit callbacks. */
    for(unsigned phase=0;phase<3;phase++) {
        fixture();assert(page_32_innovation_request_back());
        if(phase>0) first_present();
        if(phase>1) finish_animation();
        ui_page_32_innovation_suspend();current=UI_PAGE_OTHER;drain();
        assert(!animation_running && !g_back_first_frame_timer && !queued && !pops);
        assert(image.hidden && image.y==-400 && live_root.hidden);
        ui_page_32_innovation_destroy();drain();
        assert(!g_page.root && !g_page.refresh_timer && !image.valid && !live_root.valid);
    }
    fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;g_handle_gesture.preview_active=true;
    innovation_transition_open_ready(NULL);assert(queued==1);
    ui_page_32_innovation_destroy();drain();assert(!adopts && !queued && !g_page_transitioning);
    fixture();g_page.root=NULL;g_handle_gesture.pressed=true;
    ui_page_32_innovation_destroy();assert(g_handle_gesture.pressed); /* lazy create keeps held press */
    fixture();assert(page_32_innovation_request_back());first_present();
    assert(ui_page_32_innovation_resume());assert(!animation_running && image.hidden && !live_root.hidden);

    fixture();current=UI_PAGE_MAIN;g_page_transitioning=true;g_handle_gesture.preview_active=true;
    assert(innovation_transition_animate(-400,INNOVATION_TRANSITION_CANCEL_MS,
                                         innovation_transition_cancel_ready));
    assert(running_animation.duration==150);fail_async=true;finish_animation();
    assert(!g_page_transitioning && !g_handle_gesture.preview_active && image.hidden);
    test_lvgl_reset();
    puts("Innovation transition: PASS mirrored settle, successful-present gate, ownership, allocation failures, real LVGL 8.3.2 timer/async/list lifetime");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="un260-innovation-transition-") as directory:
    work = Path(directory)
    source = work / "test.c"
    binary = work / ("test.exe" if os.name == "nt" else "test")
    source.write_text(lvgl_timer_code() + deferred + stub + constants + "\n" + production + "\n" + test, encoding="utf-8")
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("UN260_TEST_SANITIZE", "0" if os.name == "nt" else "1") == "1":
        flags.append("-fsanitize=address,undefined")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
