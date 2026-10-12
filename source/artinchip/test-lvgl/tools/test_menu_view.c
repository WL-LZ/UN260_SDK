#include "un260/font/ui_locale_font_bank.h"
#include "test_notice_sink.h"
#include <assert.h>
#include "un260/lv_components/lv_content_pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lvgl/lvgl.h"
#include "aic_ui/compiled_asset.h"
#include "un260/lv_core/page_03_menu.c"
#include "un260/app_service/work_mode_service.h"
#include "un260/app_service/app_auto_qr.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/lv_modal_dialog.h"
/* UI ownership is tested with real app_ui_runtime functions separately. These
 * synchronous storage fixtures cannot model accepted asynchronous jobs. */
static unsigned operation_starts;
static app_ui_notice_operation_t operation_started;
void app_ui_runtime_notice_started(app_ui_notice_operation_t operation,const char *detail)
{(void)detail;operation_starts++;operation_started=operation;}
static workspace_model_t model;
static cashbook_t ledger;
const cashbook_t *cashbook_store_view(void){return &ledger;}
bool cashbook_store_view_archived(void){return false;}
void cashbook_store_close_view(void){}
unsigned cashbook_store_archives(const cashbook_archive_t **items){*items=NULL;return 0;}
bool cashbook_store_scan_archives(void){return true;}
bool cashbook_store_open_archive(uint32_t id){(void)id;return false;}
bool cashbook_store_archive(void){return false;}
bool cashbook_store_export(void){return true;}
bool counting_history_is_idle(void){return true;}
bool counting_history_can_archive(void){return true;}
uint32_t counting_cashbook_verify_group(void){return 0;}
void counting_cashbook_cancel_verify(void){}
static standby_config_t standby_fixture={.version=2,.minutes=5};
static int backlight=75;
static counting_sim_t count_fixture={.total_pcs=85,.total_amount=425};
static print_config_value_t print_fixture={.content=1};
const counting_sim_t *counting_data_current(void){return &count_fixture;}
int counting_data_error_detail_count(const counting_sim_t *s){(void)s;return 0;}
bool counting_data_monetary_result_supported(const counting_sim_t *s){(void)s;return true;}
void print_config_get(print_config_value_t *v){*v=print_fixture;}
bool print_config_pending(void){return false;}
const print_config_request_result_t *print_config_last_result(void){static print_config_request_result_t r={.success=true};return &r;}
bool print_config_request_field(print_config_field_t f,const print_config_value_t *v){(void)f;print_fixture=*v;return true;}
bool workspace_store_export_support(const char *r){return r&&*r;}
bool device_info_is_valid(void){return true;}
const char *device_info_fpga(void){return "1.2";}
const char *device_info_main_boot(void){return "1.0";}
const char *device_info_image_boot(void){return "1.1";}
const cashbook_t *cashbook_store_get(void){return &ledger;}
bool cashbook_store_ready(void){return true;}
bool cashbook_store_busy(void){return false;}
static bool record_success=true;
bool cashbook_store_last_success(void){return record_success;}
const char *cashbook_store_message(void){return "Records ready.";}
bool cashbook_store_submit(const cashbook_command_t *c){char reason[160];return cashbook_apply(&ledger,c,reason,sizeof(reason));}
bool counting_cashbook_arm_verify(uint32_t g){return g>0;}
bool backlight_service_probe(void){return true;}
int backlight_service_level(void){return backlight;}
int backlight_service_max(void){return 100;}
bool backlight_service_set(int level){backlight=level;return true;}
bool backlight_service_save(void){return true;}
const standby_config_t *standby_config(void){return &standby_fixture;}
bool standby_store_save(const standby_config_t *v){standby_fixture=*v;return true;}
void machine_time_get(machine_time_value_t *v){*v=(machine_time_value_t){2026,9,23,10,42,0};}
void ui_page_19_history_open_record(uint32_t n){assert(n);}
void ui_page_19_history_open_all(void){}
void ui_page_19_history_open_operator(uint32_t id){assert(id);}
uint8_t machine_state_mode(void){return MODE_MDC;}
uint8_t machine_state_reject_pocket_max(void){return 100;}
static unsigned mode_requests;
bool setting_service_request_mode(uint8_t n){(void)n;mode_requests++;return true;}
bool setting_service_request_reject_pocket_max(uint8_t n,uint8_t p){(void)n;(void)p;return true;}
static ui_history_store_t history_fixture;
static unsigned revision,nav_count,printed;
static ui_page_t current=UI_PAGE_MENU;
static bool busy,gestures=true,layout=true;
static bool popup_auto=true,pure_count,touch_guide=true;
static bool top_popup_save_fails,local_save_fails;
static ui_main_batch_action_t main_batch_action=UI_MAIN_BATCH_SETTINGS;
ui_main_batch_action_t ui_state_main_batch_action_get(void){return main_batch_action;}
bool ui_state_main_batch_action_set(ui_main_batch_action_t value)
{if(local_save_fails||value>UI_MAIN_BATCH_CYCLE)return false;main_batch_action=value;return true;}
bool ui_state_top_popup_set_enabled(bool enabled)
{if(top_popup_save_fails||local_save_fails)return false;ui_notice_set_enabled(enabled);return true;}
static uint8_t quick_order[QUICK_CONTROL_COUNT]={0,1,2,3,4,5,6,7,8,9,10,11,12};
static gesture_pointer_policy_t quick_pointer_policy;
static bool quick_navigation_blocked;
void gesture_service_set_pointer_policy(uint32_t owner,gesture_pointer_policy_t cb)
{assert(owner==UI_PAGE_MENU);quick_pointer_policy=cb;}
void gesture_service_clear_pointer_policy(uint32_t owner)
{assert(owner==UI_PAGE_MENU);quick_pointer_policy=NULL;}
void gesture_service_set_navigation_blocked(uint32_t owner,bool blocked)
{assert(owner==UI_PAGE_MENU);quick_navigation_blocked=blocked;}
#if !defined(CASH_REAL_FONTS) || defined(MENU_REAL_FONTS)
void ui_cash_journal_filter_operator(uint32_t id){(void)id;}
#endif
#ifndef CASH_REAL_FONTS
const lv_font_t *ui_message_font(const lv_font_t *font){return font;}
#endif
static uint16_t quick_visible=UI_STATE_QUICK_VISIBLE_ALL;
uint16_t ui_state_quick_visible_get(void){return quick_visible;}
bool ui_state_quick_visible_set(uint8_t id,bool visible){if(local_save_fails||id>=QUICK_CONTROL_COUNT)return false;if(visible)quick_visible|=1U<<id;else quick_visible&=~(1U<<id);return true;}
void ui_state_quick_order_get(uint8_t order[QUICK_CONTROL_COUNT]){memcpy(order,quick_order,sizeof(quick_order));}
bool fault_popup_get_auto_enabled(void){return popup_auto;}
void fault_popup_set_auto_enabled(bool value){popup_auto=value;}
void ui_state_save_popup_auto_state(void){}
bool ui_state_popup_auto_set_enabled(bool enabled){if(local_save_fails)return false;popup_auto=enabled;return true;}
bool app_setting_notice_page_allowed(void){return current==UI_PAGE_MENU;}
bool smart_island_pure_count_is_enabled(void){return pure_count;}
void smart_island_set_pure_count_enabled(bool value){pure_count=value;}
void ui_state_save_pure_count_state(void){}
bool touch_feedback_enabled(void){return touch_guide;}
bool touch_feedback_set_enabled(bool value){if(local_save_fails)return false;touch_guide=value;return true;}
bool ui_export_data_request(void){return true;}
void smart_island_show_qr_popup(void){}
bool ui_page_18_pure_toggle_theme(void){pure_count=current!=UI_PAGE_PURE;ui_manager_switch(pure_count?UI_PAGE_PURE:UI_PAGE_MAIN);return true;}
const char *app_standby_runtime_blocker(void){return busy?"Finish counting before standby.":NULL;}
bool app_standby_runtime_needs_fault_review(void){return false;}
bool app_standby_runtime_request(void){if(app_standby_runtime_blocker())return false;ui_manager_push_page(UI_PAGE_STANDBY);return true;}
static bool auto_qr_blocked,auto_qr_waiting;
bool ui_manager_is_transitioning(void){return false;}
bool app_command_runtime_result_pending(void){return auto_qr_waiting;}
bool fault_popup_is_showing(void){return auto_qr_blocked;}
bool fault_popup_get_pending_fault(fault_source_t *s,uint8_t *t,uint8_t *c){(void)s;(void)t;(void)c;return false;}
bool smart_island_is_expanded(void){return false;}
bool page_01_main_quick_is_open(void){return false;}
static machine_state_snapshot_t actual_state={.batch_enabled=true,.batch_num=50,.speed=1,.buzzer_enabled=true};
const workspace_model_t *workspace_store_get(void){return &model;}
uint32_t workspace_store_revision(void){return revision;}
bool workspace_store_ready(void){return true;}
bool workspace_store_busy(void){return busy;}
bool workspace_store_last_success(void){return true;}
const char *workspace_store_message(void){return "Saved on this device.";}
bool workspace_store_save(const workspace_model_t *m){if(busy||!workspace_model_valid(m))return false;model=*m;revision++;return true;}
static uint8_t batch_edit_previous,batch_edit_target;
static unsigned batch_edit_saves;
bool workspace_service_save_batches(uint32_t owner,const uint8_t *values,unsigned count,uint8_t previous,uint8_t target){
    workspace_model_t next=model;workspace_user_t *u=workspace_find(&next,owner);if(!u)return false;
    u->batch_count=count;memset(u->batches,0,sizeof(u->batches));memcpy(u->batches,values,count);
    if(!workspace_store_save(&next))return false;
    batch_edit_previous=previous;batch_edit_target=target;batch_edit_saves++;return true;
}
const char *workspace_service_batch_save_message(void){return "Batch cycle saved.";}
bool workspace_store_scan_usb(void){return true;}
uint32_t workspace_store_usb_images(void){return 0;}
bool workspace_store_import_avatar(unsigned i){(void)i;return false;}
const workspace_avatar_t *workspace_store_avatar(void){static workspace_avatar_t a;return &a;}
bool workspace_service_applying(void){return false;}
void workspace_service_cancel_apply(void){}
const char *workspace_service_apply_message(void){return "";}
workspace_apply_result_t workspace_service_apply_result(void){return WORKSPACE_APPLY_IDLE;}
bool workspace_service_quick_enabled(void){return workspace_active(&model)->quick_enabled;}
const char *workspace_service_switch_blocker(void){return busy?"Busy":NULL;}
bool workspace_service_switch(uint32_t id){if(busy)return false;model.active_id=id;revision++;return true;}
bool workspace_service_apply(const workspace_profile_t *p,uint32_t n){(void)p;(void)n;return false;}
uint8_t machine_state_speed(void){return actual_state.speed;}
uint8_t machine_state_work_mode(void){return actual_state.work_mode;}
uint8_t machine_state_fo_mode(void){return actual_state.fo_mode;}
bool machine_state_add_enabled(void){return actual_state.add_enabled;}
bool machine_state_buzzer_enabled(void){return actual_state.buzzer_enabled;}
bool machine_state_batch_enabled(void){return actual_state.batch_enabled;}
uint8_t machine_state_batch_num(void){return actual_state.batch_num;}
bool setting_service_request_speed(uint8_t n){actual_state.speed=n;return true;}
bool setting_service_request_work_mode(uint8_t n){actual_state.work_mode=n;return true;}
bool setting_service_request_fo_mode(uint8_t n){actual_state.fo_mode=n;return true;}
bool setting_service_request_add(bool n){actual_state.add_enabled=n;return true;}
bool setting_service_request_beep(bool n){actual_state.buzzer_enabled=n;return true;}
bool setting_service_request_batch_switch(bool enabled,uint8_t n,bool e,uint8_t old){(void)e;(void)old;actual_state.batch_enabled=enabled;actual_state.batch_num=n;return true;}
bool app_command_runtime_count_start_busy(void){return busy;}
bool standby_store_busy(void){return false;}
bool gesture_service_enabled(void){return gestures;}
bool gesture_service_set_enabled(bool e){if(local_save_fails)return false;gestures=e;return true;}
bool page_01_main_layout_is_enabled(void){return layout;}
void page_01_main_layout_set_enabled(bool e){layout=e;}
bool ui_manager_pop_page(void){nav_count++;current=UI_PAGE_MAIN;return true;}
void ui_manager_push_page(ui_page_t page){nav_count++;current=page;}
void ui_manager_switch(ui_page_t page){nav_count++;current=page;}
ui_page_t ui_manager_get_current_page(void){return current;}
void ui_page_05_set_password_open(void){nav_count++;}
void page_01_print_btn_event_cb(lv_event_t *e){(void)e;printed++;}
bool ui_qr_data_build_summary(char *b,size_t n){snprintf(b,n,"UN260 SUMMARY fixture");return true;}
const ui_history_store_t *ui_history_data_get(void){return &history_fixture;}
void currency_state_get_selected_code(char c[4]){memcpy(c,"CNY",4);}
void currency_state_get_effective_code(char c[4]){memcpy(c,"CNY",4);}
const char *currency_state_display_code(const char c[4]){return c;}
const char *device_info_main_app(void){return "1.0";}
const char *device_info_image_app(void){return "1.0";}
const char *device_info_display_app(void){return "1.0";}
bool protocol_send_is_ready(void){return false;}
int protocol_send(uint8_t c,const uint8_t *p,uint16_t n){(void)c;(void)p;(void)n;return -1;}
void page_06_settings_set_status(const char *s,lv_color_t c){(void)s;(void)c;}
void work_mode_service_retry(void){}
void work_mode_service_get_snapshot(work_mode_snapshot_t *s){memset(s,0,sizeof(*s));}
const char *work_mode_service_status_text(void){return "Host test";}
const char *app_command_runtime_diagnostic_run_blocker(void){return "Host test";}
bool app_command_runtime_request_diagnostic_run(void){return false;}
void perf_profile_watch_invalidation(const void *p,const char *n){(void)p;(void)n;}
void perf_profile_unwatch_invalidation(const void *p){(void)p;}
void uart_debug_printf(const char *s,...){(void)s;}
bool user_cfg_touch_feedback_enabled(void){return true;}
static lv_color_t framebuffer[1280*400],draw_buffer[1280*64];
static lv_point_t point;static lv_indev_state_t down=LV_INDEV_STATE_RELEASED;
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
uint64_t app_clock_monotonic_us(void){return (uint64_t)lv_tick_get()*1000;}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p)
{assert(a->x1>=0&&a->x2<1280&&a->y1>=0&&a->y2<400);for(int y=a->y1;y<=a->y2;y++)memcpy(framebuffer+y*1280+a->x1,p+(y-a->y1)*lv_area_get_width(a),lv_area_get_width(a)*4);lv_disp_flush_ready(d);}
static void read_pointer(lv_indev_drv_t *d,lv_indev_data_t *p)
{
    (void)d;static bool contact_down,captured;p->point=point;p->state=down;
    bool pressed=down==LV_INDEV_STATE_PRESSED;
    lv_event_code_t event=pressed?(contact_down?LV_EVENT_PRESSING:LV_EVENT_PRESSED):LV_EVENT_RELEASED;
    bool owned=quick_pointer_policy && quick_pointer_policy(lv_indev_get_next(NULL),event,&point,pressed?1:0);
    if(owned)captured=true;
    if(captured)p->state=LV_INDEV_STATE_RELEASED;
    if(!pressed)captured=false;
    contact_down=pressed;
}
static void tick(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
static void menu_quick_flip(bool next)
{
    bool top=ui_notice_get_enabled(),popup=popup_auto;
    uint8_t target=lv_quick_controls_page(&menu.quick_controls)^1U;
    point=(lv_point_t){next?1050:150,110};down=LV_INDEV_STATE_PRESSED;tick(40);
    point=(lv_point_t){next?550:850,110};tick(40);down=LV_INDEV_STATE_RELEASED;tick(40);
    assert(lv_content_pager_is_moving(menu.quick_controls.pager));tick(320);
    assert(menu.quick && lv_quick_controls_page(&menu.quick_controls)==target);
    assert(ui_notice_get_enabled()==top&&popup_auto==popup&&quick_navigation_blocked);
}
static lv_obj_t *find(lv_obj_t *p,const char *s)
{
    if(!p||!lv_obj_is_visible(p))return NULL;
    for(unsigned i=lv_obj_get_child_cnt(p);i>0;i--){lv_obj_t *r=find(lv_obj_get_child(p,i-1),s);if(r)return r;}
    if(lv_obj_check_type(p,&lv_label_class)&&!strcmp(lv_label_get_text(p),s))return lv_obj_get_parent(p);return NULL;
}
static void click(const char *s)
{
    ui_notice_dismiss(NULL);
    lv_obj_update_layout(lv_scr_act());lv_obj_t *o=find(lv_scr_act(),s);if(!o)fprintf(stderr,"Missing button: %s\n",s);assert(o);
    lv_area_t a;lv_obj_get_coords(o,&a);fprintf(stderr,"click %s @ %d,%d-%d,%d\n",s,a.x1,a.y1,a.x2,a.y2);assert(a.x1>=0&&a.x2<1280&&a.y1>=0&&a.y2<400);
    point=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(180);
}

static void check_header(void)
{
    assert(!menu.nav[0]&&!menu.nav[2]);
    lv_obj_t *tray=lv_obj_get_parent(menu.nav[1]);lv_area_t t;
    lv_obj_get_coords(tray,&t);
    assert(lv_obj_get_child_cnt(tray)==4);
    /* Arabic labels are shaped; identify the adjacent header control by tree. */
    lv_obj_t *quick=lv_obj_get_child(menu.root,lv_obj_get_index(tray)+1);assert(quick);
    lv_area_t q;lv_obj_get_coords(quick,&q);
    lv_obj_t *title=lv_obj_get_child(menu.root,1);lv_area_t title_bounds;
    lv_obj_get_coords(title,&title_bounds);
    assert(t.x1>=title_bounds.x2+24&&t.x2+16<=q.x1);
    assert(lv_obj_get_style_border_width(tray,0)==0);
    assert(lv_obj_get_style_bg_color(tray,0).full==lv_color_hex(PANEL).full);
    for(unsigned n=0;n<4;n++){
        unsigned i=visible_tabs[n];
        lv_obj_t *v=menu.nav[i],*l=lv_obj_get_child(v,0),*im=lv_obj_get_child(v,1);
        lv_area_t b,a,c;lv_obj_get_coords(v,&b);lv_obj_get_coords(im,&a);lv_obj_get_coords(l,&c);
        assert(abs((a.x1-b.x1)-(b.x2-c.x2))<=1);
        assert(c.x1-a.x2-1==8);
        assert(abs((a.y1+a.y2)-(b.y1+b.y2))<=1);
        assert(abs((c.y1+c.y2)-(b.y1+b.y2))<=1);
        assert(lv_obj_get_style_border_width(v,0)==0&&lv_obj_get_style_shadow_width(v,0)==0);
        assert(lv_obj_get_style_bg_color(v,0).full==lv_color_hex(menu.tab==i?0xFFFFFF:PANEL).full);
        assert(lv_obj_get_height(v)==40);
        int width=lv_txt_get_width(lv_label_get_text(l),strlen(lv_label_get_text(l)),lv_obj_get_style_text_font(l,0),0,LV_TEXT_FLAG_NONE);
        assert(width==lv_obj_get_width(l)&&lv_obj_get_height(l)<=lv_obj_get_height(v));
    }
    lv_area_t first,last;lv_obj_get_coords(menu.nav[1],&first);lv_obj_get_coords(menu.nav[5],&last);
    assert(first.x1-t.x1==t.x2-last.x2&&first.x1-t.x1==4);
    lv_obj_t *back=lv_obj_get_child(menu.root,0);
    assert(lv_obj_get_width(back)==76&&lv_obj_get_height(back)==42);
    assert(lv_obj_get_child_cnt(back)==1);
    assert(lv_obj_get_style_border_width(back,0)==0);
    for(unsigned i=0;i<lv_obj_get_child_cnt(menu.root);i++){
        lv_obj_t *v=lv_obj_get_child(menu.root,i);
        if(!lv_obj_has_flag(v,LV_OBJ_FLAG_CLICKABLE)||lv_obj_get_y(v)!=14)continue;
        assert(lv_obj_get_height(v)==44&&lv_obj_get_style_border_width(v,0)==0);
    }
}
static void check_interaction_header(lv_obj_t *content)
{
    lv_obj_t *title=lv_obj_get_child(content,0),*tray=lv_obj_get_child(content,1);
    lv_obj_t *controls=lv_obj_get_child(tray,0),*visibility=lv_obj_get_child(tray,1);
    assert(lv_obj_get_child_cnt(tray)==2&&lv_obj_get_height(tray)==48);
    assert(lv_obj_get_style_bg_color(tray,0).full==lv_color_hex(PANEL).full);
    assert(lv_obj_get_style_border_width(tray,0)==0);
    assert(lv_obj_get_x(title)+lv_obj_get_width(title)+20<=lv_obj_get_x(tray));
    assert(lv_obj_get_x(controls)==4&&lv_obj_get_y(controls)==4);
    assert(lv_obj_get_x(visibility)==lv_obj_get_width(controls)+6&&lv_obj_get_y(visibility)==4);
    assert(lv_obj_get_x(visibility)+lv_obj_get_width(visibility)+4==lv_obj_get_width(tray));
    for(unsigned i=0;i<2;i++){
        lv_obj_t *v=lv_obj_get_child(tray,i);
        bool selected=i==menu.shortcut_visibility;
        assert(lv_obj_get_height(v)==40&&lv_obj_get_style_border_width(v,0)==0);
        assert(lv_obj_has_state(v,LV_STATE_CHECKED)==selected);
        assert(lv_obj_get_style_bg_color(v,0).full==lv_color_hex(selected?0xFFFFFF:PANEL).full);
        lv_obj_t *caption=lv_obj_get_child(v,0);
        int natural=lv_txt_get_width(lv_label_get_text(caption),strlen(lv_label_get_text(caption)),lv_obj_get_style_text_font(caption,0),0,LV_TEXT_FLAG_NONE);
        assert(natural<=lv_obj_get_width(caption)&&lv_obj_get_height(caption)<=40);
    }
    if(menu.interaction_created){
        assert(lv_obj_get_x(tray)+lv_obj_get_width(tray)+12<=lv_obj_get_x(menu.interaction_controls.device_button));
    }
}
static void check_numeric_keypad(void)
{
    lv_obj_update_layout(lv_scr_act());
    lv_obj_t *one=find(lv_scr_act(),"1"),*two=find(lv_scr_act(),"2");
    lv_obj_t *four=find(lv_scr_act(),"4"),*clear=find(lv_scr_act(),"Clear");
    assert(one&&two&&four&&clear&&!find(lv_scr_act(),"Q")&&!find(lv_scr_act(),"."));
    assert(lv_obj_get_width(one)==107&&lv_obj_get_height(one)==65);
    assert(lv_obj_get_x(two)-lv_obj_get_x(one)==115);
    assert(lv_obj_get_y(four)-lv_obj_get_y(one)==73);
    assert(lv_obj_get_style_bg_color(one,0).full==lv_color_hex(0xF0F3F5).full);
    assert(lv_obj_get_style_border_width(one,0)==0&&lv_obj_get_style_radius(one,0)==12);
    assert(lv_obj_get_style_text_font(lv_obj_get_child(one,0),0)==ui_message_font(&lv_font_instrument_sans_medium_26));
    assert(lv_obj_get_style_bg_color(clear,0).full==lv_color_hex(0xFBFCFD).full);
    assert(lv_obj_get_style_border_width(clear,0)==0);
}
static void raster(const char *name)
{
    if(page_03_menu_is_visible()){
        lv_obj_update_layout(menu.root);check_header();
        if(menu.tab==4&&menu.sub==1)check_interaction_header(lv_obj_get_child(menu.body,1));
    }
    lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);
    const char *dir=getenv("MENU_OUTPUT");if(!dir)return;char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",dir,name);
    FILE *f=fopen(path,"wb");assert(f);assert(fwrite(framebuffer,1,sizeof(framebuffer),f)==sizeof(framebuffer));fclose(f);
}
static lv_res_t asset_info(lv_img_decoder_t *decoder,const void *src,lv_img_header_t *h)
{
    (void)decoder;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;
    const un260_compiled_asset_t *a=un260_compiled_asset_find(src);if(!a){fprintf(stderr,"Missing image: %s\n",(const char *)src);abort();}
    memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;
}
static lv_res_t asset_open(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *d)
{if(asset_info(decoder,d->src,&d->header)!=LV_RES_OK)return LV_RES_INV;d->img_data=un260_compiled_asset_find(d->src)->pixels;return LV_RES_OK;}
static void asset_close(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *d){(void)decoder;(void)d;}
static void seed_ledger(void)
{
    cashbook_command_t c={.operation=CASHBOOK_INGEST,.result={.source=1,.day=20260923,.hour=9,.minute=10,.operator_id=1,.operator_name="Alex Morgan",.pcs=100,.currencies=1,.complete=true,
        .money={{.code="CNY",.pcs=100,.amount=10000}},.sample_count=4,.samples={11,22,33,44}}};
    assert(cashbook_store_submit(&c));
    cashbook_command_t confirm={.operation=CASHBOOK_CONFIRM,.group=1,.run=1};assert(cashbook_store_submit(&confirm));
    c.result.source=2;c.result.minute=15;c.result.recount_group=1;assert(cashbook_store_submit(&c));
    c.result.source=3;c.result.minute=20;assert(cashbook_store_submit(&c));
    confirm.run=3;assert(cashbook_store_submit(&confirm));
    c.result.source=4;c.result.recount_group=0;c.result.sample_count=0;c.result.pcs=12;c.result.money[0]=(cashbook_money_t){.code="USD",.pcs=12,.amount=240};assert(cashbook_store_submit(&c));
    confirm.group=2;confirm.run=4;assert(cashbook_store_submit(&confirm));
    c.result.source=5;c.result.pcs=85;c.result.money[0]=(cashbook_money_t){.code="CNY",.pcs=85,.amount=425};assert(cashbook_store_submit(&c));
}
static void test_auto_qr(void)
{
    current=UI_PAGE_MAIN;model.users[0].qr_after_count=true;model.active_id=1;
    app_auto_qr_on_start();app_auto_qr_on_end(1000);app_auto_qr_poll(1300);assert(!lv_qr_popup_is_showing());
    auto_qr_waiting=true;app_auto_qr_poll(1400);assert(!lv_qr_popup_is_showing());auto_qr_waiting=false;
    auto_qr_blocked=true;app_auto_qr_poll(1500);assert(!lv_qr_popup_is_showing());auto_qr_blocked=false;
    app_auto_qr_poll(1600);assert(lv_qr_popup_is_showing());raster("auto-qr");
    app_auto_qr_on_start();assert(!lv_qr_popup_is_showing());
    app_auto_qr_on_end(2000);app_auto_qr_poll(2400);assert(lv_qr_popup_is_showing());
    lv_qr_popup_hide();assert(lv_qr_popup_show("MANUAL RESULT"));app_auto_qr_on_start();assert(lv_qr_popup_is_showing());
    lv_qr_popup_hide();app_auto_qr_on_end(3000);current=UI_PAGE_MENU;app_auto_qr_poll(3400);current=UI_PAGE_MAIN;app_auto_qr_poll(3500);assert(!lv_qr_popup_is_showing());
    app_auto_qr_on_start();app_auto_qr_on_end(4000);app_auto_qr_poll(8001);assert(!lv_qr_popup_is_showing());
    app_auto_qr_cancel();puts("PASS auto QR: delay, pending result, fault, new count, manual ownership, page change and expiry");
}
static void test_local_feedback_contract(void)
{
    bool old_g=gestures,old_l=layout,old_p=popup_auto,old_t=touch_guide;
    ui_main_batch_action_t old_batch=main_batch_action;
    uint16_t old_visible=quick_visible;
    uint8_t old_order[QUICK_CONTROL_COUNT];memcpy(old_order,quick_order,sizeof(old_order));
    for(unsigned id=0;id<APP_LOCAL_SETTING_COUNT;id++){
        ui_notice_set_enabled(true);local_save_fails=false;
        unsigned before=test_notice_count;
        assert(app_local_setting_apply(id,1,0));
        assert(test_notice_count==before+1 && test_notice_kind==UI_NOTICE_SUCCESS);
        assert(*test_notice_key && *test_notice_title);
        if(id==APP_LOCAL_LAYOUT)continue; /* Runtime-only, not a disk transaction. */
        local_save_fails=true;before=test_notice_count;
        assert(!app_local_setting_apply(id,0,1));
        assert(test_notice_count==before+1 && test_notice_kind==UI_NOTICE_ERROR);
        local_save_fails=false;
    }
    unsigned before=test_notice_count;
    assert(!app_local_setting_apply(APP_LOCAL_SETTING_COUNT,0,0));assert(test_notice_count==before);
    assert(app_local_setting_apply(APP_LOCAL_TOP_POPUP,0,0));assert(test_notice_count==before);
    assert(app_local_setting_apply(APP_LOCAL_GESTURES,0,0));assert(test_notice_count==before);
    assert(app_local_setting_apply(APP_LOCAL_TOP_POPUP,1,0));assert(test_notice_count==before+1);
    before=test_notice_count;current=UI_PAGE_MAIN;
    assert(app_local_setting_apply(APP_LOCAL_GESTURES,1,0));assert(test_notice_count==before);
    current=UI_PAGE_MENU;gestures=old_g;layout=old_l;popup_auto=old_p;touch_guide=old_t;main_batch_action=old_batch;
    memcpy(quick_order,old_order,sizeof(old_order));quick_visible=old_visible;ui_notice_dismiss(NULL);
    puts("PASS all registered local settings: one success/error; disabled popup, invalid ID and Main origin quiet; no replay");
}
static void check_keyboard_labels(lv_obj_t *object)
{
    if(!lv_obj_is_visible(object))return;
    if(lv_obj_check_type(object,&lv_label_class)){
        const char *text=lv_label_get_text(object);
        const lv_font_t *font=lv_obj_get_style_text_font(object,0);
        uint32_t offset=0;
        while(text[offset]){
            uint32_t cp=_lv_txt_encoded_next(text,&offset);lv_font_glyph_dsc_t glyph;
            if(cp<32||(cp>=0x200b&&cp<=0x200f)||(cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069))continue;
            assert(lv_font_get_glyph_dsc(font,&glyph,cp,0)&&!glyph.is_placeholder);
        }
        lv_obj_t *parent=lv_obj_get_parent(object);
        if(lv_obj_check_type(parent,&lv_btn_class)||
           (lv_obj_has_flag(parent,LV_OBJ_FLAG_CLICKABLE)&&lv_obj_get_style_radius(parent,0)==12)){
            lv_point_t size;lv_txt_get_size(&size,text,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
            if(size.x>lv_obj_get_width(parent)-12||size.y>lv_obj_get_height(parent))
                fprintf(stderr,"Keyboard action %s: %s %d,%d in %d,%d\\n",ui_lang_current()->tag,text,size.x,size.y,lv_obj_get_width(parent),lv_obj_get_height(parent));
            assert(size.x<=lv_obj_get_width(parent)-12&&size.y<=lv_obj_get_height(parent));
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_cnt(object);++i)check_keyboard_labels(lv_obj_get_child(object,i));
}
#include "test_alnum_keyboard_cases.h"
int main(void)
{
    test_local_feedback_contract();
    cashbook_defaults(&ledger);workspace_defaults(&model);model.operators_enabled=1;strcpy(model.users[0].name,"Alex Morgan");
    seed_ledger();char report[8192];assert(support_report_build(report,sizeof(report)));
    assert(strstr(report,"Confirmed settings")&&!strstr(report,"Alex Morgan")&&!strstr(report,"10000"));assert(!support_report_build(report,8));
    workspace_profile_t p=model.users[0].profiles[0];strcpy(p.name,"Bundle preparation");p.batch_enabled=1;p.batch=100;assert(workspace_add_profile(&model,1,&p));
    uint32_t uid;assert(workspace_add_user(&model,"Taylor",&uid));
    history_fixture.record_count=1;history_fixture.records[0]=(ui_history_record_t){.valid=true,.record_no=1,.pcs=85,.amount=425,.currency="CNY",.year=2026,.month=9,.day=23,.hour=10,.minute=42};
    lv_init();
#ifdef MENU_REAL_FONTS
    ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
#endif
    lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_buffer,NULL,1280*64);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1280;d.ver_res=400;d.flush_cb=flush;d.draw_buf=&buf;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);ui_scrollbar_init(display);
    lv_indev_drv_t in;lv_indev_drv_init(&in);in.type=LV_INDEV_TYPE_POINTER;in.read_cb=read_pointer;assert(lv_indev_drv_register(&in));
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,asset_info);lv_img_decoder_set_open_cb(decoder,asset_open);lv_img_decoder_set_close_cb(decoder,asset_close);
    ui_page_03_menu_create(lv_scr_act());tick(200);
    assert(menu.tab==1&&!find(menu.root,"Overview")&&!find(menu.root,"Records"));
    raster("menu-default-count");assert(!test_notice_visible);
    assert(main_batch_action==UI_MAIN_BATCH_SETTINGS);
    click("Main Batch: Settings");assert(main_batch_action==UI_MAIN_BATCH_CYCLE);
    raster("menu-main-batch-cycle");
    click("Main Batch: Cycle");assert(main_batch_action==UI_MAIN_BATCH_SETTINGS);
    menu.batch_dirty=true;menu.saving=1;
    click("Main Batch: Settings");assert(main_batch_action==UI_MAIN_BATCH_CYCLE&&menu.batch_dirty&&menu.saving==1);
    menu.batch_dirty=false;menu.saving=0;
    click("Main Batch: Cycle");assert(main_batch_action==UI_MAIN_BATCH_SETTINGS);
    ui_notice_set_enabled(false);local_save_fails=true;
    click("Main Batch: Settings");assert(main_batch_action==UI_MAIN_BATCH_SETTINGS&&settings_detail_overlay_is_open());
    click("OK");local_save_fails=false;ui_notice_set_enabled(true);
    /* An explicit Main Batch entry chooses Count/Presets on the existing Menu. */
    menu.tab=5;menu.sub=1;menu.dirty=true;tick(120);
    page_03_menu_open_batch();tick(120);assert(menu.tab==1&&menu.sub==0&&find(menu.body,"Batch presets"));
    click("Quick");assert(menu.quick);click("Close");assert(!menu.quick);
    unsigned nav_before=nav_count;point=(lv_point_t){1233,37};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(180);assert(nav_count==nav_before+1);
    nav_before=nav_count;point=(lv_point_t){47,37};down=LV_INDEV_STATE_PRESSED;tick(60);down=LV_INDEV_STATE_RELEASED;tick(180);assert(nav_count==nav_before+1&&current==UI_PAGE_MAIN);current=UI_PAGE_MENU;
    nav_before=nav_count;assert(lv_nav_button_request_back()==LV_NAV_BACK_HANDLED);assert(nav_count==nav_before+1);current=UI_PAGE_MENU;
    click("Count");assert(menu.tab==1);assert(find(menu.body,"Edit")&&find(menu.body,"Remove"));raster("menu-batch");click("Add slot");assert(menu.batch_dirty);assert(!lv_obj_has_state(find(menu.body,"Use this preset"),LV_STATE_DISABLED));assert(find(menu.body,"Save"));click("Cancel");assert(!menu.batch_dirty);
    menu.selected_batch=1;menu.dirty=true;tick(120);click("Edit");assert(settings_detail_overlay_is_open());check_numeric_keypad();raster("menu-batch-keypad");settings_detail_keyboard_hide();
    ui_page_03_menu_suspend();actual_state.batch_num=10;assert(ui_page_03_menu_resume());assert(menu.selected_batch==1);
    batch_submit("13",NULL);assert(menu.saving==4&&menu.batch_apply_target==13);
    assert(batch_edit_saves==1&&batch_edit_previous==0&&batch_edit_target==0);
    ui_page_03_menu_refresh_data(0);tick(120);
    assert(!menu.batch_dirty&&actual_state.batch_num==13);
    menu.selected_batch=2;batch_submit("60",NULL);ui_page_03_menu_refresh_data(0);tick(120);
    assert(batch_edit_saves==2&&!menu.batch_dirty&&actual_state.batch_num==60);
    reset_batch();
    click("Add slot");assert(menu.batch_count>=4);menu.selected_batch=2;menu.dirty=true;tick(120);
    click("Remove");assert(menu.selected_batch==2&&menu.batch_page==0);click("Cancel");
    model.users[0].batches[1]=10;actual_state.batch_num=50;reset_batch();menu.dirty=true;tick(120);
    click("Profiles");raster("menu-profiles");click("Delete");assert(settings_detail_overlay_is_open());raster("menu-delete-confirm");click("Cancel");assert(model.users[0].profile_count==2);
    click("Delete");click("Delete");assert(model.users[0].profile_count==1);ui_page_03_menu_refresh_data(0);tick(100);assert(model.users[0].profile_count==1);assert(lv_obj_has_state(find(menu.body,"Delete"),LV_STATE_DISABLED));
    click("Options");raster("menu-options");
    click("Mixed");assert(mode_requests==0&&!settings_detail_overlay_is_open());
    click("Single");assert(mode_requests==1&&!settings_detail_overlay_is_open()&&!test_notice_visible);
    click("High");assert(!settings_detail_overlay_is_open()&&!test_notice_visible);
    while(model.users[0].profile_count<WORKSPACE_PROFILES){
        workspace_profile_t extra=model.users[0].profiles[0];
        snprintf(extra.name,sizeof(extra.name),"Profile %u",model.users[0].profile_count+1);
        assert(workspace_add_profile(&model,model.active_id,&extra));
    }
    click("Profiles");assert(menu.task_scroll);
    workspace_profile_t match=current_profile();assert(profile_matches(&match));
    match.beep=!match.beep;assert(!profile_matches(&match));
    match=current_profile();match.mode=0;assert(profile_matches(&match));
    for(unsigned i=0;i<lv_obj_get_child_cnt(menu.task_scroll);++i){
        lv_obj_t *card=lv_obj_get_child(menu.task_scroll,i);
        assert(lv_obj_get_x(card)+lv_obj_get_width(card)<=ui_scrollbar_content_width(menu.task_scroll));
    }
    lv_obj_scroll_to_y(menu.task_scroll,500,LV_ANIM_OFF);tick(120);click("Profile 8");assert(menu.selected_profile==7&&lv_obj_get_scroll_y(menu.task_scroll)>200);raster("menu-profiles-scrolled");
    /* Hidden records keep their implementation and stable ID for future reuse. */
    menu.tab=2;menu.sub=0;menu.dirty=true;tick(120);raster("menu-records-retained-private");
    uint32_t prior_day=business_day();
    record_date_done("20260925",NULL);assert(settings_detail_overlay_is_open());
    assert(business_day()==prior_day);click("Cancel");assert(business_day()==prior_day);
    menu.selected_group=1;menu.selected_run=2;menu.dirty=true;tick(120);raster("menu-recount-detail");
    click("Count this result");click("Confirm");assert(ledger.groups[0].selected==2);click("Back to list");
    click("Confirm singles");click("Confirm");assert(ledger.groups[2].confirmed);
    click("Close day");click("Confirm");assert(ledger.close_count==1);click("Day closes");raster("menu-day-closes");
    show_closed_day(1);tick(120);raster("menu-saved-close");click("Show saved totals QR");assert(lv_qr_popup_is_showing());raster("menu-saved-close-qr");lv_qr_popup_hide();click("Back to list");
    click("History");raster("menu-history");assert(!find(menu.body,"Next"));click("Output");assert(!test_notice_visible);raster("menu-output");
    assert(output_action(807));tick(120);raster("menu-receipt-layout");
    assert(output_action(822));tick(120);assert(print_fixture.space_top==2&&test_notice_visible&&test_notice_kind==UI_NOTICE_PROGRESS);
    assert(output_action(827));assert(settings_detail_overlay_is_open());check_numeric_keypad();raster("menu-receipt-keypad");settings_detail_keyboard_hide();
    output_input("99",(void *)(uintptr_t)PRINT_CONFIG_BOTTOM);tick(120);assert(print_fixture.space_bottom==99);
    assert(output_action(807));tick(120);
    click("QR export");raster("menu-qr-export");
    click("Preferences");assert(find(menu.body,"Manage operators"));raster("menu-operators-entry");click("Manage operators");assert(current==UI_PAGE_OPERATORS);current=UI_PAGE_MENU;assert(model.user_count==2);
    click("Interaction");raster("menu-interaction");
    assert(menu.interaction_created && quick_pointer_policy && !quick_navigation_blocked);
    assert(lv_content_pager_get_count(menu.interaction_controls.pager)==2);
    lv_area_t controls_bounds;lv_obj_get_coords(menu.interaction_controls.viewport,&controls_bounds);
    bool swipe_layout=layout,swipe_gestures=gestures;
    point=(lv_point_t){controls_bounds.x1+800,controls_bounds.y1+50};down=LV_INDEV_STATE_PRESSED;tick(40);
    point.x-=400;tick(40);down=LV_INDEV_STATE_RELEASED;tick(400);
    assert(lv_quick_controls_page(&menu.interaction_controls)==1 && layout==swipe_layout && gestures==swipe_gestures);
    lv_content_pager_set_active(menu.interaction_controls.pager,0,false);
    menu.batch_dirty=true;unsigned blocked_nav=nav_count;
    lv_event_send(menu.interaction_controls.tiles[QUICK_HISTORY],LV_EVENT_CLICKED,NULL);assert(nav_count==blocked_nav);
    menu.batch_dirty=false;ui_notice_dismiss(NULL);
    lv_content_pager_set_active(menu.interaction_controls.pager,1,false);raster("menu-controls-page-2");
    lv_obj_t *top_toggle=menu.interaction_controls.tiles[QUICK_TOP_POPUP];
    top_popup_save_fails=true;lv_event_send(top_toggle,LV_EVENT_CLICKED,NULL);tick(120);
    assert(ui_notice_get_enabled()&&popup_auto);
    top_popup_save_fails=false;top_toggle=menu.interaction_controls.tiles[QUICK_TOP_POPUP];
    lv_event_send(top_toggle,LV_EVENT_CLICKED,NULL);tick(120);assert(!ui_notice_get_enabled()&&popup_auto);
    click("Pull-down shortcuts");raster("menu-pull-down-shortcuts");
    click("Layout edit");assert(!(quick_visible&(1U<<QUICK_LAYOUT)));
    uint16_t before_mask=quick_visible;local_save_fails=true;click("Gestures");
    assert(quick_visible==before_mask);settings_detail_dialog_hide();local_save_fails=false;
    click("Controls");assert(menu.interaction_created);
    assert(!lv_obj_has_flag(menu.interaction_controls.tiles[QUICK_LAYOUT],LV_OBJ_FLAG_HIDDEN));
    lv_event_send(menu.interaction_controls.tiles[QUICK_LAYOUT],LV_EVENT_CLICKED,NULL);tick(120);
    assert(!layout); /* Hidden pull-down entry still executes from full Controls. */
    quick_visible=UI_STATE_QUICK_VISIBLE_ALL;
    click("Quick");assert(menu.quick);assert(!test_notice_visible);
    quick_visible=0;tick(120);assert(lv_content_pager_get_count(menu.quick_controls.pager)==1);
    assert(lv_obj_is_visible(menu.quick_controls.empty));
    for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++)assert(lv_obj_has_flag(menu.quick_controls.tiles[i],LV_OBJ_FLAG_HIDDEN));
    quick_visible=1U<<QUICK_LAYOUT;tick(120);assert(!lv_obj_has_flag(menu.quick_controls.tiles[QUICK_LAYOUT],LV_OBJ_FLAG_HIDDEN));
    assert(lv_content_pager_get_count(menu.quick_controls.pager)==1);
    quick_visible=UI_STATE_QUICK_VISIBLE_ALL;tick(120);
    assert(lv_content_pager_get_count(menu.quick_controls.pager)==2);raster("menu-quick");
    assert(quick_pointer_policy&&quick_navigation_blocked);
    lv_obj_t *quick_sheet=lv_obj_get_child(menu.quick,0);
    assert(lv_obj_get_width(quick_sheet)==1280&&lv_obj_get_height(quick_sheet)==328);
    lv_obj_t *page_indicator=lv_obj_get_child(menu.quick_controls.pager,1);lv_area_t indicator_bounds;
    lv_obj_get_coords(page_indicator,&indicator_bounds);
    assert(indicator_bounds.x1==230&&indicator_bounds.y1==279&&indicator_bounds.x2<320&&indicator_bounds.y2<311);
    assert(lv_obj_get_child_cnt(page_indicator)==2&&lv_obj_is_visible(page_indicator));
    for(unsigned i=0;i<QUICK_CONTROLS_PER_PAGE;i++){
        lv_area_t bounds;lv_obj_get_coords(menu.quick_controls.tiles[i],&bounds);
        assert(lv_area_get_width(&bounds)==299&&lv_area_get_height(&bounds)==96);
        assert(bounds.x1>=25&&bounds.x2<1255&&bounds.y1>=65&&bounds.y2<267);
    }
    assert(lv_obj_has_flag(menu.quick_controls.version_panel,LV_OBJ_FLAG_HIDDEN));
    assert(find(menu.quick,"Popup"));
    lv_event_send(menu.quick_controls.tiles[QUICK_POPUP],LV_EVENT_CLICKED,NULL);tick(30);
    assert(!popup_auto&&!ui_notice_get_enabled());
    menu_quick_flip(true);raster("menu-quick-page-2");
    lv_area_t top_bounds;lv_obj_get_coords(menu.quick_controls.tiles[QUICK_TOP_POPUP],&top_bounds);
    assert(top_bounds.x1==25&&top_bounds.y1==65&&lv_area_get_width(&top_bounds)==299&&lv_area_get_height(&top_bounds)==96);
    click("Top popup");
    assert(!popup_auto&&ui_notice_get_enabled());
    assert(lv_obj_get_x(menu.quick_controls.knobs[QUICK_TOP_POPUP])==24);
    top_popup_save_fails=true;
    click("Top popup");
    assert(!popup_auto&&ui_notice_get_enabled()&&lv_obj_get_x(menu.quick_controls.knobs[QUICK_TOP_POPUP])==24);
    top_popup_save_fails=false;
    menu_quick_flip(false);
    lv_event_send(menu.quick_controls.tiles[QUICK_POPUP],LV_EVENT_CLICKED,NULL);tick(30);assert(popup_auto);
    lv_event_send(menu.quick_controls.device_button,LV_EVENT_CLICKED,NULL);tick(30);
    assert(!lv_obj_has_flag(menu.quick_controls.version_panel,LV_OBJ_FLAG_HIDDEN));raster("menu-quick-device");
    lv_obj_t *device_close=lv_obj_get_child(menu.quick_controls.version_panel,1);
    assert(lv_obj_get_width(device_close)==44&&lv_obj_get_height(device_close)==44);
    lv_event_send(device_close,LV_EVENT_CLICKED,NULL);tick(30);
    assert(lv_obj_has_flag(menu.quick_controls.version_panel,LV_OBJ_FLAG_HIDDEN));
    lv_obj_t *quick_before=menu.quick,*gesture_state=menu.quick_controls.switches[QUICK_GESTURES];
    gestures=!gestures;tick(120);assert(menu.quick==quick_before&&menu.quick_controls.switches[QUICK_GESTURES]==gesture_state);
    assert(lv_obj_get_x(menu.quick_controls.knobs[QUICK_GESTURES])==(gestures?24:4));
    point=(lv_point_t){40,365};down=LV_INDEV_STATE_PRESSED;tick(40);down=LV_INDEV_STATE_RELEASED;tick(120);assert(!menu.quick);
    assert(quick_pointer_policy&&!quick_navigation_blocked);
    click("Quick");assert(lv_quick_controls_page(&menu.quick_controls)==0);
    for(unsigned lap=0;lap<2;lap++){
        menu_quick_flip(false);menu_quick_flip(true);
        menu_quick_flip(true);menu_quick_flip(false);
    }
    menu_quick_flip(true);ui_page_03_menu_suspend();
    assert(!menu.quick&&!quick_pointer_policy&&!quick_navigation_blocked);
    assert(ui_page_03_menu_resume());click("Quick");assert(lv_quick_controls_page(&menu.quick_controls)==0);
    point=(lv_point_t){40,365};down=LV_INDEV_STATE_PRESSED;tick(40);down=LV_INDEV_STATE_RELEASED;tick(120);assert(!menu.quick);
    click("Display & sound");raster("menu-display-sound");
    click("Help");raster("menu-help-empty");click("All codes");raster("menu-help");click("Care");raster("menu-care");click("Device");raster("menu-device");ui_page_03_menu_suspend();assert(!page_03_menu_is_visible());assert(ui_page_03_menu_resume());assert(menu.tab==5);
    cashbook_t *saved=malloc(sizeof(*saved));assert(saved);*saved=ledger;cashbook_defaults(&ledger);
    menu.tab=2;menu.sub=0;menu.selected_group=menu.selected_close=0;menu.dirty=true;tick(120);
    assert(lv_obj_has_state(find(menu.body,"Close day"),LV_STATE_DISABLED)&&!find(menu.body,"Confirm singles"));raster("menu-records-empty");
    ledger=*saved;free(saved);
    notify(UI_NOTICE_SUCCESS,"Saved");assert(test_notice_visible&&test_notice_kind==UI_NOTICE_SUCCESS);
    unsigned notice_count=test_notice_count;ui_page_03_menu_refresh_data(0);tick(3600);assert(test_notice_count==notice_count);
    /* Page refresh cannot duplicate a storage result: the app owns it. */
    record_success=false;ui_page_03_menu_refresh_data(0);assert(test_notice_count==notice_count);record_success=true;
    export_records_confirm(NULL);assert(operation_starts&&operation_started==APP_UI_NOTICE_RECORD_STORE);
    notify(UI_NOTICE_ERROR,"Setting rejected");assert(test_notice_visible&&!settings_detail_overlay_is_open());
    ui_page_03_menu_suspend();notify(UI_NOTICE_INFO,"Hidden update");assert(!test_notice_visible);assert(ui_page_03_menu_resume());
    /* Repeated show/hide and parent destruction balance independent owners. */
    lv_modal_dialog_t first={0},second={0};
    lv_modal_dialog_config_t decision={.title="Decision",.body="Review",.primary_text="Confirm",
        .title_font=LV_FONT_DEFAULT,.body_font=LV_FONT_DEFAULT,.button_font=LV_FONT_DEFAULT};
    assert(test_notice_dialog_holds==0);
    assert(lv_modal_dialog_show(&first,lv_scr_act(),&decision));
    assert(lv_modal_dialog_show(&first,lv_scr_act(),&decision));
    assert(test_notice_dialog_holds==1);
    assert(lv_modal_dialog_show(&second,lv_scr_act(),&decision));
    lv_modal_dialog_hide(&first);lv_modal_dialog_hide(&first);assert(test_notice_dialog_holds==1);
    lv_obj_del(second.root);assert(second.root==NULL&&test_notice_dialog_holds==0);
    lv_obj_t *temporary=lv_obj_create(lv_scr_act());
    assert(lv_modal_dialog_show(&first,temporary,&decision));assert(test_notice_dialog_holds==1);
    settings_detail_dialog_show("Decision", "Review", "Confirm", "Cancel", NULL, NULL, NULL);
    assert(test_notice_dialog_holds==2);
    lv_obj_del(temporary);assert(first.root==NULL&&test_notice_dialog_holds==1);
    settings_detail_dialog_hide();settings_detail_dialog_hide();assert(test_notice_dialog_holds==0);
    settings_detail_dialog_show("Decision", "Review", "Confirm", "Cancel", NULL, NULL, NULL);
    lv_obj_t *settings_root=lv_obj_get_child(lv_scr_act(),-1);assert(settings_root);
    lv_obj_del(settings_root);
    assert(!settings_detail_overlay_is_open()&&test_notice_dialog_holds==0);
    lv_modal_dialog_destroy(&first);lv_modal_dialog_destroy(&second);
    puts("PASS Menu hierarchy: four visible sections, default Count, localized standard Back, common gray mode trays and white selection, aligned utilities, fixed OFF, guarded drafts, retained private records, in-place Quick refresh");
    /* Full-page visual fixtures: not production sample data. */
    ui_page_03_menu_destroy();workspace_defaults(&model);model.operators_enabled=1;strcpy(model.users[0].name,"Local operator");assert(workspace_add_user(&model,"Alex",&uid));strcpy(model.users[1].employee_id,"A-002");
    p=model.users[0].profiles[0];p.add=1;model.users[0].profiles[0]=p;strcpy(p.name,"Bundle preparation");p.batch_enabled=1;p.batch=100;assert(workspace_add_profile(&model,1,&p));
    strcpy(p.name,"Careful verification");p.speed=0;p.work=1;p.batch_enabled=0;assert(workspace_add_profile(&model,1,&p));
    cashbook_defaults(&ledger);
    cashbook_command_t golden_day={.operation=CASHBOOK_SET_DAY,.day=20260924};assert(cashbook_store_submit(&golden_day));
    cashbook_command_t golden={.operation=CASHBOOK_INGEST,.result={.source=30,.day=20260924,.hour=8,.minute=58,.operator_id=1,.operator_name="Foreign currency",.pcs=12,.currencies=1,.complete=true,.money={{.code="USD",.pcs=12,.amount=240}}}};
    assert(cashbook_store_submit(&golden));cashbook_command_t accepted={.operation=CASHBOOK_CONFIRM,.group=1,.run=1};assert(cashbook_store_submit(&accepted));
    golden.result.source=31;golden.result.hour=9;golden.result.minute=10;strcpy(golden.result.operator_name,"Opening balance");golden.result.pcs=100;golden.result.money[0]=(cashbook_money_t){.code="CNY",.pcs=100,.amount=10000};assert(cashbook_store_submit(&golden));accepted.group=2;accepted.run=2;assert(cashbook_store_submit(&accepted));
    golden.result.source=32;golden.result.minute=31;strcpy(golden.result.operator_name,"Cash deposit");golden.result.pcs=60;golden.result.money[0].pcs=60;golden.result.money[0].amount=6000;assert(cashbook_store_submit(&golden));
    golden.result.source=33;golden.result.minute=38;strcpy(golden.result.operator_name,"Counter 02");golden.result.pcs=137;golden.result.money[0].pcs=137;golden.result.money[0].amount=6850;assert(cashbook_store_submit(&golden));
    golden.result.source=34;golden.result.minute=42;strcpy(golden.result.operator_name,"Morning count");golden.result.pcs=773;golden.result.money[0].pcs=773;golden.result.money[0].amount=7730;assert(cashbook_store_submit(&golden));
    golden.result.recount_group=5;golden.result.source=35;golden.result.pcs=771;golden.result.money[0].pcs=771;golden.result.money[0].amount=7710;assert(cashbook_store_submit(&golden));
    golden.result.source=36;golden.result.pcs=773;golden.result.money[0].pcs=773;golden.result.money[0].amount=7730;assert(cashbook_store_submit(&golden));
    actual_state.add_enabled=true;actual_state.speed=0;actual_state.buzzer_enabled=true;gestures=true;backlight=75;
    count_fixture.total_pcs=773;count_fixture.total_amount=7730;
    current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(120);
    assert(menu.tab==1&&!menu.nav[0]&&!menu.nav[2]);raster("studio-default-count");
    menu.tab=0;menu.sub=0;menu.dirty=true;tick(120);assert(business_day()==20260924);assert(find(menu.body,"Review 3 pending counts"));raster("studio-overview-retained-private");
    lv_obj_add_state(menu.nav[1],LV_STATE_FOCUS_KEY);raster("studio-focus");lv_obj_clear_state(menu.nav[1],LV_STATE_FOCUS_KEY);
    const char *shots[][5]={{NULL},{"batch","profiles","options"},{"records","recounts","history","verify","day-closes"},{"print","qr"},{"operators","interaction","display"},{"reject","care","device"}};
    for(unsigned tab=1;tab<6;tab++)for(unsigned sub=0;sub<subcounts[tab];sub++){
        menu.tab=tab;menu.sub=sub;menu.selected_group=menu.selected_close=0;menu.record_pending=menu.verify_pick=false;menu.dirty=true;tick(100);
        char filename[80];snprintf(filename,sizeof(filename),"studio-%s",shots[tab][sub]);raster(filename);
        assert(lv_obj_get_y(menu.body)==76&&lv_obj_get_height(menu.body)==296);
    }
    menu.tab=5;menu.sub=0;menu.reject_all=true;
    menu.tab=4;menu.sub=1;menu.shortcut_visibility=true;menu.dirty=true;tick(100);
    assert(menu.task_scroll);
    for(unsigned i=0;i<QUICK_CONTROL_COUNT-1;++i){
        lv_obj_t *item=lv_obj_get_child(menu.task_scroll,i);
        assert(lv_obj_get_x(item)+lv_obj_get_width(item)<=ui_scrollbar_content_width(menu.task_scroll));
        assert(lv_obj_get_style_border_width(item,0)==0);
        assert(lv_obj_get_style_text_align(lv_obj_get_child(item,0),0)==LV_TEXT_ALIGN_LEFT);
    }
    lv_obj_scroll_to_y(menu.task_scroll,100,LV_ANIM_OFF);tick(200);
    lv_coord_t shortcut_y=lv_obj_get_scroll_y(menu.task_scroll);assert(shortcut_y>0);
    lv_event_send(lv_obj_get_child(menu.task_scroll,QUICK_CONTROL_COUNT-2),LV_EVENT_CLICKED,NULL);tick(200);
    assert(lv_obj_get_scroll_y(menu.task_scroll)==shortcut_y);raster("shortcuts-selected-scrolled");
    menu.tab=5;menu.sub=0;menu.reject_all=true;
    for(unsigned code=0;code<0x32;code++){menu.reject_code=code;menu.dirty=true;tick(100);
        const counting_reject_guide_t *guide=counting_reject_guide_get(code);assert(guide&&*guide->title&&*guide->meaning&&*guide->causes&&*guide->action);
        lv_obj_t *content=lv_obj_get_child(menu.body,1),*detail=lv_obj_get_child(content,-1);lv_obj_update_layout(detail);
        for(unsigned i=0;i<lv_obj_get_child_cnt(detail);i++){lv_obj_t *child=lv_obj_get_child(detail,i);assert(lv_obj_get_y(child)+lv_obj_get_height(child)<=248);}
        if(code==0x14||code==0x1e||code==0x2e){lv_obj_scroll_to_y(menu.task_scroll,(code-1)*58-90,LV_ANIM_OFF);char filename[64];snprintf(filename,sizeof(filename),"studio-reject-%02x",code);raster(filename);}
    }
    assert(strstr(counting_reject_guide_get(0x1e)->title,"denomination"));assert(strstr(counting_reject_guide_get(0x2e)->title,"Duplicate"));assert(strstr(counting_reject_guide_get(0xfe)->title,"Unknown"));
    menu.tab=4;menu.sub=0;menu.dirty=true;tick(100);assert(find(menu.body,"Manage operators"));raster("studio-operators-entry");
    for(unsigned i=model.user_count;i<8;++i){char n[25];snprintf(n,sizeof(n),"Operator %u",i+1);assert(workspace_add_user(&model,n,NULL));}
    revision++;menu.dirty=true;tick(120);raster("operators-summary-eight");
    lv_obj_t *person=find(menu.body,"Alex");assert(person);
    lv_obj_t *list=lv_obj_get_parent(person); /* the last matching label is the card */
    assert(lv_obj_get_height(person)==68&&lv_obj_get_width(person)==284);assert(lv_obj_get_x(person)+lv_obj_get_width(person)<=lv_obj_get_width(list)-UI_SCROLLBAR_GUTTER);lv_obj_scroll_to_y(list,152,LV_ANIM_OFF);tick(100);raster("operators-summary-scrolled");
    puts("PASS Studio: shared settings and dedicated Operators navigation");
#ifdef MENU_REAL_FONTS
    ui_page_03_menu_destroy();
    for(size_t language=0;language<ui_lang_count();++language){
        const ui_locale_t *locale=ui_lang_at(language);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
        ui_page_03_menu_create(lv_scr_act());
        menu.tab=1;menu.sub=1;menu.dirty=true;tick(100);
        const workspace_user_t *profiles_user=user();
        assert(workspace_profile_has_default_name(&profiles_user->profiles[0]));
        assert(!strcmp(profiles_user->profiles[0].name,"Everyday count"));
        lv_obj_t *name_probe=lv_label_create(lv_scr_act());
        lv_label_set_text(name_probe,ui_tr("Everyday count"));
        assert(find(menu.body,lv_label_get_text(name_probe)));lv_obj_del(name_probe);
        char locale_shot[80];snprintf(locale_shot,sizeof(locale_shot),"profiles-%s",locale->tag);raster(locale_shot);
        menu.sub=2;menu.dirty=true;tick(100);
        snprintf(locale_shot,sizeof(locale_shot),"options-%s",locale->tag);raster(locale_shot);
        menu.tab=4;menu.sub=1;menu.shortcut_visibility=false;menu.dirty=true;tick(100);
        lv_obj_t *interaction_content=lv_obj_get_child(menu.body,1);
        check_interaction_header(interaction_content);
        for(unsigned page=0;page<lv_content_pager_get_count(menu.interaction_controls.pager);page++){
            lv_content_pager_set_active(menu.interaction_controls.pager,page,false);
            char name[80];snprintf(name,sizeof(name),"controls-%s-page-%u",locale->tag,page+1);raster(name);
        }
        for(unsigned i=0;i<QUICK_CONTROL_COUNT;i++){
            lv_obj_t *title=lv_obj_get_child(menu.interaction_controls.tiles[i],1);
            const char *text=lv_label_get_text(title);const lv_font_t *font=lv_obj_get_style_text_font(title,0);
            int width=lv_txt_get_width(text,strlen(text),font,0,LV_TEXT_FLAG_NONE);
            if(width>lv_obj_get_width(title))fprintf(stderr,"Control title clipped %s: %s %d/%d\n",locale->tag,text,width,lv_obj_get_width(title));
            assert(width<=155);
        }
        menu.shortcut_visibility=true;menu.dirty=true;tick(100);
        /* The local Batch preference uses one complete line in every locale. */
        menu.tab=1;menu.sub=0;menu.dirty=true;tick(120);
        for(unsigned mode=0;mode<2;mode++){
            main_batch_action=mode;menu.dirty=true;tick(120);
            lv_obj_t *batch_content=lv_obj_get_child(menu.body,1);
            lv_obj_t *toggle=lv_obj_get_child(batch_content,-1),*caption=lv_obj_get_child(toggle,0);
            assert(lv_obj_get_y(toggle)==256&&lv_obj_get_height(toggle)==40&&lv_obj_get_width(toggle)==470);
            assert(lv_obj_get_style_bg_color(toggle,0).full==lv_color_hex(BLUE).full);
            int natural=lv_txt_get_width(lv_label_get_text(caption),strlen(lv_label_get_text(caption)),lv_obj_get_style_text_font(caption,0),0,LV_TEXT_FLAG_NONE);
            assert(natural<=lv_obj_get_width(caption)&&lv_obj_get_height(caption)<=40);
            char batch_name[80];snprintf(batch_name,sizeof(batch_name),"batch-%s-mode-%u",locale->tag,mode);raster(batch_name);
        }
        main_batch_action=UI_MAIN_BATCH_SETTINGS;
        menu.tab=4;menu.sub=1;menu.shortcut_visibility=true;menu.dirty=true;tick(100);
        char name[80];snprintf(name,sizeof(name),"shortcuts-%s",locale->tag);raster(name);
        check_interaction_header(lv_obj_get_child(menu.body,1));
        lv_obj_t *left=lv_obj_get_child(lv_obj_get_child(menu.body,1),2),*rows=lv_obj_get_child(left,0);
        for(unsigned i=0;i<2;i++){
            lv_obj_t *caption=lv_obj_get_child(rows,i*4);
            int natural=lv_txt_get_width(lv_label_get_text(caption),strlen(lv_label_get_text(caption)),lv_obj_get_style_text_font(caption,0),0,LV_TEXT_FLAG_NONE);
            if(natural>lv_obj_get_width(caption))fprintf(stderr,"Toggle title clipped %s: %s %d/%d\n",locale->tag,lv_label_get_text(caption),natural,lv_obj_get_width(caption));
            assert(natural<=lv_obj_get_width(caption));
        }
        assert(lv_obj_get_child_cnt(menu.task_scroll)==QUICK_CONTROL_COUNT-1);
        for(unsigned i=0;i<QUICK_CONTROL_COUNT-1;i++){
            lv_obj_t *item=lv_obj_get_child(menu.task_scroll,i),*title=lv_obj_get_child(item,0);
            const char *text=lv_label_get_text(title);int width=lv_txt_get_width(text,strlen(text),lv_obj_get_style_text_font(title,0),0,LV_TEXT_FLAG_NONE);
            if(width>lv_obj_get_width(title))fprintf(stderr,"Shortcut title clipped %s: %s %d/%d\n",locale->tag,text,width,lv_obj_get_width(title));
            assert(width<=lv_obj_get_width(title));

        }
        settings_detail_keyboard_show(ui_tr("Enter capacity"),"10",3,
            SETTINGS_DETAIL_KEYBOARD_UINT,batch_submit,NULL);
        lv_obj_update_layout(lv_scr_act());check_keyboard_labels(lv_obj_get_child(lv_scr_act(),-1));
        snprintf(name,sizeof(name),"numeric-keyboard-%s",locale->tag);raster(name);
        settings_detail_keyboard_hide();
        settings_detail_keyboard_show(ui_tr("Header line 1"),"UNION",32,
            SETTINGS_DETAIL_KEYBOARD_TEXT,output_input,NULL);
        lv_obj_update_layout(lv_scr_act());check_keyboard_labels(lv_obj_get_child(lv_scr_act(),-1));
        snprintf(name,sizeof(name),"text-keyboard-%s",locale->tag);raster(name);
        settings_detail_keyboard_hide();
        alnum_test_context_t keyboard_state={.owner=alnum_test_owner()};
        lv_alnum_keyboard_config_t keyboard_config=alnum_test_config(&keyboard_state);
        keyboard_config.title=ui_text_get(UI_TEXT_QUERY_SERIAL);
        keyboard_config.placeholder=ui_text_get(UI_TEXT_SERIAL_INPUT_HINT);
        keyboard_config.apply_text=ui_tr("Search");
        keyboard_config.clear_text=ui_tr("Clear");
        keyboard_config.cancel_text=ui_tr("Back");keyboard_config.symbols=true;
        lv_alnum_keyboard_t *keyboard=lv_alnum_keyboard_create(keyboard_state.owner,&keyboard_config);assert(keyboard);
        lv_alnum_keyboard_show(keyboard);lv_obj_update_layout(keyboard_state.owner);check_keyboard_labels(keyboard_state.owner);
        snprintf(name,sizeof(name),"search-keyboard-%s",locale->tag);raster(name);
        lv_obj_del(keyboard_state.owner);
        ui_page_03_menu_destroy();
    }
    ui_lang_set(LANGUAGE_EN);ui_page_03_menu_create(lv_scr_act());menu.tab=4;menu.sub=1;menu.dirty=true;tick(100);
    quick_visible=0;open_quick();raster("quick-no-shortcuts");destroy_quick();
    uint8_t hidden_order[QUICK_CONTROL_COUNT];memcpy(hidden_order,quick_order,sizeof(hidden_order));
    quick_visible=UI_STATE_QUICK_VISIBLE_ALL & ~(1U<<QUICK_LAYOUT) & ~(1U<<QUICK_TOUCH);
    uint8_t value=quick_order[1];quick_order[1]=quick_order[7];quick_order[7]=value;
    open_quick();raster("quick-filtered-order");
    assert(lv_obj_has_flag(menu.quick_controls.tiles[QUICK_LAYOUT],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(menu.quick_controls.tiles[QUICK_TOUCH],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_has_flag(menu.quick_controls.tiles[QUICK_CASH_JOURNAL],LV_OBJ_FLAG_HIDDEN));
    assert(lv_obj_get_x(menu.quick_controls.tiles[QUICK_STANDBY])==0 && lv_obj_get_y(menu.quick_controls.tiles[QUICK_STANDBY])==0);
    assert(!memcmp(menu.quick_controls.state.order,quick_order,sizeof(quick_order)));
    destroy_quick();memcpy(quick_order,hidden_order,sizeof(hidden_order));quick_visible=UI_STATE_QUICK_VISIBLE_ALL;
    menu.shortcut_visibility=true;menu.dirty=true;tick(100);
    test_alnum_keyboard_cases();
    puts("PASS Controls and pull-down visibility in ten languages: actual locale fonts, complete titles, compact pages, independent empty selection");
#endif
    settings_detail_keyboard_show("Lifecycle check","10",3,SETTINGS_DETAIL_KEYBOARD_UINT,batch_submit,NULL);
    assert(settings_detail_overlay_is_open());ui_page_03_menu_destroy();assert(!menu.root&&!settings_detail_overlay_is_open());
    /* Main can request Presets before Menu has any LVGL objects. */
    current=UI_PAGE_MAIN;main_batch_action=UI_MAIN_BATCH_SETTINGS;
    unsigned cold_navigation=nav_count;page_03_menu_open_batch();
    assert(!menu.root&&nav_count==cold_navigation+1&&menu.tab==1&&menu.sub==0);
    current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(120);
    assert(find(menu.body,"Batch presets")&&find(menu.body,"Main Batch: Settings"));
    raster("menu-cold-main-batch-entry");ui_page_03_menu_destroy();
    tick(200);test_auto_qr();puts("PASS native Menu: four visible tabs, retained private ledger decisions/close/QR, batch action/drafts/keypad, minimum-one delete, registration, outside-close, resume/destroy; 1280x400");return 0;
}

const char *work_mode_service_status_msgid(void){return work_mode_service_status_text();}
const ui_message_t *workspace_store_message_info(void){static ui_message_t m;ui_message_key(&m,workspace_store_message());return &m;}
