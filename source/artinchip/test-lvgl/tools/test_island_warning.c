/* Real LVGL and production island; exclude the unrelated display-test page. */
#include "lvgl/lvgl.h"
#define HOST_ISLAND_ONLY
#define main main_raster_entry_unused
#include "test_main_view.c"
#undef main

static void test_readable_warning(const char *message)
{
    smart_island_notify_warning(message);tick(300);render();
    assert(lv_obj_get_x(g_si_ctx.objects.title_clip)==36);
    assert(lv_obj_get_x(g_si_ctx.objects.title)==0);
    write_bmp("island-warning-start");
    tick(800);render();
    lv_coord_t from=lv_obj_get_x(g_si_ctx.objects.title);
    tick(500);render();
    lv_coord_t to=lv_obj_get_x(g_si_ctx.objects.title);
    assert(to<from && from-to<=25); /* <=45 px/s plus one 20ms frame. */
    smart_island_view_refresh_scene();render();
    assert(lv_obj_get_x(g_si_ctx.objects.title)==to);
    smart_island_notify_warning(message); /* Duplicate must not restart hold. */
    assert(lv_obj_get_x(g_si_ctx.objects.title)==to);
    write_bmp("island-warning-scrolling");
    tick(18000);render();
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(!g_si_ctx.warning.marquee_running);
}
static void test_preset_title_glyphs(void)
{
    ui_lang_set(LANGUAGE_CN);
    smart_island_notify_preset_full((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
    tick(300);render();
    const char *title=lv_label_get_text(g_si_ctx.objects.title);
    assert(!strcmp(title,"预置数已满"));
    const lv_font_t *font=lv_obj_get_style_text_font(g_si_ctx.objects.title,0);
    lv_font_glyph_dsc_t glyph;uint32_t offset=0,codepoint;
    while((codepoint=_lv_txt_encoded_next(title,&offset))!=0)
        assert(lv_font_get_glyph_dsc(font,&glyph,codepoint,0));
    tick(4000);ui_lang_set(LANGUAGE_EN);
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
    unsigned initial_timers=timers(), initial_posts=host_notice_posts, initial_tx=protocol_calls;
    test_preset_title_glyphs();
    test_readable_warning("Mode timed out. Retry Start mode in Menu.");
    test_readable_warning("A much longer diagnostic warning must remain readable to the end");
    smart_island_notify_warning("Mode timed out. Retry Start mode in Menu.");tick(1200);
    smart_island_notify_warning("USB ready");tick(100);
    assert(lv_label_get_long_mode(g_si_ctx.objects.title)==LV_LABEL_LONG_CLIP);
    page_01_main_suspend();tick(10000);assert(page_01_main_resume());tick(400);
    assert(g_si_ctx.view.scene==SMART_ISLAND_SCENE_IDLE);
    assert(timers()==initial_timers);
    smart_island_notify_warning("A long warning interrupted by destroying its host page");tick(1200);
    ui_main_destroy();tick(15000);
    assert(host_notice_posts==initial_posts && protocol_calls==initial_tx);
    counting_data_clear_serials(counting_data_mutable());counting_data_clear_errors(counting_data_mutable());
    lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS warning: initial hold, bounded pixel speed, clipped viewport, length-independent speed, refresh/duplicate, expiry, replacement, hide/resume, destroy");
}
