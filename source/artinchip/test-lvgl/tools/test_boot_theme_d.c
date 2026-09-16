#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/lv_core/boot_anim/page_08_boot_flow.c"
#include "un260/lv_core/page_00_boot_anim.h"
static ui_page_t current=UI_PAGE_BOOT;
static boot_snapshot_t snapshot;
static unsigned pixels,opens;
static bool fail_timer;
static uint8_t *bg,*icon,*ready_bg;
static lv_color_t framebuffer[1280*400],buffer[1280*40];
void boot_service_snapshot(boot_snapshot_t *s){*s=snapshot;}
ui_page_t ui_manager_get_current_page(void){return current;}
void ui_manager_switch(ui_page_t p){current=p;}
lv_timer_t *__real_lv_timer_create(lv_timer_cb_t,uint32_t,void*);
lv_timer_t *__wrap_lv_timer_create(lv_timer_cb_t cb,uint32_t ms,void *d){if(fail_timer){fail_timer=false;return NULL;}return __real_lv_timer_create(cb,ms,d);}
static lv_res_t info(lv_img_decoder_t *d,const void *s,lv_img_header_t *h){
 (void)d;if(lv_img_src_get_type(s)!=LV_IMG_SRC_FILE)return LV_RES_INV;
 bool full=strstr(s,"background.png")!=NULL||strstr(s,"backgrounds/user.png")!=NULL;if(!full&&!strstr(s,"emblem.png"))return LV_RES_INV;
 memset(h,0,sizeof(*h));h->cf=LV_IMG_CF_TRUE_COLOR_ALPHA;h->w=full?1280:96;h->h=full?400:96;return LV_RES_OK;
}
static lv_res_t open_image(lv_img_decoder_t *d,lv_img_decoder_dsc_t *s){if(info(d,s->src,&s->header)!=LV_RES_OK)return LV_RES_INV;s->img_data=strstr(s->src,"backgrounds/user.png")?ready_bg:s->header.w==1280?bg:icon;opens++;return LV_RES_OK;}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){assert(a->x1>=0&&a->y1>=0&&a->x2<1280&&a->y2<400);unsigned w=a->x2-a->x1+1;pixels+=w*(a->y2-a->y1+1);for(int y=a->y1;y<=a->y2;y++)memcpy(framebuffer+y*1280+a->x1,p+(y-a->y1)*w,w*4);lv_disp_flush_ready(d);}
static uint8_t *load(const char *name,size_t size){char path[2048];snprintf(path,sizeof(path),"%s/%s",getenv("BOOT_THEME_C_PIXEL_INPUT"),name);FILE*f=fopen(path,"rb");assert(f);uint8_t*p=malloc(size);assert(p&&fread(p,1,size,f)==size);fclose(f);return p;}
static void frame(unsigned ms){lv_tick_inc(ms);lv_timer_handler();lv_refr_now(NULL);}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))n++;return n;}
static void dump(const char *name){const char *dir=getenv("BOOT_THEME_C_RASTER_OUTPUT");if(!dir)return;char path[2048];snprintf(path,sizeof(path),"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1280 400\n255\n");for(unsigned i=0;i<1280*400;i++){unsigned v=framebuffer[i].full;fputc(v>>16,f);fputc(v>>8,f);fputc(v,f);}fclose(f);}
int main(void){
 bg=load("background.bgra",1280*400*4);icon=load("emblem.bgra",96*96*4);
 ready_bg=load("user.bgra",1280*400*4);
 lv_init();lv_disp_draw_buf_t draw;lv_disp_draw_buf_init(&draw,buffer,NULL,1280*40);lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=400;driver.draw_buf=&draw;driver.flush_cb=flush;lv_disp_drv_register(&driver);
 lv_img_decoder_t*d=lv_img_decoder_create();lv_img_decoder_set_info_cb(d,info);lv_img_decoder_set_open_cb(d,open_image);unsigned baseline=timers();
 ui_page_08_curr_defer_next_create();ui_page_08_curr_create(NULL);ui_page_00_boot_anim_create(lv_layer_top());ui_page_00_boot_anim_set_startup_ready(false);
 ui_page_00_boot_anim_adopt_elapsed(4300);frame(20);dump("welcome");
 for(unsigned y=0;y<80;y++)for(unsigned x=0;x<1280;x++){
  unsigned n=y*1280+x,expected;memcpy(&expected,bg+n*4,4);
  if((framebuffer[n].full&0xffffff)!=(expected&0xffffff)){fprintf(stderr,"static background mismatch %u,%u got=%08x want=%08x\n",x,y,framebuffer[n].full,expected);assert(0);}
 }
 for(unsigned i=0;i<100;i++)frame(14);
 assert(ui_page_00_boot_anim_is_active());assert(view.covered&&!view.timer);ui_page_00_boot_anim_set_startup_ready(true);
 for(unsigned i=0;i<100;i++)frame(14);assert(ui_page_00_boot_anim_is_active()); /* Never cut short three dot cycles. */
 for(unsigned i=0;i<300;i++)frame(14);assert(!ui_page_00_boot_anim_is_active());assert(!view.covered);
 pixels=0;unsigned waiting_opens=opens;for(unsigned i=0;i<40;i++)frame(14);
 assert(pixels>0 && opens==waiting_opens);dump("handshake-wait");
 assert(view.phase[0]==CHECK&&!snapshot.connected&&snapshot.requested_count==0&&snapshot.completed_count==0);
 assert(view.waiting&&lv_obj_get_width(view.progress)==0);
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
 ui_page_08_curr_start_handoff();current=UI_PAGE_MAIN;ui_page_08_curr_destroy();frame(1200);
 assert(bridge.root&&bridge.phase==1&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(180);assert(bridge.phase==2&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(200);
 assert(bridge.root&&lv_obj_get_style_img_opa(bridge.image,0)==255);
 frame(160);assert(lv_obj_get_style_text_opa(bridge.ready.title,0)==0);
 assert(lv_obj_get_style_text_opa(bridge.ready.sub,0)==0&&lv_obj_get_style_text_opa(bridge.ready.signature,0)==0);
 frame(40);dump("handoff-text-retired");
 frame(300);assert(bridge.root&&lv_obj_get_style_img_opa(bridge.image,0)>100&&lv_obj_get_style_img_opa(bridge.image,0)<155);
 frame(310);assert(!bridge.root&&timers()==baseline);
 current=UI_PAGE_BOOT;ui_page_08_curr_create(NULL);frame(20);frame(2600);
 ui_page_08_curr_start_handoff();current=UI_PAGE_PURE;ui_page_08_curr_destroy();frame(40);
 assert(bridge.root);current=UI_PAGE_BOOT;frame(40);assert(!bridge.root);
 ui_page_08_curr_create(NULL);frame(20);frame(2600);fail_timer=true;ui_page_08_curr_start_handoff();
 assert(!bridge.root);ui_page_08_curr_destroy();assert(timers()==baseline);
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
 puts("PASS D: real LVGL intro adoption, snapshot replay, three-cycle idle zero redraw, fault preservation, Ready gate, bridge cleanup and timer failure");return 0;
}
