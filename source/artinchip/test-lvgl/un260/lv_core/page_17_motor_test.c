#include "page_17_motor_test.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include "settings_detail_ui.h"
#include "lv_page_manager.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/motor_test_service.h"
#include <string.h>

static lv_settings_frame_t motor_frame;
static lv_timer_t *motor_timer;
static lv_obj_t *status[3],*forward[3],*mode_retry_button;
static const char *names[]={"Level 1 motor","Level 2 motor","Impeller motor"};
static const char *hints[]={"First transport stage","Second transport stage","Stacker transport"};
static const char *phase_text[]={"Stopped","Queued","Starting","Running","Stopping",
    "Awaiting acknowledgement","Command rejected","Could not send"};
static void request(lv_event_t *e)
{
    unsigned key=(uintptr_t)lv_event_get_user_data(e);
    (void)motor_test_service_request(key/2,key%2==0);
}
static void retry(lv_event_t *e){(void)e;work_mode_service_retry();}
static void back(lv_event_t *e){(void)e;ui_manager_pop_page();}
static void motor_tick(lv_timer_t *timer)
{
    (void)timer;
    work_mode_snapshot_t mode;work_mode_service_get_snapshot(&mode);
    if(mode.phase==WORK_MODE_FAILED)lv_obj_clear_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(mode_retry_button,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=0;i<3;i++){
        motor_test_snapshot_t s=motor_test_service_get(i);
        const char *text=phase_text[s.phase];
        if(strcmp(lv_label_get_text(status[i]),text))lv_label_set_text(status[i],text);
        lv_obj_set_style_text_color(status[i],lv_color_hex(s.phase>=MOTOR_UNCONFIRMED?0xA35B12:s.running?0x1462CC:0x586B78),0);
        if(s.pending||s.queued||s.running||!work_mode_service_diagnostic_ready())lv_obj_add_state(forward[i],LV_STATE_DISABLED);
        else lv_obj_clear_state(forward[i],LV_STATE_DISABLED);
    }
    const char *text=work_mode_service_diagnostic_ready()?"Stops are confirmed one at a time. Keep the path clear.":work_mode_service_status_text();
    if(strcmp(lv_label_get_text(motor_frame.message),text))lv_label_set_text(motor_frame.message,text);
}
void ui_page_17_motor_test_create(lv_obj_t *parent)
{
    if(motor_frame.root)return;
    lv_settings_header_t h={"Motor test","Maintenance / Transport controls","Wrench",back,NULL};
    motor_frame=lv_settings_frame_create(parent,&h);
    settings_detail_add_run(motor_frame.root);
    for(unsigned i=0;i<3;i++){
        int y=12+i*74;
        lv_settings_label(motor_frame.body,names[i],24,y+5,&lv_font_instrument_sans_medium_20,0x1D2B34);
        lv_settings_label(motor_frame.body,hints[i],24,y+33,&lv_font_instrument_sans_medium_14,0x586B78);
        status[i]=lv_settings_label(motor_frame.body,"",410,y+20,&lv_font_instrument_sans_medium_16,0x586B78);
        forward[i]=lv_settings_button(motor_frame.body,852,y+8,164,46,"Forward",true,request,(void*)(uintptr_t)(i*2));
        lv_settings_button(motor_frame.body,1032,y+8,172,46,"Stop",false,request,(void*)(uintptr_t)(i*2+1));
        if(i<2)lv_settings_box(motor_frame.body,24,y+69,1184,1,0xE8EDF0);
    }
    mode_retry_button=lv_settings_button(motor_frame.footer,1050,0,182,46,"Retry mode",false,retry,NULL);
    lv_obj_set_width(motor_frame.message,1024);
    motor_timer=lv_timer_create(motor_tick,150,NULL);motor_tick(NULL);
}
void ui_page_17_motor_test_destroy(void)
{
    if(!motor_frame.root)return;
    motor_test_service_stop_all();
    if(motor_timer)lv_timer_del(motor_timer);
    motor_timer=NULL;
    lv_obj_del(motor_frame.root);motor_frame=(lv_settings_frame_t){0};
    memset(status,0,sizeof(status));memset(forward,0,sizeof(forward));mode_retry_button=NULL;
}
