#include "page_26_set_aging.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/machine_state/machine_state.h"
#include "un260/app_service/setting_service.h"
#include "un260/app_service/work_mode_service.h"
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

static lv_settings_frame_t aging_frame;
static lv_obj_t *aging_button, *aging_title, *aging_detail;
static lv_timer_t *aging_timer;
static bool start_pending;
static enum { AGING_VIEW_IDLE, AGING_VIEW_WAIT, AGING_VIEW_RUNNING, AGING_VIEW_DONE,
              AGING_VIEW_FAILED, AGING_VIEW_TIMEOUT, AGING_VIEW_SEND_FAILED } aging_view;
static void aging_refresh(void);

static bool aging_gesture(gesture_action_t action)
{
    return (action==GESTURE_ACTION_HOME || action==GESTURE_ACTION_EXIT_PAGE) &&
           (start_pending || machine_state_aging_running());
}
static void aging_back(lv_event_t *e)
{
    if (lv_event_get_code(e)!=LV_EVENT_CLICKED) return;
    if (start_pending || machine_state_aging_running()) return;
    settings_detail_dialog_hide();
    ui_manager_pop_page();
}
static void aging_confirm_start(void *user_data)
{
    (void)user_data;
    if (start_pending || aging_view == AGING_VIEW_TIMEOUT || machine_state_aging_running() || !work_mode_service_diagnostic_ready()) return;
    if (!setting_service_request_aging_start()) {
        aging_view=AGING_VIEW_SEND_FAILED;
        aging_refresh();
        return;
    }
    work_mode_service_hold_operation(WORK_MODE_OPERATION_AGING,true);
    start_pending=true;aging_view=AGING_VIEW_WAIT;
    aging_refresh();
}
static void aging_start(lv_event_t *e)
{
    if (lv_event_get_code(e)!=LV_EVENT_CLICKED || start_pending || aging_view == AGING_VIEW_TIMEOUT ||
        machine_state_aging_running() || !work_mode_service_diagnostic_ready()) return;
    settings_detail_dialog_show("Start aging test?",
        "The machine will run the aging procedure. Keep the transport path clear. "
        "There is no stop command in this protocol.",
        "Start","Cancel",aging_confirm_start,NULL,NULL);
}
static void aging_refresh(void)
{
    if (!aging_frame.root) return;
    mode_retry_refresh();
    bool running=machine_state_aging_running();
    const char *title="Ready for testing";
    const char *detail="Check the machine before starting the aging procedure.";
    uint32_t color=0x1D2B34;
    if (running) aging_view=AGING_VIEW_RUNNING;
    switch(aging_view) {
    case AGING_VIEW_WAIT:title="Start requested";detail="Waiting for acknowledgement from the controller.";color=0x1462CC;break;
    case AGING_VIEW_RUNNING:title="Aging test running";detail="The controller accepted the test. Keep the transport path clear.";color=0x1462CC;break;
    case AGING_VIEW_DONE:title="Test completed";detail="The controller reported that the aging procedure has finished.";color=0x247650;break;
    case AGING_VIEW_FAILED:title="Test not started";detail="The controller rejected the request. Check the machine before retrying.";color=0xB63B32;break;
    case AGING_VIEW_TIMEOUT:title="No acknowledgement";detail="The result is unknown. The machine remains in manual mode for safety.";color=0xA35B12;break;
    case AGING_VIEW_SEND_FAILED:title="Could not send";detail="Check the controller connection, then try again.";color=0xB63B32;break;
    default:break;
    }
    if(strcmp(lv_label_get_text(aging_title),title))lv_label_set_text(aging_title,title);
    lv_obj_set_style_text_color(aging_title,lv_color_hex(color),0);
    if(strcmp(lv_label_get_text(aging_detail),detail))lv_label_set_text(aging_detail,detail);
    if (running || start_pending || aging_view == AGING_VIEW_TIMEOUT || !work_mode_service_diagnostic_ready()) lv_obj_add_state(aging_button,LV_STATE_DISABLED);
    else lv_obj_clear_state(aging_button,LV_STATE_DISABLED);
    if (running || start_pending) lv_obj_add_state(aging_frame.back,LV_STATE_DISABLED);
    else lv_obj_clear_state(aging_frame.back,LV_STATE_DISABLED);
    const char *message=!work_mode_service_diagnostic_ready() ? work_mode_service_status_text() :
        "A controller acknowledgement is not a completion report.";
    if(strcmp(lv_label_get_text(aging_frame.message),message))lv_label_set_text(aging_frame.message,message);
}
static void aging_tick(lv_timer_t *timer) { (void)timer;aging_refresh(); }
void ui_page_26_set_aging_create(lv_obj_t *parent)
{
    if (aging_frame.root) return;
    lv_settings_header_t header={"Aging test","Maintenance / Endurance procedure","Wrench",aging_back,NULL};
    aging_frame=lv_settings_frame_create(parent,&header);
    lv_obj_set_style_bg_opa(aging_frame.body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(aging_frame.body,0,0);
    lv_obj_t *prepare=lv_settings_box(aging_frame.body,0,0,632,242,0xFFFFFF);
    lv_obj_set_style_radius(prepare,14,0);
    lv_settings_label(prepare,"Prepare the machine",24,24,&lv_font_instrument_sans_medium_22,0x1D2B34);
    lv_settings_label(prepare,"Transport path",24,76,&lv_font_instrument_sans_medium_18,0x1D2B34);
    lv_settings_label(prepare,"Remove loose objects and check the path before starting.",24,104,&lv_font_instrument_sans_medium_16,0x586B78);
    lv_settings_box(prepare,24,143,584,1,0xE3E9ED);
    lv_settings_label(prepare,"While the test runs",24,165,&lv_font_instrument_sans_medium_18,0x1D2B34);
    lv_settings_label(prepare,"Keep hands clear of moving parts. Follow the service procedure.",24,193,&lv_font_instrument_sans_medium_16,0x586B78);
    lv_obj_t *result=lv_settings_box(aging_frame.body,648,0,584,242,0xF1F4F5);
    lv_obj_set_style_radius(result,14,0);
    lv_settings_label(result,"CONTROLLER STATUS",24,24,&lv_font_instrument_sans_medium_14,0x586B78);
    aging_title=lv_settings_label(result,"",24,65,&lv_font_instrument_sans_medium_24,0x1D2B34);
    aging_detail=lv_settings_label(result,"",24,108,&lv_font_instrument_sans_medium_16,0x586B78);
    lv_obj_set_width(aging_detail,536);lv_label_set_long_mode(aging_detail,LV_LABEL_LONG_WRAP);
    aging_button=lv_settings_button(aging_frame.footer,1050,0,182,46,"Start test",true,aging_start,NULL);
    gesture_service_set_page_policy(UI_PAGE_AGING_SETTING,NULL,aging_gesture);
    mode_retry_button=lv_settings_button(aging_frame.footer,904,0,130,46,"Retry",false,mode_retry_clicked,NULL);
    lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(aging_frame.message,884);
    aging_timer=lv_timer_create(aging_tick,200,NULL);
    aging_refresh();
}
void ui_page_26_set_aging_on_reply(uint8_t result)
{
    if (result!=0 && result!=1 && result!=2) return;
    start_pending=false;
    if(result==0) {machine_state_confirm_aging_running(true);aging_view=AGING_VIEW_RUNNING;}
    else {
        machine_state_confirm_aging_running(false);
        work_mode_service_hold_operation(WORK_MODE_OPERATION_AGING,false);
        aging_view=result==2?AGING_VIEW_DONE:AGING_VIEW_FAILED;
    }
    aging_refresh();
}
void ui_page_26_set_aging_on_timeout(void)
{
    start_pending=false;aging_view=AGING_VIEW_TIMEOUT;
    /* A missing response is not proof that the hardware stopped. */
    aging_refresh();
}
void ui_page_26_set_aging_destroy(void)
{
    gesture_service_clear_page_policy(UI_PAGE_AGING_SETTING);
    settings_detail_dialog_hide();
    if(aging_timer)lv_timer_del(aging_timer);
    aging_timer=NULL;
    if(aging_frame.root)lv_obj_del(aging_frame.root);
    aging_frame=(lv_settings_frame_t){0};
    mode_retry_button=NULL;
    aging_button=aging_title=aging_detail=NULL;
}
