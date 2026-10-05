#include "un260/lv_system/ui_i18n.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_37_touch_calibration.h"
#include "lv_page_manager.h"
#include "settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/app_service/touch_calibration_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/gesture/gesture_service.h"
#include <stdio.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *status, *description, *toggle, *start_button, *save_button;
static lv_obj_t *overlay, *target, *step_label, *time_label, *sample_hint;
static lv_timer_t *monitor;
static touch_calibration_snapshot_t shown;
static bool shown_valid;

static void label_update(lv_obj_t *label, const char *text)
{
    if (strcmp(lv_label_get_text(label), text)) lv_label_set_text(label, text);
}

static bool sampling(touch_calibration_phase_t phase)
{
    return phase == TOUCH_CAL_COLLECT || phase == TOUCH_CAL_VALIDATE;
}

static const char *error_text(touch_calibration_error_t error)
{
    switch (error) {
    case TOUCH_CAL_ERROR_UNAVAILABLE: return UI_N_("Touch input is unavailable. Check the touchscreen connection.");
    case TOUCH_CAL_ERROR_TOO_SHORT: return UI_N_("Hold the target briefly, then lift your finger.");
    case TOUCH_CAL_ERROR_UNSTABLE: return UI_N_("Keep your finger still on the target, then lift.");
    case TOUCH_CAL_ERROR_MULTITOUCH: return UI_N_("Use one finger. Lift all fingers before trying again.");
    case TOUCH_CAL_ERROR_INPUT_LOST: return UI_N_("Touch input was interrupted. Lift your finger and try again.");
    case TOUCH_CAL_ERROR_FIT: return UI_N_("The samples could not form a reliable calibration. Try again.");
    case TOUCH_CAL_ERROR_VALIDATION: return UI_N_("Accuracy check failed. The previous calibration is unchanged.");
    case TOUCH_CAL_ERROR_TIMEOUT: return UI_N_("Calibration timed out. The previous calibration is unchanged.");
    case TOUCH_CAL_ERROR_STORE: return UI_N_("Save could not be confirmed. The current setting remains active. Tap Save to retry.");
    default: return UI_N_("Touch each target with one finger, hold briefly, then lift.");
    }
}

static bool gesture(gesture_action_t action)
{
    (void)action;
    if (!frame.root || !lv_obj_is_visible(frame.root)) return false;
    touch_calibration_snapshot_t s;
    touch_calibration_get_snapshot(&s);
    return sampling(s.phase) || s.phase == TOUCH_CAL_READY || s.phase == TOUCH_CAL_SAVING;
}

static void refresh(lv_timer_t *timer);

static void cancel(lv_event_t *event)
{
    (void)event;
    if (!touch_calibration_cancel()) return;
    shown_valid = false;
    refresh(NULL);
}

static void back(lv_event_t *event)
{
    (void)event;
    touch_calibration_snapshot_t s;
    touch_calibration_get_snapshot(&s);
    if (s.phase == TOUCH_CAL_SAVING) return;
    (void)touch_calibration_cancel();
    ui_manager_pop_page();
}

static void begin(lv_event_t *event)
{
    (void)event;
    if (app_command_runtime_count_start_busy()) return;
    (void)touch_calibration_begin(1280, 400, app_clock_uptime_ms());
    shown_valid = false;
    refresh(NULL);
}

static void save(lv_event_t *event)
{
    (void)event;
    (void)touch_calibration_save();
    shown_valid = false;
    refresh(NULL);
}

static void switch_enabled(lv_event_t *event)
{
    (void)event;
    touch_calibration_snapshot_t s;
    touch_calibration_get_snapshot(&s);
    (void)touch_calibration_set_enabled(!s.enabled);
    shown_valid = false;
    refresh(NULL);
}

static void show_sampling(const touch_calibration_snapshot_t *s)
{
    if (!overlay) {
        overlay = lv_settings_box(frame.root, 0, 0, 1280, 400, 0xF1F4F5);
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_settings_label(overlay, ui_tr("Touch calibration"), 24, 16,
            &lv_font_instrument_sans_semibold_24, 0x1D2B34);
        step_label = lv_settings_label(overlay, "", 470, 21,
            &lv_font_instrument_sans_medium_18, 0x1462CC);
        lv_obj_set_width(step_label, 490);
        time_label = lv_settings_label(overlay, "", 1020, 21,
            &lv_font_instrument_sans_medium_16, 0x586B78);
        lv_obj_set_width(time_label, 230);
        lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_settings_box(overlay, 24, 54, 1232, 1, 0xE3E9ED);
        target = lv_settings_box(overlay, 0, 0, 50, 50, 0xFFFFFF);
        lv_obj_set_style_radius(target, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(target, lv_color_hex(0x1462CC), 0);
        lv_obj_set_style_border_width(target, 2, 0);
        lv_settings_box(target, 5, 23, 40, 2, 0x1462CC);
        lv_settings_box(target, 23, 5, 2, 40, 0x1462CC);
        lv_obj_t *center = lv_settings_box(target, 21, 21, 8, 8, 0x1462CC);
        lv_obj_set_style_radius(center, LV_RADIUS_CIRCLE, 0);
        sample_hint = lv_settings_label(overlay, "", 24, 359,
            &lv_font_instrument_sans_medium_14, 0x586B78);
        lv_obj_set_width(sample_hint, 1040);
        lv_label_set_long_mode(sample_hint, LV_LABEL_LONG_DOT);
        lv_settings_button(overlay, 1110, 348, 146, 44, ui_tr("Cancel"), false, cancel, NULL);
    }
    if (lv_obj_get_x(target) != s->target_x - 25 || lv_obj_get_y(target) != s->target_y - 25)
        lv_obj_set_pos(target, s->target_x - 25, s->target_y - 25);
    char text[128];
    snprintf(text, sizeof(text), ui_tr(s->phase == TOUCH_CAL_VALIDATE ?
        "Accuracy check %u / %u" : "Calibration point %u / %u"),
        s->target_index + 1, s->target_count);
    label_update(step_label, text);
    snprintf(text, sizeof(text), ui_tr("%u seconds remaining"), (s->remaining_ms + 999) / 1000);
    label_update(time_label, text);
    label_update(sample_hint, ui_tr(error_text(s->error)));
}

static void refresh(lv_timer_t *timer)
{
    if (!frame.root || (timer && !lv_obj_is_visible(frame.root))) return;
    touch_calibration_poll(app_clock_uptime_ms());
    touch_calibration_snapshot_t s;
    touch_calibration_get_snapshot(&s);
    bool busy = s.phase == TOUCH_CAL_SAVING;
    bool active = sampling(s.phase) || s.phase == TOUCH_CAL_READY || touch_calibration_can_save();
    settings_detail_action_block(frame.back, busy ? UI_N_("Wait for the calibration to finish saving.") : NULL);
    settings_detail_action_block(start_button, !s.available ? UI_N_("Touch input is unavailable. Check the touchscreen connection.") :
        busy || active ? UI_N_("Finish or cancel the current calibration first.") :
        app_command_runtime_count_start_busy() ? UI_N_("Stop counting before calibrating the touchscreen.") : NULL);
    settings_detail_action_block(toggle, busy || active ? UI_N_("Finish or cancel the current calibration first.") :
        !s.calibrated ? UI_N_("Complete a calibration before enabling it.") : NULL);
    settings_detail_action_block(save_button, !touch_calibration_can_save() ?
        UI_N_("Complete all calibration and accuracy checks before saving.") : NULL);
    if (lv_obj_has_state(toggle, LV_STATE_CHECKED) != s.enabled)
        lv_settings_toggle_set(toggle, s.enabled, false);
    if (sampling(s.phase)) show_sampling(&s);
    else if (overlay) {
        lv_obj_del(overlay);
        overlay = target = step_label = time_label = sample_hint = NULL;
    }
    if (shown_valid && s.phase == shown.phase && s.error == shown.error &&
        s.load_error == shown.load_error && s.enabled == shown.enabled &&
        s.calibrated == shown.calibrated) return;
    shown = s;
    shown_valid = true;
    const char *state = s.phase == TOUCH_CAL_SAVING ? UI_N_("Saving") :
        s.phase == TOUCH_CAL_READY ? UI_N_("Accuracy verified") :
        s.phase == TOUCH_CAL_FAILED ? UI_N_("Calibration needs attention") :
        s.calibrated ? s.enabled ? UI_N_("Calibration enabled") : UI_N_("Calibration disabled") : UI_N_("Factory touch coordinates");
    lv_label_set_text(status, ui_tr(state));
    const char *text = s.phase == TOUCH_CAL_READY ? UI_N_("All checks passed. Save to use this calibration after restart and power loss.") :
        s.phase == TOUCH_CAL_SAVING ? UI_N_("Saving to local storage. Keep power connected.") :
        s.error != TOUCH_CAL_ERROR_NONE ? error_text(s.error) :
        s.load_error != TOUCH_CAL_ERROR_NONE ? UI_N_("Saved calibration is unavailable or invalid. Factory coordinates are active; calibrate again.") :
        UI_N_("Five calibration points and three accuracy checks. Cancel or timeout keeps the previous setting.");
    lv_label_set_text(description, ui_tr(text));
    lv_label_set_text(frame.message, ui_tr(s.phase == TOUCH_CAL_SAVED ?
        "Touch calibration saved to local storage." :
        s.phase == TOUCH_CAL_READY ? "Save applies the verified calibration. Back discards this draft." :
        "Calibration stays on this device during software upgrades."));
}

static void deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != frame.root) return;
    (void)touch_calibration_cancel();
    gesture_service_clear_page_policy(UI_PAGE_TOUCH_CALIBRATION);
    if (monitor) lv_timer_del(monitor);
    monitor = NULL;
    status = description = toggle = start_button = save_button = NULL;
    overlay = target = step_label = time_label = sample_hint = NULL;
    memset(&frame, 0, sizeof(frame));
    shown_valid = false;
}

void ui_page_37_touch_calibration_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {.title = ui_tr("Touch calibration"), .icon = "EntryDisplay", .back = back};
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_add_event_cb(frame.root, deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *left = lv_settings_panel(frame.body, 0, 0, 620, 300);
    lv_settings_label(left, ui_tr("Touch alignment"), 24, 20,
        &lv_font_instrument_sans_medium_18, 0x586B78);
    status = lv_settings_label(left, "", 24, 52, &lv_font_instrument_sans_semibold_24, 0x1D2B34);
    lv_obj_set_width(status, 572);
    lv_label_set_long_mode(status, LV_LABEL_LONG_WRAP);
    description = lv_settings_label(left, "", 24, 96, &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(description, 572);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    lv_settings_box(left, 24, 198, 572, 1, 0xE3E9ED);
    lv_settings_label(left, ui_tr("Use saved calibration"), 24, 226,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    toggle = lv_settings_toggle(left, 532, 220, false, NULL, NULL);
    lv_settings_action_guard_init(toggle);
    lv_obj_add_event_cb(toggle, switch_enabled, LV_EVENT_CLICKED, NULL);
    lv_obj_t *right = lv_settings_panel(frame.body, 636, 0, 596, 300);
    lv_settings_label(right, ui_tr("Before you start"), 24, 20,
        &lv_font_instrument_sans_semibold_22, 0x1D2B34);
    const char *steps[] = {UI_N_("1  Stop counting and clean the screen."),
        UI_N_("2  Touch the center of each blue target."),
        UI_N_("3  Hold briefly, lift, then follow the next target."),
        UI_N_("4  Pass the accuracy checks and tap Save.")};
    for (unsigned i = 0; i < 4; ++i) {
        lv_obj_t *line = lv_settings_label(right, ui_tr(steps[i]), 24, 70 + i * 49,
            &lv_font_instrument_sans_medium_16, 0x1D2B34);
        lv_obj_set_width(line, 548);
        lv_label_set_long_mode(line, LV_LABEL_LONG_WRAP);
    }
    start_button = lv_settings_button(frame.footer, 0, 0, 172, 46, ui_tr("Calibrate"), false, begin, NULL);
    save_button = lv_settings_button(frame.footer, 0, 0, 132, 46, ui_tr("Save"), true, save, NULL);
    gesture_service_set_page_policy(UI_PAGE_TOUCH_CALIBRATION, NULL, gesture);
    shown_valid = false;
    monitor = lv_timer_create(refresh, 100, NULL);
    if (!monitor) {
        settings_detail_action_block(start_button, UI_N_("Touch input is unavailable. Check the touchscreen connection."));
        settings_detail_action_block(toggle, UI_N_("Touch input is unavailable. Check the touchscreen connection."));
        settings_detail_action_block(save_button, UI_N_("Touch input is unavailable. Check the touchscreen connection."));
        lv_label_set_text(status, ui_tr("Unavailable"));
        return;
    }
    refresh(NULL);
}

void ui_page_37_touch_calibration_destroy(void)
{
    if (frame.root) lv_obj_del(frame.root);
}
