/* Production PURE page on real LVGL. Device data/navigation are test adapters. */
#include <assert.h>
#include <stdio.h>
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
language_t ui_lang_get(void){return LANGUAGE_EN;}
const char *ui_text_get(ui_text_id_t id){(void)id;return "Label";}
ui_page_t ui_manager_get_current_page(void){return current;}
bool ui_manager_is_transitioning(void){return transitioning;}
void ui_manager_switch(ui_page_t target){if(reject_navigation)return;if(current==UI_PAGE_PURE)ui_page_18_pure_suspend();current=target;if(target==UI_PAGE_PURE){if(!ui_page_18_pure_resume())ui_page_18_pure_create(lv_scr_act());}}
void ui_state_save_pure_count_state(void){saved++;persisted_preference=pure_preference;}
void smart_island_close(void){closed++;}
void smart_island_set_pure_count_enabled(bool enabled){pure_preference=enabled;}
void smart_island_create(lv_obj_t *host){if(!island)island=lv_obj_create(host);else lv_obj_set_parent(island,host);island_suspended=false;}
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
void lv_damped_button_register(lv_obj_t *o,lv_color_t a,lv_color_t b){(void)o;(void)a;(void)b;}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){(void)a;(void)p;lv_disp_flush_ready(d);}
int main(void){
 lv_init();static lv_color_t pixels[1280*20];lv_disp_draw_buf_t buffer;lv_disp_draw_buf_init(&buffer,pixels,NULL,1280*20);
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
 assert(result.total_pcs==231&&result.total_amount==10692&&start_clicked==0&&clear_clicked==0);
 lv_obj_t *other=lv_obj_create(lv_scr_act());smart_island_create(other);ui_page_18_pure_destroy();
 assert(island&&lv_obj_is_valid(island)&&!quick_host&&!g_pure_page.page&&!g_pure_page.refresh_timer&&!pointer_policy&&!navigation_blocked);
 assert(ui_page_18_pure_toggle_theme());assert(current==UI_PAGE_PURE&&navigation_blocked);ui_page_18_pure_destroy();assert(!island&&!navigation_blocked);
 puts("PASS production PURE lifecycle: retained timers/policies, theme success/rejection, stale preference recovery, data retention, shared island ownership, destroy/recreate");
 return 0;
}
