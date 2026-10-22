/* Real production pages/query/fonts; only peripheral and route endpoints mocked. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_core/panel_pages.c"
#include "un260/font/ui_message_font.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/counting/counting_data_store_internal.h"
#include "src/misc/lv_txt_ap.h"
#include "panel_view_checks.h"
static bool busy,applying,send_ok=true,sound=true;
static unsigned requests,notices,requested_speed,requested_mode,guide_requests;
static int backlight=80;
static ui_page_t routed;
static unsigned menu_category,menu_subcategory,gesture_guides;
static ui_page_t pin_destination;
static workspace_model_t workspace;
static standby_config_t standby={.minutes=5};
/* Test-only population of the real service store; production pages remain read-only. */
#define multi (*(counting_multi_t *)(void *)counting_multi_current())
static multi_serial_cache_t serial_cache[2];
static multi_serial_t multi_rows[2][8];
static multi_reject_cache_t rejects;
static uint32_t extra_revision=1;
static unsigned reject_requests,serial_requests,key_overflows;
static bool extra_busy;
static lv_color_t pixels[1280*400],draw_pixels[1280*40];
static const char *output;
static settings_detail_dialog_cb_t dialog_confirm;
static void *dialog_context;
uint8_t lv_port_indev_touch_count(void){return 1;}
bool app_command_runtime_count_start_busy(void){return busy;}
bool workspace_service_applying(void){return applying;}
bool setting_service_request_speed(uint8_t value){requests++;requested_speed=value;return send_ok;}
bool setting_service_request_mode(uint8_t value){requests++;requested_mode=value;return send_ok;}
bool setting_service_request_batch_switch(bool enabled,uint8_t value,bool old_enabled,uint8_t old){(void)enabled;(void)value;(void)old_enabled;(void)old;requests++;return send_ok;}
bool setting_service_request_fo_mode(uint8_t value){(void)value;requests++;return send_ok;}
bool setting_service_request_beep(bool value){(void)value;requests++;return send_ok;}
bool user_cfg_touch_sound_enabled(void){return sound;}
bool app_local_setting_apply(app_local_setting_t setting,uint8_t a,uint8_t b){(void)b;assert(setting==APP_LOCAL_TOUCH_SOUND);if(send_ok)sound=a;return send_ok;}
int backlight_service_max(void){return 100;}
int backlight_service_level(void){return backlight;}
bool backlight_service_set(int level){backlight=level;return true;}
bool backlight_service_save(void){return send_ok;}
const standby_config_t *standby_config(void){return &standby;}
bool standby_store_save(const standby_config_t *cfg){if(send_ok)standby=*cfg;return send_ok;}
const char *device_info_display_app(void){return "1.0.0";}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
int protocol_send(uint8_t command,const uint8_t *data,uint16_t size){(void)command;(void)data;return send_ok?size:-1;}
const multi_serial_cache_t *counting_multi_serials(unsigned index){return index<2?&serial_cache[index]:NULL;}
uint32_t counting_multi_extra_revision(void){return extra_revision;}
const multi_reject_cache_t *counting_multi_rejects(void){return &rejects;}
bool counting_multi_extra_busy(void){return extra_busy;}
bool counting_multi_reject_request(uint32_t now){(void)now;reject_requests++;rejects.status=send_ok?MULTI_DETAIL_LOADING:MULTI_DETAIL_INVALID;extra_revision++;return send_ok;}
bool counting_multi_serial_request(unsigned index,uint32_t now){(void)now;assert(index<2);assert(!extra_busy&&!counting_multi_query_busy());serial_requests++;serial_cache[index].status=send_ok?MULTI_DETAIL_LOADING:MULTI_DETAIL_INVALID;extra_busy=send_ok;extra_revision++;return send_ok;}
void ui_notice_post_text(ui_notice_kind_t kind,const char *key,const char *title,const char *detail){(void)kind;(void)key;(void)title;(void)detail;notices++;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;guide_requests++;return true;}
void page_01_action_run(page_01_action_t action){assert(action==PAGE_01_ACTION_PRINT);requests++;}
bool ui_export_data_request(void){return send_ok;}
const workspace_model_t *workspace_store_get(void){return &workspace;}
const workspace_user_t *workspace_active(const workspace_model_t *model){return &model->shared;}
void counting_history_current_invalidate(void){}
void ui_manager_switch(ui_page_t page){routed=page;}
void ui_manager_push_page(ui_page_t page){routed=page;}
void ui_manager_clear_stack(void){}
void ui_page_05_set_password_open(void){routed=UI_PAGE_SET_PASSAGE;}
bool ui_page_05_set_password_open_for(ui_page_t destination){pin_destination=destination;routed=UI_PAGE_SET_PASSAGE;return true;}
void gesture_guide_show(void){gesture_guides++;}
void ui_page_19_history_open_all(void){routed=UI_PAGE_HISTORY;}
void page_03_menu_open_detail(unsigned category,unsigned subcategory){menu_category=category;menu_subcategory=subcategory;routed=UI_PAGE_MENU;}
void page_06_settings_open_detail(const char *id){(void)id;routed=UI_PAGE_SETTING;}
bool settings_detail_dialog_show(const char *title,const char *body,const char *confirm,const char *cancel,settings_detail_dialog_cb_t yes,settings_detail_dialog_cb_t no,void *user){(void)title;(void)body;(void)confirm;(void)cancel;(void)no;dialog_confirm=yes;dialog_context=user;return true;}
static void flush(lv_disp_drv_t *driver,const lv_area_t *area,lv_color_t *source){for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++){if(x>=0&&x<1280&&y>=0&&y<400)pixels[y*1280+x]=*source;source++;}lv_disp_flush_ready(driver);}
static void advance(unsigned ms){for(unsigned i=0;i<ms;i+=20){lv_tick_inc(20);lv_timer_handler();ui_frame_commit_flush();}}
static void capture(const char *name){lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);for(unsigned i=0;i<3;i++)if(visible(pages[i])){panel_test_layout(pages[i]->panel.root,false);panel_test_no_corner_graphics(pages[i]->panel.root);}char path[1024];snprintf(path,sizeof(path),"%s/%s.bmp",output,name);FILE *f=fopen(path,"wb");assert(f);unsigned char h[54]={0};h[0]='B';h[1]='M';uint32_t size=54+1280*400*4,offset=54,dib=40,w=1280;int32_t height=-400;uint16_t planes=1,bits=32;memcpy(h+2,&size,4);memcpy(h+10,&offset,4);memcpy(h+14,&dib,4);memcpy(h+18,&w,4);memcpy(h+22,&height,4);memcpy(h+26,&planes,2);memcpy(h+28,&bits,2);assert(fwrite(h,1,54,f)==54);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f);}
static lv_obj_t *find_text(lv_obj_t *o,const char *value){if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return NULL;if(lv_obj_check_type(o,&lv_label_class)){char shaped[512];assert(_lv_txt_ap_calc_bytes_cnt(value)<sizeof(shaped));_lv_txt_ap_proc(value,shaped);if(!strcmp(lv_label_get_text(o),shaped))return o;}for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *found=find_text(lv_obj_get_child(o,i),value);if(found)return found;}return NULL;}
static void key_bounds(panel_page_t *p){
 for(unsigned side=0;side<2;side++){
  lv_obj_t *wing=lv_obj_get_child(p->panel.root,side);
  for(unsigned i=0;i<lv_obj_get_child_cnt(wing);i++){
   lv_obj_t *button=lv_obj_get_child(wing,i);if(!lv_obj_check_type(button,&lv_btn_class)||lv_obj_has_flag(button,LV_OBJ_FLAG_HIDDEN))continue;
   lv_obj_t *body=lv_obj_get_child(button,0);lv_area_t ba;lv_obj_get_coords(button,&ba);
   for(unsigned j=0;j<lv_obj_get_child_cnt(body);j++){
    lv_obj_t *caption=lv_obj_get_child(body,j);if(!lv_obj_check_type(caption,&lv_label_class)||lv_obj_has_flag(caption,LV_OBJ_FLAG_HIDDEN))continue;
    lv_area_t ca;lv_obj_get_coords(caption,&ca);bool fits=ca.x1>ba.x1&&ca.x2<ba.x2&&ca.y1>ba.y1&&ca.y2<ba.y2;
    if(!fits)fprintf(stderr,"Key clipping: locale=%u text=%s button=(%d,%d,%d,%d) label=(%d,%d,%d,%d)\n",ui_lang_get(),lv_label_get_text(caption),ba.x1,ba.y1,ba.x2,ba.y2,ca.x1,ca.y1,ca.x2,ca.y2);
    if(!fits)key_overflows++;assert(lv_color_to32(lv_obj_get_style_text_color(caption,0))==lv_color_to32(lv_color_hex(0xffffff)));assert(lv_obj_get_style_text_opa(caption,0)==255);
   }
  }
 }
}
static void regions(panel_page_t *p){lv_obj_update_layout(p->panel.root);assert(lv_obj_get_x(p->panel.content)==244&&lv_obj_get_width(p->panel.content)==792&&lv_obj_get_height(p->panel.content)==400);assert(lv_obj_get_width(p->panel.root)==1280&&lv_obj_get_height(p->panel.root)==400);key_bounds(p);}
static void filter_key_bounds(panel_page_t *p)
{
 lv_obj_update_layout(p->panel.root);lv_obj_t *base=lv_obj_get_child(p->list.filters,0);
 assert(lv_obj_get_x(base)==28&&lv_obj_get_y(base)==70&&lv_obj_get_width(base)==143&&lv_obj_get_height(base)==36);
 const int x[2]={3,65},width[2]={59,74};
 for(unsigned i=0;i<2;i++){
  lv_obj_t *button=lv_obj_get_child(base,i),*label=lv_obj_get_child(button,0);
  assert(lv_obj_get_x(button)==x[i]&&lv_obj_get_y(button)==3&&lv_obj_get_width(button)==width[i]&&lv_obj_get_height(button)==29);
  const lv_font_t *font=lv_obj_get_style_text_font(label,0);assert(font==ui_panel_font(12,false));
  lv_point_t natural;lv_txt_get_size(&natural,lv_label_get_text(label),font,lv_obj_get_style_text_letter_space(label,0),lv_obj_get_style_text_line_space(label,0),LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  lv_area_t ba,la;lv_obj_get_coords(button,&ba);lv_obj_get_coords(label,&la);
  int text_left=la.x1+(lv_obj_get_width(label)-natural.x)/2,text_right=text_left+natural.x-1;
  bool fits=!strchr(lv_label_get_text(label),'\n')&&natural.x<=width[i]-2&&lv_obj_get_height(label)<=font->line_height&&text_left>ba.x1&&text_right<ba.x2&&la.y1>ba.y1&&la.y2<ba.y2;
  if(!fits)fprintf(stderr,"Filter clipping/wrapping: locale=%u text=%s natural=%d label-height=%d line-height=%u key-width=%d\n",ui_lang_get(),lv_label_get_text(label),natural.x,lv_obj_get_height(label),font->line_height,width[i]);
  assert(fits);
 }
}
static void fixture(void){
 counting_sim_t *d=counting_data_mutable();d->total_pcs=12;d->total_amount=310;d->denom_number=5;
 int values[12]={10,20,10,5,50,20,10,100,5,20,50,10};const char *codes[12]={"AA0001","BB0002","AA0003","CC0004","BB0005","AA0006","DD0007","AA0008","CC0009","DD0010","AA0011","BB0012"};
 assert(counting_data_ensure_serial_capacity(d,12));for(int i=0;i<12;i++){d->sn_str[i]=strdup(codes[i]);d->denom_mix[i]=values[i];}
 d->denom[0]=(denom_t){100,1,100};d->denom[1]=(denom_t){50,2,100};d->denom[2]=(denom_t){20,3,60};d->denom[3]=(denom_t){10,4,40};d->denom[4]=(denom_t){5,2,10};
 currency_state_reset();currency_state_begin_list_sync();assert(currency_state_append_list_code(1,"USD"));assert(currency_state_append_list_code(2,"EUR"));assert(currency_state_finish_list_sync());assert(currency_state_confirm_active_selection(2,"USD"));machine_state_confirm_mode(MODE_MDC);machine_state_confirm_speed(1);machine_state_confirm_batch(true,199);workspace.shared.id=1;
 multi.count=2;multi.total_pcs=6;multi.currencies[0]=(multi_currency_t){.code="USD",.pcs=4,.amount=60,.status=MULTI_DETAIL_READY,.denom_count=2,.denom={{10,2},{20,2}}};multi.currencies[1]=(multi_currency_t){.code="EUR",.pcs=2,.amount=50,.status=MULTI_DETAIL_READY,.denom_count=2,.denom={{20,1},{30,1}}};
 multi_rows[0][0]=(multi_serial_t){.number=2,.value=10,.text="AA02"};multi_rows[0][1]=(multi_serial_t){.number=5,.value=20,.text="BB05"};multi_rows[0][2]=(multi_serial_t){.number=9,.value=20,.text="AA09"};multi_rows[0][3]=(multi_serial_t){.number=12,.value=10,.text="CC12"};serial_cache[0]=(multi_serial_cache_t){.status=MULTI_DETAIL_READY,.count=4,.rows=multi_rows[0]};
 multi_rows[1][0]=(multi_serial_t){.number=3,.value=20,.text="AA03"};multi_rows[1][1]=(multi_serial_t){.number=8,.value=30,.text="DD08"};serial_cache[1]=(multi_serial_cache_t){.status=MULTI_DETAIL_READY,.count=2,.rows=multi_rows[1]};
}
static void visual_fixture(void){
 counting_sim_t *d=counting_data_mutable();counting_data_clear_serials(d);assert(counting_data_ensure_serial_capacity(d,178));
 d->total_pcs=178;d->total_amount=12580;d->denom_number=4;d->denom[0]=(denom_t){100,100,10000};d->denom[1]=(denom_t){50,40,2000};d->denom[2]=(denom_t){20,20,400};d->denom[3]=(denom_t){10,18,180};
 unsigned position=0;for(unsigned order=0;order<179;order++)for(unsigned original=0;original<178;original++)if((original*73)%179==order){
  char serial[32];snprintf(serial,sizeof(serial),"LK%u",93821001+position);d->sn_str[position]=strdup(serial);d->denom_mix[position]=original<100?100:original<140?50:original<160?20:10;position++;}
 assert(position==178);
}
static void live_projection_test(panel_page_t *p)
{
 perform(p,A_SERIAL);counting_data_mark_multi_result(counting_data_mutable());panel_pages_refresh(UI_PAGE_LIST,false);
 assert(!strcmp(p->list.cell[0][2].source,"--"));counting_data_reset_result_scope(counting_data_mutable());panel_pages_refresh(UI_PAGE_LIST,false);
 perform(p,A_FILTER_SERIAL);perform(p,A_SERIAL_INPUT);perform(p,A_KEY_CLEAR);perform(p,A_CHAR_BASE+'A');
 lv_obj_t *body=p->body,*editor=p->list.editor,*row=p->list.row[0];
 lv_obj_t *wing=lv_obj_get_child(p->panel.root,0),*key=lv_obj_get_child(wing,0);
 lv_obj_t *keyboard_key=lv_obj_get_parent(find_text(editor,"Q"));
 lv_event_send(keyboard_key,LV_EVENT_PRESSED,NULL);
 for(unsigned i=0;i<60;i++){
  counting_data_mutable()->total_amount+=10;
  ui_frame_commit_begin_batch();panel_pages_refresh(UI_PAGE_LIST,false);panel_pages_refresh(UI_PAGE_LIST,false);ui_frame_commit_end_batch();ui_frame_commit_flush();
  assert(p->body==body&&p->list.editor==editor&&p->list.row[0]==row&&lv_obj_get_child(wing,0)==key);
  assert(p->editing&&!strcmp(p->draft,"A")&&lv_obj_is_valid(keyboard_key));
 }
 lv_event_send(keyboard_key,LV_EVENT_RELEASED,NULL);lv_event_send(keyboard_key,LV_EVENT_CLICKED,NULL);
 assert(!strcmp(p->draft,"AQ")&&p->list.editor==editor);
 capture("list-live-editor");
 lv_event_send(editor,LV_EVENT_PRESSED,NULL);lv_event_send(editor,LV_EVENT_LONG_PRESSED,NULL);lv_event_send(editor,LV_EVENT_RELEASED,NULL);assert(p->editing);
 lv_event_send(editor,LV_EVENT_PRESSED,NULL);lv_event_send(editor,LV_EVENT_PRESS_LOST,NULL);lv_event_send(editor,LV_EVENT_RELEASED,NULL);assert(p->editing);
 lv_event_send(editor,LV_EVENT_PRESSED,NULL);lv_event_send(editor,LV_EVENT_RELEASED,NULL);assert(!p->editing&&!strcmp(p->serial_query.text,"AA"));perform(p,A_VALUE);
}
static void reject_projection_test(void)
{
 assert(currency_state_confirm_active_selection(2,"USD"));assert(panel_pages_create(UI_PAGE_LIST,lv_scr_act()));
 panel_page_t *p=page_for(UI_PAGE_LIST);counting_sim_t *d=counting_data_mutable();
 assert(counting_data_ensure_error_capacity(d,8));d->err_num=8;d->err_expected=17;
 for(unsigned i=0;i<8;i++){d->err_code[i]=i+1;d->err_pcs[i]=i==6?4:1;}
 panel_pages_list_select(PAGE_02_MULTI_REJECTS,PAGE_02_MULTI_TAB_VALUE);
 assert(p->tab==LIST_REJECTS&&p->count==8&&find_text(p->body,"17 PCS"));
 perform(p,A_NEXT);assert(p->page==1&&!strcmp(p->list.cell[0][0].source,"007")&&!strcmp(p->list.cell[0][1].source,"4"));
 assert(!strcmp(p->list.cell[0][2].source,ui_text_counting_reject_reason(7)));capture("list-rejects");
 d->err_num=0;d->err_expected=9;panel_pages_refresh(UI_PAGE_LIST,false);
 assert(!p->count&&find_text(p->body,"9 PCS")&&find_text(p->body,ui_text_get(UI_TEXT_LIST_NO_REJECT_DETAILS)));capture("list-rejects-pending");
 panel_pages_destroy(UI_PAGE_LIST);
 counting_multi_reset();uint8_t closed[16];memset(closed,0xff,sizeof(closed));closed[3]=0x0b;counting_multi_denom(closed,sizeof(closed),lv_tick_get());assert(!counting_multi_query_busy());
 assert(currency_state_confirm_multi_selection());multi.count=1;multi.currencies[0]=(multi_currency_t){.code="USD",.pcs=4,.amount=60};multi.total_pcs=4;multi.reject=6;
 rejects=(multi_reject_cache_t){0};extra_revision++;assert(panel_pages_create(UI_PAGE_LIST,lv_scr_act()));p=page_for(UI_PAGE_LIST);
 unsigned before=reject_requests;busy=true;panel_pages_list_select(PAGE_02_MULTI_REJECTS,PAGE_02_MULTI_TAB_VALUE);assert(p->reject_pending&&reject_requests==before);
 busy=false;extra_busy=true;advance(240);assert(p->reject_pending&&reject_requests==before);
 extra_busy=false;advance(240);assert(!p->reject_pending&&reject_requests==before+1&&p->count==0);
 rejects.status=MULTI_DETAIL_READY;rejects.count=2;rejects.rows[0]=(multi_reject_t){0x1c,4};rejects.rows[1]=(multi_reject_t){3,2};extra_revision++;advance(240);
 assert(p->count==2&&!strcmp(p->list.cell[0][0].source,"0x1C")&&!strcmp(p->list.cell[1][0].source,"0x03"));
 assert(!strcmp(p->list.cell[0][1].source,"4")&&!strcmp(p->list.cell[1][1].source,"2"));assert(find_text(p->body,"6 PCS"));capture("list-multi-rejects");
 const multi_detail_status_t states[]={MULTI_DETAIL_LOADING,MULTI_DETAIL_INVALID,MULTI_DETAIL_TIMEOUT,MULTI_DETAIL_EMPTY};
 for(unsigned i=0;i<4;i++){rejects.status=states[i];extra_revision++;advance(240);assert(!p->count&&!find_text(p->body,"0x1C"));assert(reject_requests==before+1);}
 capture("list-multi-rejects-empty");
 extra_busy=true;rejects.status=MULTI_DETAIL_TIMEOUT;extra_revision++;advance(240);assert(!strcmp(p->list.empty.source,ui_text_get(UI_TEXT_MULTI_WAIT_END)));capture("list-multi-rejects-timeout");
 multi.generation++;rejects.status=MULTI_DETAIL_NONE;rejects.count=0;extra_revision++;advance(240);assert(p->reject_pending&&reject_requests==before+1);
 extra_busy=false;send_ok=false;advance(240);assert(!p->reject_pending&&reject_requests==before+2&&rejects.status==MULTI_DETAIL_INVALID);capture("list-multi-rejects-failed");send_ok=true;
 /* A cold serial selection waits for a live denomination transaction, then
  * requests exactly once; old cached rows are never mistaken for a reply. */
 serial_cache[0].status=MULTI_DETAIL_NONE;unsigned serial_before=serial_requests;
 panel_pages_list_select(0,PAGE_02_MULTI_TAB_VALUE);assert(counting_multi_query_busy());
 panel_pages_list_select(0,PAGE_02_MULTI_TAB_SERIAL);assert(p->count==0&&serial_requests==serial_before);
 memset(closed,0xff,sizeof(closed));closed[3]=0x0b;counting_multi_denom(closed,sizeof(closed),lv_tick_get());advance(240);
 assert(serial_requests==serial_before+1&&serial_cache[0].status==MULTI_DETAIL_LOADING&&!p->count&&!find_text(p->body,"AA02"));
 advance(500);assert(serial_requests==serial_before+1);
 serial_cache[0].status=MULTI_DETAIL_READY;extra_busy=false;extra_revision++;advance(240);assert(p->count==4&&find_text(p->body,"002")&&find_text(p->body,"009"));
 capture("list-multi-serial-ready");serial_cache[0].status=MULTI_DETAIL_INVALID;extra_revision++;advance(240);assert(!p->count&&!find_text(p->body,"AA02"));
 serial_cache[0].status=MULTI_DETAIL_READY;extra_revision++;
 multi.count=2;multi.currencies[1]=(multi_currency_t){.code="EUR",.pcs=2,.amount=50,.status=MULTI_DETAIL_READY};serial_cache[1].status=MULTI_DETAIL_NONE;
 panel_pages_list_select(1,PAGE_02_MULTI_TAB_SERIAL);assert(serial_requests==serial_before+2&&!p->count);
 serial_cache[1].status=MULTI_DETAIL_READY;extra_busy=false;extra_revision++;advance(240);assert(p->count==2&&find_text(p->body,"003")&&find_text(p->body,"008"));
 panel_pages_list_select(0,PAGE_02_MULTI_TAB_SERIAL);assert(p->tab==LIST_SERIAL);perform(p,A_SEARCH);perform(p,A_FILTER_SERIAL);perform(p,A_SERIAL_INPUT);perform(p,A_CHAR_BASE+'X');
 lv_obj_t *body=p->body,*editor=p->list.editor;multi.revision++;advance(240);assert(p->editing&&p->body==body&&p->list.editor==editor&&!strcmp(p->draft,"X"));
 multi.generation++;extra_revision++;advance(240);assert(!p->editing&&!p->draft[0]&&!p->serial_query.text[0]&&lv_obj_has_flag(editor,LV_OBJ_FLAG_HIDDEN));
 panel_pages_list_select(0,PAGE_02_MULTI_TAB_VALUE);assert(p->tab==LIST_VALUE);
 panel_pages_destroy(UI_PAGE_LIST);assert(currency_state_confirm_active_selection(2,"USD"));counting_data_clear_errors(d);
}

int main(int argc,char **argv){assert(argc==2);output=argv[1];lv_init();ui_lang_init();ui_lang_set(LANGUAGE_EN);ui_message_font_set_provider(ui_locale_font_compose);static lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw_pixels,NULL,1280*40);static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=1280;drv.ver_res=400;drv.draw_buf=&buf;drv.flush_cb=flush;assert(lv_disp_drv_register(&drv));fixture();visual_fixture();
 for(unsigned language=0;language<ui_lang_count();language++){language_t lang=ui_lang_at(language)->id;assert(ui_locale_font_prepare(lang));ui_lang_set(lang);
 for(unsigned page=0;page<2;page++){ui_page_t id=page?UI_PAGE_SETTING:UI_PAGE_MENU;assert(panel_pages_create(id,lv_scr_act()));panel_page_t *p=page_for(id);for(unsigned category=0;category<(page?6:4);category++){perform(p,A_CATEGORY_BASE+category);advance(700);regions(p);char name[80];snprintf(name,sizeof(name),"%s-%u-%u",page?"set":"menu",language,category);capture(name);}panel_pages_destroy(id);}
 assert(panel_pages_create(UI_PAGE_LIST,lv_scr_act()));panel_page_t *p=page_for(UI_PAGE_LIST);assert(p->count==178);regions(p);filter_key_bounds(p);
 assert(find_text(p->panel.root,ui_tr("Denom")));assert(!strcmp(p->list.head[2].source,ui_tr("Denom")));
 char list_name[80];snprintf(list_name,sizeof(list_name),"list-%s",lang==LANGUAGE_CN?"zh":ui_lang_at(language)->tag);assert(ui_lang_get()==lang);capture(list_name);printf("List capture: %s locale=%s\n",list_name,ui_lang_at(language)->tag);
 perform(p,A_SEARCH);filter_key_bounds(p);perform(p,A_FILTER_SERIAL);filter_key_bounds(p);perform(p,A_FILTER_VALUE);filter_key_bounds(p);perform(p,A_VALUE);assert(!strcmp(p->list.head[0].source,ui_tr("Denom")));panel_pages_destroy(UI_PAGE_LIST);}
 counting_data_clear_serials(counting_data_mutable());fixture();ui_lang_set(LANGUAGE_EN);assert(panel_pages_create(UI_PAGE_LIST,lv_scr_act()));panel_page_t *p=page_for(UI_PAGE_LIST);perform(p,A_NEXT);assert(p->page==1);assert(find_text(p->body,"007"));perform(p,A_PREV);perform(p,A_SEARCH);assert(p->filter&&p->denom_mode);assert(p->denoms[0]==100);unsigned ten=0,twenty=0;for(unsigned i=0;i<p->denom_count;i++){if(p->denoms[i]==10)ten=i;if(p->denoms[i]==20)twenty=i;}
 perform(p,A_DENOM_BASE+ten);assert(p->count==4&&p->slots[0]==0&&p->slots[1]==2&&p->slots[2]==6&&p->slots[3]==11);assert(find_text(p->body,"003")&&find_text(p->body,"012"));capture("list-denom");perform(p,A_DENOM_BASE+twenty);assert(p->count==7);perform(p,A_DENOM_BASE+ten);assert(p->count==3);
 perform(p,A_FILTER_SERIAL);perform(p,A_SERIAL_INPUT);snprintf(p->draft,sizeof(p->draft),"AA");perform(p,A_SERIAL_APPLY);assert(p->count==5&&p->slots[0]==0&&p->slots[1]==2&&p->slots[4]==10);assert(find_text(p->body,"003"));capture("list-serial");perform(p,A_SERIAL_INPUT);snprintf(p->draft,sizeof(p->draft),"ZZ");perform(p,A_SERIAL_CANCEL);assert(!strcmp(p->serial_query.text,"AA")&&p->count==5);perform(p,A_SERIAL_INPUT);snprintf(p->draft,sizeof(p->draft),"ZZ");lv_obj_t *backdrop=p->list.editor;lv_event_send(backdrop,LV_EVENT_PRESSED,NULL);lv_event_send(backdrop,LV_EVENT_RELEASED,NULL);assert(!p->editing&&!strcmp(p->serial_query.text,"AA")&&p->count==5);perform(p,A_FILTER_VALUE);assert(p->count==3);perform(p,A_FILTER_SERIAL);assert(p->count==5);perform(p,A_VALUE);assert(p->count==4);assert(p->filtered_pcs[0]==1&&p->filtered_pcs[1]==1&&p->filtered_pcs[2]==1&&p->filtered_pcs[3]==2);capture("list-value-filter");
 live_projection_test(p);panel_pages_suspend(UI_PAGE_LIST);assert(p->timer->paused&&!panel_pages_visible(UI_PAGE_LIST));panel_pages_refresh(UI_PAGE_LIST,true);assert(panel_pages_resume(UI_PAGE_LIST));assert(p->count==4);panel_pages_destroy(UI_PAGE_LIST);
 assert(currency_state_confirm_multi_selection());assert(panel_pages_create(UI_PAGE_LIST,lv_scr_act()));p=page_for(UI_PAGE_LIST);assert(p->count==2);perform(p,A_MULTI_BASE);multi.currencies[0].denom_count=2;panel_pages_refresh(UI_PAGE_LIST,false);perform(p,A_SEARCH);perform(p,A_FILTER_SERIAL);perform(p,A_SERIAL_INPUT);snprintf(p->draft,sizeof(p->draft),"AA");perform(p,A_SERIAL_APPLY);assert(p->count==2&&find_text(p->body,"002")&&find_text(p->body,"009"));perform(p,A_VALUE);assert(p->count==2&&p->filtered_pcs[0]==1&&p->filtered_pcs[1]==1);capture("list-multi-filter");panel_pages_list_select(1,0);assert(!p->serial_query.text[0]&&!p->denom_query.denomination_count);assert(p->count==2);panel_pages_destroy(UI_PAGE_LIST);assert(currency_state_confirm_active_selection(2,"USD"));
 reject_projection_test();assert(panel_pages_create(UI_PAGE_SETTING,lv_scr_act()));p=page_for(UI_PAGE_SETTING);unsigned before=requests;busy=true;perform(p,A_SPEED);assert(!panel_choice_is_open()&&requests==before);busy=false;perform(p,A_SPEED);assert(panel_choice_is_open_for(p));send_ok=false;assert(!choice_confirm(2,p)&&machine_state_speed()==1);send_ok=true;assert(choice_confirm(2,p)&&machine_state_speed()==1);panel_choice_close_for(p);machine_state_confirm_speed(2);panel_pages_refresh(UI_PAGE_SETTING,false);assert(requested_speed==2);perform(p,A_MODE);assert(choice_confirm(MODE_CNT,p));assert(dialog_confirm);assert(machine_state_mode()==MODE_MDC);dialog_confirm(dialog_context);assert(requested_mode==MODE_CNT);panel_choice_close_for(p);perform(p,A_SOUND);assert(!sound);machine_fault_key_t fault={MACHINE_FAULT_RUNTIME,0,1};machine_fault_report_event(fault);advance(240);assert(p->have_fault&&find_text(p->panel.root,ui_tr("Guide")));perform(p,A_FAULT_GUIDE);assert(guide_requests==1);capture("set-fault");machine_fault_clear();advance(240);assert(!p->have_fault);perform(p,A_BRIGHTNESS);send_ok=false;assert(!number_confirm("60",p)&&backlight==80);send_ok=true;assert(number_confirm("60",p)&&backlight==60);panel_entry_close_for(p);perform(p,A_BRIGHTNESS);assert(panel_entry_is_open_for(p));ui_frame_commit_begin_batch();panel_pages_refresh(UI_PAGE_SETTING,false);panel_pages_refresh(UI_PAGE_SETTING,false);assert(ui_frame_commit_pending());panel_pages_suspend(UI_PAGE_SETTING);assert(!panel_entry_is_open()&&!ui_frame_commit_pending()&&p->timer->paused);ui_frame_commit_end_batch();assert(panel_pages_resume(UI_PAGE_SETTING)&&!p->timer->paused);panel_pages_destroy(UI_PAGE_SETTING);
 assert(panel_pages_create(UI_PAGE_MENU,lv_scr_act()));p=page_for(UI_PAGE_MENU);perform(p,A_OPERATOR);assert(routed==UI_PAGE_OPERATORS);perform(p,A_INTERACTION);assert(routed==UI_PAGE_MENU&&menu_category==4&&menu_subcategory==1);perform(p,A_DISPLAY);assert(routed==UI_PAGE_MENU&&menu_category==4&&menu_subcategory==2);perform(p,A_RECEIPT);assert(routed==UI_PAGE_PRINT_SETTING);perform(p,A_DETECTION);assert(routed==UI_PAGE_SET_PASSAGE&&pin_destination==UI_PAGE_CFD_LEVEL_SETTING);perform(p,A_QUICK_GUIDE);assert(gesture_guides==1);perform(p,A_CARE);assert(menu_category==5&&menu_subcategory==1);panel_pages_destroy(UI_PAGE_MENU);
 unsigned timers_before=0,timers_after=0;for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer))timers_before++;
 ui_frame_commit_stats_t commits;ui_frame_commit_take_stats(&commits);
 for(unsigned n=0;n<20;n++){lv_obj_t *owner=lv_obj_create(lv_scr_act());assert(panel_pages_create(UI_PAGE_MENU,owner));ui_frame_commit_begin_batch();panel_pages_refresh(UI_PAGE_MENU,false);lv_obj_del(owner);ui_frame_commit_end_batch();ui_frame_commit_flush();assert(!panel_pages_created(UI_PAGE_MENU)&&!page_for(UI_PAGE_MENU));advance(240);}
 for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer))timers_after++;assert(timers_before==timers_after);ui_frame_commit_take_stats(&commits);assert(commits.callbacks==0);printf("Lifecycle: timers %u -> %u; cancelled owner commits %u; callbacks after delete %u\n",timers_before,timers_after,commits.requests,commits.callbacks);
 assert(key_overflows==0);puts("PASS: all ten language key bounds, stable rows and live draft, normal/MULTI rejects and cache status, current machine-fault projection/guide; 4 Menu/6 Set categories EN+CN, fixed regions, real queries/original NO, independent filters/pagination, MULTI currency reset and summary, request/ACK/failure boundaries, hidden resume and parent deletion");return 0;}
