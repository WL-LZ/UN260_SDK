#include "un260/gesture/touch_sound.h"
#include <assert.h>
#include <stdio.h>
static unsigned sounds, clicks;
static lv_indev_t *pointer;
static lv_point_t point;
static bool down;
static void sound(void){sounds++;}
static void read_input(lv_indev_drv_t *d,lv_indev_data_t *v){(void)d;v->point=point;v->state=down?LV_INDEV_STATE_PR:LV_INDEV_STATE_REL;}
static lv_obj_t *evdev_pressed_obj;
static lv_point_t evdev_press_point;
static bool evdev_visual_cancelled,evdev_press_cancelled;
static void (*g_feedback_observer)(lv_indev_t*,lv_event_code_t)=touch_sound_feedback;
#include "actual_evdev_feedback.h"
static void feedback(lv_indev_drv_t *d,uint8_t code){evdev_feedback(d,code);}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *c){(void)a;(void)c;lv_disp_flush_ready(d);}
static void click(lv_event_t *e){(void)e;clicks++;}
static void custom(lv_event_t *e){
 if(lv_event_get_code(e)==LV_EVENT_RELEASED)
  lv_event_send(lv_event_get_target(e),LV_EVENT_READY,lv_event_get_indev(e));
}
static void delete_click(lv_event_t *e){clicks++;lv_obj_del(lv_event_get_target(e));}
static void sample(int x,int y,bool press,unsigned dt){point=(lv_point_t){x,y};down=press;lv_tick_inc(dt);lv_timer_handler();}
static void tap(int x,int y){sample(x,y,true,35);sample(x,y,false,35);}
static lv_obj_t *button(int x,int y){lv_obj_t *b=lv_btn_create(lv_scr_act());lv_obj_set_pos(b,x,y);lv_obj_set_size(b,100,50);lv_obj_add_event_cb(b,click,LV_EVENT_CLICKED,NULL);return b;}
int main(void){
 lv_init();static lv_color_t pixels[640*20];static lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,pixels,NULL,640*20);
 static lv_disp_drv_t dd;lv_disp_drv_init(&dd);dd.hor_res=640;dd.ver_res=400;dd.draw_buf=&db;dd.flush_cb=flush;lv_disp_drv_register(&dd);
 static lv_indev_drv_t id;lv_indev_drv_init(&id);id.type=LV_INDEV_TYPE_POINTER;id.read_cb=read_input;id.feedback_cb=feedback;pointer=lv_indev_drv_register(&id);
 (void)pointer;touch_sound_init(sound);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_t *b=button(20,20);lv_obj_update_layout(lv_scr_act());
 tap(40,40);assert(sounds==1&&clicks==1);tap(40,40);assert(sounds==2&&clicks==2);
 lv_obj_add_state(b,LV_STATE_DISABLED);tap(40,40);assert(sounds==2);lv_obj_clear_state(b,LV_STATE_DISABLED);
 tap(600,350);assert(sounds==2);
 sample(40,40,true,35);sample(300,40,true,35);sample(300,40,false,35);assert(sounds==2);
 tap(40,40);assert(sounds==3); /* Cancellation cannot poison the next press. */
 sample(40,40,true,35);for(int i=0;i<20;i++)sample(40,40,true,100);sample(40,40,false,35);assert(sounds==4);
 lv_event_send(b,LV_EVENT_CLICKED,NULL);assert(sounds==4); /* Programmatic refresh is silent. */
 lv_obj_add_event_cb(b,click,LV_EVENT_LONG_PRESSED,NULL);
 sample(40,40,true,35);for(int i=0;i<20;i++)sample(40,40,true,100);sample(40,40,false,35);assert(sounds==5);
 lv_obj_t *container=lv_obj_create(lv_scr_act());lv_obj_set_pos(container,150,100);lv_obj_set_size(container,180,150);
 lv_obj_t *child=lv_btn_create(container);lv_obj_set_size(child,100,50);lv_obj_add_flag(child,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_add_event_cb(container,click,LV_EVENT_CLICKED,NULL);
 lv_obj_t *filler=lv_obj_create(container);lv_obj_set_pos(filler,0,500);lv_obj_update_layout(lv_scr_act());
 lv_area_t area;lv_obj_get_coords(child,&area);int x=area.x1+20,y=area.y1+20;
 tap(x,y);assert(sounds==6);sample(x,y,true,35);sample(x,y-40,true,35);sample(x,y-70,false,35);assert(sounds==6);
 lv_obj_t *del=button(400,20);lv_obj_add_event_cb(del,delete_click,LV_EVENT_CLICKED,NULL);lv_obj_update_layout(lv_scr_act());tap(420,40);assert(sounds==7);
 lv_obj_t *replacement=button(400,20);(void)replacement;lv_obj_update_layout(lv_scr_act());tap(420,40);assert(sounds==8);
 lv_obj_t *matrix=lv_btnmatrix_create(lv_scr_act());static const char *map[]={"A","B",""};lv_btnmatrix_set_map(matrix,map);lv_obj_set_pos(matrix,380,200);lv_obj_set_size(matrix,200,70);lv_btnmatrix_set_btn_ctrl(matrix,1,LV_BTNMATRIX_CTRL_DISABLED);lv_obj_update_layout(lv_scr_act());
 tap(425,230);assert(sounds==9);tap(530,230);assert(sounds==9);
 lv_obj_t *slider=lv_slider_create(lv_scr_act());lv_obj_set_pos(slider,20,320);lv_obj_set_size(slider,200,20);lv_obj_add_flag(slider,LV_OBJ_FLAG_USER_4|LV_OBJ_FLAG_PRESS_LOCK);lv_obj_update_layout(lv_scr_act());
 sample(30,330,true,35);sample(120,330,true,35);sample(180,330,true,35);sample(180,330,false,35);assert(sounds==9);
 lv_obj_t *custom_obj=lv_obj_create(lv_scr_act());lv_obj_set_pos(custom_obj,400,310);lv_obj_set_size(custom_obj,100,50);lv_obj_add_event_cb(custom_obj,custom,LV_EVENT_ALL,NULL);lv_obj_update_layout(lv_scr_act());
 tap(420,330);assert(sounds==10);lv_event_send(custom_obj,LV_EVENT_READY,NULL);assert(sounds==10);
 /* Batch is a plain object with an explicit CLICKED handler. A small
    movement inside an accepted button must agree with its actual action. */
 lv_obj_clean(lv_scr_act());lv_obj_t *batch=lv_obj_create(lv_scr_act());
 lv_obj_set_pos(batch,20,20);lv_obj_set_size(batch,200,70);
 lv_obj_clear_flag(batch,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(batch,click,LV_EVENT_CLICKED,NULL);
 lv_obj_update_layout(lv_scr_act());unsigned before=sounds,actions=clicks;
 sample(40,40,true,35);sample(57,40,true,35);sample(57,40,false,35);
 assert(clicks==actions+1&&sounds==before+1);
 /* Captured custom controls emit READY only after accepting a real action. */
 touch_sound_contact(true);lv_event_send(batch,LV_EVENT_READY,NULL);
 /* Without active input, programmatic READY stays silent. */
 assert(sounds==before+1);touch_sound_contact(false);
 /* Roller drag is silent, just like slider drag and navigation gestures. */
 lv_obj_clean(lv_scr_act());lv_obj_t *roller=lv_roller_create(lv_scr_act());
 lv_obj_set_pos(roller,20,30);lv_obj_set_size(roller,120,150);
 lv_roller_set_options(roller,"0\n1\n2\n3\n4\n5",LV_ROLLER_MODE_NORMAL);
 lv_obj_add_flag(roller,LV_OBJ_FLAG_USER_4);lv_obj_update_layout(lv_scr_act());before=sounds;
 sample(70,100,true,35);sample(70,65,true,35);sample(70,55,false,35);assert(sounds==before);
 puts("PASS: real LVGL clicks, blank/disabled, slide cancellation, long hold, bubbling, scrolling, programmatic event, deletion/recreation, keyboard disabled key, silent slider/roller drags");
}
