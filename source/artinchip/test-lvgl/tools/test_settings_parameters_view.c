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
#include "un260/lv_core/page_33_set_brightness.h"
#include "un260/lv_core/page_36_display_test.h"
#include "un260/lv_components/lv_nav_button.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_print_toast.h"
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
static char password[5] = "1111", toast[160];
static settings_detail_dialog_cb_t confirm_dialog;
static void *dialog_data;
static settings_detail_keyboard_cb_t keyboard_confirm;
static void *keyboard_data;
static bool (*gesture_policy)(gesture_action_t);
uint64_t app_clock_monotonic_ms(void) { return lv_tick_get(); }
uint32_t app_clock_uptime_ms(void) { return lv_tick_get(); }
int protocol_send(uint8_t cmd, const uint8_t *data, uint16_t len)
{ sends++; last_command=cmd; assert(len<=32); memcpy(last_payload,data,len); return send_ok?0:-1; }
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
bool setting_service_request_factory_reset(void) { uint8_t value=1; return settings_detail_send_command(0x44,&value,1); }
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
lv_print_toast_config_t lv_print_toast_get_default_config(void) { return (lv_print_toast_config_t){0}; }
void lv_print_toast_show_with_config(const lv_print_toast_config_t *cfg) { snprintf(toast,sizeof(toast),"%s",cfg->text); }
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

static void immediate_pages(void)
{
    setting_value_result_t result={.target=3,.previous=2,.success=false};
    ui_page_22_set_double_note_create(lv_scr_act());snapshot("double");
    lv_color_t checked=lv_obj_get_style_bg_color(button(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_2)),0);
    click(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3));assert(lv_obj_get_style_bg_color(button(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_2)),0).full==checked.full);assert(last_command==0x31&&last_payload[0]==3);
    assert(find_label(lv_scr_act(),"Confirmed level: 2"));snapshot("double-pending");
    unsigned sent_before=sends,notice_before=action_notices;
    assert(action_blocked(button(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3))));
    click(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3));
    assert(sends==sent_before&&action_notices==notice_before+1&&strstr(action_notice,"confirm"));
    ui_page_22_set_double_note_on_reply(&result);assert(find_label(lv_scr_act(),"Confirmed level: 2"));
    click(ui_text_get(UI_TEXT_SETTINGS_DOUBLE_NOTE_LEVEL_3));double_level=3;result.success=true;
    ui_page_22_set_double_note_on_reply(&result);assert(find_label(lv_scr_act(),"Confirmed level: 3"));ui_page_22_set_double_note_destroy();
    ui_page_23_set_flap_create(lv_scr_act());snapshot("flap");click(ui_text_get(UI_TEXT_SETTINGS_FLAP_DOWN));
    assert(last_command==0x42&&last_payload[0]==2);flap=2;ui_page_23_set_flap_on_reply(&result);ui_page_23_set_flap_destroy();
    ui_page_24_set_reject_pocket_create(lv_scr_act());snapshot("reject");click("Enter capacity");assert(keyboard_confirm);
    unsigned before=sends;keyboard_confirm("0",keyboard_data);assert(sends==before);keyboard_confirm("75",keyboard_data);
    assert(last_payload[0]==75);reject_capacity=75;ui_page_24_set_reject_pocket_on_reply(&result);ui_page_24_set_reject_pocket_destroy();
    serial_number_state_confirm(false,0);ui_page_25_set_serial_number_create(lv_scr_act());snapshot("serial-off");
    click(ui_text_get(UI_TEXT_SETTINGS_SERIAL_LEVEL_2));assert(last_command==0x32&&last_payload[0]==2);
    serial_number_setting_result_t serial_result;assert(serial_number_service_take_reply(2,1,&serial_result));
    serial_number_state_confirm(true,2);ui_page_25_set_serial_number_on_reply(2,1);snapshot("serial-confirmed");ui_page_25_set_serial_number_destroy();
    puts("PASS immediate parameters: confirmed values, pending locks, reject range, ACK refresh and serial OFF=0");
}

static void print_page_test(void)
{
    unsigned before=sends;
    ui_page_20_set_print_create(lv_scr_act());snapshot("print");
    lv_obj_t *receipt_body=lv_obj_get_parent(find_label(lv_scr_act(),"COUNT REPORT"));
    lv_obj_t *paper=lv_obj_get_parent(receipt_body);
    click("2");lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_y(receipt_body)==16&&lv_obj_get_height(paper)==192);
    snapshot("print-top-spacing");
    lv_obj_t *rows=lv_obj_get_parent(find_label(lv_scr_act(),ui_text_get(UI_TEXT_SETTINGS_PRINT_SPACE_BOTTOM)));
    lv_obj_t *bottom_input=NULL,*bottom_base=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(rows);i++){
        lv_obj_t *o=lv_obj_get_child(rows,i);
        if(lv_obj_get_x(o)==646)bottom_input=o;
        if(lv_obj_get_x(o)==350)bottom_base=o;
    }
    assert(bottom_input&&bottom_base);
    lv_obj_t *two=find_label(bottom_base,"2");assert(two);
    lv_event_send(lv_obj_get_parent(two),LV_EVENT_CLICKED,NULL);
    lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_y(receipt_body)==16&&lv_obj_get_height(paper)==208);
    snapshot("print-both-spacing");
    lv_event_send(bottom_input,LV_EVENT_CLICKED,NULL);assert(keyboard_confirm);
    keyboard_confirm("99",keyboard_data);settings_detail_keyboard_hide();
    lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_height(paper)==216&&lv_obj_get_y(receipt_body)>0);
    assert(find_label(lv_scr_act(),"Example / blank spacing scaled"));
    snapshot("print-large-spacing");click("Cancel");lv_obj_update_layout(lv_scr_act());
    assert(lv_obj_get_height(paper)==176&&lv_obj_get_y(receipt_body)==0&&sends==before);
    puts("PASS receipt top/bottom preset and keyboard preview, bounded large margins, Cancel rollback, no draft sends");
    click("Serial");
    click("Not set");assert(keyboard_confirm);keyboard_confirm("UNION",keyboard_data);settings_detail_keyboard_hide();
    click("2");assert(sends==before);snapshot("print-draft");click("Save");
    assert(last_command==0x41&&last_payload[0]==2&&last_payload[1]==1);
    print_config_request_result_t result;
    assert(print_config_take_status_reply(1,&result));ui_page_20_set_print_on_reply(&result);
    assert(last_payload[0]==3&&last_payload[1]==1&&last_payload[2]==2);
    assert(print_config_take_status_reply(0,&result));ui_page_20_set_print_on_reply(&result);
    print_config_value_t actual;print_config_get(&actual);assert(!strcmp(actual.head1,"UNION")&&actual.space_top==0&&actual.content==1);
    snapshot("print-partial-save");click("Save");assert(last_payload[0]==3);
    assert(print_config_take_status_reply(1,&result));ui_page_20_set_print_on_reply(&result);
    assert(last_payload[0]==1);
    assert(print_config_take_status_reply(1,&result));ui_page_20_set_print_on_reply(&result);
    print_config_get(&actual);assert(actual.space_top==2&&actual.content==2);
    assert(action_blocked(button("Save")));
    click("Summary");click("Cancel");
    assert(lv_obj_has_state(button("Serial"),LV_STATE_CHECKED));
    ui_page_20_set_print_destroy();
    puts("PASS print draft isolation, serialized Save, partial failure retry and Cancel");
}

static void cfd_page_test(void)
{
    uint8_t reply[16]={'C','N','Y',1,3,3,3,3,3,3,3,3,3,3,3,3};
    send_ok=false;ui_page_27_set_cfd_level_create(lv_scr_act());assert(action_blocked(button("1")));
    snapshot("cfd-query-failed");send_ok=true;click("Retry");assert(last_command==0x45&&last_payload[0]==1);
    assert(!lv_obj_is_visible(find_label(lv_scr_act(),"IR")));
    snapshot("cfd-loading");
    lv_obj_t *cover=lv_obj_get_parent(find_label(lv_scr_act(),"Reading detection levels"));
    lv_obj_t *body=lv_obj_get_parent(cover);
    assert(lv_obj_get_width(cover)==lv_obj_get_content_width(body));
    assert(lv_obj_get_height(cover)==lv_obj_get_content_height(body));
    lv_obj_set_height(body,312);lv_obj_update_layout(body);
    assert(lv_obj_get_height(cover)==lv_obj_get_content_height(body));
    lv_obj_set_height(body,300);lv_obj_update_layout(body);
    ui_page_27_set_cfd_level_on_info(reply,sizeof(reply));
    assert(!lv_obj_is_visible(find_label(lv_scr_act(),"IR")));
    assert(!find_label(lv_scr_act(),"Levels confirmed. Select a channel level to edit."));
    snapshot("cfd-ack-animation");
    assert(find_label(lv_scr_act(),"Reading detection levels"));advance(920);
    assert(lv_obj_is_visible(find_label(lv_scr_act(),"IR")));
    assert(!find_label(lv_scr_act(),"Reading detection levels"));snapshot("cfd");
    lv_obj_t *segment=lv_obj_get_parent(button("4"));
    lv_obj_t *row=lv_obj_get_parent(segment);lv_obj_t *rows=lv_obj_get_parent(row);
    lv_area_t control_area,row_area;lv_obj_get_coords(segment,&control_area);lv_obj_get_coords(row,&row_area);
    assert(control_area.y1-row_area.y1>=8&&row_area.y2-control_area.y2>=8);
    assert(lv_obj_get_scroll_bottom(rows)<=2);
    lv_obj_scroll_to_y(rows,999,LV_ANIM_OFF);snapshot("cfd-bottom");
    lv_obj_scroll_to_y(rows,0,LV_ANIM_OFF);
    click("4");assert(lv_obj_has_state(button("4"),LV_STATE_CHECKED));assert(gesture_policy&&gesture_policy(GESTURE_ACTION_HOME)&&overlay);
    settings_detail_dialog_hide();click(ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UPDATE));assert(last_payload[0]==2);
    unsigned notices_before=action_notices,pops_before=pops;
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(action_notices==notices_before+1&&pops==pops_before);assert(gesture_policy(GESTURE_ACTION_HOME));
    advance(800);assert(cfd_service_take_update_timeout());ui_page_27_set_cfd_level_on_request_failed();
    assert(find_label(lv_scr_act(),"4"));snapshot("cfd-save-failed");click(ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UPDATE));
    reply[4]=4;reply[3]=2;ui_page_27_set_cfd_level_on_info(reply,sizeof(reply));assert(cfd_service_busy());
    reply[3]=1;ui_page_27_set_cfd_level_on_info(reply,sizeof(reply));assert(!cfd_service_busy());
    assert(action_blocked(button(ui_text_get(UI_TEXT_SETTINGS_CFD_LEVEL_UPDATE))));
    ui_page_27_set_cfd_level_destroy();assert(!gesture_policy);ui_page_27_set_cfd_level_on_info(reply,sizeof(reply));
    puts("PASS CFD query gate, draft retention, Home/Back guard, timeout retry, wrong profile rejection and valid echo");
}

static void password_page_test(void)
{
    /* Modal over an existing real page. Outside taps cannot reach its fields. */
    ui_page_20_set_print_create(lv_scr_act());
    ui_page_05_set_password_open();
    assert(ui_page_05_set_password_is_open());
    advance(120);snapshot("login-modal");
    assert(find_label(lv_layer_top(),"Settings access"));
    assert(find_label(lv_layer_top(),"Clear")&&!find_label(lv_layer_top(),"Confirm"));
    lv_obj_t *panel=lv_obj_get_parent(find_label(lv_layer_top(),"Settings access"));
    assert(lv_obj_get_width(panel)==850&&lv_obj_get_height(panel)==364);
    assert(lv_obj_get_style_opa(panel,0)==LV_OPA_COVER);
    assert(lv_obj_get_x(panel)==412&&lv_obj_get_y(panel)==18);
    assert(lv_color_to32(lv_obj_get_style_bg_color(panel,0))==lv_color_to32(lv_color_hex(0xFBFCFD)));
    lv_obj_t *show=find_label(lv_layer_top(),"Show");
    assert((!show||!lv_obj_is_visible(show))&&!find_label(lv_layer_top(),"Close"));
    assert(find_label(lv_layer_top(),"For authorized configuration and service."));
    lv_obj_t *one=lv_obj_get_parent(find_label(lv_layer_top(),"1"));
    lv_obj_t *clear=lv_obj_get_parent(find_label(lv_layer_top(),"Clear"));
    assert(lv_obj_get_width(one)==107&&lv_obj_get_height(one)==65);
    assert(lv_obj_get_x(one)==486&&lv_obj_get_y(one)==55);
    assert(lv_obj_get_x(clear)==486&&lv_obj_get_y(clear)==273);
    static lv_color_t stable[810*320];
    for(int y=0;y<320;y++)memcpy(stable+y*810,pixels+(y+38)*1280+432,810*sizeof(lv_color_t));
    lv_obj_t *behind=lv_label_create(lv_scr_act());lv_obj_set_pos(behind,430,280);
    for(unsigned i=0;i<8;i++){
        lv_label_set_text(behind,i%2?"Changing underneath":"Background content");
        advance(500); /* Includes the old cursor's period and background invalidation. */
        for(int y=0;y<320;y++)assert(!memcmp(stable+y*810,pixels+(y+38)*1280+432,810*sizeof(lv_color_t)));
    }
    lv_obj_del(behind);
    unsigned before=pops;pointer_tap(25,200);
    assert(!ui_page_05_set_password_is_open()&&pops==before);
    ui_page_05_set_password_open();advance(20);
    pointer_tap(952,106);pointer_tap(1067,106); /* 1,2 */
    snapshot("login-partial");
    pointer_tap(1179,324); /* Right-hand outline backspace. */
    pointer_tap(952,324); /* Left-hand Clear. */
    snapshot("login-cleared");
    for(unsigned i=0;i<4;i++)pointer_tap(1067,324);
    lv_obj_t *error=find_label(lv_layer_top(),"Incorrect password. Try again.");
    assert(error&&lv_obj_is_visible(error));
    assert(lv_color_to32(lv_obj_get_style_text_color(error,0))==lv_color_to32(lv_color_hex(0xB23E40)));
    snapshot("login-error");
    pointer_tap(952,106);
    assert(find_label(lv_layer_top(),"Opens automatically when the code is correct."));
    pointer_tap(1224,51); /* Original top-right x, not a Close text button. */
    assert(!ui_page_05_set_password_is_open()&&pops==before);
    for(unsigned i=0;i<20;i++){
        ui_page_05_set_password_open();advance(20);
        assert(ui_page_05_set_password_request_back());
        advance(200);assert(!ui_page_05_set_password_is_open());
    }
    ui_page_05_set_password_destroy();ui_page_20_set_print_destroy();
    puts("PASS real LVGL login: first-concept geometry/colors/labels, opaque surface, 4s stable pixels, Clear/backspace/x/outside dismissal and repeated lifecycle");
    ui_page_29_set_password_create(lv_scr_act());snapshot("password");click("Current password");
    snapshot("password-keypad");
    click("9");click("9");click("9");click("9");click("Confirm");
    assert(find_label(lv_scr_act(),"Incorrect current PIN. Try again."));
    click("Cancel");click("Current password");
    for(unsigned i=0;i<4;i++){char key[2]={password[i],0};click(key);}click("Confirm");
    assert(find_label(lv_scr_act(),"New password"));
    click("0");click("0");click("2");click("2");click("Confirm");
    click("0");click("0");click("2");click("3");click("Confirm");
    assert(find_label(lv_scr_act(),"PINs do not match. Re-enter the new PIN."));
    click("Cancel");click("Confirm password");click("0");click("0");click("2");click("2");click("Confirm");
    assert(strcmp(password,"0022"));click("Save");assert(!strcmp(password,"0022"));
    ui_page_29_set_password_destroy();assert(!gesture_policy);
    puts("PASS PIN wizard: wrong old PIN blocked, staged progression, mismatch blocked, Save only");
}

static void brightness_test(void)
{
    ui_page_33_set_brightness_create(lv_scr_act());snapshot("brightness");lv_obj_t *slider=find_type(lv_scr_act(),&lv_slider_class);assert(slider);
    lv_slider_set_value(slider,80,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);assert(brightness==80);snapshot("brightness-preview");
    persist_ok=false;click("Keep");advance(8001);assert(brightness==50);persist_ok=true;
    lv_slider_set_value(slider,70,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);click("Keep");ui_page_33_set_brightness_destroy();assert(brightness==70);
    ui_page_33_set_brightness_create(lv_scr_act());slider=find_type(lv_scr_act(),&lv_slider_class);
    lv_slider_set_value(slider,90,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);ui_page_33_set_brightness_destroy();assert(brightness==70);
    backlight_ok=false;ui_page_33_set_brightness_create(lv_scr_act());snapshot("brightness-unavailable");
    assert(lv_obj_has_state(find_type(lv_scr_act(),&lv_slider_class),LV_STATE_DISABLED));ui_page_33_set_brightness_destroy();backlight_ok=true;
    puts("PASS brightness actual service-edge preview, save failure rollback, Keep, destroy rollback and unavailable driver");
}

static void factory_test(void)
{
    ui_page_30_set_factory_create(lv_scr_act());snapshot("factory");
    assert(lv_color_to32(lv_obj_get_style_bg_color(button(ui_text_get(UI_TEXT_SETTINGS_FACTORY_START)),0))==lv_color_to32(lv_color_hex(0xB42332)));
    click(ui_text_get(UI_TEXT_SETTINGS_FACTORY_START));
    assert(overlay);accept();assert(last_command==0x44);
    unsigned notices_before=action_notices,pops_before=pops;
    assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);
    assert(action_notices==notices_before+1&&pops==pops_before);
    assert(gesture_policy(GESTURE_ACTION_HOME));ui_page_30_set_factory_on_reply(0);settings_detail_dialog_hide();
    click(ui_text_get(UI_TEXT_SETTINGS_FACTORY_START));accept();ui_page_30_set_factory_on_reply(1);accept();
    assert(last_command==0x3B&&reboots==0);ui_page_30_set_factory_destroy();advance(1001);assert(reboots==1);
    puts("PASS reset explicit confirmation, pending navigation lock, failure retry, success-only restart");
}

static void color_calibration_test(void)
{
    ui_page_36_display_test_create(lv_scr_act());snapshot("color-gray");
    assert(find_label(lv_scr_act(),"Not enabled for the current UI output.\nThese references do not alter color output."));
    click("2  Near white");snapshot("color-white");click("3  Color");snapshot("color-rgb");
    click("4  Interface");snapshot("color-interface");
    click("Adjust");assert(pushed_page==UI_PAGE_BRIGHTNESS_SETTING);
    brightness=60;advance(501);assert(find_label(lv_scr_act(),"60%"));
    ui_page_36_display_test_destroy();advance(1000);
    backlight_ok=false;ui_page_36_display_test_create(lv_scr_act());
    assert(action_blocked(button("Adjust")));snapshot("color-unavailable");
    ui_page_36_display_test_destroy();backlight_ok=true;advance(1000);
    puts("PASS color references, shared brightness route, refreshed value, unavailable driver and timer teardown");
}

int main(void)
{
    lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
    lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&db;driver.flush_cb=flush;lv_disp_drv_register(&driver);
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,info);lv_img_decoder_set_open_cb(decoder,image_open);
    lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;lv_indev_drv_register(&input);
    lv_obj_t *palette=lv_settings_segment_base(lv_scr_act(),0,0,240,48);
    lv_obj_t *choice=lv_settings_segment(palette,0,2,"Palette",NULL,NULL);
    assert(lv_color_to32(lv_obj_get_style_bg_color(palette,0))==lv_color_to32(lv_color_hex(0xF7F7F7)));
    assert(lv_color_to32(lv_obj_get_style_bg_color(choice,0))==lv_color_to32(lv_color_hex(0xF7F7F7)));
    lv_obj_add_state(choice,LV_STATE_CHECKED|LV_STATE_DISABLED);
    assert(lv_color_to32(lv_obj_get_style_bg_color(choice,0))==lv_color_to32(lv_color_hex(0xFFFFFF)));
    lv_obj_del(palette);
    palette=lv_settings_button(lv_scr_act(),0,0,140,48,"Action",true,NULL,NULL);
    assert(lv_color_to32(lv_obj_get_style_bg_color(palette,0))==lv_color_to32(lv_color_hex(0x1559B7)));
    assert(lv_color_to32(lv_obj_get_style_text_color(lv_obj_get_child(palette,0),0))==lv_color_to32(lv_color_hex(0xFFFFFF)));
    lv_obj_add_state(palette,LV_STATE_DISABLED);
    assert(lv_color_to32(lv_obj_get_style_text_color(lv_obj_get_child(palette,0),0))==lv_color_to32(lv_color_hex(0x64717C)));
    lv_obj_clear_state(palette,LV_STATE_DISABLED);lv_settings_action_style(palette,LV_SETTINGS_ACTION_SECONDARY);
    assert(lv_color_to32(lv_obj_get_style_bg_color(palette,0))==lv_color_to32(lv_color_hex(0xF7F7F7)));
    lv_obj_add_state(palette,LV_STATE_PRESSED);
    assert(lv_color_to32(lv_obj_get_style_bg_color(palette,0))==lv_color_to32(lv_color_hex(0xE2E9EE)));
    lv_obj_del(palette);
    immediate_pages();print_page_test();cfd_page_test();password_page_test();brightness_test();
    color_calibration_test();factory_test();
    puts("PASS 10 ordinary settings actual-LVGL host renders and state transitions (not board verification)");return 0;
}
