#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int lv_event_code_t;
typedef struct { int x,y; } lv_point_t;
typedef struct { bool disabled; } lv_obj_t;
typedef struct { lv_point_t point; } lv_indev_t;
typedef struct { int code; void *data; } lv_event_t;
enum {LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED,LV_EVENT_PRESS_LOST,
 LV_EVENT_CANCEL,LV_EVENT_SCROLL_BEGIN,LV_EVENT_LONG_PRESSED,LV_EVENT_CLICKED,LV_EVENT_DELETE,LV_STATE_DISABLED};
static lv_indev_t input;
static unsigned fingers=1,cycles,cancels;
static void *owners[2];static lv_obj_t buttons[2];
static void page_01_mode_contact_event(lv_event_t *e);
static void *lv_event_get_user_data(lv_event_t *e){return e->data;}
static int lv_event_get_code(lv_event_t *e){return e->code;}
static lv_indev_t *lv_indev_get_act(void){return &input;}
static void lv_indev_get_point(lv_indev_t *i,lv_point_t *p){*p=i->point;}
static unsigned lv_port_indev_touch_count(void){return fingers;}
static bool lv_obj_has_state(lv_obj_t *o,int state){(void)state;return o->disabled;}
static void lv_event_send(lv_obj_t *o,int code,void *data){(void)data;++cancels;lv_event_t e={code,owners[o-buttons]};page_01_mode_contact_event(&e);}
static void lv_obj_add_event_cb(lv_obj_t *o,void (*cb)(lv_event_t *),int code,void *data){(void)cb;(void)code;owners[o-buttons]=data;}
static void page_01_mode_cycle(void){++cycles;}
#include "contact_under_test.h"
static void event(unsigned b,int code){lv_event_t e={code,owners[b]};page_01_mode_contact_event(&e);}
static void release(unsigned b){event(b,LV_EVENT_RELEASED);event(b,LV_EVENT_CLICKED);}
int main(void){
 for(unsigned b=0;b<2;b++){
  page_01_mode_button_bind(&buttons[b]);unsigned n=cycles;
  event(b,LV_EVENT_PRESSED);release(b);assert(cycles==++n);
  event(b,LV_EVENT_PRESSED);event(b,LV_EVENT_LONG_PRESSED);release(b);assert(cycles==n);
  event(b,LV_EVENT_PRESSED);input.point.x+=11;event(b,LV_EVENT_PRESSING);release(b);assert(cycles==n&&cancels);
  event(b,LV_EVENT_PRESSED);fingers=2;event(b,LV_EVENT_PRESSING);release(b);fingers=1;assert(cycles==n);
  buttons[b].disabled=true;event(b,LV_EVENT_PRESSED);release(b);buttons[b].disabled=false;assert(cycles==n);
  event(b,LV_EVENT_PRESSED);event(b,LV_EVENT_CANCEL);release(b);assert(cycles==n);
  event(b,LV_EVENT_PRESSED);event(b,LV_EVENT_SCROLL_BEGIN);release(b);assert(cycles==n);
  event(b,LV_EVENT_PRESSED);event(b,LV_EVENT_PRESS_LOST);release(b);assert(cycles==n);
  event(b,LV_EVENT_PRESSED);release(b);assert(cycles==++n);
 }
 for(unsigned b=0;b<2;b++)event(b,LV_EVENT_DELETE);
 assert(!s_mode_contacts[0].button&&!s_mode_contacts[1].button);
 page_01_mode_button_bind(&buttons[0]);event(0,LV_EVENT_PRESSED);release(0);assert(cycles==5);
 puts("PASS both mode keys: tap, hold, drag, multitouch, disabled, cancel, scroll, lost press and recreate");
}
