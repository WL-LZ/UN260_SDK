/* Real grid builder and LVGL rendering; selection/protocol edges are captured. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/font/ui_locale_font_bank.h"
#include "un260/font/ui_message_font.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_core/page_07_curr/page_07_curr_internal.h"
#include "un260/lv_core/page_07_curr/page_07_curr_layout.h"
#include "un260/lv_core/page_07_curr/page_07_curr_star.h"
#include "un260/lv_core/page_07_curr/page_07_curr_view_icons.h"
#include "un260/lv_core/page_07_curr/page_07_curr_view.h"
#include "un260/lv_core/page_07_curr/page_07_curr_overview.h"
#include "un260/lv_components/lv_damped_button.h"
#include "un260/lv_components/ui_scrollbar.h"
#include "un260/gesture/touch_sound.h"
#include "grid_assets.h"
#include "un260/lv_resources/lv_curr/curr_view_atlas.h"
static unsigned atlas_opens, image_opens;
static bool fail_atlas;
page07_curr_context_t g_page07_curr;
static lv_obj_t *curr_page;
static int g_curr_grid_styled_abs_idx=-1;
static const char *codes[]={"AUT","MUL","USD","AUD","CAD","GBP","EUR","TRY","AED","IQD","SAR","IRR","CNY","JPY","AFN","RUB","CHF","KRW","KWD","INR","PKR","TMT","AZN","AMD","GEL","OMR","BHD","QAR","YER","SYP","NZD","ZAR","HKD","SGD"};
int page07_curr_model_find_visible_pos(int abs_idx) { for(int i=0;i<g_page07_curr.model.visible_count;i++) if(g_page07_curr.model.visible_indices[i]==abs_idx) return i; return -1; }
static uint8_t catalog_count=34;
static bool favorite_values[34];
uint8_t currency_state_count(void){return catalog_count;}
bool page07_curr_model_is_favorite(int abs_idx){return abs_idx>=2&&abs_idx<34&&favorite_values[abs_idx];}
bool page07_curr_model_is_fixed(int abs_idx){return abs_idx<2;}
static void curr_update_grid_fav_ui(int i);
static unsigned select_requests,favorite_requests;
static int selected_request=-1,favorite_request=-1;
static unsigned card_requests,back_requests,filter_requests;
static void curr_set_mode_visible(void);
static void curr_switch_cached_view(void);
static void curr_refresh_right_views(void){assert(!"Unexpected empty-catalog cache rebuild");}
static void curr_build_card_layer(void){g_page07_curr.objects.card_layer=lv_obj_create(g_page07_curr.objects.right_area);}
static void curr_scroll_to_visible_idx(int index,bool animate){(void)index;assert(!animate);}
void page07_curr_carousel_enable(page07_curr_carousel_t *c,bool enabled){(void)c;(void)enabled;}
static void curr_apply_selected_style(void){}
static void curr_update_track_by_scroll(void){}
static void curr_view_btn_click_cb(lv_event_t *e){(void)e;card_requests++;g_page07_curr.model.view_mode=g_page07_curr.model.view_mode==PAGE07_CURR_VIEW_CARD?PAGE07_CURR_VIEW_GRID:PAGE07_CURR_VIEW_CARD;curr_switch_cached_view();}
static void curr_grid_filter_click_cb(lv_event_t *e){filter_requests++;g_page07_curr.model.favorite_only=(intptr_t)lv_event_get_user_data(e)!=0;}
static void curr_back_btn_click_cb(lv_event_t *e){(void)e;back_requests++;}
bool currency_state_is_auto_code(const char *s){return !strcmp(s,"AUTO")||!strcmp(s,"AUT");}
bool currency_state_is_multi_code(const char *s){return !strcmp(s,"MUL");}
bool currency_state_get_code(uint8_t n,char out[4]){if(n>=sizeof(codes)/sizeof(codes[0]))return false;snprintf(out,4,"%.3s",codes[n]);return true;}
const char *currency_state_display_code(const char *s);
static void curr_grid_item_click_cb(lv_event_t *e){selected_request=(int)(intptr_t)lv_event_get_user_data(e);select_requests++;}
static void curr_grid_fav_click_cb(lv_event_t *e){favorite_request=(int)(intptr_t)lv_event_get_user_data(e);favorite_requests++;favorite_values[favorite_request]=!favorite_values[favorite_request];curr_update_grid_fav_ui(favorite_request);page07_curr_overview_selection(&g_page07_curr);}

void perf_profile_watch_invalidation(lv_obj_t *o,const char *s){(void)o;(void)s;}
static bool evdev_press_cancelled;
static lv_obj_t *evdev_pressed_obj;
static lv_point_t evdev_press_point;
static bool evdev_visual_cancelled;
static void (*g_feedback_observer)(lv_indev_t *, lv_event_code_t);
#include "grid_under_test.h"
static lv_color_t frame[1280*400],buffer[1280*40];
static lv_point_t point;
static lv_indev_state_t state;
static unsigned sound_requests;
static void request_sound(void){sound_requests++;}
static void input(lv_indev_drv_t*d,lv_indev_data_t*data){(void)d;data->point=point;data->state=state;if(evdev_press_cancelled){data->state=LV_INDEV_STATE_RELEASED;if(state==LV_INDEV_STATE_RELEASED)evdev_press_cancelled=false;}}
static void feed(int x,int y,lv_indev_state_t s){if(s!=state)touch_sound_contact(s==LV_INDEV_STATE_PRESSED);point=(lv_point_t){x,y};state=s;for(unsigned i=0;i<3;i++){lv_tick_inc(16);lv_timer_handler();}}
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)memcpy(frame+y*1280+a->x1,p+(y-a->y1)*(a->x2-a->x1+1),(a->x2-a->x1+1)*4);lv_disp_flush_ready(d);}
static const grid_asset_t *asset(const void *src){if(lv_img_src_get_type(src)!=LV_IMG_SRC_FILE)return NULL;const char *name=strrchr(src,'/');name=name?name+1:src;for(unsigned i=0;i<sizeof(grid_assets)/sizeof(grid_assets[0]);i++)if(!strcmp(name,grid_assets[i].name))return &grid_assets[i];return NULL;}
static lv_res_t image_info(lv_img_decoder_t*d,const void*src,lv_img_header_t*h){(void)d;const grid_asset_t*a=asset(src);if(!a)return LV_RES_INV;memset(h,0,sizeof(*h));h->w=a->w;h->h=a->h;h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;return LV_RES_OK;}
static lv_res_t image_open(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){if(image_info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;const grid_asset_t*a=asset(s->src);if(!strcmp(a->name,"flags.png")){if(fail_atlas)return LV_RES_INV;atlas_opens++;}image_opens++;FILE*f=fopen(a->path,"rb");assert(f);unsigned char*p=malloc(a->w*a->h*4);assert(p);assert(fread(p,4,a->w*a->h,f)==a->w*a->h);fclose(f);s->img_data=p;return LV_RES_OK;}
static void image_close(lv_img_decoder_t*d,lv_img_decoder_dsc_t*s){(void)d;free((void*)s->img_data);}
static void snapshot(const char*n){lv_obj_update_layout(lv_scr_act());lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/%s.bgra",getenv("OUT"),n);FILE*f=fopen(path,"wb");assert(f);assert(fwrite(frame,4,1280*400,f)==1280*400);fclose(f);}

static void advance(unsigned frames)
{
 for(unsigned i=0;i<frames;i++){lv_tick_inc(16);lv_timer_handler();}
 lv_obj_update_layout(lv_scr_act());lv_refr_now(NULL);
}
static lv_area_t bounds(lv_obj_t *obj)
{
 lv_area_t area;lv_obj_get_coords(obj,&area);return area;
}
static bool contained(lv_area_t inner,lv_area_t outer)
{
 return inner.x1>=outer.x1&&inner.x2<=outer.x2&&inner.y1>=outer.y1&&inner.y2<=outer.y2;
}
static void tap_at(int x,int y)
{
 feed(x,y,LV_INDEV_STATE_PRESSED);feed(x,y,LV_INDEV_STATE_RELEASED);advance(2);
}
static void tap(lv_obj_t *obj)
{
 lv_area_t a=bounds(obj);tap_at((a.x1+a.x2)/2,(a.y1+a.y2)/2);
}
static void swipe(int start,int end,int y)
{
 feed(start,y,LV_INDEV_STATE_PRESSED);
 for(int step=1;step<=15;step++)feed(start+(end-start)*step/15,y,LV_INDEV_STATE_PRESSED);
 feed(end,y,LV_INDEV_STATE_RELEASED);advance(60);
}
static unsigned color_at(int x,int y)
{
 lv_color_t c=frame[y*1280+x];return (c.ch.red<<16)|(c.ch.green<<8)|c.ch.blue;
}
static lv_obj_t *text_child(lv_obj_t *parent)
{
 if(lv_obj_check_type(parent,&lv_label_class))return parent;
 for(unsigned i=0;i<lv_obj_get_child_cnt(parent);i++){
  lv_obj_t *label=text_child(lv_obj_get_child(parent,i));if(label)return label;
 }
 return NULL;
}
static void text_fits(lv_obj_t *label,lv_obj_t *parent,const char *locale,const char *name)
{
 assert(label&&parent);lv_obj_update_layout(parent);
 const char *text=lv_label_get_text(label);const lv_font_t *font=lv_obj_get_style_text_font(label,0);
 lv_point_t ink;lv_txt_get_size(&ink,text,font,lv_obj_get_style_text_letter_space(label,0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
 lv_area_t area=bounds(label),box=bounds(parent);
 bool fits=ink.x<=lv_obj_get_width(label)&&contained(area,box);
 if(!fits)fprintf(stderr,"TEXT OVERFLOW %s %s %s ink=%dx%d label=%dx%d bounds=[%d,%d,%d,%d] parent=[%d,%d,%d,%d]\n",locale,name,text,ink.x,ink.y,lv_obj_get_width(label),lv_obj_get_height(label),area.x1,area.y1,area.x2,area.y2,box.x1,box.y1,box.x2,box.y2);
 assert(fits);
 uint32_t offset=0;
 while(text[offset]){
  uint32_t cp=_lv_txt_encoded_next(text,&offset);lv_font_glyph_dsc_t glyph;
  if(cp<32||(cp>=0x200b&&cp<=0x200f)||(cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069))continue;
  bool exists=lv_font_get_glyph_dsc(font,&glyph,cp,0);
  if(!exists||glyph.is_placeholder)fprintf(stderr,"MISSING GLYPH %s %s U+%04X\n",locale,name,cp);
  assert(exists&&!glyph.is_placeholder);
 }
}
static void setup_catalog(unsigned count,int selected)
{
 g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
 g_page07_curr.model.favorite_only=false;
 if(count>=32)catalog_count=(uint8_t)count;
 g_page07_curr.model.visible_count=(int)count;
 g_page07_curr.model.selected_abs_idx=selected;
 g_page07_curr.model.selected_visible_idx=selected;
 for(unsigned i=0;i<count;i++)g_page07_curr.model.visible_indices[i]=(int)i;
 curr_build_grid_layer();curr_set_mode_visible();advance(2);
}
static void destroy_grid(void)
{
 lv_obj_del(g_page07_curr.objects.grid_layer);
 assert(!g_page07_curr.objects.overview.atlas_pin);
 g_page07_curr.objects.grid_layer=NULL;g_page07_curr.objects.grid_scroll=NULL;
 g_page07_curr.objects.empty_label=NULL;
 memset(&g_page07_curr.objects.overview,0,sizeof(g_page07_curr.objects.overview));
 memset(g_page07_curr.grid_items,0,sizeof(g_page07_curr.grid_items));
 advance(2);
}
static void assert_cell_geometry(unsigned count)
{
 lv_area_t viewport=bounds(g_page07_curr.objects.grid_scroll);unsigned fully_visible=0;
 for(unsigned i=0;i<count;i++){
  page07_curr_grid_item_t *item=&g_page07_curr.grid_items[i];
  lv_area_t cell=bounds(item->item),flag=bounds(item->img),label=bounds(item->name);
  if(contained(cell,viewport))fully_visible++;
  int left=flag.x1<label.x1?flag.x1:label.x1,right=flag.x2>label.x2?flag.x2:label.x2;
  int top=flag.y1<label.y1?flag.y1:label.y1,bottom=flag.y2>label.y2?flag.y2:label.y2;
  assert(abs(left+right-cell.x1-cell.x2)<=2&&abs(top+bottom-cell.y1-cell.y2)<=2);
  text_fits(item->name,item->item,"en",codes[i]);
  assert(lv_obj_get_width(item->img)==50&&lv_obj_get_height(item->img)==33);
  assert(lv_obj_get_style_img_opa(item->img,0)==LV_OPA_COVER);
  if(i<2){assert(lv_obj_has_flag(item->fav_btn,LV_OBJ_FLAG_HIDDEN));continue;}
  lv_area_t star=bounds(item->fav_btn);
  assert(lv_obj_get_width(item->fav_btn)==16&&lv_obj_get_height(item->fav_btn)==16);
  assert(contained(star,cell));
  assert(star.x1>label.x2||star.y2<label.y1);
 }
 assert(fully_visible==(count>32?32:count));
}
static void selection_and_touch_test(void)
{
 setup_catalog(32,11);assert_cell_geometry(32);
 assert(g_page07_curr.objects.overview.page_count==1);
 assert(lv_obj_get_scroll_right(g_page07_curr.objects.grid_scroll)<=0);
 snapshot("currency-view-32-irr");
 page07_curr_grid_item_t *item=&g_page07_curr.grid_items[11];lv_area_t a=bounds(item->item),star=bounds(item->fav_btn);
 int sample_y=a.y2-14,left=a.x1+12,right=a.x2-12;
 assert(color_at(left,sample_y)==0xf3f7fc&&color_at(right,sample_y)==0xf3f7fc);
 assert(lv_obj_get_style_border_width(item->item,0)==0);
 unsigned vertical=color_at(a.x2,(a.y1+a.y2)/2),horizontal=color_at((a.x1+a.x2)/2,a.y2);
 assert(vertical!=0xffffff&&horizontal!=0xffffff);
 unsigned selections=select_requests,favorites=favorite_requests,sounds=sound_requests;
 feed((a.x1+a.x2)/2,(a.y1+a.y2)/2,LV_INDEV_STATE_PRESSED);advance(2);
 assert(color_at(left,sample_y)==0xdfe9ee&&color_at(right,sample_y)==0xdfe9ee);
 assert(color_at(a.x2,(a.y1+a.y2)/2)==vertical&&color_at((a.x1+a.x2)/2,a.y2)==horizontal);
 snapshot("currency-view-selected-pressed");
 feed((a.x1+a.x2)/2,(a.y1+a.y2)/2,LV_INDEV_STATE_RELEASED);advance(2);
 assert(select_requests==selections+1&&favorite_requests==favorites&&selected_request==11&&sound_requests==sounds+1);
 assert(color_at(left,sample_y)==0xf3f7fc&&color_at(right,sample_y)==0xf3f7fc);
 selections=select_requests;sounds=sound_requests;tap(item->fav_btn);
 assert(select_requests==selections&&favorite_requests==favorites+1&&favorite_request==11&&sound_requests==sounds+1);
 /* Only the visible 16px star intercepts selection. */
 int points[][2]={{star.x1-1,(star.y1+star.y2)/2},{star.x2+1,(star.y1+star.y2)/2},{(star.x1+star.x2)/2,star.y1-1},{(star.x1+star.x2)/2,star.y2+1}};
 for(unsigned i=0;i<4;i++){
  selections=select_requests;favorites=favorite_requests;tap_at(points[i][0],points[i][1]);
  assert(select_requests==selections+1&&favorite_requests==favorites&&selected_request==11);
 }
 for(unsigned target=0;target<2;target++)for(unsigned axis=0;axis<2;axis++){
  lv_area_t box=bounds(target?item->fav_btn:item->item);int x=(box.x1+box.x2)/2,y=(box.y1+box.y2)/2;
  selections=select_requests;favorites=favorite_requests;sounds=sound_requests;
  feed(x,y,LV_INDEV_STATE_PRESSED);
  for(int offset=4;offset<=20;offset+=4)feed(x+(axis?offset:0),y+(axis?0:offset),LV_INDEV_STATE_PRESSED);
  feed(x+(axis?20:0),y+(axis?0:20),LV_INDEV_STATE_RELEASED);advance(30);
  assert(select_requests==selections&&favorite_requests==favorites&&sound_requests==sounds);
 }
 selections=select_requests;sounds=sound_requests;tap(item->item);assert(select_requests==selections+1&&sound_requests==sounds+1);
 unsigned cards=card_requests;tap(g_page07_curr.objects.overview.card_button);
 assert(card_requests==cards+1&&g_page07_curr.model.view_mode==PAGE07_CURR_VIEW_CARD);
 assert(!g_page07_curr.objects.overview.atlas_pin&&lv_obj_has_flag(g_page07_curr.objects.grid_layer,LV_OBJ_FLAG_HIDDEN));
 tap(g_page07_curr.objects.btn_view);assert(g_page07_curr.model.view_mode==PAGE07_CURR_VIEW_GRID);
 assert(g_page07_curr.objects.overview.atlas_pin);
 unsigned filters=filter_requests;tap(g_page07_curr.objects.overview.filter[1]);assert(filter_requests==filters+1&&g_page07_curr.model.favorite_only);
 tap(g_page07_curr.objects.overview.filter[0]);assert(!g_page07_curr.model.favorite_only);
 unsigned backs=back_requests;tap(g_page07_curr.objects.overview.back_button);assert(back_requests==backs+1);
 destroy_grid();
 puts("PASS 32 currencies fit one screen; centered flags/codes, 16px independent star boundaries, selected/pressed divider pixels, drag cancellation and fresh tap, mode/filter/back");
}
static void paging_and_lifetime_test(void)
{
 unsigned opens=atlas_opens;
 for(unsigned cycle=0;cycle<3;cycle++){
  setup_catalog(34,11);assert_cell_geometry(34);
  assert(g_page07_curr.objects.overview.page_count==2&&g_page07_curr.objects.overview.page_index==0);
  unsigned selected=select_requests,favorites=favorite_requests,decoded=image_opens;
  tap(g_page07_curr.objects.overview.page_next);advance(60);
  assert(g_page07_curr.objects.overview.page_index==1);
  assert(contained(bounds(g_page07_curr.grid_items[33].item),bounds(g_page07_curr.objects.grid_scroll)));
  if(!cycle)snapshot("currency-view-34-last-page");
  tap(g_page07_curr.grid_items[33].item);assert(select_requests==selected+1&&selected_request==33);
  tap(g_page07_curr.objects.overview.page_prev);advance(60);
  assert(g_page07_curr.objects.overview.page_index==0);
  selected=select_requests;unsigned sounds=sound_requests;
  swipe(1100,200,180);
  assert(g_page07_curr.objects.overview.page_index==1);
  assert(select_requests==selected&&favorite_requests==favorites&&sound_requests==sounds);
  tap_at(200,180);
  assert(select_requests==selected&&favorite_requests==favorites&&sound_requests==sounds);
  swipe(200,1100,180);
  assert(g_page07_curr.objects.overview.page_index==0);
  assert(select_requests==selected&&favorite_requests==favorites&&sound_requests==sounds);
  tap(g_page07_curr.grid_items[11].item);
  assert(select_requests==selected+1&&selected_request==11&&sound_requests==sounds+1);
  g_page07_curr.model.selected_abs_idx=33;g_page07_curr.model.selected_visible_idx=33;
  page07_curr_overview_reveal_selection(&g_page07_curr);advance(60);
  assert(g_page07_curr.objects.overview.page_index==1);
  assert(contained(bounds(g_page07_curr.grid_items[33].item),bounds(g_page07_curr.objects.grid_scroll)));
  for(unsigned n=0;n<8;n++){tap(g_page07_curr.objects.overview.page_prev);advance(40);tap(g_page07_curr.objects.overview.page_next);advance(40);}
  assert(image_opens==decoded&&favorite_requests==favorites);
  page07_curr_overview_set_active(&g_page07_curr,false);
  lv_obj_add_flag(curr_page,LV_OBJ_FLAG_HIDDEN);curr_set_mode_visible();assert(!g_page07_curr.objects.overview.atlas_pin);
  lv_obj_clear_flag(curr_page,LV_OBJ_FLAG_HIDDEN);curr_set_mode_visible();assert(g_page07_curr.objects.overview.atlas_pin);
  lv_img_cache_invalidate_src(CURR_VIEW_ATLAS_PATH);assert(g_page07_curr.objects.overview.atlas_pin->invalidate_pending);
  /* Deletion while an input target is pressed must cancel it and release the pin. */
  lv_area_t a=bounds(g_page07_curr.grid_items[33].item);selected=select_requests;
  feed((a.x1+a.x2)/2,(a.y1+a.y2)/2,LV_INDEV_STATE_PRESSED);
  destroy_grid();feed((a.x1+a.x2)/2,(a.y1+a.y2)/2,LV_INDEV_STATE_RELEASED);advance(40);
  assert(select_requests==selected&&favorite_requests==favorites);
 }
 printf("PASS 34 currencies: Next/Prev/reveal and real horizontal swipes reach both pages without selection or sound, fresh tap succeeds; repeated paging has no decoder opens, hidden/deleted atlas release and pressed-target teardown (%u atlas decodes)\n",atlas_opens-opens);
}
static lv_indev_state_t keyboard_state;
static void keyboard_input(lv_indev_drv_t *driver,lv_indev_data_t *data)
{
 (void)driver;data->key=LV_KEY_ENTER;data->state=keyboard_state;
}
static void keyboard_activation_test(void)
{
 setup_catalog(32,11);
 lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_KEYPAD;driver.read_cb=keyboard_input;
 lv_indev_t *keyboard=lv_indev_drv_register(&driver);assert(keyboard);
 lv_group_t *group=lv_group_create();lv_indev_set_group(keyboard,group);
 lv_group_add_obj(group,g_page07_curr.grid_items[12].item);
 unsigned selected=select_requests,favorites=favorite_requests;
 keyboard_state=LV_INDEV_STATE_PRESSED;advance(4);keyboard_state=LV_INDEV_STATE_RELEASED;advance(4);
 assert(select_requests==selected+1&&selected_request==12&&favorite_requests==favorites);
 lv_group_add_obj(group,g_page07_curr.grid_items[12].fav_btn);lv_group_focus_obj(g_page07_curr.grid_items[12].fav_btn);
 keyboard_state=LV_INDEV_STATE_PRESSED;advance(4);keyboard_state=LV_INDEV_STATE_RELEASED;advance(4);
 assert(select_requests==selected+1&&favorite_requests==favorites+1&&favorite_request==12);
 lv_indev_delete(keyboard);lv_group_del(group);destroy_grid();
 puts("PASS keypad Enter activates currency and star independently without pointer-coordinate cancellation");
}
static void cached_entry_test(void)
{
 memset(favorite_values,0,sizeof(favorite_values));catalog_count=34;
 g_page07_curr.model.visible_count=34;g_page07_curr.model.selected_abs_idx=33;g_page07_curr.model.selected_visible_idx=33;
 for(int i=0;i<34;i++)g_page07_curr.model.visible_indices[i]=i;
 g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_CARD;curr_switch_cached_view();
 unsigned selected=select_requests,favorites=favorite_requests;
 tap(g_page07_curr.objects.btn_view);advance(2);
 assert(g_page07_curr.model.view_mode==PAGE07_CURR_VIEW_GRID&&g_page07_curr.objects.overview.page_index==1);
 assert(g_page07_curr.model.selected_abs_idx==33&&select_requests==selected&&favorite_requests==favorites);
 assert(contained(bounds(g_page07_curr.grid_items[33].item),bounds(g_page07_curr.objects.grid_scroll)));
 /* Card updates the shared favorite model while the Grid tree is retained. */
 tap(g_page07_curr.objects.overview.card_button);favorite_values[5]=true;
 tap(g_page07_curr.objects.btn_view);advance(2);
 char expected[120];snprintf(expected,sizeof(expected),"%s 1",ui_tr("Favorites"));
 assert(!strcmp(lv_label_get_text(g_page07_curr.objects.overview.filter_label[1]),expected));
 assert(lv_img_get_src(g_page07_curr.grid_items[5].fav_icon)==&curr_view_star_filled);
 assert(g_page07_curr.model.selected_abs_idx==33&&g_page07_curr.objects.overview.page_index==1&&select_requests==selected);
 tap(g_page07_curr.objects.overview.card_button);favorite_values[5]=false;tap(g_page07_curr.objects.btn_view);advance(2);
 snprintf(expected,sizeof(expected),"%s 0",ui_tr("Favorites"));
 assert(!strcmp(lv_label_get_text(g_page07_curr.objects.overview.filter_label[1]),expected));
 assert(lv_img_get_src(g_page07_curr.grid_items[5].fav_icon)==&curr_view_star_outline);
 destroy_grid();
 puts("PASS production cached-mode entry: first Grid entry reveals confirmed tail currency, retained Grid refreshes Card favorite count and star without selecting or sending");
}
static void locale_test(void)
{
 for(size_t language=0;language<ui_lang_count();language++){
  const ui_locale_t *locale=ui_lang_at(language);
  assert(ui_lang_prepare_resources(locale->id));ui_lang_set(locale->id);
  setup_catalog(34,1);
  assert(!strcmp(lv_label_get_text(g_page07_curr.grid_items[0].name),"AUTO"));
  assert(!strcmp(lv_label_get_text(g_page07_curr.grid_items[1].name),"MULTI"));
  for(unsigned i=0;i<34;i++)text_fits(g_page07_curr.grid_items[i].name,g_page07_curr.grid_items[i].item,locale->tag,"currency code");
  lv_obj_t *buttons[]={g_page07_curr.objects.overview.filter[0],g_page07_curr.objects.overview.filter[1],g_page07_curr.objects.overview.card_button,g_page07_curr.objects.overview.back_button};
  const char *names[]={"All","Favorites","Card view","Back"};
  for(unsigned i=0;i<4;i++)text_fits(text_child(buttons[i]),buttons[i],locale->tag,names[i]);
  char filename[80];snprintf(filename,sizeof(filename),"currency-view-%s",locale->tag);snapshot(filename);
  g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_CARD;curr_set_mode_visible();curr_style_view_button();advance(2);
  lv_obj_t *button=g_page07_curr.objects.btn_view,*label=g_page07_curr.objects.btn_view_label,*icon=g_page07_curr.objects.btn_view_icon;
  assert(lv_obj_get_width(button)==70&&lv_obj_get_height(button)==36&&lv_obj_get_x(button)==18&&lv_obj_get_y(button)==358);
  text_fits(label,button,locale->tag,"Card Grid switch");
  lv_area_t b=bounds(button),i=bounds(icon),l=bounds(label);
  assert(lv_obj_get_width(icon)==16&&lv_obj_get_height(icon)==16&&contained(i,b));
  assert(abs(i.x1+l.x2-b.x1-b.x2)<=2&&abs(i.y1+i.y2-l.y1-l.y2)<=2);
  lv_obj_t *probe=lv_label_create(lv_scr_act());lv_label_set_text(probe,ui_trc("Currency view","Grid"));
  assert(!strcmp(lv_label_get_text(label),lv_label_get_text(probe)));lv_obj_del(probe);
  snprintf(filename,sizeof(filename),"currency-card-grid-button-%s",locale->tag);snapshot(filename);
  printf("PASS locale %s: real View controls and 70x36 Card Grid button, glyphs and centered icon/text\n",locale->tag);
  destroy_grid();
 }
 ui_lang_set(LANGUAGE_EN);
}
/* Verify both delivery paths with the machine's full currency directory. */
static void currency_artwork_test(void)
{
 static const char *const directory[] = {
  "USD","AUD","CAD","GBP","EUR","TRY","AED","IQD","SAR","IRR",
  "CNY","JPY","AFN","RUB","CHF","KRW","KWD","INR","PKR","TMT",
  "AZN","AMD","GEL","OMR","BHD","QAR","YER","SYP","NZD","ZAR",
 };
 const size_t count=sizeof(directory)/sizeof(directory[0]);
 for(size_t i=0;i<count;i++) codes[i+2]=directory[i];
 g_page07_curr.model.view_mode=PAGE07_CURR_VIEW_GRID;
 g_page07_curr.model.favorite_only=false;
 g_page07_curr.model.visible_count=count+2;
 for(int i=0;i<g_page07_curr.model.visible_count;i++) g_page07_curr.model.visible_indices[i]=i;
 for(int fallback=0;fallback<2;fallback++){
  lv_img_cache_invalidate_src(CURR_VIEW_ATLAS_PATH);
  fail_atlas=fallback;
  curr_build_grid_layer();curr_set_mode_visible();
  assert(!!g_page07_curr.objects.overview.atlas_pin==!fallback);
  for(size_t i=0;i<count;i++){
   char file[24];snprintf(file,sizeof(file),"CURR_%s.png",directory[i]);
   const char *src=get_currency_img(directory[i]);
   assert(src && strrchr(src,'/') && !strcmp(strrchr(src,'/')+1,file));
   assert(asset(src));
   lv_obj_t *img=g_page07_curr.grid_items[i+2].img;
   if(fallback){
    assert(!strcmp(lv_img_get_src(img),src));
   }else{
    assert(!strcmp(lv_img_get_src(img),CURR_VIEW_ATLAS_PATH));
    bool found=false;
    for(size_t k=0;k<sizeof(curr_view_atlas_entries)/sizeof(curr_view_atlas_entries[0]);k++){
     if(strcmp(file,curr_view_atlas_entries[k].file)) continue;
     assert(lv_img_get_offset_x(img)==-curr_view_atlas_entries[k].x);
     assert(lv_img_get_offset_y(img)==-curr_view_atlas_entries[k].y);
     found=true;break;
    }
    assert(found);
   }
  }
  if(!fallback){
   snapshot("currency-art-top");
   page07_curr_overview_reveal_selection(&g_page07_curr);
   snapshot("currency-art-scrolled");
  }
  destroy_grid();
 }
 fail_atlas=false;
 puts("PASS 30 requested codes resolve to their own external flags and exact overview atlas tiles, including atlas failure");
}

int main(void)
{
 setvbuf(stdout,NULL,_IOLBF,0);
 lv_init();touch_sound_init(request_sound);g_feedback_observer=touch_sound_feedback;ui_lang_set_resource_preparer(ui_locale_font_prepare);ui_message_font_set_provider(ui_locale_font_compose);
 lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
 lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=1280;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;
 lv_disp_t *display=lv_disp_drv_register(&dd);ui_scrollbar_init(display);lv_img_cache_set_size(48);
 lv_img_decoder_t *decoder=lv_img_decoder_create();lv_img_decoder_set_info_cb(decoder,image_info);lv_img_decoder_set_open_cb(decoder,image_open);lv_img_decoder_set_close_cb(decoder,image_close);
 lv_indev_drv_t driver;lv_indev_drv_init(&driver);driver.type=LV_INDEV_TYPE_POINTER;driver.read_cb=input;driver.feedback_cb=evdev_feedback;lv_indev_drv_register(&driver);
 g_page07_curr.objects.root=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(g_page07_curr.objects.root);
 lv_obj_set_size(g_page07_curr.objects.root,1280,400);lv_obj_clear_flag(g_page07_curr.objects.root,LV_OBJ_FLAG_SCROLLABLE);curr_page=g_page07_curr.objects.root;
 g_page07_curr.objects.left_panel=lv_obj_create(curr_page);lv_obj_remove_style_all(g_page07_curr.objects.left_panel);
 lv_obj_set_size(g_page07_curr.objects.left_panel,288,400);lv_obj_clear_flag(g_page07_curr.objects.left_panel,LV_OBJ_FLAG_SCROLLABLE);
 g_page07_curr.objects.right_area=lv_obj_create(curr_page);lv_obj_remove_style_all(g_page07_curr.objects.right_area);
 lv_obj_set_pos(g_page07_curr.objects.right_area,288,0);lv_obj_set_size(g_page07_curr.objects.right_area,992,400);lv_obj_clear_flag(g_page07_curr.objects.right_area,LV_OBJ_FLAG_SCROLLABLE);
 host_create_card_view_button();curr_style_view_button();
 selection_and_touch_test();paging_and_lifetime_test();keyboard_activation_test();cached_entry_test();locale_test();currency_artwork_test();
 setup_catalog(0,0);snapshot("currency-view-empty");assert(g_page07_curr.objects.overview.back_button&&g_page07_curr.objects.overview.card_button);destroy_grid();
 setup_catalog(5,4);g_page07_curr.model.favorite_only=true;page07_curr_overview_selection(&g_page07_curr);snapshot("currency-view-favorites");destroy_grid();
 lv_img_cache_invalidate_src(CURR_VIEW_ATLAS_PATH);fail_atlas=true;setup_catalog(34,11);
 assert(!g_page07_curr.objects.overview.atlas_pin);snapshot("currency-view-fallback");
 fail_atlas=false;page07_curr_overview_set_active(&g_page07_curr,true);advance(2);
 assert(g_page07_curr.objects.overview.atlas_pin);snapshot("currency-view-recovered");destroy_grid();
 lv_obj_del(g_page07_curr.objects.root);lv_img_cache_invalidate_src(NULL);advance(2);
 puts("PASS actual LVGL Currency View: 32/34, selection pixels, independent star hit routing, gestures, locales, atlas failure and lifecycle. Protocol ACK is covered separately; board performance is not measured.");
 return 0;
}