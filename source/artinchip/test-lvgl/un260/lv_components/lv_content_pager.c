#include "lv_content_pager.h"

#include <stdlib.h>
#include <string.h>

#define CONTENT_PAGER_MAX_CONTEXTS 8
#define CONTENT_PAGER_MAX_PAGES    8

typedef struct {
    lv_obj_t *root;
    lv_obj_t *viewport;
    lv_obj_t *indicator;
    lv_obj_t *pages[CONTENT_PAGER_MAX_PAGES];
    lv_obj_t *dots[CONTENT_PAGER_MAX_PAGES];
    uint8_t count;
    uint8_t active;
    lv_coord_t page_width;
    bool circular, dragging, settling;
    uint8_t neighbor, target, requested;
    bool has_requested;
    int direction;
    int32_t offset, drag_origin, sample_displacement, velocity;
    uint32_t sample_tick, motion_tick;
} content_pager_context_t;

static content_pager_context_t *g_contexts[CONTENT_PAGER_MAX_CONTEXTS];

static content_pager_context_t *pager_find(lv_obj_t *root)
{
    int i;
    for (i = 0; i < CONTENT_PAGER_MAX_CONTEXTS; i++) {
        if (g_contexts[i] != NULL && g_contexts[i]->root == root) return g_contexts[i];
    }
    return NULL;
}

static void pager_update_dots(content_pager_context_t *context)
{
    int i;
    for (i = 0; i < context->count; i++) {
        if (context->dots[i] == NULL) continue;
        lv_obj_set_style_bg_color(context->dots[i],
            lv_color_hex(i == context->active ? 0xFFFFFF : 0x7F8992), 0);
    }
}

static uint8_t pager_neighbor(content_pager_context_t *context, int direction)
{
    return (uint8_t)((context->active + context->count + direction) % context->count);
}

static void pager_stop(content_pager_context_t *context)
{
    lv_anim_del(context, NULL);
    context->dragging = context->settling = false;
    context->velocity = 0;
}

static void pager_place(content_pager_context_t *context, int32_t offset)
{
    int direction = offset < 0 ? 1 : offset > 0 ? -1 : context->direction;
    if (direction != context->direction) {
        context->direction = direction;
        context->neighbor = pager_neighbor(context, direction);
    }
    context->offset = offset;
    for (int i = 0; i < context->count; i++) {
        int32_t x = context->page_width;
        if (i == context->active) x = offset;
        else if (i == context->neighbor)
            x = offset + direction * context->page_width;
        lv_obj_set_pos(context->pages[i], (lv_coord_t)x, 0);
    }
}

static void pager_center(content_pager_context_t *context, uint8_t active)
{
    pager_stop(context);
    context->has_requested = false;
    context->active = context->target = active;
    context->direction = 1;
    context->neighbor = pager_neighbor(context, 1);
    pager_place(context, 0);
    pager_update_dots(context);
}

static void pager_offset_exec(void *data, int32_t offset)
{
    pager_place(data, offset);
}

static void pager_offset_ready(lv_anim_t *animation)
{
    content_pager_context_t *context = animation->var;
    /* The target already occupies x=0 at the final animation frame. Swapping
     * its logical ownership and recentering leaves those pixels unchanged. */
    context->active = context->target;
    context->dragging = context->settling = false;
    context->direction = 1;
    context->neighbor = pager_neighbor(context, 1);
    pager_place(context, 0);
    pager_update_dots(context);
    if (context->has_requested) {
        uint8_t requested = context->requested;
        context->has_requested = false;
        lv_content_pager_set_active(context->root, requested, true);
    }
}

static void pager_settle(content_pager_context_t *context, int32_t destination, uint8_t target)
{
    pager_stop(context);
    context->target = target;
    if (destination == context->offset) {
        pager_center(context, target);
        return;
    }
    context->settling = true;
    lv_obj_update_layout(context->root);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, context);
    lv_anim_set_exec_cb(&animation, pager_offset_exec);
    lv_anim_set_values(&animation, context->offset, destination);
    uint32_t distance = (uint32_t)abs(destination - context->offset);
    uint32_t duration = 200 + 60 * distance / context->page_width;
    if (duration > 260) duration = 260;
    lv_anim_set_time(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&animation, pager_offset_ready);
    if (!lv_anim_start(&animation)) pager_center(context, target);
}

/* Stay within one page width: with two real pages an overshoot would expose a
 * gap. The last tenth gains progressive resistance without a hard boundary. */
static int32_t pager_resisted_offset(content_pager_context_t *context, int64_t displacement)
{
    int64_t magnitude = displacement < 0 ? -displacement : displacement;
    int64_t width = context->page_width;
    int64_t start = width * 9 / 10;
    if (magnitude > start) {
        int64_t range = width - start;
        int64_t excess = magnitude - start;
        magnitude = start + range * excess / (range + excess);
    }
    return (int32_t)(displacement < 0 ? -magnitude : magnitude);
}

static int32_t pager_unresisted_offset(content_pager_context_t *context)
{
    int64_t magnitude = abs(context->offset);
    int64_t width = context->page_width;
    int64_t start = width * 9 / 10;
    if (magnitude > start) {
        int64_t range = width - start;
        int64_t remaining = width - magnitude;
        magnitude = start + (range * (magnitude - start) + remaining - 1) / remaining;
    }
    return (int32_t)(context->offset < 0 ? -magnitude : magnitude);
}

static void pager_scroll_end_cb(lv_event_t *event)
{
    content_pager_context_t *context = lv_event_get_user_data(event);
    lv_coord_t x;
    uint8_t index;
    if (context == NULL || context->page_width <= 0 || context->circular) return;
    x = lv_obj_get_scroll_x(context->viewport);
    if (x < 0) x = 0;
    index = (uint8_t)((x + context->page_width / 2) / context->page_width);
    if (index >= context->count) index = context->count - 1;
    context->active = index;
    pager_update_dots(context);
}

static void pager_delete_cb(lv_event_t *event)
{
    content_pager_context_t *context = lv_event_get_user_data(event);
    int i;
    if (context == NULL) return;
    pager_stop(context);
    for (i = 0; i < CONTENT_PAGER_MAX_CONTEXTS; i++) {
        if (g_contexts[i] == context) g_contexts[i] = NULL;
    }
    lv_mem_free(context);
}

lv_obj_t *lv_content_pager_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                  lv_coord_t width, lv_coord_t height,
                                  uint8_t page_count)
{
    content_pager_context_t *context;
    lv_obj_t *indicator;
    int slot = -1;
    int i;
    if (parent == NULL || width <= 0 || page_count == 0 || page_count > CONTENT_PAGER_MAX_PAGES) return NULL;
    for (i = 0; i < CONTENT_PAGER_MAX_CONTEXTS; i++) if (g_contexts[i] == NULL) { slot = i; break; }
    if (slot < 0) return NULL;
    context = lv_mem_alloc(sizeof(*context));
    if (context == NULL) return NULL;
    memset(context, 0, sizeof(*context));
    context->count = page_count;
    context->page_width = width;

    context->root = lv_obj_create(parent);
    lv_obj_remove_style_all(context->root);
    lv_obj_set_pos(context->root, x, y);
    lv_obj_set_size(context->root, width, height);
    lv_obj_clear_flag(context->root, LV_OBJ_FLAG_SCROLLABLE);
    context->viewport = lv_obj_create(context->root);
    lv_obj_remove_style_all(context->viewport);
    lv_obj_set_size(context->viewport, width, height - (page_count > 1 ? 18 : 0));
    lv_obj_set_flex_flow(context->viewport, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(context->viewport, 0, 0);
    lv_obj_set_style_pad_column(context->viewport, 0, 0);
    lv_obj_set_scroll_dir(context->viewport, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(context->viewport, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(context->viewport, LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scrollbar_mode(context->viewport, LV_SCROLLBAR_MODE_OFF);
    for (i = 0; i < page_count; i++) {
        context->pages[i] = lv_obj_create(context->viewport);
        lv_obj_remove_style_all(context->pages[i]);
        lv_obj_set_size(context->pages[i], width, height - (page_count > 1 ? 18 : 0));
        lv_obj_set_style_bg_opa(context->pages[i], LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(context->pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    if (page_count > 1) {
        indicator = lv_obj_create(context->root);
        context->indicator=indicator;
        lv_obj_remove_style_all(indicator);
        lv_obj_clear_flag(indicator,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(indicator, 20 + page_count * 14, 16);
        lv_obj_align(indicator, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_bg_color(indicator, lv_color_hex(0x34414B), 0);
        lv_obj_set_style_bg_opa(indicator, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(indicator, 8, 0);
        lv_obj_set_flex_flow(indicator, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(indicator, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(indicator, 7, 0);
        for (i = 0; i < page_count; i++) {
            context->dots[i] = lv_obj_create(indicator);
            lv_obj_remove_style_all(context->dots[i]);
            lv_obj_clear_flag(context->dots[i],LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(context->dots[i], 6, 6);
            lv_obj_set_style_radius(context->dots[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(context->dots[i], LV_OPA_COVER, 0);
        }
        pager_update_dots(context);
    }
    g_contexts[slot] = context;
    lv_obj_add_event_cb(context->viewport, pager_scroll_end_cb, LV_EVENT_SCROLL_END, context);
    lv_obj_add_event_cb(context->root, pager_delete_cb, LV_EVENT_DELETE, context);
    return context->root;
}

lv_obj_t *lv_content_pager_get_page(lv_obj_t *pager, uint8_t index)
{
    content_pager_context_t *context = pager_find(pager);
    return context != NULL && index < context->count ? context->pages[index] : NULL;
}
lv_obj_t *lv_content_pager_get_viewport(lv_obj_t *pager)
{
    content_pager_context_t *context=pager_find(pager);
    return context?context->viewport:NULL;
}
void lv_content_pager_set_indicator_position(lv_obj_t *pager,lv_coord_t x,lv_coord_t y)
{
    content_pager_context_t *context=pager_find(pager);
    if(context && context->indicator){
        lv_obj_set_align(context->indicator,LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(context->indicator,x,y);
    }
}

bool lv_content_pager_set_active(lv_obj_t *pager, uint8_t index, bool animated)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || index >= context->count) return false;
    if (context->circular) {
        if (!animated) {
            pager_center(context, index);
            return true;
        }
        if (context->dragging || context->settling || context->offset != 0) {
            context->has_requested = false;
            if (context->settling && index == context->target) return true;
            if (index == context->active) {
                pager_settle(context, 0, context->active);
            } else if (index == context->neighbor) {
                pager_settle(context, -context->direction * context->page_width, index);
            } else {
                /* Only two pages are visible. Defer a third destination until
                 * the current handoff instead of snapping either page. */
                context->requested = index;
                context->has_requested = true;
                if (!context->settling)
                    pager_settle(context, -context->direction * context->page_width, context->neighbor);
            }
            return true;
        }
        if (index == context->active) return true;
        unsigned forward = (index + context->count - context->active) % context->count;
        context->direction = forward <= context->count - forward ? 1 : -1;
        context->neighbor = index;
        pager_settle(context, -context->direction * context->page_width, index);
        return true;
    }
    pager_stop(context);
    context->active = index;
    lv_obj_scroll_to_x(context->viewport, index * context->page_width,
                       animated ? LV_ANIM_ON : LV_ANIM_OFF);
    pager_update_dots(context);
    return true;
}

uint8_t lv_content_pager_get_active(lv_obj_t *pager)
{
    content_pager_context_t *context = pager_find(pager);
    return context != NULL ? context->active : 0;
}

uint8_t lv_content_pager_get_count(lv_obj_t *pager)
{
    content_pager_context_t *context = pager_find(pager);
    return context != NULL ? context->count : 0;
}

bool lv_content_pager_set_circular(lv_obj_t *pager, bool enabled)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || (enabled && context->count < 2)) return false;
    if (context->circular == enabled) return true;
    pager_stop(context);
    context->circular = enabled;
    if (enabled) {
        lv_obj_scroll_to_x(context->viewport, 0, LV_ANIM_OFF);
        lv_obj_set_layout(context->viewport, 0);
        lv_obj_clear_flag(context->viewport, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_snap_x(context->viewport, LV_SCROLL_SNAP_NONE);
        pager_center(context, context->active);
    } else {
        lv_obj_set_flex_flow(context->viewport, LV_FLEX_FLOW_ROW);
        lv_obj_add_flag(context->viewport, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_snap_x(context->viewport, LV_SCROLL_SNAP_CENTER);
        lv_obj_update_layout(context->root);
        lv_obj_scroll_to_x(context->viewport, context->active * context->page_width, LV_ANIM_OFF);
    }
    lv_obj_update_layout(context->root);
    return true;
}

bool lv_content_pager_drag_begin(lv_obj_t *pager)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || !context->circular) return false;
    if (abs(context->offset) >= context->page_width)
        pager_center(context, context->neighbor);
    pager_stop(context);
    context->has_requested = false;
    context->dragging = true;
    context->drag_origin = pager_unresisted_offset(context);
    context->sample_displacement = 0;
    context->sample_tick = context->motion_tick = lv_tick_get();
    return true;
}

bool lv_content_pager_drag_update(lv_obj_t *pager, int32_t displacement)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || !context->circular || !context->dragging) return false;
    uint32_t elapsed = lv_tick_elaps(context->sample_tick);
    int64_t delta = (int64_t)displacement - context->sample_displacement;
    if (delta != 0) context->motion_tick = lv_tick_get();
    if (elapsed != 0) {
        int64_t speed = delta * 1000 / elapsed;
        if (speed > 8000) speed = 8000;
        if (speed < -8000) speed = -8000;
        context->velocity = elapsed > 100 ? (int32_t)speed :
            (context->velocity + (int32_t)speed) / 2;
        context->sample_displacement = displacement;
        context->sample_tick = lv_tick_get();
    }
    int32_t offset = pager_resisted_offset(context, (int64_t)context->drag_origin + displacement);
    if (offset != context->offset) pager_place(context, offset);
    return true;
}

bool lv_content_pager_drag_end(lv_obj_t *pager, bool cancelled)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || !context->circular || !context->dragging) return false;
    int32_t magnitude = abs(context->offset);
    int32_t minimum_fling = context->page_width / 50;
    if (minimum_fling < 18) minimum_fling = 18;
    bool fling = lv_tick_elaps(context->motion_tick) <= 100 &&
        magnitude >= minimum_fling && abs(context->velocity) >= 600 &&
        ((context->velocity > 0) == (context->offset > 0));
    bool advance = !cancelled && (magnitude >= context->page_width / 5 || fling);
    int32_t destination = advance ? -context->direction * context->page_width : 0;
    pager_settle(context, destination, advance ? context->neighbor : context->active);
    return true;
}

bool lv_content_pager_is_moving(lv_obj_t *pager)
{
    content_pager_context_t *context = pager_find(pager);
    return context != NULL && context->circular &&
        (context->dragging || context->settling || context->offset != 0);
}

bool lv_content_pager_finish(lv_obj_t *pager)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || !context->circular) return false;
    bool changed = context->dragging || context->settling || context->offset != 0;
    if (!changed) return false;
    uint8_t target = context->settling ? context->target :
        abs(context->offset) >= context->page_width / 2 ? context->neighbor : context->active;
    pager_center(context, target);
    lv_obj_update_layout(context->root);
    return changed;
}
