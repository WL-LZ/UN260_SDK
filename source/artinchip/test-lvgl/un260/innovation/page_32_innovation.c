#include "page_32_innovation.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/app_service/setting_service.h"
#include "un260/font/manrope_fonts.h"
#include "un260/lv_components/lv_content_pager.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_system/ui_text.h"
#include "un260/gesture/gesture_guide.h"
#include "un260/gesture/gesture_service.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/app_clock.h"
#include "aic_ui/perf_stats.h"

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
static lv_obj_t *g_prompt;
static lv_obj_t *g_transition_surface;
static lv_obj_t *g_transition_title;
static innovation_handle_gesture_t g_handle_gesture;
static bool g_page_transitioning;
static void innovation_preview_preload_async(void *user_data);
static lv_obj_t *innovation_label(lv_obj_t *parent, const char *text,
                                  const lv_font_t *font, uint32_t color);
static void innovation_label_set_if_changed(lv_obj_t *label, const char *text);
static lv_obj_t *innovation_box(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                lv_coord_t width, lv_coord_t height,
                                uint32_t color, lv_coord_t radius);

static void innovation_transition_surface_delete(void)
{
    if (g_transition_surface != NULL && lv_obj_is_valid(g_transition_surface)) {
        lv_obj_del(g_transition_surface);
    }
    g_transition_surface = NULL;
    g_transition_title = NULL;
}

static void innovation_transition_surface_park(void)
{
    if (g_transition_surface == NULL || !lv_obj_is_valid(g_transition_surface)) {
        g_transition_surface = NULL;
        g_transition_title = NULL;
        return;
    }
    lv_anim_del(g_transition_surface, NULL);
    lv_obj_set_y(g_transition_surface, -400);
}

static lv_obj_t *innovation_transition_surface_create(lv_coord_t y)
{
    lv_obj_t *header;
    lv_obj_t *label;
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    if (g_transition_surface != NULL && lv_obj_is_valid(g_transition_surface)) {
        if (g_transition_title != NULL && lv_obj_is_valid(g_transition_title)) {
            innovation_label_set_if_changed(
                g_transition_title,
                ui_text_get(UI_TEXT_INNOVATION_CENTER_TITLE));
        }
        lv_obj_set_y(g_transition_surface, y);
        lv_obj_move_foreground(g_transition_surface);
        return g_transition_surface;
    }
    g_transition_surface = NULL;
    g_transition_title = NULL;
    g_transition_surface = innovation_box(lv_scr_act(), 0, y, 1280, 400,
                                           INNOVATION_BG, 0);
    header = innovation_box(g_transition_surface, 16, 12, 1248, 50,
                            INNOVATION_CARD, 18);
    label = innovation_label(header, ui_text_get(UI_TEXT_INNOVATION_CENTER_TITLE),
                             &lv_font_instrument_sans_bold_20,
                             INNOVATION_TEXT);
    g_transition_title = label;
    lv_obj_set_pos(label, 22, 13);
    innovation_box(g_transition_surface, 16, 72, 238, 314,
                   INNOVATION_CARD, 22);
    innovation_box(g_transition_surface, 266, 72, 998, 314,
                   INNOVATION_CARD, 22);
    lv_obj_clear_flag(g_transition_surface, LV_OBJ_FLAG_CLICKABLE |
                                            LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(g_transition_surface);
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "SURFACE_CREATE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    return g_transition_surface;
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
    if (g_prompt != NULL && lv_obj_is_valid(g_prompt)) {
        lv_obj_del(g_prompt);
    }
    g_prompt = NULL;
}

static bool innovation_page_refresh(void);
static void innovation_prompt_single(const char *title, const char *body,
                                     const char *button_text, uint32_t accent,
                                     lv_event_cb_t callback);

static void innovation_feature_hint_refresh(void)
{
    lv_coord_t top;
    lv_coord_t bottom;
    if (g_page.feature_scroll == NULL) return;
    top = lv_obj_get_scroll_y(g_page.feature_scroll);
    bottom = lv_obj_get_scroll_bottom(g_page.feature_scroll);
    if (g_page.feature_hint_top != NULL) {
        if (top > 2) lv_obj_clear_flag(g_page.feature_hint_top, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_page.feature_hint_top, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_page.feature_hint_bottom != NULL) {
        if (bottom > 2) lv_obj_clear_flag(g_page.feature_hint_bottom, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_page.feature_hint_bottom, LV_OBJ_FLAG_HIDDEN);
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

static void innovation_prompt_dismiss_cb(lv_event_t *event)
{
    (void)event;
    innovation_prompt_close();
}

static void innovation_prompt_view_cb(lv_event_t *event)
{
    (void)event;
    innovation_prompt_close();
    if (ui_manager_get_current_page() != UI_PAGE_INNOVATION_CENTER) {
        ui_manager_push_page(UI_PAGE_INNOVATION_CENTER);
    } else {
        innovation_page_refresh();
    }
}

static void innovation_prompt_same_bundle_cb(lv_event_t *event)
{
    multi_pass_capture_kind_t result;

    (void)event;
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

static void innovation_prompt_new_bundle_cb(lv_event_t *event)
{
    (void)event;
    innovation_prompt_close();
    if (multi_pass_verification_restart_from_latest()) {
        innovation_prompt_single(ui_text_get(UI_TEXT_INNOVATION_NEW_BASELINE_TITLE),
            ui_text_get(UI_TEXT_INNOVATION_NEW_BASELINE_BODY),
            ui_text_get(UI_TEXT_INNOVATION_CONTINUE), INNOVATION_BLUE, NULL);
    }
}

static lv_obj_t *innovation_prompt_create(const char *title, const char *body,
                                          uint32_t accent)
{
    lv_obj_t *panel;
    lv_obj_t *label;
    lv_obj_t *bar;

    innovation_prompt_close();
    g_prompt = innovation_box(lv_scr_act(), 0, 0, 1280, 400, 0x101820, 0);
    lv_obj_set_style_bg_opa(g_prompt, LV_OPA_50, 0);
    lv_obj_move_foreground(g_prompt);

    panel = innovation_box(g_prompt, 330, 62, 620, 276,
                           INNOVATION_CARD, 24);
    lv_obj_set_style_shadow_width(panel, 35, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_20, 0);
    bar = innovation_box(panel, 28, 24, 8, 54, accent, 4);
    (void)bar;

    label = innovation_label(panel, title,
                             &lv_font_instrument_sans_bold_24,
                             INNOVATION_TEXT);
    lv_obj_set_pos(label, 54, 24);
    label = innovation_label(panel, body,
                             &lv_font_instrument_sans_medium_16,
                             INNOVATION_MUTED);
    lv_obj_set_pos(label, 54, 67);
    lv_obj_set_size(label, 525, 104);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return panel;
}

static void innovation_prompt_single(const char *title, const char *body,
                                     const char *button_text, uint32_t accent,
                                     lv_event_cb_t callback)
{
    lv_obj_t *panel = innovation_prompt_create(title, body, accent);

    innovation_button(panel, 390, 208, 190, 48, accent, button_text,
                      callback != NULL ? callback : innovation_prompt_dismiss_cb);
}

static void innovation_prompt_review(const multi_pass_capture_event_t *event)
{
    char body[256];
    lv_obj_t *panel;

    lv_snprintf(body, sizeof(body),
                ui_text_get(UI_TEXT_INNOVATION_REVIEW_BODY_FMT),
                event->captured_passes,
                event->comparison.input_delta,
                event->comparison.accepted_delta,
                event->comparison.reject_delta,
                (double)event->comparison.amount_delta);
    panel = innovation_prompt_create(ui_text_get(UI_TEXT_INNOVATION_REVIEW_TITLE), body,
                                     INNOVATION_RED);
    innovation_button(panel, 54, 208, 245, 48, 0x72808B,
                      ui_text_get(UI_TEXT_INNOVATION_RESTART), innovation_prompt_new_bundle_cb);
    innovation_button(panel, 321, 208, 259, 48, INNOVATION_BLUE,
                      ui_text_get(UI_TEXT_INNOVATION_KEEP_COMPARING), innovation_prompt_same_bundle_cb);
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

static void innovation_transition_commit_async(void *user_data)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    (void)user_data;
    g_page_transitioning = false;
    g_handle_gesture.preview_active = false;
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        lv_obj_set_y(g_page.root, 0);
        lv_obj_clear_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_page.root);
    }
    if (!ui_manager_adopt_precreated_page(UI_PAGE_INNOVATION_CENTER)) {
        ui_manager_push_page(UI_PAGE_INNOVATION_CENTER);
    }
    innovation_transition_surface_park();
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
    innovation_transition_surface_park();
}

static void innovation_transition_back_async(void *user_data)
{
    uint64_t started_us = perf_profile_is_enabled() ?
        app_clock_monotonic_us() : 0;

    (void)user_data;
    g_page_transitioning = false;
    if (!ui_manager_pop_page()) ui_manager_switch(UI_PAGE_MAIN);
    innovation_transition_surface_park();
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
    lv_async_call(innovation_transition_back_async, NULL);
}

static void innovation_transition_animate(lv_coord_t destination,
                                          uint32_t duration,
                                          lv_anim_ready_cb_t ready_cb)
{
    lv_anim_t animation;

    if (g_transition_surface == NULL || !lv_obj_is_valid(g_transition_surface)) return;
    lv_anim_del(g_transition_surface, innovation_transition_set_y);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, g_transition_surface);
    lv_anim_set_values(&animation, lv_obj_get_y(g_transition_surface), destination);
    lv_anim_set_time(&animation, duration);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, innovation_transition_set_y);
    lv_anim_set_ready_cb(&animation, ready_cb);
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
    innovation_page_refresh();
    lv_obj_add_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
    if (innovation_transition_surface_create(-400) == NULL) return false;
    g_handle_gesture.preview_active = true;
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
    if (dy == 0 || dy == 400 ||
        (lv_tick_elaps(g_handle_gesture.last_render_tick) >= 24U &&
         (dy - g_handle_gesture.last_render_y >= 2 ||
          g_handle_gesture.last_render_y - dy >= 2))) {
        g_handle_gesture.last_render_tick = lv_tick_get();
        g_handle_gesture.last_render_y = dy;
        if (g_transition_surface != NULL && lv_obj_is_valid(g_transition_surface))
            lv_obj_set_y(g_transition_surface, (lv_coord_t)(-400 + dy));
    }
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
    g_handle_gesture.pressed = false;
    g_page_transitioning = true;

    if (dy >= 90 || fast_flick) {
        g_handle_gesture.opened = true;
        innovation_transition_animate(0, 180,
                                      innovation_transition_open_ready);
    } else {
        g_handle_gesture.opened = false;
        innovation_transition_animate(-400, 150,
                                      innovation_transition_cancel_ready);
    }
}

static void innovation_handle_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;

    if (indev == NULL) return;
    if (code == LV_EVENT_PRESSED) {
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
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
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
    lv_obj_add_flag(g_handle_touch, LV_OBJ_FLAG_CLICKABLE |
                                    LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(g_handle_touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(g_handle_touch, innovation_handle_event_cb,
                        LV_EVENT_PRESS_LOST, NULL);

    handle = innovation_box(g_handle_touch, 50, 4, 112, 7,
                            INNOVATION_CARD, 999);
    lv_obj_set_style_shadow_color(handle, lv_color_hex(0x87939C), 0);
    lv_obj_set_style_shadow_width(handle, 7, 0);
    lv_obj_set_style_shadow_opa(handle, LV_OPA_30, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_foreground(g_handle_touch);
    lv_async_call(innovation_preview_preload_async, NULL);
}

static void innovation_preview_preload_async(void *user_data)
{
    uint64_t started_us;

    (void)user_data;
    if (ui_manager_get_current_page() != UI_PAGE_MAIN ||
        g_handle_touch == NULL || !lv_obj_is_valid(g_handle_touch) ||
          (g_page.root != NULL && lv_obj_is_valid(g_page.root))) {
        return;
    }
    started_us = perf_profile_is_enabled() ? app_clock_monotonic_us() : 0;
    ui_page_32_innovation_create(lv_scr_act());
    if (started_us != 0) {
        perf_profile_report_event_us("INNOVATION", "PRELOAD_CREATE",
            app_clock_elapsed_us32(started_us, app_clock_monotonic_us()));
    }
    if (g_page.root == NULL || !lv_obj_is_valid(g_page.root)) return;
    innovation_refresh_pause();
    lv_obj_set_y(g_page.root, -400);
    lv_obj_add_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
    innovation_transition_surface_create(-400);
}

void page_32_innovation_handle_detach(void)
{
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
    if (innovation_transition_surface_create(0) == NULL) {
        g_page_transitioning = false;
        innovation_refresh_resume();
        return;
    }
    lv_obj_add_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
    page_01_main_reveal_for_transition();
    lv_obj_move_foreground(g_transition_surface);
    innovation_transition_animate(-400, 210,
                                  innovation_transition_back_ready);
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
        lv_obj_set_style_bg_color(g_page.gesture_button,
            lv_color_hex(enabled ? INNOVATION_GREEN : 0x8D99A3),
            LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void innovation_gesture_toggle_cb(lv_event_t *event)
{
    bool enable;

    LV_UNUSED(event);
    enable = !gesture_service_enabled();
    if (!gesture_service_set_enabled(enable)) return;
    innovation_gesture_button_refresh();
    if (enable) gesture_guide_show();
}

static void innovation_page_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
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
        lv_obj_set_style_text_color(g_page.status_label,
                                    lv_color_hex(INNOVATION_BLUE), 0);
        innovation_label_set_if_changed(g_page.instruction_label,
            ui_text_get(UI_TEXT_INNOVATION_IDLE_INSTRUCTION));
        innovation_label_set_if_changed(g_page.primary_label, ui_text_get(UI_TEXT_INNOVATION_START_VERIFY));
        lv_obj_add_flag(g_page.secondary_button, LV_OBJ_FLAG_HIDDEN);
    } else if (view.state == MULTI_PASS_VERIFY_RUNNING) {
        lv_snprintf(text, sizeof(text), ui_text_get(UI_TEXT_INNOVATION_ACTIVE_FMT),
                    (unsigned)(view.captured_passes + 1), view.target_passes);
        innovation_label_set_if_changed(g_page.status_label, text);
        lv_obj_set_style_text_color(g_page.status_label,
                                    lv_color_hex(INNOVATION_GREEN), 0);
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
        lv_obj_clear_flag(g_page.secondary_button, LV_OBJ_FLAG_HIDDEN);
        innovation_label_set_if_changed(g_page.secondary_label, ui_text_get(UI_TEXT_INNOVATION_CANCEL_TASK));
    } else {
        innovation_label_set_if_changed(g_page.status_label,
                          view.all_passes_match
                              ? ui_text_get(UI_TEXT_INNOVATION_COMPLETE_MATCH)
                              : ui_text_get(UI_TEXT_INNOVATION_COMPLETE_DIFFER));
        lv_obj_set_style_text_color(g_page.status_label,
            lv_color_hex(view.all_passes_match
                             ? INNOVATION_GREEN : INNOVATION_RED), 0);
        innovation_label_set_if_changed(g_page.instruction_label,
            view.all_passes_match
                ? ui_text_get(UI_TEXT_INNOVATION_MATCH_INSTRUCTION)
                : ui_text_get(UI_TEXT_INNOVATION_DIFFER_INSTRUCTION));
        innovation_label_set_if_changed(g_page.primary_label, ui_text_get(UI_TEXT_INNOVATION_NEW_VERIFY));
        lv_obj_clear_flag(g_page.secondary_button, LV_OBJ_FLAG_HIDDEN);
        innovation_label_set_if_changed(g_page.secondary_label, ui_text_get(UI_TEXT_INNOVATION_CLEAR_RESULT));
    }

    for (i = 0; i < MULTI_PASS_VERIFY_MAX_PASSES; i++) {
        const multi_pass_snapshot_t *snapshot = view.passes[i];
        uint32_t card_color = INNOVATION_CARD;

        if (i >= g_page.target_passes) {
            lv_obj_add_flag(g_page.pass_cards[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(g_page.pass_cards[i], LV_OBJ_FLAG_HIDDEN);
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
        lv_obj_set_style_bg_color(g_page.pass_cards[i],
                                  lv_color_hex(card_color), 0);
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
        lv_obj_set_y(g_page.root, 0);
        lv_obj_clear_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
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
    innovation_button(header, 1122, 7, 108, 36, INNOVATION_BLUE,
                      ui_text_get(UI_TEXT_INNOVATION_BACK), innovation_back_cb);

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
    lv_obj_clear_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
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
    innovation_refresh_pause();
    innovation_prompt_close();
    gesture_guide_close(false);
    g_page_transitioning = false;
    g_handle_gesture.pressed = false;
    g_handle_gesture.preview_active = false;
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        lv_obj_add_flag(g_page.root, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_page_32_innovation_destroy(void)
{
    innovation_prompt_close();
    gesture_guide_close(false);
    if (g_page.refresh_timer != NULL) {
        lv_timer_del(g_page.refresh_timer);
        g_page.refresh_timer = NULL;
    }
    if (g_page.root != NULL && lv_obj_is_valid(g_page.root)) {
        lv_obj_del(g_page.root);
    }
    innovation_transition_surface_delete();
    memset(&g_page, 0, sizeof(g_page));
    g_page.target_passes = MULTI_PASS_VERIFY_MIN_PASSES;
}
