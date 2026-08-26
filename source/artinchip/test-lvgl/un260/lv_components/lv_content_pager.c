#include "lv_content_pager.h"

#include <string.h>

#define CONTENT_PAGER_MAX_CONTEXTS 8
#define CONTENT_PAGER_MAX_PAGES    8

typedef struct {
    lv_obj_t *root;
    lv_obj_t *viewport;
    lv_obj_t *pages[CONTENT_PAGER_MAX_PAGES];
    lv_obj_t *dots[CONTENT_PAGER_MAX_PAGES];
    uint8_t count;
    uint8_t active;
    lv_coord_t page_width;
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
        lv_obj_set_style_bg_color(context->dots[i],
            lv_color_hex(i == context->active ? 0xFFFFFF : 0x7F8992), 0);
    }
}

static void pager_scroll_end_cb(lv_event_t *event)
{
    content_pager_context_t *context = lv_event_get_user_data(event);
    lv_coord_t x;
    uint8_t index;
    if (context == NULL || context->page_width <= 0) return;
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
    if (parent == NULL || page_count == 0 || page_count > CONTENT_PAGER_MAX_PAGES) return NULL;
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
        lv_obj_remove_style_all(indicator);
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

bool lv_content_pager_set_active(lv_obj_t *pager, uint8_t index, bool animated)
{
    content_pager_context_t *context = pager_find(pager);
    if (context == NULL || index >= context->count) return false;
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
