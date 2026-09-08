#ifndef TEST_RECYCLED_LIST_CASES_H
#define TEST_RECYCLED_LIST_CASES_H

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
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

static void recycled_cases_input(lv_recycled_list_t *list, lv_indev_t *indev,
                                 lv_event_code_t code, lv_coord_t y, uint32_t elapsed)
{
    lv_tick_inc(elapsed);
    indev->proc.types.pointer.act_point.x = 100;
    indev->proc.types.pointer.act_point.y = y;
    indev->proc.state = code == LV_EVENT_RELEASED ? LV_INDEV_STATE_RELEASED : LV_INDEV_STATE_PRESSED;
    assert(lv_event_send(lv_recycled_list_object(list), code, indev) == LV_RES_OK);
}

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
    lv_obj_del(parent); /* Ancestor ownership must take the same cleanup path. */
    assert(ctx.deleted == ctx.created);
    assert(recycled_cases_timer_count() == baseline_timers);
    lv_tick_inc(40);
    lv_timer_handler();
    puts("PASS actual LVGL recycled-list drag, late resize, floor edges, coord16 and cleanup");
}

#endif
