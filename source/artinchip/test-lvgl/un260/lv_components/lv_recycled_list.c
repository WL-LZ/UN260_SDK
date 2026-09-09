#include "lv_recycled_list.h"
#include "un260/lv_system/ui_update_batch.h"
#include <math.h>
#include <string.h>

#define MAX_POOL 16
#define NO_ROW UINT32_MAX
#define DECAY 7.5f
#define MAX_SPEED 2600.0f
#define EDGE_LIMIT 48.0f
#define EDGE_GAIN 0.42f
#define RETURN_OMEGA 22.0f

typedef enum { DRAG_UNDECIDED, DRAG_HORIZONTAL, DRAG_VERTICAL } drag_axis_t;

struct lv_recycled_list {
    lv_obj_t *object, *thumb;
    lv_obj_t *row[MAX_POOL];
    uint32_t bound[MAX_POOL];
    lv_timer_t *timer;
    lv_recycled_list_config_t config;
    ui_list_window_t window;
    uint32_t tick, sample_tick, motion_tick;
    uint32_t reported_first, reported_last;
    lv_coord_t press_x, press_y;
    float anchor, sample_offset, velocity, overscroll;
    int32_t drawn_offset, drawn_overscroll, page_drag;
    drag_axis_t axis;
    bool pressed, moved;
};

/* The data window always stays clamped. Only row projection may stretch;
 * unlike a snapped card chooser, a vertical list settles at any valid pixel. */
static float edge_limit(const lv_recycled_list_t *list)
{ return fminf(EDGE_LIMIT, list->window.rows * list->window.row_height * 0.22f); }

static float rubber(const lv_recycled_list_t *list, float distance)
{ return distance * EDGE_GAIN / (1.0f + fabsf(distance) * EDGE_GAIN / edge_limit(list)); }

static float un_rubber(const lv_recycled_list_t *list, float distance)
{
    float limit = edge_limit(list);
    float d = fmaxf(-limit + 0.01f, fminf(limit - 0.01f, distance));
    return d / (EDGE_GAIN * (1.0f - fabsf(d) / limit));
}

static void drag_to(lv_recycled_list_t *list, float position)
{
    ui_list_window_move(&list->window, position);
    list->overscroll = ui_list_window_limit(&list->window) > 0 ?
        rubber(list, position - list->window.offset) : 0;
}

static void set_visible(lv_obj_t *o, bool visible)
{
    if (visible == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (visible) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void project(lv_recycled_list_t *list, bool force)
{
    ui_list_window_t *w = &list->window;
    /* The nonnegative logical offset and first-row calculation both floor;
     * rounding here would recycle a row up to half a pixel too early. */
    int32_t offset = (int32_t)w->offset;
    uint32_t first = (uint32_t)offset / w->row_height;
    if (w->paged) first = w->first;
    int32_t pull = (int32_t)lroundf(list->overscroll);
    float limit = ui_list_window_limit(w);
    lv_opa_t thumb_opa = w->offset <= 0 || w->offset >= limit ? LV_OPA_COVER : LV_OPA_40;
    lv_opa_t current_opa = lv_obj_get_style_bg_opa(list->thumb, 0);
    if (!force && offset == list->drawn_offset && pull == list->drawn_overscroll &&
        current_opa == thumb_opa) return;
    list->drawn_offset = offset;
    list->drawn_overscroll = pull;
    ui_update_batch_t batch;
    ui_update_batch_begin(&batch, list->object, UI_UPDATE_BATCH_INVALIDATE_ROOT);
    /* Modulo slots rebind only a newly exposed edge row; local y never grows
     * with the data index. No huge spacer, 16-bit wrap, or per-frame allocation. */
    for (unsigned k = 0; k <= w->rows; ++k) {
        uint32_t index = first + k;
        unsigned slot = index % (w->rows + 1);
        lv_obj_t *row = list->row[slot];
        bool visible = index < w->count && (!w->paged || k < w->rows);
        set_visible(row, visible);
        if (!visible) { list->bound[slot] = NO_ROW; continue; }
        if (force || list->bound[slot] != index) {
            list->config.bind_row(row, index, list->config.context);
            list->bound[slot] = index;
        }
        int32_t y = (int32_t)(index * w->row_height) - offset - pull;
        lv_obj_set_y(row, (lv_coord_t)y);
    }
    bool scrolling = !w->paged && w->count > w->rows;
    set_visible(list->thumb, scrolling);
    if (current_opa != thumb_opa) lv_obj_set_style_bg_opa(list->thumb, thumb_opa, 0);
    if (scrolling) {
        int height = w->rows * w->row_height;
        int thumb_h = (int)((uint64_t)height * w->rows / w->count);
        if (thumb_h < 18) thumb_h = 18;
        if (thumb_h > height) thumb_h = height;
        int y = (int)((height - thumb_h) * w->offset / limit);
        lv_obj_set_height(list->thumb, thumb_h);
        lv_obj_set_y(list->thumb, y);
    }
    ui_update_batch_end(&batch);
    uint32_t last = ui_list_window_last(w);
    if (force || list->reported_first != w->first || list->reported_last != last) {
        list->reported_first = w->first;
        list->reported_last = last;
        if (list->config.changed) list->config.changed(w, list->config.context);
    }
}

void lv_recycled_list_stop(lv_recycled_list_t *list)
{
    if (!list) return;
    list->pressed = list->moved = false;
    list->axis = DRAG_UNDECIDED;
    list->page_drag = 0;
    list->velocity = 0;
    if (list->timer) lv_timer_pause(list->timer);
    if (list->overscroll != 0 || list->drawn_overscroll != 0) {
        list->overscroll = 0;
        project(list, false);
    }
}

static void resume_motion(lv_recycled_list_t *list, uint32_t now)
{
    if (!list->timer) { lv_recycled_list_stop(list); return; }
    list->tick = now;
    lv_timer_reset(list->timer);
    lv_timer_resume(list->timer);
}

static void motion_tick(lv_timer_t *timer)
{
    lv_recycled_list_t *list = timer->user_data;
    uint32_t now = lv_tick_get();
    float seconds = (uint32_t)(now - list->tick) * 0.001f;
    list->tick = now;
    if (!lv_obj_is_visible(list->object) || list->window.paged ||
        ui_list_window_limit(&list->window) <= 0) {
        lv_recycled_list_stop(list);
        return;
    }
    if (list->overscroll != 0) {
        /* Exact critically damped return: stable even across a slow frame,
         * with no frame-count easing, new allocation or row-index overshoot. */
        float old = list->overscroll;
        float c = list->velocity + RETURN_OMEGA * old;
        float decay = expf(-RETURN_OMEGA * seconds);
        list->overscroll = (old + c * seconds) * decay;
        list->velocity = (list->velocity - RETURN_OMEGA * c * seconds) * decay;
        if (old * list->overscroll <= 0 ||
            (fabsf(list->overscroll) < 0.25f && fabsf(list->velocity) < 4)) {
            lv_recycled_list_stop(list);
            return;
        }
        float bound = edge_limit(list);
        if (fabsf(list->overscroll) > bound) {
            list->overscroll = copysignf(bound, list->overscroll);
            list->velocity = 0;
        }
    } else {
        float decay = expf(-DECAY * seconds);
        float next = list->window.offset + list->velocity * (1 - decay) / DECAY;
        list->velocity *= decay;
        drag_to(list, next);
        if (list->overscroll != 0) {
            /* Keep the spring below the rubber asymptote, including short
             * viewports. Hitting that limit makes the next drag anchor huge. */
            float available = edge_limit(list) - fabsf(list->overscroll);
            float outward = fminf(fabsf(list->velocity) * EDGE_GAIN, RETURN_OMEGA * available);
            list->velocity = copysignf(outward, list->velocity);
        }
    }
    project(list, false);
    if (list->overscroll == 0 && fabsf(list->velocity) < 8)
        lv_recycled_list_stop(list);
}

static void drag_pointer(lv_recycled_list_t *list, const lv_point_t *p, uint32_t now)
{
    int32_t dx = (int32_t)p->x - list->press_x, dy = (int32_t)p->y - list->press_y;
    if (list->axis == DRAG_UNDECIDED) {
        if (LV_MAX(LV_ABS(dx), LV_ABS(dy)) < 6) return;
        if (LV_ABS(dx) * 4 > LV_ABS(dy) * 5) list->axis = DRAG_HORIZONTAL;
        else if (LV_ABS(dy) * 4 > LV_ABS(dx) * 5) list->axis = DRAG_VERTICAL;
        else return;
    }
    float position;
    if (list->window.paged) {
        if (list->axis != DRAG_HORIZONTAL || ui_list_window_pages(&list->window) <= 1) return;
        list->page_drag = -dx;
        position = list->page_drag;
    } else {
        if (list->axis != DRAG_VERTICAL || ui_list_window_limit(&list->window) <= 0) return;
        drag_to(list, list->anchor - dy);
        position = list->window.offset + list->overscroll;
    }
    list->moved = true;
    uint32_t dt = now - list->sample_tick;
    if (dt >= 8) {
        float delta = position - list->sample_offset;
        if (fabsf(delta) > 0.1f) list->motion_tick = now;
        float v = fmaxf(-MAX_SPEED, fminf(MAX_SPEED, delta * 1000 / dt));
        list->velocity = v * 0.65f + list->velocity * 0.35f;
        list->sample_offset = position;
        list->sample_tick = now;
    }
    if (!list->window.paged) project(list, false);
}

static void event_cb(lv_event_t *e)
{
    lv_recycled_list_t *list = lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_DELETE) {
        /* A caller-created row may bubble DELETE while its parent is being
         * torn down. Only the viewport owns this allocation. */
        if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;
        if (list->timer) lv_timer_del(list->timer);
        lv_mem_free(list);
        return;
    }
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
         code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    uint32_t now = lv_tick_get();
    if (code == LV_EVENT_PRESSED && indev) {
        /* Interrupt a spring at its current visual location, not at the edge. */
        if (list->timer) lv_timer_pause(list->timer);
        list->velocity = 0;
        list->moved = false;
        list->axis = DRAG_UNDECIDED;
        list->page_drag = 0;
        lv_indev_get_point(indev, &p);
        list->press_x = p.x;
        list->press_y = p.y;
        list->anchor = list->window.offset + un_rubber(list, list->overscroll);
        list->sample_offset = list->window.paged ? 0 : list->window.offset + list->overscroll;
        list->sample_tick = list->motion_tick = now;
        list->pressed = true;
    } else if (code == LV_EVENT_PRESSING && list->pressed && indev) {
        lv_indev_get_point(indev, &p);
        drag_pointer(list, &p, now);
    } else if (code == LV_EVENT_RELEASED && list->pressed) {
        if (indev) {
            lv_indev_get_point(indev, &p);
            drag_pointer(list, &p, now);
        }
        list->pressed = false;
        if (list->window.paged) {
            float distance = fabsf((float)list->page_drag);
            float threshold = fmaxf(36, fminf(64, list->config.width * 0.15f));
            bool flick = distance >= 20 && (uint32_t)(now - list->motion_tick) <= 100 &&
                fabsf(list->velocity) >= 450 && list->velocity * list->page_drag > 0;
            int step = list->moved && (distance >= threshold || flick) ?
                (list->page_drag > 0 ? 1 : -1) : 0;
            lv_recycled_list_stop(list);
            if (step) lv_recycled_list_page_step(list, step);
        } else if (list->overscroll != 0) {
            list->velocity = 0;
            resume_motion(list, now);
        } else if (!list->moved || (uint32_t)(now - list->motion_tick) > 100 ||
            fabsf(list->velocity) < 8 || !list->timer) {
            lv_recycled_list_stop(list);
        } else {
            resume_motion(list, now);
        }
    } else if (code == LV_EVENT_PRESS_LOST) {
        lv_recycled_list_stop(list);
    }
}

lv_recycled_list_t *lv_recycled_list_create(lv_obj_t *parent,
                                            const lv_recycled_list_config_t *cfg)
{
    if (!parent || !cfg || !cfg->rows || cfg->rows >= MAX_POOL ||
        !cfg->row_height || !cfg->create_row || !cfg->bind_row ||
        cfg->width <= 24 || cfg->width > LV_COORD_MAX ||
        (uint32_t)(cfg->rows + 1U) * cfg->row_height + (uint32_t)EDGE_LIMIT > (uint32_t)LV_COORD_MAX)
        return NULL;
    lv_recycled_list_t *list = lv_mem_alloc(sizeof(*list));
    if (!list) return NULL;
    memset(list, 0, sizeof(*list));
    list->config = *cfg;
    ui_list_window_init(&list->window, cfg->rows, cfg->row_height);
    list->drawn_offset = -1;
    memset(list->bound, 0xff, sizeof(list->bound));
    list->object = lv_obj_create(parent);
    if (!list->object) {
        lv_mem_free(list);
        return NULL;
    }
    lv_obj_remove_style_all(list->object);
    lv_obj_set_pos(list->object, cfg->x, cfg->y);
    lv_obj_set_size(list->object, cfg->width, cfg->rows * cfg->row_height);
    lv_obj_clear_flag(list->object, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(list->object, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    if (!lv_obj_add_event_cb(list->object, event_cb, LV_EVENT_ALL, list)) {
        /* No DELETE owner was installed yet; free the private state here. */
        lv_obj_del(list->object);
        lv_mem_free(list);
        return NULL;
    }
    for (unsigned i = 0; i <= cfg->rows; ++i) {
        list->row[i] = cfg->create_row(list->object, cfg->width - 24, cfg->context);
        if (!list->row[i]) goto creation_failed;
        lv_obj_clear_flag(list->row[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(list->row[i], LV_OBJ_FLAG_HIDDEN);
    }
    list->thumb = lv_obj_create(list->object);
    if (!list->thumb) goto creation_failed;
    lv_obj_remove_style_all(list->thumb);
    lv_obj_set_size(list->thumb, 4, 18);
    /* 24px content-to-panel gutter; thumb center is 12px from panel edge. */
    lv_obj_set_x(list->thumb, cfg->width - 14);
    lv_obj_set_style_radius(list->thumb, 2, 0);
    lv_obj_set_style_bg_color(list->thumb, lv_color_hex(0xC6D0D8), 0);
    lv_obj_set_style_bg_opa(list->thumb, LV_OPA_COVER, 0);
    lv_obj_clear_flag(list->thumb, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(list->thumb, LV_OBJ_FLAG_HIDDEN);
    list->timer = lv_timer_create(motion_tick, 16, list);
    if (list->timer) lv_timer_pause(list->timer);
    return list;

creation_failed:
    /* DELETE also frees list; all partially created rows belong to object. */
    lv_obj_del(list->object);
    return NULL;
}

lv_obj_t *lv_recycled_list_object(lv_recycled_list_t *list)
{ return list ? list->object : NULL; }
const ui_list_window_t *lv_recycled_list_window(const lv_recycled_list_t *list)
{ return list ? &list->window : NULL; }
void lv_recycled_list_refresh(lv_recycled_list_t *list, uint32_t count, bool reset)
{
    if (!list) return;
    float previous = list->window.offset;
    float old_pull = list->overscroll;
    if (reset) lv_recycled_list_stop(list);
    ui_list_window_update(&list->window, count, reset);
    if (!reset) {
        float correction = list->window.offset - previous;
        /* Late rows must not cancel the pointer's current press. If shrinking
         * clamps the window, rebase both drag and velocity-sample anchors so
         * the next PRESSING cannot jump back to the old logical position. */
        if (!list->window.paged && old_pull != 0) {
            /* A growing result can turn a stretched bottom into valid content.
             * Preserve its visual position; a shrink rebases to the new edge. */
            float position = list->window.offset + old_pull;
            ui_list_window_move(&list->window, position);
            list->overscroll = ui_list_window_limit(&list->window) > 0 ?
                position - list->window.offset : 0;
            list->anchor += list->window.offset + un_rubber(list, list->overscroll) -
                (previous + un_rubber(list, old_pull));
            list->sample_offset += list->window.offset + list->overscroll - (previous + old_pull);
        } else {
            list->anchor += correction;
            if (!list->window.paged) list->sample_offset += correction;
        }
        if (correction != 0 || old_pull != list->overscroll || ui_list_window_limit(&list->window) <= 0) {
            list->velocity = 0;
            if (list->timer) lv_timer_pause(list->timer);
            if (!list->pressed && list->overscroll != 0) resume_motion(list, lv_tick_get());
        }
    }
    project(list, true);
}
void lv_recycled_list_set_paged(lv_recycled_list_t *list, bool paged)
{
    if (!list || list->window.paged == paged) return;
    lv_recycled_list_stop(list);
    ui_list_window_mode(&list->window, paged);
    project(list, true);
}
void lv_recycled_list_page_step(lv_recycled_list_t *list, int step)
{
    if (!list || !list->window.paged || step == 0) return;
    lv_recycled_list_stop(list);
    ui_list_window_page(&list->window, step);
    project(list, true);
}

bool lv_recycled_list_scroll_to_index(lv_recycled_list_t *list, uint32_t index)
{
    if (!list || index >= list->window.count) return false;
    lv_recycled_list_stop(list);
    ui_list_window_t *window = &list->window;
    if (window->paged) {
        /* The window's safe count bounds both page numbers below INT32_MAX. */
        int step = (int)(index / window->rows) - (int)(window->first / window->rows);
        ui_list_window_page(window, step);
    } else {
        ui_list_window_move(window, (float)index * window->row_height);
    }
    project(list, true);
    return true;
}

bool lv_recycled_list_index_at_point(lv_recycled_list_t *list,
                                     const lv_point_t *point, uint32_t *out_index)
{
    if (!list || !point || !out_index) return false;
    lv_obj_update_layout(list->object);
    if (!lv_obj_is_visible(list->object)) return false;
    lv_area_t area;
    lv_obj_get_coords(list->object, &area);
    if (point->x < area.x1 || point->x > area.x2 ||
        point->y < area.y1 || point->y > area.y2) return false;
    for (unsigned i = 0; i <= list->window.rows; ++i) {
        if (list->bound[i] == NO_ROW || list->bound[i] >= list->window.count ||
            !lv_obj_is_visible(list->row[i])) continue;
        lv_obj_get_coords(list->row[i], &area);
        if (point->x < area.x1 || point->x > area.x2 ||
            point->y < area.y1 || point->y > area.y2) continue;
        *out_index = list->bound[i];
        return true;
    }
    return false;
}
