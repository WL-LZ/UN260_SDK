#include "page_30_set_factory.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/ui_text.h"
#include "un260/app_service/setting_service.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *reset_button;
static lv_timer_t *reboot_timer;
static bool pending, reboot_required;

static void update_controls(void)
{
    if (!frame.root) return;
    if (pending || reboot_required) {
        settings_detail_action_block(reset_button, reboot_required ? "Restart the device to complete the reset." : "Reset is in progress. Keep power connected.");
        settings_detail_action_block(frame.back, reboot_required ? "Restart the device to complete the reset." : "Reset is in progress. Keep power connected.");
    } else {
        settings_detail_action_block(reset_button, NULL);
        settings_detail_action_block(frame.back, NULL);
    }
}

static void confirm_start(void *user_data)
{
    (void)user_data;
    if (pending || reboot_required) return;
    if (!setting_service_request_factory_reset()) {
        if (frame.message) lv_label_set_text(frame.message, "Could not send the reset request. Please try again.");
        return;
    }
    pending = true;
    if (frame.message) lv_label_set_text(frame.message, "Reset requested - waiting for controller.");
    update_controls();
}

static void reboot_tick(lv_timer_t *timer)
{
    (void)timer;
    reboot_timer = NULL;
    ui_upgrade_service_reboot();
}

static void confirm_reboot(void *user_data)
{
    (void)user_data;
    if (reboot_timer) return;
    /* Retain the established clear-detail / delayed system-reboot sequence. */
    uint8_t payload = 0x01;
    (void)settings_detail_send_command(0x3B, &payload, 1);
    reboot_timer = lv_timer_create(reboot_tick, 1000, NULL);
    if (reboot_timer) {
        lv_timer_set_repeat_count(reboot_timer, 1);
        if (frame.message) lv_label_set_text(frame.message, "Restarting...");
    } else {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING, "Restart not scheduled",
            "Please try again.", "Restart", NULL, confirm_reboot, NULL, NULL);
    }
}

static void start(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending || reboot_required) return;
    settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_TITLE),
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_CONTENT),
        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CANCEL), confirm_start, NULL, NULL);
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || pending || reboot_required) return;
    settings_detail_dialog_hide();
    ui_manager_pop_page();
}

static bool gesture(gesture_action_t action)
{
    (void)action;
    return frame.root && lv_obj_is_visible(frame.root) &&
        (pending || reboot_required || settings_detail_overlay_is_open());
}

static void factory_impact(lv_obj_t *parent, int y, const char *icon,
                           const char *title, const char *description)
{
    lv_settings_icon(parent, icon, 20, y + 7);
    lv_settings_label(parent, title, 64, y,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_obj_t *text = lv_settings_label(parent, description, 64, y + 29,
        &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(text, 616);
    lv_label_set_long_mode(text, LV_LABEL_LONG_WRAP);
}

void ui_page_30_set_factory_create(lv_obj_t *parent)
{
    if (frame.root) return;
    lv_settings_header_t header = {
        .title = ui_text_get(UI_TEXT_SETTINGS_FACTORY_TITLE), .icon = "Settings", .back = back
    };
    frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *impact = lv_settings_panel(frame.body, 0, 0, 724, 242);
    factory_impact(impact, 18, "Settings", "Controller settings",
        "Restores the controller's factory configuration.");
    factory_impact(impact, 94, "Layers", "PRE totals and details",
        "A clear request is sent before the device restarts.");
    factory_impact(impact, 170, "Clock", "Restart required",
        "Restart is offered after the controller confirms the reset.");
    lv_obj_t *action = lv_settings_panel(frame.body, 748, 0, 484, 242);
    lv_obj_set_style_border_color(action, lv_color_hex(0xDFC8C5), 0);
    lv_settings_label(action, "Restore factory settings", 24, 22,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_obj_t *description = lv_settings_label(action,
        "This replaces the current controller configuration."
        "\nReview the effects before continuing.", 24, 66,
        &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(description, 436);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    reset_button = lv_settings_button(action, 24, 172, 436, 46,
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_START), true, start, NULL);
    lv_settings_action_style(reset_button, LV_SETTINGS_ACTION_DESTRUCTIVE);
    lv_obj_set_width(frame.message, 940);
    lv_label_set_text(frame.message, pending ? "Waiting for controller." : "No changes are made until you confirm the reset.");
    update_controls();
    gesture_service_set_page_policy(UI_PAGE_FACTORY_SETTING, NULL, gesture);
    if (reboot_required && !reboot_timer) {
        lv_label_set_text(frame.message, "Reset confirmed. Restart required.");
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_SUCCESS,
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_TITLE),
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_CONTENT),
            ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM), NULL, confirm_reboot, NULL, NULL);
    }
}

void ui_page_30_set_factory_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_FACTORY_SETTING);
    settings_detail_dialog_hide();
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    reset_button = NULL;
    /* An explicitly confirmed system restart outlives the transient view. */
}

void ui_page_30_set_factory_on_reply(uint8_t res)
{
    pending = false;
    reboot_required = res == 0x01;
    update_controls();
    if (!frame.root) return;
    if (reboot_required) {
        lv_label_set_text(frame.message, "Reset confirmed. Restart required.");
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_SUCCESS,
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_TITLE),
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_SUCCESS_CONTENT),
            ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM), NULL, confirm_reboot, NULL, NULL);
    } else {
        lv_label_set_text(frame.message, "Reset was not confirmed.");
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_FAIL_TITLE),
            ui_text_get(UI_TEXT_SETTINGS_FACTORY_FAIL_CONTENT),
            ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM), NULL, NULL, NULL, NULL);
    }
}
