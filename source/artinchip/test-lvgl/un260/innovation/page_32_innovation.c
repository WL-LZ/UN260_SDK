#include "page_32_innovation.h"
#include "lv_port_indev.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/app_service/setting_service.h"
#include "un260/font/manrope_fonts.h"
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_modal_dialog.h"
#include "un260/lv_components/lv_dma_snapshot_cache.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_system/ui_text.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/gesture/gesture_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"
#include "un260/lv_drivers/uart_io.h"

#define INNOVATION_BG             0xE8EFF3
#define INNOVATION_CARD           0xFFFFFF
#define INNOVATION_TEXT           0x26333D
#define INNOVATION_MUTED          0x77838D
#define INNOVATION_BLUE           0x3578F6
#define INNOVATION_BLUE_SOFT      0xEAF2FF
#define INNOVATION_GREEN          0x27B36A
#define INNOVATION_RED            0xE45454
#define INNOVATION_LINE           0xDDE5EA
#define INNOVATION_PREVIEW_ARM_DY 1

typedef struct {
    lv_obj_t *root;
    lv_obj_t *status_label;
    lv_obj_t *instruction_label;
    lv_obj_t *target_label;
    lv_obj_t *target_minus;
    lv_obj_t *target_plus;
    lv_obj_t *primary_button;
    lv_obj_t *primary_label;
    lv_obj_t *secondary_button;
    lv_obj_t *secondary_label;
    lv_obj_t *pass_cards[MULTI_PASS_VERIFY_MAX_PASSES];
    lv_obj_t *pass_titles[MULTI_PASS_VERIFY_MAX_PASSES];
    lv_obj_t *pass_values[MULTI_PASS_VERIFY_MAX_PASSES];
    lv_obj_t *comparison_label;
    lv_obj_t *content_pager;
    lv_obj_t *detail_summary;
    lv_obj_t *feature_scroll;
    lv_obj_t *feature_hint_top;
    lv_obj_t *feature_hint_bottom;
    lv_obj_t *gesture_button;
    lv_obj_t *gesture_label;
    lv_timer_t *refresh_timer;
    uint8_t target_passes;
    bool pending_start_after_add_off;
    uint32_t add_request_tick;
    uint32_t rendered_verify_revision;
    uint8_t rendered_target_passes;
    bool rendered_pending_start;
    bool render_valid;
} innovation_page_context_t;

typedef struct {
    bool pressed;
    bool opened;
    bool preview_active;
    lv_point_t start;
    int drag_y;
    uint32_t start_tick;
    uint32_t last_render_tick;
    int last_render_y;
} innovation_handle_gesture_t;

static innovation_page_context_t g_page = {
    .target_passes = MULTI_PASS_VERIFY_MIN_PASSES,
};
static lv_obj_t *g_handle_touch;
static lv_modal_dialog_t g_prompt;
static innovation_handle_gesture_t g_handle_gesture;
static lv_dma_static_surface_t g_transition_snapshot;
static bool g_transition_snapshot_valid;
static bool g_transition_snapshot_dirty = true;
static bool g_transition_prepare_failed;
static uint32_t g_transition_failure_tick;
static lv_timer_t *g_preview_preload_timer;
static bool g_page_transitioning;
static lv_timer_t *g_back_first_frame_timer;
static uint32_t g_back_tick, g_back_frames;
static void innovation_back_stop(void);
static void innovation_preview_preload_async(void *user_data);
static void innovation_preview_preload_timer_cb(lv_timer_t *timer);
static lv_obj_t *innovation_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, uint32_t color);
static void innovation_label_set_if_changed(lv_obj_t *label, const char *text);
static lv_obj_t *innovation_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t width, lv_coord_t height,
                                uint32_t color, lv_coord_t radius);

static void innovation_set_hidden(lv_obj_t *object, bool hidden)
{
    if (object == NULL || !lv_obj_is_valid(object)) return;
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void innovation_set_y_if_changed(lv_obj_t *object, lv_coord_t y)
{
    if (object != NULL && lv_obj_is_valid(object) &&
        lv_obj_get_y(object) != y) {
        lv_obj_set_y(object, y);
    }
}

static void innovation_set_bg_if_changed(lv_obj_t *object, uint32_t color)
{
    if (object == NULL || !lv_obj_is_valid(object)) return;
    if (lv_color_to32(lv_obj_get_style_bg_color(object, 0)) ==
        lv_color_to32(lv_color_hex(color))) return;
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
}

static void innovation_set_text_color_if_changed(lv_obj_t *object,
                                                 uint32_t color)
{
    if (object == NULL || !lv_obj_is_valid(object)) return;
    if (lv_color_to32(lv_obj_get_style_text_color(object, 0)) ==
        lv_color_to32(lv_color_hex(color))) return;
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
}

static void innovation_refresh_pause(void)
{
    if (g_page.refresh_timer != NULL) lv_timer_pause(g_page.refresh_timer);
}

static void innovation_refresh_resume(void)
{
    if (g_page.refresh_timer != NULL) {
        lv_timer_resume(g_page.refresh_timer);
        lv_timer_reset(g_page.refresh_timer);
    }
}

static lv_obj_t *innovation_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text != NULL ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void innovation_label_set_if_changed(lv_obj_t *label, const char *text)
{
    const char *current;
    if (label == NULL || !lv_obj_is_valid(label)) return;
    if (text == NULL) text = "";
    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) lv_label_set_text(label, text);
}

static lv_obj_t *innovation_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t width, lv_coord_t height,
                                uint32_t color, lv_coord_t radius)
{
    lv_obj_t *box = lv_obj_create(parent);

    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, 0, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static lv_obj_t *innovation_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t width, lv_coord_t height,
                                   uint32_t color, const char *text,
                                   lv_event_cb_t callback)
{
    lv_damped_button_style_t style = {
        .normal_color = color,
        .pressed_color = color == INNOVATION_BLUE ? 0x2467DF : 0x74818B,
        .disabled_color = 0xCCD3D8,
        .text_color = 0xFFFFFF,
        .disabled_text_color = 0x8A959E,
        .radius = 14,
    };
    lv_obj_t *button = lv_damped_button_create(parent, &style, text,
                                                &lv_font_instrument_sans_bold_16);
    if (button == NULL) return NULL;
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    if (callback != NULL) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    }
    return button;
}

static void innovation_prompt_close(void)
{
    lv_modal_dialog_hide(&g_prompt);
}

static bool innovation_page_refresh(void);
static void innovation_prompt_single(const char *title, const char *body,
                                     const char *button_text, uint32_t accent,
                                     lv_modal_dialog_action_cb_t callback);

static void innovation_feature_hint_refresh(void)
{
    lv_coord_t top;
    lv_coord_t bottom;
    if (g_page.feature_scroll == NULL) return;
    top = lv_obj_get_scroll_y(g_page.feature_scroll);
    bottom = lv_obj_get_scroll_bottom(g_page.feature_scroll);
    if (g_page.feature_hint_top != NULL) {
        innovation_set_hidden(g_page.feature_hint_top, top <= 2);
    }
    if (g_page.feature_hint_bottom != NULL) {
        innovation_set_hidden(g_page.feature_hint_bottom, bottom <= 2);
    }
}

static void innovation_feature_scroll_cb(lv_event_t *event)
{
    (void)event;
    innovation_feature_hint_refresh();
}

static lv_obj_t *innovation_feature_card(lv_obj_t *parent, int number,
                                         const char *title, const char *status,
                                         bool active)
{
    char number_text[4];
    lv_obj_t *card = innovation_box(parent, 0, 0, 210, active ? 78 : 68,
                                    active ? INNOVATION_BLUE_SOFT : 0xF4F6F8, 16);
    lv_obj_t *label;
    lv_snprintf(number_text, sizeof(number_text), "%02d", number);
    label = innovation_label(card, number_text,
        active ? &lv_font_manrope_bold_28 : &lv_font_instrument_sans_bold_14,
        active ? INNOVATION_BLUE : INNOVATION_MUTED);
    lv_obj_set_pos(label, 14, active ? 12 : 13);
    label = innovation_label(card, title, &lv_font_instrument_sans_bold_14,
                             active ? INNOVATION_TEXT : INNOVATION_MUTED);
    lv_obj_set_pos(label, active ? 58 : 42, active ? 13 : 11);
    lv_obj_set_width(label, active ? 142 : 154);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    label = innovation_label(card, status, &lv_font_instrument_sans_medium_10,
                             active ? INNOVATION_GREEN : 0xA4ADB5);
    lv_obj_set_pos(label, active ? 58 : 42, active ? 43 : 37);
    return card;
}

static void innovation_prompt_dismiss_cb(void *user_data)
{
    (void)user_data;
    innovation_prompt_close();
}

static void innovation_prompt_view_cb(void *user_data)
{
    (void)user_data;
    innovation_prompt_close();
    if (ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER) {
        ui_manager_push_page(UI_PAGE_INNOVATION_CENTER);
    } else {
        innovation_page_refresh();
    }
}

static void innovation_prompt_same_bundle_cb(void *user_data)
{
    multi_pass_capture_kind_t result;

    (void)user_data;
    innovation_prompt_close();
    result = multi_pass_verification_confirm_same_bundle();
    if (result == MULTI_PASS_CAPTURE_COMPLETE) {
        multi_pass_capture_event_t complete = { 0 };
        multi_pass_verify_view_t view;

        multi_pass_verification_get_view(&view);
        complete.kind = MULTI_PASS_CAPTURE_COMPLETE;
        complete.captured_passes = view.captured_passes;
        complete.target_passes = view.target_passes;
        complete.comparison = view.latest_comparison;
        complete.all_passes_match = view.all_passes_match;
        page_32_innovation_notify_verification_event(&complete);
    }
}

static void innovation_prompt_new_bundle_cb(void *user_data)
{
    (void)user_data;
    innovation_prompt_close();
    if (multi_pass_verification_restart_from_latest()) {
        innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_NEW_BASELINE_TITLE),
            ui_text_get(UI_TEXT_INNOVATION_NEW_BASELINE_BODY),
            ui_text_get(UI_TEXT_INNOVATION_CONTINUE), INNOVATION_BLUE, NULL);
    }
}

static void innovation_prompt_single(const char *title, const char *body,
                                     const char *button_text, uint32_t accent,
                                     lv_modal_dialog_action_cb_t callback)
{
    lv_modal_dialog_config_t config = {
        .title = title,
        .body = body,
        .primary_text = button_text,
        .title_font = &lv_font_instrument_sans_bold_24,
        .body_font = &lv_font_instrument_sans_medium_16,
        .button_font = &lv_font_instrument_sans_bold_16,
        .panel_width = 620,
        .panel_height = 276,
        .primary_width = 190,
        .accent_color = accent,
        .primary_color = accent,
        .secondary_color = 0x72808B,
        .primary_action = callback != NULL ? callback : innovation_prompt_dismiss_cb,
    };

    lv_modal_dialog_show(&g_prompt, lv_scr_act(), &config);
}

static void innovation_prompt_review(const multi_pass_capture_event_t *event)
{
    char body[256];
    lv_modal_dialog_config_t config;

    lv_snprintf(body, sizeof(body),
                ui_text_get(UI_TEXT_INNOVATION_REVIEW_BODY_FMT),
                event->captured_passes,
                event->comparison.input_delta,
                event->comparison.accepted_delta,
                event->comparison.reject_delta,
                (double)event->comparison.amount_delta);
    memset(&config, 0, sizeof(config));
    config.title = ui_text_get(UI_TEXT_INNOVATION_REVIEW_TITLE);
    config.body = body;
    config.primary_text = ui_text_get(UI_TEXT_INNOVATION_KEEP_COMPARING);
    config.secondary_text = ui_text_get(UI_TEXT_INNOVATION_RESTART);
    config.title_font = &lv_font_instrument_sans_bold_24;
    config.body_font = &lv_font_instrument_sans_medium_16;
    config.button_font = &lv_font_instrument_sans_bold_16;
    config.panel_width = 620;
    config.panel_height = 276;
    config.primary_width = 259;
    config.secondary_width = 245;
    config.accent_color = INNOVATION_RED;
    config.primary_color = INNOVATION_BLUE;
    config.secondary_color = 0x72808B;
    config.primary_action = innovation_prompt_same_bundle_cb;
    config.secondary_action = innovation_prompt_new_bundle_cb;
    lv_modal_dialog_show(&g_prompt, lv_scr_act(), &config);
}

void page_32_innovation_notify_verification_event(
    const multi_pass_capture_event_t *event)
{
    char body[320];

    if (event == NULL || event->kind == MULTI_PASS_CAPTURE_IGNORED) return;
    if (event->kind == MULTI_PASS_CAPTURE_ADD_REQUIRED) {
        innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_ADD_REQUIRED_TITLE),
            ui_text_get(UI_TEXT_INNOVATION_ADD_REQUIRED_BODY),
            ui_text_get(UI_TEXT_INNOVATION_UNDERSTOOD), INNOVATION_RED, NULL);
        return;
    }
    if (event->kind == MULTI_PASS_CAPTURE_MEMORY_ERROR) {
        innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_SAVE_FAILED_TITLE),
            ui_text_get(UI_TEXT_INNOVATION_SAVE_FAILED_BODY),
            ui_text_get(UI_TEXT_INNOVATION_CLOSE), INNOVATION_RED, NULL);
        return;
    }
    if (event->kind == MULTI_PASS_CAPTURE_REVIEW_BUNDLE) {
        innovation_prompt_review(event);
        return;
    }
    if (event->kind == MULTI_PASS_CAPTURE_COMPLETE) {
        lv_snprintf(body, sizeof(body),
                    ui_text_get(event->all_passes_match
                        ? UI_TEXT_INNOVATION_COMPLETE_MATCH_BODY_FMT
                        : UI_TEXT_INNOVATION_COMPLETE_DIFFER_BODY_FMT),
                    event->captured_passes, event->target_passes);
        innovation_prompt_single(event->all_passes_match
                                     ? ui_text_get(UI_TEXT_INNOVATION_COMPLETE_MATCH_TITLE)
                                     : ui_text_get(UI_TEXT_INNOVATION_COMPLETE_DIFFER_TITLE),
                                 body, ui_text_get(UI_TEXT_INNOVATION_VIEW_REPORT),
                                 event->all_passes_match
                                     ? INNOVATION_GREEN : INNOVATION_RED,
                                 innovation_prompt_view_cb);
        return;
    }

    lv_snprintf(body, sizeof(body),
                ui_text_get(UI_TEXT_INNOVATION_CAPTURED_BODY_FMT),
                event->captured_passes,
                event->latest.accepted_pcs,
                event->latest.reject_pcs,
                (long long)event->latest.amount);
    innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_CAPTURED_TITLE), body,
                             ui_text_get(UI_TEXT_INNOVATION_CONTINUE),
                             INNOVATION_BLUE, NULL);
}

static void innovation_transition_set_y(void *object, int32_t value)
{
    lv_obj_t *root = object;

    if (root != NULL && lv_obj_is_valid(root)) {
        lv_obj_set_y(root, (lv_coord_t)value);
    }
}

static lv_obj_t *innovation_transition_target(void)
{
    if (g_transition_snapshot_valid && g_transition_snapshot.image != NULL &&
        lv_obj_is_valid(g_transition_snapshot.image)) {
        return g_transition_snapshot.image;
    }
    /* A missing image must never silently select the live object tree. */
    return NULL;
}

static void innovation_transition_snapshot_release(void)
{
    g_transition_snapshot_valid = false;
    g_transition_snapshot_dirty = true;
    if (g_transition_snapshot.snapshot != NULL ||
        g_transition_snapshot.image != NULL) {
        lv_dma_static_surface_release(&g_transition_snapshot);
    }
}

/* Build/refresh one private surface before exposing any moving pixels.
 * No image decoding or object creation occurs in drag_update/animation. */
static bool innovation_transition_prepare(bool force_refresh)
{
    bool ok;
    uint64_t started = app_clock_monotonic_us();

    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) return false;
    if (g_transition_snapshot_valid && g_transition_snapshot.image != NULL &&
        lv_obj_is_valid(g_transition_snapshot.image) &&
        !g_transition_snapshot_dirty &&
        !force_refresh) return true;
    if (g_transition_prepare_failed &&
        lv_tick_elaps(g_transition_failure_tick) < 5000U) return false;

    innovation_set_hidden(g_transition_snapshot.image, true);
    innovation_set_y_if_changed(g_page.root, 0);
    innovation_set_hidden(g_page.root, false);
    ok = lv_dma_transition_surface_capture(&g_transition_snapshot, g_page.root,
                                           "INNOVATION_TRANSITION");
    g_transition_snapshot_valid = ok;
    g_transition_snapshot_dirty = !ok;
    g_transition_prepare_failed = !ok;
    if (!ok) g_transition_failure_tick = lv_tick_get();
    innovation_set_hidden(g_page.root, true);
    innovation_set_y_if_changed(g_page.root, -400);
    innovation_set_hidden(g_transition_snapshot.image, true);
    innovation_set_y_if_changed(g_transition_snapshot.image, -400);
    if (perf_profile_is_enabled())
        uart_debug_printf("INNOVATION_SURFACE result=%s bytes=%u prepare_us=%u\n",
                          ok ? "ready" : "direct_fallback",
                          lv_dma_snapshot_size(g_transition_snapshot.snapshot),
                          app_clock_elapsed_us32(started, app_clock_monotonic_us()));
    return ok;
}

static void innovation_transition_commit_async(void *user_data)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    (void)user_data;
    g_page_transitioning = false;
    g_handle_gesture.preview_active = false;
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        innovation_set_y_if_changed(g_page.root, 0);
    }
    innovation_set_hidden(g_transition_snapshot.image, true);
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        innovation_set_hidden(g_page.root, false);
        lv_obj_move_foreground(g_page.root);
    }
    if (!ui_manager_adopt_precreated_page(UI_PAGE_INNOVATION_CENTER)) {
        ui_manager_push_page(UI_PAGE_INNOVATION_CENTER);
    }
    innovation_refresh_resume();
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "OPEN_COMMIT",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
}

static void innovation_transition_cancel_async(void *user_data)
{
    (void)user_data;
    g_page_transitioning = false;
    g_handle_gesture.preview_active = false;
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        innovation_set_y_if_changed(g_page.root, -400);
        innovation_set_hidden(g_page.root, true);
    }
    /* Keep the completed surface for the next small pull. release() would
     * unhide its source as a side effect and discard our private allocation. */
    innovation_set_hidden(g_transition_snapshot.image, true);
    innovation_set_y_if_changed(g_transition_snapshot.image, -400);
}

static void innovation_transition_back_async(void *user_data)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    (void)user_data;
    if (ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER) return;
    innovation_back_stop();
    innovation_set_hidden(g_transition_snapshot.image, true);
    innovation_set_hidden(g_page.root, true);
    g_page_transitioning = false;
    if (!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
    /* The outgoing surface and incoming page are invalidated in one handoff. */
    lv_obj_invalidate(lv_scr_act());
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "BACK_COMMIT",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
}

static void innovation_transition_open_ready(lv_anim_t *animation)
{
    (void)animation;
    lv_async_call(innovation_transition_commit_async, NULL);
}

static void innovation_transition_cancel_ready(lv_anim_t *animation)
{
    (void)animation;
    lv_async_call(innovation_transition_cancel_async, NULL);
}

static void innovation_transition_back_ready(lv_anim_t *animation)
{
    (void)animation;
    innovation_set_hidden(g_transition_snapshot.image, true);
    if (perf_profile_is_enabled())
        uart_debug_printf("INNOVATION_BACK event=end elapsed_ms=%u draw_passes=%u\n",
                          lv_tick_elaps(g_back_tick), g_back_frames);
    if(lv_async_call(innovation_transition_back_async, NULL) != LV_RES_OK)
        innovation_transition_back_async(NULL);
}

static void innovation_transition_animate(lv_coord_t destination,
                                          uint32_t duration,
                                          lv_anim_ready_cb_t ready_cb)
{
    lv_obj_t *target = innovation_transition_target();
    lv_anim_t animation;

    if (target == NULL) return;
    lv_anim_del(target, innovation_transition_set_y);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_values(&animation, lv_obj_get_y(target), destination);
    lv_anim_set_time(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, innovation_transition_set_y);
    lv_anim_set_ready_cb(&animation, ready_cb);
    lv_anim_start(&animation);
}

/* A 210 ms timer must not run before the replacement has drawn even once.
 * DRAW_POST_END records CPU draw completion, not panel scanout completion. */
static void innovation_back_draw(lv_event_t *event)
{
    if(g_page_transitioning && lv_event_get_code(event) == LV_EVENT_DRAW_POST_END)
        ++g_back_frames;
}
static void innovation_back_stop(void)
{
    if(g_back_first_frame_timer) lv_timer_del(g_back_first_frame_timer);
    g_back_first_frame_timer = NULL;
    if(g_transition_snapshot.image && lv_obj_is_valid(g_transition_snapshot.image)) {
        lv_anim_del(g_transition_snapshot.image, innovation_transition_set_y);
        lv_obj_remove_event_cb(g_transition_snapshot.image, innovation_back_draw);
    }
}
static void innovation_back_first_frame(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if(ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER) {
        innovation_back_stop();
        return;
    }
    if(!g_back_frames && lv_tick_elaps(g_back_tick) < 700) return;
    lv_timer_del(g_back_first_frame_timer);
    g_back_first_frame_timer = NULL;
    if(!g_back_frames) {
        if(perf_profile_is_enabled())
            uart_debug_printf("INNOVATION_BACK event=no_first_draw fallback=atomic\n");
        innovation_transition_back_async(NULL);
        return;
    }
    if(perf_profile_is_enabled())
        uart_debug_printf("INNOVATION_BACK event=first_draw wait_ms=%u duration_ms=210\n",
                          lv_tick_elaps(g_back_tick));
    lv_obj_t *target = innovation_transition_target();
    if(!target) { innovation_transition_back_async(NULL); return; }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_values(&animation, lv_obj_get_y(target),
                       -lv_obj_get_height(target) - _lv_obj_get_ext_draw_size(target) - 2);
    lv_anim_set_time(&animation, 210);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&animation, innovation_transition_set_y);
    lv_anim_set_ready_cb(&animation, innovation_transition_back_ready);
    lv_anim_start(&animation);
}

static bool innovation_handle_preview_begin(void)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    if (g_page_transitioning || g_handle_gesture.preview_active) return false;

    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) {
        ui_page_32_innovation_create(lv_scr_act());
    }
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) return false;
    innovation_refresh_pause();
    if (innovation_page_refresh()) g_transition_snapshot_dirty = true;
    (void)innovation_transition_prepare(false);
    /* Keep the live tree parked.  When the preloaded snapshot exists, the
     * gesture moves only that one opaque image, so child draw order cannot
     * leak through the transition. */
    innovation_set_y_if_changed(g_page.root, -400);
    if (g_transition_snapshot_valid && g_transition_snapshot.image != NULL &&
        lv_obj_is_valid(g_transition_snapshot.image)) {
        innovation_set_hidden(g_page.root, true);
        innovation_set_y_if_changed(g_transition_snapshot.image, -400);
        innovation_set_hidden(g_transition_snapshot.image, false);
        lv_obj_move_foreground(g_transition_snapshot.image);
    } else {
        /* Resource failure must not revive the known broken multi-object
         * slide. Keep Main intact; release commits a stationary full page. */
        innovation_set_hidden(g_page.root, true);
    }
    if (g_handle_touch != NULL && lv_obj_is_valid(g_handle_touch))
        lv_obj_move_foreground(g_handle_touch);
    g_handle_gesture.preview_active = true;
    if (perf_profile_is_enabled())
        uart_debug_printf("INNOVATION_TRANSITION route=%s event=drag_begin\n",
                          g_transition_snapshot_valid ? "single_surface" : "direct_fallback");
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "PREVIEW_PREPARE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return true;
}

static int innovation_handle_drag_update(lv_indev_t *indev)
{
    lv_point_t point;
    int dy;

    if (indev == NULL || !g_handle_gesture.pressed ||
        !g_handle_gesture.preview_active || g_page_transitioning) {
        return g_handle_gesture.drag_y;
    }

    lv_indev_get_point(indev, &point);
    dy = point.y - g_handle_gesture.start.y;
    if (dy < 0) dy = 0;
    if (dy > 400) dy = 400;
    g_handle_gesture.drag_y = dy;
    /* Apply every touch sample.  The old 24 ms / 2 px throttle visibly lagged
     * behind the finger on the normal touch stream. */
    g_handle_gesture.last_render_tick = lv_tick_get();
    g_handle_gesture.last_render_y = dy;
    if (g_transition_snapshot_valid)
        innovation_set_y_if_changed(g_transition_snapshot.image,
                                    (lv_coord_t)(-400 + dy));
    return dy;
}

static void innovation_handle_drag_finish(lv_indev_t *indev)
{
    uint32_t elapsed;
    int dy;
    bool fast_flick;

    if (!g_handle_gesture.pressed ||
        !g_handle_gesture.preview_active || g_page_transitioning) {
        g_handle_gesture.pressed = false;
        return;
    }

    dy = innovation_handle_drag_update(indev);
    elapsed = lv_tick_elaps(g_handle_gesture.start_tick);
    fast_flick = dy >= 45 && elapsed <= 350U;
    if (perf_profile_is_enabled())
        uart_debug_printf("PULLDOWN event=release dy=%d elapsed_ms=%u decision=%s\n",
                          dy, elapsed, (dy >= 90 || fast_flick) ? "open" : "cancel");
    g_handle_gesture.pressed = false;
    g_page_transitioning = true;

    if (dy >= 90 || fast_flick) {
        g_handle_gesture.opened = true;
        if (g_transition_snapshot_valid)
            innovation_transition_animate(0, 180,
                                          innovation_transition_open_ready);
        else
            lv_async_call(innovation_transition_commit_async, NULL);
    } else {
        g_handle_gesture.opened = false;
        if (g_transition_snapshot_valid)
            innovation_transition_animate(-400, 150,
                                          innovation_transition_cancel_ready);
        else
            lv_async_call(innovation_transition_cancel_async, NULL);
    }
}

static void innovation_handle_drag_cancel(void)
{
    /* A lost/captured press is not a physical release. In particular the raw
     * edge and multi-finger recognizers may take ownership at any distance. */
    if (g_handle_gesture.pressed && perf_profile_is_enabled())
        uart_debug_printf("PULLDOWN event=cancel reason=press_lost dy=%d elapsed_ms=%u\n",
                          g_handle_gesture.drag_y,
                          lv_tick_elaps(g_handle_gesture.start_tick));
    g_handle_gesture.pressed = false;
    if (g_handle_gesture.preview_active && !g_page_transitioning) {
        g_page_transitioning = true;
        innovation_transition_cancel_async(NULL);
    }
}

static void innovation_handle_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;

    if (code == LV_EVENT_PRESS_LOST) {
        innovation_handle_drag_cancel();
        return;
    }
    if (indev == NULL) return;
    if (code == LV_EVENT_PRESSED) {
        if (g_page_transitioning || g_handle_gesture.preview_active) return;
        lv_indev_get_point(indev, &g_handle_gesture.start);
        g_handle_gesture.pressed = true;
        g_handle_gesture.opened = false;
        g_handle_gesture.drag_y = 0;
        g_handle_gesture.start_tick = lv_tick_get();
        g_handle_gesture.last_render_tick = g_handle_gesture.start_tick;
        g_handle_gesture.last_render_y = 0;
        return;
    }
    if (code == LV_EVENT_PRESSING) {
        if (!g_handle_gesture.pressed) return;
        if (!g_handle_gesture.preview_active) {
            lv_indev_get_point(indev, &point);
            if ((point.y - g_handle_gesture.start.y) <
                INNOVATION_PREVIEW_ARM_DY) {
                return;
            }
            if (!innovation_handle_preview_begin()) {
                g_handle_gesture.pressed = false;
                return;
            }
        }
        innovation_handle_drag_update(indev);
        return;
    }
    if (code == LV_EVENT_RELEASED) {
        innovation_handle_drag_finish(indev);
    }
}

void page_32_innovation_handle_attach(lv_obj_t *main_page)
{
    lv_obj_t *handle;

    page_32_innovation_handle_detach();
    if (main_page == NULL) return;

    g_handle_touch = lv_obj_create(main_page);
    lv_obj_remove_style_all(g_handle_touch);
    lv_obj_set_pos(g_handle_touch, 1058, 0);
    lv_obj_set_size(g_handle_touch, 212, 44);
    lv_obj_set_style_bg_opa(g_handle_touch, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(g_handle_touch, LV_OBJ_FLAG_CLICKABLE);
    lv_port_indev_set_drag_obj(g_handle_touch, true);
    lv_obj_clear_flag(g_handle_touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESS_LOST, NULL);

    handle = innovation_box(g_handle_touch, 24, 1, 164, 16, 0xFFFFFF, 8);
    lv_obj_set_style_border_width(handle, 1, 0);
    lv_obj_set_style_border_color(handle, lv_color_hex(0xB8CADA), 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *hint = innovation_label(handle, "PULL DOWN",
        &lv_font_instrument_sans_bold_10, INNOVATION_BLUE);
    lv_obj_set_pos(hint, 27, 2);
    lv_obj_t *chevron = innovation_label(handle, LV_SYMBOL_DOWN,
        &lv_font_montserrat_12, INNOVATION_BLUE);
    lv_obj_set_pos(chevron, 128, 2);
    lv_obj_move_foreground(g_handle_touch);
    /* Main can be constructed while another page is still current (for
     * example during boot/self-test prewarm).  Capturing at that point is
     * rejected by the page guard, and the first user drag would then pay the
     * 120 ms synchronous capture cost.  Retry after Main is active so the
     * snapshot is ready before the gesture starts. */
    if (g_preview_preload_timer == NULL) {
        g_preview_preload_timer = lv_timer_create(
            innovation_preview_preload_timer_cb, 120, NULL);
    }
}

void page_32_innovation_schedule_preload(void)
{
    if (g_preview_preload_timer == NULL) {
        g_preview_preload_timer = lv_timer_create(
            innovation_preview_preload_timer_cb, 120, NULL);
    }
    if (g_preview_preload_timer != NULL) {
        /* The manager calls this immediately after current=MAIN.  Make the
         * existing retry timer due on the next LVGL cycle instead of waiting
         * for its original attach-time period. */
        lv_timer_ready(g_preview_preload_timer);
    }
}

static void innovation_preview_preload_timer_cb(lv_timer_t *timer)
{
    if (timer == NULL || timer != g_preview_preload_timer) return;
    if (ui_manager_get_current_page() != UI_PAGE_MAIN ||
        g_handle_touch == NULL || !lv_obj_is_valid(g_handle_touch) ||
        g_handle_gesture.pressed || g_page_transitioning) {
        return;
    }
    lv_timer_del(timer);
    g_preview_preload_timer = NULL;
    innovation_preview_preload_async(NULL);
}

static void innovation_preview_preload_async(void *user_data)
{
    uint64_t started_us;

    (void)user_data;
    if (ui_manager_get_current_page() != UI_PAGE_MAIN ||
        g_handle_touch == NULL || !lv_obj_is_valid(g_handle_touch)) {
        return;
    }
    /* The page object may already be cached from an earlier visit.  That
     * must not skip creation of the single-image transition surface. */
    started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) {
        ui_page_32_innovation_create(lv_scr_act());
    }
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "PRELOAD_CREATE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) return;
    innovation_refresh_pause();
    if (innovation_page_refresh()) g_transition_snapshot_dirty = true;
    (void)innovation_transition_prepare(false);
}

void page_32_innovation_handle_detach(void)
{
    if (g_preview_preload_timer != NULL) {
        lv_timer_del(g_preview_preload_timer);
        g_preview_preload_timer = NULL;
    }
    if (g_handle_touch != NULL && lv_obj_is_valid(g_handle_touch)) {
        lv_obj_del(g_handle_touch);
    }
    g_handle_touch = NULL;
    memset(&g_handle_gesture, 0, sizeof(g_handle_gesture));
    if (ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER &&
        g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        ui_page_32_innovation_destroy();
    }
}

static void innovation_back_cb(lv_event_t *event)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    (void)event;
    if (g_page_transitioning) return;
    g_page_transitioning = true;
    innovation_refresh_pause();
    innovation_prompt_close();
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) {
        g_page_transitioning = false;
        innovation_refresh_resume();
        return;
    }
    /* Tabs, button states and pass results may have changed while active.
     * Re-capture into the retained allocation, not a stale name-keyed cache. */
    bool have_surface = innovation_transition_prepare(true);
    page_01_main_reveal_for_transition();
    if (have_surface) {
        innovation_set_y_if_changed(g_transition_snapshot.image, 0);
        innovation_set_hidden(g_transition_snapshot.image, false);
        lv_obj_move_foreground(g_transition_snapshot.image);
        innovation_back_stop();
        g_back_frames = 0;
        g_back_tick = lv_tick_get();
        lv_obj_add_event_cb(g_transition_snapshot.image, innovation_back_draw,
                            LV_EVENT_DRAW_POST_END, NULL);
        lv_obj_invalidate(g_transition_snapshot.image);
        g_back_first_frame_timer = lv_timer_create(innovation_back_first_frame, 16, NULL);
        if(!g_back_first_frame_timer) innovation_transition_back_async(NULL);
    } else {
        lv_async_call(innovation_transition_back_async, NULL);
    }
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "BACK_PREPARE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
}

static void innovation_target_minus_cb(lv_event_t *event)
{
    multi_pass_verify_view_t view;

    (void)event;
    multi_pass_verification_get_view(&view);

    if (view.state != MULTI_PASS_VERIFY_IDLE &&
        view.target_passes >= MULTI_PASS_VERIFY_MIN_PASSES &&
        view.target_passes <= MULTI_PASS_VERIFY_MAX_PASSES) {
        g_page.target_passes = view.target_passes;
    }
    if (view.state == MULTI_PASS_VERIFY_RUNNING) return;
    if (g_page.target_passes > MULTI_PASS_VERIFY_MIN_PASSES) {
        g_page.target_passes--;
        innovation_page_refresh();
    }
}

static void innovation_target_plus_cb(lv_event_t *event)
{
    multi_pass_verify_view_t view;

    (void)event;
    multi_pass_verification_get_view(&view);
    if (view.state == MULTI_PASS_VERIFY_RUNNING) return;
    if (g_page.target_passes < MULTI_PASS_VERIFY_MAX_PASSES) {
        g_page.target_passes++;
        innovation_page_refresh();
    }
}

static void innovation_begin_task(void)
{
    if (!multi_pass_verification_start(g_page.target_passes,
                                       machine_state_add_enabled())) {
        return;
    }
    g_page.pending_start_after_add_off = false;
    if (!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
    innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_READY_TITLE),
        ui_text_get(UI_TEXT_INNOVATION_READY_BODY),
        ui_text_get(UI_TEXT_INNOVATION_START_PASS1), INNOVATION_BLUE, NULL);
}

static void innovation_primary_cb(lv_event_t *event)
{
    multi_pass_verify_view_t view;

    (void)event;
    multi_pass_verification_get_view(&view);
    if (view.state == MULTI_PASS_VERIFY_RUNNING) {
        if (!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
        return;
    }
    if (view.state == MULTI_PASS_VERIFY_COMPLETE) {
        multi_pass_verification_cancel();
    }
    if (machine_state_add_enabled()) {
        g_page.pending_start_after_add_off = true;
        if (!setting_service_request_add(false)) {
            g_page.pending_start_after_add_off = false;
            innovation_label_set_if_changed(g_page.instruction_label,
                              ui_text_get(UI_TEXT_INNOVATION_ADD_BUSY));
        } else {
            g_page.add_request_tick = lv_tick_get();
            innovation_label_set_if_changed(g_page.instruction_label,
                              ui_text_get(UI_TEXT_INNOVATION_ADD_WAITING));
        }
        return;
    }
    innovation_begin_task();
}

static void innovation_secondary_cb(lv_event_t *event)
{
    multi_pass_verify_view_t view;

    (void)event;
    multi_pass_verification_get_view(&view);
    if (view.state == MULTI_PASS_VERIFY_RUNNING ||
        view.state == MULTI_PASS_VERIFY_COMPLETE) {
        multi_pass_verification_cancel();
        g_page.pending_start_after_add_off = false;
        innovation_page_refresh();
    }
}

static void innovation_guide_cb(lv_event_t *event)
{
    (void)event;
    innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_GUIDE_TITLE),
        ui_text_get(UI_TEXT_INNOVATION_GUIDE_BODY),
        ui_text_get(UI_TEXT_INNOVATION_GOT_IT), INNOVATION_BLUE, NULL);
}

static void innovation_gesture_button_refresh(void)
{
    bool enabled = gesture_service_enabled();

    if (g_page.gesture_label != NULL && lv_obj_is_valid(g_page.gesture_label)) {
        innovation_label_set_if_changed(g_page.gesture_label,
            ui_text_get(enabled ? UI_TEXT_GESTURE_TOGGLE_ON :
                                  UI_TEXT_GESTURE_TOGGLE_OFF));
    }
    if (g_page.gesture_button != NULL && lv_obj_is_valid(g_page.gesture_button)) {
        lv_color_t color = lv_color_hex(enabled ? INNOVATION_GREEN : 0x8D99A3);
        lv_damped_button_set_palette(g_page.gesture_button, color, color);
    }
}

static void innovation_gesture_toggle_cb(lv_event_t *event)
{
    bool enable;

    LV_UNUSED(event);
    enable = !gesture_service_enabled();
    if (!gesture_service_set_enabled(enable)) return;
    innovation_gesture_button_refresh();
    g_transition_snapshot_dirty = true;
    if (enable) gesture_guide_show();
}

static void innovation_page_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root) ||
        lv_obj_has_flag(g_page.root, LV_OBJ_FLAG_HIDDEN) ||
        ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER) {
        return;
    }
    if (g_page.pending_start_after_add_off &&
        !machine_state_add_enabled()) {
        innovation_begin_task();
        return;
    }
    if (g_page.pending_start_after_add_off &&
        lv_tick_elaps(g_page.add_request_tick) >= 1500U) {
        g_page.pending_start_after_add_off = false;
        innovation_label_set_if_changed(g_page.instruction_label,
            ui_text_get(UI_TEXT_INNOVATION_ADD_TIMEOUT));
        return;
    }
    innovation_page_refresh();
}

static bool innovation_page_refresh(void)
{
    multi_pass_verify_view_t view;
    uint32_t revision;
    char text[256];
    int i;

    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) return false;
    multi_pass_verification_get_view(&view);

    if (view.state != MULTI_PASS_VERIFY_IDLE &&
        view.target_passes >= MULTI_PASS_VERIFY_MIN_PASSES &&
        view.target_passes <= MULTI_PASS_VERIFY_MAX_PASSES) {
        g_page.target_passes = view.target_passes;
    }

    revision = multi_pass_verification_revision();
    if (g_page.render_valid &&
        g_page.rendered_verify_revision == revision &&
        g_page.rendered_target_passes == g_page.target_passes &&
        g_page.rendered_pending_start == g_page.pending_start_after_add_off) {
        return false;
    }

    lv_snprintf(text, sizeof(text), ui_text_get(UI_TEXT_INNOVATION_PASSES_FMT),
                g_page.target_passes);
    innovation_label_set_if_changed(g_page.target_label, text);
    lv_damped_button_set_enabled(g_page.target_minus,
        view.state != MULTI_PASS_VERIFY_RUNNING &&
        g_page.target_passes > MULTI_PASS_VERIFY_MIN_PASSES);
    lv_damped_button_set_enabled(g_page.target_plus,
        view.state != MULTI_PASS_VERIFY_RUNNING &&
        g_page.target_passes < MULTI_PASS_VERIFY_MAX_PASSES);

    if (view.state == MULTI_PASS_VERIFY_IDLE) {
        innovation_label_set_if_changed(g_page.status_label, ui_text_get(UI_TEXT_INNOVATION_READY));
        innovation_set_text_color_if_changed(g_page.status_label,
                                             INNOVATION_BLUE);
        innovation_label_set_if_changed(g_page.instruction_label,
            ui_text_get(UI_TEXT_INNOVATION_IDLE_INSTRUCTION));
        innovation_label_set_if_changed(g_page.primary_label, ui_text_get(UI_TEXT_INNOVATION_START_VERIFY));
        innovation_set_hidden(g_page.secondary_button, true);
    } else if (view.state == MULTI_PASS_VERIFY_RUNNING) {
        lv_snprintf(text, sizeof(text), ui_text_get(UI_TEXT_INNOVATION_ACTIVE_FMT),
                    (unsigned)(view.captured_passes + 1), view.target_passes);
        innovation_label_set_if_changed(g_page.status_label, text);
        innovation_set_text_color_if_changed(g_page.status_label,
                                             INNOVATION_GREEN);
        if (view.awaiting_bundle_confirmation) {
            innovation_label_set_if_changed(g_page.instruction_label,
                ui_text_get(UI_TEXT_INNOVATION_CONFIRM_INSTRUCTION));
        } else if (view.count_armed) {
            innovation_label_set_if_changed(g_page.instruction_label,
                ui_text_get(UI_TEXT_INNOVATION_COUNTING_INSTRUCTION));
        } else {
            innovation_label_set_if_changed(g_page.instruction_label,
                ui_text_get(UI_TEXT_INNOVATION_NEXT_INSTRUCTION));
        }
        innovation_label_set_if_changed(g_page.primary_label, ui_text_get(UI_TEXT_INNOVATION_GO_COUNT));
        innovation_set_hidden(g_page.secondary_button, false);
        innovation_label_set_if_changed(g_page.secondary_label, ui_text_get(UI_TEXT_INNOVATION_CANCEL_TASK));
    } else {
        innovation_label_set_if_changed(g_page.status_label,
                          view.all_passes_match
                              ? ui_text_get(UI_TEXT_INNOVATION_COMPLETE_MATCH)
                              : ui_text_get(UI_TEXT_INNOVATION_COMPLETE_DIFFER));
        innovation_set_text_color_if_changed(g_page.status_label,
            view.all_passes_match ? INNOVATION_GREEN : INNOVATION_RED);
        innovation_label_set_if_changed(g_page.instruction_label,
            view.all_passes_match
                ? ui_text_get(UI_TEXT_INNOVATION_MATCH_INSTRUCTION)
                : ui_text_get(UI_TEXT_INNOVATION_DIFFER_INSTRUCTION));
        innovation_label_set_if_changed(g_page.primary_label, ui_text_get(UI_TEXT_INNOVATION_NEW_VERIFY));
        innovation_set_hidden(g_page.secondary_button, false);
        innovation_label_set_if_changed(g_page.secondary_label, ui_text_get(UI_TEXT_INNOVATION_CLEAR_RESULT));
    }

    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        const multi_pass_snapshot_t *snapshot = view.passes[i];
        uint32_t card_color = INNOVATION_CARD;

        if (i >= g_page.target_passes) {
            innovation_set_hidden(g_page.pass_cards[i], true);
            continue;
        }
        innovation_set_hidden(g_page.pass_cards[i], false);
        lv_snprintf(text, sizeof(text), ui_text_get(UI_TEXT_INNOVATION_PASS_FMT), i + 1);
        innovation_label_set_if_changed(g_page.pass_titles[i], text);
        if (snapshot != NULL && snapshot->valid) {
            const multi_pass_comparison_t *comparison = view.comparisons[i];

            lv_snprintf(text, sizeof(text), ui_text_get(UI_TEXT_INNOVATION_SN_FMT),
                        snapshot->accepted_pcs, snapshot->reject_pcs,
                        (long long)snapshot->amount, snapshot->serial_count);
            if (i == 0 || (comparison != NULL && comparison->exact_match)) {
                card_color = 0xF1FBF5;
            } else {
                card_color = 0xFFF3F2;
            }
        } else {
            innovation_label_set_if_changed(g_page.pass_values[i], ui_text_get(UI_TEXT_INNOVATION_WAITING));
        }
        if (snapshot != NULL && snapshot->valid) {
            innovation_label_set_if_changed(g_page.pass_values[i], text);
        }
        innovation_set_bg_if_changed(g_page.pass_cards[i], card_color);
    }

    if (!view.latest_comparison.available) {
        innovation_label_set_if_changed(g_page.comparison_label,
            ui_text_get(UI_TEXT_INNOVATION_COMPARISON_DEFAULT));
    } else {
        const multi_pass_comparison_t *cmp = &view.latest_comparison;
        lv_snprintf(text, sizeof(text),
            ui_text_get(UI_TEXT_INNOVATION_COMPARISON_FMT),
            cmp->exact_match ? ui_text_get(UI_TEXT_INNOVATION_LATEST_MATCH)
                             : ui_text_get(UI_TEXT_INNOVATION_LATEST_DIFFER),
            cmp->accepted_delta, cmp->reject_delta, cmp->input_delta,
            (double)cmp->amount_delta, cmp->denomination_diff_count,
            cmp->serial_missing_count, cmp->serial_extra_count,
            cmp->first_missing_serial[0] ? "\n" : "",
            cmp->first_missing_serial[0] ? cmp->first_missing_serial : "");
        innovation_label_set_if_changed(g_page.comparison_label, text);
    }
    if (g_page.detail_summary != NULL) {
        const multi_pass_comparison_t *cmp = &view.latest_comparison;
        if (!cmp->available) {
            innovation_label_set_if_changed(g_page.detail_summary,
                ui_text_get(UI_TEXT_INNOVATION_COMPARISON_DEFAULT));
        } else {
            lv_snprintf(text, sizeof(text),
                "%s\n\n%s   %d\n%s   %+d\n%s   -%d / +%d%s%s",
                cmp->exact_match ? ui_text_get(UI_TEXT_INNOVATION_LATEST_MATCH)
                                 : ui_text_get(UI_TEXT_INNOVATION_LATEST_DIFFER),
                ui_text_get(UI_TEXT_INNOVATION_DETAIL_DENOM), cmp->denomination_diff_count,
                ui_text_get(UI_TEXT_INNOVATION_DETAIL_REJECT), cmp->reject_delta,
                ui_text_get(UI_TEXT_INNOVATION_DETAIL_SERIAL), cmp->serial_missing_count,
                cmp->serial_extra_count, cmp->first_missing_serial[0] ? "\n" : "",
                cmp->first_missing_serial[0] ? cmp->first_missing_serial : "");
            innovation_label_set_if_changed(g_page.detail_summary, text);
        }
    }

    g_page.rendered_verify_revision = revision;
    g_page.rendered_target_passes = g_page.target_passes;
    g_page.rendered_pending_start = g_page.pending_start_after_add_off;
    g_page.render_valid = true;
    return true;
}

void ui_page_32_innovation_create(lv_obj_t *parent)
{
    lv_obj_t *backdrop;
    lv_obj_t *header;
    lv_obj_t *rail;
    lv_obj_t *content;
    lv_obj_t *overview;
    lv_obj_t *details;
    lv_obj_t *card;
    lv_obj_t *label;
    int i;
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        innovation_set_y_if_changed(g_page.root, 0);
        innovation_set_hidden(g_page.root, false);
        lv_obj_move_foreground(g_page.root);
        innovation_page_refresh();
        innovation_refresh_resume();
        if (started_us != 0) {
            perf_profile_report_event_us("INNOVATION", "PAGE_RESUME",
                app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
        }
        return;
    }
    ui_page_32_innovation_destroy();
    memset(&g_page, 0, sizeof(g_page));
    g_page.target_passes = MULTI_PASS_VERIFY_MIN_PASSES;

    g_page.root = innovation_box(parent != NULL ? parent : lv_scr_act(),
                                 0, 0, 1280, 400,
                                 INNOVATION_BG, 0);
    /* Keep the transition background as an explicit first child.  Relying on
     * the root style background allowed the GE/partial-refresh path to flush
     * child panels before the cyan background, exposing the old page through
     * the gaps during a slow pull-down. */
    lv_obj_set_style_bg_opa(g_page.root, LV_OPA_TRANSP, 0);
    backdrop = innovation_box(g_page.root, 0, 0, 1280, 400,
                              INNOVATION_BG, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_CLICKABLE |
                               LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(backdrop);
    header = innovation_box(g_page.root, 16, 12, 1248, 50,
                            INNOVATION_CARD, 18);
    label = innovation_label(header, ui_text_get(UI_TEXT_INNOVATION_CENTER_TITLE),
                             &lv_font_instrument_sans_bold_20,
                             INNOVATION_TEXT);
    lv_obj_set_pos(label, 22, 13);
    label = innovation_label(header, ui_text_get(UI_TEXT_INNOVATION_LAYER),
                             &lv_font_instrument_sans_medium_12,
                             INNOVATION_MUTED);
    lv_obj_set_pos(label, 358, 18);
    innovation_button(header, 1018, 7, 94, 36, 0x8D99A3,
                      ui_text_get(UI_TEXT_INNOVATION_GUIDE), innovation_guide_cb);
    g_page.gesture_button = innovation_button(header, 824, 7, 184, 36,
        gesture_service_enabled() ? INNOVATION_GREEN : 0x8D99A3,
        ui_text_get(gesture_service_enabled() ? UI_TEXT_GESTURE_TOGGLE_ON :
                                               UI_TEXT_GESTURE_TOGGLE_OFF),
        innovation_gesture_toggle_cb);
    g_page.gesture_label = lv_damped_button_get_label(g_page.gesture_button);
    lv_nav_button_create(header, 1122, 7, 108, 36, innovation_back_cb, NULL);

    rail = innovation_box(g_page.root, 16, 72, 238, 314,
                          INNOVATION_CARD, 22);
    label = innovation_label(rail, ui_text_get(UI_TEXT_INNOVATION_FEATURES),
                             &lv_font_instrument_sans_bold_14,
                             INNOVATION_MUTED);
    lv_obj_set_pos(label, 18, 18);
    g_page.feature_scroll = lv_obj_create(rail);
    lv_obj_remove_style_all(g_page.feature_scroll);
    lv_obj_set_pos(g_page.feature_scroll, 12, 44);
    lv_obj_set_size(g_page.feature_scroll, 214, 248);
    lv_obj_set_flex_flow(g_page.feature_scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(g_page.feature_scroll, 0, 0);
    lv_obj_set_style_pad_row(g_page.feature_scroll, 10, 0);
    lv_obj_set_scroll_dir(g_page.feature_scroll, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(g_page.feature_scroll, LV_SCROLL_SNAP_START);
    lv_obj_set_scrollbar_mode(g_page.feature_scroll, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(g_page.feature_scroll, 3, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(g_page.feature_scroll, lv_color_hex(INNOVATION_BLUE), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(g_page.feature_scroll, LV_OPA_60, LV_PART_SCROLLBAR);
    innovation_feature_card(g_page.feature_scroll, 1,
        ui_text_get(UI_TEXT_INNOVATION_MULTI_PASS),
        ui_text_get(UI_TEXT_INNOVATION_ACTIVE_FEATURE), true);
    innovation_feature_card(g_page.feature_scroll, 2,
        ui_text_get(UI_TEXT_INNOVATION_TASK_WORKFLOWS),
        ui_text_get(UI_TEXT_INNOVATION_RESERVED), false);
    innovation_feature_card(g_page.feature_scroll, 3,
        ui_text_get(UI_TEXT_INNOVATION_DEVICE_INSIGHTS),
        ui_text_get(UI_TEXT_INNOVATION_RESERVED), false);
    g_page.feature_hint_top = innovation_box(rail, 100, 39, 38, 3,
                                             INNOVATION_BLUE, 2);
    g_page.feature_hint_bottom = innovation_box(rail, 100, 300, 38, 3,
                                                INNOVATION_BLUE, 2);
    lv_obj_add_event_cb(g_page.feature_scroll, innovation_feature_scroll_cb,
                        LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(g_page.feature_scroll, innovation_feature_scroll_cb,
                        LV_EVENT_SCROLL_END, NULL);

    content = innovation_box(g_page.root, 266, 72, 998, 314,
                             INNOVATION_CARD, 22);
    label = innovation_label(content, ui_text_get(UI_TEXT_INNOVATION_MULTI_PASS),
                             &lv_font_instrument_sans_bold_24,
                             INNOVATION_TEXT);
    lv_obj_set_pos(label, 22, 16);
    g_page.status_label = innovation_label(content, ui_text_get(UI_TEXT_INNOVATION_READY),
                                           &lv_font_instrument_sans_bold_12,
                                           INNOVATION_BLUE);
    lv_obj_set_pos(g_page.status_label, 292, 24);
    g_page.instruction_label = innovation_label(content, "",
        &lv_font_instrument_sans_medium_14, INNOVATION_MUTED);
    lv_obj_set_pos(g_page.instruction_label, 22, 53);
    lv_obj_set_size(g_page.instruction_label, 665, 42);
    lv_label_set_long_mode(g_page.instruction_label, LV_LABEL_LONG_WRAP);

    label = innovation_label(content, ui_text_get(UI_TEXT_INNOVATION_TARGET),
                             &lv_font_instrument_sans_bold_12,
                             INNOVATION_MUTED);
    lv_obj_set_pos(label, 710, 19);
    g_page.target_minus = innovation_button(content, 710, 44, 42, 36, INNOVATION_BLUE,
                      "-", innovation_target_minus_cb);
    g_page.target_label = innovation_label(content, "",
        &lv_font_instrument_sans_bold_16, INNOVATION_TEXT);
    lv_obj_set_pos(g_page.target_label, 762, 53);
    g_page.target_plus = innovation_button(content, 864, 44, 42, 36, INNOVATION_BLUE,
                      "+", innovation_target_plus_cb);

    g_page.content_pager = lv_content_pager_create(content, 16, 96, 966, 210, 2);
    overview = lv_content_pager_get_page(g_page.content_pager, 0);
    details = lv_content_pager_get_page(g_page.content_pager, 1);

    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        lv_coord_t x = 22 + i * 132;

        g_page.pass_cards[i] = innovation_box(overview, x, 0, 120, 94,
                                              0xF4F6F8, 16);
        g_page.pass_titles[i] = innovation_label(g_page.pass_cards[i], "",
            &lv_font_instrument_sans_bold_12, INNOVATION_MUTED);
        lv_obj_set_pos(g_page.pass_titles[i], 12, 10);
        g_page.pass_values[i] = innovation_label(g_page.pass_cards[i],
            ui_text_get(UI_TEXT_INNOVATION_WAITING), &lv_font_manrope_bold_14, INNOVATION_TEXT);
        lv_obj_set_pos(g_page.pass_values[i], 12, 36);
        lv_obj_set_style_text_line_space(g_page.pass_values[i], 3, 0);
    }

    card = innovation_box(overview, 22, 102, 650, 70, 0xF4F6F8, 14);
    g_page.comparison_label = innovation_label(card, "",
        &lv_font_instrument_sans_medium_12, INNOVATION_TEXT);
    lv_obj_set_pos(g_page.comparison_label, 14, 10);
    lv_obj_set_size(g_page.comparison_label, 622, 50);
    lv_label_set_long_mode(g_page.comparison_label, LV_LABEL_LONG_WRAP);

    g_page.primary_button = innovation_button(overview, 694, 102, 258, 40,
        INNOVATION_BLUE, ui_text_get(UI_TEXT_INNOVATION_START_VERIFY), innovation_primary_cb);
    g_page.primary_label = lv_damped_button_get_label(g_page.primary_button);
    g_page.secondary_button = innovation_button(overview, 694, 148, 258, 26,
        0x8D99A3, ui_text_get(UI_TEXT_INNOVATION_CANCEL_TASK), innovation_secondary_cb);
    g_page.secondary_label = lv_damped_button_get_label(g_page.secondary_button);

    label = innovation_label(details, ui_text_get(UI_TEXT_INNOVATION_DETAIL_TITLE),
                             &lv_font_instrument_sans_bold_20, INNOVATION_TEXT);
    lv_obj_set_pos(label, 22, 4);
    label = innovation_label(details, ui_text_get(UI_TEXT_INNOVATION_DETAIL_HINT),
                             &lv_font_instrument_sans_medium_12, INNOVATION_MUTED);
    lv_obj_set_pos(label, 22, 31);
    card = innovation_box(details, 22, 58, 930, 113, 0xF4F6F8, 14);
    g_page.detail_summary = innovation_label(card, "",
        &lv_font_instrument_sans_medium_14, INNOVATION_TEXT);
    lv_obj_set_pos(g_page.detail_summary, 16, 12);
    lv_obj_set_size(g_page.detail_summary, 898, 90);
    lv_label_set_long_mode(g_page.detail_summary, LV_LABEL_LONG_WRAP);

    g_page.refresh_timer = lv_timer_create(innovation_page_refresh_timer_cb,
                                           250, NULL);
    innovation_page_refresh();
    lv_obj_update_layout(g_page.feature_scroll);
    innovation_feature_hint_refresh();
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "PAGE_CREATE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
}

bool ui_page_32_innovation_resume(void)
{
    uint64_t started_us;
    bool refreshed;

    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) {
        return false;
    }

    started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    innovation_set_y_if_changed(g_page.root, 0);
    innovation_set_hidden(g_page.root, false);
    lv_obj_move_foreground(g_page.root);
    refreshed = innovation_page_refresh();
    innovation_refresh_resume();
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION",
            refreshed ? "RESUME_DIRTY" : "RESUME_CLEAN",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return true;
}

void ui_page_32_innovation_suspend(void)
{
    innovation_back_stop();
    innovation_refresh_pause();
    innovation_prompt_close();
    gesture_guide_close(false);
    innovation_set_hidden(g_transition_snapshot.image, true);
    g_transition_snapshot_dirty = true;
    g_page_transitioning = false;
    g_handle_gesture.pressed = false;
    g_handle_gesture.preview_active = false;
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        innovation_set_hidden(g_page.root, true);
    }
}

void ui_page_32_innovation_destroy(void)
{
    innovation_back_stop();
    lv_modal_dialog_destroy(&g_prompt);
    gesture_guide_close(false);
    innovation_transition_snapshot_release();
    g_transition_prepare_failed = false;
    if (g_page.refresh_timer != NULL) {
        lv_timer_del(g_page.refresh_timer);
        g_page.refresh_timer = NULL;
    }
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        lv_obj_del(g_page.root);
    }
    memset(&g_page, 0, sizeof(g_page));
    g_page.target_passes = MULTI_PASS_VERIFY_MIN_PASSES;
}
