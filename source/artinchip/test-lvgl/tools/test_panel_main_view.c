/* Production Main and entry rendering. External requests are controlled so
 * assertions distinguish requested settings from controller-confirmed state. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_core/panel_main.c"
#include "un260/font/ui_message_font.h"
#include "un260/font/ui_locale_font_bank.h"
#include "src/misc/lv_txt_ap.h"
#include "un260/counting/counting_data_store_internal.h"
#include "actual_amount.inc"
#include "panel_view_checks.h"

static bool busy,confirmed,applying,send_ok=true;
static unsigned requests,notices,toggles,guide_requests,clear_requests,cancel_calls,confirm_calls;
static unsigned requested_speed,requested_mode,requested_batch;
static page_01_detail_section_t section;
static workspace_model_t workspace;
static lv_color_t pixels[1280*400],draw_pixels[1280*40];
static const char *output;
static uint8_t fingers=1;
static int list_currency=-2,list_tab=-1;
static unsigned key_overflows;

uint8_t lv_port_indev_touch_count(void){return fingers;}
bool app_command_runtime_count_start_busy(void){return busy;}
bool app_command_runtime_counting_confirmed(void){return confirmed;}
bool workspace_service_applying(void){return applying;}
bool app_add_runtime_busy(void){return false;}
bool setting_service_mode_is_pending(void){return false;}
bool setting_service_request_speed(uint8_t value){requests++;requested_speed=value;return send_ok;}
bool setting_service_request_mode(uint8_t value){requests++;requested_mode=value;return send_ok;}
bool setting_service_request_batch_switch(bool enabled,uint8_t value,bool previous,uint8_t old)
{(void)enabled;(void)previous;(void)old;requests++;requested_batch=value;return send_ok;}
bool currency_service_request_switch(uint8_t index,const char code[4]){(void)index;(void)code;requests++;return send_ok;}
void currency_service_cancel_switch(void){}
int protocol_send(uint8_t command,const uint8_t *data,uint16_t size)
{(void)command;(void)data;return send_ok?(int)size:-1;}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail)
{(void)kind;(void)key;(void)title;(void)detail;notices++;}
void ui_notice_post_message(ui_notice_kind_t kind,const char *key,const char *title,const ui_message_t *message)
{(void)kind;(void)key;(void)title;(void)message;notices++;}
bool app_command_runtime_clear_counting_data(const char *reason){(void)reason;clear_requests++;return send_ok;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;guide_requests++;return true;}
bool app_fault_recovery_request_clear(machine_fault_key_t key){(void)key;clear_requests++;return send_ok;}
const workspace_model_t *workspace_store_get(void){return &workspace;}
const workspace_user_t *workspace_active(const workspace_model_t *model){return &model->shared;}
void counting_history_current_invalidate(void){}
void page_01_detail_section_set(page_01_detail_section_t value,bool refresh){(void)refresh;section=value;}
page_01_detail_section_t page_01_detail_section_get(void){return section;}
void ui_state_save_page01_detail_section(void){}
ui_main_batch_action_t ui_state_main_batch_action_get(void){return UI_MAIN_BATCH_SETTINGS;}
bool page_01_main_toggle_display_mode(void){toggles++;return true;}
void page_01_mode_cycle(void){requests++;requested_mode=MODE_SDC;}
void page_01_action_run(page_01_action_t id){(void)id;requests++;}
void page_02_list_multi_open(int currency,int tab){list_currency=currency;list_tab=tab;}

static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *source)
{
    for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++){
        if(x>=0&&x<1280&&y>=0&&y<400)pixels[y*1280+x]=*source;
        source++;
    }
    lv_disp_flush_ready(driver);
}
static void advance(unsigned ms)
{for(unsigned t=0;t<ms;t+=20){lv_tick_inc(20);lv_timer_handler();}}
static void assert_key_text(lv_obj_t *object,const lv_area_t *key)
{
    if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(object,&lv_label_class)&&*lv_label_get_text(object)){
        lv_area_t area;lv_obj_get_coords(object,&area);
        if(area.x1<key->x1 || area.x2>key->x2 || area.y1<key->y1 || area.y2>key->y2){
            fprintf(stderr,"Key text overflow locale=%u text=%s label=[%d,%d,%d,%d] key=[%d,%d,%d,%d]\n",
                ui_lang_get(),lv_label_get_text(object),area.x1,area.y1,area.x2,area.y2,key->x1,key->y1,key->x2,key->y2);
            key_overflows++;
        }
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(object);i++)assert_key_text(lv_obj_get_child(object,i),key);
}
static void assert_keys_fit(lv_obj_t *root)
{
    for(unsigned wing=0;wing<2;wing++){
        lv_obj_t *parent=lv_obj_get_child(root,wing);
        for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);i++){
            lv_obj_t *key=lv_obj_get_child(parent,i);
            if(!lv_obj_check_type(key,&lv_btn_class)||lv_obj_has_flag(key,LV_OBJ_FLAG_HIDDEN))continue;
            lv_area_t area;lv_obj_get_coords(key,&area);assert_key_text(key,&area);
        }
    }
}
static void assert_speed_key(void);
static void capture(const char *name)
{
    lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);
    lv_obj_t *root=panel_entry_is_open()?lv_obj_get_child(lv_layer_top(),-1):panel_main_root();
    panel_test_layout(root,panel_entry_is_open());panel_test_no_corner_graphics(root);assert_keys_fit(root);
    if(!panel_entry_is_open())assert_speed_key();
    char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",output,name);
    FILE *file=fopen(path,"wb");assert(file);unsigned char h[54]={0};
    h[0]='B';h[1]='M';uint32_t size=54+1280*400*4,offset=54,dib=40,w=1280;
    int32_t height=-400;uint16_t planes=1,bits=32;
    memcpy(h+2,&size,4);memcpy(h+10,&offset,4);memcpy(h+14,&dib,4);memcpy(h+18,&w,4);
    memcpy(h+22,&height,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);
    assert(fwrite(h,1,54,file)==54);assert(fwrite(pixels,4,1280*400,file)==1280*400);fclose(file);
}
static lv_obj_t *find_text(lv_obj_t *parent,const char *text)
{
    if(lv_obj_has_flag(parent,LV_OBJ_FLAG_HIDDEN))return NULL;
    if(lv_obj_check_type(parent,&lv_label_class)){
        char shaped[512];assert(_lv_txt_ap_calc_bytes_cnt(text)<sizeof(shaped));_lv_txt_ap_proc(text,shaped);
        if(!strcmp(lv_label_get_text(parent),shaped))return parent;
    }
    for(uint32_t i=0;i<lv_obj_get_child_cnt(parent);i++){
        lv_obj_t *found=find_text(lv_obj_get_child(parent,i),text);if(found)return found;
    }return NULL;
}
static lv_obj_t *speed_key(void)
{
    lv_obj_t *key=find_text(panel_main_root(),ui_trc("Panel key","Speed"));assert(key);
    while(key&&!lv_obj_check_type(key,&lv_btn_class))key=lv_obj_get_parent(key);
    assert(key);return key;
}
static void no_visible_images(lv_obj_t *object)
{
    if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return;
    assert(!lv_obj_check_type(object,&lv_img_class));
    for(unsigned i=0;i<lv_obj_get_child_cnt(object);i++)no_visible_images(lv_obj_get_child(object,i));
}
static void assert_speed_key(void)
{
    lv_obj_t *key=speed_key(),*body=lv_obj_get_child(key,0);lv_obj_update_layout(key);
    assert(lv_obj_get_x(key)==20&&lv_obj_get_y(key)==268&&lv_obj_get_width(key)==96&&lv_obj_get_height(key)==112);
    no_visible_images(key);assert(find_text(key,speed_text(machine_state_speed())));
    lv_area_t ka,ba;lv_obj_get_coords(key,&ka);lv_obj_get_coords(body,&ba);
    assert(abs((ka.x1+ka.x2)-(ba.x1+ba.x2))<=2&&abs((ka.y1+ka.y2)-(ba.y1+ba.y2))<=2);
}
static void touch_speed_key(void)
{
    lv_obj_t *key=speed_key();lv_event_send(key,LV_EVENT_PRESSED,NULL);
    lv_obj_t *name=find_text(key,ui_trc("Panel key","Speed")),*value=find_text(key,speed_text(machine_state_speed()));
    assert(name&&value&&lv_color_to32(lv_obj_get_style_text_color(name,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
    assert(lv_color_to32(lv_obj_get_style_text_color(value,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
    assert(lv_color_to32(lv_obj_get_style_bg_color(key,0))==lv_color_to32(lv_color_hex(0)));
    assert(lv_obj_get_style_border_width(key,0)==1&&lv_color_to32(lv_obj_get_style_border_color(key,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
    counting_sim_t *data=counting_data_mutable();unsigned saved_pcs=data->total_pcs;
    for(unsigned i=0;i<12;i++){
        data->total_pcs++;panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);advance(20);
        char expected[16];snprintf(expected,sizeof(expected),"%u",(unsigned)data->total_pcs);
        assert(!strcmp(lv_label_get_text(main_view.pcs),expected));assert(speed_key()==key);
        assert(lv_color_to32(lv_obj_get_style_text_color(name,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
        assert(lv_color_to32(lv_obj_get_style_text_color(value,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
        assert(lv_color_to32(lv_obj_get_style_border_color(key,0))==lv_color_to32(lv_color_hex(0x8c8c8c)));
    }
    data->total_pcs=saved_pcs;panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);
    capture("speed-pressed");lv_event_send(key,LV_EVENT_RELEASED,NULL);
    assert(lv_color_to32(lv_obj_get_style_text_color(name,0))==lv_color_to32(lv_color_hex(0xffffff)));
    assert(lv_color_to32(lv_obj_get_style_text_color(value,0))==lv_color_to32(lv_color_hex(0xffffff)));
    lv_event_send(key,LV_EVENT_CLICKED,NULL);
}
static void press_text(lv_obj_t *parent,const char *text)
{
    lv_obj_t *object=find_text(parent,text);if(!object)fprintf(stderr,"Missing clickable text: %s\n",text);assert(object);
    while(object&&!lv_obj_has_flag(object,LV_OBJ_FLAG_CLICKABLE))object=lv_obj_get_parent(object);
    assert(object);lv_event_send(object,LV_EVENT_PRESSED,NULL);lv_event_send(object,LV_EVENT_RELEASED,NULL);lv_event_send(object,LV_EVENT_CLICKED,NULL);
    advance(40);
}
static void assert_regions(void)
{
    lv_obj_t *content=ui_panel_content(&main_view.panel);lv_obj_update_layout(content);
    assert(lv_obj_get_x(content)==244&&lv_obj_get_y(content)==0);
    assert(lv_obj_get_width(content)==792&&lv_obj_get_height(content)==400);
    assert(lv_obj_get_width(main_view.panel.root)==1280&&lv_obj_get_height(main_view.panel.root)==400);
}
static lv_timer_t *main_panel_timer(void)
{
    for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer))
        if(timer->user_data==main_view.panel.private_state)return timer;
    assert(!"Missing panel key feedback timer");return NULL;
}
static void assert_key_feedback(void)
{
    lv_obj_t *right=lv_obj_get_child(panel_main_root(),1),*key=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(right);i++){
        lv_obj_t *candidate=lv_obj_get_child(right,i);
        if(lv_obj_check_type(candidate,&lv_btn_class)&&!lv_obj_has_flag(candidate,LV_OBJ_FLAG_HIDDEN)){key=candidate;break;}
    }
    assert(key);lv_obj_t *glyph=lv_obj_get_child(lv_obj_get_child(key,0),0);
    action(NULL,A_ADD,UI_PANEL_EVENT_CLICK);
    assert(lv_obj_get_style_translate_y(glyph,0)!=0||lv_obj_get_style_img_opa(glyph,0)<255);
    assert(!main_panel_timer()->paused);panel_test_layout(panel_main_root(),false);
    advance(500);assert(!lv_obj_get_style_translate_y(glyph,0)&&lv_obj_get_style_img_opa(glyph,0)==255);
    assert(main_panel_timer()->paused);
}
static void fixture(void)
{
    counting_sim_t *data=counting_data_mutable();
    data->total_pcs=178;data->total_amount=12580;data->denom_number=4;
    data->denom[0]=(denom_t){100,100,10000};data->denom[1]=(denom_t){50,40,2000};
    data->denom[2]=(denom_t){20,20,400};data->denom[3]=(denom_t){10,18,180};
    assert(counting_data_ensure_serial_capacity(data,4));
    for(int i=0;i<4;i++){char text[32];snprintf(text,sizeof(text),"HB0263847%dA",i);data->sn_str[i]=strdup(text);}
    currency_state_reset();currency_state_begin_list_sync();
    assert(currency_state_append_list_code(1,"USD"));assert(currency_state_append_list_code(2,"EUR"));
    assert(currency_state_finish_list_sync());assert(currency_state_confirm_active_selection(2,"USD"));
    machine_state_confirm_mode(MODE_MDC);machine_state_confirm_speed(1);machine_state_confirm_batch(true,199);
    snprintf(workspace.shared.name,sizeof(workspace.shared.name),"Operator 01");
}
static void cancelled(void *user){(void)user;cancel_calls++;}
static bool pin_confirm(const char *text,void *user)
{(void)user;confirm_calls++;assert(strlen(text)==4);panel_entry_set_error(ui_tr("Incorrect password"));return false;}
int main(int argc,char **argv)
{
    assert(argc==2);output=argv[1];lv_init();ui_lang_init();ui_lang_set(LANGUAGE_EN);
    ui_message_font_set_provider(ui_locale_font_compose);
    static lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,draw_pixels,NULL,1280*40);
    static lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;
    driver.draw_buf=&buffer;driver.flush_cb=flush;assert(lv_disp_drv_register(&driver));
    fixture();assert(panel_main_create(lv_scr_act()));panel_main_set_visible(true);
    advance(100);assert_regions();assert(!strcmp(lv_label_get_text(main_view.amount),"12,580"));
    assert(find_text(panel_main_root(),ui_trc("Panel key","Batch")));capture("main-en");
    assert(find_text(panel_test_status_slot(panel_main_root()),ui_tr("Ready")));assert_key_feedback();
    busy=true;panel_main_refresh_start_state();assert(!main_view.running);capture("start-pending");
    assert(find_text(panel_test_status_slot(panel_main_root()),ui_tr("Waiting for device")));
    unsigned before=requests;touch_speed_key();assert(!panel_choice_is_open());assert(requests==before);
    confirmed=true;panel_main_refresh_start_state();assert(main_view.running);advance(160);capture("counting");
    assert(find_text(panel_test_status_slot(panel_main_root()),ui_tr("Counting")));advance(600);assert(main_panel_timer()->paused);
    confirmed=busy=false;panel_main_refresh_start_state();
    touch_speed_key();assert(panel_choice_is_open_for(&main_view));capture("speed");
    applying=true;assert(!picker_confirm(2,&main_view));assert(requests==before);applying=false;
    send_ok=false;assert(!picker_confirm(2,&main_view));assert(machine_state_speed()==1);capture("speed-rejected");
    send_ok=true;assert(picker_confirm(2,&main_view));panel_choice_close_for(&main_view);
    assert(machine_state_speed()==1);assert_speed_key();machine_state_confirm_speed(2);panel_main_refresh(0);assert(machine_state_speed()==2);assert_speed_key();capture("speed-high");
    machine_state_confirm_speed(0);panel_main_refresh(0);assert_speed_key();capture("speed-low");machine_state_confirm_speed(2);panel_main_refresh(0);
    action(NULL,A_MODE_PICK,UI_PANEL_EVENT_CLICK);capture("mode");assert(picker_confirm(MODE_CNT,&main_view));
    assert(requested_mode==MODE_CNT&&machine_state_mode()==MODE_MDC);panel_choice_close_for(&main_view);
    machine_state_confirm_mode(MODE_CNT);panel_main_refresh(0);assert(machine_state_mode()==MODE_CNT);
    machine_state_confirm_mode(MODE_MDC);panel_main_refresh(0);
    action(NULL,A_CURRENCY,UI_PANEL_EVENT_CLICK);capture("currency");assert(picker_confirm(3,&main_view));
    char selected[4];currency_state_get_selected_code(selected);assert(!strcmp(selected,"USD"));panel_choice_close_for(&main_view);
    assert(currency_state_confirm_active_selection(3,"EUR"));panel_main_refresh(0);assert(!strcmp(lv_label_get_text(main_view.currency),"EUR"));
    assert(currency_state_confirm_active_selection(2,"USD"));machine_state_confirm_batch(true,75);panel_main_refresh(0);
    action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);assert(panel_entry_is_open_for(&main_view));capture("numeric-empty");
    press_text(lv_layer_top(),"1");press_text(lv_layer_top(),"9");press_text(lv_layer_top(),"9");capture("numeric");
    before=requests;applying=true;press_text(lv_layer_top(),ui_tr("Confirm"));assert(requests==before);assert(panel_entry_is_open());
    applying=false;press_text(lv_layer_top(),ui_tr("Confirm"));assert(!panel_entry_is_open());assert(requested_batch==199);
    assert(machine_state_batch_num()==75);machine_state_confirm_batch(true,199);panel_main_refresh(0);
    machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,1};machine_fault_report_event(jam);
    panel_main_refresh_start_state();assert(find_text(panel_main_root(),ui_tr("Error")));
    capture("error");action(NULL,A_FAULT_GUIDE,UI_PANEL_EVENT_CLICK);assert(guide_requests==1);
    action(NULL,A_FAULT_CHECK,UI_PANEL_EVENT_CLICK);assert(clear_requests==1);machine_fault_clear();panel_main_refresh_start_state();
    action(NULL,A_TAB_B,UI_PANEL_EVENT_CLICK);capture("serials");action(NULL,A_TAB_A,UI_PANEL_EVENT_CLICK);
    machine_state_confirm_mode(MODE_CNT);panel_main_refresh(0);assert(!strcmp(lv_label_get_text(main_view.amount),"178"));capture("count-mode");
    machine_state_confirm_mode(MODE_MDC);counting_multi_begin(false);
    uint8_t dollar[]={0,0,0,0x0e,'U','S','D',0,0,0x31,0x24,0,178,0,1,0};
    uint8_t euro[]={0,0,0,0x0e,'E','U','R',0,0,0x7,0xd0,0,40,0,1,0};
    assert(counting_multi_info(dollar,sizeof(dollar)));assert(counting_multi_info(euro,sizeof(euro)));
    currency_state_confirm_multi_selection();counting_data_mark_multi_result(counting_data_mutable());panel_main_refresh(0);
    assert(!strcmp(lv_label_get_text(main_view.amount),"218"));assert(!strcmp(lv_label_get_text(main_view.pcs),"2"));
    assert(!strcmp(lv_label_get_text(main_view.denom[0]),"USD"));capture("multi");
    action(NULL,A_CURRENCY_DETAIL+1,UI_PANEL_EVENT_CLICK);assert(list_currency==1&&list_tab==0);
    action(NULL,A_TAB_B,UI_PANEL_EVENT_CLICK);action(NULL,A_CURRENCY_DETAIL,UI_PANEL_EVENT_CLICK);assert(list_currency==0&&list_tab==1);
    action(NULL,A_TAB_A,UI_PANEL_EVENT_CLICK);assert(currency_state_confirm_active_selection(2,"USD"));
    counting_data_reset_result_scope(counting_data_mutable());panel_main_refresh(0);
    int owner=1,other=2;panel_entry_config_t pin={.kind=PANEL_ENTRY_PIN,.title=ui_tr("Password"),
        .prompt=ui_tr("Enter your 4-digit password."),.field_label=ui_tr("Password"),.max_digits=4,.confirm=pin_confirm,.cancel=cancelled,.user=&owner};
    assert(panel_entry_open(lv_layer_top(),&pin));press_text(lv_layer_top(),"7");press_text(lv_layer_top(),"8");capture("password");
    panel_entry_close_for(&other);assert(panel_entry_is_open_for(&owner));
    press_text(lv_layer_top(),"5");press_text(lv_layer_top(),"6");assert(confirm_calls==1);capture("password-rejected");
    press_text(lv_layer_top(),ui_tr("Cancel"));assert(cancel_calls==1&&!panel_entry_is_open());
    action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);press_text(lv_layer_top(),"8");
    lv_obj_t *entry_content=lv_obj_get_child(lv_obj_get_child(lv_layer_top(),-1),2);
    before=requests;lv_event_send(entry_content,LV_EVENT_PRESSED,NULL);lv_event_send(entry_content,LV_EVENT_LONG_PRESSED,NULL);
    lv_event_send(entry_content,LV_EVENT_RELEASED,NULL);assert(panel_entry_is_open());
    lv_event_send(entry_content,LV_EVENT_PRESSED,NULL);lv_event_send(entry_content,LV_EVENT_PRESS_LOST,NULL);
    lv_event_send(entry_content,LV_EVENT_RELEASED,NULL);assert(panel_entry_is_open());
    lv_event_send(entry_content,LV_EVENT_PRESSED,NULL);lv_event_send(entry_content,LV_EVENT_RELEASED,NULL);
    assert(!panel_entry_is_open()&&requests==before);
    action(NULL,A_SPEED,UI_PANEL_EVENT_CLICK);lv_obj_t *shade=lv_obj_get_child(ui_panel_content(&main_view.panel),-1);
    lv_event_send(shade,LV_EVENT_PRESSED,NULL);lv_event_send(shade,LV_EVENT_RELEASED,NULL);assert(!panel_choice_is_open());
    action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);assert(panel_entry_is_open());panel_main_set_visible(false);assert(!panel_entry_is_open());
    panel_main_set_visible(true);assert_regions();
    const language_t languages[]={LANGUAGE_CN,LANGUAGE_KR,LANGUAGE_JA,LANGUAGE_AR};
    const char *names[]={"main-zh","main-ko","main-ja","main-ar"};
    for(unsigned i=0;i<4;i++){
        assert(ui_locale_font_prepare(languages[i]));
        lv_obj_t *existing=panel_main_root();ui_lang_set(languages[i]);panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);
        assert(panel_main_root()==existing);
        assert(lv_obj_get_style_text_font(main_view.amount_title,0)==ui_panel_font(13,false));
        assert(find_text(panel_main_root(),ui_trc("Panel key","Batch")));
        advance(100);assert_regions();capture(names[i]);
        if(i==0){
            action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);press_text(lv_layer_top(),"1");press_text(lv_layer_top(),"9");capture("numeric-zh");
            panel_entry_close_for(&main_view);pin.title=ui_tr("Password");pin.prompt=ui_tr("Enter your 4-digit password.");pin.field_label=ui_tr("Password");
            assert(panel_entry_open(lv_layer_top(),&pin));press_text(lv_layer_top(),"7");capture("password-zh");panel_entry_close_for(&owner);
        }
    }
    for(size_t i=0;i<ui_lang_count();i++){
        const ui_locale_t *locale=ui_lang_at(i);if(locale->id<=LANGUAGE_AR)continue;
        assert(ui_locale_font_prepare(locale->id));ui_lang_set(locale->id);panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);
        char name[64];snprintf(name,sizeof(name),"main-%s",locale->tag);advance(80);assert_regions();capture(name);
    }
    for(size_t i=0;i<ui_lang_count();i++){
        const ui_locale_t *locale=ui_lang_at(i);assert(ui_locale_font_prepare(locale->id));ui_lang_set(locale->id);
        panel_main_refresh(PAGE_01_MAIN_DIRTY_ALL);char name[64];
        action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);assert(panel_entry_is_open_for(&main_view));
        snprintf(name,sizeof(name),"numeric-%s",locale->tag);capture(name);panel_entry_close_for(&main_view);
        pin.title=ui_tr("Password");pin.prompt=ui_tr("Enter your 4-digit password.");pin.field_label=ui_tr("Password");
        assert(panel_entry_open(lv_layer_top(),&pin));snprintf(name,sizeof(name),"password-%s",locale->tag);capture(name);panel_entry_close_for(&owner);
    }
    main_view.section=PAGE_01_DETAIL_SECTION_C;action(NULL,A_LIST,UI_PANEL_EVENT_CLICK);
    assert(list_currency==PAGE_02_MULTI_REJECTS&&list_tab==PAGE_02_MULTI_TAB_VALUE);
    main_view.section=PAGE_01_DETAIL_SECTION_A;lv_event_send(main_view.reject_text,LV_EVENT_CLICKED,NULL);
    assert(list_currency==PAGE_02_MULTI_REJECTS&&list_tab==PAGE_02_MULTI_TAB_VALUE);
    panel_main_destroy();panel_entry_destroy();counting_data_clear_serials(counting_data_mutable());
    for(unsigned i=0;i<30;i++){
        assert(panel_main_create(lv_scr_act()));panel_main_set_visible(true);action(NULL,A_BATCH,UI_PANEL_EVENT_CLICK);
        panel_main_destroy();assert(!panel_entry_is_open());advance(20);
    }
    assert(!key_overflows);
    printf("PASS: Main fixed regions, real totals, pending/confirmed transport, request/ACK boundary, busy/profile guards, failure retention, numeric/PIN owner lifecycle, fault routes, locales, 30 rebuilds; Speed text-only, real ACK levels, touch path and held feedback across 12 live data refreshes.\n");
    return 0;
}
