/* Production island must forward notices without owning their visual lifetime. */
#include "lvgl/lvgl.h"
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main

static void test_notice_adapter(void)
{
    unsigned initial_timers=timers(),posts=host_notice_posts,tx=protocol_calls;
    smart_island_notify_warning("Mode timed out. Retry Start mode in Menu.");
    assert(host_notice_posts==posts+1 && host_notice_kind==UI_NOTICE_WARNING);
    assert(!strcmp(host_notice_detail,"Mode timed out. Retry Start mode in Menu."));
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && timers()==initial_timers);
    smart_island_notify_warning_level("Export failed",SMART_ISLAND_WARNING_LEVEL_ERROR);
    assert(host_notice_posts==posts+2 && host_notice_kind==UI_NOTICE_ERROR);
    assert(!strcmp(host_notice_detail,"Export failed"));
    smart_island_notify_warning_level("Invalid",(smart_island_warning_level_t)99);
    assert(host_notice_posts==posts+2);
    smart_island_notify_warning(NULL);
    assert(host_notice_posts==posts+3 && host_notice_detail[0]);
    assert(protocol_calls==tx); /* A notice never acknowledges machine hardware. */
    tick(18000);render();assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(timers()==initial_timers);
    page_01_main_suspend();
    smart_island_notify_warning("Hidden page notice");
    assert(host_notice_posts==posts+4 && !strcmp(host_notice_detail,"Hidden page notice"));
    tick(1000);assert(page_01_main_resume());tick(400);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE && timers()==initial_timers);
    host_fault_pending=true;smart_island_refresh_summary();render();
    assert(!strcmp(lv_label_get_text(g_si_ctx.objects.title),"Machine issue"));
    write_bmp("island-unresolved-fault");
    host_fault_pending=false;smart_island_refresh_summary();render();
    assert(strcmp(lv_label_get_text(g_si_ctx.objects.title),"Machine issue"));
}

int main(void)
{
    if (getenv("ISLAND_RUN_FULL_MAIN")) return main_raster_entry_unused();
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
    test_notice_adapter();
    ui_main_destroy();
    unsigned posts=host_notice_posts;
    smart_island_notify_warning("Notice without Main");
    assert(host_notice_posts==posts+1 && !strcmp(host_notice_detail,"Notice without Main"));
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS notice adapter: type/content, no scene/timer/protocol mutation, hidden/destroyed Main, persistent unresolved fault");
}
