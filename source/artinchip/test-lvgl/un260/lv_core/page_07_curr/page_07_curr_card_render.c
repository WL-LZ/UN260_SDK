#include "un260/lv_core/page_07_curr/page_07_curr_card_render.h"
#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"
#include "un260/lv_core/page_07_curr/page_07_curr_layout.h"
#include "un260/lv_core/page_07_curr/page_07_curr_view.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/lv_components/lv_card_surface.h"
#include "un260/currency/currency_state.h"

#include <stdio.h>

/* Private Currency renderer: geometry, cached faces and live fallback share
 * the page-owned card records. No timers, input handling, persistence or
 * currency commands live here; the page decides when idle work may run. */

static void curr_set_card_render_state(int i, int pos_x, int pos_y,
                                       bool focused)
{
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    if (card->render_root == NULL) return;
    lv_obj_set_pos(card->render_root, pos_x, pos_y);
    if (card->render_initialized && card->render_focused == focused) return;

    const lv_card_surface_style_t skin = {
        .width = CURR_CARD_W, .height = CURR_CARD_H,
        .radius = CURR_CARD_RADIUS,
        .background = CURR_CARD_BG,
        .border = focused ? CURR_CARD_FOCUS_BORDER : CURR_CARD_NORMAL_BORDER,
        .border_width = 1,
    };
    if (!card->render_initialized) {
        lv_card_surface_apply(card->render_root, &skin);
        lv_obj_set_style_text_color(card->name, lv_color_hex(0x283740), 0);
        lv_obj_set_style_text_color(card->no, lv_color_hex(0x7E91A1), 0);
        page07_curr_view_set_image_selected_style(card->img);
    } else {
        /* Focus changes only the border, never the face brightness. */
        lv_obj_set_style_border_color(card->render_root, lv_color_hex(skin.border), 0);
    }
    card->render_initialized = true;
    card->render_focused = focused;
}

static bool curr_card_snapshot_key(int i, bool focused, char key[48])
{
    char curr_code[4];
    if (i < 0 || i >= g_page07_curr.model.visible_count || key == NULL ||
        !currency_state_get_code((uint8_t)g_page07_curr.cards[i].abs_idx,
                                 curr_code)) return false;
    /* The code, displayed sequence and renderer revision define identity.
     * Old enlarged SELECTED/NORMAL surfaces cannot match this generation. */
    snprintf(key, 48, "CURR_CAROUSEL_V7_%s_%02d_%c", curr_code,
             g_page07_curr.cards[i].abs_idx + 1, focused ? 'F' : 'N');
    return true;
}

static bool curr_acquire_card_snapshot(int i, bool focused, bool create_on_miss)
{
    page07_curr_card_t *card;
    char cache_key[48];
    if (i < 0 || i >= g_page07_curr.model.visible_count) return false;
    card = &g_page07_curr.cards[i];
    if (card->has_scaled_flag || card->render_root == NULL ||
        !curr_card_snapshot_key(i, focused, cache_key)) return false;
    unsigned state = focused ? 1U : 0U;
    if (card->surface_cache[state] != NULL) return true;
    if (create_on_miss) {
        curr_set_card_render_state(i, card->base_x, card->drawn_y, focused);
        card->surface_cache[state] = lv_dma_snapshot_cache_acquire_or_create(
            card->render_root, cache_key);
    } else {
        card->surface_cache[state] = lv_dma_snapshot_cache_acquire(cache_key);
    }
    return card->surface_cache[state] != NULL;
}

static bool curr_card_cache_wanted(int i, bool focused, int focus)
{
    /* Pin at most seven normal + three focus faces (about 2 MiB), not
     * two full catalogs. Old faces remain evictable in the global LRU. */
    return focused ? (i >= focus - 1 && i <= focus + 1)
                   : (i >= focus - 2 && i <= focus + 4);
}

bool page07_curr_card_render_sync_snapshots(int i, int focus)
{
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    bool changed = false;
    for (unsigned state = 0; state < 2; state++) {
        lv_dma_snapshot_t *before = card->surface_cache[state];
        if (!card->has_scaled_flag && curr_card_cache_wanted(i, state != 0, focus)) {
            (void)curr_acquire_card_snapshot(i, state != 0, false);
        } else if (before != NULL) {
            /* Detach before making this image evictable; another cache
             * owner can reclaim it at the next idle allocation. */
            if (card->composite != NULL &&
                lv_img_get_src(card->composite) == lv_dma_snapshot_image(before)) {
                lv_obj_add_flag(card->composite, LV_OBJ_FLAG_HIDDEN);
                lv_img_set_src(card->composite, NULL);
                card->using_cache = false;
            }
            lv_dma_snapshot_cache_release(before);
            card->surface_cache[state] = NULL;
        }
        changed |= before != card->surface_cache[state];
    }
    return changed;
}

bool page07_curr_card_render_prewarm_step(void)
{
    int focus = (int)ui_scroll_physics_nearest(&g_page07_curr.carousel.motion);
    /* Current highlight first, then nearby normal/focus states. This is an
     * idle-only producer; cache misses while dragging use the live tree. */
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
            page07_curr_card_t *card = &g_page07_curr.cards[i];
            bool focused = pass != 1;
            if (card->has_scaled_flag || (pass == 0 && i != focus) ||
                !curr_card_cache_wanted(i, focused, focus) ||
                card->render_root == NULL ||
                card->surface_cache[focused ? 1 : 0] != NULL) continue;
            bool created = curr_acquire_card_snapshot(i, focused, true);
            page07_curr_card_render_apply(i, card->base_x, card->drawn_y);
            return created;
        }
    }
    return false;
}

void page07_curr_card_render_release_snapshots(void)
{
    for (int i = 0; i < PAGE07_CURR_MAX_ITEMS; i++) {
        for (unsigned state = 0; state < 2; state++) {
            lv_dma_snapshot_cache_release(g_page07_curr.cards[i].surface_cache[state]);
            g_page07_curr.cards[i].surface_cache[state] = NULL;
        }
    }
}

void page07_curr_card_render_apply(int i, int pos_x, int pos_y)
{
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    lv_dma_snapshot_t *snapshot = card->surface_cache[card->focused ? 1 : 0];
    /* Only the static face is captured. The live flag/selection/favorite
     * children of card stay visible regardless of cache hit or fallback. */
    if (!card->has_scaled_flag && snapshot != NULL && card->composite != NULL) {
        const lv_img_dsc_t *image = lv_dma_snapshot_image(snapshot);
        lv_obj_add_flag(card->render_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card->composite, LV_OBJ_FLAG_HIDDEN);
        if (lv_img_get_src(card->composite) != image)
            lv_img_set_src(card->composite, image);
        lv_obj_set_pos(card->composite, pos_x, pos_y);
        card->using_cache = true;
    } else {
        if (card->composite != NULL) lv_obj_add_flag(card->composite, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(card->render_root, LV_OBJ_FLAG_HIDDEN);
        curr_set_card_render_state(i, pos_x, pos_y, card->focused);
        card->using_cache = false;
    }
    lv_obj_set_pos(card->card, pos_x, pos_y);
    card->drawn_y = pos_y;
}
