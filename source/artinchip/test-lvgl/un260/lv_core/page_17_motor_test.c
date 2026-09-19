#include "page_17_motor_test.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_system/app_clock.h"
#include "un260/app_service/work_mode_service.h"
#include <stdbool.h>
#include <string.h>

typedef struct {
    const char *title, *hint;
    uint8_t command, forward[2], stop[2];
    lv_obj_t *status, *run_button;
    bool pending, requested_run;
    bool accepted_run;
    unsigned stale_acks;
    uint32_t deadline;
} motor_item_t;
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

static motor_item_t motors[] = {
    {"Level 1 motor", "First transport stage", 0x52, {1,1}, {0,0}, NULL, NULL, false, false, false, 0, 0},
    {"Level 2 motor", "Second transport stage", 0x53, {1,1}, {0,0}, NULL, NULL, false, false, false, 0, 0},
    {"Impeller motor", "Stacker transport", 0x54, {1,1}, {1,2}, NULL, NULL, false, false, false, 0, 0}
};
static lv_settings_frame_t motor_frame;
static lv_timer_t *motor_timer;

static void motor_status(motor_item_t *item, const char *text, uint32_t color)
{
    if (!item->status) return;
    if (strcmp(lv_label_get_text(item->status), text)) lv_label_set_text(item->status, text);
    lv_obj_set_style_text_color(item->status, lv_color_hex(color), 0);
}
static void motor_request(motor_item_t *item, bool run)
{
    if (run && (!work_mode_service_diagnostic_ready() || item->pending || item->accepted_run)) return;
    /* Stop remains available even if a start acknowledgement is delayed. */
    if (!run && item->pending && !item->requested_run &&
        (int32_t)(app_clock_uptime_ms()-item->deadline)<0) return;
    if (!settings_detail_send_command(item->command, run ? item->forward : item->stop, 2)) {
        motor_status(item, "Could not send", 0xB63B32);
        return;
    }
    if (item->pending) item->stale_acks++;
    work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN << (item - motors), true);
    item->pending = true;
    item->requested_run = run;
    item->deadline = app_clock_uptime_ms() + 5000;
    motor_status(item, run ? "Start requested" : "Stop requested", 0x1462CC);
}
static void motor_forward_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) motor_request(lv_event_get_user_data(e), true);
}
static void motor_stop_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) motor_request(lv_event_get_user_data(e), false);
}
void ui_page_17_motor_test_on_reply(uint8_t command, uint8_t result)
{
    for (unsigned i=0; i<3; i++) {
        motor_item_t *item = &motors[i];
        if (item->command != command || !item->pending) continue;
        /* Replies do not carry an operation ID. A superseded reply is not proof
         * that the later Stop was accepted. Prefer timeout over false success. */
        if (result != 1 && result != 2) return;
        if (item->stale_acks) { item->stale_acks--; return; }
        item->pending = false;
        if (result == 1) item->accepted_run = item->requested_run;
        if ((result == 1 && !item->requested_run) ||
            (result == 2 && item->requested_run && !item->accepted_run))
            work_mode_service_hold_operation(WORK_MODE_OPERATION_MOTOR_MAIN << i, false);
        motor_status(item, result == 2 ? "Command rejected" :
                     item->requested_run ? "Start accepted" : "Stop accepted",
                     result == 2 ? 0xB63B32 : 0x247650);
        return;
    }
}
static void motor_tick(lv_timer_t *timer)
{
    (void)timer;
    mode_retry_refresh();
    uint32_t now = app_clock_uptime_ms();
    for (unsigned i=0; i<3; i++) {
        motor_item_t *item=&motors[i];
        if (item->pending && (int32_t)(now-item->deadline)>=0) {
            motor_status(item, "No acknowledgement", 0xA35B12);
        }
        if (item->pending || item->accepted_run || !work_mode_service_diagnostic_ready())
            lv_obj_add_state(item->run_button, LV_STATE_DISABLED);
        else lv_obj_clear_state(item->run_button, LV_STATE_DISABLED);
    }
    const char *message = work_mode_service_diagnostic_ready() ?
        "Commands require controller acknowledgement. Keep the transport path clear." :
        work_mode_service_status_text();
    if (strcmp(lv_label_get_text(motor_frame.message), message)) lv_label_set_text(motor_frame.message, message);
}
static void motor_back(lv_event_t *e)
{
    if (lv_event_get_code(e)==LV_EVENT_CLICKED) ui_manager_pop_page();
}
void ui_page_17_motor_test_create(lv_obj_t *parent)
{
    if (motor_frame.root) return;
    lv_settings_header_t header={"Motor test","Maintenance / Transport controls","Wrench",motor_back,NULL};
    motor_frame=lv_settings_frame_create(parent,&header);
    lv_obj_set_style_bg_opa(motor_frame.body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(motor_frame.body,0,0);
    for (unsigned i=0;i<3;i++) {
        motor_item_t *item=&motors[i];
        lv_obj_t *card=lv_settings_box(motor_frame.body,i*416,0,400,242,0xFFFFFF);
        lv_obj_set_style_radius(card,14,0);
        lv_settings_label(card,item->title,24,24,&lv_font_instrument_sans_medium_22,0x1D2B34);
        lv_settings_label(card,item->hint,24,58,&lv_font_instrument_sans_medium_14,0x586B78);
        lv_obj_t *status=lv_settings_box(card,24,99,352,50,0xF1F4F5);
        lv_obj_set_style_radius(status,10,0);
        item->status=lv_settings_label(status,"Not requested",18,14,&lv_font_instrument_sans_medium_16,0x586B78);
        if(item->pending)motor_status(item,item->requested_run?"Start requested":"Stop requested",0x1462CC);
        else if(item->accepted_run)motor_status(item,"Start accepted",0x247650);
        item->run_button=lv_settings_button(card,24,173,170,46,"Forward",true,motor_forward_cb,item);
        lv_settings_button(card,206,173,170,46,"Stop",false,motor_stop_cb,item);
    }
    mode_retry_button=lv_settings_button(motor_frame.footer,1050,0,182,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(motor_frame.message,1030);
    motor_timer=lv_timer_create(motor_tick,150,NULL);
    motor_tick(NULL);
}
void ui_page_17_motor_test_destroy(void)
{
    if (!motor_frame.root) return;
    if (motor_timer) lv_timer_del(motor_timer);
    motor_timer=NULL;
    for (unsigned i=0;i<3;i++) {
        motor_request(&motors[i], false);
        motors[i].status=motors[i].run_button=NULL;
    }
    lv_obj_del(motor_frame.root);
    motor_frame=(lv_settings_frame_t){0};
    mode_retry_button=NULL;
}
