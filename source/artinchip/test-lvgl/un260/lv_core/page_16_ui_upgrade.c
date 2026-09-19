#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "page_16_ui_upgrade.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_upgrade_popup.h"
#include "un260/device_info/device_info.h"
#include "un260/gesture/gesture_service.h"
#include "un260/app_service/upgrade_session.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    lv_settings_frame_t frame;
    lv_obj_t *usb, *package, *installed, *phase, *status, *progress, *percent;
    lv_obj_t *steps[3], *start;
    lv_timer_t *timer;
    ui_upgrade_detect_info_t detected;
    ui_upgrade_service_status_t actual;
    uint32_t last_detect;
    const char *start_error;
    bool have_detected, have_status;
    bool blocked, uncertain, home_requested;
} ui_upgrade_page_context_t;

static ui_upgrade_page_context_t page;
/* A result already acknowledged must not be presented again on every re-entry. */
static bool result_presented;

static bool package_ready(void)
{
    return page.detected.usb_present && page.detected.usb_mounted &&
        page.detected.package_found &&
        (page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH ||
         page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT);
}

static const char *preparation_text(void)
{
    if (!page.detected.usb_present) return "Insert the USB drive containing the UI update package.";
    if (!page.detected.usb_mounted) return "The USB drive is detected but not mounted. Check the drive and reconnect it.";
    if (!page.detected.package_found) return "No UI update package was found on this USB drive.";
    if (page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_ERROR)
        return "The update package could not be verified. Check the file and try again.";
    if (!package_ready()) return "Waiting for package verification.";
    if (page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH)
        return "This package matches the installed version. Start only if you want to reinstall it.";
    return "The update package is ready. Keep power and the USB drive connected.";
}

static const char *stage_title(ui_upgrade_stage_t stage)
{
    switch (stage) {
    case UI_UPGRADE_STAGE_PREPARE: return "Preparing update";
    case UI_UPGRADE_STAGE_VERIFY: return "Verifying package";
    case UI_UPGRADE_STAGE_EXTRACT: return "Unpacking package";
    case UI_UPGRADE_STAGE_PREFLIGHT: return "Checking requirements";
    case UI_UPGRADE_STAGE_INSTALL: return "Installing update";
    case UI_UPGRADE_STAGE_SYNC: return "Writing changes";
    case UI_UPGRADE_STAGE_FINISH: return "Finishing update";
    case UI_UPGRADE_STAGE_SUCCESS: return "Update complete";
    case UI_UPGRADE_STAGE_FAIL: return "Update not completed";
    default: return "Waiting for update status";
    }
}

static void render(void)
{
    if (!page.frame.root) return;
    bool running = page.actual.running;
    bool finished = page.actual.finished && !running;
    const char *title = page.uncertain ? "Update result unknown" :
        page.blocked ? "Another update is active" : page.start_error ? "Update could not start" :
        running || finished ? stage_title(page.actual.stage) :
        package_ready() ? "Ready to update" : "Prepare an update package";
    const char *detail = page.uncertain ?
        (page.actual.result_text[0] ? page.actual.result_text :
         "The updater process result could not be confirmed. Keep power connected.") :
        page.blocked ? "Wait for the other update to finish. Its result may still be unknown." :
        page.start_error ? page.start_error :
        finished ? page.actual.result_text :
        running ? page.actual.step_text : preparation_text();
    if (!detail[0]) detail = finished ?
        (page.actual.success ? "The update completed successfully." : "The update did not complete. Check the package.") :
        "Waiting for the updater to report its next step.";
    uint32_t tone = page.uncertain ? 0x946321 :
        page.start_error || (finished && !page.actual.success) ? 0xB1393E :
        finished ? 0x287953 : 0x1D2B34;
    lv_label_set_text(page.phase, title);
    lv_obj_set_style_text_color(page.phase, lv_color_hex(tone), 0);
    lv_label_set_text(page.status, detail);
    lv_label_set_text(page.usb, !page.detected.usb_present ? "Not connected" :
        page.detected.usb_mounted ? "Connected" : "Not mounted");
    lv_label_set_text(page.package, !page.detected.package_found ? "Not found" :
        page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_MATCH ? "Same as installed" :
        page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_DIFFERENT ? "Update available" :
        page.detected.package_hash_status == UI_UPGRADE_PACKAGE_HASH_ERROR ? "Cannot verify" : "Not verified");
    if (running || page.blocked || !package_ready()) lv_obj_add_state(page.start, LV_STATE_DISABLED);
    else lv_obj_clear_state(page.start, LV_STATE_DISABLED);
    if (running) lv_obj_add_state(page.frame.back, LV_STATE_DISABLED);
    else lv_obj_clear_state(page.frame.back, LV_STATE_DISABLED);
    lv_label_set_text(page.frame.message, page.uncertain ?
        "Update result unknown. Keep power connected." : running ?
        "Keep power connected. Do not remove the USB drive." :
        finished && page.actual.success ? "Restart the device to use the updated UI." :
        "The update package is kept on the USB drive.");

    int progress = page.actual.progress < 0 ? 0 : page.actual.progress > 100 ? 100 : page.actual.progress;
    /* Only the updater may advance progress. No independent animation or timer. */
    lv_bar_set_value(page.progress, progress, LV_ANIM_OFF);
    lv_label_set_text_fmt(page.percent, "%d%%", progress);
    if (page.uncertain) {
        lv_obj_add_flag(page.progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(page.percent, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(page.progress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(page.percent, LV_OBJ_FLAG_HIDDEN);
    }
    unsigned active = finished || page.uncertain ? 2 :
        page.actual.stage >= UI_UPGRADE_STAGE_INSTALL ? 1 : 0;
    for (unsigned i = 0; i < 3; ++i) {
        bool selected = i == active && (running || finished || page.uncertain);
        lv_obj_set_style_bg_color(page.steps[i], lv_color_hex(selected ? 0xEDF4FF : 0xF1F4F5), 0);
        lv_obj_set_style_border_width(page.steps[i], selected ? 1 : 0, 0);
        lv_obj_set_style_border_color(page.steps[i], lv_color_hex(0xBDD0EA), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(page.steps[i], 0),
            lv_color_hex(selected ? 0x1462CC : 0x586B78), 0);
    }
}

static bool status_equal(const ui_upgrade_service_status_t *a, const ui_upgrade_service_status_t *b)
{
    return a->running == b->running && a->finished == b->finished &&
        a->success == b->success && a->progress == b->progress && a->stage == b->stage &&
        !strcmp(a->step_text, b->step_text) && !strcmp(a->result_text, b->result_text);
}

static void refresh(lv_timer_t *timer)
{
    (void)timer;
    if (!page.frame.root) return;
    ui_upgrade_service_status_t actual = {0};
    ui_upgrade_service_poll(&actual);
    /* The existing result popup resets the service when dismissed. Retain
     * this page's result until the next explicit attempt, rather than show 0%. */
    if (!actual.running && !actual.finished && page.actual.finished) actual = page.actual;
    bool changed = !page.have_status || !status_equal(&page.actual, &actual);
    upgrade_session_owner_t owner = upgrade_session_owner();
    bool blocked = owner != UPGRADE_SESSION_NONE &&
        (owner != UPGRADE_SESSION_UI || !actual.running);
    bool uncertain = owner == UPGRADE_SESSION_UI && !actual.running;
    changed |= page.blocked != blocked || page.uncertain != uncertain;
    page.blocked = blocked;
    page.uncertain = uncertain;
    page.actual = actual;
    page.have_status = true;
    if (!page.have_detected || (!actual.running && lv_tick_elaps(page.last_detect) >= 1000)) {
        ui_upgrade_detect_info_t info = {0};
        ui_upgrade_service_detect(&info);
        bool updated = !page.have_detected ||
            page.detected.usb_present != info.usb_present ||
            page.detected.usb_mounted != info.usb_mounted ||
            page.detected.package_found != info.package_found ||
            page.detected.package_hash_status != info.package_hash_status;
        page.detected = info;
        page.have_detected = true;
        page.last_detect = lv_tick_get();
        if (updated) page.start_error = NULL;
        changed |= updated;
    }
    if (changed) render();
    if (actual.finished && !actual.running && !page.uncertain && !result_presented) {
        result_presented = true;
        lv_upgrade_popup_show_result(actual.success, actual.result_text);
    }
}

static void leave_page(void *unused)
{
    (void)unused;
    if (page.home_requested) { ui_manager_clear_stack(); ui_manager_switch(UI_PAGE_MAIN); }
    else ui_manager_pop_page();
}

static void request_back(bool home)
{
    if (page.actual.running || lv_upgrade_popup_is_showing() || settings_detail_overlay_is_open()) return;
    page.home_requested = home;
    if (page.uncertain) {
        settings_detail_dialog_show_ex(SETTINGS_DIALOG_WARNING, "Leave update status?",
            "The result is unknown. Leaving does not stop the update. Keep the machine powered on.",
            "Leave page", "Keep waiting", leave_page, NULL, NULL);
    } else leave_page(NULL);
}

static bool guard_gesture(gesture_action_t action)
{
    if (settings_detail_overlay_is_open()) return true;
    if (page.uncertain &&
        (action == GESTURE_ACTION_HOME || action == GESTURE_ACTION_EXIT_PAGE)) {
        request_back(action == GESTURE_ACTION_HOME);
        return true;
    }
    /* Includes export: USB media must not be reused during installation. */
    return page.actual.running || page.uncertain || lv_upgrade_popup_is_showing();
}

static void back_clicked(lv_event_t *event)
{
    (void)event;
    request_back(false);
}

static void start_clicked(lv_event_t *event)
{
    (void)event;
    /* Recheck the service and removable media at the point of action. */
    refresh(NULL);
    if (page.actual.running || page.blocked || lv_upgrade_popup_is_showing()) return;
    page.have_detected = false;
    refresh(NULL);
    if (!package_ready()) { render(); return; }
    ui_upgrade_service_reset();
    memset(&page.actual, 0, sizeof(page.actual));
    page.have_status = false;
    ui_upgrade_start_result_t result = ui_upgrade_service_start();
    page.start_error = NULL;
    switch (result) {
    case UI_UPGRADE_START_OK: result_presented = false; break;
    case UI_UPGRADE_START_BUSY: page.start_error = "Another update is still running. Wait for its result."; break;
    case UI_UPGRADE_START_SCRIPT_NOT_FOUND: page.start_error = "The UI updater is unavailable. Contact service support."; break;
    case UI_UPGRADE_START_PACKAGE_NOT_READY: page.start_error = "The update package is not ready. Check the USB drive and file."; break;
    case UI_UPGRADE_START_STATUS_CLEANUP_FAILED: page.start_error = "Previous update status could not be cleared. Contact service support."; break;
    case UI_UPGRADE_START_FORK_FAILED: page.start_error = "The updater could not start. Try again when the device is idle."; break;
    }
    refresh(NULL);
    render();
}

void ui_page_16_ui_upgrade_create(lv_obj_t *parent)
{
    if (page.frame.root) return;
    lv_settings_header_t header = {.title = "UI update", .subtitle = "Data / Upgrade",
        .back = back_clicked};
    page.frame = lv_settings_frame_create(parent, &header);
    lv_obj_set_style_bg_opa(page.frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(page.frame.body, 0, 0);
    lv_obj_t *media = lv_settings_panel(page.frame.body, 0, 0, 404, 242);
    lv_settings_label(media, "Update source", 22, 18, &lv_font_instrument_sans_semibold_18, 0x1D2B34);
    const char *names[] = {"USB drive", "UI package", "Installed version"};
    lv_obj_t **values[] = {&page.usb, &page.package, &page.installed};
    for (unsigned i = 0; i < 3; ++i) {
        int y = 67 + 57 * i;
        lv_settings_label(media, names[i], 22, y, &lv_font_instrument_sans_medium_14, 0x586B78);
        *values[i] = lv_settings_label(media, "", 166, y - 2, &lv_font_instrument_sans_medium_16, 0x1D2B34);
        lv_obj_set_width(*values[i], 214);
        lv_obj_set_style_text_align(*values[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(*values[i], LV_LABEL_LONG_DOT);
        if (i < 2) lv_settings_box(media, 22, y + 34, 360, 1, 0xE3E9ED);
    }
    const char *version = device_info_display_app();
    lv_label_set_text(page.installed, version && version[0] ? version : "Not received");
    lv_obj_t *workflow = lv_settings_panel(page.frame.body, 420, 0, 812, 242);
    page.phase = lv_settings_label(workflow, "", 24, 21, &lv_font_instrument_sans_semibold_22, 0x1D2B34);
    page.status = lv_settings_label(workflow, "", 24, 59, &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(page.status, 764);
    lv_obj_set_height(page.status, 44);
    lv_label_set_long_mode(page.status, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_line_space(page.status, 4, 0);
    page.progress = lv_bar_create(workflow);
    lv_obj_remove_style_all(page.progress);
    lv_obj_set_pos(page.progress, 24, 116); lv_obj_set_size(page.progress, 694, 6);
    lv_obj_set_style_bg_color(page.progress, lv_color_hex(0xE2E9EE), 0);
    lv_obj_set_style_bg_opa(page.progress, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(page.progress, 3, 0);
    lv_obj_set_style_bg_color(page.progress, lv_color_hex(0x1462CC), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(page.progress, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(page.progress, 3, LV_PART_INDICATOR);
    page.percent = lv_settings_label(workflow, "0%", 734, 107, &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(page.percent, 54); lv_obj_set_style_text_align(page.percent, LV_TEXT_ALIGN_RIGHT, 0);
    const char *stages[] = {"Prepare", "Installing", "Result"};
    for (unsigned i = 0; i < 3; ++i) {
        page.steps[i] = lv_settings_box(workflow, 24 + 258 * i, 155, 248, 58, 0xF1F4F5);
        lv_obj_set_style_radius(page.steps[i], 10, 0);
        lv_obj_t *label = lv_settings_label(page.steps[i], stages[i], 0, 0,
            &lv_font_instrument_sans_medium_16, 0x586B78);
        lv_obj_center(label);
    }
    page.start = lv_settings_button(page.frame.footer, 1062, 0, 170, 44,
        "Start update", true, start_clicked, NULL);
    gesture_service_set_page_policy(UI_PAGE_UI_UPGRADE, NULL, guard_gesture);
    refresh(NULL);
    page.timer = lv_timer_create(refresh, 200, NULL);
}

void ui_page_16_ui_upgrade_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_UI_UPGRADE);
    settings_detail_dialog_hide();
    if (page.timer) lv_timer_del(page.timer);
    if (page.frame.root) lv_obj_del(page.frame.root);
    /* The updater process is service-owned and is never reset by page teardown. */
    memset(&page, 0, sizeof(page));
}
