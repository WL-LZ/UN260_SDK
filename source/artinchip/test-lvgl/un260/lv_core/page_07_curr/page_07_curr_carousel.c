#include "un260/lv_core/page_07_curr/page_07_curr_carousel.h"
#include "lv_port_indev.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_drivers/uart_io.h"
#include "un260/lv_system/app_clock.h"

#include <string.h>

#define CAROUSEL_DRAG_THRESHOLD 14
#define CAROUSEL_TIMER_MS 16

static int abs_delta(int value) { return value < 0 ? -value : value; }

static void trace(page07_curr_carousel_t *c, const char *event)
{
    if (!c->profile_tracking || !perf_profile_is_enabled()) return;
    unsigned target = c->motion.stride > 0.0f ?
        (unsigned)(c->motion.target / c->motion.stride + 0.5f) : 0U;
    /* project_us is total/max, keeping even worst-case counters within the
     * shared UART logger's 256-byte line buffer. Only action boundaries emit. */
    uart_debug_printf("[PERF_CAROUSEL] event=%s x=%ld v=%ld targetIndex=%u "
           "elapsed_ms=%lu steps=%lu flush_frames=%lu max_tick_gap_ms=%lu reason=%s "
           "project_calls=%lu project_us=%llu/%lu\n",
           event, (long)c->motion.position, (long)c->motion.velocity, target,
           (unsigned long)(lv_tick_get() - c->profile_started),
           (unsigned long)c->profile_steps,
           (unsigned long)(perf_profile_frame_sequence() - c->profile_frame_start),
           (unsigned long)c->profile_max_gap, c->profile_reason,
           (unsigned long)c->profile_project_calls,
           (unsigned long long)c->profile_project_total_us,
           (unsigned long)c->profile_project_max_us);
}

static void trace_reset(page07_curr_carousel_t *c, const char *reason,
                        bool reset_projects)
{
    c->profile_tracking = perf_profile_is_enabled();
    if (!c->profile_tracking) return;
    c->profile_started = c->profile_last_tick = lv_tick_get();
    c->profile_max_gap = c->profile_steps = 0;
    /* Reuse the existing diagnostic flush counter. The page must not depend
     * on one framebuffer implementation or an uncommitted display pipeline. */
    c->profile_frame_start = perf_profile_frame_sequence();
    c->profile_reason = reason;
    if (reset_projects) {
        c->profile_project_calls = 0;
        c->profile_project_total_us = 0;
        c->profile_project_max_us = 0;
    }
}

static void trace_begin(page07_curr_carousel_t *c, const char *event,
                        const char *reason)
{
    /* Keep elapsed_ms/steps/flush_frames relative to release, while
     * retaining the direct-drag projection work counted since PRESSED. */
    trace_reset(c, reason, !c->profile_tracking);
    trace(c, event);
}

static void trace_settled(page07_curr_carousel_t *c)
{
    trace(c, "SETTLED");
    c->profile_tracking = false;
}

static void project(page07_curr_carousel_t *c)
{
    if (c->viewport == NULL || c->project == NULL) return;
    if (!c->profile_tracking || !perf_profile_is_enabled()) {
        c->project();
        return;
    }
    lv_obj_t *viewport = c->viewport;
    void (*render)(void) = c->project;
    uint64_t started_us = app_clock_monotonic_us();
    render();
    uint32_t elapsed_us = app_clock_elapsed_us32(started_us,
                                                app_clock_monotonic_us());
    /* The owner context is retained, but a callback may have cleared it.
     * Never dereference a former viewport or attribute work to a reset owner. */
    if (!c->profile_tracking || c->viewport != viewport || c->project != render)
        return;
    ++c->profile_project_calls;
    c->profile_project_total_us += elapsed_us;
    if (elapsed_us > c->profile_project_max_us)
        c->profile_project_max_us = elapsed_us;
}

static void schedule(page07_curr_carousel_t *c)
{
    if (c->motion.phase == UI_SCROLL_IDLE || !c->enabled) {
        if (c->timer != NULL) lv_timer_pause(c->timer);
        trace_settled(c);
    } else if (c->timer != NULL) {
        lv_timer_reset(c->timer);
        lv_timer_resume(c->timer);
    } else {
        /* Timer allocation failure keeps direct dragging and taps usable. */
        ui_scroll_physics_stop(&c->motion);
        project(c);
        c->profile_reason = "timer_unavailable";
        trace_settled(c);
    }
}

static void motion_timer_cb(lv_timer_t *timer)
{
    page07_curr_carousel_t *c = timer->user_data;
    if (c == NULL || !c->enabled || c->viewport == NULL || c->active) {
        lv_timer_pause(timer);
        return;
    }
    uint32_t now = lv_tick_get();
    if (c->profile_tracking) {
        uint32_t gap = now - c->profile_last_tick;
        if (gap > c->profile_max_gap) c->profile_max_gap = gap;
        c->profile_last_tick = now;
        c->profile_steps++;
    }
    bool moving = ui_scroll_physics_step(&c->motion, now);
    project(c);
    if (!moving) {
        lv_timer_pause(timer);
        trace_settled(c);
    }
}

void page07_curr_carousel_stop(page07_curr_carousel_t *c)
{
    if (c == NULL) return;
    if (page07_curr_carousel_busy(c)) {
        if (!c->profile_tracking) trace_begin(c, "CANCEL", "owner_stop");
        else {
            c->profile_reason = "owner_stop";
            trace(c, "CANCEL");
        }
    }
    c->profile_tracking = false;
    if (c->timer != NULL) lv_timer_pause(c->timer);
    c->active = false;
    c->dragging = false;
    c->suppress_click = true;
    ui_scroll_physics_stop(&c->motion);
    project(c);
}

void page07_curr_carousel_destroy(page07_curr_carousel_t *c)
{
    if (c == NULL) return;
    if (page07_curr_carousel_busy(c)) {
        if (!c->profile_tracking) trace_begin(c, "CANCEL", "owner_destroy");
        else {
            c->profile_reason = "owner_destroy";
            trace(c, "CANCEL");
        }
    }
    if (c->timer != NULL) lv_timer_del(c->timer);
    /* Called before the viewport is deleted: do not project into its tree. */
    memset(c, 0, sizeof(*c));
}

static void viewport_event_cb(lv_event_t *event)
{
    page07_curr_carousel_t *c = lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (c == NULL) return;
    if (code == LV_EVENT_DELETE) {
        if (lv_event_get_target(event) == c->viewport) {
            page07_curr_carousel_destroy(c);
        }
        return;
    }
    if (!c->enabled || c->viewport == NULL) return;

    if (code == LV_EVENT_PRESS_LOST) {
        if (!c->active) return;
        c->active = false;
        c->dragging = false;
        c->suppress_click = true;
        c->last_interaction = lv_tick_get();
        ui_scroll_physics_release(&c->motion, c->last_interaction, true);
        trace_begin(c, "CANCEL", "press_lost");
        project(c);
        schedule(c);
        return;
    }

    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    uint32_t now = lv_tick_get();

    if (code == LV_EVENT_PRESSED) {
        bool was_moving = c->motion.phase == UI_SCROLL_COAST ||
                          c->motion.phase == UI_SCROLL_SPRING;
        bool was_active = c->active;
        if ((was_moving || was_active) && c->profile_tracking) {
            c->profile_reason = was_active ? "press_restart" : "press_interrupt";
            trace(c, "CANCEL");
            c->profile_tracking = false;
        }
        if (c->timer != NULL) lv_timer_pause(c->timer);
        c->press_anchor = ui_scroll_physics_begin(&c->motion, now);
        c->press_point = point;
        c->active = true;
        c->dragging = false;
        /* A contact intended to catch moving cards must not submit a currency
         * merely because it was released before crossing the drag threshold. */
        c->suppress_click = was_moving || was_active;
        c->last_interaction = now;
        /* Start counters without emitting a log in the press/drag hot path. */
        trace_reset(c, "press", true);
        return;
    }
    if (!c->active) return;

    if (code == LV_EVENT_PRESSING) {
        int dx = point.x - c->press_point.x;
        int dy = point.y - c->press_point.y;
        c->last_interaction = now;
        if (!c->dragging && abs_delta(dy) > CAROUSEL_DRAG_THRESHOLD &&
            abs_delta(dy) > abs_delta(dx)) {
            /* A vertical intent must not become a tap or later turn into a
             * horizontal fling during the same contact. */
            c->active = false;
            c->suppress_click = true;
            ui_scroll_physics_release(&c->motion, now, true);
            trace_begin(c, "CANCEL", "vertical_intent");
            schedule(c);
            return;
        }
        if (!c->dragging && abs_delta(dx) > CAROUSEL_DRAG_THRESHOLD &&
            abs_delta(dx) >= abs_delta(dy)) {
            c->dragging = true;
            c->suppress_click = true;
        }
        if (c->dragging) {
            ui_scroll_physics_drag(&c->motion, c->press_anchor - dx, now);
            project(c);
        }
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        bool cancelled = !c->dragging;
        c->active = false;
        c->dragging = false;
        c->last_interaction = now;
        ui_scroll_physics_release(&c->motion, now, cancelled);
        trace_begin(c, cancelled && c->suppress_click ? "CANCEL" : "RELEASE",
                     cancelled ? (c->suppress_click ? "press_interrupt" : "tap") : "drag");
        project(c);
        schedule(c);
    }
}

void page07_curr_carousel_bind_child(lv_obj_t *obj)
{
    if (obj == NULL) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_port_indev_set_drag_obj(obj, true);
}

void page07_curr_carousel_init(page07_curr_carousel_t *c, lv_obj_t *viewport,
                               unsigned count, unsigned stride, unsigned index,
                               void (*render)(void))
{
    page07_curr_carousel_destroy(c);
    if (c == NULL || viewport == NULL) return;
    ui_scroll_physics_init(&c->motion, count, (float)stride, index);
    c->viewport = viewport;
    c->project = render;
    c->enabled = true;
    c->last_interaction = lv_tick_get();
    c->timer = lv_timer_create(motion_timer_cb, CAROUSEL_TIMER_MS, c);
    if (c->timer != NULL) lv_timer_pause(c->timer);
    lv_obj_clear_flag(viewport, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(viewport, LV_OBJ_FLAG_CLICKABLE);
    lv_port_indev_set_drag_obj(viewport, true);
    lv_obj_add_event_cb(viewport, viewport_event_cb, LV_EVENT_ALL, c);
}

void page07_curr_carousel_enable(page07_curr_carousel_t *c, bool enabled)
{
    if (c == NULL) return;
    if (!enabled) page07_curr_carousel_stop(c);
    c->enabled = enabled;
}

void page07_curr_carousel_snap(page07_curr_carousel_t *c, unsigned index,
                               bool animate)
{
    if (c == NULL || c->viewport == NULL) return;
    if (page07_curr_carousel_busy(c) && c->profile_tracking) {
        c->profile_reason = "snap_interrupt";
        trace(c, "CANCEL");
    }
    trace_reset(c, "snap", true);
    c->active = false;
    c->dragging = false;
    c->suppress_click = true;
    ui_scroll_physics_snap(&c->motion, index, animate && c->enabled, lv_tick_get());
    project(c);
    schedule(c);
}

bool page07_curr_carousel_busy(const page07_curr_carousel_t *c)
{
    return c != NULL && (c->active || c->motion.phase != UI_SCROLL_IDLE);
}

bool page07_curr_carousel_click_allowed(const page07_curr_carousel_t *c)
{
    return c != NULL && c->enabled && !c->active && !c->suppress_click;
}
