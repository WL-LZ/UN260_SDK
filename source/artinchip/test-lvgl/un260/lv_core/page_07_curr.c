#include "un260/lv_core/page_07_curr.h"
#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"
#include "un260/lv_core/page_07_curr/page_07_curr_layout.h"
#include "un260/lv_core/page_07_curr/page_07_curr_card_render.h"
#include "un260/lv_core/page_07_curr/page_07_curr_view.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/lv_components/lv_components.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_resources/lv_image_declear.h"
#include "un260/lv_resources/lv_img_init.h"
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_service.h"
#include "un260/app_service/setting_service.h"
#include "un260/protocol/protocol_send.h"
#include "lv_page_event.h"
#include "aic_ui/aic_ui.h"
#include "lv_port_indev.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_core/page_07_curr/page_07_curr_star.h"

static lv_obj_t* curr_page = NULL;
static currency_state_snapshot_t g_curr_page_snapshot;
static bool g_curr_page_snapshot_valid;
static char g_curr_page_selected_code[4];
static lv_timer_t *g_curr_snapshot_prewarm_timer;

static void curr_snapshot_prewarm_timer_cb(lv_timer_t *timer);
static void curr_focus_confirmed_selection_on_entry(void);

ui_element_t page_07_curr_obj[] = {
    // 背景图
    {
        .obj_name = "page_07_bg.png",
        .obj_type = LV_OBJ_TYPE_IMAGE,
        .obj_item = { .x = 0, .y = 0, .w = 1280, .h = 400 },
        .obj_style = { .opacity = 255 },
    },

};

int page_07_curr_len = sizeof(page_07_curr_obj) / sizeof(page_07_curr_obj[0]);

void ui_page_07_curr_create(lv_obj_t* parent)
{
    (void)parent;
    if (curr_page) return;
    g_curr_page_snapshot_valid = false;
    memset(g_curr_page_selected_code, 0, sizeof(g_curr_page_selected_code));
    curr_page = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(curr_page);
    lv_obj_set_pos(curr_page, 0, 0);
    lv_obj_set_size(curr_page, 1280, 400);
    lv_obj_clear_flag(curr_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(curr_page, LV_SCROLLBAR_MODE_OFF);
    lv_ui_obj_init(curr_page, page_07_curr_obj, page_07_curr_len);
    page_07_curr_img_refre();
    curr_focus_confirmed_selection_on_entry();
    if (g_curr_snapshot_prewarm_timer == NULL) {
        g_curr_snapshot_prewarm_timer =
            lv_timer_create(curr_snapshot_prewarm_timer_cb, 120, NULL);
    }

};

void ui_page_07_curr_destroy(void)
{
    if (g_curr_snapshot_prewarm_timer != NULL) {
        lv_timer_del(g_curr_snapshot_prewarm_timer);
        g_curr_snapshot_prewarm_timer = NULL;
    }
    if (curr_page)
    {
        page_07_curr_img_reset();
        lv_obj_del(curr_page);
        curr_page = NULL;
    }
    g_curr_page_snapshot_valid = false;

}

page07_curr_context_t g_page07_curr = {
    .model.view_mode = PAGE07_CURR_VIEW_CARD,
};

static int g_curr_track_x = -1;
static int g_curr_track_w = -1;
static int g_curr_card_styled_visible_idx = -1;
static int g_curr_grid_styled_abs_idx = -1;
static int g_curr_cache_focus_idx = -1;

static void curr_refresh_right_views(void);
static void curr_apply_selected_style(void);
static void curr_project_carousel(void);
static void curr_style_back_button(void);

static void curr_update_track_by_scroll(void)
{
    int nearest = (int)ui_scroll_physics_nearest(&g_page07_curr.carousel.motion);
    if (g_page07_curr.objects.arrow_prev != NULL &&
        lv_obj_get_style_opa(g_page07_curr.objects.arrow_prev, 0) != (nearest <= 0 ? 153 : LV_OPA_COVER))
        lv_obj_set_style_opa(g_page07_curr.objects.arrow_prev,
                            nearest <= 0 ? 153 : LV_OPA_COVER, 0);
    if (g_page07_curr.objects.arrow_next != NULL &&
        lv_obj_get_style_opa(g_page07_curr.objects.arrow_next, 0) !=
            (nearest >= g_page07_curr.model.visible_count - 1 ? 153 : LV_OPA_COVER))
        lv_obj_set_style_opa(g_page07_curr.objects.arrow_next,
                            nearest >= g_page07_curr.model.visible_count - 1 ? 153 : LV_OPA_COVER, 0);
    if (g_page07_curr.objects.thumb == NULL || g_page07_curr.model.visible_count <= 0) return;

    int thumb_w = CURR_TRACK_W / g_page07_curr.model.visible_count;
    if (thumb_w < CURR_TRACK_MIN_THUMB) thumb_w = CURR_TRACK_MIN_THUMB;
    if (thumb_w > CURR_TRACK_W) thumb_w = CURR_TRACK_W;

    float max_scroll = g_page07_curr.carousel.motion.max_position;
    if (max_scroll <= 0 || g_page07_curr.model.visible_count <= 1) {
        if (g_curr_track_w != CURR_TRACK_W) {
            lv_obj_set_size(g_page07_curr.objects.thumb, CURR_TRACK_W, CURR_TRACK_H);
            g_curr_track_w = CURR_TRACK_W;
        }
        if (g_curr_track_x != CURR_TRACK_X) {
            lv_obj_set_pos(g_page07_curr.objects.thumb, CURR_TRACK_X, CURR_TRACK_Y);
            g_curr_track_x = CURR_TRACK_X;
        }
        return;
    }

    float position = g_page07_curr.carousel.motion.position;
    if (position < 0) position = 0;
    if (position > max_scroll) position = max_scroll;
    int x = CURR_TRACK_X +
        (int)(position * (CURR_TRACK_W - thumb_w) / max_scroll + 0.5f);

    if (g_curr_track_w != thumb_w) {
        lv_obj_set_size(g_page07_curr.objects.thumb, thumb_w, CURR_TRACK_H);
        g_curr_track_w = thumb_w;
    }
    if (g_curr_track_x != x) {
        lv_obj_set_pos(g_page07_curr.objects.thumb, x, CURR_TRACK_Y);
        g_curr_track_x = x;
    }
}

static void curr_set_left_info_by_abs(int abs_idx)
{
    char curr_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, curr_code)) return;
    lv_img_set_src(g_page07_curr.objects.left_img, get_currency_img(curr_code));
    page07_curr_view_set_img_target_width(g_page07_curr.objects.left_img, curr_code,
                              CURR_LEFT_FLAG_TARGET_W);
    lv_obj_align(g_page07_curr.objects.left_img, LV_ALIGN_TOP_MID, CURR_LEFT_IMG_ALIGN_X, CURR_LEFT_IMG_ALIGN_Y);
    lv_label_set_text(g_page07_curr.objects.left_code,
                      currency_state_display_code(curr_code));
    if (g_page07_curr.objects.left_code_decor) {
        lv_label_set_text(g_page07_curr.objects.left_code_decor,
                          currency_state_display_code(curr_code));
    }
    lv_label_set_text_fmt(g_page07_curr.objects.left_no, "NO. %02d", abs_idx + 1);
}

static void curr_style_view_button(void)
{
    if (g_page07_curr.objects.btn_view == NULL || g_page07_curr.objects.btn_view_label == NULL) return;

    lv_obj_set_style_radius(g_page07_curr.objects.btn_view, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_view, LV_OPA_COVER, 0);
    lv_damped_button_set_palette(g_page07_curr.objects.btn_view,
                                 lv_color_hex(0x0073FF), lv_color_hex(0x005DDB));
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_view, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_view, 12, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_view, LV_OPA_10, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_view_label, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g_page07_curr.objects.btn_view_label,
                      (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) ? "CARD" : "VIEW");
    lv_obj_center(g_page07_curr.objects.btn_view_label);
}

static void curr_style_fav_button(void)
{
    lv_color_t bg;

    if (g_page07_curr.objects.btn_favorite == NULL || g_page07_curr.objects.btn_favorite_label == NULL) return;

    bg = g_page07_curr.model.favorite_only ? lv_color_hex(0xBFDFFF) : lv_color_hex(0xE9EDF0);
    lv_obj_set_style_radius(g_page07_curr.objects.btn_favorite, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_favorite, LV_OPA_COVER, 0);
    lv_damped_button_set_palette(g_page07_curr.objects.btn_favorite, bg,
                                 lv_color_darken(bg, LV_OPA_20));
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_favorite, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_favorite, 0, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_favorite, LV_OPA_0, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_favorite_label,
                                g_page07_curr.model.favorite_only ? lv_color_hex(0x377DAE) : lv_color_hex(0x5F5F5F), 0);
    lv_label_set_text(g_page07_curr.objects.btn_favorite_label, "FAV");
    lv_obj_center(g_page07_curr.objects.btn_favorite_label);
}

static void curr_style_back_button(void)
{
    if (g_page07_curr.objects.btn_back == NULL || g_page07_curr.objects.btn_back_label == NULL) return;

    lv_obj_set_style_radius(g_page07_curr.objects.btn_back, 10, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.btn_back, LV_OPA_COVER, 0);
    lv_damped_button_set_palette(g_page07_curr.objects.btn_back,
                                 lv_color_hex(0xE9EDF0), lv_color_hex(0xBFC3C8));
    lv_obj_set_style_border_width(g_page07_curr.objects.btn_back, 0, 0);
    lv_obj_set_style_shadow_width(g_page07_curr.objects.btn_back, 0, 0);
    lv_obj_set_style_shadow_opa(g_page07_curr.objects.btn_back, LV_OPA_0, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.btn_back_label, lv_color_hex(0x000000), 0);
    lv_label_set_text(g_page07_curr.objects.btn_back_label, "Back");
    lv_obj_center(g_page07_curr.objects.btn_back_label);
}

static void curr_refresh_left_buttons(void)
{
    curr_style_view_button();
    curr_style_fav_button();
    curr_style_back_button();
}

static void curr_back_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_manager_switch(UI_PAGE_MAIN);
}

static void curr_scroll_to_visible_idx(int vis_idx, bool anim)
{
    if (g_page07_curr.model.visible_count <= 0) return;
    if (vis_idx < 0) vis_idx = 0;
    if (vis_idx >= g_page07_curr.model.visible_count) vis_idx = g_page07_curr.model.visible_count - 1;
    page07_curr_carousel_snap(&g_page07_curr.carousel, (unsigned)vis_idx, anim);
}

static void curr_select_and_exit_abs(int abs_idx)
{
    char curr_code[4];
    char target_code[4];

    if (abs_idx < 0 || !currency_state_get_code((uint8_t)abs_idx, target_code)) return;
    if (currency_service_switch_pending() || setting_service_mode_is_pending()) return;
    currency_state_get_selected_code(curr_code);
    if (page07_curr_model_code_equal(curr_code, target_code)) {
        ui_manager_switch(UI_PAGE_MAIN);
        return;
    }

    if (!currency_service_request_switch((uint8_t)abs_idx, target_code)) return;
    if (protocol_send(0x03, (const uint8_t*)target_code, 3) < 0) {
        currency_switch_result_t result;

        if (currency_service_take_switch_result(0x02, &result)) {
            page_07_curr_apply_switch_result(&result);
        }
    }
}

void page_07_curr_apply_switch_result(const currency_switch_result_t* result)
{
    char curr_code[4];

    if (!result) return;
    if (result->success) {
        g_page07_curr.model.selected_abs_idx = result->target_index;
        g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
        page07_curr_model_save();
        if (curr_page == NULL) return;
        ui_manager_switch(UI_PAGE_MAIN);
        // 切换币种成功后再次归零，确保不会出现首行被遮挡
        page_01_main_scroll_reset();
        return;
    }

    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
    if (curr_page == NULL) return;
    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_apply_selected_style();
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx, true);
    } else {
        curr_refresh_right_views();
    }
    show_currency_set_fail_popup();
}

static void curr_update_card_fav_content(int i)
{
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    bool fav = page07_curr_model_is_favorite(card->abs_idx);
    if (card->fav_btn == NULL || card->fav_icon == NULL) return;
    if (card->favorite_initialized && card->favorite_value == fav) return;

    lv_obj_set_style_bg_color(card->fav_btn,
        lv_color_hex(0xBFDFFF), 0);
    lv_obj_set_style_bg_opa(card->fav_btn, LV_OPA_TRANSP, 0);
    lv_img_set_src(card->fav_icon, fav ? (const void *)"L:/usr/local/share/lvgl_data/fav.png" : (const void *)&curr_star_outline);
    card->favorite_initialized = true;
    card->favorite_value = fav;
}

static void curr_update_card_fav_ui(int i)
{
    bool sel = (i == (int)ui_scroll_physics_nearest(&g_page07_curr.carousel.motion));
    page07_curr_card_t *card = &g_page07_curr.cards[i];
    if (card->selected_label != NULL) {
        if (card->abs_idx == g_page07_curr.model.selected_abs_idx)
            lv_obj_clear_flag(card->selected_label, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(card->selected_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (card->fav_btn == NULL || card->fav_icon == NULL) return;

    if (!sel || g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD ||
        page07_curr_model_is_fixed(card->abs_idx)) {
        lv_obj_add_flag(card->fav_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(card->fav_btn, LV_OBJ_FLAG_HIDDEN);
    curr_update_card_fav_content(i);
}

static void curr_update_grid_fav_ui(int i)
{
    bool fav = page07_curr_model_is_favorite(g_page07_curr.grid_items[i].abs_idx);

    if (g_page07_curr.grid_items[i].fav_btn == NULL || g_page07_curr.grid_items[i].fav_icon == NULL) return;

    if (page07_curr_model_is_fixed(g_page07_curr.grid_items[i].abs_idx)) {
        lv_obj_add_flag(g_page07_curr.grid_items[i].fav_btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_TRANSP, 0);
    lv_img_set_src(g_page07_curr.grid_items[i].fav_icon, fav ? (const void *)"L:/usr/local/share/lvgl_data/fav.png" : (const void *)&curr_star_outline);
}

static void curr_fav_press_feedback_cb(lv_event_t* e)
{
    lv_obj_t* btn = lv_event_get_target(e);
    if (btn == NULL) return;

    lv_obj_t* icon = lv_obj_get_child(btn, 0);
    if (icon == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        lv_obj_set_style_opa(btn, 220, 0);
        lv_img_set_zoom(icon, 235);
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        lv_obj_set_style_opa(btn, LV_OPA_COVER, 0);
        lv_img_set_zoom(icon, 256);
    }
}

bool ui_page_07_curr_prepare_static_step(void)
{
    if (curr_page == NULL || !lv_obj_is_valid(curr_page) ||
        page07_curr_carousel_busy(&g_page07_curr.carousel) ||
        lv_obj_has_flag(curr_page, LV_OBJ_FLAG_HIDDEN) ||
        g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD ||
        g_page07_curr.objects.card_layer == NULL ||
        !lv_obj_is_valid(g_page07_curr.objects.card_layer) ||
        lv_obj_has_flag(g_page07_curr.objects.card_layer,
                        LV_OBJ_FLAG_HIDDEN) ||
        currency_state_count() <= 0) {
        return false;
    }

    return page07_curr_card_render_prewarm_step();
}

bool ui_page_07_curr_prewarm_ready(void)
{
    return currency_state_list_is_ready();
}

static void curr_apply_selected_style(void)
{
    if (g_page07_curr.objects.card_layer == NULL) return;
    /* Keep a pressed favorite alive even as its card moves away from focus.
     * Its release must reach the shared viewport before visibility changes. */
    if (g_page07_curr.carousel.active) return;
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        curr_update_card_fav_ui(i);
    }
    g_curr_card_styled_visible_idx =
        (int)ui_scroll_physics_nearest(&g_page07_curr.carousel.motion);
}

static void curr_project_carousel(void)
{
    page07_curr_carousel_t *carousel = &g_page07_curr.carousel;
    if (g_page07_curr.objects.list == NULL) return;
    float position = carousel->motion.position;
    int offset = (int)(position + (position < 0.0f ? -0.5f : 0.5f));
    int focus = (int)ui_scroll_physics_nearest(&carousel->motion);
    bool cache_window_changed = focus != g_curr_cache_focus_idx;
    if (lv_obj_get_x(g_page07_curr.objects.list) != -offset) {
        /* A fixed-size strip is translated directly inside a clipped viewport.
         * There is no native scroll animation, content-bound clamp or dummy. */
        lv_obj_set_x(g_page07_curr.objects.list, -offset);
    }
    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        page07_curr_card_t *card = &g_page07_curr.cards[i];
        if (card->card == NULL) continue;
        unsigned strength = ui_scroll_physics_focus(&carousel->motion, (unsigned)i);
        int y = card->base_y -
            (int)((strength * CURR_CARD_FOCUS_LIFT + 512U) / 1024U);
        bool focused = i == focus;
        bool appearance_changed = card->focused != focused;
        card->focused = focused;
        if (cache_window_changed && page07_curr_card_render_sync_snapshots(i, focus))
            appearance_changed = true;
        if (appearance_changed) {
            page07_curr_card_render_apply(i, card->base_x, y);
        } else if (card->drawn_y != y) {
            /* A hidden fallback tree need not move on every animation tick. */
            lv_obj_set_y(card->using_cache ? card->composite : card->render_root, y);
            lv_obj_set_y(card->card, y);
            card->drawn_y = y;
        }
    }
    g_curr_cache_focus_idx = focus;
    if (!carousel->active && g_curr_card_styled_visible_idx != focus) {
        curr_apply_selected_style();
    }
    curr_update_track_by_scroll();
    /* Deliberately no model.selected_* writes, left-summary updates,
     * persistence, decoding or snapshot allocation in this projection. */
}

static void curr_card_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) return;

    if (!page07_curr_carousel_click_allowed(&g_page07_curr.carousel)) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;
    /* Favorite events bubble for dragging, never for currency confirmation. */
    if (lv_event_get_target(e) != g_page07_curr.cards[vis_idx].card) return;
    curr_select_and_exit_abs(g_page07_curr.model.visible_indices[vis_idx]);
}

static void curr_fav_icon_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD) return;
    if (!page07_curr_carousel_click_allowed(&g_page07_curr.carousel)) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    page07_curr_model_toggle_favorite(abs_idx);

    if (g_page07_curr.model.favorite_only && !page07_curr_model_is_favorite(abs_idx)) {
        curr_refresh_right_views();
        return;
    }

    curr_apply_selected_style();
}

static void curr_grid_fav_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_GRID) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    page07_curr_model_toggle_favorite(abs_idx);

    if (g_page07_curr.model.favorite_only && !page07_curr_model_is_favorite(abs_idx)) {
        curr_refresh_right_views();
        return;
    }

    curr_update_grid_fav_ui(vis_idx);
}

static void curr_grid_item_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_GRID) return;

    int vis_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (vis_idx < 0 || vis_idx >= g_page07_curr.model.visible_count) return;

    int abs_idx = g_page07_curr.model.visible_indices[vis_idx];
    curr_select_and_exit_abs(abs_idx);
}

/* Arrow navigation uses the same spring owner as swiping; it never selects
 * or sends a currency command. Ignore taps while a drag contact is active. */
static void curr_step_click_cb(lv_event_t *e)
{
    if (g_page07_curr.carousel.active || g_page07_curr.model.visible_count <= 1) return;
    int next = (int)ui_scroll_physics_nearest(&g_page07_curr.carousel.motion)
             + (int)(intptr_t)lv_event_get_user_data(e);
    if (next < 0 || next >= g_page07_curr.model.visible_count) return;
    page07_curr_carousel_snap(&g_page07_curr.carousel, (unsigned)next, true);
}

static void curr_build_card_layer(void)
{
    g_page07_curr.objects.card_layer = lv_obj_create(g_page07_curr.objects.right_area);
    /* Build the complete layer while hidden. Child creation and style setup
     * then cannot enqueue hundreds of intermediate invalid areas. Showing the
     * finished layer produces one coherent first frame. */
    lv_obj_add_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(g_page07_curr.objects.card_layer);
    lv_obj_set_size(g_page07_curr.objects.card_layer, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.card_layer, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.card_layer, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.card_layer, LV_SCROLLBAR_MODE_OFF);
    /* Page-owned labels: rebuilt with the catalog/filter, never in motion. */
    lv_obj_t *heading = lv_label_create(g_page07_curr.objects.card_layer);
    lv_label_set_text(heading, g_page07_curr.model.favorite_only ?
                      "Favorite currencies" : "All currencies");
    lv_obj_set_pos(heading, 24, 25);
    lv_obj_set_style_text_font(heading, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(heading, lv_color_hex(0x60798D), 0);
    lv_obj_t *available = lv_label_create(g_page07_curr.objects.card_layer);
    /* currency_state owns AUT/MUL insertion; do not add them twice. */
    lv_label_set_text_fmt(available, "%u AVAILABLE", (unsigned)currency_state_count());
    lv_obj_set_size(available, 180, LV_SIZE_CONTENT);
    lv_obj_set_pos(available, CURR_VIEW_W - 204, 25);
    lv_obj_set_style_text_align(available, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(available, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(available, lv_color_hex(0x7E91A1), 0);
    page07_curr_carousel_init(&g_page07_curr.carousel,
        g_page07_curr.objects.card_layer,
        (unsigned)g_page07_curr.model.visible_count, CURR_CARD_STRIDE,
        (unsigned)g_page07_curr.model.selected_visible_idx, curr_project_carousel);

    g_page07_curr.objects.list = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.list);
    lv_obj_set_size(g_page07_curr.objects.list,
                    CURR_VIEW_W + (g_page07_curr.model.visible_count - 1) *
                        CURR_CARD_STRIDE, CURR_CARD_SCROLL_H);
    lv_obj_set_pos(g_page07_curr.objects.list, 0, CURR_CARD_STRIP_Y);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.list, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.list, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_page07_curr.objects.list, LV_OBJ_FLAG_SCROLLABLE);
    page07_curr_carousel_bind_child(g_page07_curr.objects.list);
    perf_profile_watch_invalidation(g_page07_curr.objects.list,
                                    "CURRENCY_CARD_SCROLL");

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.model.visible_indices[i];
        int x = CURR_CARD_FIRST_X + i * CURR_CARD_STRIDE;
        char curr_code[4];

        if (!currency_state_get_code((uint8_t)abs_idx, curr_code)) continue;

        g_page07_curr.cards[i].abs_idx = abs_idx;
        g_page07_curr.cards[i].base_x = x;
        g_page07_curr.cards[i].base_y = CURR_CARD_LOCAL_Y;
        g_page07_curr.cards[i].drawn_y = CURR_CARD_LOCAL_Y;

        g_page07_curr.cards[i].render_root =
            lv_obj_create(g_page07_curr.objects.list);
        lv_obj_remove_style_all(g_page07_curr.cards[i].render_root);
        lv_obj_clear_flag(g_page07_curr.cards[i].render_root,
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].render_root,
                                  LV_SCROLLBAR_MODE_OFF);

        g_page07_curr.cards[i].name =
            lv_label_create(g_page07_curr.cards[i].render_root);
        lv_label_set_text(g_page07_curr.cards[i].name,
                          currency_state_display_code(curr_code));
        lv_obj_set_pos(g_page07_curr.cards[i].name, 22, 158);
        lv_obj_set_style_text_font(g_page07_curr.cards[i].name, &lv_font_instrument_sans_semibold_32, 0);

        g_page07_curr.cards[i].no =
            lv_label_create(g_page07_curr.cards[i].render_root);
        lv_label_set_text_fmt(g_page07_curr.cards[i].no, "NO. %02d", abs_idx + 1);
        lv_obj_set_pos(g_page07_curr.cards[i].no, 22, 230);
        lv_obj_set_style_text_font(g_page07_curr.cards[i].no, &lv_font_instrument_sans_medium_12, 0);

        /* Static footer divider stays inside the original card footprint. */
        g_page07_curr.cards[i].focus_mark =
            lv_obj_create(g_page07_curr.cards[i].render_root);
        lv_obj_remove_style_all(g_page07_curr.cards[i].focus_mark);
        lv_obj_set_pos(g_page07_curr.cards[i].focus_mark, 22, 214);
        lv_obj_set_size(g_page07_curr.cards[i].focus_mark, 156, 1);
        lv_obj_set_style_bg_color(g_page07_curr.cards[i].focus_mark, lv_color_hex(0xEEF1F6), 0);
        lv_obj_set_style_bg_opa(g_page07_curr.cards[i].focus_mark, LV_OPA_COVER, 0);
        lv_obj_clear_flag(g_page07_curr.cards[i].focus_mark, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

        /* Missing composites are captured by the active-page idle prewarmer,
         * never in a motion callback or while this retained page is hidden. */
        g_page07_curr.cards[i].composite =
            lv_img_create(g_page07_curr.objects.list);
        lv_obj_clear_flag(g_page07_curr.cards[i].composite,
                          LV_OBJ_FLAG_CLICKABLE);

        g_page07_curr.cards[i].card = lv_obj_create(g_page07_curr.objects.list);
        lv_obj_remove_style_all(g_page07_curr.cards[i].card);
        lv_obj_set_size(g_page07_curr.cards[i].card, CURR_CARD_W,
                        CURR_CARD_H);
        lv_obj_set_pos(g_page07_curr.cards[i].card, x, CURR_CARD_LOCAL_Y);
        lv_obj_set_style_radius(g_page07_curr.cards[i].card, CURR_CARD_RADIUS, 0);
        lv_obj_set_style_bg_opa(g_page07_curr.cards[i].card,
                                LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_page07_curr.cards[i].card, 0, 0);
        /* The real 1px border belongs to the rendered card, never a second
         * large selection rectangle on this transparent input owner. */
        lv_obj_set_style_shadow_width(g_page07_curr.cards[i].card, 0, 0);
        lv_obj_set_style_shadow_opa(g_page07_curr.cards[i].card, LV_OPA_0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].card,
                                  LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.cards[i].card,
                          LV_OBJ_FLAG_SCROLLABLE);
        page07_curr_carousel_bind_child(g_page07_curr.cards[i].card);
        lv_obj_add_event_cb(g_page07_curr.cards[i].card, curr_card_click_cb,
                            LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* Keep the transformed flag outside the static face capture.
         * Both layers share identical geometry; cached/live faces cannot
         * re-sample or change the flag's source, zoom or position. */
        g_page07_curr.cards[i].img =
            lv_img_create(PAGE07_CURR_SPLIT_FACE_CACHE ? g_page07_curr.cards[i].card :
                          g_page07_curr.cards[i].render_root);
        lv_img_set_src(g_page07_curr.cards[i].img, get_currency_img(curr_code));
        page07_curr_view_set_img_target_width(g_page07_curr.cards[i].img, curr_code, CURR_FLAG_TARGET_W);
        g_page07_curr.cards[i].has_scaled_flag = !PAGE07_CURR_SPLIT_FACE_CACHE;
        lv_obj_set_pos(g_page07_curr.cards[i].img, -27, 34);

        lv_obj_clear_flag(g_page07_curr.cards[i].img, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *selected = lv_obj_create(g_page07_curr.cards[i].card);
        lv_obj_remove_style_all(selected);
        lv_obj_set_pos(selected, 94, 230);
        lv_obj_set_size(selected, 84, 20);
        lv_obj_clear_flag(selected, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        static const lv_point_t selected_tick[] = {{1,8},{4,11},{10,4}};
        lv_obj_t *tick = lv_line_create(selected);
        lv_line_set_points(tick, selected_tick, 3);
        lv_obj_set_style_line_color(tick, lv_color_hex(0x377DAE), 0);
        lv_obj_set_style_line_width(tick, 1, 0);
        lv_obj_set_style_line_rounded(tick, true, 0);
        lv_obj_t *selected_text = lv_label_create(selected);
        lv_label_set_text(selected_text, "Selected");
        lv_obj_set_pos(selected_text, 17, 0);
        lv_obj_set_style_text_font(selected_text, &lv_font_instrument_sans_medium_12, 0);
        lv_obj_set_style_text_color(selected_text, lv_color_hex(0x377DAE), 0);
        g_page07_curr.cards[i].selected_label = selected;
        lv_obj_add_flag(selected, LV_OBJ_FLAG_HIDDEN);

        g_page07_curr.cards[i].fav_btn =
            lv_obj_create(g_page07_curr.cards[i].card);
        lv_obj_set_size(g_page07_curr.cards[i].fav_btn, CURR_FAV_BTN_IN_CARD_W, CURR_FAV_BTN_IN_CARD_H);
        lv_obj_set_pos(g_page07_curr.cards[i].fav_btn, CURR_FAV_BTN_IN_CARD_X, CURR_FAV_BTN_IN_CARD_Y);
        lv_obj_set_style_radius(g_page07_curr.cards[i].fav_btn, 14, 0);
        lv_obj_set_style_border_width(g_page07_curr.cards[i].fav_btn, 0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.cards[i].fav_btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.cards[i].fav_btn, LV_OBJ_FLAG_SCROLLABLE);
        page07_curr_carousel_bind_child(g_page07_curr.cards[i].fav_btn);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_icon_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_page07_curr.cards[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

        g_page07_curr.cards[i].fav_icon = lv_img_create(g_page07_curr.cards[i].fav_btn);
        /* Bind every icon once, before motion. Focus changes only toggle its
         * visibility; only a real favorite edit rebinds source/palette. */
        curr_update_card_fav_content(i);
        lv_obj_center(g_page07_curr.cards[i].fav_icon);

        page07_curr_card_render_apply(i, x, CURR_CARD_LOCAL_Y);
    }

    lv_obj_t *hint = lv_label_create(g_page07_curr.objects.card_layer);
    lv_label_set_text(hint, "Select a currency to continue");
    lv_obj_set_pos(hint, 24, 358);
    lv_obj_set_style_text_font(hint, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x7E91A1), 0);
    /* Transparent arrow buttons retain a 36px touch target. */
    static const lv_point_t chevron_left[] = {{18,12},{12,18},{18,24}};
    static const lv_point_t chevron_right[] = {{12,12},{18,18},{12,24}};
    for (int direction = -1; direction <= 1; direction += 2) {
        lv_obj_t *button = lv_btn_create(g_page07_curr.objects.card_layer);
        lv_obj_remove_style_all(button);
        lv_obj_set_size(button, 36, 36);
        lv_obj_set_pos(button, direction < 0 ? CURR_TRACK_X - 52 : CURR_TRACK_X + CURR_TRACK_W + 16, 348);
        lv_obj_set_style_radius(button, 10, 0);
        if (direction < 0) g_page07_curr.objects.arrow_prev = button;
        else g_page07_curr.objects.arrow_next = button;
        lv_obj_set_style_bg_opa(button, LV_OPA_50, 0);
        lv_obj_set_style_border_width(button, 1, 0);
        lv_obj_set_style_border_color(button, lv_color_hex(0xE1E7EC), 0);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(button, curr_step_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)direction);
        lv_damped_button_register(button, lv_color_hex(0xF7F8FA), lv_color_hex(0xE9EDF0));
        lv_obj_t *line = lv_line_create(button);
        lv_line_set_points(line, direction < 0 ? chevron_left : chevron_right, 3);
        lv_obj_set_style_line_color(line, lv_color_hex(0x7E91A1), 0);
        lv_obj_set_style_line_width(line, 1, 0);
        lv_obj_set_style_line_rounded(line, true, 0);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }

    g_page07_curr.objects.track = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.track);
    lv_obj_clear_flag(g_page07_curr.objects.track,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_page07_curr.objects.track, CURR_TRACK_W, CURR_TRACK_H);
    lv_obj_set_pos(g_page07_curr.objects.track, CURR_TRACK_X, CURR_TRACK_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.track, lv_color_hex(CURR_TRACK_BG), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_page07_curr.objects.track, CURR_TRACK_H / 2, 0);

    g_page07_curr.objects.thumb = lv_obj_create(g_page07_curr.objects.card_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.thumb);
    lv_obj_clear_flag(g_page07_curr.objects.thumb,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_page07_curr.objects.thumb, 1, CURR_TRACK_H);
    lv_obj_set_pos(g_page07_curr.objects.thumb, CURR_TRACK_X, CURR_TRACK_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.thumb, lv_color_hex(CURR_TRACK_FG), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.thumb, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_page07_curr.objects.thumb, CURR_TRACK_H / 2, 0);
    g_curr_track_x = -1;
    g_curr_track_w = -1;
    g_curr_card_styled_visible_idx = -1;
    g_curr_cache_focus_idx = -1;
    curr_project_carousel();
}

static void curr_build_grid_layer(void)
{
    g_page07_curr.objects.grid_layer = lv_obj_create(g_page07_curr.objects.right_area);
    /* Keep the incomplete grid invisible for the same reason as the card
     * layer: only publish it after every child is ready. */
    lv_obj_add_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(g_page07_curr.objects.grid_layer);
    lv_obj_set_size(g_page07_curr.objects.grid_layer, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.grid_layer, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.grid_layer, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.grid_layer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_SCROLLABLE);

    g_page07_curr.objects.grid_scroll = lv_obj_create(g_page07_curr.objects.grid_layer);
    lv_obj_remove_style_all(g_page07_curr.objects.grid_scroll);
    lv_obj_set_size(g_page07_curr.objects.grid_scroll, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.grid_scroll, 0, 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.grid_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(g_page07_curr.objects.grid_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.grid_scroll, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(g_page07_curr.objects.grid_scroll, LV_OBJ_FLAG_SCROLLABLE);
    perf_profile_watch_invalidation(g_page07_curr.objects.grid_scroll,
                                    "CURRENCY_GRID_SCROLL");

    int rows = (g_page07_curr.model.visible_count + CURR_GRID_COLS - 1) / CURR_GRID_COLS;
    int content_h = CURR_GRID_START_Y + rows * CURR_GRID_ROW_STEP + 10;
    if (content_h < CURR_VIEW_H) content_h = CURR_VIEW_H;

    lv_obj_t* content = lv_obj_create(g_page07_curr.objects.grid_scroll);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, CURR_VIEW_W, content_h);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.model.visible_indices[i];
        int row = i / CURR_GRID_COLS;
        int col = i % CURR_GRID_COLS;
        int x = CURR_GRID_START_X + col * CURR_GRID_CELL_W;
        int y = CURR_GRID_START_Y + row * CURR_GRID_ROW_STEP;
        char curr_code[4];

        if (!currency_state_get_code((uint8_t)abs_idx, curr_code)) continue;

        g_page07_curr.grid_items[i].abs_idx = abs_idx;

        g_page07_curr.grid_items[i].item = lv_obj_create(content);
        lv_obj_remove_style_all(g_page07_curr.grid_items[i].item);
        lv_obj_set_size(g_page07_curr.grid_items[i].item, CURR_GRID_ITEM_W, CURR_GRID_CELL_H);
        lv_obj_set_pos(g_page07_curr.grid_items[i].item, x, y);
        lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].item, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(g_page07_curr.grid_items[i].item, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(g_page07_curr.grid_items[i].item, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(g_page07_curr.grid_items[i].item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].item, curr_grid_item_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        g_page07_curr.grid_items[i].img = lv_img_create(g_page07_curr.grid_items[i].item);
        lv_img_set_src(g_page07_curr.grid_items[i].img, get_currency_img(curr_code));
        page07_curr_view_set_img_target_width(g_page07_curr.grid_items[i].img, curr_code, CURR_FLAG_TARGET_W);
        lv_obj_align(g_page07_curr.grid_items[i].img, LV_ALIGN_TOP_MID, CURR_GRID_GROUP_OFS_X, CURR_GRID_FLAG_Y);

        g_page07_curr.grid_items[i].selected_mark = lv_img_create(g_page07_curr.grid_items[i].item);
        lv_img_set_src(g_page07_curr.grid_items[i].selected_mark, CURR_GRID_SELECTED_MARK_PATH);
        lv_obj_set_size(g_page07_curr.grid_items[i].selected_mark, 24, 24);
        lv_obj_align_to(g_page07_curr.grid_items[i].selected_mark,
                        g_page07_curr.grid_items[i].img,
                        LV_ALIGN_CENTER, 0, 0);
        if (abs_idx != g_page07_curr.model.selected_abs_idx) {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark, LV_OBJ_FLAG_HIDDEN);
        }

        g_page07_curr.grid_items[i].fav_btn = lv_obj_create(g_page07_curr.grid_items[i].item);
        lv_obj_set_size(g_page07_curr.grid_items[i].fav_btn, CURR_FAV_BTN_IN_CARD_W - 2, CURR_FAV_BTN_IN_CARD_H - 2);
        lv_obj_set_pos(g_page07_curr.grid_items[i].fav_btn, CURR_GRID_FAV_X, CURR_GRID_FAV_Y);
        lv_obj_set_style_radius(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_bg_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_shadow_width(g_page07_curr.grid_items[i].fav_btn, 0, 0);
        lv_obj_set_style_shadow_opa(g_page07_curr.grid_items[i].fav_btn, LV_OPA_0, 0);
        lv_obj_set_scrollbar_mode(g_page07_curr.grid_items[i].fav_btn, LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(g_page07_curr.grid_items[i].fav_btn, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(g_page07_curr.grid_items[i].fav_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_grid_fav_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_RELEASED, NULL);
        lv_obj_add_event_cb(g_page07_curr.grid_items[i].fav_btn, curr_fav_press_feedback_cb, LV_EVENT_PRESS_LOST, NULL);

        g_page07_curr.grid_items[i].fav_icon = lv_img_create(g_page07_curr.grid_items[i].fav_btn);
        lv_img_set_src(g_page07_curr.grid_items[i].fav_icon, &curr_star_outline);
        lv_obj_center(g_page07_curr.grid_items[i].fav_icon);

        g_page07_curr.grid_items[i].name = lv_label_create(g_page07_curr.grid_items[i].item);
        lv_label_set_text(g_page07_curr.grid_items[i].name,
                          currency_state_display_code(curr_code));
        lv_obj_set_width(g_page07_curr.grid_items[i].name, CURR_GRID_ITEM_W);
        lv_obj_set_style_text_align(g_page07_curr.grid_items[i].name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(g_page07_curr.grid_items[i].name, &lv_font_instrument_sans_medium_20, 0);
        lv_obj_set_style_text_color(g_page07_curr.grid_items[i].name,
                                    (abs_idx == g_page07_curr.model.selected_abs_idx) ? lv_color_hex(CURR_TEXT_SEL) : lv_color_hex(0x7E7E7E), 0);
        lv_obj_align(g_page07_curr.grid_items[i].name, LV_ALIGN_BOTTOM_MID, CURR_GRID_GROUP_OFS_X, -CURR_GRID_TEXT_BOTTOM);

        if (abs_idx == g_page07_curr.model.selected_abs_idx) {
            page07_curr_view_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            page07_curr_view_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        curr_update_grid_fav_ui(i);
    }

    g_page07_curr.objects.empty_label = NULL;
    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

static void curr_apply_grid_selected_style(void)
{
    if (g_page07_curr.objects.grid_layer == NULL ||
        g_curr_grid_styled_abs_idx ==
            g_page07_curr.model.selected_abs_idx) {
        return;
    }

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.grid_items[i].abs_idx;
        bool selected = abs_idx == g_page07_curr.model.selected_abs_idx;

        if (selected) {
            lv_obj_clear_flag(g_page07_curr.grid_items[i].selected_mark,
                              LV_OBJ_FLAG_HIDDEN);
            page07_curr_view_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark,
                            LV_OBJ_FLAG_HIDDEN);
            page07_curr_view_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        lv_obj_set_style_text_color(
            g_page07_curr.grid_items[i].name,
            selected ? lv_color_hex(CURR_TEXT_SEL)
                     : lv_color_hex(0x7E7E7E), 0);
    }

    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

static void curr_set_mode_visible(void)
{
    page07_curr_carousel_enable(&g_page07_curr.carousel,
        g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD);
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        if (g_page07_curr.objects.grid_layer) {
            lv_obj_add_flag(g_page07_curr.objects.grid_layer,
                            LV_OBJ_FLAG_HIDDEN);
        }
        if (g_page07_curr.objects.card_layer) {
            lv_obj_clear_flag(g_page07_curr.objects.card_layer, LV_OBJ_FLAG_HIDDEN);
            curr_apply_selected_style();
            curr_update_track_by_scroll();
        }
    } else {
        if (g_page07_curr.objects.card_layer) {
            lv_obj_add_flag(g_page07_curr.objects.card_layer,
                            LV_OBJ_FLAG_HIDDEN);
        }
        if (g_page07_curr.objects.grid_layer) {
            lv_obj_clear_flag(g_page07_curr.objects.grid_layer, LV_OBJ_FLAG_HIDDEN);
            curr_apply_grid_selected_style();
            for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
                curr_update_grid_fav_ui(i);
            }
        }
    }
}

static void curr_switch_cached_view(void)
{
    if (g_page07_curr.model.visible_count <= 0) {
        curr_refresh_right_views();
        return;
    }

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        if (g_page07_curr.objects.card_layer == NULL ||
            !lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
            memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
            curr_build_card_layer();
        }
    } else if (g_page07_curr.objects.grid_layer == NULL ||
               !lv_obj_is_valid(g_page07_curr.objects.grid_layer)) {
        memset(g_page07_curr.grid_items, 0,
               sizeof(g_page07_curr.grid_items));
        curr_build_grid_layer();
    }

    curr_set_mode_visible();
    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_scroll_to_visible_idx(
            g_page07_curr.model.selected_visible_idx, false);
        curr_update_track_by_scroll();
    }
}

static void curr_refresh_right_views(void)
{
    char curr_code[4];

    if (g_page07_curr.objects.right_area == NULL) return;
    /* Stop before changing index maps or deleting any referenced objects. */
    page07_curr_carousel_destroy(&g_page07_curr.carousel);

    if (g_page07_curr.objects.empty_label && lv_obj_is_valid(g_page07_curr.objects.empty_label)) {
        lv_obj_del(g_page07_curr.objects.empty_label);
        g_page07_curr.objects.empty_label = NULL;
    }

    page07_curr_model_refresh_visible();

    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);

    if (g_page07_curr.objects.card_layer && lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
        perf_profile_unwatch_invalidation(g_page07_curr.objects.list);
        lv_obj_del(g_page07_curr.objects.card_layer);
        page07_curr_card_render_release_snapshots();
        g_page07_curr.objects.card_layer = NULL;
        g_page07_curr.objects.list = NULL;
        g_page07_curr.objects.track = NULL;
    g_page07_curr.objects.arrow_prev = NULL;
    g_page07_curr.objects.arrow_next = NULL;
        g_page07_curr.objects.thumb = NULL;
    }

    if (g_page07_curr.objects.grid_layer && lv_obj_is_valid(g_page07_curr.objects.grid_layer)) {
        perf_profile_unwatch_invalidation(g_page07_curr.objects.grid_scroll);
        lv_obj_del(g_page07_curr.objects.grid_layer);
        g_page07_curr.objects.grid_layer = NULL;
        g_page07_curr.objects.grid_scroll = NULL;
        g_page07_curr.objects.empty_label = NULL;
    }

    memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
    memset(g_page07_curr.grid_items, 0, sizeof(g_page07_curr.grid_items));

    if (g_page07_curr.model.visible_count <= 0) {
        g_page07_curr.objects.empty_label = lv_label_create(g_page07_curr.objects.right_area);
        lv_label_set_text(g_page07_curr.objects.empty_label, g_page07_curr.model.favorite_only ? "NO FAVORITE CURRENCY" : "NO CURRENCY");
        lv_obj_set_style_text_color(g_page07_curr.objects.empty_label, lv_color_hex(0xB3B3B3), 0);
        lv_obj_set_style_text_font(g_page07_curr.objects.empty_label, &lv_font_instrument_sans_medium_20, 0);
        lv_obj_center(g_page07_curr.objects.empty_label);
        return;
    }

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_build_card_layer();
    } else {
        curr_build_grid_layer();
    }
    curr_set_mode_visible();

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx, false);
        curr_apply_selected_style();
        curr_update_track_by_scroll();
    }
}

static void curr_view_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_page07_curr.model.view_mode = (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) ? PAGE07_CURR_VIEW_GRID : PAGE07_CURR_VIEW_CARD;
    curr_switch_cached_view();
    curr_refresh_left_buttons();
    page07_curr_model_save();

}

static void curr_fav_btn_click_cb(lv_event_t* e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    g_page07_curr.model.favorite_only = !g_page07_curr.model.favorite_only;
    page07_curr_model_save();
    curr_refresh_left_buttons();
    curr_refresh_right_views();
}

void page_07_curr_img_reset(void)
{
    page07_curr_carousel_destroy(&g_page07_curr.carousel);

    perf_profile_unwatch_invalidation(g_page07_curr.objects.list);
    perf_profile_unwatch_invalidation(g_page07_curr.objects.grid_scroll);

    if (g_page07_curr.objects.root && lv_obj_is_valid(g_page07_curr.objects.root)) {
        lv_obj_del(g_page07_curr.objects.root);
    }
    page07_curr_card_render_release_snapshots();

    g_page07_curr.objects.root = NULL;
    g_page07_curr.objects.left_panel = NULL;
    g_page07_curr.objects.left_img = NULL;
    g_page07_curr.objects.left_code = NULL;
    g_page07_curr.objects.left_code_decor = NULL;
    g_page07_curr.objects.left_no = NULL;

    g_page07_curr.objects.btn_view = NULL;
    g_page07_curr.objects.btn_view_label = NULL;
    g_page07_curr.objects.btn_favorite = NULL;
    g_page07_curr.objects.btn_favorite_label = NULL;
    g_page07_curr.objects.btn_back = NULL;
    g_page07_curr.objects.btn_back_label = NULL;

    g_page07_curr.objects.right_area = NULL;
    g_page07_curr.objects.card_layer = NULL;
    g_page07_curr.objects.grid_layer = NULL;

    g_page07_curr.objects.list = NULL;
    g_page07_curr.objects.track = NULL;
    g_page07_curr.objects.thumb = NULL;

    g_page07_curr.objects.grid_scroll = NULL;
    g_page07_curr.objects.empty_label = NULL;

    memset(g_page07_curr.cards, 0, sizeof(g_page07_curr.cards));
    memset(g_page07_curr.grid_items, 0, sizeof(g_page07_curr.grid_items));
    memset(g_page07_curr.model.visible_indices, 0, sizeof(g_page07_curr.model.visible_indices));
    g_page07_curr.model.visible_count = 0;
    g_curr_track_x = -1;
    g_curr_track_w = -1;
    g_curr_card_styled_visible_idx = -1;
    g_curr_grid_styled_abs_idx = -1;
}

void page_07_curr_img_refre(void)
{
    if (curr_page == NULL || currency_state_count() <= 0) return;

    page07_curr_model_load();

    page_07_curr_img_reset();

    g_page07_curr.model.selected_visible_idx = page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);

    g_page07_curr.objects.root = lv_obj_create(curr_page);
    lv_obj_remove_style_all(g_page07_curr.objects.root);
    lv_obj_set_size(g_page07_curr.objects.root, 1280, 400);
    lv_obj_set_pos(g_page07_curr.objects.root, 0, 0);
    lv_obj_clear_flag(g_page07_curr.objects.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.root, LV_SCROLLBAR_MODE_OFF);

    g_page07_curr.objects.left_panel = lv_obj_create(g_page07_curr.objects.root);
    lv_obj_remove_style_all(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.left_panel, CURR_SEL_W, CURR_SEL_H);
    lv_obj_set_pos(g_page07_curr.objects.left_panel, 0, 0);
    lv_obj_set_style_bg_color(g_page07_curr.objects.left_panel, lv_color_hex(CURR_LEFT_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.left_panel, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.left_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.left_panel, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* left_title = lv_label_create(g_page07_curr.objects.left_panel);
    lv_label_set_text(left_title, "CURRENCY");
    lv_obj_set_pos(left_title, 105, 14);
    lv_obj_set_style_text_font(left_title, &lv_font_instrument_sans_semibold_24, 0);
    lv_obj_set_style_text_color(left_title, lv_color_hex(0x707070), 0);

    g_page07_curr.objects.left_img = lv_img_create(g_page07_curr.objects.left_panel);
    lv_img_set_zoom(g_page07_curr.objects.left_img, 170);
    lv_obj_align(g_page07_curr.objects.left_img, LV_ALIGN_TOP_MID, CURR_LEFT_IMG_ALIGN_X, CURR_LEFT_IMG_ALIGN_Y);

    lv_obj_t *current_caption = lv_label_create(g_page07_curr.objects.left_panel);
    lv_label_set_text(current_caption, "CURRENT CURRENCY");
    lv_obj_set_pos(current_caption, 51, 82);
    lv_obj_set_style_text_font(current_caption, &lv_font_instrument_sans_medium_12, 0);
    lv_obj_set_style_text_color(current_caption, lv_color_hex(0x7E91A1), 0);
    lv_obj_t *current_dot = lv_obj_create(g_page07_curr.objects.left_panel);
    lv_obj_remove_style_all(current_dot);
    lv_obj_set_pos(current_dot, 39, 87);
    lv_obj_set_size(current_dot, 5, 5);
    lv_obj_set_style_radius(current_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(current_dot, lv_color_hex(0x2C9C7C), 0);
    lv_obj_set_style_bg_opa(current_dot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(current_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *current_rule = lv_obj_create(g_page07_curr.objects.left_panel);
    lv_obj_remove_style_all(current_rule);
    lv_obj_set_pos(current_rule, 39, 288);
    lv_obj_set_size(current_rule, 193, 2);
    lv_obj_set_style_bg_color(current_rule, lv_color_hex(0xEEF1F6), 0);
    lv_obj_set_style_bg_opa(current_rule, LV_OPA_COVER, 0);
    lv_obj_clear_flag(current_rule, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    g_page07_curr.objects.left_code = lv_label_create(g_page07_curr.objects.left_panel);
    lv_obj_set_pos(g_page07_curr.objects.left_code, CURR_LEFT_CODE_X, CURR_LEFT_CODE_Y);
    lv_obj_set_style_text_font(g_page07_curr.objects.left_code, &lv_font_instrument_sans_semibold_40, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.left_code, lv_color_hex(0x202020), 0);

    g_page07_curr.objects.left_no = lv_label_create(g_page07_curr.objects.left_panel);
    lv_obj_set_pos(g_page07_curr.objects.left_no, CURR_LEFT_NO_X, CURR_LEFT_NO_Y);
    lv_obj_set_style_text_font(g_page07_curr.objects.left_no, &lv_font_instrument_sans_medium_14, 0);
    lv_obj_set_style_text_color(g_page07_curr.objects.left_no, lv_color_hex(0x7E91A1), 0);

    g_page07_curr.objects.btn_view = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_view, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_view, CURR_VIEW_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_view, curr_view_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_view_label = lv_label_create(g_page07_curr.objects.btn_view);
    lv_label_set_text(g_page07_curr.objects.btn_view_label, "CARD");
    lv_obj_center(g_page07_curr.objects.btn_view_label);
    lv_damped_button_register(g_page07_curr.objects.btn_view,
                              lv_color_hex(0x0073FF), lv_color_hex(0x005DDB));

    g_page07_curr.objects.btn_favorite = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_favorite, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_favorite, CURR_FAV_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_favorite, curr_fav_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_favorite_label = lv_label_create(g_page07_curr.objects.btn_favorite);
    lv_label_set_text(g_page07_curr.objects.btn_favorite_label, "FAV");
    lv_obj_center(g_page07_curr.objects.btn_favorite_label);
    lv_damped_button_register(g_page07_curr.objects.btn_favorite,
                              lv_color_hex(0xE9EDF0), lv_color_hex(0x737373));

    g_page07_curr.objects.btn_back = lv_btn_create(g_page07_curr.objects.left_panel);
    lv_obj_set_size(g_page07_curr.objects.btn_back, CURR_BTN_W, CURR_BTN_H);
    lv_obj_set_pos(g_page07_curr.objects.btn_back, CURR_BACK_BTN_X, CURR_BTN_Y);
    lv_obj_add_event_cb(g_page07_curr.objects.btn_back, curr_back_btn_click_cb, LV_EVENT_CLICKED, NULL);
    g_page07_curr.objects.btn_back_label = lv_label_create(g_page07_curr.objects.btn_back);
    lv_label_set_text(g_page07_curr.objects.btn_back_label, "Back");
    lv_obj_center(g_page07_curr.objects.btn_back_label);
    lv_damped_button_register(g_page07_curr.objects.btn_back,
                              lv_color_hex(0xE9EDF0), lv_color_hex(0xBFC3C8));

    g_page07_curr.objects.right_area = lv_obj_create(g_page07_curr.objects.root);
    lv_obj_remove_style_all(g_page07_curr.objects.right_area);
    lv_obj_set_size(g_page07_curr.objects.right_area, CURR_VIEW_W, CURR_VIEW_H);
    lv_obj_set_pos(g_page07_curr.objects.right_area, CURR_VIEW_X, CURR_VIEW_Y);
    lv_obj_set_style_bg_color(g_page07_curr.objects.right_area, lv_color_hex(CURR_RIGHT_BG_COLOR), 0);
    lv_obj_set_style_bg_opa(g_page07_curr.objects.right_area, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(g_page07_curr.objects.right_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(g_page07_curr.objects.right_area, LV_SCROLLBAR_MODE_OFF);

    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    curr_refresh_left_buttons();
    curr_refresh_right_views();
    currency_state_get_snapshot(&g_curr_page_snapshot);
    currency_state_get_selected_code(g_curr_page_selected_code);
    g_curr_page_snapshot_valid = true;
}

static bool curr_cached_list_matches(const currency_state_snapshot_t* snapshot)
{
    if (!g_curr_page_snapshot_valid || snapshot == NULL ||
        snapshot->count != g_curr_page_snapshot.count) {
        return false;
    }
    return memcmp(snapshot->codes, g_curr_page_snapshot.codes,
                  sizeof(snapshot->codes)) == 0;
}

static void curr_snapshot_prewarm_timer_cb(lv_timer_t *timer)
{
    currency_state_snapshot_t snapshot;

    if (timer == NULL || curr_page == NULL ||
        !lv_obj_is_valid(curr_page) ||
        lv_obj_has_flag(curr_page, LV_OBJ_FLAG_HIDDEN) ||
        ui_manager_get_current_page() != UI_PAGE_CURR) {
        return;
    }

    /* Snapshot capture is intentionally confined to a quiet window on the
     * active Currency page.  A retained page must be completely dormant
     * after suspend: rendering its hidden object tree steals frame time from
     * the newly visible page and can evict that page's decoded resources.
     * Do not gate this on lv_anim_count_running(); shared components can own
     * infinite animations even while Currency itself is idle. */
    if (lv_disp_get_inactive_time(NULL) < 1200U ||
        page07_curr_carousel_busy(&g_page07_curr.carousel)) {
        return;
    }
    if (currency_state_count() <= 0) return;

    currency_state_get_snapshot(&snapshot);
    if (!curr_cached_list_matches(&snapshot)) {
        page_07_curr_img_refre();
        lv_timer_set_period(timer, 120);
        return;
    }

    if (g_page07_curr.model.view_mode != PAGE07_CURR_VIEW_CARD ||
        g_page07_curr.objects.card_layer == NULL ||
        !lv_obj_is_valid(g_page07_curr.objects.card_layer)) {
        lv_timer_set_period(timer, 500);
        return;
    }

    if (page07_curr_card_render_prewarm_step()) {
        lv_timer_set_period(timer, 120);
    } else {
        lv_timer_set_period(timer, 500);
    }
}

static bool curr_cached_selection_matches(
    const currency_state_snapshot_t* snapshot,
    const char selected_code[4])
{
    if (!g_curr_page_snapshot_valid || snapshot == NULL ||
        selected_code == NULL) {
        return false;
    }

    return strncmp(snapshot->active_code,
                   g_curr_page_snapshot.active_code, 3) == 0 &&
           snapshot->active_currency == g_curr_page_snapshot.active_currency &&
           snapshot->active_index == g_curr_page_snapshot.active_index &&
           strncmp(selected_code, g_curr_page_selected_code, 3) == 0;
}

static void curr_refresh_cached_selection(void)
{
    char curr_code[4];

    /* A retained view owns its current filter and index map. Reloading a
     * saved FAV preference here could remap still-cached ALL card objects. */
    currency_state_get_selected_code(curr_code);
    g_page07_curr.model.selected_abs_idx = page07_curr_model_find_abs_idx(curr_code);
    g_page07_curr.model.selected_visible_idx =
        page07_curr_model_find_visible_pos(g_page07_curr.model.selected_abs_idx);
    curr_set_left_info_by_abs(g_page07_curr.model.selected_abs_idx);
    curr_refresh_left_buttons();

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        return;
    }

    for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
        int abs_idx = g_page07_curr.grid_items[i].abs_idx;
        bool selected = abs_idx == g_page07_curr.model.selected_abs_idx;

        if (selected) {
            lv_obj_clear_flag(g_page07_curr.grid_items[i].selected_mark,
                              LV_OBJ_FLAG_HIDDEN);
            page07_curr_view_set_image_selected_style(g_page07_curr.grid_items[i].img);
        } else {
            lv_obj_add_flag(g_page07_curr.grid_items[i].selected_mark,
                            LV_OBJ_FLAG_HIDDEN);
            page07_curr_view_set_image_unselected_style(g_page07_curr.grid_items[i].img);
        }
        lv_obj_set_style_text_color(g_page07_curr.grid_items[i].name,
                                    selected ? lv_color_hex(CURR_TEXT_SEL)
                                             : lv_color_hex(0x7E7E7E), 0);
        curr_update_grid_fav_ui(i);
    }
    g_curr_grid_styled_abs_idx = g_page07_curr.model.selected_abs_idx;
}

static void curr_focus_confirmed_selection_on_entry(void)
{
    int selected = g_page07_curr.model.selected_abs_idx;

    if (g_page07_curr.model.visible_count <= 0) return;

    /* Browsing focus is not a currency selection. If the current currency
     * is filtered out, reveal ALL for this visit without changing favorites
     * or persisting an automatic filter preference change. */
    if (g_page07_curr.model.favorite_only &&
        !page07_curr_model_is_fixed(selected) &&
        !page07_curr_model_is_favorite(selected)) {
        g_page07_curr.model.favorite_only = false;
        curr_refresh_left_buttons();
        curr_refresh_right_views();
    }

    if (g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD) {
        curr_scroll_to_visible_idx(g_page07_curr.model.selected_visible_idx,
                                   false);
    } else {
        for (int i = 0; i < g_page07_curr.model.visible_count; i++) {
            if (g_page07_curr.grid_items[i].abs_idx == selected &&
                g_page07_curr.grid_items[i].item != NULL) {
                lv_obj_update_layout(g_page07_curr.objects.grid_scroll);
                lv_obj_scroll_to_view(g_page07_curr.grid_items[i].item,
                                      LV_ANIM_OFF);
                break;
            }
        }
    }
}

bool ui_page_07_curr_resume(void)
{
    currency_state_snapshot_t snapshot;
    char selected_code[4];
    bool profile_enabled;
    uint64_t profile_started_us = 0;
    const char* profile_event = "RESUME_CLEAN";

    if (curr_page == NULL || !lv_obj_is_valid(curr_page)) {
        return false;
    }

    currency_state_get_snapshot(&snapshot);
    currency_state_get_selected_code(selected_code);
    profile_enabled = perf_profile_is_enabled();
    if (profile_enabled) {
        profile_started_us = app_clock_monotonic_us();
    }
    if (!curr_cached_list_matches(&snapshot)) {
        profile_event = "RESUME_LIST";
        page_07_curr_img_refre();
    } else if (!curr_cached_selection_matches(&snapshot, selected_code)) {
        profile_event = "RESUME_SELECTION";
        curr_refresh_cached_selection();
        g_curr_page_snapshot = snapshot;
        memcpy(g_curr_page_selected_code, selected_code,
               sizeof(g_curr_page_selected_code));
    }
    /* A clean model snapshot says nothing about a retained browsing offset.
     * Restore focus before showing the page, even when selection is unchanged. */
    curr_focus_confirmed_selection_on_entry();
    lv_obj_clear_flag(curr_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(curr_page);
    page07_curr_carousel_enable(&g_page07_curr.carousel,
        g_page07_curr.model.view_mode == PAGE07_CURR_VIEW_CARD);
    if (g_curr_snapshot_prewarm_timer != NULL) {
        lv_timer_set_period(g_curr_snapshot_prewarm_timer, 120);
        lv_timer_resume(g_curr_snapshot_prewarm_timer);
        lv_timer_ready(g_curr_snapshot_prewarm_timer);
    }
    if (profile_enabled) {
        perf_profile_report_event_us(
            "CURRENCY", profile_event,
            app_clock_elapsed_us32(profile_started_us,
                                   app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_07_curr_suspend(void)
{
    if (curr_page == NULL || !lv_obj_is_valid(curr_page)) {
        return;
    }

    page07_curr_carousel_enable(&g_page07_curr.carousel, false);
    /*
     * g_curr_page_snapshot describes what has actually been rendered, not
     * merely the latest model state.  A successful currency command can
     * switch to MAIN before this cached page has moved its card viewport.
     * Capturing the model here would make resume incorrectly treat the stale
     * viewport as current and skip RESUME_SELECTION.
    */
    lv_obj_add_flag(curr_page, LV_OBJ_FLAG_HIDDEN);
    if (g_curr_snapshot_prewarm_timer != NULL) {
        lv_timer_pause(g_curr_snapshot_prewarm_timer);
    }
}
