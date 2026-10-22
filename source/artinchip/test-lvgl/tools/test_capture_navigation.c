/* Actual settings facade/render/lifecycle; the Panel root is a navigation boundary. */
#define main legacy_settings_suite_main
#include "test_settings_view.c"
#undef main

static bool panel_mode,panel_allowed=true;
static lv_obj_t *panel_fixture_root;
static unsigned panel_category;
ui_display_mode_t ui_display_mode_get(void){return panel_mode?UI_DISPLAY_MODE_PANEL:UI_DISPLAY_MODE_FULLSCREEN;}
bool ui_display_mode_is_panel(void){return panel_mode;}
bool ui_display_mode_set(ui_display_mode_t mode){panel_mode=mode==UI_DISPLAY_MODE_PANEL;return true;}
bool panel_pages_create(ui_page_t page,lv_obj_t *parent){assert(page==UI_PAGE_SETTING);if(!panel_allowed)return false;panel_fixture_root=lv_obj_create(parent);return true;}
bool panel_pages_created(ui_page_t page){assert(page==UI_PAGE_SETTING);return panel_fixture_root!=NULL;}
bool panel_pages_visible(ui_page_t page){assert(page==UI_PAGE_SETTING);return panel_fixture_root&&lv_obj_is_visible(panel_fixture_root);}
void panel_pages_destroy(ui_page_t page){assert(page==UI_PAGE_SETTING);if(panel_fixture_root)lv_obj_del(panel_fixture_root);panel_fixture_root=NULL;}
bool panel_pages_resume(ui_page_t page){assert(page==UI_PAGE_SETTING);if(!panel_fixture_root)return false;lv_obj_clear_flag(panel_fixture_root,LV_OBJ_FLAG_HIDDEN);return true;}
void panel_pages_suspend(ui_page_t page){assert(page==UI_PAGE_SETTING);if(panel_fixture_root)lv_obj_add_flag(panel_fixture_root,LV_OBJ_FLAG_HIDDEN);}
void panel_pages_refresh(ui_page_t page,bool reset){assert(page==UI_PAGE_SETTING);(void)reset;}
bool panel_pages_settings_category(unsigned category){panel_category=category;return panel_fixture_root!=NULL;}

int main(void)
{
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);
 dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 lv_img_decoder_t *dec=lv_img_decoder_create();lv_img_decoder_set_info_cb(dec,info);lv_img_decoder_set_open_cb(dec,image_open);
 device_info_init("V1.0.0");
 for(unsigned panel=0;panel<2;panel++){
  panel_mode=panel;ui_page_06_settings_create(lv_scr_act());assert(!diagnostic_scope);
  for(unsigned cycle=0;cycle<3;cycle++){
   page_06_settings_open_collection();assert(page_06_settings_is_collection()&&diagnostic_scope&&!panel_fixture_root);
   assert(label_find(view,"RUN")&&label_find(view,"All notes")&&label_find(view,"End collection"));
   unsigned before=sends;assert(page_06_settings_back_sub_page());assert(!diagnostic_scope&&sends==before);
   if(panel)assert(panel_fixture_root&&!settings_page&&panel_category==3);
   else assert(settings_page&&scope_is("data"));
  }
  page_06_settings_open_collection();assert(diagnostic_scope);ui_page_06_settings_reset_navigation();
  if(panel){
   assert(panel_rebuild_pending);current_page=UI_PAGE_MAIN;diagnostic_scope=false;
   /* The real manager resolves diagnostic ownership before resume/create. */
   assert(!page_06_settings_is_collection());work_mode_service_set_diagnostic(page_06_settings_is_collection());
   assert(ui_page_06_settings_resume());current_page=UI_PAGE_SETTING;assert(panel_fixture_root&&!diagnostic_scope);
   page_06_settings_open_collection();assert(diagnostic_scope);ui_page_06_settings_suspend();ui_page_06_settings_destroy();
   current_page=UI_PAGE_MAIN;diagnostic_scope=false;assert(!page_06_settings_is_collection());
   work_mode_service_set_diagnostic(page_06_settings_is_collection());ui_page_06_settings_create(lv_scr_act());
   current_page=UI_PAGE_SETTING;assert(panel_fixture_root&&!diagnostic_scope);
  }
  else assert(!diagnostic_scope&&scope_is("device"));
  ui_page_06_settings_destroy();
 }
 panel_mode=true;panel_allowed=true;ui_page_06_settings_create(lv_scr_act());page_06_settings_open_collection();assert(diagnostic_scope);
 panel_allowed=false;assert(page_06_settings_back_sub_page());assert(!diagnostic_scope&&settings_page&&!panel_fixture_root&&scope_is("data"));
 page_06_settings_open_collection();assert(settings_page&&!panel_detail&&diagnostic_scope&&page_06_settings_is_collection());
 ui_page_06_settings_suspend();current_page=UI_PAGE_MAIN;diagnostic_scope=false;
 assert(page_06_settings_is_collection());work_mode_service_set_diagnostic(page_06_settings_is_collection());
 assert(ui_page_06_settings_resume());current_page=UI_PAGE_SETTING;assert(diagnostic_scope&&label_find(view,"RUN"));
 assert(page_06_settings_back_sub_page());assert(!diagnostic_scope);
 ui_page_06_settings_destroy();
 puts("PASS actual capture navigation: fullscreen and Panel entry/Back, repeated lifecycle, Home/reentry diagnostic query before resume, invalidated detail recreation, Panel allocation fallback release diagnostic scope without C0 or RUN");
 lv_deinit();return 0;
}
