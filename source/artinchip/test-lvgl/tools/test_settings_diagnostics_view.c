#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/page_09_cis_cala.h"
#include "un260/lv_core/page_10_debug.h"
#include "un260/lv_core/page_12_sensor.h"
#include "un260/lv_core/page_17_motor_test.h"
#include "un260/lv_core/page_26_set_aging.h"
#include "un260/lv_core/page_28_get_image.h"
#include "un260/lv_core/page_31_get_wave.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/lv_page_manager.h"
#include "test_settings_actions.h"
#include "un260/lv_components/lv_print_toast.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/motor_test_service.h"
#include "un260/lv_components/lv_settings.h"
static void settings_run_clicked(lv_event_t *event){(void)event;}
const char *app_command_runtime_diagnostic_run_blocker(void){return work_mode_service_diagnostic_ready()?NULL:work_mode_service_status_text();}
const char *app_command_runtime_calibration_blocker(void){return app_command_runtime_diagnostic_run_blocker();}
#include "un260/diagnostic/diagnostic.h"
#include "un260/gesture/gesture_service.h"
#include "tools/test_page_background_asset.h"
#include "actual_diagnostic_pointer.h"

static unsigned sends,pops,retries;static uint8_t sent_cmd;static bool gate=true,aging,send_ok=true;
static uint32_t holds;
static settings_detail_dialog_cb_t confirm;
bool work_mode_service_diagnostic_ready(void){return gate;}
const char *work_mode_service_status_text(void){return "Waiting for manual mode";}
void work_mode_service_hold_operation(uint32_t owner,bool active){if(active)holds|=owner;else holds&=~owner;}
void work_mode_service_retry(void){retries++;}
void work_mode_service_get_snapshot(work_mode_snapshot_t*s){memset(s,0,sizeof(*s));s->phase=gate?WORK_MODE_READY:WORK_MODE_FAILED;}
bool machine_state_aging_running(void){return aging;}
void machine_state_confirm_aging_running(bool v){aging=v;}
bool settings_detail_send_command(uint8_t cmd,const uint8_t *sub,uint16_t len){(void)sub;(void)len;sends++;sent_cmd=cmd;return send_ok;}
bool setting_service_request_aging_start(void){sends++;sent_cmd=0x46;return send_ok;}
int protocol_send(uint8_t cmd,const uint8_t *sub,uint16_t len){return settings_detail_send_command(cmd,sub,len)?len+5:-1;}
void ui_manager_push_page(ui_page_t p){(void)p;}
bool ui_manager_pop_page(void){pops++;return true;}
void ui_manager_switch(ui_page_t p){(void)p;}
void ui_manager_clear_stack(void){}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
const char *ui_text_get(ui_text_id_t id){(void)id;return "Waiting for response";}
void settings_detail_dialog_hide(void){confirm=NULL;}
bool settings_detail_dialog_show(const char*a,const char*b,const char*c,const char*d,settings_detail_dialog_cb_t cb,settings_detail_dialog_cb_t cancel,void*data){(void)a;(void)b;(void)c;(void)d;(void)cancel;(void)data;confirm=cb;return true;}
static bool (*policy)(gesture_action_t);
void gesture_service_set_page_policy(uint32_t owner,bool(*drag)(void),bool(*handler)(gesture_action_t)){(void)owner;(void)drag;policy=handler;}
void gesture_service_clear_page_policy(uint32_t owner){(void)owner;policy=NULL;}
bool user_cfg_screenshot_enabled(void){return false;}
bool user_cfg_screenshot_save(bool v){(void)v;return true;}
bool user_cfg_screen_recording_enabled(void){return false;}
bool user_cfg_screen_recording_save(bool v){(void)v;return true;}
bool user_cfg_performance_monitor_enabled(void){return false;}
bool user_cfg_performance_monitor_save(bool v){(void)v;return true;}
bool user_cfg_performance_profile_enabled(void){return false;}
bool user_cfg_performance_profile_save(bool v){(void)v;return true;}
void screen_recording_service_request_stop(void){}
void lv_debug_overlay_init(void){}
void lv_debug_overlay_set_enabled(bool v){(void)v;}
void perf_profile_set_enabled(bool v){(void)v;}
lv_print_toast_config_t lv_print_toast_get_default_config(void){return (lv_print_toast_config_t){0};}
void lv_print_toast_show_with_config(const lv_print_toast_config_t*c){(void)c;}
ui_export_text_result_t ui_export_text_lines(const char*p,const char*const*l,size_t n){(void)p;(void)l;return n?UI_EXPORT_TEXT_OK:UI_EXPORT_TEXT_EMPTY;}

static lv_color_t pixels[1280*400],buffer[1280*40];
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(pixels+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static lv_res_t info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;const un260_compiled_asset_t*a=test_page_asset_find(src);if(!a){fprintf(stderr,"Missing asset: %s\n",(const char*)src);abort();}memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=test_page_asset_find(s->src)->pixels;return LV_RES_OK;}
#include "test_settings_toolbar.h"
static void snapshot(const char*n){lv_obj_update_layout(lv_scr_act());assert_settings_toolbars(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f);}
static lv_obj_t *label_find(lv_obj_t *o,const char *text){if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t*f=label_find(lv_obj_get_child(o,i),text);if(f)return f;}return NULL;}
static void click(const char *text){lv_obj_t*l=label_find(lv_scr_act(),text);assert(l);while(l&&!lv_obj_has_flag(l,LV_OBJ_FLAG_CLICKABLE))l=lv_obj_get_parent(l);assert(l);assert(!lv_obj_has_flag(l,LV_OBJ_FLAG_HIDDEN));lv_event_send(l,LV_EVENT_CLICKED,NULL);}
static void dump_labels(lv_obj_t *o){if(lv_obj_check_type(o,&lv_label_class))fprintf(stderr,"LABEL %s\n",lv_label_get_text(o));for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)dump_labels(lv_obj_get_child(o,i));}
static void tick(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
static void calibration_test(void){
 unsigned before=sends;gate=false;ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());snapshot("cis-ready");click("Start");assert(sends==before);gate=true;tick(220);click("Start");assert(sent_cmd==0x5B&&holds&1&&policy(GESTURE_ACTION_HOME));snapshot("cis-running");
 uint8_t response[]={0xfd,0xdf,6,0x5b,2,0};diagnostic_reply_hooks_t hooks={cis_calib_ui_refresh};diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);assert(label_find(lv_scr_act(),"Calibration complete"));snapshot("cis-complete");ui_page_cis_calib_destroy();assert(!policy);
 ui_page_cis_calib_select(true);ui_page_cis_calib_create(lv_scr_act());click("Start");assert(sent_cmd==0x5F);diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);assert(label_find(lv_scr_act(),"Calibration complete"));snapshot("white-balance");ui_page_cis_calib_destroy();
 ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());click("Start");
 assert(diagnostic_calibration_poll(lv_tick_get()+DIAGNOSTIC_CALIBRATION_TIMEOUT_MS+1));
 cis_calib_ui_refresh();snapshot("cis-timeout");assert(label_find(lv_scr_act(),"Waiting for a final result"));
 assert(policy(GESTURE_ACTION_HOME)&&confirm);settings_detail_dialog_hide();click("Back");assert(confirm);confirm(NULL);
 ui_page_cis_calib_destroy();diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);
 puts("PASS calibration target, no entry command, manual gate, running Home guard, 5F/5B completion and lifecycle");
}
static void sensor_test(void){
 sensor_state_clear();ui_page_12_sensor_create(lv_scr_act());assert(sent_cmd==0x1D);snapshot("sensors-waiting");for(unsigned i=0;i<11;i++)sensor_state_set_voltage(i,16+i*18);tick(320);snapshot("sensors-live");assert(label_find(lv_scr_act(),"0.206 V"));ui_page_12_sensor_destroy();unsigned before=sends;tick(500);assert(sends==before);puts("PASS sensor polling, received values and timer cleanup");
}
static void motor_test(void){
 ui_page_17_motor_test_create(lv_scr_act());snapshot("motors-ready");click("Forward");
 motor_test_service_poll(lv_tick_get());assert(sent_cmd==0x52&&holds&2);
 motor_test_service_on_reply(0x52,1);tick(180);assert(label_find(lv_scr_act(),"Running"));snapshot("motors-accepted");
 ui_page_17_motor_test_destroy();motor_test_service_poll(lv_tick_get());
 assert(sent_cmd==0x52);motor_test_service_on_reply(0x52,1);assert(!(holds&14));
 puts("PASS motor serialized ACK, leave stop and late ACK owner release");
}
static void aging_test(void){
 ui_page_26_set_aging_create(lv_scr_act());snapshot("aging-ready");click("Start test");assert(confirm);confirm(NULL);assert(sent_cmd==0x46&&holds&16);ui_page_26_set_aging_on_reply(0);assert(aging&&policy(GESTURE_ACTION_HOME));snapshot("aging-running");ui_page_26_set_aging_on_reply(2);assert(!aging&&!(holds&16));snapshot("aging-complete");ui_page_26_set_aging_destroy();assert(!policy);puts("PASS aging confirmation, running guard, legacy completion and timer cleanup");
}
static void stream_test(void){
 ui_page_28_get_image_create(lv_scr_act());snapshot("image-ready");click("Capture");assert(sent_cmd==0x47);uint8_t header[]={0,1,0,2,0,0},row[]={1,0,1,0xff,0xff,0,0};ui_page_28_get_image_on_frame(header,sizeof(header));ui_page_28_get_image_on_frame(row,sizeof(row));uint8_t end[]={0xff};ui_page_28_get_image_on_frame(end,1);assert(label_find(lv_scr_act(),"Transfer ended with missing image data. Capture again."));snapshot("image-missing");ui_page_28_get_image_destroy();
 ui_page_28_get_image_create(lv_scr_act());click("Capture");ui_page_28_get_image_on_frame(header,sizeof(header));ui_page_28_get_image_on_frame(row,sizeof(row));row[2]=2;ui_page_28_get_image_on_frame(row,sizeof(row));ui_page_28_get_image_on_frame(end,1);snapshot("image-complete");if(!label_find(lv_scr_act(),"Capture complete"))dump_labels(lv_scr_act());assert(label_find(lv_scr_act(),"Capture complete"));ui_page_28_get_image_destroy();
 ui_page_31_get_wave_create(lv_scr_act());snapshot("wave-ready");click("Capture");assert(sent_cmd==0x48);uint8_t values[257];for(unsigned channel=1;channel<=7;channel++){values[0]=channel;for(unsigned i=1;i<=256;i++)values[i]=(i*13)%256;ui_page_31_get_wave_on_frame(values,sizeof(values));}snapshot("wave-captured");assert(label_find(lv_scr_act(),"Capture complete - 7 channels received"));ui_page_31_get_wave_destroy();puts("PASS image completeness, waveform transfer and late-frame lifecycle");
}
static lv_obj_t *debug_find_type(lv_obj_t *o,const lv_obj_class_t *type){
 if(lv_obj_check_type(o,type))return o;
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *found=debug_find_type(lv_obj_get_child(o,i),type);if(found)return found;}
 return NULL;
}
static void debug_key(lv_obj_t *kb,unsigned key){lv_btnmatrix_set_selected_btn(kb,key);lv_event_send(kb,LV_EVENT_VALUE_CHANGED,NULL);}
static void debug_test(void){
 ui_page_10_debug_create();snapshot("debug-communication");
 lv_obj_t *kb=debug_find_type(lv_scr_act(),&lv_btnmatrix_class),*input=debug_find_type(lv_scr_act(),&lv_textarea_class);assert(kb&&input);
 for(unsigned i=0;i<700;i++)debug_key(kb,i%16);
 assert(strlen(lv_textarea_get_text(input))<=191&&!strncmp(lv_textarea_get_text(input),"FD DF ",6));
 for(unsigned i=0;i<220;i++)debug_key(kb,16);
 assert(!strcmp(lv_textarea_get_text(input),"FD DF "));
 debug_key(kb,10);debug_key(kb,11);assert(!strcmp(lv_textarea_get_text(input),"FD DF AB "));
 debug_key(kb,17);assert(!strcmp(lv_textarea_get_text(input),"FD DF "));
 puts("PASS Debug production HEX keyboard: long input, deletion, prefix and Clear without recursion");
 for(unsigned i=0;i<205;i++)debug_append_rx_log("FD DF 06 38 01 26");
 lv_obj_t *record=label_find(lv_scr_act(),"RX 0040: FD DF 06 38 01 26");assert(record);lv_obj_t *log=lv_obj_get_parent(record);lv_obj_scroll_to_y(log,100,LV_ANIM_OFF);lv_obj_update_layout(log);lv_area_t before,after;lv_obj_get_coords(record,&before);debug_append_rx_log("FD DF 06 38 01 26");lv_obj_get_coords(record,&after);assert(before.y1==after.y1);
 snapshot("debug-log");click("Tools");snapshot("debug-tools");click("Communication");click("Clear");ui_page_10_debug_destroy();assert(!debug_page_rx_log_is_active());tick(500);puts("PASS debug tabs, bounded log flex, retained scroll position, clear and lifecycle");
}
static void failure_retry_test(void){
 gate=true;send_ok=false;
 ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());click("Start");tick(220);assert(label_find(lv_scr_act(),"Could not start"));gate=false;tick(220);click("Retry");snapshot("cis-mode-retry");ui_page_cis_calib_destroy();
 gate=true;ui_page_12_sensor_create(lv_scr_act());snapshot("sensors-send-failed");assert(label_find(lv_scr_act(),"Could not send query. Retrying the controller connection."));gate=false;tick(320);assert(!label_find(lv_scr_act(),"Retry"));ui_page_12_sensor_destroy();
 ui_page_17_motor_test_create(lv_scr_act());click("Retry mode");ui_page_17_motor_test_destroy();
 gate=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");confirm(NULL);assert(label_find(lv_scr_act(),"Could not send"));gate=false;tick(220);click("Retry");ui_page_26_set_aging_destroy();
 gate=true;ui_page_28_get_image_create(lv_scr_act());click("Capture");snapshot("image-send-failed");assert(label_find(lv_scr_act(),"Could not send capture request. Try again."));gate=false;ui_page_28_get_image_poll(lv_tick_get());click("Retry");gate=true;ui_page_28_get_image_poll(lv_tick_get());assert(label_find(lv_scr_act(),"Could not send capture request. Try again."));ui_page_28_get_image_destroy();
 ui_page_31_get_wave_create(lv_scr_act());click("Capture");snapshot("wave-send-failed");assert(label_find(lv_scr_act(),"Could not send capture request. Try again."));gate=false;ui_page_31_get_wave_poll(lv_tick_get());click("Retry");ui_page_31_get_wave_destroy();
 ui_page_10_debug_create();click("Retry");ui_page_10_debug_destroy();assert(retries==6);
 gate=true;send_ok=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");confirm(NULL);ui_page_26_set_aging_on_timeout();unsigned before=sends;click("Start test");assert(sends==before&&(holds&16));snapshot("aging-timeout");ui_page_26_set_aging_destroy();ui_page_26_set_aging_on_reply(2);assert(!(holds&16));
 puts("PASS mode recovery, Sensor has no unrelated Retry, send failures and unknown aging result guard");
}
int main(void){
 lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);
 calibration_test();sensor_test();motor_test();aging_test();stream_test();debug_test();failure_retry_test();
 for(unsigned i=0;i<4;i++){ui_page_12_sensor_create(lv_scr_act());ui_page_12_sensor_destroy();ui_page_10_debug_create();ui_page_10_debug_destroy();ui_page_28_get_image_create(lv_scr_act());ui_page_28_get_image_destroy();ui_page_31_get_wave_create(lv_scr_act());ui_page_31_get_wave_destroy();}
 puts("PASS actual LVGL diagnostic details suite");return 0;
}
