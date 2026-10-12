#include "test_notice_sink.h"
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
#include "un260/lv_core/page_dynamic_capture.h"

#include "un260/protocol/stream_data_reply.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_frame_parser.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/lv_page_manager.h"
#include "test_settings_actions.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/ui_export_data.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/motor_test_service.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_content_pager.h"
static unsigned run_clicks;
static void settings_run_clicked(lv_event_t *event){(void)event;++run_clicks;}
const char *app_command_runtime_diagnostic_run_blocker(void){return work_mode_service_diagnostic_ready()?NULL:work_mode_service_status_text();}
const char *app_command_runtime_calibration_blocker(void){return app_command_runtime_diagnostic_run_blocker();}
#include "un260/diagnostic/diagnostic.h"
#include "un260/gesture/gesture_service.h"
#include "tools/test_page_background_asset.h"
#include "actual_diagnostic_pointer.h"

static unsigned sends,pops,retries;static uint8_t sent_cmd,sent_sub;static bool gate=true,aging,send_ok=true;
static uint32_t holds, count_end_generation;
typedef struct { lv_obj_t *root; settings_detail_dialog_cb_t confirm_cb,cancel_cb; void *user_data; } settings_dialog_ctx_t;
static settings_dialog_ctx_t g_settings_dialog;
static struct {lv_obj_t *root;} g_settings_keyboard;
bool work_mode_service_diagnostic_ready(void){return gate;}
const char *work_mode_service_status_text(void){return "Waiting for manual mode";}
void work_mode_service_hold_operation(uint32_t owner,bool active){if(active)holds|=owner;else holds&=~owner;}
void work_mode_service_retry(void){retries++;}
void work_mode_service_get_snapshot(work_mode_snapshot_t*s){memset(s,0,sizeof(*s));s->phase=gate?WORK_MODE_READY:WORK_MODE_FAILED;}
bool machine_state_aging_running(void){return aging;}
void machine_state_confirm_aging_running(bool v){aging=v;}
bool settings_detail_send_command(uint8_t cmd,const uint8_t *sub,uint16_t len){sends++;sent_cmd=cmd;sent_sub=(sub&&len)?sub[0]:0;return send_ok;}
bool setting_service_request_aging_start(void){sends++;sent_cmd=0x46;return send_ok;}
int protocol_send(uint8_t cmd,const uint8_t *sub,uint16_t len){return settings_detail_send_command(cmd,sub,len)?len+5:-1;}
void ui_manager_push_page(ui_page_t p){(void)p;}
static uint64_t capture_nav_locks, capture_gesture_locks;
void ui_manager_hold_navigation(ui_page_t owner,bool hold){uint64_t bit=UINT64_C(1)<<owner;if(hold)capture_nav_locks|=bit;else capture_nav_locks&=~bit;}
void gesture_service_set_navigation_blocked(uint32_t owner,bool hold){uint64_t bit=UINT64_C(1)<<owner;if(hold)capture_gesture_locks|=bit;else capture_gesture_locks&=~bit;}
bool ui_manager_pop_page(void){if(capture_nav_locks)return false;pops++;return true;}
static ui_page_t current_page=UI_PAGE_IMAGE_GET;
ui_page_t ui_manager_get_current_page(void){return current_page;}
void ui_manager_switch(ui_page_t p){current_page=p;}
void ui_manager_clear_stack(void){}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
uint64_t app_clock_monotonic_ms(void){return lv_tick_get();}
uint32_t app_command_runtime_count_end_generation(void){return count_end_generation;}
const char *ui_text_get(ui_text_id_t id){(void)id;return "Waiting for response";}
const char *ui_text_msgid(ui_text_id_t id){return ui_text_get(id);}

#define SETTINGS_DETAIL_W 1280
#define SETTINGS_DETAIL_H 400
#include "un260/lv_components/lv_popup_style.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/font/ui_message_font.h"
#include "actual_capture_dialog.h"
#define confirm g_settings_dialog.confirm_cb
static bool (*policy)(gesture_action_t);
static gesture_pointer_policy_t capture_pointer_policy;
static bool (*capture_drag_policy)(void);
void gesture_service_set_page_policy(uint32_t owner,bool(*drag)(void),bool(*handler)(gesture_action_t)){(void)owner;capture_drag_policy=drag;policy=handler;}
void gesture_service_clear_page_policy(uint32_t owner){(void)owner;capture_drag_policy=NULL;policy=NULL;}
void gesture_service_set_pointer_policy(uint32_t owner,gesture_pointer_policy_t observer){(void)owner;capture_pointer_policy=observer;}
void gesture_service_clear_pointer_policy(uint32_t owner){(void)owner;capture_pointer_policy=NULL;}
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


ui_export_text_result_t ui_export_text_lines(const char*p,const char*const*l,size_t n){(void)p;(void)l;return n?UI_EXPORT_TEXT_OK:UI_EXPORT_TEXT_EMPTY;}

static lv_color_t pixels[1280*400],buffer[1280*40];
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(pixels+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*sizeof(lv_color_t));lv_disp_flush_ready(d);}
static lv_res_t info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;const un260_compiled_asset_t*a=test_page_asset_find(src);if(!a){fprintf(stderr,"Missing asset: %s\n",(const char*)src);abort();}memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){
 if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;
 const un260_compiled_asset_t *asset=test_page_asset_find(s->src);
#if LV_COLOR_DEPTH == 32
 s->img_data=asset->pixels;
#else
 /* Compiled assets are BGRA; LVGL's native 16-bit alpha images use RGB565+A. */
 size_t count=(size_t)asset->width*asset->height,stride=sizeof(lv_color_t)+1;
 uint8_t *native=malloc(count*stride);assert(native);
 for(size_t i=0;i<count;i++){
  const uint8_t *pixel=asset->pixels+i*4;
  lv_color_t color=lv_color_make(pixel[2],pixel[1],pixel[0]);
  memcpy(native+i*stride,&color,sizeof(color));native[i*stride+sizeof(color)]=pixel[3];
 }
 s->img_data=native;s->user_data=native;
#endif
 return LV_RES_OK;
}
static void image_close(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){
 (void)d;
#if LV_COLOR_DEPTH != 32
 free(s->user_data);s->user_data=NULL;
#else
 (void)s;
#endif
}
static void screenshot_write(FILE *file){
#if LV_COLOR_DEPTH == 32
 assert(fwrite(pixels,sizeof(lv_color_t),1280*400,file)==1280*400);
#else
 static lv_color32_t bgra[1280*400];
 for(size_t i=0;i<1280*400;i++)bgra[i].full=lv_color_to32(pixels[i]);
 assert(fwrite(bgra,sizeof(lv_color32_t),1280*400,file)==1280*400);
#endif
}
#include "test_settings_toolbar.h"
static lv_obj_t *label_find(lv_obj_t *o,const char *text);
static void click(const char *text);
static void labels_dump(lv_obj_t *o){if(lv_obj_check_type(o,&lv_label_class))fprintf(stderr,"Label: [%s]\n",lv_label_get_text(o));for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)labels_dump(lv_obj_get_child(o,i));}
static void snapshot(const char*n){
 bool keep_run=!strncmp(n,"debug-",6);
 assert((label_find(lv_scr_act(),"RUN")!=NULL)==keep_run);
 if(keep_run){unsigned before=run_clicks;click("RUN");assert(run_clicks==before+1);}
 if(!strncmp(n,"image-",6)||!strncmp(n,"wave-",5)||!strncmp(n,"capture-",8)){
  lv_obj_update_layout(lv_scr_act());lv_obj_t *title=label_find(lv_scr_act(),"Dynamic data capture");assert(title);
  lv_area_t heading;lv_obj_get_coords(title,&heading);assert(heading.y1>=0&&heading.y2<84&&lv_obj_get_scroll_y(lv_scr_act())==0);
 }
 lv_obj_update_layout(lv_scr_act());assert_settings_toolbars(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);screenshot_write(f);fclose(f);
}
static lv_obj_t *label_find(lv_obj_t *o,const char *text){if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t*f=label_find(lv_obj_get_child(o,i),text);if(f)return f;}return NULL;}
static void click(const char *text){lv_obj_t*l=label_find(lv_scr_act(),text);if(!l){fprintf(stderr,"Missing click label: %s\n",text);labels_dump(lv_scr_act());}assert(l);while(l&&!lv_obj_has_flag(l,LV_OBJ_FLAG_CLICKABLE))l=lv_obj_get_parent(l);assert(l);assert(!lv_obj_has_flag(l,LV_OBJ_FLAG_HIDDEN));lv_event_send(l,LV_EVENT_CLICKED,NULL);}
static void tick(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
static void calibration_test(void){
 unsigned before=sends;gate=false;ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());snapshot("cis-ready");click("Start");assert(sends==before);gate=true;tick(220);click("Start");assert(sent_cmd==0x5B&&holds&1&&policy(GESTURE_ACTION_HOME));snapshot("cis-running");
 uint8_t response[]={0xfd,0xdf,6,0x5b,2,0};diagnostic_reply_hooks_t hooks={cis_calib_ui_refresh};diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);assert(label_find(lv_scr_act(),"Calibration complete"));snapshot("cis-complete");ui_page_cis_calib_destroy();assert(!policy);
 ui_page_cis_calib_select(true);ui_page_cis_calib_create(lv_scr_act());click("Start");assert(sent_cmd==0x5F);diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);assert(label_find(lv_scr_act(),"Calibration complete"));snapshot("white-balance");ui_page_cis_calib_destroy();
 ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());click("Start");
 assert(diagnostic_calibration_poll(lv_tick_get()+DIAGNOSTIC_CALIBRATION_TIMEOUT_MS+1));
 cis_calib_ui_refresh();snapshot("cis-timeout");assert(label_find(lv_scr_act(),"Waiting for a final result"));
 assert(policy(GESTURE_ACTION_HOME)&&confirm);settings_detail_dialog_hide();click("Back");assert(confirm);settings_detail_dialog_close(true);
 ui_page_cis_calib_destroy();diagnostic_reply_dispatch(0x5B,response,6,lv_tick_get(),&hooks);
 puts("PASS calibration target, no entry command, manual gate, running Home guard, 5F/5B completion and lifecycle");
}
static void sensor_test(void){
 sensor_state_clear();ui_page_12_sensor_create(lv_scr_act());assert(sent_cmd==0x1D);snapshot("sensors-waiting");for(unsigned i=0;i<11;i++)sensor_state_set_voltage(i,16+i*18);tick(320);snapshot("sensors-live");assert(label_find(lv_scr_act(),"0.206 V"));ui_page_12_sensor_destroy();unsigned before=sends;tick(500);assert(sends==before);puts("PASS sensor polling, received values and timer cleanup");
}
static void sensor_stress_test(void){
 for(unsigned round=0;round<12;round++){
  sensor_state_clear();ui_page_12_sensor_create(lv_scr_act());
  lv_obj_t *root=lv_obj_get_child(lv_scr_act(),lv_obj_get_child_cnt(lv_scr_act())-1);
  for(unsigned step=0;step<12;step++){
   uint8_t reply[]={0xfd,0xdf,7,0x1d,0,0,0};
   diagnostic_reply_dispatch(0x1d,reply,sizeof(reply),lv_tick_get(),NULL);
   for(unsigned channel=1;channel<=11;channel++){
    reply[4]=channel;reply[5]=(round+step+channel)*7;
    diagnostic_reply_dispatch(0x1d,reply,sizeof(reply),lv_tick_get(),NULL);
   }
   lv_obj_set_y(root,step%2?-20:0);tick(320);
  }
  ui_page_12_sensor_destroy();unsigned before=sends;tick(500);assert(sends==before);
 }
 puts("PASS Sensors 12 recreate/destroy cycles, 144 protocol updates, top-edge clipping and timer cleanup");
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
 ui_page_26_set_aging_create(lv_scr_act());snapshot("aging-ready");click("Start test");assert(confirm);settings_detail_dialog_close(true);assert(sent_cmd==0x46&&holds&16);ui_page_26_set_aging_on_reply(0);assert(aging&&policy(GESTURE_ACTION_HOME));snapshot("aging-running");ui_page_26_set_aging_on_reply(2);assert(!aging&&!(holds&16));snapshot("aging-complete");ui_page_26_set_aging_destroy();assert(!policy);puts("PASS aging confirmation, running guard, legacy completion and timer cleanup");
}
static const char *capture_source_names[] = {
 "Upper white","Lower white","Upper IR reflect","Lower IR reflect",
 "Upper IR transmit","Lower IR transmit","MT1","MT2","MG1","MG2",
 "MG3","MG4","MG5","MG6","Upper UV","Lower UV"
};
static lv_obj_t *capture_source_find(lv_obj_t *object,const char *text){
 if(lv_obj_check_type(object,&lv_label_class)&&!strcmp(lv_label_get_text(object),text)){
  /* The label's owning card remains identifiable when capture clears CLICKABLE. */
  lv_obj_t *button=lv_obj_get_parent(object);
  if(button&&button!=lv_scr_act()){
   if(lv_obj_get_width(button)==244&&lv_obj_get_height(button)==44)return button;
  }
 }
 for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++){
  lv_obj_t *found=capture_source_find(lv_obj_get_child(object,i),text);if(found)return found;
 }
 return NULL;
}
static lv_obj_t *capture_source_button(unsigned index){
 assert(index<16);lv_obj_t *button=capture_source_find(lv_scr_act(),capture_source_names[index]);
 if(!button){fprintf(stderr,"Missing source button: %s\n",capture_source_names[index]);labels_dump(lv_scr_act());}
 assert(button);return button;
}
static lv_obj_t *capture_source_scroll(void){
 lv_obj_t *button=capture_source_button(0);
 return lv_obj_get_parent(lv_obj_get_parent(button));
}
static lv_obj_t *capture_source_pager(void){return lv_obj_get_parent(capture_source_scroll());}
static lv_obj_t *capture_source_panel(void){return lv_obj_get_parent(capture_source_pager());}
static lv_area_t capture_selected_bounds;
static unsigned capture_selected_index;
static bool capture_selected_geometry;
static void capture_assert_fixed_geometry(void){
 for(unsigned i=0;i<16;i++){lv_obj_t *button=capture_source_button(i);assert(lv_obj_get_width(button)==244&&lv_obj_get_height(button)==44);}
 assert(lv_obj_get_scroll_y(capture_source_scroll())==0);
 if(capture_selected_geometry){lv_area_t now;lv_obj_get_coords(capture_source_button(capture_selected_index),&now);assert(!memcmp(&now,&capture_selected_bounds,sizeof(now)));assert(lv_obj_get_scroll_x(capture_source_scroll())==0);assert(lv_content_pager_get_active(capture_source_pager())==capture_selected_index/6);}
}
static lv_obj_t *capture_source_progress(lv_obj_t *button){
 for(unsigned i=0;i<lv_obj_get_child_cnt(button);i++){
  lv_obj_t *child=lv_obj_get_child(button,i);
  if(!lv_obj_check_type(child,&lv_label_class)&&lv_obj_get_style_bg_color(child,0).full==lv_color_hex(0xB9E5CC).full)return child;
 }
 assert(false);return NULL;
}
static void capture_assert_sources_locked(bool locked){
 for(unsigned i=0;i<16;i++)assert(lv_obj_has_flag(capture_source_button(i),LV_OBJ_FLAG_CLICKABLE)!=locked);
}
static void capture_assert_only_progress(int selected){
 for(unsigned i=0;i<16;i++)assert(lv_obj_has_flag(capture_source_progress(capture_source_button(i)),LV_OBJ_FLAG_HIDDEN)==((int)i!=selected));
}
static void capture_assert_pager_footer(unsigned page){
 ui_page_dynamic_capture_poll(lv_tick_get());
 lv_obj_update_layout(lv_scr_act());lv_obj_t *panel=capture_source_panel();
 lv_obj_t *caption=label_find(panel,page?"Signals":"Images");assert(caption);
 lv_area_t outer,text,view;lv_obj_get_coords(panel,&outer);lv_obj_get_coords(caption,&text);lv_obj_get_coords(capture_source_scroll(),&view);
 assert(text.x1>=outer.x1&&text.x2<=outer.x2&&text.y1>view.y2&&text.y2<=outer.y2);
 lv_obj_t *indicator=lv_obj_get_child(capture_source_pager(),1);assert(indicator&&lv_obj_get_child_cnt(indicator)==3);
 lv_area_t dots;lv_obj_get_coords(indicator,&dots);assert(dots.x1>text.x2&&dots.x2<=outer.x2&&dots.y1>view.y2&&dots.y2<=outer.y2);
 for(unsigned i=0;i<3;i++)assert(lv_obj_get_style_bg_color(lv_obj_get_child(indicator,i),0).full==lv_color_hex(i==page?0xFFFFFF:0x7F8992).full);
}
static void capture_click_source(unsigned index){
 lv_obj_t *button=capture_source_button(index);
 assert(lv_obj_has_flag(button,LV_OBJ_FLAG_CLICKABLE));
 assert(lv_content_pager_set_active(capture_source_pager(),index/6,false));ui_page_dynamic_capture_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());
 capture_selected_index=index;lv_obj_get_coords(button,&capture_selected_bounds);capture_selected_geometry=true;
 lv_event_send(button,LV_EVENT_CLICKED,NULL);
 capture_assert_fixed_geometry();
}
static void capture_prepare_note(void){
 current_page=UI_PAGE_IMAGE_GET;lv_tick_inc(1);
 ui_page_dynamic_capture_on_count_start(0,0,false);
 ui_page_dynamic_capture_on_count_info(0,0,0);
 ui_page_dynamic_capture_on_count_info(1,0,1);
 ui_page_dynamic_capture_on_count_info(0,0,2);
 assert(!label_find(lv_scr_act(),"Place one note in the hopper."));
}
static void capture_prepare_if_needed(void){ui_page_dynamic_capture_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());if(label_find(lv_scr_act(),"Place one note in the hopper."))capture_prepare_note();}
static void capture_pointer_swipe(int direction){
 lv_obj_update_layout(lv_scr_act());lv_area_t a;lv_obj_get_coords(capture_source_scroll(),&a);
 lv_point_t start={(a.x1+a.x2)/2,a.y1+20},end={start.x+direction*160,start.y};
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSED,&start,1));lv_tick_inc(40);
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSING,&end,1));lv_tick_inc(40);
 assert(capture_pointer_policy(NULL,LV_EVENT_RELEASED,&end,0));tick(320);
 ui_page_dynamic_capture_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());
}
static void capture_scroll_regression(void){
 current_page=UI_PAGE_IMAGE_GET;ui_page_dynamic_capture_create(lv_scr_act());unsigned before=sends;
 lv_obj_t *viewport=capture_source_scroll();lv_obj_update_layout(lv_scr_act());
 assert(lv_obj_get_width(viewport)==244&&lv_obj_get_height(viewport)==274);capture_assert_sources_locked(false);
 assert(lv_obj_get_width(capture_source_panel())==260&&lv_obj_get_height(capture_source_panel())==300);
 assert(!lv_obj_has_flag(viewport,LV_OBJ_FLAG_SCROLLABLE)&&lv_obj_get_scroll_snap_x(viewport)==LV_SCROLL_SNAP_NONE);
 assert(lv_content_pager_get_count(capture_source_pager())==3);
 for(unsigned i=0;i<3;i++)assert(lv_obj_get_child_cnt(lv_content_pager_get_page(capture_source_pager(),i))==(i==2?4:6));
 assert(capture_pointer_policy&&capture_drag_policy);
 for(unsigned i=0;i<16;i++){
  assert(lv_content_pager_set_active(capture_source_pager(),i/6,false));ui_page_dynamic_capture_poll(lv_tick_get());
  lv_obj_t *button=capture_source_button(i);lv_obj_update_layout(lv_scr_act());lv_area_t view,row;
  lv_obj_get_coords(viewport,&view);lv_obj_get_coords(button,&row);
  assert(row.x1==view.x1&&row.y1==view.y1+(int)(i%6)*46&&row.y2<=view.y2);
  assert(lv_obj_get_width(button)==244&&lv_obj_get_height(button)==44);
  assert(lv_obj_get_scroll_x(viewport)==0&&lv_obj_get_scroll_y(viewport)==0);capture_assert_pager_footer(i/6);
 }
 snapshot("capture-sources-page-last");capture_selected_geometry=false;
 assert(lv_content_pager_set_active(capture_source_pager(),2,false));capture_pointer_swipe(-1);
 assert(lv_content_pager_get_active(capture_source_pager())==0);capture_assert_pager_footer(0);snapshot("capture-loop-last-to-first");
 capture_pointer_swipe(1);assert(lv_content_pager_get_active(capture_source_pager())==2);capture_assert_pager_footer(2);
 for(unsigned page=0;page<3;page++){
  assert(lv_content_pager_set_active(capture_source_pager(),page,false));capture_pointer_swipe(-1);
  assert(lv_content_pager_get_active(capture_source_pager())==(page+1)%3);capture_assert_fixed_geometry();
 }
 assert(sends==before);assert(lv_content_pager_set_active(capture_source_pager(),0,false));
 /* Invisible/mid-motion cards never select or transmit. Vertical motion and
  * multi-touch cancel a press; a clean touch is delegated to the original card. */
 lv_event_send(capture_source_button(6),LV_EVENT_CLICKED,NULL);assert(sends==before);
 lv_content_pager_drag_begin(capture_source_pager());lv_content_pager_drag_update(capture_source_pager(),-120);
 lv_event_send(capture_source_button(0),LV_EVENT_CLICKED,NULL);assert(!settings_detail_overlay_is_open());
 lv_content_pager_drag_end(capture_source_pager(),true);tick(320);
 lv_area_t a;lv_obj_get_coords(viewport,&a);lv_point_t down={a.x1+50,a.y1+20},vertical={down.x,down.y+60};
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSED,&down,1));assert(capture_drag_policy());
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSING,&vertical,1));
 assert(capture_pointer_policy(NULL,LV_EVENT_RELEASED,&vertical,0));tick(320);assert(!settings_detail_overlay_is_open());
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSED,&down,1));
 assert(capture_pointer_policy(NULL,LV_EVENT_PRESSING,&down,2));
 assert(capture_pointer_policy(NULL,LV_EVENT_RELEASED,&down,0));tick(320);assert(!settings_detail_overlay_is_open());
 assert(sends==before);ui_page_dynamic_capture_destroy();
 for(unsigned cycle=0;cycle<12;cycle++){
  ui_page_dynamic_capture_create(lv_scr_act());assert(capture_pointer_policy&&capture_drag_policy);
  ui_page_dynamic_capture_destroy();assert(!capture_pointer_policy&&!capture_drag_policy);
 }
 lv_obj_t *owners[8];for(unsigned i=0;i<8;i++){owners[i]=lv_obj_create(lv_scr_act());assert(lv_content_pager_create(owners[i],0,0,244,292,3));}
 ui_page_dynamic_capture_create(lv_scr_act());assert(!capture_pointer_policy&&!capture_drag_policy&&test_notice_kind==UI_NOTICE_ERROR);
 ui_page_dynamic_capture_destroy();for(unsigned i=0;i<8;i++)lv_obj_del(owners[i]);
 ui_page_dynamic_capture_create(lv_scr_act());assert(capture_pointer_policy&&capture_drag_policy);ui_page_dynamic_capture_destroy();
 puts("PASS actual circular capture paging: 3-to-1/1-to-3, 16 fixed cards, cancelled/multitouch/vertical presses, no invisible/moving click, context lifetime");
}

static void image_select(uint8_t source);
static void stream_test(void){
 ui_page_dynamic_capture_create(lv_scr_act());snapshot("image-ready");image_select(3);assert(sent_cmd==0x47);
 uint8_t header[]={0,3,0,3,0,2},row[]={3,0,1,24,48,72},end[]={0xff};
 ui_page_dynamic_capture_on_frame(0x47,header,sizeof(header));ui_page_dynamic_capture_on_frame(0x47,row,sizeof(row));
 ui_page_dynamic_capture_on_frame(0x47,end,sizeof(end));assert(test_notice_kind==UI_NOTICE_WARNING);
 snapshot("image-missing");ui_page_dynamic_capture_destroy();lv_tick_inc(3001);
 ui_page_dynamic_capture_create(lv_scr_act());image_select(3);
 ui_page_dynamic_capture_on_frame(0x47,header,sizeof(header));ui_page_dynamic_capture_on_frame(0x47,row,sizeof(row));row[2]=2;
 ui_page_dynamic_capture_on_frame(0x47,row,sizeof(row));ui_page_dynamic_capture_on_frame(0x47,end,sizeof(end));
 snapshot("image-complete");assert(test_notice_kind==UI_NOTICE_SUCCESS);ui_page_dynamic_capture_destroy();
 puts("PASS dynamic image completeness and complete-only cache");
}
static lv_obj_t *debug_find_type(lv_obj_t *o,const lv_obj_class_t *type);
static protocol_frame_parser_t image_parser;
static void image_wire(const uint8_t *payload,uint16_t payload_len){
 uint8_t frame[776];uint16_t frame_len=payload_len+5;assert(frame_len<=sizeof(frame));
 frame[0]=0xfd;frame[1]=0xdf;frame[2]=(uint8_t)frame_len;frame[3]=0x47;
 memcpy(frame+4,payload,payload_len);frame[frame_len-1]=0x83;
 unsigned ready=0;
 for(uint16_t i=0;i<frame_len;i++){
  protocol_frame_view_t packet;
  protocol_frame_parse_result_t result=protocol_frame_parser_feed(&image_parser,frame[i],&packet);
  assert(result==PROTOCOL_FRAME_PARSE_INCOMPLETE||result==PROTOCOL_FRAME_PARSE_READY);
  if(result==PROTOCOL_FRAME_PARSE_READY){
   assert(i==frame_len-1&&packet.len==frame_len);ready++;
   stream_data_reply_view_t reply=stream_data_reply_parse(packet.data,packet.len);
   assert(reply.kind==STREAM_DATA_REPLY_IMAGE&&reply.payload_len==payload_len);
   ui_page_dynamic_capture_on_frame(0x47,reply.payload,reply.payload_len);
  }
 }
 assert(ready==1);
}
static void image_header(uint8_t source,uint16_t width,uint16_t height){
 uint8_t payload[]={0,source,width>>8,width&255,height>>8,height&255};
 image_wire(payload,sizeof(payload));
}
static void image_end(void){uint8_t end[]={0xff};image_wire(end,sizeof(end));}
static lv_obj_t *image_button(uint8_t source){assert(source>=1&&source<=6);return capture_source_button(source-1);}
static void image_select(uint8_t source){
 capture_prepare_if_needed();
 capture_click_source(source-1);
}
static void image_finish_feedback(void){tick(200);ui_page_dynamic_capture_poll(lv_tick_get());}
static void image_capture(uint8_t source){
 unsigned before=sends;image_select(source);
 if(!(sends==before+1&&sent_cmd==0x47&&sent_sub==source)){fprintf(stderr,"Capture attempt source=%u before=%u sends=%u hint=%u modal=%u current=%u gate=%u\n",source,before,sends,label_find(lv_scr_act(),"Place one note in the hopper.")!=NULL,settings_detail_overlay_is_open(),current_page,gate);labels_dump(lv_scr_act());}
 assert(sends==before+1&&sent_cmd==0x47&&sent_sub==source);
 assert(!lv_obj_has_flag(capture_source_scroll(),LV_OBJ_FLAG_SCROLLABLE));
 capture_assert_sources_locked(true);
}
static lv_obj_t *image_canvas_find(void){
 lv_obj_t *canvas=debug_find_type(lv_scr_act(),&lv_canvas_class);assert(canvas);return canvas;
}
static uint16_t image_rgb565(uint8_t source,unsigned x,unsigned y,unsigned width,unsigned height){
 if(width==400&&height==200){
  if(x<8&&y<8)return 0xf800;
  if(x>=width-8&&y<8)return 0x07e0;
  if(x<8&&y>=height-8)return 0x001f;
  if(x>=width-8&&y>=height-8)return 0xffe0;
  if(x==width/2&&y==height/2)return 0xfddf; /* An embedded header is image data. */
 }
 return source==1?0x4208:0x8410;
}
static uint8_t image_gray(uint8_t source,unsigned x,unsigned y,unsigned width,unsigned height){
 if(width==400&&height==200){
  if(x<8&&y<8)return 16;
  if(x>=width-8&&y<8)return 64;
  if(x<8&&y>=height-8)return 160;
  if(x>=width-8&&y>=height-8)return 224;
  if(y==height/2&&x==width/2)return 0xfd;
  if(y==height/2&&x==width/2+1)return 0xdf;
 }
 return source*24;
}
static void image_scanline(uint8_t source,uint16_t line,uint16_t width,uint16_t height){
 uint8_t payload[771]={source,line>>8,line&255};
 unsigned count=image_data_reply_scanline_bytes(source,width,height);
 for(unsigned byte=0;byte<count;byte++){
  size_t offset=(size_t)(line-1)*count+byte;
  if(source<=2){
   size_t pixel=offset/2;
   /* Independent controller fixture: white rows arrive right bank first.
    * Two 400-byte packets encode native x=200..399, then x=0..199. */
   unsigned native_x=(pixel%width+width/2)%width;
   uint16_t color=image_rgb565(source,native_x,pixel/width,width,height);
   payload[3+byte]=offset%2?color&255:color>>8;
  }else payload[3+byte]=image_gray(source,offset%width,offset/width,width,height);
 }
 image_wire(payload,count+3);
}
static void image_native_size(lv_obj_t *canvas,unsigned width,unsigned height){
 capture_assert_fixed_geometry();
 assert(!lv_obj_has_flag(canvas,LV_OBJ_FLAG_HIDDEN));
 char dimensions[32];snprintf(dimensions,sizeof(dimensions),"%u x %u",width,height);
 assert(label_find(lv_scr_act(),dimensions));
 const lv_img_dsc_t *image=lv_canvas_get_img(canvas);
 assert(image&&image->header.w==width&&image->header.h==height&&image->header.cf==LV_IMG_CF_TRUE_COLOR);
 assert(image->data&&lv_img_buf_get_img_size(width,height,LV_IMG_CF_TRUE_COLOR)==(size_t)width*height*sizeof(lv_color_t));
}
static void image_native_base(uint8_t source){
 lv_obj_t *canvas=image_canvas_find();image_native_size(canvas,5,3);
 lv_color_t color=lv_canvas_get_px(canvas,2,1);
 if(source<=2){
  lv_color32_t rgb={.full=lv_color_to32(color)};
  unsigned level=source==1?64:128;
  assert(abs((int)rgb.ch.red-(int)level)<=7&&abs((int)rgb.ch.green-(int)level)<=7&&abs((int)rgb.ch.blue-(int)level)<=7);
 }else assert(color.full==lv_color_make(source*24,source*24,source*24).full);
}
static void image_complete_small(uint8_t source){
 image_capture(source);image_header(source,5,3);
 unsigned lines=image_data_reply_scanline_count(source,5,3);
 for(unsigned line=lines;line;line--)image_scanline(source,line,5,3);
 image_end();assert(test_notice_kind==UI_NOTICE_SUCCESS);image_native_base(source);image_finish_feedback();
}
static void image_render_corners(uint8_t source){
 lv_obj_t *canvas=image_canvas_find();image_native_size(canvas,400,200);
 lv_color_t corners[]={lv_canvas_get_px(canvas,0,0),lv_canvas_get_px(canvas,399,0),
                       lv_canvas_get_px(canvas,0,199),lv_canvas_get_px(canvas,399,199)};
 if(source<=2){
  const unsigned char expected[4][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,0}};
  for(unsigned i=0;i<4;i++){
   lv_color32_t rgb={.full=lv_color_to32(corners[i])};
   assert(abs((int)rgb.ch.red-expected[i][0])<=7&&abs((int)rgb.ch.green-expected[i][1])<=7&&abs((int)rgb.ch.blue-expected[i][2])<=7);
  }
 }else{
  const uint8_t gray[]={16,64,160,224};
  for(unsigned i=0;i<4;i++)assert(corners[i].full==lv_color_make(gray[i],gray[i],gray[i]).full);
  assert(lv_canvas_get_px(canvas,200,100).full==lv_color_make(0xfd,0xfd,0xfd).full);
  assert(lv_canvas_get_px(canvas,201,100).full==lv_color_make(0xdf,0xdf,0xdf).full);
 }
 assert(lv_img_get_zoom(canvas)==384);
 char name[64];snprintf(name,sizeof(name),"image-source-%u-400x200",source);snapshot(name);
 lv_area_t film;lv_obj_get_coords(lv_obj_get_parent(canvas),&film);
 int expected_w=(400*lv_img_get_zoom(canvas)+255)/256;
 int expected_h=(200*lv_img_get_zoom(canvas)+255)/256;
 int center_x=(film.x1+film.x2)/2,center_y=(film.y1+film.y2)/2;
 int x1=1280,y1=400,x2=-1,y2=-1;
 int counts[4]={0},cx1[4]={1280,1280,1280,1280},cy1[4]={400,400,400,400};
 int cx2[4]={-1,-1,-1,-1},cy2[4]={-1,-1,-1,-1};
 for(int y=film.y1;y<=film.y2;y++)for(int x=film.x1;x<=film.x2;x++){
  assert(x>=0&&x<1280&&y>=0&&y<400);
  /* Native 16-bit interpolation can create a marker's color near the central FDDF sample. */
  for(unsigned i=0;i<4;i++)if(pixels[y*1280+x].full==corners[i].full&&
      (i%2?x>center_x+expected_w/4:x<center_x-expected_w/4)&&
      (i>=2?y>center_y+expected_h/4:y<center_y-expected_h/4)){
   counts[i]++;if(x<cx1[i])cx1[i]=x;if(x>cx2[i])cx2[i]=x;
   if(y<cy1[i])cy1[i]=y;
   if(y>cy2[i])cy2[i]=y;
   if(x<x1)x1=x;
   if(x>x2)x2=x;
   if(y<y1)y1=y;
   if(y>y2)y2=y;
  }
 }
 for(unsigned i=0;i<4;i++)assert(counts[i]>=16);
 assert(x2-x1+1>=596&&abs((x2-x1+1)-expected_w)<=2&&abs((y2-y1+1)-expected_h)<=2);
 /* Exact-color edges may move two pixels per axis after native-color interpolation. */
 if(abs((x2-x1+1)-2*(y2-y1+1))>3)fprintf(stderr,"Rendered source %u marker bounds %dx%d, transform %dx%d\n",source,x2-x1+1,y2-y1+1,expected_w,expected_h);
 assert(abs((x2-x1+1)-2*(y2-y1+1))<=6);
 assert(x1>=film.x1&&x2<=film.x2&&y1>=film.y1&&y2<=film.y2);
 assert(cx2[0]<cx1[1]&&cx2[2]<cx1[3]&&cy2[0]<cy1[2]&&cy2[1]<cy1[3]);
 assert(abs(cx1[0]-x1)<=2&&abs(cx1[2]-x1)<=2&&abs(cx2[1]-x2)<=2&&abs(cx2[3]-x2)<=2);
 assert(abs(cy1[0]-y1)<=2&&abs(cy1[1]-y1)<=2&&abs(cy2[2]-y2)<=2&&abs(cy2[3]-y2)<=2);
}
static bool image_probe_pending;
static bool image_pending_probe(uint8_t command,uint64_t started,uint64_t deadline){
 (void)started;(void)deadline;return image_probe_pending&&(command==0x47||command==0x48);
}
static void image_transfer_regression_test(void){
 protocol_frame_parser_init(&image_parser);unsigned before=sends;
 ui_page_dynamic_capture_create(lv_scr_act());assert(sends==before);
 assert(!label_find(lv_scr_act(),"RUN")&&!label_find(lv_scr_act(),"Capture"));
 for(unsigned id=0;id<16;id++)assert(lv_obj_get_style_bg_color(capture_source_button(id),0).full==lv_color_hex(0xF4F6F7).full);
 lv_obj_update_layout(lv_scr_act());if(!label_find(lv_scr_act(),"Place one note in the hopper."))labels_dump(lv_scr_act());assert(label_find(lv_scr_act(),"Place one note in the hopper."));snapshot("image-ready");
 capture_prepare_note();
 assert(!label_find(lv_scr_act(),"Place one note in the hopper."));
 for(uint8_t source=1;source<=6;source++){
  image_capture(source);assert(lv_obj_get_style_bg_color(image_button(source),0).full==lv_color_hex(0xFFFFFF).full);
  unsigned locked_sends=sends;
  for(unsigned id=0;id<16;id++)lv_event_send(capture_source_button(id),LV_EVENT_CLICKED,NULL);
  assert(sends==locked_sends);image_header(source,400,200);
  unsigned lines=image_data_reply_scanline_count(source,400,200);
  image_scanline(source,lines,400,200);image_scanline(source,lines,400,200);
  for(unsigned line=lines-1;line;line--){
   image_scanline(source,line,400,200);
   if(source==1&&line==lines/2+1){
    ui_page_dynamic_capture_poll(lv_tick_get());tick(240);lv_obj_update_layout(lv_scr_act());
    lv_obj_t *fill=capture_source_progress(image_button(source));
    if(lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN)||abs(lv_obj_get_width(fill)-lv_obj_get_width(image_button(source))/2)>2)
     fprintf(stderr,"Progress hidden=%u width=%d style=%d\n",lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN),lv_obj_get_width(fill),lv_obj_get_style_width(fill,0));
    assert(!lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN)&&abs(lv_obj_get_width(fill)-lv_obj_get_width(image_button(source))/2)<=2);
    assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
   const lv_img_dsc_t *partial=lv_canvas_get_img(image_canvas_find());assert(partial&&partial->data);
   assert(lv_canvas_get_px(image_canvas_find(),0,199).full!=lv_color_hex(0xEFF4F8).full);
   assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_hex(0xEFF4F8).full);
   snapshot("image-progress");
   }
  }
  ui_page_dynamic_capture_poll(lv_tick_get());
  assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
  assert(ui_page_dynamic_capture_is_capturing() && capture_nav_locks && capture_gesture_locks);
  unsigned back_before=pops;click("Back");assert(pops==back_before);
  assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED&&pops==back_before);
  image_end();assert(!ui_page_dynamic_capture_is_capturing() && !capture_nav_locks && !capture_gesture_locks);assert(test_notice_kind==UI_NOTICE_SUCCESS);image_render_corners(source);
  image_finish_feedback();
  capture_assert_sources_locked(false);
  assert(lv_obj_has_flag(capture_source_progress(image_button(source)),LV_OBJ_FLAG_HIDDEN));
 }
 before=sends;
 for(uint8_t source=1;source<=6;source++){image_select(source);image_native_size(image_canvas_find(),400,200);}
 assert(sends==before);snapshot("image-cached");ui_page_dynamic_capture_destroy();

 /* Odd byte block boundaries and reverse-order blocks must preserve RGB pairs. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
 lv_obj_update_layout(lv_scr_act());if(!label_find(lv_scr_act(),"Place one note in the hopper."))labels_dump(lv_scr_act());assert(label_find(lv_scr_act(),"Place one note in the hopper."));
 for(uint8_t source=1;source<=6;source++)image_complete_small(source);
 before=sends;for(uint8_t source=1;source<=6;source++){image_select(source);image_native_base(source);}assert(sends==before);
 ui_page_dynamic_capture_destroy();

 /* Partial data is never a successful cache. Retry by tapping that same source. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
 image_capture(3);image_header(3,5,3);image_scanline(3,1,5,3);image_scanline(3,1,5,3);
 uint8_t bad_zero[]={3,0,0,1,2,3,4,5},bad_boundary[]={3,0,4,1,2,3,4,5};
 uint8_t bad_short[]={3,0,2,1,2,3,4},bad_source[]={7,0,2,1,2,3,4,5};
 ui_page_dynamic_capture_on_frame(0x47,bad_zero,sizeof(bad_zero));ui_page_dynamic_capture_on_frame(0x47,bad_boundary,sizeof(bad_boundary));
 ui_page_dynamic_capture_on_frame(0x47,bad_short,sizeof(bad_short));ui_page_dynamic_capture_on_frame(0x47,bad_source,sizeof(bad_source));
 ui_page_dynamic_capture_poll(lv_tick_get());assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 image_end();assert(test_notice_kind==UI_NOTICE_WARNING&&lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 assert(!capture_nav_locks&&!capture_gesture_locks);
 before=sends;image_complete_small(3);assert(sends==before+1);
 image_complete_small(1);image_select(3);image_native_base(3);ui_page_dynamic_capture_destroy();

 const uint16_t unsupported[][2]={{0,3},{5,0},{513,3},{5,385}};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++){
  protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
  image_capture(3);image_header(3,unsupported[i][0],unsupported[i][1]);assert(test_notice_kind==UI_NOTICE_ERROR&&ui_page_dynamic_capture_is_capturing());
  before=sends;lv_event_send(image_button(3),LV_EVENT_CLICKED,NULL);assert(sends==before);
  image_end();assert(!capture_nav_locks&&!capture_gesture_locks);
  lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());image_complete_small(3);ui_page_dynamic_capture_destroy();
 }

 /* A frame waiting in RX protects an expired UI deadline; absence is a real timeout. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());image_capture(3);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(5001);assert(!ui_page_dynamic_capture_poll(lv_tick_get()));
 image_probe_pending=false;assert(ui_page_dynamic_capture_poll(lv_tick_get()));
 assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 protocol_request_set_pending_reply_probe(NULL);before=sends;
 lv_event_send(image_button(1),LV_EVENT_CLICKED,NULL);assert(sends==before);
 lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());image_complete_small(3);ui_page_dynamic_capture_destroy();

 /* Leave cancels UI ownership, drops late data and frees all six caches. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
 image_capture(3);image_header(3,5,3);image_scanline(3,1,5,3);ui_page_dynamic_capture_destroy();
 ui_page_dynamic_capture_create(lv_scr_act());before=sends;
 lv_event_send(image_button(3),LV_EVENT_CLICKED,NULL);assert(sends==before);
 image_scanline(3,2,5,3);image_end();assert(lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());image_complete_small(3);ui_page_dynamic_capture_destroy();
 /* Once a header starts transmission, allocation failure drains until FF. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());image_capture(1);
 lv_mem_monitor_t memory;void *pressure[512];unsigned pressure_count=0;
 while(pressure_count<512){
  lv_mem_monitor(&memory);if(memory.free_biggest_size<131072)break;
  void *block=lv_mem_alloc(65536);if(!block)break;pressure[pressure_count++]=block;
 }
 assert(pressure_count);image_header(1,512,384);assert(test_notice_kind==UI_NOTICE_ERROR);
 assert(ui_page_dynamic_capture_is_capturing()&&capture_nav_locks&&capture_gesture_locks);
 while(pressure_count)lv_mem_free(pressure[--pressure_count]);
 image_end();assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 ui_page_dynamic_capture_destroy();lv_tick_inc(3001);
 /* Duplicate packets and duplicate headers cannot keep an incomplete request alive. */
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
 image_capture(1);image_header(1,400,200);image_scanline(1,1,400,200);
 ui_page_dynamic_capture_poll(lv_tick_get());image_native_size(image_canvas_find(),400,200);
 assert(lv_canvas_get_px(image_canvas_find(),399,0).full==lv_color_make(0,255,0).full);
 assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_hex(0xEFF4F8).full);
 snapshot("image-first-real-block");image_scanline(1,2,400,200);ui_page_dynamic_capture_poll(lv_tick_get());
 assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_make(255,0,0).full);
 assert(lv_canvas_get_px(image_canvas_find(),0,1).full==lv_color_hex(0xEFF4F8).full);
 snapshot("image-first-real-row");ui_page_dynamic_capture_destroy();lv_tick_inc(3001);
 protocol_frame_parser_init(&image_parser);ui_page_dynamic_capture_create(lv_scr_act());
 image_capture(1);image_header(1,5,3);image_scanline(1,2,5,3);
 ui_page_dynamic_capture_poll(lv_tick_get());image_native_size(image_canvas_find(),5,3);
 assert(lv_canvas_get_px(image_canvas_find(),4,0).full==lv_color_hex(0xEFF4F8).full);
 image_scanline(1,1,5,3);ui_page_dynamic_capture_poll(lv_tick_get());
 assert(lv_canvas_get_px(image_canvas_find(),4,0).full!=lv_color_hex(0xEFF4F8).full);
 for(unsigned i=0;i<5;i++){lv_tick_inc(1000);image_header(1,5,3);image_scanline(1,1,5,3);}
 assert(ui_page_dynamic_capture_poll(lv_tick_get())&&capture_nav_locks&&capture_gesture_locks);image_end();assert(!capture_nav_locks&&!capture_gesture_locks);
 ui_page_dynamic_capture_destroy();
 lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());image_capture(3);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(120001);assert(ui_page_dynamic_capture_poll(lv_tick_get()));
 assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 image_probe_pending=false;protocol_request_set_pending_reply_probe(NULL);ui_page_dynamic_capture_destroy();
 /* Bursts may update more often than the animation timer. Progress must track
  * received packets immediately, without repeatedly restarting a width tween. */
 for(uint8_t source=1;source<=6;source++){
  lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());image_capture(source);
  image_header(source,400,200);unsigned total=image_data_reply_scanline_count(source,400,200);
  lv_obj_t *fill=capture_source_progress(image_button(source));int previous=4;
  for(unsigned packet=1;packet<=total;packet++){
   image_scanline(source,packet,400,200);
   if(packet%20!=0&&packet!=total)continue;
   ui_page_dynamic_capture_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());
   unsigned percent=packet*100U/total;if(percent>99)percent=99;
   int expected=(unsigned)lv_obj_get_width(image_button(source))*percent/100U;if(expected<4)expected=4;
   assert(!lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN));
   assert(lv_obj_get_width(fill)==expected&&lv_obj_get_width(fill)>=previous);
   previous=lv_obj_get_width(fill);
   image_scanline(source,packet,400,200);ui_page_dynamic_capture_poll(lv_tick_get());
   lv_obj_update_layout(lv_scr_act());assert(lv_obj_get_width(fill)==previous);
  }
  assert(previous<lv_obj_get_width(image_button(source))&&ui_page_dynamic_capture_is_capturing());image_end();
  tick(110);lv_obj_update_layout(lv_scr_act());assert(lv_obj_get_width(fill)==lv_obj_get_width(image_button(source)));
  ui_page_dynamic_capture_destroy();
 }
 puts("PASS click-to-capture, empty selection, no RUN/Capture, locked input, real packet progress, progressive preview with complete-only six-source cache, 400x200 right-bank-first RGB565/linear gray corners, odd byte boundaries, failure/retry, queued-RX timeout protection and destroy/recreate late guard");
}
static void wave_wire_payload(const uint8_t *payload,uint16_t count){
 assert(count>0&&count+5<=255);uint8_t frame[255]={0xfd,0xdf,0,0x48};
 frame[2]=(uint8_t)(count+5);memcpy(frame+4,payload,count);frame[count+4]=0x83;
 stream_data_reply_view_t reply=stream_data_reply_parse(frame,count+5);
 assert(reply.kind==STREAM_DATA_REPLY_WAVE&&reply.payload_len==count);
 ui_page_dynamic_capture_on_frame(0x48,reply.payload,reply.payload_len);
}
static void wave_wire_values(uint8_t type,const uint8_t *values,uint16_t count){
 assert(type>=1&&type<=10&&count&&count<=249);uint8_t payload[250]={type};
 memcpy(payload+1,values,count);wave_wire_payload(payload,count+1);
}
static void wave_wire_channel(uint8_t type,uint8_t value){
 uint8_t values[244];memset(values,value,sizeof(values));wave_wire_values(type,values,sizeof(values));
}
static void wave_end(void){uint8_t end[]={0xff};wave_wire_payload(end,sizeof(end));}
static void wave_capture(uint8_t type){
 capture_prepare_if_needed();
 unsigned before=sends;capture_click_source(5+type);
 assert(sends==before+1&&sent_cmd==0x48&&sent_sub==type);
 assert(ui_page_dynamic_capture_is_capturing()&&capture_nav_locks&&capture_gesture_locks);
 assert(!lv_obj_has_flag(capture_source_scroll(),LV_OBJ_FLAG_SCROLLABLE));
 capture_assert_sources_locked(true);
}
static bool capture_effectively_hidden(lv_obj_t *object){
 for(;object;object=lv_obj_get_parent(object))if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return true;
 return false;
}
static lv_obj_t *wave_plot_find(lv_obj_t *object){
 if(lv_obj_check_type(object,&lv_line_class)&&lv_obj_get_style_line_color(object,0).full==lv_color_hex(0x1462CC).full)return object;
 for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++){lv_obj_t *found=wave_plot_find(lv_obj_get_child(object,i));if(found)return found;}
 return NULL;
}
static lv_line_t *wave_visible_line(void){
 capture_assert_fixed_geometry();
 lv_obj_t *line=wave_plot_find(lv_scr_act());assert(line);
 assert(!capture_effectively_hidden(line));return (lv_line_t*)line;
}
static void wave_grid_bounds(lv_obj_t *host,lv_coord_t *top,lv_coord_t *bottom){
 unsigned horizontal=0;*top=INT16_MAX;*bottom=INT16_MIN;
 for(unsigned i=0;i<lv_obj_get_child_cnt(host);i++){
  lv_obj_t *child=lv_obj_get_child(host,i);
  if(!lv_obj_check_type(child,&lv_line_class)||lv_obj_get_style_line_color(child,0).full!=lv_color_hex(0xE2E7EA).full)continue;
  lv_line_t *grid=(lv_line_t*)child;
  if(grid->point_num==2&&grid->point_array[0].y==grid->point_array[1].y){
   lv_coord_t y=grid->point_array[0].y;if(y<*top)*top=y;if(y>*bottom)*bottom=y;horizontal++;
  }
 }
 assert(horizontal>=3&&*bottom>*top);
 assert(label_find(host,"255")&&label_find(host,"128")&&label_find(host,"0"));
}
static void wave_expect_constant(uint16_t count,uint8_t value){
 lv_line_t *signal=wave_visible_line();assert(signal->point_num==count);
 lv_obj_t *host=lv_obj_get_parent((lv_obj_t*)signal);lv_obj_update_layout(host);
 lv_coord_t top,bottom;wave_grid_bounds(host,&top,&bottom);
 int expected=bottom-(bottom-top)*(int)value/255;
 for(unsigned i=0;i<count;i++){
  assert(signal->point_array[i].y==signal->point_array[0].y);
  assert(abs(signal->point_array[i].y-expected)<=1);
  assert(signal->point_array[i].x>=0&&signal->point_array[i].x<lv_obj_get_width(host));
  assert(signal->point_array[i].y>=0&&signal->point_array[i].y<lv_obj_get_height(host));
  if(i)assert(signal->point_array[i].x>=signal->point_array[i-1].x);
 }
}
static void wave_transfer_regression_test(void){
 uint8_t no_data[]={0},unknown[]={11,0xdb,0xda},empty[]={4},bad_end[]={0xff,1};
 lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());
 assert(!label_find(lv_scr_act(),"RUN")&&!label_find(lv_scr_act(),"Capture"));
 for(unsigned i=0;i<16;i++)assert(lv_obj_get_style_bg_color(capture_source_button(i),0).full==lv_color_hex(0xF4F6F7).full);
 wave_capture(1);unsigned before=sends,blocked_pops=pops;
 for(unsigned i=0;i<16;i++)lv_event_send(capture_source_button(i),LV_EVENT_CLICKED,NULL);
 assert(sends==before);click("Back");assert(pops==blocked_pops);
 assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED&&pops==blocked_pops);
 wave_wire_channel(2,48);ui_page_dynamic_capture_poll(lv_tick_get());
 assert(ui_page_dynamic_capture_is_capturing());
 lv_obj_t *line=wave_plot_find(lv_scr_act());assert(line&&capture_effectively_hidden(line));
 uint8_t header[]={0,1,1,144,0,200},image_end_payload[]={0xff};
 ui_page_dynamic_capture_on_frame(0x47,header,sizeof(header));
 ui_page_dynamic_capture_on_frame(0x47,image_end_payload,sizeof(image_end_payload));
 assert(ui_page_dynamic_capture_is_capturing());
 uint8_t range[]={0,128,255};wave_wire_values(1,range,sizeof(range));
 ui_page_dynamic_capture_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());
 lv_line_t *signal=wave_visible_line();assert(signal->point_num==3);
 assert(signal->point_array[0].y>signal->point_array[1].y&&signal->point_array[1].y>signal->point_array[2].y);
 assert(signal->point_array[0].y-signal->point_array[2].y>200);
 assert(abs(2*signal->point_array[1].y-signal->point_array[0].y-signal->point_array[2].y)<=2);
 lv_obj_t *viewport=capture_source_panel(),*host=lv_obj_get_parent((lv_obj_t*)signal);
 while(lv_obj_get_height(host)!=300&&lv_obj_get_parent(host))host=lv_obj_get_parent(host);
 assert(lv_obj_get_height(host)==300);lv_area_t left,right;lv_obj_get_coords(viewport,&left);lv_obj_get_coords(host,&right);
 assert(left.y1==right.y1&&left.y2==right.y2);
 uint8_t narrow[]={64,128,192};wave_wire_values(1,narrow,sizeof(narrow));ui_page_dynamic_capture_poll(lv_tick_get());
 signal=wave_visible_line();assert(signal->point_num==3);
 lv_coord_t grid_top,grid_bottom;wave_grid_bounds(lv_obj_get_parent((lv_obj_t*)signal),&grid_top,&grid_bottom);
 assert(signal->point_array[0].y<grid_bottom&&signal->point_array[2].y>grid_top);
 assert(abs((signal->point_array[0].y-signal->point_array[2].y)*255-(grid_bottom-grid_top)*128)<=255);
 wave_wire_channel(1,24);ui_page_dynamic_capture_poll(lv_tick_get());wave_expect_constant(244,24);
 assert(label_find(lv_scr_act(),"Receiving signal: 244 samples")&&label_find(lv_scr_act(),"244 samples"));
 lv_coord_t old_y=wave_visible_line()->point_array[0].y;
 uint8_t replacement[]={190,190,190,190,190};wave_wire_values(1,replacement,sizeof(replacement));
 ui_page_dynamic_capture_poll(lv_tick_get());wave_expect_constant(sizeof(replacement),190);
 assert(wave_visible_line()->point_array[0].y<old_y); /* Replace, never append. */
 unsigned notices=test_notice_count;
 ui_page_dynamic_capture_on_frame(0x48,empty,sizeof(empty));
 ui_page_dynamic_capture_on_frame(0x48,unknown,sizeof(unknown));
 ui_page_dynamic_capture_on_frame(0x48,bad_end,sizeof(bad_end));
 assert(test_notice_count==notices&&ui_page_dynamic_capture_is_capturing());
 uint8_t sample_ff[]={0xff,0xff};wave_wire_values(1,sample_ff,sizeof(sample_ff));
 ui_page_dynamic_capture_poll(lv_tick_get());assert(ui_page_dynamic_capture_is_capturing());wave_expect_constant(2,255);capture_assert_only_progress(6);
 lv_obj_t *moving=capture_source_progress(capture_source_button(6));
 lv_coord_t initial_x=lv_obj_get_x(moving);tick(260);lv_obj_update_layout(lv_scr_act());
 assert(!lv_obj_has_flag(moving,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_x(moving)>initial_x&&lv_obj_get_x(moving)<lv_obj_get_width(capture_source_button(6)));
 assert(lv_obj_get_width(moving)>0&&lv_obj_get_width(moving)<lv_obj_get_width(capture_source_button(6)));
 snapshot("wave-selected-partial");notices=test_notice_count;wave_end();assert(test_notice_count==notices+1);
 assert(test_notice_kind==UI_NOTICE_SUCCESS&&!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 assert(!lv_obj_has_flag(capture_source_scroll(),LV_OBJ_FLAG_SCROLLABLE));
 capture_assert_sources_locked(true);before=sends;lv_event_send(capture_source_button(7),LV_EVENT_CLICKED,NULL);assert(sends==before);
 tick(120);lv_obj_update_layout(lv_scr_act());
 assert(lv_obj_get_width(capture_source_progress(capture_source_button(6)))==lv_obj_get_width(capture_source_button(6)));
 lv_tick_inc(59);ui_page_dynamic_capture_poll(lv_tick_get());capture_assert_sources_locked(true);
 lv_tick_inc(2);ui_page_dynamic_capture_poll(lv_tick_get());capture_assert_sources_locked(false);capture_assert_only_progress(-1);
 for(uint8_t type=2;type<=10;type++){
  wave_capture(type);
  if(type==10){uint8_t maximum[249];memset(maximum,type*20,sizeof(maximum));wave_wire_values(type,maximum,sizeof(maximum));}
  else wave_wire_channel(type,type*20);
  ui_page_dynamic_capture_poll(lv_tick_get());wave_expect_constant(type==10?249:244,type*20);
  assert(ui_page_dynamic_capture_is_capturing());notices=test_notice_count;wave_end();assert(test_notice_count==notices+1&&test_notice_kind==UI_NOTICE_SUCCESS);
  image_finish_feedback();capture_assert_sources_locked(false);
 }
 before=sends;for(unsigned type=1;type<=10;type++){capture_click_source(5+type);wave_expect_constant(type==1?2:type==10?249:244,type==1?255:type*20);}
 assert(sends==before);snapshot("wave-ten-source-cache");ui_page_dynamic_capture_destroy();
 /* Empty END cannot create a complete cache or wait for nine other types. */
 ui_page_dynamic_capture_create(lv_scr_act());wave_capture(10);wave_end();
 assert(test_notice_kind!=UI_NOTICE_SUCCESS&&!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 line=wave_plot_find(lv_scr_act());assert(line&&capture_effectively_hidden(line));
 ui_page_dynamic_capture_destroy();lv_tick_inc(3001);
 /* NO_DATA and a real timeout release ownership and keep the 3-second retry guard. */
 ui_page_dynamic_capture_create(lv_scr_act());wave_capture(3);wave_wire_channel(3,72);
 ui_page_dynamic_capture_on_frame(0x48,no_data,sizeof(no_data));
 assert(test_notice_kind!=UI_NOTICE_SUCCESS&&ui_page_dynamic_capture_is_capturing()&&capture_nav_locks&&capture_gesture_locks);wave_end();
 before=sends;lv_event_send(capture_source_button(8),LV_EVENT_CLICKED,NULL);assert(sends==before);assert(settings_detail_overlay_is_open());settings_detail_dialog_hide();
 lv_tick_inc(2999);ui_page_dynamic_capture_poll(lv_tick_get());lv_event_send(capture_source_button(8),LV_EVENT_CLICKED,NULL);assert(sends==before);settings_detail_dialog_hide();
 lv_tick_inc(1);ui_page_dynamic_capture_poll(lv_tick_get());wave_capture(3);
 lv_tick_inc(5001);assert(ui_page_dynamic_capture_poll(lv_tick_get()));
 assert(!ui_page_dynamic_capture_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 before=sends;lv_event_send(capture_source_button(8),LV_EVENT_CLICKED,NULL);assert(sends==before);
 notices=test_notice_count;wave_wire_channel(3,73);wave_end();assert(test_notice_count==notices);
 ui_page_dynamic_capture_destroy();
 /* Actual timely arrival survives delayed UI dispatch; every valid same-type replacement touches inactivity. */
 lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());wave_capture(4);
 uint64_t arrival=lv_tick_get()+4999;lv_tick_inc(5001);
 protocol_request_result_context_begin(arrival);wave_wire_channel(4,96);protocol_request_result_context_end();
 assert(ui_page_dynamic_capture_is_capturing()&&!ui_page_dynamic_capture_poll(lv_tick_get()));
 for(unsigned i=0;i<6;i++){lv_tick_inc(1000);wave_wire_channel(4,96+i);assert(!ui_page_dynamic_capture_poll(lv_tick_get()));}
 assert(ui_page_dynamic_capture_is_capturing());wave_end();assert(test_notice_kind==UI_NOTICE_SUCCESS);ui_page_dynamic_capture_destroy();
 /* Destruction releases the unified owner, drops late frames and guards a rebuilt page. */
 ui_page_dynamic_capture_create(lv_scr_act());wave_capture(5);wave_wire_channel(5,100);ui_page_dynamic_capture_destroy();
 assert(!capture_nav_locks&&!capture_gesture_locks&&!capture_pointer_policy);ui_page_dynamic_capture_create(lv_scr_act());assert(capture_pointer_policy);
 before=sends;lv_event_send(capture_source_button(10),LV_EVENT_CLICKED,NULL);assert(sends==before);
 wave_wire_channel(5,101);wave_end();line=wave_plot_find(lv_scr_act());assert(line&&capture_effectively_hidden(line));
 lv_tick_inc(3001);ui_page_dynamic_capture_poll(lv_tick_get());wave_capture(5);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(120001);assert(ui_page_dynamic_capture_poll(lv_tick_get()));
 assert(!capture_nav_locks&&!capture_gesture_locks);image_probe_pending=false;protocol_request_set_pending_reply_probe(NULL);
 ui_page_dynamic_capture_destroy();
 puts("PASS ten selected magnetic types, 250-byte frames, same-type replacement, fixed 0..255 signal, FF exact END, complete-only cache, wrong-command/type guard, NO_DATA/timeout/retry, timely RX and unified lifecycle");
}

static lv_obj_t *debug_find_type(lv_obj_t *o,const lv_obj_class_t *type){
 if(lv_obj_check_type(o,type))return o;
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *found=debug_find_type(lv_obj_get_child(o,i),type);if(found)return found;}
 return NULL;
}
static void debug_key(lv_obj_t *kb,unsigned key){lv_btnmatrix_set_selected_btn(kb,key);lv_event_send(kb,LV_EVENT_VALUE_CHANGED,NULL);}
static void debug_run_layout(void){
 lv_obj_update_layout(lv_scr_act());
 lv_obj_t *run=lv_obj_get_parent(label_find(lv_scr_act(),"RUN"));
 lv_obj_t *tabs=lv_obj_get_parent(lv_obj_get_parent(label_find(lv_scr_act(),"Tools")));
 lv_obj_t *back=lv_obj_get_parent(label_find(lv_scr_act(),"Back"));
 assert(!action_blocked(run)&&!lv_obj_has_state(run,LV_STATE_DISABLED)&&!lv_obj_has_flag(run,LV_OBJ_FLAG_HIDDEN));
 lv_area_t tab_area,actions_area,back_area;
 lv_obj_get_coords(tabs,&tab_area);lv_obj_get_coords(lv_obj_get_parent(run),&actions_area);lv_obj_get_coords(back,&back_area);
 assert(tab_area.x2<actions_area.x1&&actions_area.x2<back_area.x1);
 assert(tab_area.y1==actions_area.y1&&tab_area.y2==actions_area.y2);
 unsigned before=run_clicks;click("RUN");assert(run_clicks==before+1);
}
#include "test_capture_fresh_run_cases.h"
static void debug_test(void){
 ui_page_10_debug_create();snapshot("debug-communication");debug_run_layout();
 lv_obj_t *kb=debug_find_type(lv_scr_act(),&lv_btnmatrix_class),*input=debug_find_type(lv_scr_act(),&lv_textarea_class);assert(kb&&input);
 for(unsigned i=0;i<700;i++)debug_key(kb,i%16);
 assert(strlen(lv_textarea_get_text(input))<=191&&!strncmp(lv_textarea_get_text(input),"FD DF ",6));
 for(unsigned i=0;i<220;i++)debug_key(kb,16);
 assert(!strcmp(lv_textarea_get_text(input),"FD DF "));
 debug_key(kb,10);debug_key(kb,11);assert(!strcmp(lv_textarea_get_text(input),"FD DF AB "));
 debug_key(kb,17);assert(!strcmp(lv_textarea_get_text(input),"FD DF "));
 /* Actions use logical key IDs, independent of the displayed language. */
 const char **original_map=lv_btnmatrix_get_map(kb);
 static const char *translated_map[]={"1","2","3","4","5","6","\n","7","8","9","0","A","B","\n","C","D","E","F","Supprimer","Effacer",""};
 lv_btnmatrix_set_map(kb,translated_map);
 debug_key(kb,10);debug_key(kb,11);debug_key(kb,16);
 assert(!strcmp(lv_textarea_get_text(input),"FD DF A"));
 debug_key(kb,17);assert(!strcmp(lv_textarea_get_text(input),"FD DF "));
 lv_btnmatrix_set_map(kb,original_map);
 puts("PASS Debug production HEX keyboard: long input, deletion, prefix and Clear without recursion");
 for(unsigned i=0;i<205;i++)debug_append_rx_log("FD DF 06 38 01 26");
 lv_obj_t *record=label_find(lv_scr_act(),"RX 0040: FD DF 06 38 01 26");assert(record);lv_obj_t *log=lv_obj_get_parent(record);lv_obj_scroll_to_y(log,100,LV_ANIM_OFF);lv_obj_update_layout(log);lv_area_t before,after;lv_obj_get_coords(record,&before);debug_append_rx_log("FD DF 06 38 01 26");lv_obj_get_coords(record,&after);assert(before.y1==after.y1);
 snapshot("debug-log");click("Tools");snapshot("debug-tools");debug_run_layout();click("Communication");click("Clear");ui_page_10_debug_destroy();assert(!debug_page_rx_log_is_active());tick(500);puts("PASS debug RUN on both tabs, non-overlapping toolbar, bounded log flex, retained scroll position, clear and lifecycle");
}
static void failure_retry_test(void){
 gate=true;send_ok=false;
 ui_page_cis_calib_select(false);ui_page_cis_calib_create(lv_scr_act());click("Start");tick(220);assert(label_find(lv_scr_act(),"Could not start"));gate=false;tick(220);click("Retry");snapshot("cis-mode-retry");ui_page_cis_calib_destroy();
 gate=true;ui_page_12_sensor_create(lv_scr_act());snapshot("sensors-send-failed");assert(label_find(lv_scr_act(),"Could not send query. Retrying the controller connection."));gate=false;tick(320);assert(!label_find(lv_scr_act(),"Retry"));ui_page_12_sensor_destroy();
 ui_page_17_motor_test_create(lv_scr_act());click("Retry mode");ui_page_17_motor_test_destroy();
 gate=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");settings_detail_dialog_close(true);assert(label_find(lv_scr_act(),"Could not send"));gate=false;tick(220);click("Retry");ui_page_26_set_aging_destroy();
 gate=true;lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());image_select(3);snapshot("image-send-failed");assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Capture failed. Run one note again."));gate=false;ui_page_dynamic_capture_poll(lv_tick_get());click("Retry");gate=true;ui_page_dynamic_capture_poll(lv_tick_get());assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Capture failed. Run one note again."));ui_page_dynamic_capture_destroy();
 lv_tick_inc(3001);ui_page_dynamic_capture_create(lv_scr_act());capture_prepare_note();capture_click_source(6);snapshot("wave-send-failed");assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Capture failed. Run one note again."));gate=false;ui_page_dynamic_capture_poll(lv_tick_get());click("Retry");ui_page_dynamic_capture_destroy();
 ui_page_10_debug_create();debug_run_layout();snapshot("debug-mode-retry");click("Retry");ui_page_10_debug_destroy();assert(retries==6);
 gate=true;send_ok=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");settings_detail_dialog_close(true);ui_page_26_set_aging_on_timeout();unsigned before=sends;click("Start test");assert(sends==before&&(holds&16));snapshot("aging-timeout");ui_page_26_set_aging_destroy();ui_page_26_set_aging_on_reply(2);assert(!(holds&16));
 puts("PASS mode recovery, Sensor has no unrelated Retry, send failures and unknown aging result guard");
}
int main(void){
 setbuf(stdout,NULL);lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);lv_img_decoder_set_close_cb(dec,image_close);
 if(getenv("UN260_TEST_FRESH_ONLY")){capture_scroll_regression();capture_fresh_run_regression();return 0;}
 if(getenv("UN260_TEST_CAPTURE_ONLY")){capture_scroll_regression();capture_fresh_run_regression();image_transfer_regression_test();wave_transfer_regression_test();return 0;}
 if(getenv("UN260_TEST_IMAGE_ONLY")){image_transfer_regression_test();return 0;}
 calibration_test();sensor_test();sensor_stress_test();motor_test();aging_test();capture_scroll_regression();capture_fresh_run_regression();stream_test();image_transfer_regression_test();wave_transfer_regression_test();debug_test();failure_retry_test();
 for(unsigned i=0;i<4;i++){ui_page_12_sensor_create(lv_scr_act());ui_page_12_sensor_destroy();ui_page_10_debug_create();ui_page_10_debug_destroy();ui_page_dynamic_capture_create(lv_scr_act());ui_page_dynamic_capture_destroy();}
 puts("PASS actual LVGL diagnostic details suite");return 0;
}

const char *work_mode_service_status_msgid(void){return work_mode_service_status_text();}
