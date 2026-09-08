#include "lv_recycled_list.h"
#include "un260/lv_system/ui_update_batch.h"
#include <math.h>
#include <string.h>

#define MAX_POOL 16
#define NO_ROW UINT32_MAX
#define DECAY 7.5f
#define MAX_SPEED 2600.0f

struct lv_recycled_list {
    lv_obj_t *object, *thumb;
    lv_obj_t *row[MAX_POOL];
    uint32_t bound[MAX_POOL];
    lv_timer_t *timer;
    lv_recycled_list_config_t config;
    ui_list_window_t window;
    uint32_t tick, sample_tick, motion_tick;
    uint32_t reported_first, reported_last;
    lv_coord_t press_y;
    float anchor, sample_offset, velocity;
    int32_t drawn_offset;
    bool pressed, moved;
};

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
    if (!force && offset == list->drawn_offset) return;
    list->drawn_offset = offset;
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
        int32_t y = (int32_t)(index * w->row_height) - offset;
        lv_obj_set_y(row, (lv_coord_t)y);
    }
    bool scrolling = !w->paged && w->count > w->rows;
    set_visible(list->thumb, scrolling);
    if (scrolling) {
        int height = w->rows * w->row_height;
        int thumb_h = (int)((uint64_t)height * w->rows / w->count);
        if (thumb_h < 18) thumb_h = 18;
        if (thumb_h > height) thumb_h = height;
        int y = (int)((height - thumb_h) * w->offset / ui_list_window_limit(w));
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
    list->velocity = 0;
    if (list->timer) lv_timer_pause(list->timer);
}

static void motion_tick(lv_timer_t *timer)
{
    lv_recycled_list_t *list = timer->user_data;
    uint32_t now = lv_tick_get();
    float seconds = (uint32_t)(now - list->tick) * 0.001f;
    list->tick = now;
    float decay = expf(-DECAY * seconds);
    float next = list->window.offset + list->velocity * (1 - decay) / DECAY;
    list->velocity *= decay;
    ui_list_window_move(&list->window, next);
    project(list, false);
    if (fabsf(list->velocity) < 8 || list->window.offset <= 0 ||
        list->window.offset >= ui_list_window_limit(&list->window))
        lv_recycled_list_stop(list);
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
    if (list->window.paged ||
        (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
         code != LV_EVENT_RELEASED && code != LV_EVENT_PRESS_LOST)) return;
    lv_indev_t *indev = lv_event_get_indev(e);
    lv_point_t p;
    uint32_t now = lv_tick_get();
    if (code == LV_EVENT_PRESSED && indev) {
        lv_recycled_list_stop(list);
        lv_indev_get_point(indev, &p);
        list->press_y = p.y;
        list->anchor = list->sample_offset = list->window.offset;
        list->sample_tick = list->motion_tick = now;
        list->pressed = true;
    } else if (code == LV_EVENT_PRESSING && list->pressed && indev) {
        lv_indev_get_point(indev, &p);
        if (!list->moved && LV_ABS(p.y - list->press_y) < 6) return;
        list->moved = true;
        ui_list_window_move(&list->window, list->anchor - (p.y - list->press_y));
        uint32_t dt = now - list->sample_tick;
        if (dt >= 8) {
            float delta = list->window.offset - list->sample_offset;
            if (fabsf(delta) > 0.1f) list->motion_tick = now;
            float v = delta * 1000 / dt;
            if (v > MAX_SPEED) v = MAX_SPEED;
            if (v < -MAX_SPEED) v = -MAX_SPEED;
            list->velocity = v * 0.65f + list->velocity * 0.35f;
            list->sample_offset = list->window.offset;
            list->sample_tick = now;
        }
        project(list, false);
    } else if (code == LV_EVENT_RELEASED && list->pressed) {
        list->pressed = false;
        if (!list->moved || (uint32_t)(now - list->motion_tick) > 100 ||
            fabsf(list->velocity) < 8 || !list->timer) {
            lv_recycled_list_stop(list);
        } else {
            list->tick = now;
            lv_timer_reset(list->timer);
            lv_timer_resume(list->timer);
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
        (uint32_t)(cfg->rows + 1U) * cfg->row_height > (uint32_t)LV_COORD_MAX)
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
    if (reset) lv_recycled_list_stop(list);
    ui_list_window_update(&list->window, count, reset);
    if (!reset) {
        float correction = list->window.offset - previous;
        /* Late rows must not cancel the pointer's current press. If shrinking
         * clamps the window, rebase both drag and velocity-sample anchors so
         * the next PRESSING cannot jump back to the old logical position. */
        list->anchor += correction;
        list->sample_offset += correction;
        if (correction != 0 || ui_list_window_limit(&list->window) <= 0) {
            list->velocity = 0;
            if (list->timer) lv_timer_pause(list->timer);
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
