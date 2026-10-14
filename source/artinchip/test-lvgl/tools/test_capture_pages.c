#include "test_notice_sink.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/font/ui_message_font.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/lv_page_manager.h"
#include "test_settings_actions.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/page_20_set_print.h"
#include "un260/lv_core/page_05_set_password.h"
#include "un260/lv_core/page_22_set_double_note.h"
#include "un260/lv_core/page_23_set_flap.h"
#include "un260/lv_core/page_24_set_reject_pocket.h"
#include "un260/lv_core/page_25_set_serial_number.h"
#include "un260/lv_core/page_27_set_cfd_level.h"
#include "un260/lv_core/page_29_set_password.h"
#include "un260/lv_core/page_30_set_factory.h"
#include "un260/app_service/ui_factory_reset_service.h"
#include "un260/lv_core/page_33_set_brightness.h"
#include "un260/lv_core/page_36_display_test.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_pin_keypad.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/cfd/cfd.h"
#include "un260/serial_number/serial_number.h"
#include "tools/test_page_background_asset.h"

static unsigned sends, pops, reboots;
static ui_page_t pushed_page;
static bool send_ok = true, persist_ok = true, backlight_ok = true, overlay;
static uint8_t last_command, last_payload[32], double_level = 2, flap = 1, reject_capacity = 50;
static int brightness = 50;
static char password[5] = "1111";
static settings_detail_dialog_cb_t confirm_dialog;
static void *dialog_data;
static settings_detail_keyboard_cb_t keyboard_confirm;
static void *keyboard_data;
static bool (*gesture_policy)(gesture_action_t);
uint64_t app_clock_monotonic_ms(void) { return lv_tick_get(); }
uint32_t app_clock_uptime_ms(void) { return lv_tick_get(); }
int protocol_send(uint8_t cmd, const uint8_t *data, uint16_t len)
{ sends++; last_command=cmd; assert(len<=32); memcpy(last_payload,data,len); return send_ok?len+5:-1; }
bool settings_detail_send_command(uint8_t cmd,const uint8_t *data,uint16_t len)
{ return protocol_send(cmd,data,len)>=0; }
uint8_t machine_state_double_note_level(void) { return double_level; }
uint8_t machine_state_flap_position(void) { return flap; }
uint8_t machine_state_reject_pocket_max(void) { return reject_capacity; }
bool setting_service_request_double_note_level(uint8_t target,uint8_t previous)
{ assert(previous==double_level); return settings_detail_send_command(0x31,&target,1); }
bool setting_service_request_flap_position(uint8_t target,uint8_t previous)
{ assert(previous==flap); return settings_detail_send_command(0x42,&target,1); }
bool setting_service_request_reject_pocket_max(uint8_t target,uint8_t previous)
{ assert(previous==reject_capacity); return settings_detail_send_command(0x33,&target,1); }
void setting_service_clear_double_note_level_request(void) {}
void setting_service_clear_reject_pocket_max_request(void) {}
static ui_factory_reset_phase_t factory_phase;
ui_factory_reset_phase_t ui_factory_reset_get_phase(void) { return factory_phase; }
bool ui_factory_reset_blocks_navigation(void) { return factory_phase != UI_FACTORY_RESET_IDLE; }
const char *ui_factory_reset_blocker(void) { return NULL; }
bool ui_factory_reset_request(void) {
    uint8_t value=1;
    if (!settings_detail_send_command(0x44,&value,1)) return false;
    factory_phase=UI_FACTORY_RESET_WAIT_CONTROLLER; return true;
}
bool ui_factory_reset_retry_local(void) {
    if (factory_phase != UI_FACTORY_RESET_LOCAL_FAILED) return false;
    factory_phase=UI_FACTORY_RESET_SAVING; return true;
}
void currency_state_get_active_code(char out[4]) { memcpy(out,"CNY",4); }
bool ui_manager_pop_page(void) { pops++; return true; }
void ui_manager_push_page(ui_page_t page) { pushed_page=page; }
void ui_manager_switch(ui_page_t page) { (void)page; pops++; }
void ui_manager_clear_stack(void) {}
void ui_upgrade_service_reboot(void) { reboots++; }
bool backlight_service_probe(void) { return backlight_ok; }
int backlight_service_level(void) { return brightness; }
int backlight_service_max(void) { return backlight_ok ? 100 : 0; }
bool backlight_service_set(int level) { if(!backlight_ok)return false; brightness=level; return true; }
bool backlight_service_save(void) { return persist_ok; }
const char *user_cfg_password_get(void) { return password; }
bool user_cfg_password_save(const char *value) { if(!persist_ok)return false; memcpy(password,value,5);return true; }
bool user_cfg_password_visibility_enabled(void) { return false; }
bool user_cfg_password_visibility_save(bool visible) { (void)visible;return true; }


void gesture_service_set_page_policy(uint32_t owner,bool (*drag)(void),bool (*handler)(gesture_action_t))
{ (void)owner;(void)drag;gesture_policy=handler; }
void gesture_service_clear_page_policy(uint32_t owner) { (void)owner;gesture_policy=NULL; }
bool settings_detail_overlay_is_open(void) { return overlay; }
void settings_detail_dialog_hide(void) { overlay=false;confirm_dialog=NULL; }
bool settings_detail_dialog_show_ex(settings_detail_dialog_kind_t kind,const char *title,const char *body,
    const char *ok,const char *cancel,settings_detail_dialog_cb_t confirm,settings_detail_dialog_cb_t reject,void *data)
{ (void)kind;(void)title;(void)body;(void)ok;(void)cancel;(void)reject;overlay=true;confirm_dialog=confirm;dialog_data=data;return true; }
bool settings_detail_keyboard_show_ex(const char *title,const char *initial,uint16_t length,
    settings_detail_keyboard_mode_t mode,settings_detail_keyboard_cb_t confirm,void *data,
    settings_detail_keyboard_close_cb_t close,void *close_data)
{ (void)title;(void)initial;(void)length;(void)mode;(void)close;(void)close_data;keyboard_confirm=confirm;keyboard_data=data;return true; }
bool settings_detail_keyboard_show(const char *title,const char *initial,uint16_t length,
    settings_detail_keyboard_mode_t mode,settings_detail_keyboard_cb_t confirm,void *data)
{ return settings_detail_keyboard_show_ex(title,initial,length,mode,confirm,data,NULL,NULL); }
void settings_detail_keyboard_hide(void) { keyboard_confirm=NULL; }

static lv_color_t pixels[1280*400], buffer[1280*40];
static lv_point_t pointer_position;
static lv_indev_state_t pointer_state;
static void pointer_read(lv_indev_drv_t *driver,lv_indev_data_t *data)
{ (void)driver;data->point=pointer_position;data->state=pointer_state; }
static void pointer_feed(int x,int y,lv_indev_state_t state)
{ pointer_position=(lv_point_t){x,y};pointer_state=state;for(unsigned i=0;i<3;i++){lv_tick_inc(16);lv_timer_handler();} }
static void pointer_tap(int x,int y)
{ pointer_feed(x,y,LV_INDEV_STATE_PRESSED);pointer_feed(x,y,LV_INDEV_STATE_RELEASED); }
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p)
{ for(int y=a->y1;y<=a->y2;y++)memcpy(pixels+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d); }
static lv_res_t info(lv_img_decoder_t *d,const void *src,lv_img_header_t *h)
{ (void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;
  const un260_compiled_asset_t *a=test_page_asset_find(src);if(!a){fprintf(stderr,"Missing asset: %s\n",(const char*)src);abort();}
  memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK; }
static lv_res_t image_open(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s)
{ if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=test_page_asset_find(s->src)->pixels;return LV_RES_OK; }
#include "test_settings_toolbar.h"
static void snapshot(const char *name)
{ lv_obj_update_layout(lv_scr_act());assert_settings_toolbars(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);
  char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),name);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f); }
static lv_obj_t *find_label(lv_obj_t *o,const char *text)
{ if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;
  for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_label(lv_obj_get_child(o,i),text);if(r)return r;}return NULL; }
static lv_obj_t *find_type(lv_obj_t *o,const lv_obj_class_t *type)
{ if(lv_obj_check_type(o,type))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_type(lv_obj_get_child(o,i),type);if(r)return r;}return NULL; }
static lv_obj_t *button(const char *text)
{ lv_obj_t *o=find_label(lv_scr_act(),text);assert(o);while(o&&!lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE))o=lv_obj_get_parent(o);assert(o);return o; }
static void click(const char *text) { lv_event_send(button(text),LV_EVENT_CLICKED,NULL); }
static void accept(void) { assert(confirm_dialog);settings_detail_dialog_cb_t cb=confirm_dialog;void *data=dialog_data;settings_detail_dialog_hide();cb(data); }
static void advance(unsigned ms) { lv_tick_inc(ms);lv_timer_handler(); }

#include "un260/lv_core/page_collection.h"
#include "un260/lv_core/page_protocol_log.h"
#include "un260/storage/protocol_log.h"
#include "un260/data_collection/data_collection.h"
#include "un260/app_service/app_collection.h"
#include "un260/lv_components/smart_island/smart_island_activity.h"
#include "un260/lv_components/smart_island/smart_island_internal.h"
smart_island_context_t g_si_ctx;
static protocol_log_snapshot_t log_state;
void protocol_log_snapshot(protocol_log_snapshot_t *s){*s=log_state;}
bool protocol_log_init(void){return true;}
bool protocol_log_start(void){log_state.active=true;log_state.revision++;return true;}
bool protocol_log_stop(void){log_state.active=false;log_state.revision++;return true;}
bool protocol_log_export(uint64_t id){(void)id;return true;}
bool protocol_log_delete(uint64_t id){(void)id;return true;}
ui_page_t ui_manager_get_current_page(void){return UI_PAGE_MAIN;}
bool app_command_runtime_count_start_busy(void){return false;}
void page_06_settings_open_collection(void){}
bool settings_detail_dialog_show(const char *a,const char *b,const char *c,const char *d,settings_detail_dialog_cb_t e,settings_detail_dialog_cb_t f,void *g){return settings_detail_dialog_show_ex(SETTINGS_DIALOG_INFO,a,b,c,d,e,f,g);}
static void receive(unsigned code){uint8_t b[]={0xFD,0xDF,6,0xC0,code,0x0A};advance(10);data_collection_reply_handle(b,6,lv_tick_get());}
static void capture_pages(void){
 for(size_t lang=0;lang<ui_lang_count();lang++){
  const ui_locale_t *l=ui_lang_at(lang);assert(ui_lang_prepare_resources(l->id));ui_lang_set(l->id);
  lv_settings_header_t h={.title=ui_tr("Data collection")};lv_settings_frame_t f=lv_settings_frame_create(lv_scr_act(),&h);page_collection_create(&f);
  lv_obj_update_layout(lv_scr_act());char name[100];snprintf(name,sizeof(name),"capture-%s-off",l->tag);snapshot(name);
  assert(app_collection_request(DATA_COLLECT_MODE_FALSE));assert(last_command==0xC0&&last_payload[0]==2);receive(3);page_collection_refresh();
  snprintf(name,sizeof(name),"capture-%s-on",l->tag);snapshot(name);
  assert(app_collection_request(DATA_COLLECT_MODE_NONE));advance(4000);assert(data_collection_request_take_timeout(lv_tick_get()));page_collection_refresh();
  snprintf(name,sizeof(name),"capture-%s-close-unknown",l->tag);snapshot(name);receive(0xFF);lv_obj_del(f.root);
  log_state=(protocol_log_snapshot_t){.active=true,.state=PLOG_ACTIVE,.current=6,.count=5,.revision=lang+1};
  for(unsigned i=0;i<5;i++){log_state.files[i].id=6-i;log_state.files[i].bytes=1024*(i+1);snprintf(log_state.files[i].name,48,"log_%012u.txt",6-i);}
  page_protocol_log_open();advance(250);page_protocol_log_poll();lv_obj_update_layout(lv_layer_top());assert_settings_toolbars(lv_layer_top());snprintf(name,sizeof(name),"logs-%s",l->tag);snapshot(name);page_protocol_log_close();
 }
 ui_lang_set(LANGUAGE_CN);assert(ui_lang_prepare_resources(LANGUAGE_CN));
 g_si_ctx.objects.root=lv_settings_box(lv_scr_act(),492,348,261,44,0x111111);g_si_ctx.lifecycle.created=true;g_si_ctx.view.scene=SMART_ISLAND_SCENE_IDLE;g_si_ctx.view.visual=SMART_ISLAND_VISUAL_COMPACT;
 page_collection_main_badge_create(lv_scr_act());assert(app_collection_request(DATA_COLLECT_MODE_FALSE));receive(3);data_collection_count_start();receive(4);data_collection_count_end(12);advance(250);page_collection_poll();snapshot("island-collecting");
 receive(7);advance(250);page_collection_poll();snapshot("island-success");
 g_si_ctx.lifecycle.count_session_active=true;g_si_ctx.view.scene=SMART_ISLAND_SCENE_COUNTING;assert(!smart_island_activity_set(SI_ACTIVITY_WORKING,"Collecting","",NULL));
 g_si_ctx.lifecycle.count_session_active=false;g_si_ctx.view.scene=SMART_ISLAND_SCENE_IDLE;g_si_ctx.lifecycle.suspended=true;assert(!smart_island_activity_set(SI_ACTIVITY_WORKING,"Collecting","",NULL));smart_island_activity_destroy();lv_obj_del(g_si_ctx.objects.root);g_si_ctx.objects.root=NULL;
 puts("PASS capture settings and log settings: ten locales, actual fonts, protocol controls; collection overlay never replaces counting/suspended scene");
}
int main(void)
{
    lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&db;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,info);lv_img_decoder_set_open_cb(decoder,image_open);
    lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;lv_indev_drv_register(&input);
    capture_pages();
    lv_deinit();return 0;
}
