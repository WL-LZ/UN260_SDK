#include "test_notice_sink.h"
#include <assert.h>
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
uint8_t machine_state_mode(void){return MODE_MDC;}
uint8_t machine_state_reject_pocket_max(void){return 100;}
static unsigned mode_requests;
bool setting_service_request_mode(uint8_t n){(void)n;mode_requests++;return true;}
bool setting_service_request_reject_pocket_max(uint8_t n,uint8_t p){(void)n;(void)p;return true;}
static ui_history_store_t history_fixture;
static unsigned revision,nav_count,printed;
static ui_page_t current=UI_PAGE_MENU;
static bool busy,gestures=true,layout=true;
static bool auto_qr_blocked,auto_qr_waiting;
bool ui_manager_is_transitioning(void){return false;}
bool app_command_runtime_result_pending(void){return auto_qr_waiting;}
bool fault_popup_is_showing(void){return auto_qr_blocked;}
bool fault_popup_get_pending_fault(fault_source_t *s,uint8_t *t,uint8_t *c){(void)s;(void)t;(void)c;return false;}
bool smart_island_is_expanded(void){return false;}
bool page_01_main_quick_is_open(void){return false;}
static machine_state_snapshot_t actual_state={.batch_enabled=true,.batch_num=50,.speed=1,.buzzer_enabled=true};
const workspace_model_t *workspace_store_get(void){return &model;}
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
bool gesture_service_set_enabled(bool e){gestures=e;return true;}
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
static void read_pointer(lv_indev_drv_t *d,lv_indev_data_t *p){(void)d;p->point=point;p->state=down;}
static void tick(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();}}
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
    lv_obj_t *tray=lv_obj_get_parent(menu.nav[0]);lv_area_t t;
    lv_obj_get_coords(tray,&t);
    assert(lv_obj_get_style_border_width(tray,0)==0);
    assert(lv_obj_get_style_bg_color(tray,0).full==lv_color_hex(0xE7EDF0).full);
    for(unsigned i=0;i<6;i++){
        lv_obj_t *v=menu.nav[i],*l=lv_obj_get_child(v,0),*im=lv_obj_get_child(v,1);
        lv_area_t b,a,c;lv_obj_get_coords(v,&b);lv_obj_get_coords(im,&a);lv_obj_get_coords(l,&c);
        assert(abs((a.x1-b.x1)-(b.x2-c.x2))<=1);
        assert(c.x1-a.x2-1==8);
        assert(abs((a.y1+a.y2)-(b.y1+b.y2))<=1);
        assert(abs((c.y1+c.y2)-(b.y1+b.y2))<=1);
        assert(lv_obj_get_style_border_width(v,0)==0&&lv_obj_get_style_shadow_width(v,0)==0);
        assert(lv_obj_get_style_bg_color(v,0).full==lv_color_hex(menu.tab==i?0xFFFFFF:0xE7EDF0).full);
    }
    lv_area_t first,last;lv_obj_get_coords(menu.nav[0],&first);lv_obj_get_coords(menu.nav[5],&last);
    assert(first.x1-t.x1==t.x2-last.x2&&first.x1-t.x1==4);
    for(unsigned i=0;i<lv_obj_get_child_cnt(menu.root);i++){
        lv_obj_t *v=lv_obj_get_child(menu.root,i);
        if(!lv_obj_has_flag(v,LV_OBJ_FLAG_CLICKABLE)||lv_obj_get_y(v)!=14)continue;
        assert(lv_obj_get_height(v)==44&&lv_obj_get_style_border_width(v,0)==0);
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
    assert(lv_obj_get_style_border_width(one,0)==1&&lv_obj_get_style_radius(one,0)==12);
    assert(lv_obj_get_style_border_opa(one,0)==LV_OPA_TRANSP);
    assert(lv_obj_get_style_text_font(lv_obj_get_child(one,0),0)==&lv_font_instrument_sans_medium_26);
    assert(lv_obj_get_style_bg_color(clear,0).full==lv_color_hex(0xFBFCFD).full);
    assert(lv_obj_get_style_border_width(clear,0)==0);
}
static void raster(const char *name)
{
    if(page_03_menu_is_visible()){lv_obj_update_layout(menu.root);check_header();}
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
int main(void)
{
    cashbook_defaults(&ledger);workspace_defaults(&model);strcpy(model.users[0].name,"Alex Morgan");
    seed_ledger();char report[8192];assert(support_report_build(report,sizeof(report)));
    assert(strstr(report,"Confirmed settings")&&!strstr(report,"Alex Morgan")&&!strstr(report,"10000"));assert(!support_report_build(report,8));
    workspace_profile_t p=model.users[0].profiles[0];strcpy(p.name,"Bundle preparation");p.batch_enabled=1;p.batch=100;assert(workspace_add_profile(&model,1,&p));
    uint32_t uid;assert(workspace_add_user(&model,"Taylor",&uid));
    history_fixture.record_count=1;history_fixture.records[0]=(ui_history_record_t){.valid=true,.record_no=1,.pcs=85,.amount=425,.currency="CNY",.year=2026,.month=9,.day=23,.hour=10,.minute=42};
    lv_init();lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_buffer,NULL,1280*64);lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=1280;d.ver_res=400;d.flush_cb=flush;d.draw_buf=&buf;lv_disp_t *display=lv_disp_drv_register(&d);assert(display);ui_scrollbar_init(display);
    lv_indev_drv_t in;lv_indev_drv_init(&in);in.type=LV_INDEV_TYPE_POINTER;in.read_cb=read_pointer;assert(lv_indev_drv_register(&in));
    lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,asset_info);lv_img_decoder_set_open_cb(decoder,asset_open);lv_img_decoder_set_close_cb(decoder,asset_close);
    ui_page_03_menu_create(lv_scr_act());tick(200);raster("menu-overview");assert(!test_notice_visible);
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
    lv_obj_scroll_to_y(menu.task_scroll,500,LV_ANIM_OFF);tick(120);click("Profile 8");assert(menu.selected_profile==7&&lv_obj_get_scroll_y(menu.task_scroll)>200);raster("menu-profiles-scrolled");
    click("Records");raster("menu-records");
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
    click("Preferences");assert(!find(menu.body,"Switch operator"));raster("menu-operators");click("New operator");assert(menu.user_edit);raster("menu-register");click("Enter a name");assert(settings_detail_overlay_is_open());raster("menu-name-keyboard");settings_detail_keyboard_hide();
    name_submit("Jordan",NULL);tick(120);click("Import USB photo");ui_page_03_menu_refresh_data(0);tick(120);assert(menu.photo_sheet);raster("menu-usb-empty");
    point=(lv_point_t){80,190};down=LV_INDEV_STATE_PRESSED;tick(40);down=LV_INDEV_STATE_RELEASED;tick(120);assert(!menu.photo_sheet);
    click("Create operator");ui_page_03_menu_refresh_data(0);tick(100);assert(model.user_count==3&&!menu.user_edit);
    assert(find(menu.body,"Delete")&&!lv_obj_has_state(find(menu.body,"Delete"),LV_STATE_DISABLED));
    click("Delete");assert(settings_detail_overlay_is_open());click("Cancel");assert(model.user_count==3);
    click("Delete");assert(settings_detail_overlay_is_open());click("Delete");
    ui_page_03_menu_refresh_data(0);tick(100);
    assert(model.user_count==2&&menu.selected_user==0);
    assert(lv_obj_has_state(find(menu.body,"Delete"),LV_STATE_DISABLED));
    click("Interaction");raster("menu-interaction");click("Try Quick controls");assert(menu.quick);assert(!test_notice_visible);raster("menu-quick");
    lv_obj_t *quick_before=menu.quick;lv_obj_t *switch_before=menu.quick_switch[1];
    gestures=!gestures;tick(120);assert(menu.quick==quick_before&&menu.quick_switch[1]==switch_before);
    assert(lv_obj_has_state(switch_before,LV_STATE_CHECKED)==gestures);
    assert(lv_obj_get_x(lv_obj_get_child(switch_before,0))==(gestures?25:3));
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
    puts("PASS Menu hierarchy: centered borderless ABC-color header, aligned utility buttons, fixed OFF, guarded drafts, empty records, current operator status, no persistent footer, scoped transient feedback, in-place Quick refresh");
    /* Full-page visual fixtures: not production sample data. */
    ui_page_03_menu_destroy();workspace_defaults(&model);strcpy(model.users[0].name,"Local operator");assert(workspace_add_user(&model,"Alex",&uid));strcpy(model.users[1].employee_id,"A-002");
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
    current=UI_PAGE_MENU;ui_page_03_menu_create(lv_scr_act());tick(120);assert(business_day()==20260924);assert(find(menu.body,"Review 3 pending counts"));raster("studio-overview");
    lv_obj_add_state(menu.nav[1],LV_STATE_FOCUS_KEY);raster("studio-focus");lv_obj_clear_state(menu.nav[1],LV_STATE_FOCUS_KEY);
    const char *shots[][5]={{NULL},{"batch","profiles","options"},{"records","recounts","history","verify","day-closes"},{"print","qr"},{"operators","interaction","display"},{"reject","care","device"}};
    for(unsigned tab=1;tab<6;tab++)for(unsigned sub=0;sub<subcounts[tab];sub++){
        menu.tab=tab;menu.sub=sub;menu.selected_group=menu.selected_close=0;menu.record_pending=menu.verify_pick=false;menu.dirty=true;tick(100);
        char filename[80];snprintf(filename,sizeof(filename),"studio-%s",shots[tab][sub]);raster(filename);
        assert(lv_obj_get_y(menu.body)==76&&lv_obj_get_height(menu.body)==296);
    }
    menu.tab=5;menu.sub=0;menu.reject_all=true;
    for(unsigned code=0;code<0x32;code++){menu.reject_code=code;menu.dirty=true;tick(100);
        const counting_reject_guide_t *guide=counting_reject_guide_get(code);assert(guide&&*guide->title&&*guide->meaning&&*guide->causes&&*guide->action);
        lv_obj_t *content=lv_obj_get_child(menu.body,1),*detail=lv_obj_get_child(content,-1);lv_obj_update_layout(detail);
        for(unsigned i=0;i<lv_obj_get_child_cnt(detail);i++){lv_obj_t *child=lv_obj_get_child(detail,i);assert(lv_obj_get_y(child)+lv_obj_get_height(child)<=248);}
        if(code==0x14||code==0x1e||code==0x2e){lv_obj_scroll_to_y(menu.task_scroll,(code-1)*58-90,LV_ANIM_OFF);char filename[64];snprintf(filename,sizeof(filename),"studio-reject-%02x",code);raster(filename);}
    }
    assert(strstr(counting_reject_guide_get(0x1e)->title,"denomination"));assert(strstr(counting_reject_guide_get(0x2e)->title,"Duplicate"));assert(strstr(counting_reject_guide_get(0xfe)->title,"Unknown"));
    menu.tab=4;menu.sub=0;menu.selected_user=0;menu.dirty=true;tick(100);click("Edit details");name_submit("Local operator",NULL);name_submit("B-007",(void *)2);name_submit("Branch One",(void *)3);tick(100);click("Save details");ui_page_03_menu_refresh_data(0);tick(100);
    assert(!strcmp(model.users[0].employee_id,"B-007")&&!strcmp(model.users[0].team,"Branch One"));raster("studio-operator-metadata");
    puts("PASS Studio: all 17 pages, 50 protocol code entries, unknown fallback, 28px reserve, keyboard focus, operator metadata edit");
    settings_detail_keyboard_show("Lifecycle check","10",3,SETTINGS_DETAIL_KEYBOARD_UINT,batch_submit,NULL);
    assert(settings_detail_overlay_is_open());ui_page_03_menu_destroy();assert(!menu.root&&!settings_detail_overlay_is_open());
    tick(200);test_auto_qr();puts("PASS native Menu: six tabs, real ledger decisions/close/QR, batch drafts/keypad, minimum-one delete, registration, outside-close, resume/destroy; 1280x400");return 0;
}

const char *work_mode_service_status_msgid(void){return work_mode_service_status_text();}
const ui_message_t *workspace_store_message_info(void){static ui_message_t m;ui_message_key(&m,workspace_store_message());return &m;}
