#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_system/ui_lang.h"
static unsigned font_bindings;
static uint32_t locale_generation=1;
static bool selected_locale;
const lv_font_t *ui_message_font(const lv_font_t *f){font_bindings++;return f;}
const lv_font_t *ui_message_font_sized(const lv_font_t *f,unsigned pixels){(void)pixels;return f;}
const char *ui_tr(const char *s){
 if(selected_locale){
  if(!strcmp(s,"Getting ready."))return "Preparation";
  if(!strcmp(s,"Configuration"))return "Configuration FR";
  if(!strcmp(s,"Waiting"))return "Attente";
  if(!strcmp(s,"Ready to count."))return "Pret";
 }
 return s;
}
language_t ui_lang_get(void){return LANGUAGE_EN;}
uint32_t ui_lang_generation(void){return locale_generation;}

#include "un260/lv_core/boot_anim/page_08_boot_flow.c"
#include "un260/lv_core/page_00_boot_anim.h"
static ui_page_t current=UI_PAGE_BOOT;
static boot_snapshot_t snapshot;
static unsigned pixels,opens;
static bool fail_timer,fail_object;
static const char *fail_image;
static unsigned decoder_attempts;
static uint8_t *bg,*icon,*ready_bg;
static lv_color_t framebuffer[1280*400],buffer[1280*40];
void boot_service_snapshot(boot_snapshot_t *s){*s=snapshot;}
ui_page_t ui_manager_get_current_page(void){return current;}
void ui_manager_switch(ui_page_t p){current=p;}
static bool bridge_input_held;
static unsigned bridge_holds,bridge_releases;
void ui_manager_hold_transition_input(ui_page_t owner,bool hold){
 assert(owner==UI_PAGE_BOOT);
 if(hold){assert(!bridge_input_held);bridge_input_held=true;bridge_holds++;}
 else {assert(bridge_input_held);bridge_input_held=false;bridge_releases++;}
}
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t,uint32_t,void*);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t cb,uint32_t ms,void *d){if(fail_timer){fail_timer=false;return NULL;}return __real_lv_timer_create(cb,ms,d);}
lv_obj_t *__real_lv_obj_create(lv_obj_t*);
lv_obj_t *__wrap_lv_obj_create(lv_obj_t *parent){if(fail_object){fail_object=false;return NULL;}return __real_lv_obj_create(parent);}
static lv_res_t info(lv_img_decoder_t *d,const void *s,lv_img_header_t *h){
 (void)d;if(lv_img_src_get_type(s)!=LV_IMG_SRC_FILE)return LV_RES_INV;
 bool full=strstr(s,"background.png")!=NULL||strstr(s,"backgrounds/boot.png")!=NULL;if(!full&&!strstr(s,"emblem.png"))return LV_RES_INV;
 memset(h,0,sizeof(*h));h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;h->w=full?1280:96;h->h=full?400:96;return LV_RES_OK;
}
static lv_res_t open_image(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){decoder_attempts++;if(fail_image&&!strcmp(s->src,fail_image))return LV_RES_INV;if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=strstr(s->src,"backgrounds/boot.png")?ready_bg:s->header.w==1280?bg:icon;opens++;return LV_RES_OK;}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){assert(a->x1>=0&&a->y1>=0&&a->x2<1280&&a->y2<400);unsigned w=a->x2-a->x1+1;pixels+=w*(a->y2-a->y1+1);for(int y=a->y1;y<=a->y2;y++)memcpy(framebuffer+y*1280+a->x1,p+(y-a->y1)*w,w*4);lv_disp_flush_ready(d);}
static uint8_t *load(const char *name,size_t size){char path[2048];snprintf(path,sizeof(path),"%s/%s",getenv("BOOT_THEME_C_PIXEL_INPUT"),name);FILE*f=fopen(path,"rb");assert(f);uint8_t*p=malloc(size);assert(p&&fread(p,1,size,f)==size);fclose(f);return p;}
static void frame(unsigned ms){lv_tick_inc(ms);lv_timer_handler();lv_refr_now(NULL);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))n++;return n;}
static void dump(const char *name){const char *dir=getenv("BOOT_THEME_C_RASTER_OUTPUT");if(!dir)return;char path[2048];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1280 400\n255\n");for(unsigned i=0;i<1280*400;i++){unsigned v=framebuffer[i].full;fputc(v>>16,f);fputc(v>>8,f);fputc(v,f);}fclose(f);}
static lv_obj_t *find_text(lv_obj_t *parent,const char *text){
 if(lv_obj_check_type(parent,&lv_label_class)&&!strcmp(lv_label_get_text(parent),text))return parent;
 for(unsigned i=0;i<lv_obj_get_child_cnt(parent);i++){lv_obj_t *found=find_text(lv_obj_get_child(parent,i),text);if(found)return found;}
 return NULL;
}
static void assert_layout_clean(lv_obj_t *obj){
 assert(!obj->layout_inv);
 for(unsigned i=0;i<lv_obj_get_child_cnt(obj);i++)assert_layout_clean(lv_obj_get_child(obj,i));
}
int main(void){
 bg=load("background.bgra",1280*400*4);icon=load("emblem.bgra",96*96*4);
 ready_bg=load("boot.bgra",1280*400*4);
 lv_init();lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,buffer,NULL,1280*40);lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&draw;driver.flush_cb=flush;lv_disp_drv_register(&driver);
 lv_img_decoder_t*d=lv_img_decoder_create();lv_img_decoder_set_info_cb(d,info);lv_img_decoder_set_open_cb(d,open_image);unsigned baseline=timers();
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_00_boot_anim_create(lv_layer_top());ui_page_00_boot_anim_set_startup_ready(false);
 ui_page_00_boot_anim_adopt_elapsed(4300);frame(20);dump("welcome");
 for(unsigned y=0;y<80;y++)for(unsigned x=0;x<1280;x++){
  unsigned n=y*1280+x,expected;memcpy(&expected,bg+n*4,4);
  if((framebuffer[n].full&0xffffff)!=(expected&0xffffff)){fprintf(stderr,"static background mismatch %u,%u got=%08x want=%08x\n",x,y,framebuffer[n].full,expected);assert(0);}
 }
 for(unsigned i=0;i<100;i++)frame(14);
 assert(ui_page_00_boot_anim_is_active());assert(view.covered&&view.timer&&view.timer->paused);ui_page_00_boot_anim_set_startup_ready(true);
 for(unsigned i=0;i<100;i++)frame(14);assert(ui_page_00_boot_anim_is_active()); /* Never cut short three dot cycles. */
 for(unsigned i=0;i<300;i++)frame(14);assert(!ui_page_00_boot_anim_is_active());assert(!view.covered);
 pixels=0;unsigned waiting_opens=opens;for(unsigned i=0;i<40;i++)frame(14);
 assert(pixels>0 && opens==waiting_opens);dump("handshake-wait");
 assert(view.phase[0]==CHECK&&!snapshot.connected&&snapshot.requested_count==0&&snapshot.completed_count==0);
 assert(view.waiting&&lv_obj_get_width(view.progress)==0);
 assert(lv_theme_default_is_inited());
 assert(lv_obj_get_style_bg_opa(view.waiting,LV_PART_KNOB)==LV_OPA_TRANSP);
 assert(!strcmp(lv_label_get_text(view.caption),"Checking your system."));
 assert(lv_obj_get_x(view.caption)>=lv_obj_get_x(view.waiting)+lv_obj_get_width(view.waiting)+16);
 for(unsigned i=1;i<5;i++)assert(view.phase[i]==WAIT);
 snapshot.stage=BOOT_STAGE_FAIL;frame(20);
 assert(view.phase[0]==FAIL&&!view.done&&!view.waiting&&snapshot.completed_count==0);
 assert(!strcmp(lv_label_get_text(view.caption),"Check the reported fault before continuing."));
 assert(lv_img_get_src(view.marks[0])==&selfcheck_fail);
 snapshot.stage=BOOT_STAGE_SENSOR;frame(20);assert(view.phase[0]==CHECK);
 assert(lv_obj_get_style_text_font(view.section,0)==&lv_font_instrument_sans_semibold_14);
 assert(lv_obj_get_style_text_font(view.count,0)==&lv_font_instrument_sans_semibold_14);
 assert(lv_obj_get_style_text_font(view.percent,0)==&lv_font_instrument_sans_semibold_16);
 assert(lv_obj_get_style_text_align(view.section,0)==LV_TEXT_ALIGN_RIGHT);
 assert(lv_obj_get_style_text_align(view.count,0)==LV_TEXT_ALIGN_RIGHT);
 assert(lv_obj_get_style_text_align(view.percent,0)==LV_TEXT_ALIGN_RIGHT);
 assert(lv_obj_get_y(view.percent)>lv_obj_get_y(view.track)+lv_obj_get_height(view.track));
 snapshot.connected=true;snapshot.requested_count=1;snapshot.stage=BOOT_STAGE_SENSOR;frame(20);
 assert(!strcmp(lv_label_get_text(view.caption),"Checking your system."));
 for(unsigned i=0;i<5;i++){
  snapshot.requested_count=i+1;frame(20);assert(view.phase[i]==CHECK);
  frame(360);assert(lv_obj_get_style_bg_opa(view.icons[i],0)==0);assert(lv_obj_get_style_bg_opa(view.cores[i],0)>0);
  dump("checking-ring");for(unsigned j=0;j<410;j++)frame(14);
  pixels=0;unsigned decodes=opens;for(unsigned j=0;j<30;j++)frame(14);assert(view.waiting&&pixels>0&&opens==decodes);
  snapshot.items[i].received=true;snapshot.items[i].result=1;snapshot.completed_count=i+1;frame(20);
  assert(view.phase[i]==PASS);
  assert(lv_img_get_src(view.marks[i])==&selfcheck_pass);
  assert(!lv_obj_has_flag(view.marks[i],LV_OBJ_FLAG_HIDDEN));
  assert(lv_obj_get_style_border_opa(view.icons[i],0)==0);
  if(i==2)dump("selfcheck");
 }
 snapshot.stage=BOOT_STAGE_DONE;frame(20);assert(!ui_page_08_curr_visual_is_quiet());for(unsigned i=0;i<100;i++)frame(14);assert(!strcmp(lv_label_get_text(view.ready.title),"Ready to count."));dump("ready");
 for(unsigned i=0;i<90;i++)frame(14);assert(ui_page_08_curr_visual_is_quiet());
 assert(ui_page_08_curr_ready_hold_complete()&&!view.waiting);
 assert(lv_obj_get_style_text_opa(view.caption,0)==0);
 unsigned ready_opens=opens;pixels=0;for(unsigned i=0;i<20;i++)frame(14);assert(pixels==0&&opens==ready_opens);
 ready_render(&view.ready,1750,0);
 assert(lv_obj_get_style_text_opa(view.ready.title,0)==255);
 assert(lv_obj_get_style_text_opa(view.ready.sub,0)>0&&lv_obj_get_style_text_opa(view.ready.sub,0)<255);
 assert(lv_obj_get_style_text_opa(view.ready.signature,0)==0);
 ready_render(&view.ready,1920,0);assert(lv_obj_get_style_text_opa(view.ready.sub,0)==255);
 assert(lv_obj_get_style_text_opa(view.ready.signature,0)<255);
 ready_render(&view.ready,2170,0);assert(lv_obj_get_style_text_opa(view.ready.signature,0)==255);
 frame(20);
 assert(lv_obj_get_style_bg_opa(view.track,0)==0);assert(lv_obj_get_style_text_opa(view.section,0)==0);dump("ready-clean");
 ui_page_08_curr_start_handoff();assert(bridge_input_held);
 unsigned before_repeat=timers();ui_page_08_curr_start_handoff();
 assert(bridge_input_held&&timers()==before_repeat&&bridge_releases+1==bridge_holds);
 current=UI_PAGE_MAIN;ui_page_08_curr_destroy();frame(1200);
 assert(bridge.root&&bridge.phase==1&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(180);assert(bridge.phase==2&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(200);
 assert(bridge.root&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(160);assert(lv_obj_get_style_text_opa(bridge.ready.title,0)==0);
 assert(lv_obj_get_style_text_opa(bridge.ready.sub,0)==0&&lv_obj_get_style_text_opa(bridge.ready.signature,0)==0);
 frame(40);dump("handoff-text-retired");
 frame(300);assert(bridge.root&&lv_obj_get_style_img_opa(bridge.image,0)>100&&lv_obj_get_style_img_opa(bridge.image,0)<155);
 frame(310);assert(!bridge.root&&timers()==baseline&&!bridge_input_held);
 assert(bridge_holds==bridge_releases);
 current=UI_PAGE_BOOT;ui_page_08_curr_create(NULL);frame(20);frame(2600);
 ui_page_08_curr_start_handoff();current=UI_PAGE_PURE;ui_page_08_curr_destroy();frame(40);
 assert(bridge.root&&bridge_input_held);current=UI_PAGE_BOOT;frame(40);assert(!bridge.root&&!bridge_input_held);
 ui_page_08_curr_create(NULL);frame(20);frame(2600);fail_timer=true;ui_page_08_curr_start_handoff();
 assert(!bridge.root&&!bridge_input_held&&bridge_holds==bridge_releases);ui_page_08_curr_destroy();assert(timers()==baseline);
 current=UI_PAGE_BOOT;memset(&snapshot,0,sizeof(snapshot));snapshot.connected=true;snapshot.requested_count=3;snapshot.completed_count=3;snapshot.items[0]=(boot_item_result_t){true,1};snapshot.items[1]=(boot_item_result_t){true,2};snapshot.items[2]=(boot_item_result_t){true,1};snapshot.stage=BOOT_STAGE_FAIL;
 ui_page_08_curr_create(NULL);frame(20);assert(view.phase[1]==FAIL&&!view.done);boot_progress_set(100);boot_selftest_list_finish();assert(view.phase[1]==FAIL&&!view.done);for(unsigned i=0;i<70;i++)frame(14);dump("fault");ui_page_08_curr_destroy();assert(timers()==baseline);
 memset(&snapshot,0,sizeof(snapshot));ui_page_08_curr_create(NULL);
 assert(view.phase[0]==CHECK&&!strcmp(lv_label_get_text(view.states[0]),"Checking"));
 frame(650);assert(lv_obj_get_y(view.cards[0])==221);
 frame(320);assert(lv_obj_get_y(view.cards[0])==221);
 frame(360);assert(lv_obj_get_y(view.cards[0])<221&&snapshot.requested_count==0);
 snapshot.items[0]=(boot_item_result_t){true,2};frame(20);
 assert(view.phase[0]==FAIL&&!view.done);ui_page_08_curr_destroy();
 memset(&snapshot,0,sizeof(snapshot));ui_page_08_curr_create(NULL);fail_timer=true;ui_page_00_boot_anim_create(lv_layer_top());assert(!ui_page_00_boot_anim_is_active());ui_page_00_boot_anim_poll();ui_page_08_curr_destroy();assert(timers()==baseline);
 /* Native preparation builds both decoded surfaces, spinner and paused timer
  * before R. Uncover performs no allocation or PNG decode. */
 memset(&snapshot,0,sizeof(snapshot));frame(20);
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
 unsigned steps=0;while(!ui_page_08_curr_prepare_step()){assert(!ui_page_08_curr_prepare_failed());assert(++steps<9);}
 assert(!ui_page_08_curr_prepare_failed()&&view.waiting&&view.timer&&view.timer->paused);
 assert_layout_clean(view.root);
 assert(!lv_obj_get_screen(view.root)->scr_layout_inv);
 assert(view.title->coords.x1==52&&view.title->coords.y1==110&&view.title->coords.x2==851);
 for(unsigned i=0;i<5;i++){
  assert(view.cards[i]->coords.x1==52+(int)i*238&&view.cards[i]->coords.y1==216);
  assert(view.cards[i]->coords.x2-view.cards[i]->coords.x1+1==224);
  assert(view.cards[i]->coords.y2-view.cards[i]->coords.y1+1==88);
  assert(view.labels[i]->coords.x2>=view.labels[i]->coords.x1&&view.labels[i]->coords.y2>=view.labels[i]->coords.y1);
 }
 lv_timer_t *prepared_timer=view.timer;lv_obj_t *prepared_waiting=view.waiting;
 for(unsigned i=0;i<5;i++)assert(lv_obj_get_style_shadow_width(view.cards[i],0)==0);
 frame(20);pixels=0;frame(20);assert(pixels==0);
 unsigned prepared_opens=opens,prepared_timers=timers(),prepared_fonts=font_bindings;
 /* Native ownership can hold the display without running any LVGL timer.
  * Hidden preparation time must not jump the spinner into its first cycle. */
 lv_tick_inc(550);
 ui_page_08_curr_set_covered(false);
 assert(view.timer==prepared_timer&&view.waiting==prepared_waiting&&!view.timer->paused);
 assert(opens==prepared_opens&&timers()==prepared_timers&&font_bindings==prepared_fonts);
 frame(20);assert(!view.covered&&pixels>0&&view.phase[0]==CHECK&&!view.done);
 assert(lv_arc_get_angle_start(view.waiting)<10);
 assert(lv_arc_get_angle_end(view.waiting)>=72&&lv_arc_get_angle_end(view.waiting)<90);
 assert(lv_obj_get_width(view.progress)==0&&snapshot.completed_count==0);
 unsigned rendered_opens=opens;for(unsigned i=0;i<30;i++)frame(14);assert(opens==rendered_opens);
 dump("native-handoff");ui_page_08_curr_destroy();assert(timers()==baseline);

 /* Keep the same visible-clock behavior through slow native preparation,
  * long holds, cover/resume and the 32-bit tick rollover. No spinner animation
  * is attached to the global LVGL animation timer. */
 const uint32_t hidden_holds[]={1,550,1600,5000};
 for(unsigned hold=0;hold<4;hold++){
  memset(&snapshot,0,sizeof(snapshot));
  if(hold==3)lv_tick_inc(UINT32_MAX-lv_tick_get()-100U);
  ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
  while(!ui_page_08_curr_prepare_step())assert(!ui_page_08_curr_prepare_failed());
  assert(view.waiting&&lv_anim_get(view.waiting,NULL)==NULL);
  assert(lv_arc_get_angle_start(view.waiting)==0&&lv_arc_get_angle_end(view.waiting)==72);
  lv_tick_inc(hidden_holds[hold]);
  ui_page_08_curr_set_covered(false);frame(20);
  assert(lv_arc_get_angle_start(view.waiting)<10&&lv_arc_get_angle_end(view.waiting)>72&&lv_arc_get_angle_end(view.waiting)<90);
  frame(205);assert(lv_arc_get_angle_start(view.waiting)>40&&lv_arc_get_angle_start(view.waiting)<80);
  assert(lv_arc_get_angle_end(view.waiting)==162);
  frame(675);assert(lv_arc_get_angle_start(view.waiting)==0&&lv_arc_get_angle_end(view.waiting)==72);
  ui_page_08_curr_set_covered(true);lv_tick_inc(1400);ui_page_08_curr_set_covered(false);frame(20);
  assert(view.waiting&&lv_anim_get(view.waiting,NULL)==NULL);
  assert(lv_arc_get_angle_start(view.waiting)<10&&lv_arc_get_angle_end(view.waiting)>72&&lv_arc_get_angle_end(view.waiting)<90);
  ui_page_08_curr_destroy();assert(timers()==baseline);
 }
 puts("PASS D: self-check arc uses visible page time through native holds, cover/resume and tick rollover; first cycle does not catch up hidden time");

 /* Locale can be published between construction steps. Final preparation
  * binds the current translation and actual geometry once while still hidden. */
 frame(20);ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
 for(unsigned i=0;i<8;i++)assert(!ui_page_08_curr_prepare_step()&&!ui_page_08_curr_prepare_failed());
 assert(view.step==9&&view.timer&&view.timer->paused);
 selected_locale=true;locale_generation++;
 assert(ui_page_08_curr_prepare_step()&&!ui_page_08_curr_prepare_failed());
 assert(view.language_generation==locale_generation);
 assert(!strcmp(lv_label_get_text(view.title),"Preparation"));
 assert(!strcmp(lv_label_get_text(view.labels[0]),"Configuration FR"));
 assert(!strcmp(lv_label_get_text(view.states[1]),"Attente"));
 assert(!strcmp(lv_label_get_text(view.ready.title),"Pret"));
 assert_layout_clean(view.root);assert(lv_obj_has_flag(view.root,LV_OBJ_FLAG_HIDDEN));
 unsigned stable_fonts=font_bindings,stable_opens=opens,stable_timers=timers();
 for(unsigned i=0;i<20;i++)assert(ui_page_08_curr_prepare_step());
 assert(stable_fonts==font_bindings&&stable_opens==opens&&stable_timers==timers());
 pixels=0;frame(20);assert(pixels==0);
 ui_page_08_curr_set_covered(false);assert(stable_fonts==font_bindings);
 selected_locale=false;locale_generation++;frame(20);
 assert(!strcmp(lv_label_get_text(view.title),"Getting ready."));
 assert(!strcmp(lv_label_get_text(view.labels[0]),"Configuration"));
 ui_page_08_curr_destroy();assert(timers()==baseline);
 puts("PASS D: hidden-root layout completed with genuine child coordinates; current locale bound before uncover; no repeated preparation/font binding; late locale publication and live refresh");
 /* Failed root, two real decoder failures, card allocation, and visual timer
  * setup remain failed without repeated work or an inferred Ready state. */
 fail_object=true;ui_page_08_curr_create(NULL);
 assert(ui_page_08_curr_prepare_failed()&&!view.root&&!ui_page_08_curr_prepare_step());
 ui_page_08_curr_destroy();assert(timers()==baseline&&!ui_page_08_curr_prepare_failed());
 const char *broken[]={background,ready_background};
 for(unsigned j=0;j<2;j++){
  lv_img_cache_invalidate_src(background);lv_img_cache_invalidate_src(ready_background);
  ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
  if(j)assert(!ui_page_08_curr_prepare_step()&&!ui_page_08_curr_prepare_failed());
  fail_image=broken[j];assert(!ui_page_08_curr_prepare_step());
  assert(ui_page_08_curr_prepare_failed()&&!view.timer&&!view.waiting);
  unsigned attempts=decoder_attempts;
  for(unsigned i=0;i<12;i++)assert(!ui_page_08_curr_prepare_step());
  ui_page_08_curr_set_covered(false);
  assert(decoder_attempts==attempts&&lv_obj_has_flag(view.root,LV_OBJ_FLAG_HIDDEN));
  fail_image=NULL;ui_page_08_curr_destroy();assert(timers()==baseline);
 }
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
 assert(!ui_page_08_curr_prepare_step());assert(!ui_page_08_curr_prepare_step());
 fail_object=true;assert(!ui_page_08_curr_prepare_step()&&ui_page_08_curr_prepare_failed());
 ui_page_08_curr_destroy();assert(timers()==baseline);
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_08_curr_set_covered(true);
 for(unsigned i=0;i<7;i++)assert(!ui_page_08_curr_prepare_step()&&!ui_page_08_curr_prepare_failed());
 fail_timer=true;assert(!ui_page_08_curr_prepare_step()&&ui_page_08_curr_prepare_failed());
 assert(!view.timer&&!view.waiting);ui_page_08_curr_destroy();assert(timers()==baseline);
 /* An actual legacy intro must consume next-surface failure instead of
  * remaining in the managed Welcome gate after its three cycles. */
 for(unsigned failure=0;failure<3;failure++){
  memset(&snapshot,0,sizeof(snapshot));
  lv_img_cache_invalidate_src(ready_background);
  ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);
  ui_page_00_boot_anim_create(lv_layer_top());ui_page_00_boot_anim_set_startup_ready(true);frame(20);
  assert(ui_page_00_boot_anim_is_active()&&view.covered);
  if(failure==0){assert(!ui_page_08_curr_prepare_step());fail_image=ready_background;}
  if(failure==1){assert(!ui_page_08_curr_prepare_step());assert(!ui_page_08_curr_prepare_step());fail_object=true;}
  if(failure==2){for(unsigned i=0;i<7;i++)assert(!ui_page_08_curr_prepare_step());fail_timer=true;}
  frame(2400);
  assert(ui_page_08_curr_prepare_failed()&&ui_page_00_boot_anim_is_active()&&view.covered);
  assert(find_text(lv_layer_top(),"STARTUP ERROR")&&!view.timer&&!view.done);
  unsigned failed_attempts=decoder_attempts;
  for(unsigned i=0;i<10;i++)frame(1000);
  assert(decoder_attempts==failed_attempts&&find_text(lv_layer_top(),"STARTUP ERROR"));
  ui_page_00_boot_anim_set_startup_error(true);
  assert(find_text(lv_layer_top(),"DIAGNOSTICS"));
  fail_image=NULL;ui_page_00_boot_anim_destroy();ui_page_08_curr_destroy();frame(20);
  assert(timers()==baseline);
 }
 puts("PASS D: real legacy intro consumes decoder/card/timer preparation failures; localized error, diagnostics and cleanup; no repeated preparation");
 puts("PASS D: native covered preparation, decoded backgrounds, prebuilt spinner/timer, no hidden redraw, allocation-free uncover, zero card shadows");
 puts("PASS D: root/background/ready-background/card/timer failures close readiness without retries; owned resources fully destroyed");
 puts("PASS D: bridge owns transition input through repeated start, normal completion, PURE cancellation and timer failure; balanced release");
 puts("PASS D: real LVGL intro adoption, snapshot replay, three-cycle idle zero redraw, fault preservation, Ready gate, bridge cleanup and timer failure");return 0;
}
