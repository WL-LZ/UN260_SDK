/* Production PURE page on real LVGL. Device data/navigation are test adapters. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "test_page_background_asset.h"
#include "src/misc/lv_txt_ap.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/lv_core/page_18_pure.c"

static ui_page_t current=UI_PAGE_MAIN;
static bool transitioning,reject_navigation,pure_preference,persisted_preference;
static unsigned saved,closed,start_clicked,clear_clicked;
static bool navigation_blocked,island_suspended;
static lv_obj_t *island,*quick_host;
static gesture_pointer_policy_t pointer_policy;
static counting_sim_t result={.total_amount=10692,.total_pcs=231};
const counting_sim_t *counting_data_current(void){return &result;}
int counting_data_reject_pcs_count(const counting_sim_t *s){(void)s;return 2;}
bool counting_data_monetary_result_supported(const counting_sim_t *s){(void)s;return true;}
bool currency_state_multi_selected(void){return false;}
ui_page_t ui_manager_get_current_page(void){return current;}
bool ui_manager_is_transitioning(void){return transitioning;}
void ui_manager_switch(ui_page_t target){if(reject_navigation)return;if(current==UI_PAGE_PURE)ui_page_18_pure_suspend();current=target;if(target==UI_PAGE_PURE){if(!ui_page_18_pure_resume())ui_page_18_pure_create(lv_scr_act());}}
void ui_state_save_pure_count_state(void){saved++;persisted_preference=pure_preference;}
void smart_island_close(void){closed++;}
void smart_island_set_pure_count_enabled(bool enabled){pure_preference=enabled;}
void smart_island_create(lv_obj_t *host){if(!island){island=lv_obj_create(host);lv_obj_remove_style_all(island);lv_obj_set_size(island,1,1);}else lv_obj_set_parent(island,host);island_suspended=false;}
void smart_island_set_suspended(bool suspended){island_suspended=suspended;}
bool smart_island_is_attached_to(lv_obj_t *host){return island&&lv_obj_is_valid(island)&&lv_obj_get_parent(island)==host;}
void smart_island_destroy(void){lv_obj_del(island);island=NULL;}
void page_01_main_quick_attach(lv_obj_t *host,uint32_t page){assert(page==UI_PAGE_PURE);quick_host=host;}
void page_01_main_quick_suspend(void){}
void page_01_main_quick_detach_host(lv_obj_t *host){if(quick_host==host)quick_host=NULL;}
bool page_01_main_quick_pointer(lv_indev_t *i,lv_event_code_t e,const lv_point_t *p,uint8_t n){(void)i;(void)e;(void)p;(void)n;return false;}
void gesture_service_set_pointer_policy(uint32_t owner,gesture_pointer_policy_t handler){assert(owner==UI_PAGE_PURE);pointer_policy=handler;}
void gesture_service_clear_pointer_policy(uint32_t owner){assert(owner==UI_PAGE_PURE);pointer_policy=NULL;}
void gesture_service_set_navigation_blocked(uint32_t owner,bool blocked){assert(owner==UI_PAGE_PURE);navigation_blocked=blocked;}
void page_01_start_btn_event_cb(lv_event_t *e){(void)e;start_clicked++;}
void page_01_esc_btn_event_cb(lv_event_t *e){(void)e;clear_clicked++;}
static unsigned icon_opens;
static lv_res_t asset_info(lv_img_decoder_t *decoder,const void *src,lv_img_header_t *header){
 (void)decoder;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;
 const un260_compiled_asset_t *a=test_page_asset_find(src);if(!a||!a->has_alpha)return LV_RES_INV;
 memset(header,0,sizeof(*header));header->w=a->width;header->h=a->height;header->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;
}
static lv_res_t asset_open(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *dsc){
 if(asset_info(decoder,dsc->src,&dsc->header)!=LV_RES_OK)return LV_RES_INV;
 dsc->img_data=test_page_asset_find(dsc->src)->pixels;
 if(!strcmp(dsc->src,"L:/usr/local/share/lvgl_data/pin_icons/close.png"))++icon_opens;
 return LV_RES_OK;
}
static void asset_close(lv_img_decoder_t *decoder,lv_img_decoder_dsc_t *dsc){(void)decoder;dsc->img_data=NULL;}
static void raster(const char *tag){
 const char *dir=getenv("PURE_RASTER_OUTPUT");if(!dir)return;
 lv_obj_update_layout(g_pure_page.page);
 lv_img_dsc_t *snap=lv_snapshot_take(g_pure_page.page,LV_IMG_CF_TRUE_COLOR_ALPHA);assert(snap);
 char name[2048];snprintf(name,sizeof(name),"%s/pure-%s.ppm",dir,tag);FILE *f=fopen(name,"wb");assert(f);
 fprintf(f,"P6\n%u %u\n255\n",snap->header.w,snap->header.h);
 for(unsigned i=0;i<snap->header.w*snap->header.h;i++){const uint8_t *p=snap->data+i*4;fputc(p[2],f);fputc(p[1],f);fputc(p[0],f);}
 fclose(f);lv_snapshot_free(snap);assert(icon_opens);
}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){(void)a;(void)p;lv_disp_flush_ready(d);}
int main(void){
 lv_init();ui_lang_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 lv_img_decoder_t *decoder=lv_img_decoder_create();assert(decoder);
 lv_img_decoder_set_info_cb(decoder,asset_info);lv_img_decoder_set_open_cb(decoder,asset_open);lv_img_decoder_set_close_cb(decoder,asset_close);
 static lv_color_t pixels[1280*20];lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*20);
 lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&buffer;driver.flush_cb=flush;lv_disp_drv_register(&driver);
 assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_PURE&&pure_preference&&persisted_preference&&saved==1);
 assert(navigation_blocked&&pointer_policy&&quick_host==g_pure_page.page);
 assert(!g_pure_page.refresh_timer->paused&&!island_suspended);
 assert(!strcmp(lv_label_get_text(g_pure_page.amount_value),"10,692"));
 assert(!strcmp(lv_label_get_text(g_pure_page.pcs_value),"231"));
 for(unsigned n=0;n<10;n++){
  ui_page_18_pure_suspend();assert(!navigation_blocked&&!pointer_policy&&g_pure_page.refresh_timer->paused&&island_suspended);
  assert(lv_obj_has_flag(g_pure_page.page,LV_OBJ_FLAG_HIDDEN));
  assert(ui_page_18_pure_resume());assert(navigation_blocked&&pointer_policy&&!g_pure_page.refresh_timer->paused&&!island_suspended);
 }
 transitioning=true;assert(!ui_page_18_pure_toggle_theme());assert(saved==1&&pure_preference);transitioning=false;
 reject_navigation=true;assert(!ui_page_18_pure_toggle_theme());assert(saved==1&&pure_preference&&current==UI_PAGE_PURE);reject_navigation=false;
 assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_MAIN&&!navigation_blocked&&!pure_preference&&!persisted_preference&&saved==2);
 /* Old gesture exits could leave this preference true on Main. Enter must
  * still navigate, and must not silently flip it off without entering. */
 pure_preference=true;assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_PURE&&pure_preference&&saved==3);
 pure_preference=false;assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_MAIN&&!pure_preference&&saved==4);
 assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_PURE&&saved==5);
 lv_obj_t *exit_button=lv_obj_get_parent(g_pure_page.exit_btn_icon);
 lv_obj_update_layout(g_pure_page.page);
 assert(lv_obj_check_type(g_pure_page.exit_btn_icon,&lv_img_class));
 assert(!strcmp(lv_img_get_src(g_pure_page.exit_btn_icon),"L:/usr/local/share/lvgl_data/pin_icons/close.png"));
 assert(!lv_obj_has_flag(g_pure_page.exit_btn_icon,LV_OBJ_FLAG_CLICKABLE));
 assert(lv_obj_get_child_cnt(exit_button)==1);
 assert(lv_obj_get_style_bg_opa(exit_button,0)==LV_OPA_TRANSP);
 assert(lv_obj_get_style_img_opa(g_pure_page.exit_btn_icon,0)==LV_OPA_50);
 lv_obj_add_state(exit_button,LV_STATE_PRESSED);lv_event_send(exit_button,LV_EVENT_PRESSED,NULL);
 lv_tick_inc(250);lv_timer_handler();assert(lv_obj_get_style_bg_opa(exit_button,0)==LV_OPA_60);
 lv_obj_clear_state(exit_button,LV_STATE_PRESSED);lv_event_send(exit_button,LV_EVENT_RELEASED,NULL);
 lv_tick_inc(250);lv_timer_handler();assert(lv_obj_get_style_bg_opa(exit_button,0)==LV_OPA_TRANSP);
 assert(lv_obj_get_x(exit_button)==1222&&lv_obj_get_y(exit_button)==8);
 assert(lv_obj_get_width(exit_button)==44&&lv_obj_get_height(exit_button)==42);
 assert(lv_obj_get_y(exit_button)+lv_obj_get_height(exit_button)<lv_obj_get_y(g_pure_page.amount_value));
 raster("en");
 for(size_t i=0;i<ui_lang_count();i++){
  const ui_locale_t *locale=ui_lang_at(i);if(!ui_lang_is_available(locale))continue;
  ui_page_18_pure_suspend();assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);assert(ui_page_18_pure_resume());
  char shaped[256];assert(_lv_txt_ap_calc_bytes_cnt(ui_text_get(UI_TEXT_WIDGET_PURE_CLEAR))<sizeof(shaped));
  _lv_txt_ap_proc(ui_text_get(UI_TEXT_WIDGET_PURE_CLEAR),shaped);
  assert(!strcmp(lv_label_get_text(g_pure_page.clear_btn_label),shaped));
  lv_obj_t *labels[]={g_pure_page.amount_label,g_pure_page.total_top_label,g_pure_page.value_label,
   g_pure_page.pcs_label,g_pure_page.total_bottom_label,g_pure_page.pieces_label,g_pure_page.reject_label,
   g_pure_page.start_btn_label,g_pure_page.clear_btn_label};
  for(unsigned n=0;n<sizeof(labels)/sizeof(labels[0]);n++){
   const lv_font_t *font=lv_obj_get_style_text_font(labels[n],0);const char *s=lv_label_get_text(labels[n]);
   uint32_t pos=0,code;while((code=_lv_txt_encoded_next(s,&pos))){lv_font_glyph_dsc_t glyph;bool found=lv_font_get_glyph_dsc(font,&glyph,code,0);if(!found)fprintf(stderr,"%s label %u missing U+%04X\n",locale->tag,n,code);assert(found);}
  }
  assert(lv_obj_get_style_text_font(g_pure_page.clear_btn_label,0)==ui_message_font_sized(&lv_font_rajdhani_27,28));
  assert(!strcmp(lv_img_get_src(g_pure_page.exit_btn_icon),"L:/usr/local/share/lvgl_data/pin_icons/close.png"));
  raster(locale->tag);
 }
 ui_lang_set(LANGUAGE_EN);
 transitioning=true;lv_event_send(exit_button,LV_EVENT_CLICKED,NULL);assert(current==UI_PAGE_PURE&&saved==5);transitioning=false;
 reject_navigation=true;lv_event_send(exit_button,LV_EVENT_CLICKED,NULL);assert(current==UI_PAGE_PURE&&pure_preference&&saved==5);reject_navigation=false;
 lv_event_send(exit_button,LV_EVENT_CLICKED,NULL);assert(current==UI_PAGE_MAIN&&!pure_preference&&!persisted_preference&&saved==6);
 lv_event_send(exit_button,LV_EVENT_CLICKED,NULL);assert(current==UI_PAGE_MAIN&&saved==6);
 assert(result.total_pcs==231&&result.total_amount==10692&&start_clicked==0&&clear_clicked==0);
 lv_obj_t *other=lv_obj_create(lv_scr_act());smart_island_create(other);ui_page_18_pure_destroy();
 assert(island&&lv_obj_is_valid(island)&&!quick_host&&!g_pure_page.page&&!g_pure_page.refresh_timer&&!pointer_policy&&!navigation_blocked);
 assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_PURE&&navigation_blocked);ui_page_18_pure_destroy();assert(!island&&!navigation_blocked);
 puts("PASS production PURE lifecycle: retained timers/policies, theme success/rejection, stale preference recovery, data retention, shared island ownership, destroy/recreate");
 return 0;
}
