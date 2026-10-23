/* Actual settings rendering, collection navigation and lifecycle. */
#define main legacy_settings_suite_main
#include "test_settings_view.c"
#undef main

int main(void)
{
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);
 dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t *dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);
 device_info_init("V1.0.0");
 for(unsigned cycle=0;cycle<3;cycle++){
  ui_page_06_settings_create(lv_scr_act());assert(!diagnostic_scope);
  page_06_settings_open_collection();assert(page_06_settings_is_collection()&&diagnostic_scope);
  assert(label_find(view,"RUN")&&label_find(view,"All notes")&&label_find(view,"End collection"));
  unsigned before=sends;assert(page_06_settings_back_sub_page());assert(!diagnostic_scope&&sends==before);
  assert(settings_page&&scope_is("data"));
  page_06_settings_open_collection();assert(diagnostic_scope);ui_page_06_settings_reset_navigation();
  assert(!diagnostic_scope&&scope_is("device"));
  ui_page_06_settings_destroy();
 }
 ui_page_06_settings_create(lv_scr_act());page_06_settings_open_collection();assert(diagnostic_scope);
 ui_page_06_settings_suspend();current_page=UI_PAGE_MAIN;diagnostic_scope=false;
 assert(page_06_settings_is_collection());work_mode_service_set_diagnostic(page_06_settings_is_collection());
 assert(ui_page_06_settings_resume());current_page=UI_PAGE_SETTING;assert(diagnostic_scope&&label_find(view,"RUN"));
 assert(page_06_settings_back_sub_page());assert(!diagnostic_scope);
 ui_page_06_settings_destroy();
 puts("PASS collection entry/Back, repeated lifecycle, Home reset and child return preserve diagnostic ownership without extra protocol sends");
 lv_deinit();return 0;
}
