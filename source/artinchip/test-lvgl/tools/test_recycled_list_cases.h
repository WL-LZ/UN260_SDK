#ifndef TEST_RECYCLED_LIST_CASES_H
#define TEST_RECYCLED_LIST_CASES_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_components/lv_recycled_list.h"

/* Host-only interaction tests against the real LVGL 8.3 pointer/event path.
 * Call after lv_init() and display registration, with no page under test yet.
 * This does not register a device or simulate the controller protocol. */
typedef struct {
    lv_obj_t *row[16];
    uint32_t index[16];
    unsigned created, deleted, calls, binds, changes, fail_at;
    bool partial_failure;
} recycled_cases_context_t;

/* Linked with --wrap=lv_timer_create only in this host test executable. */
static bool recycled_cases_fail_next_timer;
static unsigned recycled_cases_failed_timers;
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t callback, uint32_t period, void *data);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t callback, uint32_t period, void *data)
{
    if (recycled_cases_fail_next_timer && period == 16) {
        recycled_cases_fail_next_timer = false;
        ++recycled_cases_failed_timers;
        return NULL;
    }
    return __real_lv_timer_create(callback, period, data);
}

static unsigned recycled_cases_timer_count(void)
{
    unsigned count = 0;
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t)) ++count;
    return count;
}

static lv_timer_t *recycled_cases_owned_timer(lv_recycled_list_t *list)
{
    for (lv_timer_t *t = lv_timer_get_next(NULL); t; t = lv_timer_get_next(t))
        if (t->user_data == list) return t;
    return NULL;
}

static void recycled_cases_row_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    recycled_cases_context_t *ctx = lv_event_get_user_data(event);
    ++ctx->deleted;
}

static lv_obj_t *recycled_cases_create_row(lv_obj_t *parent, lv_coord_t width, void *context)
{
    recycled_cases_context_t *ctx = context;
    bool fail = ++ctx->calls == ctx->fail_at;
    if (fail && !ctx->partial_failure) return NULL;
    assert(ctx->created < 16);
    lv_obj_t *row = lv_obj_create(parent);
    assert(row);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, width, 36);
    /* Destruction also exercises a caller-owned child that bubbles DELETE. */
    lv_obj_add_flag(row, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(row, recycled_cases_row_deleted, LV_EVENT_DELETE, ctx);
    ctx->row[ctx->created] = row;
    ctx->index[ctx->created++] = UINT32_MAX;
    return fail ? NULL : row;
}

static void recycled_cases_bind_row(lv_obj_t *row, uint32_t index, void *context)
{
    recycled_cases_context_t *ctx = context;
    for (unsigned i = 0; i < ctx->created; ++i) {
        if (ctx->row[i] != row) continue;
        ctx->index[i] = index;
        ++ctx->binds;
        return;
    }
    assert(!"binding an unowned recycled row");
}

static void recycled_cases_changed(const ui_list_window_t *window, void *context)
{
    recycled_cases_context_t *ctx = context;
    assert(window->first <= window->count);
    ++ctx->changes;
}

static lv_recycled_list_config_t recycled_cases_config(recycled_cases_context_t *ctx)
{
    lv_recycled_list_config_t cfg = {0};
    cfg.width = 320;
    cfg.rows = 7;
    cfg.row_height = 36;
    cfg.create_row = recycled_cases_create_row;
    cfg.bind_row = recycled_cases_bind_row;
    cfg.changed = recycled_cases_changed;
    cfg.context = ctx;
    return cfg;
}

static void recycled_cases_input_xy(lv_recycled_list_t *list, lv_indev_t *indev,
                                    lv_event_code_t code, lv_coord_t x, lv_coord_t y,
                                    uint32_t elapsed)
{
    lv_tick_inc(elapsed);
    indev->proc.types.pointer.act_point.x = x;
    indev->proc.types.pointer.act_point.y = y;
    indev->proc.state = code == LV_EVENT_RELEASED ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
    assert(lv_event_send(lv_recycled_list_object(list), code, indev) == LV_RES_OK);
    const ui_list_window_t *window = lv_recycled_list_window(list);
    if (!window->paged)
        assert(window->offset >= 0 && window->offset <= ui_list_window_limit(window));
}

static void recycled_cases_input(lv_recycled_list_t *list, lv_indev_t *indev,
                                 lv_event_code_t code, lv_coord_t y, uint32_t elapsed)
{ recycled_cases_input_xy(list, indev, code, 100, y, elapsed); }

static void recycled_cases_offset(lv_recycled_list_t *list, float expected)
{
    assert(fabsf(lv_recycled_list_window(list)->offset - expected) < 0.05f);
}

static void recycled_cases_rows(lv_recycled_list_t *list, recycled_cases_context_t *ctx)
{
    const ui_list_window_t *window = lv_recycled_list_window(list);
    unsigned visible = 0;
    unsigned expected = window->paged ? window->rows : window->rows + 1U;
    if (expected > window->count - window->first) expected = window->count - window->first;
    lv_obj_update_layout(lv_recycled_list_object(list));
    for (unsigned i = 0; i < ctx->created; ++i) {
        if (lv_obj_has_flag(ctx->row[i], LV_OBJ_FLAG_HIDDEN)) continue;
        ++visible;
        assert(ctx->index[i] >= window->first && ctx->index[i] < window->count);
        int32_t y = (int32_t)(ctx->index[i] * window->row_height) - (int32_t)window->offset;
        assert(lv_obj_get_y(ctx->row[i]) == y);
        assert(y > -(int32_t)window->row_height && y <= (int32_t)window->rows * window->row_height);
        for (unsigned j = i + 1; j < ctx->created; ++j)
            if (!lv_obj_has_flag(ctx->row[j], LV_OBJ_FLAG_HIDDEN)) assert(ctx->index[i] != ctx->index[j]);
    }
    assert(visible == expected);
    assert(ctx->created == window->rows + 1U);
    assert(lv_obj_get_child_cnt(lv_recycled_list_object(list)) == ctx->created + 1U);
}

static int recycled_cases_pull(lv_recycled_list_t *list, recycled_cases_context_t *ctx)
{
    const ui_list_window_t *window = lv_recycled_list_window(list);
    if (!window->paged)
        assert(window->offset >= 0 && window->offset <= ui_list_window_limit(window));
    lv_obj_update_layout(lv_recycled_list_object(list));
    int shift = 0;
    bool found = false;
    for (unsigned i = 0; i < ctx->created; ++i) {
        if (lv_obj_has_flag(ctx->row[i], LV_OBJ_FLAG_HIDDEN)) continue;
        assert(ctx->index[i] >= window->first && ctx->index[i] < window->count);
        int32_t logical_y = (int32_t)(ctx->index[i] * window->row_height) - (int32_t)window->offset;
        int y = lv_obj_get_y(ctx->row[i]);
        int actual_shift = y - logical_y;
        assert(actual_shift >= -48 && actual_shift <= 48);
        if (found) assert(actual_shift == shift);
        shift = actual_shift;
        found = true;
        assert(y > -(int32_t)window->row_height - 48 && y <= (int32_t)window->rows * window->row_height + 48);
    }
    return shift;
}

static lv_obj_t *recycled_cases_thumb(lv_recycled_list_t *list, recycled_cases_context_t *ctx)
{
    lv_obj_t *object = lv_recycled_list_object(list);
    assert(lv_obj_get_child_cnt(object) == ctx->created + 1U);
    return lv_obj_get_child(object, ctx->created);
}

static void recycled_cases_position(lv_recycled_list_t *list, float offset)
{
    ui_list_window_t *window = (ui_list_window_t *)lv_recycled_list_window(list);
    lv_recycled_list_stop(list);
    ui_list_window_move(window, offset);
    lv_recycled_list_refresh(list, window->count, false);
}

static void recycled_cases_settle(lv_recycled_list_t *list, recycled_cases_context_t *ctx,
                                  lv_timer_t *motion)
{
    int initial = recycled_cases_pull(list, ctx), previous = initial;
    assert(initial != 0 && !motion->paused);
    bool decreased = false;
    for (unsigned elapsed = 0; elapsed < 512 && !motion->paused; elapsed += 16) {
        lv_tick_inc(16);
        lv_timer_handler();
        int current = recycled_cases_pull(list, ctx);
        assert(current == 0 || (current > 0) == (initial > 0));
        assert(abs(current) <= abs(previous));
        if (!elapsed) assert(current != 0); /* No first-frame snap to the bound. */
        if (abs(current) < abs(previous)) decreased = true;
        previous = current;
    }
    assert(decreased && motion->paused && recycled_cases_pull(list, ctx) == 0);
    recycled_cases_rows(list, ctx);
}

static void recycled_cases_swipe(lv_recycled_list_t *list, lv_indev_t *indev,
                                 int dx, int dy, uint32_t elapsed, bool commit)
{
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 200 + dx, 150 + dy, elapsed);
    recycled_cases_input_xy(list, indev, commit ? LV_EVENT_RELEASED : LV_EVENT_PRESS_LOST,
                            200 + dx, 150 + dy, 0);
}

static void recycled_cases_paging(lv_recycled_list_t *list, recycled_cases_context_t *ctx,
                                  lv_indev_t *indev, lv_timer_t *motion)
{
    const ui_list_window_t *window = lv_recycled_list_window(list);
    lv_recycled_list_set_paged(list, true);
    lv_recycled_list_refresh(list, 50, true);
    assert(lv_obj_has_flag(recycled_cases_thumb(list, ctx), LV_OBJ_FLAG_HIDDEN));
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 280, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 210, 150, 200);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 20, 150, 200);
    assert(window->first == 0); /* A drag never commits before release. */
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 20, 150, 0);
    assert(window->first == 7 && motion->paused);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 20, 150, 0);
    assert(window->first == 7); /* Repeated release cannot skip another page. */
    recycled_cases_swipe(list, indev, 70, 0, 200, true);
    assert(window->first == 0);
    recycled_cases_swipe(list, indev, 70, 0, 200, true);
    assert(window->first == 0);
    lv_recycled_list_page_step(list, 1000);
    recycled_cases_swipe(list, indev, -100, 0, 200, true);
    assert(window->first == 49);
    lv_recycled_list_refresh(list, 50, true);

    recycled_cases_swipe(list, indev, -47, 0, 200, true);
    assert(window->first == 0); /* 320px viewport => a 48px distance threshold. */
    recycled_cases_swipe(list, indev, -48, 0, 200, true);
    assert(window->first == 7);
    lv_recycled_list_refresh(list, 50, true);
    recycled_cases_swipe(list, indev, -19, 0, 20, true);
    assert(window->first == 0); /* A fast twitch still needs a 20px net drag. */
    recycled_cases_swipe(list, indev, -30, 0, 200, true);
    assert(window->first == 0);
    recycled_cases_swipe(list, indev, -30, 0, 20, true);
    assert(window->first == 7); /* Short, recent, direction-consistent flick. */
    lv_recycled_list_refresh(list, 50, true);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 170, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 170, 150, 150);
    assert(window->first == 0); /* A held short drag is not a recent flick. */
    recycled_cases_swipe(list, indev, -5, 0, 1, true);
    assert(window->first == 0);
    recycled_cases_swipe(list, indev, 0, -100, 200, true);
    assert(window->first == 0);
    recycled_cases_swipe(list, indev, -70, -70, 200, true);
    assert(window->first == 0); /* Equal-axis diagonals stay undecided. */
    recycled_cases_swipe(list, indev, -50, -40, 200, true);
    assert(window->first == 0); /* Exactly 1.25 is not dominant yet. */
    recycled_cases_swipe(list, indev, -51, -40, 200, true);
    assert(window->first == 7);
    lv_recycled_list_refresh(list, 50, true);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 180, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 200, 160, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 80, 160, 200);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 80, 160, 0);
    assert(window->first == 0); /* A vertical lock cannot turn into paging. */
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 100, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 200, 150, 0);
    assert(window->first == 0);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 130, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 170, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 170, 150, 0);
    assert(window->first == 0); /* Recent velocity opposes the remaining net drag. */
    recycled_cases_swipe(list, indev, -100, 0, 200, false);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 100, 150, 0);
    assert(window->first == 0 && motion->paused);
    for (unsigned count = 0; count <= 7; count += 7) {
        lv_recycled_list_refresh(list, count, true);
        recycled_cases_swipe(list, indev, -180, 0, 20, true);
        recycled_cases_swipe(list, indev, 180, 0, 20, true);
        assert(window->first == 0 && motion->paused);
    }
    recycled_cases_rows(list, ctx);
    lv_recycled_list_set_paged(list, false);
}

static void recycled_cases_page_thresholds(lv_obj_t *parent, lv_indev_t *indev)
{
    const int widths[] = {200, 600}, thresholds[] = {36, 64};
    unsigned timers = recycled_cases_timer_count();
    for (unsigned i = 0; i < 2; ++i) {
        recycled_cases_context_t ctx = {0};
        lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
        cfg.width = widths[i];
        lv_recycled_list_t *list = lv_recycled_list_create(parent, &cfg);
        assert(list);
        lv_recycled_list_set_paged(list, true);
        lv_recycled_list_refresh(list, 50, true);
        recycled_cases_swipe(list, indev, 1-thresholds[i], 0, 250, true);
        assert(lv_recycled_list_window(list)->first == 0);
        recycled_cases_swipe(list, indev, -thresholds[i], 0, 250, true);
        assert(lv_recycled_list_window(list)->first == 7);
        lv_obj_del(lv_recycled_list_object(list));
        assert(ctx.deleted == ctx.created && recycled_cases_timer_count() == timers);
    }
}

static void recycled_cases_scrollbar(lv_recycled_list_t *list, recycled_cases_context_t *ctx,
                                     lv_indev_t *indev, lv_timer_t *motion)
{
    lv_recycled_list_set_paged(list, false);
    lv_recycled_list_refresh(list, 20, true);
    lv_obj_t *thumb = recycled_cases_thumb(list, ctx);
    assert(!lv_obj_has_flag(thumb, LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_style_bg_opa(thumb, 0) == LV_OPA_COVER);
    const float positions[] = {0.2f, 100.0f, 467.8f, 468.0f};
    for (unsigned i = 0; i < 4; ++i) {
        recycled_cases_position(list, positions[i]);
        assert(lv_obj_get_style_bg_opa(thumb, 0) == (i == 3 ? LV_OPA_COVER : LV_OPA_40));
        recycled_cases_rows(list, ctx);
    }
    /* Consecutive pointer samples cross the endpoint while both the floored
     * offset and rounded pull remain zero: opacity cannot use that cache key. */
    recycled_cases_position(list, 6.2f);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 106, 200);
    recycled_cases_offset(list, 0.2f);
    assert(recycled_cases_pull(list, ctx) == 0);
    assert(lv_obj_get_style_bg_opa(thumb, 0) == LV_OPA_40);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 107, 200);
    recycled_cases_offset(list, 0);
    assert(recycled_cases_pull(list, ctx) == 0);
    assert(lv_obj_get_style_bg_opa(thumb, 0) == LV_OPA_COVER);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 106, 200);
    recycled_cases_offset(list, 0.2f);
    assert(lv_obj_get_style_bg_opa(thumb, 0) == LV_OPA_40);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 106, 0);
    assert(motion->paused);
}

static void recycled_cases_rubber(lv_recycled_list_t *list, recycled_cases_context_t *ctx,
                                  lv_indev_t *indev, lv_timer_t *motion)
{
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_swipe(list, indev, -120, 0, 200, true);
    recycled_cases_offset(list, 0);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 120, 20);
    int small = recycled_cases_pull(list, ctx);
    assert(small > 0 && small < 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 140, 20);
    int medium = recycled_cases_pull(list, ctx);
    assert(medium > small && medium-small < small);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 280, 20);
    int large = recycled_cases_pull(list, ctx);
    assert(large > medium && large < 48);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 2100, 20);
    int saturated = recycled_cases_pull(list, ctx);
    assert(saturated >= large && saturated <= 48);
    recycled_cases_offset(list, 0);
    assert(lv_obj_get_style_bg_opa(recycled_cases_thumb(list, ctx), 0) == LV_OPA_COVER);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 2100, 0);
    assert(recycled_cases_pull(list, ctx) == saturated);
    recycled_cases_settle(list, ctx, motion);

    recycled_cases_position(list, ui_list_window_limit(lv_recycled_list_window(list)));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 20, 20);
    assert(recycled_cases_pull(list, ctx) < 0);
    recycled_cases_offset(list, 468);
    assert(lv_obj_get_style_bg_opa(recycled_cases_thumb(list, ctx), 0) == LV_OPA_COVER);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 20, 0);
    recycled_cases_settle(list, ctx, motion);

    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 280, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 280, 0);
    lv_tick_inc(64);
    lv_timer_handler();
    int returning = recycled_cases_pull(list, ctx);
    assert(returning > 0 && returning < large);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 0);
    assert(recycled_cases_pull(list, ctx) == returning && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 100, 20);
    assert(recycled_cases_pull(list, ctx) == returning);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 120, 20);
    assert(recycled_cases_pull(list, ctx) >= returning);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 120, 0);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);

    for (unsigned cancel = 0; cancel < 4; ++cancel) {
        lv_recycled_list_refresh(list, 20, true);
        recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
        recycled_cases_input(list, indev, LV_EVENT_PRESSING, 160, 20);
        assert(recycled_cases_pull(list, ctx) > 0);
        if (cancel == 0) recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 160, 0);
        else if (cancel == 1) lv_recycled_list_stop(list);
        else if (cancel == 2) lv_recycled_list_set_paged(list, true);
        else lv_recycled_list_refresh(list, 20, true);
        assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
        recycled_cases_input(list, indev, LV_EVENT_PRESSING, 200, 20);
        assert(recycled_cases_pull(list, ctx) == 0);
        lv_recycled_list_set_paged(list, false);
    }
    const unsigned small_counts[] = {0, 3, 7};
    for (unsigned i = 0; i < 3; ++i) {
        lv_recycled_list_refresh(list, small_counts[i], true);
        assert(lv_obj_has_flag(recycled_cases_thumb(list, ctx), LV_OBJ_FLAG_HIDDEN));
        recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
        recycled_cases_input(list, indev, LV_EVENT_PRESSING, 280, 20);
        assert(recycled_cases_pull(list, ctx) == 0);
        recycled_cases_input(list, indev, LV_EVENT_PRESSING, 20, 20);
        recycled_cases_input(list, indev, LV_EVENT_RELEASED, 20, 0);
        assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
        recycled_cases_rows(list, ctx);
    }
}

static int recycled_cases_index_y(recycled_cases_context_t *ctx, uint32_t index)
{
    for (unsigned i = 0; i < ctx->created; ++i)
        if (ctx->index[i] == index && !lv_obj_has_flag(ctx->row[i], LV_OBJ_FLAG_HIDDEN))
            return lv_obj_get_y(ctx->row[i]);
    assert(!"expected visual anchor row disappeared");
    return 0;
}

static void recycled_cases_pull_resize(lv_recycled_list_t *list, recycled_cases_context_t *ctx,
                                       lv_indev_t *indev, lv_timer_t *motion)
{
    const ui_list_window_t *window = lv_recycled_list_window(list);
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 140, 20);
    int pull = recycled_cases_pull(list, ctx);
    lv_recycled_list_refresh(list, 30, false);
    assert(recycled_cases_pull(list, ctx) == pull && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 140, 20);
    assert(recycled_cases_pull(list, ctx) == pull);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 160, 20);
    assert(recycled_cases_pull(list, ctx) > pull); /* Late growth kept the press. */
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 160, 0);
    recycled_cases_settle(list, ctx, motion);

    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_position(list, ui_list_window_limit(window));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    uint32_t index = window->first + 3;
    recycled_cases_pull(list, ctx);
    int before = recycled_cases_index_y(ctx, index);
    lv_recycled_list_refresh(list, 30, false);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
    assert(abs(recycled_cases_index_y(ctx, index)-before) <= 1);
    float grown = window->offset;
    assert(grown > 468 && grown < ui_list_window_limit(window));
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    recycled_cases_offset(list, grown);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 40, 20);
    recycled_cases_offset(list, grown+20);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 40, 0);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);

    recycled_cases_position(list, ui_list_window_limit(window));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    pull = recycled_cases_pull(list, ctx);
    lv_recycled_list_refresh(list, 20, false);
    recycled_cases_offset(list, 468);
    assert(recycled_cases_pull(list, ctx) == pull && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    assert(recycled_cases_pull(list, ctx) == pull);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 60, 0);
    recycled_cases_settle(list, ctx, motion);

    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    lv_recycled_list_refresh(list, 3, false);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
    assert(lv_obj_has_flag(recycled_cases_thumb(list, ctx), LV_OBJ_FLAG_HIDDEN));
    lv_recycled_list_refresh(list, 20, false);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 60, 20);
    recycled_cases_offset(list, 0);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 20, 20);
    assert(window->offset > 0); /* Fit/shrink/grow never canceled the held pointer. */
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 20, 0);
    assert(recycled_cases_pull(list, ctx) == 0 && motion->paused);
}

static void recycled_cases_timer_failure(lv_obj_t *parent, lv_indev_t *indev)
{
    unsigned timers = recycled_cases_timer_count(), failures = recycled_cases_failed_timers;
    recycled_cases_context_t ctx = {0};
    lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
    recycled_cases_fail_next_timer = true;
    lv_recycled_list_t *list = lv_recycled_list_create(parent, &cfg);
    assert(list && !recycled_cases_fail_next_timer && recycled_cases_failed_timers == failures+1);
    assert(!recycled_cases_owned_timer(list) && recycled_cases_timer_count() == timers);
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_offset(list, 100);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 100, 0);
    lv_tick_inc(80);
    lv_timer_handler();
    recycled_cases_offset(list, 100);
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 180, 20);
    assert(recycled_cases_pull(list, &ctx) > 0);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 180, 0);
    assert(recycled_cases_pull(list, &ctx) == 0);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 180, 20);
    assert(recycled_cases_pull(list, &ctx) > 0);
    lv_recycled_list_stop(list);
    assert(recycled_cases_pull(list, &ctx) == 0);
    assert(lv_recycled_list_scroll_to_index(list, 5));
    recycled_cases_offset(list, 180);
    recycled_cases_rows(list, &ctx);
    assert(!recycled_cases_owned_timer(list));
    lv_obj_del(lv_recycled_list_object(list));
    assert(ctx.deleted == ctx.created && recycled_cases_timer_count() == timers);
}

static void recycled_cases_short_viewport(lv_obj_t *parent, lv_indev_t *indev)
{
    unsigned timers = recycled_cases_timer_count();
    recycled_cases_context_t ctx = {0};
    lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
    cfg.rows = 2;
    lv_recycled_list_t *list = lv_recycled_list_create(parent, &cfg);
    assert(list);
    lv_recycled_list_refresh(list, 20, true);
    const ui_list_window_t *window = lv_recycled_list_window(list);
    lv_timer_t *motion = recycled_cases_owned_timer(list);
    assert(motion);
    recycled_cases_position(list, ui_list_window_limit(window)-70);
    /* Real input arrives after layout. Unlike the long-lived fixture above,
     * this newly created viewport has not passed through rows()/pull() yet. */
    lv_obj_t *viewport = lv_recycled_list_object(list);
    lv_obj_update_layout(viewport);
    assert(lv_obj_get_width(viewport) == 320 && lv_obj_get_height(viewport) == 72);
    assert(lv_obj_is_visible(viewport));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 60, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 40, 8);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 20, 8);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 0, 8);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 0, 0);
    recycled_cases_offset(list, ui_list_window_limit(window)-10);
    assert(!motion->paused);
    /* Regrab immediately after collision, while an uncapped outward spring
     * would hit its 15.84px asymptote and produce a huge inverse drag anchor. */
    for (unsigned i = 0; i < 2; ++i) {
        lv_tick_inc(16);
        lv_timer_handler();
        assert(abs(recycled_cases_pull(list, &ctx)) <= 16);
    }
    int returning = recycled_cases_pull(list, &ctx);
    assert(returning < 0);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 36, 0);
    assert(recycled_cases_pull(list, &ctx) == returning && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 236, 20);
    assert(window->offset < ui_list_window_limit(window));
    assert(recycled_cases_pull(list, &ctx) == 0);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 236, 0);
    lv_obj_del(lv_recycled_list_object(list));
    assert(ctx.deleted == ctx.created && recycled_cases_timer_count() == timers);
}

static void recycled_cases_creation_failures(lv_obj_t *parent)
{
    /* Warm the parent's retained spec_attr before comparing allocation totals. */
    lv_obj_t *warm = lv_obj_create(parent);
    assert(warm);
    lv_obj_del(warm);
    lv_obj_update_layout(parent);
    unsigned children = lv_obj_get_child_cnt(parent);
    unsigned timer_count = recycled_cases_timer_count();
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    for (unsigned partial = 0; partial < 2; ++partial) {
        for (unsigned fail_at = 1; fail_at <= 8; ++fail_at) {
            recycled_cases_context_t ctx = {0};
            ctx.fail_at = fail_at;
            ctx.partial_failure = partial != 0;
            lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
            assert(lv_recycled_list_create(parent, &cfg) == NULL);
            assert(ctx.calls == fail_at);
            assert(ctx.created == fail_at - 1U + partial);
            assert(ctx.deleted == ctx.created);
            assert(ctx.binds == 0 && ctx.changes == 0);
            assert(lv_obj_get_child_cnt(parent) == children);
            assert(recycled_cases_timer_count() == timer_count);
            lv_mem_monitor(&after);
            assert(after.free_size == before.free_size);
        }
    }
    recycled_cases_context_t ctx = {0};
    lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
    assert(lv_recycled_list_create(NULL, &cfg) == NULL);
    assert(lv_recycled_list_create(parent, NULL) == NULL);
    cfg.row_height = (uint16_t)(LV_COORD_MAX / 8 + 1);
    assert(lv_recycled_list_create(parent, &cfg) == NULL);
    assert(ctx.calls == 0 && recycled_cases_timer_count() == timer_count);
}

static void recycled_cases_scroll_to_index(lv_recycled_list_t *list,
                                           recycled_cases_context_t *ctx,
                                           lv_indev_t *indev, lv_timer_t *motion)
{
    const ui_list_window_t *window = lv_recycled_list_window(list);
    lv_obj_t *thumb = recycled_cases_thumb(list, ctx);
    unsigned created = ctx->created;
    assert(!lv_recycled_list_scroll_to_index(NULL, 0));
    lv_recycled_list_set_paged(list, false);
    lv_recycled_list_refresh(list, 0, true);
    unsigned changes = ctx->changes, binds = ctx->binds;
    assert(!lv_recycled_list_scroll_to_index(list, 0));
    assert(!lv_recycled_list_scroll_to_index(list, UINT32_MAX));
    assert(ctx->changes == changes && ctx->binds == binds && motion->paused);
    recycled_cases_offset(list, 0);
    recycled_cases_rows(list, ctx);

    lv_recycled_list_refresh(list, 3, true);
    assert(lv_recycled_list_scroll_to_index(list, 2));
    recycled_cases_offset(list, 0); /* A fitting list has no artificial blank tail. */
    assert(lv_obj_has_flag(thumb, LV_OBJ_FLAG_HIDDEN));
    recycled_cases_rows(list, ctx);

    lv_recycled_list_refresh(list, 20, true);
    const uint32_t indices[] = {0, 6, 19, 2, 10, 0, 19, 5, 5};
    for (unsigned i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
        uint32_t first = indices[i] < 13 ? indices[i] : 13;
        changes = ctx->changes;
        binds = ctx->binds;
        assert(lv_recycled_list_scroll_to_index(list, indices[i]));
        assert(ctx->changes == changes + 1 && motion->paused);
        assert(ctx->binds == binds + (20 - first < 8 ? 20 - first : 8));
        assert(window->first == first && window->count == 20 && !window->paged);
        assert(ui_list_window_last(window) == first + 7);
        recycled_cases_offset(list, first * 36.0f);
        recycled_cases_rows(list, ctx);
        assert(lv_obj_get_style_bg_opa(thumb, 0) ==
               (first == 0 || first == 13 ? LV_OPA_COVER : LV_OPA_40));
        int expected_y = (int)((252 - lv_obj_get_height(thumb)) * window->offset / 468);
        assert(lv_obj_get_y(thumb) == expected_y);
    }
    changes = ctx->changes;
    binds = ctx->binds;
    assert(!lv_recycled_list_scroll_to_index(list, 20));
    assert(!lv_recycled_list_scroll_to_index(list, UINT32_MAX));
    assert(ctx->changes == changes && ctx->binds == binds);
    recycled_cases_offset(list, 180);

    /* Invalid input does not steal a press; valid input cancels its remaining
     * PRESSING/RELEASED events and prevents a stale drag anchor or fling. */
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 160, 20);
    assert(!lv_recycled_list_scroll_to_index(list, 20));
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 140, 20);
    recycled_cases_offset(list, 240);
    assert(lv_recycled_list_scroll_to_index(list, 3));
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 100, 0);
    recycled_cases_offset(list, 108);
    assert(motion->paused && recycled_cases_pull(list, ctx) == 0);

    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 100, 0);
    assert(!motion->paused);
    assert(!lv_recycled_list_scroll_to_index(list, 20) && !motion->paused);
    assert(lv_recycled_list_scroll_to_index(list, 5));
    assert(motion->paused);
    lv_tick_inc(64);
    lv_timer_handler();
    recycled_cases_offset(list, 180);
    recycled_cases_rows(list, ctx);

    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 160, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 160, 0);
    assert(!motion->paused && recycled_cases_pull(list, ctx) > 0);
    assert(lv_recycled_list_scroll_to_index(list, 4));
    assert(motion->paused && recycled_cases_pull(list, ctx) == 0);
    lv_tick_inc(64);
    lv_timer_handler();
    recycled_cases_offset(list, 144);
    recycled_cases_rows(list, ctx);

    lv_recycled_list_set_paged(list, true);
    const uint32_t paged_indices[] = {0, 6, 7, 19, 7, 7};
    for (unsigned i = 0; i < sizeof(paged_indices) / sizeof(paged_indices[0]); ++i) {
        uint32_t first = paged_indices[i] / 7 * 7;
        changes = ctx->changes;
        assert(lv_recycled_list_scroll_to_index(list, paged_indices[i]));
        assert(window->paged && window->first == first && ctx->changes == changes + 1);
        assert(ui_list_window_page_number(window) == first / 7 + 1);
        recycled_cases_offset(list, first * 36.0f);
        assert(lv_obj_has_flag(thumb, LV_OBJ_FLAG_HIDDEN) && motion->paused);
        recycled_cases_rows(list, ctx);
    }
    assert(!lv_recycled_list_scroll_to_index(list, 20));
    assert(window->first == 7);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 200, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING, 100, 150, 200);
    assert(lv_recycled_list_scroll_to_index(list, 0));
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 20, 150, 0);
    assert(window->first == 0 && motion->paused);
    recycled_cases_swipe(list, indev, -70, 0, 200, true);
    assert(window->first == 7); /* A new gesture still uses normal page behavior. */

    lv_recycled_list_refresh(list, 10000, true);
    assert(lv_recycled_list_scroll_to_index(list, 9999));
    assert(window->first == 9996 && ui_list_window_page_number(window) == 1429);
    recycled_cases_rows(list, ctx);
    lv_recycled_list_set_paged(list, false);
    assert(lv_recycled_list_scroll_to_index(list, 9999));
    assert(window->first == 9993);
    recycled_cases_offset(list, 359748);
    recycled_cases_rows(list, ctx);
    assert(ctx->created == created); /* Seeking never expands the recycled pool. */
}

static void recycled_cases_hit_at(lv_recycled_list_t *list, int x, int y,
                                  uint32_t expected)
{
    lv_area_t area;
    lv_obj_get_coords(lv_recycled_list_object(list), &area);
    const lv_point_t point = {(lv_coord_t)(area.x1 + x), (lv_coord_t)(area.y1 + y)};
    uint32_t index = 1234567;
    bool found = lv_recycled_list_index_at_point(list, &point, &index);
    assert(found == (expected != UINT32_MAX));
    assert(index == (found ? expected : 1234567));
}

static void recycled_cases_hit_test(lv_recycled_list_t *list,
                                    recycled_cases_context_t *ctx,
                                    lv_indev_t *indev, lv_timer_t *motion)
{
    const lv_point_t point = {0, 0};
    uint32_t index = 1234567;
    assert(!lv_recycled_list_index_at_point(NULL, &point, &index));
    assert(!lv_recycled_list_index_at_point(list, NULL, &index));
    assert(!lv_recycled_list_index_at_point(list, &point, NULL));
    assert(index == 1234567);
    lv_recycled_list_set_paged(list, false);
    lv_recycled_list_refresh(list, 0, true);
    recycled_cases_hit_at(list, 10, 10, UINT32_MAX);
    lv_recycled_list_refresh(list, 3, true);
    recycled_cases_hit_at(list, 10, 0, 0);
    recycled_cases_hit_at(list, 10, 107, 2);
    recycled_cases_hit_at(list, 10, 108, UINT32_MAX);

    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_position(list, 18.5f);
    /* No rows()/pull() layout flush first: hit-test must see newly moved rows. */
    recycled_cases_hit_at(list, 10, 0, 0);
    recycled_cases_hit_at(list, 10, 17, 0);
    recycled_cases_hit_at(list, 10, 18, 1);
    recycled_cases_hit_at(list, 10, 251, 7);
    recycled_cases_hit_at(list, 10, -1, UINT32_MAX);
    recycled_cases_hit_at(list, 10, 252, UINT32_MAX);
    recycled_cases_hit_at(list, -1, 10, UINT32_MAX);
    recycled_cases_hit_at(list, 295, 10, 0);
    recycled_cases_hit_at(list, 296, 10, UINT32_MAX); /* Scrollbar gutter. */
    recycled_cases_hit_at(list, 320, 10, UINT32_MAX);

    assert(lv_recycled_list_scroll_to_index(list, 0));
    lv_obj_add_flag(ctx->row[0], LV_OBJ_FLAG_HIDDEN);
    recycled_cases_hit_at(list, 10, 10, UINT32_MAX);
    lv_obj_clear_flag(ctx->row[0], LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lv_recycled_list_object(list), LV_OBJ_FLAG_HIDDEN);
    recycled_cases_hit_at(list, 10, 10, UINT32_MAX);
    lv_obj_clear_flag(lv_recycled_list_object(list), LV_OBJ_FLAG_HIDDEN);

    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 180, 20);
    int pull = recycled_cases_pull(list, ctx);
    assert(pull > 0 && pull < 36);
    unsigned binds = ctx->binds, changes = ctx->changes;
    recycled_cases_hit_at(list, 10, pull - 1, UINT32_MAX);
    recycled_cases_hit_at(list, 10, 36 + pull / 2, 0);
    assert(ctx->binds == binds && ctx->changes == changes && motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 180, 0);
    assert(!motion->paused);
    recycled_cases_hit_at(list, 10, 36 + pull / 2, 0);
    assert(!motion->paused); /* A read-only hit test never arrests the spring. */
    lv_tick_inc(32);
    lv_timer_handler();
    pull = recycled_cases_pull(list, ctx);
    assert(pull > 0);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 0);
    assert(motion->paused && recycled_cases_pull(list, ctx) == pull);
    recycled_cases_hit_at(list, 10, pull - 1, UINT32_MAX);
    recycled_cases_hit_at(list, 10, 36 + pull / 2, 0);
    assert(recycled_cases_pull(list, ctx) == pull);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 100, 0);

    assert(lv_recycled_list_scroll_to_index(list, 19));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 120, 20);
    pull = recycled_cases_pull(list, ctx);
    assert(pull < 0 && pull > -36);
    recycled_cases_hit_at(list, 10, 0, 13);
    recycled_cases_hit_at(list, 10, 36 + pull, 14);
    recycled_cases_hit_at(list, 10, 251 + pull, 19);
    recycled_cases_hit_at(list, 10, 252 + pull, UINT32_MAX);
    recycled_cases_hit_at(list, 10, 251, UINT32_MAX);
    recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 120, 0);

    lv_recycled_list_set_paged(list, true);
    assert(lv_recycled_list_scroll_to_index(list, 19));
    recycled_cases_hit_at(list, 10, 0, 14);
    recycled_cases_hit_at(list, 10, 215, 19);
    recycled_cases_hit_at(list, 10, 216, UINT32_MAX);
    recycled_cases_hit_at(list, 10, 251, UINT32_MAX);
    lv_recycled_list_set_paged(list, false);
}

static void recycled_cases_tap_eligibility(lv_recycled_list_t *list,
    recycled_cases_context_t *ctx, lv_indev_t *indev, lv_timer_t *motion)
{
    assert(!lv_recycled_list_tap_allowed(NULL));
    lv_recycled_list_set_paged(list, false);
    lv_recycled_list_refresh(list, 100, true);
    assert(!lv_recycled_list_tap_allowed(list));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
    assert(!lv_recycled_list_tap_allowed(list)); /* Not a completed tap yet. */
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
    assert(lv_recycled_list_tap_allowed(list));
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 0);
    assert(!lv_recycled_list_tap_allowed(list));

    recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 100, 150, 20);
    recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 105, 155, 20);
    assert(lv_recycled_list_tap_allowed(list));
    /* Crossing the drag threshold then returning to the original row is not
     * a tap, even when direction locking keeps the viewport stationary. */
    const int deltas[][2] = {{6, 0}, {0, 6}, {6, 6}, {40, 40}};
    for (unsigned i = 0; i < sizeof(deltas)/sizeof(deltas[0]); ++i) {
        lv_recycled_list_refresh(list, 100, true);
        recycled_cases_input_xy(list, indev, LV_EVENT_PRESSED, 100, 150, 20);
        recycled_cases_input_xy(list, indev, LV_EVENT_PRESSING,
            100 + deltas[i][0], 150 + deltas[i][1], 20);
        recycled_cases_input_xy(list, indev, LV_EVENT_RELEASED, 100, 150, 20);
        assert(!lv_recycled_list_tap_allowed(list));
    }

    lv_recycled_list_refresh(list, 100, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 100, 0);
    assert(!motion->paused && !lv_recycled_list_tap_allowed(list));
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
    assert(motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
    assert(!lv_recycled_list_tap_allowed(list)); /* Stop inertia, don't open. */
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
    assert(lv_recycled_list_tap_allowed(list));

    lv_recycled_list_refresh(list, 100, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, indev, LV_EVENT_PRESSING, 180, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 180, 0);
    assert(recycled_cases_pull(list, ctx) > 0 && !motion->paused);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
    assert(!lv_recycled_list_tap_allowed(list)); /* Same rule for edge return. */

    for (unsigned cancel = 0; cancel < 3; ++cancel) {
        lv_recycled_list_refresh(list, 100, true);
        recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
        if (cancel == 0) lv_recycled_list_refresh(list, 100, true);
        else if (cancel == 1) lv_recycled_list_stop(list);
        else recycled_cases_input(list, indev, LV_EVENT_PRESS_LOST, 150, 0);
        recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
        assert(!lv_recycled_list_tap_allowed(list));
    }
    lv_recycled_list_set_paged(list, true);
    recycled_cases_input(list, indev, LV_EVENT_PRESSED, 150, 20);
    recycled_cases_input(list, indev, LV_EVENT_RELEASED, 150, 20);
    assert(lv_recycled_list_tap_allowed(list));
    recycled_cases_swipe(list, indev, -100, 0, 200, true);
    assert(!lv_recycled_list_tap_allowed(list));
    lv_recycled_list_set_paged(list, false);
    puts("PASS recycled-list tap eligibility: inertia/edge regrab, threshold latch, cancellation and paging");
}

static void test_recycled_list_cases(void)
{
    assert(sizeof(lv_coord_t) == 2); /* The firmware risk being tested is coord16. */
    unsigned baseline_timers = recycled_cases_timer_count();
    lv_obj_t *parent = lv_obj_create(lv_scr_act());
    assert(parent);
    lv_obj_remove_style_all(parent);
    lv_obj_set_size(parent, 400, 300);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    recycled_cases_creation_failures(parent);

    recycled_cases_context_t ctx = {0};
    lv_recycled_list_config_t cfg = recycled_cases_config(&ctx);
    lv_recycled_list_t *list = lv_recycled_list_create(parent, &cfg);
    assert(list && ctx.created == 8);
    lv_timer_t *motion = recycled_cases_owned_timer(list);
    assert(motion && motion->paused);
    assert(recycled_cases_timer_count() == baseline_timers + 1);
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_rows(list, &ctx);

    lv_indev_drv_t driver;
    lv_indev_drv_init(&driver);
    driver.type = LV_INDEV_TYPE_POINTER;
    driver.disp = lv_disp_get_default();
    lv_indev_t indev = {0};
    indev.driver = &driver;
    recycled_cases_input(list, &indev, LV_EVENT_PRESSED, 250, 20);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 246, 20);
    recycled_cases_offset(list, 0); /* Preserve the six-pixel drag threshold. */
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 150, 20);
    recycled_cases_offset(list, 100);
    unsigned binds = ctx.binds;
    lv_recycled_list_refresh(list, 25, false);
    assert(ctx.binds == binds + 8); /* Same range still rebinds changed data. */
    lv_recycled_list_set_paged(list, false); /* No-op controls must not end the press. */
    lv_recycled_list_page_step(list, 1);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 120, 20);
    recycled_cases_offset(list, 130);
    lv_recycled_list_refresh(list, 9, false);
    recycled_cases_offset(list, 72);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 120, 20);
    recycled_cases_offset(list, 72);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 140, 20);
    recycled_cases_offset(list, 52); /* The clamped anchor still follows the finger. */
    recycled_cases_rows(list, &ctx);
    recycled_cases_input(list, &indev, LV_EVENT_RELEASED, 140, 0);
    assert(!motion->paused);
    lv_tick_inc(16);
    lv_timer_handler();
    /* Correctly rebasing the velocity sample prevents a false shrink fling. */
    recycled_cases_offset(list, 52.0f - 650.0f * (1.0f - expf(-7.5f * 0.016f)) / 7.5f);
    lv_recycled_list_stop(list);
    assert(motion->paused);
    float stopped = lv_recycled_list_window(list)->offset;
    lv_tick_inc(40);
    lv_timer_handler();
    recycled_cases_offset(list, stopped);

    recycled_cases_input(list, &indev, LV_EVENT_PRESSED, 200, 20);
    lv_recycled_list_refresh(list, 20, true);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_offset(list, 0); /* A new result, unlike a late update, stops the press. */
    assert(motion->paused);

    /* Only this fractional-edge fixture mutates the exposed model in a test;
     * production projection and row binding remain unchanged. */
    ui_list_window_t *window = (ui_list_window_t *)lv_recycled_list_window(list);
    const float edges[] = {35.9f, 36.0f};
    for (unsigned i = 0; i < 2; ++i) {
        ui_list_window_move(window, edges[i]);
        lv_recycled_list_refresh(list, 20, false);
        assert(window->first == i);
        recycled_cases_rows(list, &ctx);
    }
    lv_recycled_list_refresh(list, 10000, true);
    const float large_offsets[] = {0, 35.9f, 9876.25f, 359747.5f, 359748.0f};
    for (unsigned i = 0; i < sizeof(large_offsets) / sizeof(large_offsets[0]); ++i) {
        ui_list_window_move(window, large_offsets[i]);
        lv_recycled_list_refresh(list, 10000, false);
        recycled_cases_rows(list, &ctx);
    }
    lv_recycled_list_set_paged(list, true);
    lv_recycled_list_page_step(list, 100000);
    assert(window->first == 9996 && ui_list_window_page_number(window) == 1429);
    recycled_cases_rows(list, &ctx);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 100, 20);
    assert(window->first == 9996 && motion->paused);
    lv_recycled_list_set_paged(list, false);
    assert(window->first == 9993);
    recycled_cases_rows(list, &ctx);

    recycled_cases_paging(list, &ctx, &indev, motion);
    recycled_cases_page_thresholds(parent, &indev);
    recycled_cases_scrollbar(list, &ctx, &indev, motion);
    recycled_cases_rubber(list, &ctx, &indev, motion);
    recycled_cases_pull_resize(list, &ctx, &indev, motion);
    recycled_cases_scroll_to_index(list, &ctx, &indev, motion);
    recycled_cases_hit_test(list, &ctx, &indev, motion);
    recycled_cases_tap_eligibility(list, &ctx, &indev, motion);
    recycled_cases_timer_failure(parent, &indev);
    recycled_cases_short_viewport(parent, &indev);

    /* A bubbled child DELETE must not free the viewport's owner or timer. */
    lv_obj_t *child = lv_obj_create(lv_recycled_list_object(list));
    assert(child);
    lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_del(child);
    assert(recycled_cases_owned_timer(list) == motion);
    lv_recycled_list_refresh(list, 10000, true);
    recycled_cases_rows(list, &ctx);

    /* Delete the viewport itself while inertia is active, then run real timers. */
    recycled_cases_input(list, &indev, LV_EVENT_PRESSED, 200, 20);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 100, 20);
    recycled_cases_input(list, &indev, LV_EVENT_RELEASED, 100, 0);
    assert(!motion->paused);
    lv_obj_del(lv_recycled_list_object(list));
    assert(ctx.deleted == ctx.created && lv_obj_get_child_cnt(parent) == 0);
    assert(recycled_cases_timer_count() == baseline_timers);
    lv_tick_inc(40);
    lv_timer_handler();

    memset(&ctx, 0, sizeof(ctx));
    list = lv_recycled_list_create(parent, &cfg);
    assert(list);
    lv_recycled_list_refresh(list, 10000, true);
    motion = recycled_cases_owned_timer(list);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSED, 100, 20);
    recycled_cases_input(list, &indev, LV_EVENT_PRESSING, 220, 20);
    recycled_cases_input(list, &indev, LV_EVENT_RELEASED, 220, 0);
    assert(motion && !motion->paused && recycled_cases_pull(list, &ctx) > 0);
    lv_obj_del(parent); /* Ancestor ownership must take the same cleanup path. */
    assert(ctx.deleted == ctx.created);
    assert(recycled_cases_timer_count() == baseline_timers);
    lv_tick_inc(40);
    lv_timer_handler();
    puts("PASS actual LVGL recycled-list XY paging, rubber return/regrab, endpoint thumb opacity, late resize, timer failure, floor edges, coord16 and cleanup");
}

#endif
