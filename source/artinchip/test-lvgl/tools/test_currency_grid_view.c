/* Real grid builder and LVGL rendering; selection/protocol edges are captured. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"
#include "un260/lv_core/page_07_curr/page_07_curr_layout.h"
#include "un260/lv_core/page_07_curr/page_07_curr_star.h"
#include "un260/lv_core/page_07_curr/page_07_curr_view.h"
#include "un260/lv_core/page_07_curr/page_07_curr_overview.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/ui_scrollbar.h"
#include "grid_assets.h"
page07_curr_context_t g_page07_curr;
static int g_curr_grid_styled_abs_idx=-1;
static const char *codes[]={"AUTO","MUL","EUR","USD","CNY","RUB","TRY","GBP","MXN","CAD","ILS","AED","SAR","IRR","KRW","JPY","HKD","AUD","SGD","MOP","XOF","XAF","UAH","INR","EGP","PHP","THB","IDR","ZAR","QAR","PKR","CHF","TJS","AMD","AZN","LBP"};
static unsigned select_requests,favorite_requests;
static unsigned card_requests,back_requests,filter_requests;
static void curr_set_mode_visible(void);
void page07_curr_carousel_enable(page07_curr_carousel_t *c,bool enabled){(void)c;(void)enabled;}
static void curr_apply_selected_style(void){}
static void curr_update_track_by_scroll(void){}
static void curr_view_btn_click_cb(lv_event_t *e){(void)e;card_requests++;g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_CARD;curr_set_mode_visible();}
static void curr_grid_filter_click_cb(lv_event_t *e){filter_requests++;g_page07_curr.model.favorite_only=(intptr_t)lv_event_get_user_data(e)!=0;}
static void curr_back_btn_click_cb(lv_event_t *e){(void)e;back_requests++;}
bool currency_state_is_auto_code(const char *s){return !strcmp(s,"AUTO")||!strcmp(s,"AUT");}
bool currency_state_is_multi_code(const char *s){return !strcmp(s,"MUL");}
bool currency_state_get_code(uint8_t n,char out[4]){if(n>=sizeof(codes)/sizeof(codes[0]))return false;snprintf(out,4,"%.3s",codes[n]);return true;}
const char *currency_state_display_code(const char *s);
static void curr_grid_item_click_cb(lv_event_t *e){(void)e;select_requests++;}
static void curr_grid_fav_click_cb(lv_event_t *e){(void)e;favorite_requests++;}
static void curr_update_grid_fav_ui(int i){if(i<2)lv_obj_add_flag(g_page07_curr.grid_items[i].fav_btn,LV_OBJ_FLAG_HIDDEN);}
void perf_profile_watch_invalidation(lv_obj_t *o,const char *s){(void)o;(void)s;}
static bool evdev_press_cancelled;
static lv_obj_t *evdev_pressed_obj;
static lv_point_t evdev_press_point;
static bool evdev_visual_cancelled;
#include "grid_under_test.h"
static lv_color_t frame[1280*400],buffer[1280*40];
static lv_point_t point;
static lv_indev_state_t state;
static void input(lv_indev_drv_t*d,lv_indev_data_t*data){(void)d;data->point=point;data->state=state;if(evdev_press_cancelled){data->state=LV_INDEV_STATE_RELEASED;if(state==LV_INDEV_STATE_RELEASED)evdev_press_cancelled=false;}}
static void feed(int x,int y,lv_indev_state_t s){point=(lv_point_t){x,y};state=s;for(unsigned i=0;i<3;i++){lv_tick_inc(16);lv_timer_handler();}}
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(frame+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static const grid_asset_t *asset(const void *src){if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return NULL;const char *name=strrchr(src,'/');name=name?name+1:src;for(unsigned i=0;i<sizeof(grid_assets)/sizeof(grid_assets[0]);i++)if(!strcmp(name,grid_assets[i].name))return &grid_assets[i];return NULL;}
static lv_res_t image_info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;const grid_asset_t*a=asset(src);if(!a)return LV_RES_INV;memset(h,0,sizeof(*h));h->w=a->w;h->h=a->h;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){if(image_info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;const grid_asset_t*a=asset(s->src);FILE*f=fopen(a->path,"rb");assert(f);unsigned char*p=malloc(a->w*a->h*4);assert(p);assert(fread(p,4,a->w*a->h,f)==a->w*a->h);fclose(f);s->img_data=p;return LV_RES_OK;}
static void image_close(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){(void)d;free((void*)s->img_data);}
static void snapshot(const char*n){lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(frame,4,1280*400,f)==1280*400);fclose(f);}
int main(void){
 lv_init();lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_t*disp=lv_disp_drv_register(&dd);ui_scrollbar_init(disp);
 lv_img_decoder_t*decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,image_info);lv_img_decoder_set_open_cb(decoder,image_open);lv_img_decoder_set_close_cb(decoder,image_close);
 lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=input;driver.feedback_cb=evdev_feedback;lv_indev_drv_register(&driver);
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0xEDF1F6),0);
 g_page07_curr.objects.root=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(g_page07_curr.objects.root);lv_obj_set_size(g_page07_curr.objects.root,1280,400);lv_obj_clear_flag(g_page07_curr.objects.root,LV_OBJ_FLAG_SCROLLABLE);
 g_page07_curr.objects.left_panel=lv_obj_create(g_page07_curr.objects.root);
 g_page07_curr.objects.right_area=lv_obj_create(g_page07_curr.objects.root);lv_obj_remove_style_all(g_page07_curr.objects.right_area);lv_obj_set_pos(g_page07_curr.objects.right_area,288,0);lv_obj_set_size(g_page07_curr.objects.right_area,992,400);lv_obj_clear_flag(g_page07_curr.objects.right_area,LV_OBJ_FLAG_SCROLLABLE);
 g_page07_curr.model.visible_count=34;g_page07_curr.model.selected_abs_idx=4;for(int i=0;i<34;i++)g_page07_curr.model.visible_indices[i]=i;
 for(int cycle=0;cycle<3;cycle++){
  g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
  curr_build_grid_layer();curr_set_mode_visible();lv_obj_update_layout(lv_scr_act());
  assert(lv_obj_get_parent(g_page07_curr.objects.grid_layer)==g_page07_curr.objects.root);
  assert(lv_obj_has_flag(g_page07_curr.objects.left_panel,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(g_page07_curr.objects.right_area,LV_OBJ_FLAG_HIDDEN));
  lv_area_t viewport;lv_obj_get_coords(g_page07_curr.objects.grid_scroll,&viewport);int visible=0;
  for(int i=0;i<34;i++){
   page07_curr_grid_item_t *item=&g_page07_curr.grid_items[i];lv_area_t bounds,name,favorite;lv_obj_get_coords(item->item,&bounds);lv_obj_get_coords(item->name,&name);lv_obj_get_coords(item->fav_btn,&favorite);
   if(bounds.y1>=viewport.y1&&bounds.y2<=viewport.y2)visible++;
   assert(bounds.x2<viewport.x2-12&&name.x2<bounds.x2);
   if(!lv_obj_has_flag(item->fav_btn,LV_OBJ_FLAG_HIDDEN))assert(name.x2<favorite.x1);
   lv_point_t ink;lv_txt_get_size(&ink,lv_label_get_text(item->name),lv_obj_get_style_text_font(item->name,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
   assert(ink.x<=lv_obj_get_width(item->name));
   assert(lv_obj_get_style_img_opa(item->img,0)==LV_OPA_COVER);
  }
  assert(visible==32);assert(lv_obj_get_scroll_bottom(g_page07_curr.objects.grid_scroll)>0);
  if(!cycle)snapshot("currency-view-top");
  feed(645,220,LV_INDEV_STATE_PRESSED);lv_obj_t *pressed=evdev_pressed_obj;assert(pressed);
  for(int y=210;y>=90;y-=12)feed(645,y,LV_INDEV_STATE_PRESSED);
  assert(!lv_obj_has_state(pressed,LV_STATE_PRESSED));feed(645,90,LV_INDEV_STATE_RELEASED);
  assert(select_requests==0&&favorite_requests==0);assert(lv_obj_get_scroll_y(g_page07_curr.objects.grid_scroll)>30);
  for(int settle=0;settle<90;settle++){lv_tick_inc(16);lv_timer_handler();}
  assert(lv_obj_get_scroll_bottom(g_page07_curr.objects.grid_scroll)>=0);
  if(!cycle)snapshot("currency-view-scrolled");
  g_page07_curr.model.selected_abs_idx=10;curr_apply_grid_selected_style();assert(!lv_obj_has_flag(g_page07_curr.grid_items[10].selected_mark,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_state(g_page07_curr.grid_items[10].item,LV_STATE_CHECKED));
  assert(!strcmp(lv_label_get_text(g_page07_curr.objects.overview.current_code),"ILS"));
  lv_event_send(g_page07_curr.objects.overview.filter[1],LV_EVENT_CLICKED,NULL);assert(g_page07_curr.model.favorite_only);
  lv_event_send(g_page07_curr.objects.overview.filter[0],LV_EVENT_CLICKED,NULL);assert(!g_page07_curr.model.favorite_only);
  lv_event_send(g_page07_curr.objects.overview.back_button,LV_EVENT_CLICKED,NULL);
  lv_event_send(g_page07_curr.objects.overview.card_button,LV_EVENT_CLICKED,NULL);
  assert(!lv_obj_has_flag(g_page07_curr.objects.left_panel,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(g_page07_curr.objects.right_area,LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_has_flag(g_page07_curr.objects.grid_layer,LV_OBJ_FLAG_HIDDEN));
  lv_obj_del(g_page07_curr.objects.grid_layer);
 }
 assert(card_requests==3&&back_requests==3&&filter_requests==6);
 g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
 g_page07_curr.model.favorite_only=true;
 g_page07_curr.model.visible_count=5;
 g_page07_curr.model.selected_abs_idx=4;
 curr_build_grid_layer();curr_set_mode_visible();snapshot("currency-view-favorites");
 lv_obj_del(g_page07_curr.objects.grid_layer);
 g_page07_curr.model.visible_count=0;
 curr_build_grid_layer();curr_set_mode_visible();snapshot("currency-view-empty");
 assert(g_page07_curr.objects.overview.back_button&&g_page07_curr.objects.overview.card_button);
 lv_obj_del(g_page07_curr.objects.grid_layer);
 lv_obj_del(g_page07_curr.objects.root);lv_img_cache_invalidate_src(NULL);
 puts("PASS full production Currency VIEW: complete header, 32 full cells, opaque flags, selection projection, filtering/card/back routing, native scroll cancels feedback and selection, repeated lifetime");return 0;
}
