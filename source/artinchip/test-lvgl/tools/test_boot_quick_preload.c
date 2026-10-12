/* Real Main, shared Main/Pure drawer and LVGL snapshot. No board claims. */
#include "lvgl/lvgl.h"
#define main shared_main_view_regression_main
#include "test_main_view.c"
#undef main

int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);lv_init();
    static lv_color_t pixels[1280*40];static lv_disp_draw_buf_t buffer;
    lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*40);
    static lv_disp_drv_t display;lv_disp_drv_init(&display);
    display.hor_res=1280;display.ver_res=400;display.draw_buf=&buffer;display.flush_cb=flush;
    assert(lv_disp_drv_register(&display));ui_scrollbar_init(lv_disp_get_default());
    lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
    lv_img_decoder_set_info_cb(decoder,host_image_info);lv_img_decoder_set_open_cb(decoder,host_image_open);
    lv_img_decoder_set_close_cb(decoder,host_image_close);
    static lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;
    driver.read_cb=pointer_read;driver.feedback_cb=pointer_feedback;
    g_pointer_indev=lv_indev_drv_register(&driver);assert(g_pointer_indev);
    extern unsigned host_quick_captures;
    host_gestures=true;page_01_main_layout_set_enabled(false);
    for(unsigned cycle=0;cycle<3;++cycle){
        fixture(false);assert(currency_state_confirm_active_code("USD"));
        host_current_page=UI_PAGE_BOOT;prewarming=true;ui_main_create(lv_scr_act());prewarming=false;
        assert(!quick.root&&quick.timer->paused);
        uint64_t started=app_clock_monotonic_us();
        assert(page_01_main_quick_prepare_static_step());
        uint32_t build_us=app_clock_elapsed_us32(started,app_clock_monotonic_us());
        lv_obj_t *built=quick.root;
        assert(built&&!page_01_main_quick_prepare_static_step()&&!quick.prepared);
        page_01_main_suspend();assert(!page_01_main_quick_prepare_static_step());
        host_current_page=UI_PAGE_MAIN;assert(page_01_main_resume());render();
        assert(quick.root==built);
        host_transitioning=true;page_01_main_quick_schedule_preload();
        unsigned captures=host_quick_captures;
        started=app_clock_monotonic_us();
        bool prepared=page_01_main_quick_prepare_preload();
        uint32_t capture_us=app_clock_elapsed_us32(started,app_clock_monotonic_us());
#ifdef HOST_SKIN_FALLBACK
        assert(!prepared&&!quick.prepared&&host_quick_captures==captures);
#else
        assert(prepared&&quick.prepared&&host_quick_captures==captures+1);
#endif
        assert(quick.root==built&&quick.timer->paused&&!lv_obj_is_visible(quick.root));
        captures=host_quick_captures;render();unsigned decoded=asset_opens;
        tick(1200);
        assert(quick.root==built&&quick.timer->paused&&host_quick_captures==captures);
        assert(asset_opens==decoded);
        printf("PREPARED cycle=%u: static tree=%u us, opaque-cover snapshot=%u us; no timer/capture/image work during 1200 ms handoff window\n",cycle,build_us,capture_us);
        host_transitioning=false;quick_open_test();quick_close_test();
        assert(host_quick_captures==captures); /* manual pull reuses success; failure stays live */
        page_01_main_suspend();assert(!page_01_main_quick_prepare_preload());
        lv_obj_t *pure_host=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(pure_host);
        lv_obj_set_size(pure_host,1280,400);
        page_01_main_quick_attach(pure_host,UI_PAGE_PURE);host_current_page=UI_PAGE_PURE;
        lv_obj_update_layout(lv_scr_act());
        assert(quick.root==built&&!page_01_main_quick_prepare_static_step());
        host_transitioning=true;
        prepared=page_01_main_quick_prepare_preload();
#ifdef HOST_SKIN_FALLBACK
        assert(!prepared);
#else
        assert(prepared);
#endif
        assert(!strcmp(lv_label_get_text(quick.page_label),ui_tr("Pure"))&&quick.timer->paused);
        captures=host_quick_captures;tick(1200);assert(host_quick_captures==captures);
        host_transitioning=false;qc_settle(true);tick(240);assert(quick.active);qc_close_now();
        page_01_main_quick_detach_host(pure_host);lv_obj_del(pure_host);ui_main_destroy();
        assert(!quick.root&&!quick.timer&&!quick.surface.snapshot);
    }
#ifdef HOST_PAGER_FAILURE_INJECTION
    fixture(false);host_current_page=UI_PAGE_MAIN;ui_main_create(lv_scr_act());
    host_pager_create_fails=true;
    assert(!page_01_main_quick_prepare_static_step()&&!quick.root&&quick.preload_failed);
    assert(!host_pager_create_fails);
    host_pager_create_fails=true;
    assert(!page_01_main_quick_prepare_static_step());
    page_01_main_quick_schedule_preload();assert(!page_01_main_quick_prepare_preload());
    tick(1500);assert(host_pager_create_fails&&!quick.root&&quick.timer->paused);
    host_pager_create_fails=false;quick_open_test();quick_close_test();
    assert(quick.root&&!quick.preload_failed);ui_main_destroy();
    puts("PASS quick preload allocation failure: no automatic retry or wait; fresh manual pull rebuilds successfully");
#endif
    ui_lang_set_resource_preparer(ui_locale_font_prepare);
    ui_message_font_set_provider(ui_locale_font_compose);
    for(size_t i=0;i<ui_lang_count();++i){
        const ui_locale_t *locale=ui_lang_at(i);
        assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
        fixture(true);assert(currency_state_confirm_active_code("USD"));
        host_current_page=UI_PAGE_MAIN;ui_main_create(lv_scr_act());render();
        assert(page_01_main_quick_prepare_static_step());
        (void)page_01_main_quick_prepare_preload();
        const char *unit=ui_tr("pcs");
        if(locale->id!=LANGUAGE_EN)assert(strcmp(unit,"pcs"));
        char expected[48];snprintf(expected,sizeof(expected),"USD   |   %d %s",counting_data_current()->total_pcs,unit);
        lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,expected);
        assert(!strcmp(lv_label_get_text(quick.context),lv_label_get_text(probe)));lv_obj_del(probe);
        const lv_font_t *font=lv_obj_get_style_text_font(quick.context,0);
        assert(font==ui_message_font(&lv_font_instrument_sans_medium_13));
        const char *shaped=lv_label_get_text(quick.context);uint32_t offset=0;
        while(shaped[offset]){
            uint32_t cp=_lv_txt_encoded_next(shaped,&offset);lv_font_glyph_dsc_t glyph;
            if(cp<=32)continue;
            assert(lv_font_get_glyph_dsc(font,&glyph,cp,0)&&!glyph.is_placeholder);
            if(glyph.box_w&&glyph.box_h)assert(lv_font_get_glyph_bitmap(glyph.resolved_font?glyph.resolved_font:font,cp));
        }
        quick_open_test();check_single_line(quick.context);render();
        char name[80];snprintf(name,sizeof(name),"quick-context-%s",locale->tag);write_bmp(name);
        quick_close_test();ui_main_destroy();
        printf("QUICK_CONTEXT locale=%s unit=%s count=%d: actual localized label, current font glyphs and single line PASS\n",locale->tag,unit,counting_data_current()->total_pcs);
    }
    ui_lang_set(LANGUAGE_EN);
    lv_indev_delete(g_pointer_indev);lv_img_decoder_delete(decoder);lv_deinit();host_external_assets_release();
    puts("PASS boot quick preload: static retained tree reused, MAIN/PURE target, no work during motion, optional snapshot fallback, manual pull, hide/destroy/recreate");
    return 0;
}
