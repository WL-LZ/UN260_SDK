#include "support.h"
#include "un260/gesture/gesture_service.h"
#include "un260/lv_components/lv_nav_button.h"
#include <assert.h>
#include <stdio.h>
static lv_port_pointer_observer_t observer;
static uint32_t tick;
static bool enabled;
static ui_page_t page = UI_PAGE_MENU;
static lv_point_t points[3];
static int32_t ids[3];
static uint8_t fingers;
static unsigned home, back, exported;
static lv_nav_back_result_t nav_result;
static unsigned esc_calls;
lv_nav_back_result_t lv_nav_button_request_back(void){esc_calls++;return nav_result;}
static void (*queued)(void*);
static void *queued_data;
static lv_timer_t nav_timer;
static bool nav_wait, hint_enabled, hint_returning;
void uart_debug_printf(const char *fmt,...){LV_UNUSED(fmt);}
lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *),uint32_t period,void *data){
    assert(!nav_wait && period==16);nav_timer=(lv_timer_t){data,cb};nav_wait=true;return &nav_timer;
}
void lv_timer_del(lv_timer_t *timer){assert(timer==&nav_timer);nav_wait=false;}
void lv_port_indev_set_pointer_observer(lv_port_pointer_observer_t cb,void *data) { LV_UNUSED(data); observer=cb; }
uint8_t lv_port_indev_touch_points(lv_point_t *p,int32_t *id,uint8_t cap) {
    unsigned n=fingers<cap?fingers:cap;
    for(unsigned i=0;i<n;i++){p[i]=points[i];id[i]=ids[i];} return n;
}
uint32_t lv_tick_get(void){return tick;}
uint32_t lv_tick_elaps(uint32_t t){return tick-t;}
int lv_async_call(void(*cb)(void*),void *data){assert(!queued);queued=cb;queued_data=data;return 0;}
ui_page_t ui_manager_get_current_page(void){return page;}
void ui_manager_clear_stack(void){}
void ui_manager_switch(ui_page_t p){if(p==UI_PAGE_MAIN)home++;page=p;}
void ui_manager_push_page(ui_page_t p){page=p;}
bool ui_export_data_request(void){exported++;return true;}
void touch_feedback_edge_hint(int side,int distance,int y){LV_UNUSED(distance);LV_UNUSED(y);if(!side && hint_enabled)hint_returning=true;}
bool touch_feedback_edge_hint_is_returning(void){return hint_returning;}
bool ui_manager_pop_page(void){back++;return true;}
bool page_06_settings_back_sub_page(void){return false;}
bool user_cfg_gesture_enabled(void){return enabled;}
bool user_cfg_gesture_save(bool e){enabled=e;return true;}
bool gesture_guide_is_open(void){return false;}
void gesture_guide_close(bool e){LV_UNUSED(e);}
void touch_feedback_init(void){}
void touch_feedback_sample(const lv_point_t *p,uint8_t n){LV_UNUSED(p);LV_UNUSED(n);}
static bool sample(uint8_t n,int x,int y,int id_offset)
{
    fingers=n; tick+=100;
    for(unsigned i=0;i<n && i<3;i++){points[i]=(lv_point_t){x+40*i,y};ids[i]=i+id_offset;}
    lv_point_t center={x+(n?40*(n-1)/2:0),y};
    return observer(NULL,n?LV_EVENT_PRESSING:LV_EVENT_RELEASED,&center,n,NULL);
}
static void drain(void){if(nav_wait)nav_timer.cb(&nav_timer);if(queued){void(*cb)(void*)=queued;queued=NULL;cb(queued_data);}}
static void release(void){sample(0,0,0,0);drain();page=UI_PAGE_MENU;}
int main(void)
{
    gesture_service_init();
    assert(!sample(2,100,300,0)); release();
    assert(gesture_service_set_enabled(true));
    assert(!sample(1,100,300,0)); assert(!sample(1,100,100,0)); release();
    assert(!home && !exported && !back);
    sample(2,100,300,0); sample(2,100,200,0);
    assert(!queued && !exported); release(); assert(exported==1);
    sample(2,100,100,0); sample(2,100,200,0); release(); assert(home==1);
    sample(3,100,300,0); sample(3,100,200,0); release(); assert(home==1 && exported==1);
    sample(3,100,300,0); sample(2,100,300,0); sample(2,100,180,0); release(); assert(exported==1);
    sample(2,100,300,0); sample(2,100,200,5); release(); assert(exported==1);
    sample(2,100,300,0); sample(2,250,210,0); release(); assert(exported==1);
    sample(2,100,300,0); tick+=1900; sample(2,100,200,0); release(); assert(exported==1);
    page=UI_PAGE_UI_UPGRADE; assert(!sample(2,100,300,0)); assert(!sample(2,100,100,0)); release();
    sample(2,100,300,0);
    points[0].y=160; points[1].y=300; tick+=100;
    lv_point_t middle={120,230};
    observer(NULL,LV_EVENT_PRESSING,&middle,2,NULL); release(); assert(exported==1);
    sample(2,100,300,0); sample(2,100,200,0); sample(0,0,0,0);
    page=UI_PAGE_SETTING; drain(); assert(exported==1); release();
    sample(4,100,300,0); sample(4,100,100,0); release(); assert(exported==1);
    /* Edge taps remain clicks; centre swipes never become edge navigation. */
    assert(!sample(1,5,200,0)); release(); assert(!back);
    assert(!sample(1,5,200,0)); assert(sample(1,110,200,0)); release(); assert(back==1);
    assert(!sample(1,1275,200,0)); assert(sample(1,1160,200,0)); release(); assert(back==2);
    sample(1,5,200,0); sample(1,60,200,0); release(); assert(back==2);
    sample(1,5,200,0); sample(1,130,200,0); sample(1,20,200,0); release(); assert(back==2);
    sample(1,5,100,0); sample(1,7,230,0); release(); assert(back==2);
    /* Late third finger cancels a recognized two-finger action before release. */
    sample(2,100,300,0); sample(2,100,200,0); sample(3,100,180,0); release(); assert(exported==1);
    /* Staggered release still produces exactly one action. */
    sample(2,100,300,0); sample(2,100,200,0); sample(1,100,200,0); release(); assert(exported==2);
    /* A lifted and re-added finger cancels, even after the threshold. */
    sample(2,100,300,0); sample(2,100,200,0); sample(1,100,200,0);
    sample(2,100,180,0); release(); assert(exported==2);
    /* Navigation runs on the first async turn while the edge is still returning. */
    hint_enabled=true; page=UI_PAGE_INNOVATION_CENTER;
    unsigned previous_back=back;
    sample(1,5,200,0);sample(1,130,200,0);sample(0,0,0,0);
    assert(!nav_wait && queued && hint_returning);
    drain();assert(back==previous_back+1 && hint_returning && !nav_wait);
    drain();assert(back==previous_back+1);
    /* A page change or disabling gestures before the async turn cancels work. */
    ui_page_t maintenance[]={UI_PAGE_CIS_CALIB,UI_PAGE_MOTOR_TEST,UI_PAGE_AGING_SETTING,
        UI_PAGE_FACTORY_SETTING,UI_PAGE_UI_UPGRADE,UI_PAGE_MAIN_UPGRADE,UI_PAGE_IMAGE_UPGRADE};
    nav_result=LV_NAV_BACK_HANDLED;
    for(unsigned i=0;i<sizeof(maintenance)/sizeof(maintenance[0]);i++) {
        for(int side=0;side<2;side++) {
            page=maintenance[i]; unsigned calls=esc_calls, pops=back;
            sample(1,side?1275:5,200,0);sample(1,side?1150:130,200,0);
            sample(0,0,0,0);assert(queued);drain();
            assert(esc_calls==calls+1 && back==pops);
        }
    }
    page=UI_PAGE_UI_UPGRADE;nav_result=LV_NAV_BACK_BLOCKED;
    unsigned pops=back;
    sample(1,5,200,0);sample(1,130,200,0);release();assert(back==pops);
    page=UI_PAGE_MOTOR_TEST;nav_result=LV_NAV_BACK_NONE;
    sample(1,5,200,0);sample(1,130,200,0);release();assert(back==pops);
    page=UI_PAGE_MOTOR_TEST;unsigned calls=esc_calls;
    sample(1,5,200,0);sample(1,130,200,0);sample(2,130,200,0);release();assert(esc_calls==calls);
    nav_result=LV_NAV_BACK_NONE;
    page=UI_PAGE_MENU;
    sample(1,5,200,0);sample(1,130,200,0);sample(0,0,0,0);
    page=UI_PAGE_SETTING;drain();assert(back==previous_back+1 && !nav_wait);
    page=UI_PAGE_MENU;
    sample(1,5,200,0);sample(1,130,200,0);sample(0,0,0,0);
    gesture_service_set_enabled(false);drain();assert(back==previous_back+1 && !nav_wait);
    puts("gesture: PASS (navigation without waiting for hint, cancellation, multi-touch and safety)");
}
