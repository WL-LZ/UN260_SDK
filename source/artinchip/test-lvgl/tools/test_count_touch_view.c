#include "lvgl/lvgl.h"
/* Real Main, LVGL hit-testing, pointer capture and global gesture observer. */
#define HOST_REAL_GESTURE
#define HOST_ISLAND_ONLY
#define main unused_main_suite
#include "test_main_view.c"
#undef main

void lv_port_indev_set_pointer_observer(lv_port_pointer_observer_t cb,void *data)
{ (void)data;host_raw_observer=cb; }
uint8_t lv_port_indev_touch_count(void){return pointer_state==LV_INDEV_STATE_PRESSED?1:0;}
uint8_t lv_port_indev_touch_points(lv_point_t *p,int32_t *ids,uint8_t capacity)
{if(!capacity||!lv_port_indev_touch_count())return 0;p[0]=pointer_position;ids[0]=1;return 1;}
bool user_cfg_gesture_enabled(void){return host_gestures;}
bool user_cfg_gesture_save(bool enabled){host_gestures=enabled;return true;}
bool app_standby_runtime_touch(bool down){(void)down;return false;}
bool ui_page_05_set_password_is_open(void){return false;}
bool ui_page_05_set_password_request_back(void){return false;}
bool page_32_innovation_request_back(void){return false;}
bool ui_manager_restore_from_home(void){++pushes;return true;}
bool ui_manager_suspend_to_home(void){++pushes;return true;}
bool page_06_settings_back_sub_page(void){return false;}
void touch_feedback_init(void){}
void uart_debug_printf(const char *format,...){(void)format;}
void touch_feedback_sample(const lv_point_t *p,uint8_t n){(void)p;(void)n;}
void touch_feedback_edge_hint(int side,int distance,int y){(void)side;(void)distance;(void)y;}

static unsigned overlay_clicks;
static void overlay_event(lv_event_t *e){(void)e;++overlay_clicks;}
static void probe_main(void)
{
    unsigned before[16];memcpy(before,callbacks,sizeof(before));
    unsigned tx=protocol_calls,nav=pushes;
    page_01_detail_section_t section=page_01_detail_section_get();
    for(int y=20;y<400;y+=40)for(int x=20;x<1280;x+=40)tap(x,y);
    pointer(640,8,true);pointer(640,190,true);pointer(640,250,false);
    pointer(10,180,true);pointer(160,180,true);pointer(160,180,false);
    pointer(90,50,true);tick(1800);pointer(500,150,true);pointer(500,150,false);
    assert(!memcmp(before,callbacks,sizeof(before))&&tx==protocol_calls&&nav==pushes);
    assert(section==page_01_detail_section_get());
    assert(!page_01_main_quick_is_open()&&!editor.editing);
}
int main(void)
{
    lv_init();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    lv_img_decoder_t *decoder=lv_img_decoder_create();
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    static lv_indev_drv_t driver;lv_indev_drv_init(&driver);
    driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=pointer_read;driver.feedback_cb=pointer_feedback;
    g_pointer_indev=lv_indev_drv_register(&driver);assert(g_pointer_indev);
    gesture_service_init();fixture(false);assert(currency_state_confirm_active_code("USD"));
    ui_main_create(lv_scr_act());tick(400);
    unsigned nav=callbacks[CB_MENU];click_object(page_01_main_find_obj("menu_btn"));assert(callbacks[CB_MENU]==nav+1);
    /* Start arriving during a held button cancels the pending release. */
    lv_area_t a;lv_obj_get_coords(page_01_main_find_obj("menu_btn"),&a);
    int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
    pointer(x,y,true);nav=callbacks[CB_MENU];page_01_main_set_counting_locked(true);
    tick(1000);page_01_main_set_counting_locked(false);pointer(x,y,false);assert(callbacks[CB_MENU]==nav);
    click_object(page_01_main_find_obj("menu_btn"));assert(callbacks[CB_MENU]==nav+1);
    /* Pending layout long press and open quick drawer cannot outlive start. */
    pointer(90,50,true);page_01_main_set_counting_locked(true);tick(1800);
    assert(!editor.editing);pointer(90,50,false);probe_main();
    /* A modal/top-layer object cannot bypass the page gate. */
    lv_obj_t *overlay=lv_btn_create(lv_layer_top());lv_obj_set_size(overlay,400,120);
    lv_obj_center(overlay);lv_obj_add_event_cb(overlay,overlay_event,LV_EVENT_CLICKED,NULL);
    render();click_object(overlay);assert(!overlay_clicks);
    host_gestures=false;probe_main();click_object(overlay);assert(!overlay_clicks);
    page_01_main_set_counting_locked(false);click_object(overlay);assert(overlay_clicks==1);
    lv_obj_del(overlay);host_gestures=true;
    tap(640,8);tick(700);assert(page_01_main_quick_is_open());
    page_01_main_set_counting_locked(true);assert(!page_01_main_quick_is_open());probe_main();
    /* Repeated start, hidden/resume and reconstruction retain the gate. */
    page_01_main_set_counting_locked(true);page_01_main_suspend();
    assert(page_01_main_resume());tick(400);probe_main();
    ui_main_destroy();ui_main_create(lv_scr_act());tick(400);probe_main();
    /* Data/animation can still refresh while all touch is blocked. */
    page_01_main_refresh_totals(123,"123");render();assert(!strcmp(lv_label_get_text(s_total_pcs_label),"123"));
    page_01_main_set_counting_locked(false);nav=callbacks[CB_MENU];
    click_object(page_01_main_find_obj("menu_btn"));assert(callbacks[CB_MENU]==nav+1);
    tap(640,8);tick(700);assert(page_01_main_quick_is_open());
    ui_main_destroy();counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_indev_delete(g_pointer_indev);lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS real LVGL/Main/gesture/capture: full-screen clicks, swipes, hold cancellation, overlays, disabled gestures, quick/layout, retained/recreated lifecycle, live display, unlock");
}
