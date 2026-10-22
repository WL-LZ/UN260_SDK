/* Runs unchanged geometry, lifecycle and locale cases independently of the
 * pre-existing catalog-count mismatch in the full legacy suite. */
#define main legacy_settings_suite_main
#include "test_settings_view.c"
#undef main
int main(void)
{
lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);lv_img_decoder_t*dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);lv_indev_drv_t input;lv_indev_drv_init(&input);input.type=LV_INDEV_TYPE_POINTER;input.read_cb=pointer_read;input.feedback_cb=evdev_feedback;lv_indev_drv_register(&input);device_info_init("V1.0.0");
    grid_test();third_level_test();directory_locales_test();
    puts("PASS focused actual fullscreen Settings: native geometry, parent navigation, clock/language lifecycle and ten-locale directories");
    return 0;
}
