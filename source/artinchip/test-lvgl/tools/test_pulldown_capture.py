#!/usr/bin/env python3
"""Exercise production pull-down ownership/cancellation against host UI stubs.

This is a lifecycle test, not a substitute for LVGL hit-testing or device/render
QA. The input policy, raw capture handoff, and local event/settle callbacks are
extracted verbatim; rendering and the final page-manager commit are stubbed.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
page = (ROOT / "un260/innovation/page_32_innovation.c").read_text(encoding="utf-8")


def block(source, start):
    """Return a brace-delimited block without counting braces in comments/text."""
    opening = source.index("{", start)
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[start:token.end()]
    raise AssertionError("Unterminated source block")


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name) + r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, "Missing production function: " + name
    return block(source, signature.start())


attach = function(page, "page_32_innovation_handle_attach")
assert "lv_port_indev_set_drag_obj(g_handle_touch, true)" in attach
assert "LV_EVENT_PRESS_LOST" in attach and "LV_EVENT_RELEASED" in attach
assert "return NULL;" in function(page, "innovation_transition_target")
capture = block(port, port.index("if(capture && !g_contact_captured)"))
production = "\n".join([
    function(port, "lv_port_indev_set_drag_obj"),
    function(port, "evdev_feedback"),
    function(page, "innovation_transition_cancel_async"),
    function(page, "innovation_transition_open_ready"),
    function(page, "innovation_transition_cancel_ready"),
    function(page, "innovation_handle_drag_update"),
    function(page, "innovation_handle_drag_finish"),
    function(page, "innovation_handle_drag_cancel"),
    function(page, "innovation_handle_event_cb"),
])

stub = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define LV_UNUSED(x) ((void)(x))
#define LV_OBJ_FLAG_USER_4 1U
#define LV_OBJ_FLAG_PRESS_LOCK 2U
#define LV_OBJ_FLAG_CLICKABLE 4U
#define LV_STATE_PRESSED 1U
#define LV_INDEV_TYPE_POINTER 1
#define INNOVATION_PREVIEW_ARM_DY 1
#define INNOVATION_TRANSITION_SETTLE_MS 180U
#define INNOVATION_TRANSITION_CANCEL_MS 150U
#define LV_RES_OK 0
typedef int lv_coord_t;
typedef int lv_event_code_t;
enum {LV_EVENT_PRESSED=1, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST};
typedef struct {int x, y;} lv_point_t;
typedef struct {unsigned flags, state; int y; bool valid, hidden;} lv_obj_t;
typedef struct {lv_point_t point; int type;} lv_indev_t;
typedef struct {int unused;} lv_indev_drv_t;
typedef struct {lv_event_code_t code;} lv_event_t;
typedef struct {int unused;} lv_anim_t;
typedef void (*lv_anim_ready_cb_t)(lv_anim_t *);
typedef struct {
    bool pressed, opened, preview_active;
    lv_point_t start;
    int drag_y;
    uint32_t start_tick, last_render_tick;
    int last_render_y;
} innovation_handle_gesture_t;
static innovation_handle_gesture_t g_handle_gesture;
static struct {lv_obj_t *root;} g_page;
static struct {lv_obj_t *image;} g_transition_snapshot;
static bool g_page_transitioning, g_transition_snapshot_valid;
static bool evdev_press_cancelled, g_contact_captured;
static lv_obj_t *evdev_pressed_obj, *active_object;
static lv_obj_t handle, surface, live_root;
static lv_indev_t pointer, *active_indev, *g_pointer_indev;
static uint32_t tick;
static unsigned commits, animations, resets, waits, preview_begins;
static int animation_destination;
static lv_obj_t *reset_object;
static void (*queued)(void *);
static void *queued_data;
static int g_transition_action;
static lv_anim_ready_cb_t animation_ready;
static bool preview_available;
static void innovation_handle_event_cb(lv_event_t *);
static void innovation_transition_cancel_async(void *);
static bool lv_obj_is_valid(lv_obj_t *o) {return o && o->valid;}
static void lv_obj_add_flag(lv_obj_t *o,unsigned flags) {o->flags |= flags;}
static void lv_obj_clear_flag(lv_obj_t *o,unsigned flags) {o->flags &= ~flags;}
static bool lv_obj_has_flag(lv_obj_t *o,unsigned flag) {return (o->flags & flag) != 0;}
static void lv_obj_clear_state(lv_obj_t *o,unsigned state) {o->state &= ~state;}
static lv_indev_t *lv_indev_get_act(void) {return active_indev;}
static int lv_indev_get_type(lv_indev_t *i) {return i->type;}
static lv_obj_t *lv_indev_get_obj_act(void) {return active_object;}
static void lv_indev_get_point(lv_indev_t *i,lv_point_t *p) {*p=i->point;}
static void lv_indev_reset(lv_indev_t *i,lv_obj_t *o) {assert(i==&pointer); resets++; reset_object=o;}
static void lv_indev_wait_release(lv_indev_t *i) {assert(i==&pointer); waits++;}
static lv_event_code_t lv_event_get_code(lv_event_t *e) {return e->code;}
static void lv_event_send(lv_obj_t *o,lv_event_code_t code,void *data) {
    assert(data==&pointer);
    if(o==&handle) {lv_event_t event={code}; innovation_handle_event_cb(&event);}
}
static uint32_t lv_tick_get(void) {return tick;}
static uint32_t lv_tick_elaps(uint32_t from) {return tick-from;}
static bool perf_profile_is_enabled(void) {return false;}
static void uart_debug_printf(const char *fmt,...) {LV_UNUSED(fmt);}
static void innovation_set_y_if_changed(lv_obj_t *o,lv_coord_t y) {if(lv_obj_is_valid(o)) o->y=y;}
static void innovation_set_hidden(lv_obj_t *o,bool hidden) {if(lv_obj_is_valid(o)) o->hidden=hidden;}
/* This test owns capture/input policy; actual deferred/timer lifetime is
 * exercised by test_innovation_transition.py and test_deferred_action.py. */
static bool ui_deferred_action_schedule(int *owner,void(*callback)(void *),void *data) {
    assert(owner==&g_transition_action && !queued);
    queued=callback; queued_data=data; return true;
}
static void innovation_transition_commit_async(void *data) {
    LV_UNUSED(data); commits++; g_page_transitioning=false;
    g_handle_gesture.preview_active=false; surface.hidden=true;
    live_root.y=0; live_root.hidden=false;
}
static bool innovation_transition_animate(lv_coord_t destination,uint32_t duration,lv_anim_ready_cb_t ready) {
    if (!g_transition_snapshot_valid) return false;
    assert(!animation_ready);
    assert(live_root.hidden && live_root.y==-400);
    assert((destination==0 && duration==180) || (destination==-400 && duration==150));
    animations++; animation_destination=destination; animation_ready=ready;
    return true;
}
static bool innovation_handle_preview_begin(void) {
    if(!preview_available || g_page_transitioning || g_handle_gesture.preview_active) return false;
    preview_begins++; g_handle_gesture.preview_active=true;
    surface.hidden=!g_transition_snapshot_valid;
    return true;
}
'''

test = r'''
static void fixture(bool have_surface) {
    memset(&g_handle_gesture,0,sizeof(g_handle_gesture));
    handle=(lv_obj_t){.valid=true,.flags=LV_OBJ_FLAG_CLICKABLE,.state=LV_STATE_PRESSED};
    surface=(lv_obj_t){.valid=true,.hidden=true,.y=-400};
    live_root=surface; g_page.root=&live_root; g_transition_snapshot.image=&surface;
    pointer=(lv_indev_t){.point={1160,10},.type=LV_INDEV_TYPE_POINTER};
    active_indev=g_pointer_indev=&pointer; active_object=&handle;
    g_page_transitioning=evdev_press_cancelled=g_contact_captured=false;
    evdev_pressed_obj=reset_object=NULL; g_transition_snapshot_valid=have_surface;
    tick=commits=animations=resets=waits=preview_begins=0;
    queued=NULL; queued_data=NULL; animation_ready=NULL; preview_available=true;
    lv_port_indev_set_drag_obj(&handle,true);
}
static void event(int code) {
    lv_event_t e={code}; innovation_handle_event_cb(&e);
    evdev_feedback(NULL,(uint8_t)code);
}
static void press(void) {
    event(LV_EVENT_PRESSED);
    assert(g_handle_gesture.pressed);
    assert(lv_obj_has_flag(&handle,LV_OBJ_FLAG_PRESS_LOCK));
    assert(evdev_pressed_obj==&handle);
}
static void move(int y,uint32_t time) {
    tick=time; pointer.point.y=y;
    /* Model the important LVGL boundary: only an unlocked target loses its
     * press when leaving the 44 px hit box. Production feedback owns this flag. */
    if(y>=44 && !lv_obj_has_flag(&handle,LV_OBJ_FLAG_PRESS_LOCK)) event(LV_EVENT_PRESS_LOST);
    else event(LV_EVENT_PRESSING);
}
static void drain(void) {
    if(animation_ready) {
        lv_anim_ready_cb_t ready=animation_ready; animation_ready=NULL;
        surface.y=animation_destination; ready(NULL);
    }
    if(queued) {
        void(*callback)(void *)=queued; void *data=queued_data; queued=NULL;
        callback(data);
    }
}
static void assert_held(int distance) {
    assert(g_handle_gesture.pressed && g_handle_gesture.preview_active);
    assert(g_handle_gesture.drag_y==distance && surface.y==-400+distance);
    assert(!g_page_transitioning && !animations && !commits && !queued);
    assert(!resets && !evdev_press_cancelled && live_root.hidden);
}
static void assert_cancelled(void) {
    assert(!g_handle_gesture.pressed && !g_handle_gesture.preview_active);
    assert(!g_page_transitioning && !commits && !queued && !animation_ready);
    assert(surface.hidden && surface.y==-400 && live_root.hidden && live_root.y==-400);
}
int main(void) {
    /* Slow multi-second drag crosses the original hit box and opening threshold
     * while remaining held. Only physical release can start settlement. */
    fixture(true); press();
    move(11,100); assert_held(1);
    move(43,700); assert_held(33);
    move(44,1100); assert_held(34);
    move(70,1800); assert_held(60);
    move(110,2600); assert_held(100);
    move(190,4200); assert_held(180);
    move(190,7000); assert_held(180);
    assert(preview_begins==1); event(LV_EVENT_RELEASED);
    assert(!g_handle_gesture.pressed && g_handle_gesture.opened);
    assert(animations==1 && animation_destination==0 && !commits);
    drain(); assert(commits==1 && !g_page_transitioning && !live_root.hidden);
    event(LV_EVENT_RELEASED); drain(); assert(commits==1);

    fixture(true); press(); move(60,200); assert_held(50);
    event(LV_EVENT_RELEASED); drain(); assert(commits==1);
    fixture(true); press(); move(60,2000); event(LV_EVENT_RELEASED);
    assert(animations==1 && animation_destination==-400); drain(); assert_cancelled();
    fixture(true); press(); move(180,1700); assert_held(170);
    move(30,2300); assert_held(20); event(LV_EVENT_RELEASED); drain(); assert_cancelled();
    fixture(true); press(); move(450,2000); assert_held(400);
    move(-20,2400); assert_held(0); event(LV_EVENT_RELEASED); drain(); assert_cancelled();

    /* Raw one-finger edge and multi-finger capture share the same handoff.
     * Exercise the verbatim driver block with no active LVGL indev, as can
     * happen before widget event dispatch in evdev_read. Neither may commit. */
    for(unsigned fingers=1; fingers<=3; fingers++) {
        fixture(true); press(); move(180,120); assert_held(170);
        active_indev=NULL; raw_capture(); assert_cancelled();
        assert(g_contact_captured && resets==1 && waits==1 && !reset_object);
        assert(!evdev_pressed_obj && !(handle.state & LV_STATE_PRESSED));
        raw_capture(); assert(resets==1 && waits==1);
        active_indev=&pointer; event(LV_EVENT_PRESSING); event(LV_EVENT_RELEASED);
        drain(); assert_cancelled();
    }
    fixture(true); press(); move(180,1500); event(LV_EVENT_PRESS_LOST);
    assert_cancelled(); assert(evdev_press_cancelled && resets==1 && reset_object==&handle);
    fixture(true); press(); active_indev=NULL; raw_capture(); assert_cancelled();

    /* Surface failure never animates or exposes the live tree during a drag;
     * the existing stationary async fallback still waits for release. */
    fixture(false); press(); move(180,2500);
    assert(g_handle_gesture.pressed && !animations && !commits && live_root.hidden);
    event(LV_EVENT_RELEASED); assert(queued && !animations && !commits && live_root.hidden);
    drain(); assert(commits==1 && !live_root.hidden);
    fixture(false); press(); move(30,2000); event(LV_EVENT_RELEASED);
    assert(queued && !animations); drain(); assert_cancelled();
    fixture(false); press(); move(180,2500); active_indev=NULL; raw_capture(); assert_cancelled();

    /* Ordinary buttons must still slide out/cancel, not acquire drag ownership. */
    fixture(true); lv_obj_t button={.valid=true,.flags=LV_OBJ_FLAG_PRESS_LOCK};
    active_object=&button; evdev_feedback(NULL,LV_EVENT_PRESSED);
    assert(!lv_obj_has_flag(&button,LV_OBJ_FLAG_PRESS_LOCK));
    evdev_feedback(NULL,LV_EVENT_PRESS_LOST);
    assert(evdev_press_cancelled && resets==1 && reset_object==&button && !evdev_pressed_obj);
    evdev_feedback(NULL,LV_EVENT_RELEASED); assert(!evdev_pressed_obj);
    lv_port_indev_set_drag_obj(&button,true);
    assert(lv_obj_has_flag(&button,LV_OBJ_FLAG_USER_4) && lv_obj_has_flag(&button,LV_OBJ_FLAG_PRESS_LOCK));
    lv_port_indev_set_drag_obj(&button,false);
    assert(!lv_obj_has_flag(&button,LV_OBJ_FLAG_USER_4) && !lv_obj_has_flag(&button,LV_OBJ_FLAG_PRESS_LOCK));
    lv_port_indev_set_drag_obj(NULL,true);
    puts("pull-down capture: PASS slow held drag, release/flick/reversal, raw capture, atomic fallback, ordinary buttons");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="un260-pulldown-test-") as directory:
    work = Path(directory)
    source = work / "test.c"
    binary = work / ("test.exe" if os.name == "nt" else "test")
    # Source generation is an intentional test fixture, not a production rewrite.
    source.write_text(stub + production + "\nstatic void raw_capture(void) {\n"
                      + "bool capture=true;\n" + capture + "\n}\n" + test, encoding="utf-8")
    flags = ["-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("UN260_TEST_SANITIZE", "0" if os.name == "nt" else "1") == "1":
        flags.append("-fsanitize=address,undefined")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
