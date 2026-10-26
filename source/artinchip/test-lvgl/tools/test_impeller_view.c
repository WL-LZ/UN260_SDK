#include "test_notice_sink.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/font/ui_message_font.h"
#define SETTINGS_THEME_DISABLE_COLOR_REMAP
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/lv_page_manager.h"
#include "test_settings_actions.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_core/page_40_set_impeller.h"
#include "un260/lv_components/lv_settings.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/machine_state/machine_state.h"
#include "tools/test_page_background_asset.h"

/* Controller/service boundary is mocked here; test_impeller_setting exercises
 * actual serial requests, replies and the owner timeout independently. */
static unsigned sends, reads, pops;
static bool pending, known, send_ok=true, reading;
static uint8_t actual=IMPELLER_SPEED_DEFAULT, requested;
static settings_detail_keyboard_cb_t keyboard_confirm;
static void *keyboard_context;
uint64_t app_clock_monotonic_ms(void){return lv_tick_get();}
uint32_t app_clock_uptime_ms(void){return lv_tick_get();}
uint8_t machine_state_impeller_speed(void){return actual;}
bool machine_state_impeller_speed_known(void){return known;}
bool setting_service_impeller_speed_is_pending(void){return pending;}
bool setting_service_request_impeller_speed_read(void)
{if(pending||!send_ok)return false;sends++;reads++;pending=true;reading=true;return true;}
bool setting_service_request_impeller_speed(uint8_t target)
{assert(target>=15&&target<=50);if(pending||!send_ok)return false;if(known&&actual==target)return true;sends++;requested=target;pending=true;reading=false;return true;}
bool ui_manager_pop_page(void){pops++;return true;}
void ui_manager_switch(ui_page_t page){(void)page;pops++;}
bool settings_detail_keyboard_show(const char *title,const char *initial,uint16_t length,
 settings_detail_keyboard_mode_t mode,settings_detail_keyboard_cb_t cb,void *context)
{assert(title&&initial&&length==2&&mode==SETTINGS_DETAIL_KEYBOARD_UINT);keyboard_confirm=cb;keyboard_context=context;return true;}
void settings_detail_keyboard_hide(void){keyboard_confirm=NULL;keyboard_context=NULL;}
static lv_color_t pixels[1280*400],buffer[1280*40];
static lv_point_t pointer_position;
static lv_indev_state_t pointer_state;
static void pointer_read(lv_indev_drv_t *driver,lv_indev_data_t *data)
{(void)driver;data->point=pointer_position;data->state=pointer_state;}
static void advance(unsigned ms){lv_tick_inc(ms);lv_timer_handler();}
static void pointer_feed(int x,int y,lv_indev_state_t state)
{pointer_position=(lv_point_t){x,y};pointer_state=state;for(unsigned i=0;i<3;i++)advance(16);}
static void flush(lv_disp_drv_t *display,const lv_area_t *area,lv_color_t *p)
{for(int y=area->y1;y<=area->y2;y++)memcpy(pixels+y*1280+area->x1,p+(y-area->y1)*(area->x2-area->x1+1),(area->x2-area->x1+1)*4);lv_disp_flush_ready(display);}
static lv_res_t info(lv_img_decoder_t *d,const void *src,lv_img_header_t *h)
{(void)d;if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return LV_RES_INV;const un260_compiled_asset_t *a=test_page_asset_find(src);assert(a);memset(h,0,sizeof(*h));h->w=a->width;h->h=a->height;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s)
{if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=test_page_asset_find(s->src)->pixels;return LV_RES_OK;}
#include "test_settings_toolbar.h"
#include "test_locale_view_checks.h"
static lv_obj_t *find_label(lv_obj_t *o,const char *text)
{if(lv_obj_check_type(o,&lv_label_class)&&!strcmp(lv_label_get_text(o),text))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_label(lv_obj_get_child(o,i),text);if(r)return r;}return NULL;}
static lv_obj_t *find_type(lv_obj_t *o,const lv_obj_class_t *type)
{if(lv_obj_check_type(o,type))return o;for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++){lv_obj_t *r=find_type(lv_obj_get_child(o,i),type);if(r)return r;}return NULL;}
static lv_obj_t *button(const char *text)
{lv_obj_t *o=find_label(lv_scr_act(),ui_tr(text));assert(o);while(o&&!lv_obj_has_flag(o,LV_OBJ_FLAG_CLICKABLE))o=lv_obj_get_parent(o);assert(o);return o;}
static void click(const char *text){lv_event_send(button(text),LV_EVENT_CLICKED,NULL);}
static void tap(const char *text)
{lv_obj_update_layout(lv_scr_act());lv_area_t a;lv_obj_get_coords(button(text),&a);int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;pointer_feed(x,y,LV_INDEV_STATE_PRESSED);pointer_feed(x,y,LV_INDEV_STATE_RELEASED);}
static int draft(void){return lv_slider_get_value(find_type(lv_scr_act(),&lv_slider_class));}
static void move(int value)
{lv_obj_t *slider=find_type(lv_scr_act(),&lv_slider_class);lv_slider_set_value(slider,value,LV_ANIM_OFF);lv_event_send(slider,LV_EVENT_VALUE_CHANGED,NULL);}
static void numeric(const char *text)
{char current[4];snprintf(current,sizeof(current),"%d",draft());click(current);assert(keyboard_confirm);keyboard_confirm(text,keyboard_context);settings_detail_keyboard_hide();}
static void finish(bool read,bool success,bool timeout,uint8_t value)
{
 pending=false;
 if(success){known=true;actual=value;}else if(read||timeout)known=false;
 setting_impeller_result_t result={.reading=read,.success=success,.timeout=timeout,.value=value};
 ui_page_40_set_impeller_on_update(&result);
}
static void complete(bool valid,uint8_t value){finish(reading,valid,false,value);}
static void screenshot(const char *name)
{lv_obj_update_layout(lv_scr_act());assert_settings_toolbars(lv_scr_act());check_single_line(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),name);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(pixels,4,1280*400,f)==1280*400);fclose(f);}
static void bounds(lv_obj_t *o)
{
 if(lv_obj_is_visible(o)&&lv_obj_check_type(o,&lv_label_class)){
  const lv_font_t *font=lv_obj_get_style_text_font(o,0);lv_point_t ink;lv_txt_get_size(&ink,lv_label_get_text(o),font,lv_obj_get_style_text_letter_space(o,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
  lv_obj_t *p=lv_obj_get_parent(o);lv_area_t a,b;lv_obj_get_coords(o,&a);lv_obj_get_coords(p,&b);
  assert(ink.x<=lv_obj_get_width(o)&&ink.y<=font->line_height);
  assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);
  if(lv_obj_has_flag(p,LV_OBJ_FLAG_CLICKABLE)&&lv_obj_get_child_cnt(p)==1)assert(abs((a.y1+a.y2)-(b.y1+b.y2))<=2);
 }
 for(unsigned i=0;i<lv_obj_get_child_cnt(o);i++)bounds(lv_obj_get_child(o,i));
}
static void scenarios(void)
{
 ui_page_40_set_impeller_create(lv_scr_act());assert(sends==1&&reads==1&&pending&&draft()==30&&!known);
 assert(find_label(lv_scr_act(),"Speed not confirmed"));assert(lv_obj_has_state(button("Read"),LV_STATE_DISABLED)&&lv_obj_has_state(button("Save"),LV_STATE_DISABLED));
 assert(!lv_obj_has_state(button("Default"),LV_STATE_DISABLED)&&!lv_obj_has_state(button("Back"),LV_STATE_DISABLED));
 ui_page_40_set_impeller_create(lv_scr_act());assert(sends==1);screenshot("initial-pending");
 move(0);move(50);assert(sends==1&&draft()==50);tap("-");assert(draft()==49&&sends==1);tap("+");tap("+");assert(draft()==50&&sends==1);
 numeric("15");assert(draft()==15);tap("-");assert(draft()==15);numeric("50");assert(draft()==50);unsigned notices=test_notice_count;numeric("14");numeric("51");numeric("-1");numeric("x");assert(draft()==50&&test_notice_count==notices+4&&sends==1);
 click("Read");click("Save");assert(sends==1&&action_notices==0);
 complete(true,22);assert(draft()==50&&find_label(lv_scr_act(),"Confirmed speed: 22"));
 click("Default");assert(draft()==30&&sends==1);tap("Save");assert(sends==2&&requested==30&&pending);tap("+");assert(draft()==31);complete(true,30);assert(draft()==31);
 tap("Save");assert(sends==3&&requested==31);finish(false,false,false,0);assert(draft()==31&&actual==30);tap("Save");assert(sends==4);complete(true,31);assert(draft()==31);unsigned old=sends;tap("Save");assert(sends==old&&!pending);
 click("Read");assert(sends==old+1&&pending);complete(false,0);assert(draft()==31&&actual==31&&!known&&!lv_obj_has_state(button("Save"),LV_STATE_DISABLED));
 tap("Save");assert(sends==old+2&&requested==31&&pending);complete(true,31);
 send_ok=false;old=test_notice_count;tap("Read");assert(!pending&&test_notice_count==old&&draft()==31);assert(find_label(lv_scr_act(),"Request not sent. Try again."));send_ok=true;tap("Read");assert(pending);complete(true,31);
 numeric("48");tap("Save");assert(pending&&requested==48);old=sends;tap("Back");assert(pops==1&&pending);ui_page_40_set_impeller_destroy();assert(pending);ui_page_40_set_impeller_on_update(NULL);
 ui_page_40_set_impeller_create(lv_scr_act());assert(sends==old&&pending&&draft()==31);complete(true,48);assert(draft()==48);ui_page_40_set_impeller_destroy();complete(true,15);
 ui_page_40_set_impeller_create(lv_scr_act());assert(pending&&draft()==15&&reads>=4);complete(true,15);screenshot("confirmed");
 numeric("40");tap("Save");assert(pending);numeric("15");complete(true,40);assert(draft()==15);
 tap("Read");assert(pending);numeric("40");complete(true,25);assert(draft()==40);
 ui_page_40_set_impeller_destroy();
 puts("PASS impeller UI: default/unknown, read, 15/50/invalid, draft-only slider/steps, one tap one send, pending edit/back, no-op, retry, timeout, hidden reply and recreate");
}
static void read_feedback(void)
{
 known=true;actual=30;send_ok=false;unsigned notices=test_notice_count,old_sends=sends;
 ui_page_40_set_impeller_create(lv_scr_act());
 assert(!pending&&draft()==30&&sends==old_sends&&test_notice_count==notices);
 assert(find_label(lv_scr_act(),"Request not sent. Try again."));
 move(42);assert(draft()==42&&sends==old_sends&&test_notice_count==notices);
 assert(!lv_obj_has_state(button("Back"),LV_STATE_DISABLED));screenshot("read-send-failed");ui_page_40_set_impeller_destroy();
 send_ok=true;ui_page_40_set_impeller_create(lv_scr_act());assert(pending&&reading&&draft()==30);
 old_sends=sends;move(42);tap("+");assert(draft()==43&&sends==old_sends);
 finish(true,false,true,0);assert(draft()==43&&actual==30&&!known&&test_notice_count==notices);
 assert(find_label(lv_scr_act(),"No response received. Check device status."));
 assert(!lv_obj_has_state(button("Read"),LV_STATE_DISABLED)&&!lv_obj_has_state(button("Save"),LV_STATE_DISABLED));
 assert(!lv_obj_has_state(button("Default"),LV_STATE_DISABLED)&&!lv_obj_has_state(button("Back"),LV_STATE_DISABLED));
 screenshot("read-timeout");tap("Read");assert(pending&&reading);move(40);finish(true,false,false,0);
 assert(draft()==40&&test_notice_count==notices&&find_label(lv_scr_act(),"Could not read the setting. Try again."));
 screenshot("read-rejected");old_sends=sends;tap("Save");assert(!reading&&pending&&requested==40&&sends==old_sends+1);
 finish(false,false,true,0);assert(draft()==40&&actual==30&&!known);
 assert(!find_label(lv_scr_act(),"Could not read the setting. Try again."));
 tap("Read");assert(reading&&pending);tap("Back");ui_page_40_set_impeller_destroy();finish(true,false,true,0);
 assert(test_notice_count==notices);ui_page_40_set_impeller_create(lv_scr_act());assert(reading&&pending&&draft()==30);
 move(44);complete(true,35);assert(draft()==44&&actual==35&&known&&test_notice_count==notices);ui_page_40_set_impeller_destroy();
 puts("PASS read feedback stays in page: initial/manual send failure, auto-read timeout, explicit read rejection, no banners, unchanged draft, editor/back usable, Save is explicit, leave/reenter and delayed completion");
}
int main(void)
{
 lv_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);ui_lang_set(LANGUAGE_EN);
 lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,buffer,NULL,1280*40);lv_disp_drv_t display;lv_disp_drv_init(&display);display.hor_res=1280;display.ver_res=400;display.draw_buf=&draw;display.flush_cb=flush;lv_disp_drv_register(&display);
 lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,info);lv_img_decoder_set_open_cb(decoder,image_open);lv_indev_drv_t pointer;lv_indev_drv_init(&pointer);pointer.type=LV_INDEV_TYPE_POINTER;pointer.read_cb=pointer_read;lv_indev_drv_register(&pointer);
 scenarios();read_feedback();
 known=true;actual=10;unsigned before=sends, notices=test_notice_count;
 ui_page_40_set_impeller_create(lv_scr_act());assert(draft()==15&&sends==before+1);
 complete(true,10);assert(draft()==15&&find_label(lv_scr_act(),"Confirmed speed: 10")&&test_notice_count==notices);
 screenshot("legacy-readback-10");tap("Save");assert(requested==15&&sends==before+2);complete(true,15);
 ui_page_40_set_impeller_destroy();
 for(size_t i=0;i<ui_lang_count();i++){
  const ui_locale_t *locale=ui_lang_at(i);assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);ui_page_40_set_impeller_create(lv_scr_act());complete(true,30);lv_obj_update_layout(lv_scr_act());bounds(lv_scr_act());char name[80];snprintf(name,sizeof(name),"locale-%s",locale->tag);screenshot(name);ui_page_40_set_impeller_destroy();
 }
 puts("PASS ten locales: production fonts/glyphs, title/buttons single-line, measured vertical centering, toolbar positions, no clipping");return 0;
}
