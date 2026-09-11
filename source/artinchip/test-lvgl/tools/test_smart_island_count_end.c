/* Reuse the existing display/assets/peripheral harness; every island source
 * and app_counting_runtime_reset_session are compiled from production. */
#include "lvgl/lvgl.h"
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main
#include "un260/counting/counting_info_reply.h"
#include "un260/app_service/app_counting_runtime.h"

bool reset_allowed = true;
static bool fail_result_fade;
static unsigned injected_failures;
lv_anim_t *__real_lv_anim_start(const lv_anim_t *animation);
lv_anim_t *__wrap_lv_anim_start(const lv_anim_t *animation)
{
    if (fail_result_fade && animation->var == g_si_ctx.objects.counting_root) {
        fail_result_fade = false;
        ++injected_failures;
        return NULL;
    }
    return __real_lv_anim_start(animation);
}

static counting_session_state_t session;
static void start_session(void)
{
    memset(&session, 0, sizeof(session));
    session.start_confirmed = true;
    smart_island_notify_count_start();
    uint8_t live[13] = {0xFD,0xDF,13,0x0E,0,0,0,100,0,1,0,1,0};
    assert(counting_info_reply_handle(&session,counting_data_mutable(),live,sizeof(live),0).kind ==
           COUNTING_INFO_REPLY_LIVE);
    smart_island_update_counting(1,100);
    smart_island_notify_serial_number(100,"AB1234567890");
    tick(300);
    assert(session.phase == COUNTING_SESSION_ACTIVE);
    assert(g_si_ctx.lifecycle.count_session_active);
}

static void finish_session(void)
{
    uint8_t end[13] = {0xFD,0xDF,13,0x0E,0,0,0,100,0,1,0,2,0};
    assert(counting_info_reply_handle(&session,counting_data_mutable(),end,sizeof(end),0).kind ==
           COUNTING_INFO_REPLY_FINISHED);
    smart_island_notify_count_end(NULL);
}

static void assert_settled(void)
{
    tick(2500);
    assert(!g_si_ctx.lifecycle.count_session_active);
    assert(!g_si_ctx.lifecycle.result_transition_pending);
    assert(!g_si_ctx.counting.gate_anim_running);
    assert(g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE);
}

static void test_reset_lifecycle(void)
{
    const char *reasons[] = {"user clear", "currency change", "mode change", "boot finish"};
    for (unsigned i=0;i<sizeof(reasons)/sizeof(reasons[0]);++i) {
        start_session();
        assert(app_counting_runtime_reset_session(&session,reasons[i]));
        assert(!session.start_confirmed);
        assert(!g_si_ctx.lifecycle.count_session_active);
        assert(g_si_ctx.view.scene == SMART_ISLAND_SCENE_IDLE);
        /* A late transport end is intentionally rejected by the reset model;
         * the presentation must already have left COUNTING by this point. */
        uint8_t end[13] = {0xFD,0xDF,13,0x0E,0,0,0,100,0,1,0,2,0};
        assert(counting_info_reply_handle(&session,counting_data_mutable(),end,sizeof(end),0).kind ==
               COUNTING_INFO_REPLY_IGNORED);
        smart_island_refresh_summary();
        smart_island_notify_serial_number(100,"LATE123456789");
        smart_island_notify_count_end(NULL);
        assert_settled();
    }
    start_session();
    reset_allowed=false;
    assert(!app_counting_runtime_reset_session(&session,"history backpressure"));
    assert(session.start_confirmed && g_si_ctx.lifecycle.count_session_active);
    assert(g_si_ctx.view.scene == SMART_ISLAND_SCENE_COUNTING);
    reset_allowed=true;
    finish_session();assert_settled();

    start_session();
    page_01_main_suspend();
    assert(app_counting_runtime_reset_session(&session,"hidden clear"));
    assert(page_01_main_resume());
    assert_settled();

    start_session();
    smart_island_notify_warning("Pocket full");
    assert(g_si_ctx.warning.resume_counting);
    assert(app_counting_runtime_reset_session(&session,"warning clear"));
    assert(g_si_ctx.view.scene == SMART_ISLAND_SCENE_WARNING);
    assert(!strcmp(g_si_ctx.warning.text,"Pocket full"));
    assert(!g_si_ctx.lifecycle.count_session_active);
    assert_settled();

    /* A reset during the 140 ms result fade cancels its owner callback. */
    start_session();finish_session();tick(40);
    assert(app_counting_runtime_reset_session(&session,"ending clear"));
    start_session();tick(300);
    assert(g_si_ctx.lifecycle.count_session_active && g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
    finish_session();assert_settled();

    start_session();finish_session();tick(400);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_RESULT && g_si_ctx.lifecycle.result_timer);
    assert(app_counting_runtime_reset_session(&session,"result clear"));
    assert(g_si_ctx.lifecycle.result_timer==NULL);
    assert_settled();
    assert(lv_obj_get_width(g_si_ctx.objects.root)==SMART_ISLAND_WIDTH);

    start_session();smart_island_notify_update(25,"Updating");
    assert(app_counting_runtime_reset_session(&session,"update scene reset"));
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_UPDATE);
    assert(g_si_ctx.view.content.progress==25 && !g_si_ctx.lifecycle.count_session_active);
    smart_island_restore_idle();tick(400);
    start_session();smart_island_notify_qr("Report");
    assert(app_counting_runtime_reset_session(&session,"QR scene reset"));
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_QR && !g_si_ctx.lifecycle.count_session_active);
    smart_island_restore_idle();tick(400);
}

static void test_end_lifecycle(void)
{
    for (unsigned timing=0;timing<7;++timing) {
        start_session();
        if(timing==1)smart_island_notify_warning("Pocket full");
        if(timing==2)page_01_main_suspend();
        if(timing==3)smart_island_open_info_page();
        finish_session();
        if(timing==4) { tick(40);page_01_main_suspend(); }
        if(timing==5) { tick(40);smart_island_notify_warning("U DISK INSERTED"); }
        if(timing==6) {
            lv_obj_t *other=lv_obj_create(lv_scr_act());
            smart_island_create(other);tick(60);smart_island_create(main_page);lv_obj_del(other);
        }
        smart_island_refresh_summary();
        smart_island_notify_count_end(NULL); /* Duplicate end must not restart hold. */
        tick(240);
        if(timing==2 || timing==4)assert(page_01_main_resume());
        assert_settled();
    }
    start_session();
    fail_result_fade=true;
    finish_session();
    assert(injected_failures==1);
    assert(g_si_ctx.view.scene != SMART_ISLAND_SCENE_COUNTING);
    assert_settled();
    start_session();finish_session();tick(40);start_session();
    tick(1000);
    assert(g_si_ctx.lifecycle.count_session_active && g_si_ctx.view.scene==SMART_ISLAND_SCENE_COUNTING);
    finish_session();assert_settled();
}

int main(void)
{
    lv_init();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    fixture(false);assert(currency_state_confirm_active_code("USD"));ui_main_create(lv_scr_act());tick(400);
    test_reset_lifecycle();test_end_lifecycle();
    ui_main_destroy();
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS actual island count/reset lifecycle: normal, duplicate, hidden/resume, warning, reparent, history backpressure, reset/new start, animation-allocation failure");
}
