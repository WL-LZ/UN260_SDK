#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_components/ui_notice.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "upgrade_page_runtime.h"

#include <string.h>
#include "un260/protocol/protocol_send.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/gesture/gesture_service.h"

/* Only the active page owns gesture policy. Request state survives a forced
 * page teardown; hiding a view does not cancel a controller upgrade. */
static upgrade_page_runtime_t *visible_runtime;

static void paint(upgrade_page_runtime_t *runtime)
{
    if (!runtime || !runtime->root) return;
    bool terminal = runtime->has_last_status &&
                    runtime->config->is_terminal(runtime->last_status);
    bool success = terminal && (runtime->last_status == 0x03 || runtime->last_status == 0x04);
    unsigned step = terminal || runtime->timed_out ? 2 :
                    runtime->has_last_status && runtime->last_status == 0x02 ? 1 : 0;
    uint32_t ink = runtime->timed_out ? 0x946321 : terminal ? (success ? 0x287953 : 0xB1393E) : 0x1D2B34;
    const char *phase = runtime->blocked ? ui_tr("Another update is active") : runtime->timed_out ? ui_tr("Result not received") :
        terminal ? (success ? ui_tr("Update complete") : ui_tr("Update not completed")) :
        runtime->waiting ? (step == 1 ? ui_tr("Installing update") : ui_tr("Waiting for the controller")) :
        ui_tr("Ready to request update");
    const char *status = runtime->blocked ?
        ui_tr("Wait for the other update to finish. Its result may still be unknown.") : runtime->timed_out ?
        ui_tr("No reply arrived within the expected time. The update may still be running.") :
        runtime->has_last_status ? ui_tr(runtime->config->status_text(runtime->last_status)) :
        runtime->waiting ? ui_tr("The request has been sent. Keep power connected.") :
        ui_tr("Start when the correct update file is ready for the controller.");
    lv_label_set_text(runtime->phase_label, phase);
    lv_obj_set_style_text_color(runtime->phase_label, lv_color_hex(ink), 0);
    lv_label_set_text(runtime->status_label, status);
    lv_obj_set_style_text_color(runtime->status_label, lv_color_hex(0x586B78), 0);
    for (unsigned i = 0; i < 3; ++i) {
        bool active = i == step && (runtime->waiting || terminal || runtime->timed_out);
        lv_obj_set_style_bg_color(runtime->steps[i], lv_color_hex(active ? 0xEDF4FF : 0xF1F4F5), 0);
        lv_obj_set_style_border_width(runtime->steps[i], active ? 1 : 0, 0);
        lv_obj_set_style_border_color(runtime->steps[i], lv_color_hex(0xBDD0EA), 0);
        lv_obj_t *label = lv_obj_get_child(runtime->steps[i], 0);
        lv_obj_set_style_text_color(label, lv_color_hex(active ? 0x1462CC : 0x586B78), 0);
    }
    if (runtime->waiting || runtime->timed_out || runtime->blocked) settings_detail_action_block(runtime->start, runtime->waiting ? UI_N_("The update is in progress. Keep power connected.") : runtime->timed_out ? UI_N_("The update result is unknown. Check the controller before starting another update.") : UI_N_("Resolve the update error before starting another update."));
    else settings_detail_action_block(runtime->start, NULL);
    if (runtime->waiting && !runtime->timed_out) settings_detail_action_block(runtime->back, runtime->waiting ? UI_N_("The update is in progress. Keep power connected.") : runtime->timed_out ? UI_N_("The update result is unknown. Check the controller before starting another update.") : UI_N_("Resolve the update error before starting another update."));
    else settings_detail_action_block(runtime->back, NULL);
    lv_label_set_text(runtime->message, runtime->timed_out ?
        ui_tr("Keep power connected. Leaving this page does not stop the update.") :
        success ? ui_tr("The update file has been kept.") : ui_tr("Keep power connected. Do not remove the update media."));
}

static void timeout_cb(lv_timer_t *timer)
{
    upgrade_page_runtime_t *runtime = timer ? timer->user_data : NULL;
    if (!runtime || !runtime->config) return;
    upgrade_session_owner_t owner = upgrade_session_owner();
    bool blocked = owner != UPGRADE_SESSION_NONE &&
        (owner != runtime->config->owner || !runtime->waiting);
    if (blocked != runtime->blocked) { runtime->blocked = blocked; paint(runtime); }
    if (!runtime->waiting || runtime->timed_out) return;
    if (lv_tick_elaps(runtime->wait_start_tick) < runtime->config->timeout_ms) return;
    runtime->timed_out = true;
    ui_notice_post_text(UI_NOTICE_WARNING,"upgrade.controller",UI_N_("Update result unknown"),UI_N_("Keep power connected. The update may still be running."));
    paint(runtime);
}

void upgrade_page_runtime_init(upgrade_page_runtime_t *runtime,
                               const upgrade_page_runtime_config_t *config,
                               lv_obj_t *status_label)
{
    if (!runtime) return;
    runtime->config = config;
    runtime->status_label = status_label;
    if (!runtime->timeout_timer)
        runtime->timeout_timer = lv_timer_create(timeout_cb, 200, runtime);
}

bool upgrade_page_runtime_start(upgrade_page_runtime_t *runtime)
{
    const uint8_t payload = 0x01;
    if (!runtime || !runtime->config || runtime->waiting || runtime->timed_out) return false;
    if (!upgrade_session_begin(runtime->config->owner)) {
        runtime->blocked = true;
        ui_notice_post_text(UI_NOTICE_WARNING,"upgrade.controller",UI_N_("Update unavailable"),UI_N_("Another update is active."));
        paint(runtime);
        return false;
    }
    runtime->blocked = false;
    if (!protocol_send_is_ready() || protocol_send(runtime->config->command, &payload, 1) < 0) {
        upgrade_session_end(runtime->config->owner);
        ui_notice_post_text(UI_NOTICE_ERROR,"upgrade.controller",UI_N_("Update request not sent"),UI_N_("Check controller connection."));
        return false;
    }
    runtime->waiting = true;
    ui_notice_post_text(UI_NOTICE_PROGRESS,"upgrade.controller",UI_N_("Updating controller"),UI_N_("Keep power connected."));
    runtime->timed_out = false;
    runtime->has_last_status = false;
    runtime->wait_start_tick = lv_tick_get();
    paint(runtime);
    return true;
}

void upgrade_page_runtime_handle_reply(upgrade_page_runtime_t *runtime, uint8_t status)
{
    if (!runtime || !runtime->config || !runtime->waiting) return;
    bool terminal = runtime->config->is_terminal(status);
    if (!terminal && status != 0x01 && status != 0x02) return;
    runtime->has_last_status = true;
    runtime->last_status = status;
    runtime->wait_start_tick = lv_tick_get();
    runtime->timed_out = false;
    if (terminal) {
        runtime->waiting = false;
        upgrade_session_end(runtime->config->owner);
        bool success=status==0x03||status==0x04;
        ui_notice_post_text(success?UI_NOTICE_SUCCESS:UI_NOTICE_ERROR,"upgrade.controller",success?UI_N_("Update complete"):UI_N_("Update not completed"),runtime->config->status_text(status));
    }
    paint(runtime);
}

static void leave(void *data)
{
    upgrade_page_runtime_t *runtime = data;
    if (runtime->home_requested) {
        ui_manager_suspend_to_home();
    } else ui_manager_pop_page();
}

static void request_back(upgrade_page_runtime_t *runtime, bool home)
{
    if (!runtime || settings_detail_overlay_is_open()) return;
    runtime->home_requested = home;
    if (runtime->timed_out) {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING, ui_tr("Leave update status?"),
            ui_tr("The result is unknown. Leaving does not stop the update. Keep the machine powered on."),
            ui_tr("Leave page"), ui_tr("Keep waiting"), leave, NULL, runtime);
    } else if (!runtime->waiting) leave(runtime);
}

static bool upgrade_gesture(gesture_action_t action)
{
    if (!visible_runtime) return false;
    if (settings_detail_overlay_is_open()) return true;
    if (visible_runtime->timed_out &&
        (action == GESTURE_ACTION_HOME || action == GESTURE_ACTION_EXIT_PAGE)) {
        request_back(visible_runtime, action == GESTURE_ACTION_HOME);
        return true;
    }
    return visible_runtime->waiting;
}

static void back_cb(lv_event_t *event)
{
    request_back(lv_event_get_user_data(event), false);
}
static void start_cb(lv_event_t *event)
{
    upgrade_page_runtime_start(lv_event_get_user_data(event));
}

void upgrade_page_runtime_create(upgrade_page_runtime_t *runtime, lv_obj_t *parent,
    const upgrade_page_runtime_config_t *config, const char *title, const char *installed_version)
{
    if (!runtime || runtime->root) return;
    const lv_settings_header_t header = {.title = title, .subtitle = ui_tr("Data / Upgrade"),
        .back = back_cb, .user_data = runtime};
    lv_settings_frame_t frame = lv_settings_frame_create(parent, &header);
    runtime->root = frame.root;
    runtime->back = frame.back;
    runtime->message = frame.message;
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *prepare = lv_settings_panel(frame.body, 0, 0, 404, 242);
    lv_settings_label(prepare, ui_tr("Before you begin"), 22, 18, &lv_font_instrument_sans_semibold_18, 0x1D2B34);
    lv_obj_t *instructions = lv_settings_label(prepare,
        ui_tr("Prepare the correct update file.\nKeep power connected throughout the update.\nDo not remove the update media."),
        22, 54, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_obj_set_width(instructions, 360);
    lv_obj_set_style_text_line_space(instructions, 7, 0);
    lv_settings_box(prepare, 22, 167, 360, 1, 0xE3E9ED);
    lv_settings_label(prepare, ui_tr("Installed version"), 22, 183, &lv_font_instrument_sans_medium_14, 0x586B78);
    lv_settings_label(prepare, installed_version && installed_version[0] ? installed_version : ui_tr("Not received"),
        218, 181, &lv_font_instrument_sans_semibold_18, 0x1D2B34);
    lv_obj_t *progress = lv_settings_panel(frame.body, 420, 0, 812, 242);
    runtime->phase_label = lv_settings_label(progress, "", 24, 21, &lv_font_instrument_sans_semibold_22, 0x1D2B34);
    runtime->status_label = lv_settings_label(progress, "", 24, 59, &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(runtime->status_label, 764);
    lv_obj_set_style_text_line_space(runtime->status_label, 4, 0);
    const char *stages[] = {ui_tr("Request"), ui_tr("Installing"), ui_tr("Result")};
    for (unsigned i = 0; i < 3; ++i) {
        runtime->steps[i] = lv_settings_box(progress, 24 + 258 * i, 139, 248, 58, 0xF1F4F5);
        lv_obj_set_style_radius(runtime->steps[i], 10, 0);
        lv_obj_t *label = lv_settings_label(runtime->steps[i], stages[i], 0, 0,
            &lv_font_instrument_sans_medium_16, 0x586B78);
        lv_obj_center(label);
    }
    runtime->start = lv_settings_button(frame.footer, 1062, 0, 170, 44, ui_tr("Start update"), true, start_cb, runtime);
    upgrade_page_runtime_init(runtime, config, runtime->status_label);
    visible_runtime = runtime;
    gesture_service_set_page_policy(config->page_id, NULL, upgrade_gesture);
    paint(runtime);
    timeout_cb(runtime->timeout_timer);
}

void upgrade_page_runtime_destroy(upgrade_page_runtime_t *runtime)
{
    if (!runtime) return;
    if (runtime->config) gesture_service_clear_page_policy(runtime->config->page_id);
    if (visible_runtime == runtime) visible_runtime = NULL;
    settings_detail_dialog_hide();
    if (runtime->timeout_timer) lv_timer_del(runtime->timeout_timer);
    runtime->timeout_timer = NULL;
    if (runtime->root) lv_obj_del(runtime->root);
    runtime->root = runtime->back = runtime->start = runtime->message = NULL;
    runtime->status_label = runtime->phase_label = NULL;
    memset(runtime->steps, 0, sizeof(runtime->steps));
}
