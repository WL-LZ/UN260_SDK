#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#include "page_30_set_factory.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/ui_text.h"
#include "un260/app_service/ui_factory_reset_service.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static lv_settings_frame_t frame;
static lv_obj_t *reset_button;
static lv_timer_t *reboot_timer;

static void update_controls(void)
{
    if (!frame.root) return;
    ui_factory_reset_phase_t phase = ui_factory_reset_get_phase();
    const char *reason = phase == UI_FACTORY_RESET_RESTART_REQUIRED ?
        UI_N_("Restart the device to complete the reset.") :
        phase != UI_FACTORY_RESET_IDLE ? UI_N_("Reset is in progress. Keep power connected.") : NULL;
    settings_detail_action_block(frame.back, reason);
    settings_detail_action_block(reset_button,
        phase == UI_FACTORY_RESET_LOCAL_FAILED || phase == UI_FACTORY_RESET_RESTART_REQUIRED ? NULL : reason);
    lv_label_set_text(lv_obj_get_child(reset_button, 0),
        phase == UI_FACTORY_RESET_LOCAL_FAILED ? ui_tr("Retry local reset") :
        phase == UI_FACTORY_RESET_RESTART_REQUIRED ? ui_tr("Restart") :
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_START));
    lv_settings_action_style(reset_button, phase == UI_FACTORY_RESET_RESTART_REQUIRED ?
        LV_SETTINGS_ACTION_PRIMARY : LV_SETTINGS_ACTION_DESTRUCTIVE);
    const char *message = "";
    if (phase == UI_FACTORY_RESET_WAIT_CONTROLLER) message = ui_tr("Waiting for controller...");
    else if (phase == UI_FACTORY_RESET_SAVING) message = ui_tr("Saving UI factory defaults. Keep power connected.");
    else if (phase == UI_FACTORY_RESET_LOCAL_FAILED) message = ui_tr("Controller reset confirmed. Retry the local reset before restarting.");
    else if (phase == UI_FACTORY_RESET_RESTART_REQUIRED) message = ui_tr("Factory defaults saved. Restart to apply them.");
    lv_label_set_text(frame.message, message);
}

static void confirm_start(void *user_data)
{
    (void)user_data;
    if (ui_factory_reset_get_phase() != UI_FACTORY_RESET_IDLE) return;
    if (!ui_factory_reset_request()) {
        const char *reason = ui_factory_reset_blocker();
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.factory", UI_N_("Reset not sent"),
            reason ? reason : UI_N_("Try again."));
        return;
    }
    ui_notice_post_text(UI_NOTICE_PROGRESS, "settings.factory", UI_N_("Factory reset"), UI_N_("Waiting for controller..."));
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
    if (reboot_timer || ui_factory_reset_get_phase() != UI_FACTORY_RESET_RESTART_REQUIRED) return;
    /* Retain the established clear-detail / delayed system-reboot sequence. */
    uint8_t payload = 0x01;
    (void)settings_detail_send_command(0x3B, &payload, 1);
    reboot_timer = lv_timer_create(reboot_tick, 1000, NULL);
    if (reboot_timer) {
        lv_timer_set_repeat_count(reboot_timer, 1);
        ui_notice_post_text(UI_NOTICE_PROGRESS, "settings.factory", UI_N_("Restarting"), NULL);
    } else {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING, ui_tr("Restart not scheduled"),
            ui_tr("Please try again."), ui_tr("Restart"), NULL, confirm_reboot, NULL, NULL);
    }
}

static void confirm_retry(void *user_data)
{
    (void)user_data;
    if (!ui_factory_reset_retry_local()) return;
    update_controls();
    ui_notice_post_text(UI_NOTICE_PROGRESS, "settings.factory", UI_N_("Factory reset"),
        UI_N_("Saving UI factory defaults. Keep power connected."));
}

static void show_local_result(bool success)
{
    if (!frame.root || reboot_timer) return;
    ui_notice_clear("settings.factory");
    if (success) {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_SUCCESS,
            ui_tr("Factory defaults saved"),
            ui_tr("Controller and UI settings are reset. Restart to apply UI defaults."),
            ui_tr("Restart"), NULL, confirm_reboot, NULL, NULL);
    } else {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING,
            ui_tr("UI reset not saved"),
            ui_tr("Controller reset confirmed. Retry the local reset before restarting."),
            ui_tr("Retry local reset"), NULL, confirm_retry, NULL, NULL);
    }
}

static void start(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (ui_factory_reset_get_phase() == UI_FACTORY_RESET_LOCAL_FAILED) {
        confirm_retry(NULL);
        return;
    }
    if (ui_factory_reset_get_phase() == UI_FACTORY_RESET_RESTART_REQUIRED) {
        show_local_result(true);
        return;
    }
    if (ui_factory_reset_blocks_navigation()) return;
    settings_detail_dialog_show_ex(SETTINGS_DIALOG_DESTRUCTIVE,
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_CONFIRM_TITLE),
        ui_tr("Restore controller and UI preferences, including PIN and touch calibration?"
              "\nOperator identities, imported photos and business records are kept. Restart is required."),
        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CONFIRM),
        ui_text_get(UI_TEXT_SETTINGS_DIALOG_CANCEL), confirm_start, NULL, NULL);
}

static void back(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || ui_factory_reset_blocks_navigation()) return;
    settings_detail_dialog_hide();
    ui_manager_pop_page();
}

static bool gesture(gesture_action_t action)
{
    (void)action;
    return frame.root && lv_obj_is_visible(frame.root) &&
        (ui_factory_reset_blocks_navigation() || settings_detail_overlay_is_open());
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
    factory_impact(impact, 18, "Settings", ui_tr("Controller and UI preferences"),
        ui_tr("Display, PIN, language, touch calibration and controls return to defaults."));
    factory_impact(impact, 94, "Layers", ui_tr("Operator identities and records"),
        ui_tr("Identities, photos and records are kept. Counting presets return to defaults."));
    factory_impact(impact, 170, "Clock", ui_tr("Restart required"),
        ui_tr("Restart after both controller and local settings are confirmed."));
    lv_obj_t *action = lv_settings_panel(frame.body, 748, 0, 484, 242);
    lv_obj_set_style_border_color(action, lv_color_hex(0xDFC8C5), 0);
    lv_settings_label(action, ui_tr("Restore factory settings"), 24, 22,
        &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_obj_t *description = lv_settings_label(action,
        ui_tr("Restores controller and UI preferences."
              "\nUI defaults apply after restarting."), 24, 66,
        &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(description, 436);
    lv_label_set_long_mode(description, LV_LABEL_LONG_WRAP);
    reset_button = lv_settings_button(action, 24, 172, 436, 46,
        ui_text_get(UI_TEXT_SETTINGS_FACTORY_START), true, start, NULL);
    lv_settings_action_style(reset_button, LV_SETTINGS_ACTION_DESTRUCTIVE);
    lv_obj_set_width(frame.message, 1120);
    update_controls();
    gesture_service_set_page_policy(UI_PAGE_FACTORY_SETTING, NULL, gesture);
    ui_factory_reset_phase_t phase = ui_factory_reset_get_phase();
    if (phase == UI_FACTORY_RESET_RESTART_REQUIRED || phase == UI_FACTORY_RESET_LOCAL_FAILED)
        show_local_result(phase == UI_FACTORY_RESET_RESTART_REQUIRED);
}

void ui_page_30_set_factory_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_FACTORY_SETTING);
    settings_detail_dialog_hide();
    if (frame.root) lv_obj_del(frame.root);
    memset(&frame, 0, sizeof(frame));
    reset_button = NULL;
    /* Service and an explicitly confirmed restart outlive the transient view. */
}

void ui_page_30_set_factory_on_reply(uint8_t res)
{
    update_controls();
    if (res != 0x01) {
        ui_notice_post_text(UI_NOTICE_ERROR, "settings.factory", UI_N_("Reset rejected"),
            UI_N_("Check controller status before retrying."));
    } else {
        ui_notice_post_text(UI_NOTICE_PROGRESS, "settings.factory", UI_N_("Factory reset"),
            UI_N_("Saving UI factory defaults. Keep power connected."));
    }
}

void ui_page_30_set_factory_on_timeout(void)
{
    update_controls();
    ui_notice_post_text(UI_NOTICE_WARNING, "settings.factory", UI_N_("Reset unconfirmed"),
        UI_N_("No reply. Check controller status before retrying."));
}

void ui_page_30_set_factory_on_local_result(bool success)
{
    update_controls();
    show_local_result(success);
}
