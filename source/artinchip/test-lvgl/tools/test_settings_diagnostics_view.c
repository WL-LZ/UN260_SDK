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
#include "un260/lv_core/page_28_get_image.h"
#include "un260/lv_core/page_31_get_wave.h"
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
static settings_detail_dialog_cb_t confirm;
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
void ui_manager_switch(ui_page_t p){(void)p;}
void ui_manager_clear_stack(void){}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
uint64_t app_clock_monotonic_ms(void){return lv_tick_get();}
uint32_t app_command_runtime_count_end_generation(void){return count_end_generation;}
const char *ui_text_get(ui_text_id_t id){(void)id;return "Waiting for response";}
const char *ui_text_msgid(ui_text_id_t id){return ui_text_get(id);}

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
 bool keep_run=!strncmp(n,"wave-",5)||!strncmp(n,"debug-",6);
 assert((label_find(lv_scr_act(),"RUN")!=NULL)==keep_run);
 if(keep_run){unsigned before=run_clicks;click("RUN");assert(run_clicks==before+1);}
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
 assert(policy(GESTURE_ACTION_HOME)&&confirm);settings_detail_dialog_hide();click("Back");assert(confirm);confirm(NULL);
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
 ui_page_26_set_aging_create(lv_scr_act());snapshot("aging-ready");click("Start test");assert(confirm);confirm(NULL);assert(sent_cmd==0x46&&holds&16);ui_page_26_set_aging_on_reply(0);assert(aging&&policy(GESTURE_ACTION_HOME));snapshot("aging-running");ui_page_26_set_aging_on_reply(2);assert(!aging&&!(holds&16));snapshot("aging-complete");ui_page_26_set_aging_destroy();assert(!policy);puts("PASS aging confirmation, running guard, legacy completion and timer cleanup");
}
static void image_select(uint8_t source);
static void stream_test(void){
 ui_page_28_get_image_create(lv_scr_act());snapshot("image-ready");image_select(3);assert(sent_cmd==0x47);uint8_t header[]={0,3,0,3,0,2},row[]={3,0,1,24,48,72};ui_page_28_get_image_on_frame(header,sizeof(header));ui_page_28_get_image_on_frame(row,sizeof(row));uint8_t end[]={0xff};ui_page_28_get_image_on_frame(end,1);assert(test_notice_kind==UI_NOTICE_WARNING&&!strcmp(test_notice_key,"capture.image")&&!strcmp(test_notice_detail,"Image data incomplete. Tap the source to retry."));snapshot("image-missing");ui_page_28_get_image_destroy();
 ui_page_28_get_image_create(lv_scr_act());image_select(3);ui_page_28_get_image_on_frame(header,sizeof(header));ui_page_28_get_image_on_frame(row,sizeof(row));row[2]=2;ui_page_28_get_image_on_frame(row,sizeof(row));ui_page_28_get_image_on_frame(end,1);snapshot("image-complete");assert(test_notice_kind==UI_NOTICE_SUCCESS&&!strcmp(test_notice_key,"capture.image")&&!strcmp(test_notice_detail,"Capture complete"));ui_page_28_get_image_destroy();
 ui_page_31_get_wave_create(lv_scr_act());snapshot("wave-ready");click("Capture");assert(sent_cmd==0x48);uint8_t values[257];for(unsigned channel=1;channel<=7;channel++){values[0]=channel;for(unsigned i=1;i<=256;i++)values[i]=(i*13)%256;ui_page_31_get_wave_on_frame(values,sizeof(values));}snapshot("wave-captured");assert(test_notice_kind==UI_NOTICE_SUCCESS&&!strcmp(test_notice_key,"capture.wave")&&!strcmp(test_notice_detail,"Capture complete. 7 channels received."));ui_page_31_get_wave_destroy();puts("PASS image completeness, waveform transfer and late-frame lifecycle");
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
   ui_page_28_get_image_on_frame(reply.payload,reply.payload_len);
  }
 }
 assert(ready==1);
}
static void image_header(uint8_t source,uint16_t width,uint16_t height){
 uint8_t payload[]={0,source,width>>8,width&255,height>>8,height&255};
 image_wire(payload,sizeof(payload));
}
static void image_end(void){uint8_t end[]={0xff};image_wire(end,sizeof(end));}
static lv_obj_t *image_button(uint8_t source){
 lv_obj_t *canvas=debug_find_type(lv_scr_act(),&lv_canvas_class);assert(canvas&&source>=1&&source<=6);
 lv_obj_t *body=lv_obj_get_parent(lv_obj_get_parent(canvas));
 lv_obj_t *sources=lv_obj_get_child(body,0);assert(lv_obj_get_child_cnt(sources)==6);
 return lv_obj_get_child(sources,source-1);
}
static void image_select(uint8_t source){
 lv_obj_t *button=image_button(source);
 assert(lv_obj_has_flag(button,LV_OBJ_FLAG_CLICKABLE)&&!lv_obj_has_flag(button,LV_OBJ_FLAG_HIDDEN));
 lv_event_send(button,LV_EVENT_CLICKED,NULL);
}
static void image_finish_feedback(void){tick(200);ui_page_28_get_image_poll(lv_tick_get());}
static void image_capture(uint8_t source){
 unsigned before=sends;image_select(source);
 assert(sends==before+1&&sent_cmd==0x47&&sent_sub==source);
 for(unsigned id=1;id<=6;id++)assert(!lv_obj_has_flag(image_button(id),LV_OBJ_FLAG_CLICKABLE));
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
 ui_page_28_get_image_create(lv_scr_act());assert(sends==before);
 assert(!label_find(lv_scr_act(),"RUN")&&!label_find(lv_scr_act(),"Capture"));
 for(uint8_t id=1;id<=6;id++)assert(lv_obj_get_style_bg_color(image_button(id),0).full==lv_color_hex(0xF4F6F7).full);
 lv_obj_update_layout(lv_scr_act());if(!label_find(lv_scr_act(),"Place one note in the hopper."))labels_dump(lv_scr_act());assert(label_find(lv_scr_act(),"Place one note in the hopper."));snapshot("image-ready");
 ++count_end_generation;ui_page_28_get_image_poll(lv_tick_get());
 assert(!label_find(lv_scr_act(),"Place one note in the hopper."));
 for(uint8_t source=1;source<=6;source++){
  image_capture(source);assert(lv_obj_get_style_bg_color(image_button(source),0).full==lv_color_hex(0xFFFFFF).full);
  unsigned locked_sends=sends;
  for(uint8_t id=1;id<=6;id++)lv_event_send(image_button(id),LV_EVENT_CLICKED,NULL);
  assert(sends==locked_sends);image_header(source,400,200);
  unsigned lines=image_data_reply_scanline_count(source,400,200);
  image_scanline(source,lines,400,200);image_scanline(source,lines,400,200);
  for(unsigned line=lines-1;line;line--){
   image_scanline(source,line,400,200);
   if(source==1&&line==lines/2+1){
    ui_page_28_get_image_poll(lv_tick_get());tick(240);lv_obj_update_layout(lv_scr_act());
    lv_obj_t *fill=lv_obj_get_child(image_button(source),0);
    if(lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN)||lv_obj_get_width(fill)<120||lv_obj_get_width(fill)>124)
     fprintf(stderr,"Progress hidden=%u width=%d style=%d\n",lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN),lv_obj_get_width(fill),lv_obj_get_style_width(fill,0));
    assert(!lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN)&&lv_obj_get_width(fill)>=120&&lv_obj_get_width(fill)<=124);
    assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
   const lv_img_dsc_t *partial=lv_canvas_get_img(image_canvas_find());assert(partial&&partial->data);
   assert(lv_canvas_get_px(image_canvas_find(),0,199).full!=lv_color_hex(0xEFF4F8).full);
   assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_hex(0xEFF4F8).full);
   snapshot("image-progress");
   }
  }
  ui_page_28_get_image_poll(lv_tick_get());
  assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
  assert(ui_page_28_get_image_is_capturing() && capture_nav_locks && capture_gesture_locks);
  unsigned back_before=pops;click("Back");assert(pops==back_before);
  assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED&&pops==back_before);
  image_end();assert(!ui_page_28_get_image_is_capturing() && !capture_nav_locks && !capture_gesture_locks);assert(test_notice_kind==UI_NOTICE_SUCCESS);image_render_corners(source);
  image_finish_feedback();
  for(uint8_t id=1;id<=6;id++)assert(lv_obj_has_flag(image_button(id),LV_OBJ_FLAG_CLICKABLE));
  assert(lv_obj_has_flag(lv_obj_get_child(image_button(source),0),LV_OBJ_FLAG_HIDDEN));
 }
 before=sends;
 for(uint8_t source=1;source<=6;source++){image_select(source);image_native_size(image_canvas_find(),400,200);}
 assert(sends==before);snapshot("image-cached");ui_page_28_get_image_destroy();

 /* Odd byte block boundaries and reverse-order blocks must preserve RGB pairs. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
 lv_obj_update_layout(lv_scr_act());if(!label_find(lv_scr_act(),"Place one note in the hopper."))labels_dump(lv_scr_act());assert(label_find(lv_scr_act(),"Place one note in the hopper."));
 for(uint8_t source=1;source<=6;source++)image_complete_small(source);
 before=sends;for(uint8_t source=1;source<=6;source++){image_select(source);image_native_base(source);}assert(sends==before);
 ui_page_28_get_image_destroy();

 /* Partial data is never a successful cache. Retry by tapping that same source. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
 image_capture(3);image_header(3,5,3);image_scanline(3,1,5,3);image_scanline(3,1,5,3);
 uint8_t bad_zero[]={3,0,0,1,2,3,4,5},bad_boundary[]={3,0,4,1,2,3,4,5};
 uint8_t bad_short[]={3,0,2,1,2,3,4},bad_source[]={7,0,2,1,2,3,4,5};
 ui_page_28_get_image_on_frame(bad_zero,sizeof(bad_zero));ui_page_28_get_image_on_frame(bad_boundary,sizeof(bad_boundary));
 ui_page_28_get_image_on_frame(bad_short,sizeof(bad_short));ui_page_28_get_image_on_frame(bad_source,sizeof(bad_source));
 ui_page_28_get_image_poll(lv_tick_get());assert(!lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 image_end();assert(test_notice_kind==UI_NOTICE_WARNING&&lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 assert(!capture_nav_locks&&!capture_gesture_locks);
 before=sends;image_complete_small(3);assert(sends==before+1);
 image_complete_small(1);image_select(3);image_native_base(3);ui_page_28_get_image_destroy();

 const uint16_t unsupported[][2]={{0,3},{5,0},{513,3},{5,385}};
 for(unsigned i=0;i<sizeof(unsupported)/sizeof(unsupported[0]);i++){
  protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
  image_capture(3);image_header(3,unsupported[i][0],unsupported[i][1]);assert(test_notice_kind==UI_NOTICE_ERROR);
  before=sends;lv_event_send(image_button(3),LV_EVENT_CLICKED,NULL);assert(sends==before);
  lv_tick_inc(3001);ui_page_28_get_image_poll(lv_tick_get());image_complete_small(3);ui_page_28_get_image_destroy();
 }

 /* A frame waiting in RX protects an expired UI deadline; absence is a real timeout. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());image_capture(3);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(5001);assert(!ui_page_28_get_image_poll(lv_tick_get()));
 image_probe_pending=false;assert(ui_page_28_get_image_poll(lv_tick_get()));
 assert(!ui_page_28_get_image_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 protocol_request_set_pending_reply_probe(NULL);before=sends;
 lv_event_send(image_button(1),LV_EVENT_CLICKED,NULL);assert(sends==before);
 lv_tick_inc(3001);ui_page_28_get_image_poll(lv_tick_get());image_complete_small(3);ui_page_28_get_image_destroy();

 /* Leave cancels UI ownership, drops late data and frees all six caches. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
 image_capture(3);image_header(3,5,3);image_scanline(3,1,5,3);ui_page_28_get_image_destroy();
 ui_page_28_get_image_create(lv_scr_act());before=sends;
 lv_event_send(image_button(3),LV_EVENT_CLICKED,NULL);assert(sends==before);
 image_scanline(3,2,5,3);image_end();assert(lv_obj_has_flag(image_canvas_find(),LV_OBJ_FLAG_HIDDEN));
 lv_tick_inc(3001);ui_page_28_get_image_poll(lv_tick_get());image_complete_small(3);ui_page_28_get_image_destroy();
 /* Allocation failure is a terminal capture failure, with the same navigation release. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());image_capture(1);
 lv_mem_monitor_t memory;void *pressure[512];unsigned pressure_count=0;
 while(pressure_count<512){
  lv_mem_monitor(&memory);if(memory.free_biggest_size<131072)break;
  void *block=lv_mem_alloc(65536);if(!block)break;pressure[pressure_count++]=block;
 }
 assert(pressure_count);image_header(1,512,384);assert(test_notice_kind==UI_NOTICE_ERROR);
 assert(!ui_page_28_get_image_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 while(pressure_count)lv_mem_free(pressure[--pressure_count]);
 ui_page_28_get_image_destroy();lv_tick_inc(3001);
 /* Duplicate packets and duplicate headers cannot keep an incomplete request alive. */
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
 image_capture(1);image_header(1,400,200);image_scanline(1,1,400,200);
 ui_page_28_get_image_poll(lv_tick_get());image_native_size(image_canvas_find(),400,200);
 assert(lv_canvas_get_px(image_canvas_find(),399,0).full==lv_color_make(0,255,0).full);
 assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_hex(0xEFF4F8).full);
 snapshot("image-first-real-block");image_scanline(1,2,400,200);ui_page_28_get_image_poll(lv_tick_get());
 assert(lv_canvas_get_px(image_canvas_find(),0,0).full==lv_color_make(255,0,0).full);
 assert(lv_canvas_get_px(image_canvas_find(),0,1).full==lv_color_hex(0xEFF4F8).full);
 snapshot("image-first-real-row");ui_page_28_get_image_destroy();lv_tick_inc(3001);
 protocol_frame_parser_init(&image_parser);ui_page_28_get_image_create(lv_scr_act());
 image_capture(1);image_header(1,5,3);image_scanline(1,2,5,3);
 ui_page_28_get_image_poll(lv_tick_get());image_native_size(image_canvas_find(),5,3);
 assert(lv_canvas_get_px(image_canvas_find(),4,0).full==lv_color_hex(0xEFF4F8).full);
 image_scanline(1,1,5,3);ui_page_28_get_image_poll(lv_tick_get());
 assert(lv_canvas_get_px(image_canvas_find(),4,0).full!=lv_color_hex(0xEFF4F8).full);
 for(unsigned i=0;i<5;i++){lv_tick_inc(1000);image_header(1,5,3);image_scanline(1,1,5,3);}
 assert(ui_page_28_get_image_poll(lv_tick_get())&&!capture_nav_locks&&!capture_gesture_locks);
 ui_page_28_get_image_destroy();
 lv_tick_inc(3001);ui_page_28_get_image_create(lv_scr_act());image_capture(3);
 protocol_request_set_pending_reply_probe(image_pending_probe);image_probe_pending=true;
 lv_tick_inc(120001);assert(ui_page_28_get_image_poll(lv_tick_get()));
 assert(!ui_page_28_get_image_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 image_probe_pending=false;protocol_request_set_pending_reply_probe(NULL);ui_page_28_get_image_destroy();
 /* Bursts may update more often than the animation timer. Progress must track
  * received packets immediately, without repeatedly restarting a width tween. */
 for(uint8_t source=1;source<=6;source++){
  lv_tick_inc(3001);ui_page_28_get_image_create(lv_scr_act());image_capture(source);
  image_header(source,400,200);unsigned total=image_data_reply_scanline_count(source,400,200);
  lv_obj_t *fill=lv_obj_get_child(image_button(source),0);int previous=4;
  for(unsigned packet=1;packet<=total;packet++){
   image_scanline(source,packet,400,200);
   if(packet%20!=0&&packet!=total)continue;
   ui_page_28_get_image_poll(lv_tick_get());lv_obj_update_layout(lv_scr_act());
   unsigned percent=packet*100U/total;if(percent>99)percent=99;
   int expected=244U*percent/100U;if(expected<4)expected=4;
   assert(!lv_obj_has_flag(fill,LV_OBJ_FLAG_HIDDEN));
   assert(lv_obj_get_width(fill)==expected&&lv_obj_get_width(fill)>=previous);
   previous=lv_obj_get_width(fill);
   image_scanline(source,packet,400,200);ui_page_28_get_image_poll(lv_tick_get());
   lv_obj_update_layout(lv_scr_act());assert(lv_obj_get_width(fill)==previous);
  }
  assert(previous<244&&ui_page_28_get_image_is_capturing());image_end();
  tick(110);lv_obj_update_layout(lv_scr_act());assert(lv_obj_get_width(fill)==244);
  ui_page_28_get_image_destroy();
 }
 puts("PASS click-to-capture, empty selection, no RUN/Capture, locked input, real packet progress, progressive preview with complete-only six-source cache, 400x200 right-bank-first RGB565/linear gray corners, odd byte boundaries, failure/retry, queued-RX timeout protection and destroy/recreate late guard");
}
static void wave_wire_channel(uint8_t channel,uint8_t value){
 uint8_t frame[250]={0xfd,0xdf,250,0x48,channel};
 memset(&frame[5],value,sizeof(frame)-6);frame[249]=0x83;
 stream_data_reply_view_t reply=stream_data_reply_parse(frame,sizeof(frame));
 assert(reply.kind==STREAM_DATA_REPLY_WAVE&&reply.payload_len==245);
 wave_data_reply_view_t wave=wave_data_reply_parse(reply.payload,reply.payload_len);
 assert(wave.kind==WAVE_DATA_REPLY_CHANNEL&&wave.channel==channel&&wave.value_count==244);
 ui_page_31_get_wave_on_frame(reply.payload,reply.payload_len);
}
static void wave_transfer_regression_test(void){
 const char *channels[]={"MT","MG 1 / 2","MG 3","MG 4","MG 5 / 6","Upper UV","Lower UV"};
 uint8_t empty[]={4},unknown[]={8,0xdb,0xda},invalid_zero[]={0,1},no_data[]={0};
 assert(wave_data_reply_parse(NULL,0).kind==WAVE_DATA_REPLY_INVALID);
 assert(wave_data_reply_parse(empty,sizeof(empty)).kind==WAVE_DATA_REPLY_INVALID);
 assert(wave_data_reply_parse(unknown,sizeof(unknown)).kind==WAVE_DATA_REPLY_INVALID);
 assert(wave_data_reply_parse(invalid_zero,sizeof(invalid_zero)).kind==WAVE_DATA_REPLY_INVALID);
 assert(wave_data_reply_parse(no_data,sizeof(no_data)).kind==WAVE_DATA_REPLY_NO_DATA);
 ui_page_31_get_wave_create(lv_scr_act());assert(ui_page_31_get_wave_request());
 assert(ui_page_31_get_wave_is_capturing()&&capture_nav_locks&&capture_gesture_locks);
 lv_obj_update_layout(lv_scr_act());unsigned blocked_pops=pops;click("Back");assert(pops==blocked_pops);
 assert(lv_nav_button_request_back()==LV_NAV_BACK_BLOCKED&&pops==blocked_pops);
 wave_wire_channel(1,24);wave_wire_channel(2,48);wave_wire_channel(3,72);
 wave_wire_channel(7,168);
 /* A channel selector is not an end marker. Later channels must still arrive. */
 assert(test_notice_kind==UI_NOTICE_PROGRESS);
 unsigned notices=test_notice_count;
 ui_page_31_get_wave_on_frame(empty,sizeof(empty));
 ui_page_31_get_wave_on_frame(unknown,sizeof(unknown));
 ui_page_31_get_wave_on_frame(invalid_zero,sizeof(invalid_zero));
 assert(test_notice_count==notices&&!ui_page_31_get_wave_request());
 wave_wire_channel(3,73); /* A duplicate replaces its channel without completing others. */
 wave_wire_channel(4,96);wave_wire_channel(5,120);wave_wire_channel(6,144);
 assert(test_notice_kind==UI_NOTICE_SUCCESS&&!strcmp(test_notice_detail,"Capture complete. 7 channels received."));
 assert(!ui_page_31_get_wave_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 lv_obj_t *line=debug_find_type(lv_scr_act(),&lv_line_class);assert(line);
 for(unsigned channel=1;channel<=7;channel++){
  click(channels[channel-1]);assert(!lv_obj_has_flag(line,LV_OBJ_FLAG_HIDDEN));
  lv_line_t *signal=(lv_line_t*)line;uint8_t value=channel==3?73:channel*24;
  assert(signal->point_num==244&&signal->point_array[0].y==12+(255-value)*146/255);
 }
 notices=test_notice_count;ui_page_31_get_wave_on_frame(no_data,sizeof(no_data));
 assert(test_notice_count==notices&&!ui_page_31_get_wave_poll(lv_tick_get()+6000));
 snapshot("wave-out-of-order");ui_page_31_get_wave_destroy();

 ui_page_31_get_wave_create(lv_scr_act());assert(ui_page_31_get_wave_request());
 lv_obj_update_layout(lv_scr_act());
 wave_wire_channel(1,24);wave_wire_channel(7,168);
 assert(label_find(lv_scr_act(),"Received 2 / 7 channels"));lv_tick_inc(5001);
 assert(ui_page_31_get_wave_poll(lv_tick_get())&&test_notice_kind==UI_NOTICE_WARNING);
 assert(label_find(lv_scr_act(),"Missing channels: MG 1 / 2, MG 3, MG 4, MG 5 / 6, Upper UV"));
 click("MG 4");assert(label_find(lv_scr_act(),"Not received"));
 unsigned terminal_pops=pops;click("Back");assert(pops==terminal_pops+1);
 assert(!ui_page_31_get_wave_request());ui_page_31_get_wave_destroy();
 ui_page_31_get_wave_create(lv_scr_act());assert(!ui_page_31_get_wave_request());
 notices=test_notice_count;ui_page_31_get_wave_on_frame(unknown,sizeof(unknown));
 assert(test_notice_count==notices);
 lv_tick_inc(3001);ui_page_31_get_wave_poll(lv_tick_get());assert(ui_page_31_get_wave_request());
 wave_wire_channel(1,24);ui_page_31_get_wave_on_frame(no_data,sizeof(no_data));
 assert(test_notice_kind==UI_NOTICE_WARNING);
 line=debug_find_type(lv_scr_act(),&lv_line_class);assert(line);
 for(unsigned channel=1;channel<=7;channel++){
  click(channels[channel-1]);assert(lv_obj_has_flag(line,LV_OBJ_FLAG_HIDDEN));
 }
 ui_page_31_get_wave_destroy();ui_page_31_get_wave_on_frame(no_data,sizeof(no_data));
 ui_page_31_get_wave_create(lv_scr_act());assert(ui_page_31_get_wave_request());
 ui_page_31_get_wave_destroy();ui_page_31_get_wave_create(lv_scr_act());
 assert(!ui_page_31_get_wave_request());lv_tick_inc(3001);
 ui_page_31_get_wave_poll(lv_tick_get());ui_page_31_get_wave_destroy();
 lv_tick_inc(3001);ui_page_31_get_wave_create(lv_scr_act());assert(ui_page_31_get_wave_request());
 uint64_t arrival=lv_tick_get()+4999;lv_tick_inc(5001);
 protocol_request_result_context_begin(arrival);wave_wire_channel(1,24);protocol_request_result_context_end();
 assert(ui_page_31_get_wave_is_capturing()&&!ui_page_31_get_wave_poll(lv_tick_get()));
 for(unsigned i=0;i<5;i++){lv_tick_inc(1000);wave_wire_channel(1,24);}
 assert(ui_page_31_get_wave_poll(lv_tick_get())&&!capture_nav_locks&&!capture_gesture_locks);
 wave_wire_channel(4,96);wave_wire_channel(5,120);wave_wire_channel(6,144);wave_wire_channel(7,168);
 assert(!ui_page_31_get_wave_is_capturing()&&!capture_nav_locks&&!capture_gesture_locks);
 ui_page_31_get_wave_destroy();
 puts("PASS 250-byte waveform frames, out-of-order completion, duplicate/empty/unknown channels, partial timeout, no-data reset and destroy/recreate late guard");
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
 assert(!lv_obj_has_state(run,LV_STATE_DISABLED)&&!lv_obj_has_flag(run,LV_OBJ_FLAG_HIDDEN));
 lv_area_t tab_area,actions_area,back_area;
 lv_obj_get_coords(tabs,&tab_area);lv_obj_get_coords(lv_obj_get_parent(run),&actions_area);lv_obj_get_coords(back,&back_area);
 assert(tab_area.x2<actions_area.x1&&actions_area.x2<back_area.x1);
 assert(tab_area.y1==actions_area.y1&&tab_area.y2==actions_area.y2);
 unsigned before=run_clicks;click("RUN");assert(run_clicks==before+1);
}
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
 gate=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");confirm(NULL);assert(label_find(lv_scr_act(),"Could not send"));gate=false;tick(220);click("Retry");ui_page_26_set_aging_destroy();
 gate=true;ui_page_28_get_image_create(lv_scr_act());image_select(3);snapshot("image-send-failed");assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Could not send capture request. Try again."));gate=false;ui_page_28_get_image_poll(lv_tick_get());click("Retry");gate=true;ui_page_28_get_image_poll(lv_tick_get());assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Could not send capture request. Try again."));ui_page_28_get_image_destroy();
 ui_page_31_get_wave_create(lv_scr_act());click("Capture");snapshot("wave-send-failed");assert(test_notice_kind==UI_NOTICE_ERROR&&!strcmp(test_notice_detail,"Could not send capture request. Try again."));gate=false;ui_page_31_get_wave_poll(lv_tick_get());click("Retry");ui_page_31_get_wave_destroy();
 ui_page_10_debug_create();debug_run_layout();snapshot("debug-mode-retry");click("Retry");ui_page_10_debug_destroy();assert(retries==6);
 gate=true;send_ok=true;ui_page_26_set_aging_create(lv_scr_act());click("Start test");confirm(NULL);ui_page_26_set_aging_on_timeout();unsigned before=sends;click("Start test");assert(sends==before&&(holds&16));snapshot("aging-timeout");ui_page_26_set_aging_destroy();ui_page_26_set_aging_on_reply(2);assert(!(holds&16));
 puts("PASS mode recovery, Sensor has no unrelated Retry, send failures and unknown aging result guard");
}
int main(void){
 setbuf(stdout,NULL);lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);lv_img_decoder_set_close_cb(dec,image_close);
 if(getenv("UN260_TEST_IMAGE_ONLY")){image_transfer_regression_test();return 0;}
 calibration_test();sensor_test();sensor_stress_test();motor_test();aging_test();stream_test();image_transfer_regression_test();wave_transfer_regression_test();debug_test();failure_retry_test();
 for(unsigned i=0;i<4;i++){ui_page_12_sensor_create(lv_scr_act());ui_page_12_sensor_destroy();ui_page_10_debug_create();ui_page_10_debug_destroy();ui_page_28_get_image_create(lv_scr_act());ui_page_28_get_image_destroy();ui_page_31_get_wave_create(lv_scr_act());ui_page_31_get_wave_destroy();}
 puts("PASS actual LVGL diagnostic details suite");return 0;
}

const char *work_mode_service_status_msgid(void){return work_mode_service_status_text();}
