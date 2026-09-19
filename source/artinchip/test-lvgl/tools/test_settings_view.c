#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_core/page_06_settings.c"
#include "un260/lv_core/page_11_timeset.c"
#include "un260/lv_core/page_21_set_language.c"
#include "tools/test_page_background_asset.h"
static unsigned sends,pops;static uint8_t last_command,last_sub;static ui_page_t requested;
static bool white_selected,overlay;static settings_detail_dialog_cb_t discard;
static bool diagnostic_scope,diagnostic_ready=true,machine_running;
static boot_stage_t boot_stage=BOOT_STAGE_DONE;
void work_mode_service_set_diagnostic(bool active){diagnostic_scope=active;}
bool work_mode_service_diagnostic_ready(void){return diagnostic_ready;}
const char *work_mode_service_status_text(void){return diagnostic_ready?"Manual mode confirmed":"Waiting for controller mode";}
void work_mode_service_get_snapshot(work_mode_snapshot_t *s){memset(s,0,sizeof(*s));s->phase=diagnostic_ready?WORK_MODE_READY:WORK_MODE_WAITING_SYNC;}
void work_mode_service_retry(void){}
upgrade_session_owner_t upgrade_session_owner(void){return UPGRADE_SESSION_NONE;}
void diagnostic_calibration_get_snapshot(calibration_state_snapshot_t *s){memset(s,0,sizeof(*s));}
boot_stage_t boot_service_get_stage(void){return boot_stage;}
handshake_state_t boot_service_handshake_state(void){return HANDSHAKE_OK;}
bool protocol_send_is_ready(void){return true;}
bool app_command_runtime_count_start_busy(void){return machine_running;}
bool machine_state_aging_running(void){return false;}
bool fault_popup_get_pending_fault(fault_source_t*a,uint8_t*b,uint8_t*c){(void)a;(void)b;(void)c;return false;}
ui_page_t ui_manager_get_current_page(void){return UI_PAGE_SETTING;}
static standby_config_t standby={.minutes=1};
const standby_config_t *standby_config(void){return &standby;}
static int light_level=75;
int backlight_service_level(void){return light_level;}int backlight_service_max(void){return 100;}
uint8_t machine_state_reject_pocket_max(void){return 100;}uint8_t machine_state_double_note_level(void){return 2;}
const char *ui_text_get(ui_text_id_t id){(void)id;return "Not available";}
void ui_manager_push_page(ui_page_t p){requested=p;ui_page_06_settings_suspend();}
bool ui_manager_pop_page(void){pops++;return true;}
void ui_manager_switch(ui_page_t p){requested=p;}
void ui_manager_clear_stack(void){ui_page_06_settings_reset_navigation();}
void ui_manager_invalidate_all_page_caches(void){}
void ui_page_cis_calib_select(bool white){white_selected=white;}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
bool app_command_runtime_request_count_start(void){sends++;return true;}
bool settings_detail_send_command(uint8_t cmd,const uint8_t *sub,uint16_t len){assert(len==1);sends++;last_command=cmd;last_sub=sub[0];return true;}
bool settings_detail_overlay_is_open(void){return overlay;}
void settings_detail_dialog_hide(void){overlay=false;discard=NULL;}
void settings_detail_keyboard_hide(void){}
bool settings_detail_dialog_show(const char*a,const char*b,const char*c,const char*d,settings_detail_dialog_cb_t cb,settings_detail_dialog_cb_t cancel,void*data){(void)a;(void)b;(void)c;(void)d;(void)cancel;(void)data;overlay=true;discard=cb;return true;}
static bool (*policy)(gesture_action_t);
void gesture_service_set_page_policy(uint32_t owner,bool(*drag)(void),bool(*handler)(gesture_action_t)){(void)owner;(void)drag;policy=handler;}
void gesture_service_clear_page_policy(uint32_t owner){(void)owner;policy=NULL;}
static lv_color_t pixels[1280*400],buffer[1280*40];
static lv_point_t pointer_position;
static lv_indev_state_t pointer_state;
static unsigned card_clicks;
static bool evdev_press_cancelled;
static lv_obj_t *evdev_pressed_obj;
#include "actual_settings_pointer.h"
static void pointer_read(lv_indev_drv_t *d,lv_indev_data_t *data){
 (void)d;data->point=pointer_position;data->state=pointer_state;
 if(evdev_press_cancelled){data->state=LV_INDEV_STATE_RELEASED;if(pointer_state==LV_INDEV_STATE_RELEASED)evdev_press_cancelled=false;}
}
static void pointer_feed(int x,int y,lv_indev_state_t state){pointer_position=(lv_point_t){x,y};pointer_state=state;for(unsigned i=0;i<3;i++){lv_tick_inc(16);lv_timer_handler();}}
static void card_clicked(lv_event_t *e){(void)e;card_clicks++;}
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(pixels+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static lv_res_t info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;const un260_compiled_asset_t*a=test_page_asset_find(src);if(!a){fprintf(stderr,"Missing asset: %s\n",(const char*)src);abort();}memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=test_page_asset_find(s->src)->pixels;return LV_RES_OK;}
static void snapshot(const char*n){lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f);}
static lv_obj_t *label_find(lv_obj_t *o,const char *text){if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t*f=label_find(lv_obj_get_child(o,i),text);if(f)return f;}return NULL;}
static void click_label(lv_obj_t *o,const char *text){lv_obj_t*l=label_find(o,text);assert(l);while(l&&!lv_obj_has_flag(l,LV_OBJ_FLAG_CLICKABLE))l=lv_obj_get_parent(l);assert(l);lv_event_send(l,LV_EVENT_CLICKED,NULL);}
static void catalog_test(void){size_t n;const settings_node_t*c=settings_catalog(&n);assert(settings_catalog_validate(c,n));unsigned leaves=0;for(size_t i=0;i<n;i++)leaves+=c[i].kind==SETTINGS_DETAIL;assert(leaves==26);
 settings_node_t t[]={{"root",NULL,"Root",NULL,NULL,SETTINGS_CATEGORY,0},{"group","root","Group",NULL,NULL,SETTINGS_DIRECTORY,0},{"third","group","Third",NULL,NULL,SETTINGS_DIRECTORY,0},{"leaf","third","Leaf",NULL,NULL,SETTINGS_DETAIL,1}};
 assert(settings_catalog_validate(t,4));t[2].parent="third";assert(!settings_catalog_validate(t,4));t[2].parent="missing";assert(!settings_catalog_validate(t,4));t[2].parent="group";t[3].id="root";assert(!settings_catalog_validate(t,4));t[3].id="leaf";t[3].kind=SETTINGS_DIRECTORY;assert(!settings_catalog_validate(t,4));
 puts("PASS catalog: 26 details, unique IDs, parent validation, cycle/depth guards and third-level support");}
static void grid_test(void){
 lv_obj_t *test=lv_settings_box(lv_scr_act(),0,0,1280,400,0xF1F4F5),*g=lv_settings_grid(test,240,84,1016,274);lv_obj_t *cards[11];
 for(unsigned i=0;i<11;i++){lv_settings_item_t c={.title="Language",.value="English",.hint=i%2?"Optional helper text":NULL,.activate=card_clicked};cards[i]=lv_settings_item(g,&c);}
 lv_obj_update_layout(g);assert(lv_obj_get_scroll_bottom(g)>0);lv_coord_t x=lv_obj_get_x(cards[0]),y=lv_obj_get_y(cards[0]);lv_obj_del(cards[0]);lv_obj_update_layout(g);assert(lv_obj_get_x(cards[1])==x&&lv_obj_get_y(cards[1])==y);
 /* Final height, not min-height, must participate in flex cross alignment. */
 for(unsigned i=1;i<11;i++){
  lv_obj_t *arrow=lv_obj_get_child(cards[i],-1);lv_area_t card,icon;
  lv_obj_get_coords(cards[i],&card);lv_obj_get_coords(arrow,&icon);
  assert(abs((card.y1+card.y2)-(icon.y1+icon.y2))<=2);
 }
 lv_obj_add_flag(cards[1],LV_OBJ_FLAG_HIDDEN);lv_obj_update_layout(g);assert(lv_obj_get_x(cards[2])==x&&lv_obj_get_y(cards[2])==y);lv_obj_clear_flag(cards[1],LV_OBJ_FLAG_HIDDEN);
 lv_obj_update_layout(g);
 pointer_feed(420,315,LV_INDEV_STATE_PRESSED);
 for(int y=300;y>=160;y-=20)pointer_feed(420,y,LV_INDEV_STATE_PRESSED);
 pointer_feed(420,160,LV_INDEV_STATE_RELEASED);assert(lv_obj_get_scroll_y(g)>20&&card_clicks==0);
 const int starts[]={120,165,171,315};
 for(unsigned fast=0;fast<2;fast++)for(unsigned k=0;k<4;k++){
  lv_obj_scroll_to_y(g,0,LV_ANIM_OFF);lv_obj_update_layout(g);
  pointer_feed(420,starts[k],LV_INDEV_STATE_PRESSED);
  for(int dy=fast?50:8;dy<=120;dy+=fast?50:8)pointer_feed(420,starts[k]-dy,LV_INDEV_STATE_PRESSED);
  pointer_feed(420,starts[k]-120,LV_INDEV_STATE_RELEASED);
  assert(lv_obj_get_scroll_y(g)>40&&card_clicks==0);
 }
 lv_obj_scroll_to_y(g,0,LV_ANIM_OFF);pointer_feed(420,120,LV_INDEV_STATE_PRESSED);pointer_feed(420,120,LV_INDEV_STATE_RELEASED);assert(card_clicks==1);
 lv_obj_scroll_to_y(g,999,LV_ANIM_OFF);assert(lv_obj_get_scroll_y(g)>0);snapshot("dynamic-eleven-scroll");
 lv_obj_t *value=NULL;
 lv_settings_item_t long_card={.title="A longer setting name that wraps naturally",.hint="An optional longer explanation wraps without covering the current value or the next setting.",.value="Long current value",.value_label=&value};
 lv_obj_t *long_item=lv_settings_item(g,&long_card);lv_obj_update_layout(g);
 assert(value&&lv_obj_get_height(long_item)>80);lv_label_set_text(value,"Updated");assert(!strcmp(lv_label_get_text(value),"Updated"));
 lv_obj_del(test);
 lv_obj_t *plain=lv_settings_button(lv_scr_act(),20,20,100,50,"Action",false,card_clicked,NULL);
 pointer_feed(60,45,LV_INDEV_STATE_PRESSED);pointer_feed(180,45,LV_INDEV_STATE_PRESSED);pointer_feed(180,45,LV_INDEV_STATE_RELEASED);
 assert(card_clicks==1);lv_obj_del(plain);
 puts("PASS flex: delete/hide reflow, centred rows, actual driver feedback slow/fast drag matrix, ordinary slide-out cancellation");}
static void directory_test(void){
 ui_page_06_settings_create(lv_scr_act());assert(lv_obj_get_child_cnt(grid)==5);snapshot("device");assert(sends==0);
 assert(label_find(sidebar,"Back to count"));assert(!strcmp(lv_label_get_text(count_label),"5 settings"));
 machine_running=true;settings_poll(NULL);assert(!strcmp(lv_label_get_text(status_label),"Running"));machine_running=false;
 boot_stage=BOOT_STAGE_FAIL;settings_poll(NULL);assert(!strcmp(lv_label_get_text(status_label),"Attention"));boot_stage=BOOT_STAGE_DONE;settings_poll(NULL);
 lv_obj_t *brightness_value=value_labels[find("brightness")-catalog];assert(!strcmp(lv_label_get_text(brightness_value),"75%"));
 ui_page_06_settings_suspend();light_level=60;ui_page_06_settings_refresh_data(UI_DATA_TOPIC_DEVICE_VERSION);
 assert(!strcmp(lv_label_get_text(brightness_value),"75%"));ui_page_06_settings_resume();assert(!strcmp(lv_label_get_text(brightness_value),"60%"));light_level=75;
 click_label(sidebar,"Maintenance");assert(scope_is("maintenance"));assert(lv_obj_get_child_cnt(grid)==8);snapshot("maintenance");
 click_label(grid,"Calibration");assert(scope_is("calibration"));assert(lv_obj_get_child_cnt(grid)==2);snapshot("calibration");assert(sends==0);
 click_label(grid,"White balance");assert(white_selected&&requested==UI_PAGE_CIS_CALIB&&sends==0);assert(ui_page_06_settings_resume());assert(scope_is("calibration"));assert(page_06_settings_back_sub_page());assert(scope_is("maintenance"));
 click_label(sidebar,"Counting");snapshot("counting");click_label(sidebar,"Data");snapshot("data");click_label(grid,"Upgrade");snapshot("upgrade");assert(page_06_settings_back_sub_page());
 click_label(grid,"Data collection");assert(!sidebar&&!grid&&dc_btn_all&&diagnostic_scope);
 assert(lv_obj_has_state(dc_btn_start,LV_STATE_DISABLED)&&lv_obj_has_state(dc_btn_disable,LV_STATE_DISABLED));
 snapshot("collection");assert(sends==0);
 diagnostic_ready=false;click_label(view,"All notes");assert(sends==0);diagnostic_ready=true;
 click_label(view,"All notes");assert(last_command==0xC0&&last_sub==1&&sends==1);data_collection_request_cancel();
 data_collection_state_select_mode(DATA_COLLECT_MODE_ALL,"All-note collection confirmed");page_06_data_collection_refresh();
 assert(!lv_obj_has_state(dc_btn_start,LV_STATE_DISABLED)&&!lv_obj_has_flag(dc_check_all,LV_OBJ_FLAG_HIDDEN));snapshot("collection-confirmed");
 machine_running=true;page_06_data_collection_refresh();assert(lv_obj_has_state(dc_btn_start,LV_STATE_DISABLED));
 click_label(view,"Start");assert(sends==1);machine_running=false;
 data_collection_state_exit("Select a collection mode");assert(page_06_settings_back_sub_page()&&!diagnostic_scope);
 click_label(sidebar,"About & security");snapshot("about");
 for(unsigned i=0;i<lv_obj_get_child_cnt(grid);i++){
  lv_obj_t *card=lv_obj_get_child(grid,i),*arrow=lv_obj_get_child(card,-1);lv_area_t ca,ar;
  lv_obj_get_coords(card,&ca);lv_obj_get_coords(arrow,&ar);
  assert(ar.x2<ca.x2&&ar.x1>ca.x1&&lv_obj_get_width(arrow)>0);
 }
 click_label(grid,"Versions");assert(!sidebar&&version_value_labels[0]);snapshot("versions");
 ui_page_06_settings_suspend();ui_page_06_settings_reset_navigation();assert(scope_is("device"));assert(ui_page_06_settings_resume());ui_page_06_settings_destroy();
 for(unsigned i=0;i<8;i++){ui_page_06_settings_create(lv_scr_act());ui_page_06_settings_destroy();}
 assert(!settings_page&&!view&&!grid);puts("PASS directory navigation, exact parent return, full-screen internal details, command isolation and lifecycle");}
static void details_test(void){
 machine_time_value_t before={2024,1,31,23,59,58},now;machine_time_confirm(&before);
 ui_page_11_timeset_create(lv_scr_act());snapshot("date-time");lv_obj_update_layout(time_frame.root);assert(lv_obj_get_x(lv_obj_get_parent(time_frame.back))+lv_obj_get_x(time_frame.back)>1100);assert(lv_obj_get_height(time_frame.back)>=44);
 machine_time_tick();machine_time_get(&now);assert(now.second==59);lv_event_send(time_steps[1][0],LV_EVENT_CLICKED,NULL);assert(time_draft.month==2&&time_draft.day==29);machine_time_get(&now);assert(now.month==1);snapshot("date-time-draft");
 assert(policy(GESTURE_ACTION_HOME)&&discard);settings_detail_dialog_hide();unsigned old=pops;time_back(NULL);assert(discard&&pops==old);settings_detail_dialog_hide();time_cancel(NULL);assert(pops==old+1);ui_page_11_timeset_destroy();machine_time_get(&now);assert(now.month==1&&!policy);
 ui_page_11_timeset_create(lv_scr_act());lv_event_send(time_steps[1][0],LV_EVENT_CLICKED,NULL);lv_event_send(time_steps[0][0],LV_EVENT_CLICKED,NULL);assert(time_draft.year==2025&&time_draft.day==28);time_apply(NULL);machine_time_get(&now);assert(now.year==2025&&now.month==2&&now.day==28);ui_page_11_timeset_destroy();
 ui_page_21_set_language_create(lv_scr_act());snapshot("language");assert(lv_obj_has_state(language_save,LV_STATE_DISABLED));assert(!language_dirty());language_cancel(NULL);ui_page_21_set_language_destroy();
 for(unsigned i=0;i<8;i++){ui_page_11_timeset_create(lv_scr_act());ui_page_11_timeset_destroy();ui_page_21_set_language_create(lv_scr_act());ui_page_21_set_language_destroy();}
 puts("PASS full-screen details, right Back, live clock while editing, draft/cancel/Save, leap clamp, dirty Home and lifecycle");}
static void third_level_test(void){
 const settings_node_t *saved_catalog=catalog;size_t saved_count=catalog_count;
 settings_node_t custom[]={
  {"device",NULL,"Device",NULL,NULL,SETTINGS_CATEGORY,0},
  {"two",NULL,"Two",NULL,NULL,SETTINGS_CATEGORY,0},
  {"three",NULL,"Three",NULL,NULL,SETTINGS_CATEGORY,0},
  {"four",NULL,"Four",NULL,NULL,SETTINGS_CATEGORY,0},
  {"five",NULL,"Five",NULL,NULL,SETTINGS_CATEGORY,0},
  {"six",NULL,"Six",NULL,NULL,SETTINGS_CATEGORY,0},
  {"seven",NULL,"Seven",NULL,NULL,SETTINGS_CATEGORY,0},
  {"group","device","Group",NULL,NULL,SETTINGS_DIRECTORY,0},
  {"third","group","Third",NULL,NULL,SETTINGS_DIRECTORY,0},
  {"leaf","third","Leaf",NULL,NULL,SETTINGS_DETAIL,UI_PAGE_LANGUAGE_SETTING}};
 catalog=custom;catalog_count=sizeof(custom)/sizeof(custom[0]);scope=NULL;
 assert(settings_catalog_validate(catalog,catalog_count));ui_page_06_settings_create(lv_scr_act());
 lv_obj_update_layout(sidebar);assert(lv_obj_get_scroll_bottom(lv_obj_get_child(sidebar,2))>0);
 click_label(sidebar,"Seven");assert(scope_is("seven"));click_label(sidebar,"Device");
 click_label(grid,"Group");click_label(grid,"Third");assert(scope_is("third"));
 click_label(grid,"Leaf");assert(requested==UI_PAGE_LANGUAGE_SETTING);ui_page_06_settings_resume();assert(scope_is("third"));
 assert(page_06_settings_back_sub_page()&&scope_is("group"));assert(page_06_settings_back_sub_page()&&scope_is("device"));
 ui_page_06_settings_destroy();catalog=saved_catalog;catalog_count=saved_count;scope=NULL;
 memset(scroll_positions,0,sizeof(scroll_positions));
 puts("PASS actual third-level navigation, exact parent return, seven categories overflow, optional category icons");
}
int main(void){lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;input.feedback_cb=evdev_feedback;lv_indev_drv_register(&input);device_info_init("V1.0.0");catalog_test();grid_test();directory_test();third_level_test();details_test();puts("PASS actual LVGL settings suite");return 0;}
