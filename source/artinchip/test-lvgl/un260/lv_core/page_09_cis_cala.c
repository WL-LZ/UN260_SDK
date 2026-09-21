#include "page_09_cis_cala.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/app_clock.h"
#include "un260/diagnostic/diagnostic.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/gesture/gesture_service.h"
#include <string.h>

static lv_obj_t *mode_retry_button;
static void mode_retry_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) work_mode_service_retry();
}
static void mode_retry_refresh(void)
{
    if (!mode_retry_button) return;
    work_mode_snapshot_t mode;
    work_mode_service_get_snapshot(&mode);
    if (mode.phase == WORK_MODE_FAILED) lv_obj_clear_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mode_retry_button, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *cis_page, *status_title, *status_detail, *start_button;
static lv_settings_frame_t frame;
static bool selected_white_balance;
static lv_timer_t *status_timer;
static bool leave_home;
static bool send_failed;
static void calibration_leave(void *data)
{
    (void)data;
    if (leave_home) { ui_manager_clear_stack(); ui_manager_switch(UI_PAGE_MAIN); }
    else ui_manager_pop_page();
}
static void calibration_leave_warning(bool home)
{
    leave_home=home;
    settings_detail_dialog_show("Leave calibration?",
        "The controller has not reported a final result. Leaving does not stop calibration. "
        "Manual mode remains active for safety.",
        "Leave","Stay",calibration_leave,NULL,NULL);
}
static bool calibration_gesture(gesture_action_t action)
{
    calibration_state_snapshot_t state;
    if (action != GESTURE_ACTION_HOME && action != GESTURE_ACTION_EXIT_PAGE) return false;
    diagnostic_calibration_get_snapshot(&state);
    if (state.cis_state != CIS_CALIB_RUNNING && state.cb_state != CB_CALIB_RUNNING) return false;
    calibration_leave_warning(action==GESTURE_ACTION_HOME);
    return true;
}
static void calibration_text(lv_obj_t *label,const char *text)
{
    if(strcmp(lv_label_get_text(label),text))lv_label_set_text(label,text);
}
static void status_tick(lv_timer_t *timer) { (void)timer; cis_calib_ui_refresh(); }

void ui_page_cis_calib_select(bool white_balance)
{
    if (!cis_page) selected_white_balance = white_balance;
}

void cis_enter_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    ui_page_cis_calib_select(false);
    ui_manager_push_page(UI_PAGE_CIS_CALIB);
}

static void cis_back(lv_event_t *e)
{
    calibration_state_snapshot_t state;
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    diagnostic_calibration_get_snapshot(&state);
    /* No cancellation command exists. A timed-out session can be left explicitly,
     * without claiming that the controller stopped or releasing Manual mode. */
    if (state.cis_state == CIS_CALIB_RUNNING || state.cb_state == CB_CALIB_RUNNING) {
        calibration_leave_warning(false);
        return;
    }
    ui_manager_pop_page();
}

static void cis_start(lv_event_t *e)
{
    calibration_state_snapshot_t state;
    const uint8_t sub = 1;
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    diagnostic_calibration_get_snapshot(&state);
    if (state.cis_state == CIS_CALIB_RUNNING || state.cb_state == CB_CALIB_RUNNING) return;
    if (!work_mode_service_diagnostic_ready()) return;
    const char *blocker=app_command_runtime_calibration_blocker();
    if(blocker){lv_label_set_text(status_detail,blocker);return;}
    send_failed = false;
    if (!diagnostic_calibration_begin(selected_white_balance ? CALIB_TARGET_CB : CALIB_TARGET_CIS,
                                      app_clock_uptime_ms())) return;
    if (!settings_detail_send_command(selected_white_balance ? 0x5F : 0x5B, &sub, 1)) {
        diagnostic_calibration_end_session();
        send_failed = true;
        lv_label_set_text(status_title, "Could not start");
        lv_label_set_text(status_detail, "Check the controller connection, then try again.");
        return;
    }
    work_mode_service_hold_operation(WORK_MODE_OPERATION_CALIBRATION, true);
    cis_calib_ui_refresh();
}

static void preparation_row(lv_obj_t *host, int y, const char *number,
                            const char *title, const char *hint)
{
    lv_obj_t *badge = lv_settings_box(host, 24, y, 36, 36, 0xF1F4F5);
    lv_obj_t *n = lv_settings_label(badge, number, 0, 0,
                                   &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_center(n);
    lv_settings_label(host, title, 76, y - 1, &lv_font_instrument_sans_medium_18, 0x1D2B34);
    lv_settings_label(host, hint, 76, y + 24, &lv_font_instrument_sans_medium_14, 0x586B78);
}

void ui_page_cis_calib_create(lv_obj_t *parent)
{
    if (cis_page) return;
    lv_settings_header_t header = {
        selected_white_balance ? "White balance" : "CIS calibration",
        "Maintenance / Calibration", selected_white_balance ? "Sun" : "Layers", cis_back, NULL
    };
    frame = lv_settings_frame_create(parent, &header);
    cis_page = frame.root;
    if(selected_white_balance)settings_detail_add_run(cis_page);
    lv_obj_set_style_bg_opa(frame.body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame.body, 0, 0);
    lv_obj_t *prepare = lv_settings_box(frame.body, 0, 0, 680, 242, 0xFFFFFF);
    lv_obj_set_style_radius(prepare, 14, 0);
    lv_settings_label(prepare, "Before you begin", 24, 20,
                      &lv_font_instrument_sans_medium_18, 0x1D2B34);
    preparation_row(prepare, 62, "1", selected_white_balance?"Place banknotes in the hopper":"Place the CIS bar",
                    selected_white_balance?"Prepare the notes for white balance.":"Place the CIS bar manually in the upper note path.");
    preparation_row(prepare, 119, "2", selected_white_balance?"Press RUN":"Press Start",
                    selected_white_balance?"Run the notes before starting calibration.":"No banknote run is required for CIS calibration.");
    preparation_row(prepare, 176, "3", selected_white_balance?"Press Start, then wait":"Wait for the result",
                    "The machine performs calibration and reports the result.");

    lv_obj_t *state = lv_settings_box(frame.body, 696, 0, 536, 242, 0xF1F4F5);
    lv_obj_set_style_radius(state, 14, 0);
    lv_settings_label(state, "CALIBRATION STATUS", 24, 24,
                      &lv_font_instrument_sans_medium_14, 0x586B78);
    status_title = lv_settings_label(state, "Ready to calibrate", 24, 63,
                                     &lv_font_instrument_sans_medium_22, 0x1D2B34);
    status_detail = lv_settings_label(state, "", 24, 103,
                                      &lv_font_instrument_sans_medium_16, 0x586B78);
    lv_obj_set_width(status_detail, 488);
    lv_label_set_long_mode(status_detail, LV_LABEL_LONG_WRAP);
    lv_label_set_text(frame.message, "Calibration starts only when you press Start.");
    start_button = lv_settings_button(frame.footer, 1060, 0, 172, 46, "Start", true, cis_start, NULL);
    gesture_service_set_page_policy(UI_PAGE_CIS_CALIB, NULL, calibration_gesture);
    mode_retry_button=lv_settings_button(frame.footer,904,0,130,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(frame.message,884);
    status_timer = lv_timer_create(status_tick, 200, NULL);
    cis_calib_ui_refresh();
}

void cis_calib_ui_refresh(void)
{
    mode_retry_refresh();
    calibration_state_snapshot_t state;
    const char *title = "Ready to calibrate";
    const char *detail = "Prepare the machine, then press Start.";
    uint32_t color = 0x1D2B34;
    if (!cis_page || !lv_obj_is_valid(cis_page)) return;
    diagnostic_calibration_get_snapshot(&state);
    bool running = state.cis_state == CIS_CALIB_RUNNING || state.cb_state == CB_CALIB_RUNNING;
    bool success = selected_white_balance ? state.cb_state == CB_CALIB_SUCCESS : state.cis_state == CIS_CALIB_SUCCESS;
    if (state.timed_out && running) {
        title = "Waiting for a final result";
        detail = "The controller has not confirmed completion. Manual mode stays active. "
                 "Back lets you leave without stopping the calibration.";
        color = 0xA35B12;
    } else if (running) {
        title = "Calibration in progress";
        detail = "Waiting for the controller. Keep the path clear and do not power off.";
        color = 0x1462CC;
    } else if (send_failed) {
        title = "Could not start";
        detail = "Check the controller connection, then try again.";
        color = 0xB63B32;
    } else if (success) {
        title = "Calibration complete";
        detail = "The controller confirmed the result. You can return to settings.";
        color = 0x247650;
    } else if ((!selected_white_balance && state.cis_state >= CIS_CALIB_FAIL_UPPER) ||
               (selected_white_balance && state.cb_state == CB_CALIB_FAIL_IR)) {
        title = "Calibration not completed";
        detail = !selected_white_balance && state.cis_state == CIS_CALIB_FAIL_UPPER ?
                 "Check the upper channel and calibration material, then retry." :
                 !selected_white_balance && state.cis_state == CIS_CALIB_FAIL_LOWER ?
                 "Check the lower channel and calibration material, then retry." :
                 "Check the infrared channel and calibration material, then retry.";
        color = 0xB63B32;
    }
    const char *blocker=running?NULL:app_command_runtime_calibration_blocker();
    if(blocker){
        title=work_mode_service_diagnostic_ready()?"Waiting for the machine":"Preparing manual mode";
        detail=blocker;color=0xA35B12;
    }
    calibration_text(status_title, title);
    lv_obj_set_style_text_color(status_title, lv_color_hex(color), 0);
    calibration_text(status_detail, detail);
    if (running) {
        lv_obj_add_state(start_button, LV_STATE_DISABLED);
        lv_obj_clear_state(frame.back, LV_STATE_DISABLED);
    } else {
        if (!blocker) lv_obj_clear_state(start_button, LV_STATE_DISABLED);
        else lv_obj_add_state(start_button, LV_STATE_DISABLED);
        lv_obj_clear_state(frame.back, LV_STATE_DISABLED);
    }
    calibration_text(frame.message, !work_mode_service_diagnostic_ready() ? work_mode_service_status_text() :
                      running ? "Calibration is controlled by the machine." :
                      "Calibration starts only when you press Start.");
}

void ui_page_cis_calib_destroy(void)
{
    if (!cis_page) return;
    gesture_service_clear_page_policy(UI_PAGE_CIS_CALIB);
    settings_detail_dialog_hide();
    if (status_timer) lv_timer_del(status_timer);
    status_timer = NULL;
    lv_obj_del(cis_page);
    cis_page = status_title = status_detail = start_button = NULL;
    frame = (lv_settings_frame_t){0};
    mode_retry_button=NULL;
    send_failed = false;
    calibration_state_snapshot_t state;
    diagnostic_calibration_get_snapshot(&state);
    if (state.cis_state != CIS_CALIB_RUNNING && state.cb_state != CB_CALIB_RUNNING)
        diagnostic_calibration_end_session();
}
