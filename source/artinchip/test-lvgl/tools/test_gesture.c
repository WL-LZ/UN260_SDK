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
static unsigned home, back, returned;
static bool password_modal;
bool ui_page_05_set_password_is_open(void){return password_modal;}
bool ui_page_05_set_password_request_back(void){bool open=password_modal;password_modal=false;return open;}
static lv_nav_back_result_t nav_result;
static unsigned esc_calls;
static bool editor_active;
static bool raw_owned;
static unsigned raw_calls;
static bool raw_policy(lv_indev_t *indev,lv_event_code_t event,const lv_point_t *p,uint8_t n)
{ (void)indev;(void)event;(void)p;(void)n;++raw_calls;return raw_owned; }
bool app_standby_runtime_touch(bool down){(void)down;return false;}
static bool owns_single_drag(void){return editor_active;}
static bool policy_blocked;static unsigned policy_calls;
static bool handle_action(gesture_action_t action){(void)action;policy_calls++;return policy_blocked;}
bool page_32_innovation_request_back(void){return false;}
bool page_01_main_quick_request_back(void){return false;}
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
/* Count dispatches here; full chain restoration is tested by navigation_history. */
bool ui_manager_restore_from_home(void){returned++;return true;}
bool ui_manager_suspend_to_home(void){ui_manager_switch(UI_PAGE_MAIN);return true;}
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
    gesture_service_set_pointer_policy(UI_PAGE_MAIN,raw_policy);
    page=UI_PAGE_MAIN;raw_owned=true;
    assert(sample(1,1200,120,0));
    gesture_service_clear_pointer_policy(UI_PAGE_MAIN);page=UI_PAGE_MENU;
    assert(sample(1,1250,100,0));assert(sample(0,0,0,0));
    assert(!queued); /* Capture survives owner destruction, never leaks CLICKED. */
    gesture_service_set_pointer_policy(UI_PAGE_MAIN,raw_policy);
    unsigned raw_before=raw_calls;assert(!sample(1,400,100,0));release();
    assert(raw_calls==raw_before); /* Hidden owner is ignored. */
    page=UI_PAGE_MAIN;raw_owned=false;assert(!sample(1,500,100,0));
    raw_owned=true;assert(sample(1,510,100,0));assert(sample(2,510,100,0));
    assert(sample(0,0,0,0));assert(!queued);
    gesture_service_clear_pointer_policy(UI_PAGE_MAIN);page=UI_PAGE_MENU;
    gesture_service_set_page_policy(UI_PAGE_STANDBY_SETTING,owns_single_drag,handle_action);
    page=UI_PAGE_STANDBY_SETTING;editor_active=true;
    assert(!sample(1,200,120,0));assert(!sample(2,200,140,0));assert(!queued);
    release();editor_active=false;
    assert(!sample(2,100,300,0)); release();
    assert(gesture_service_set_enabled(true));
    assert(!sample(1,100,300,0)); assert(!sample(1,100,100,0)); release();
    assert(!home && !returned && !back);
    sample(2,100,300,0); sample(2,100,200,0);
    assert(!queued && !returned); release(); assert(returned==1);
    sample(2,100,100,0); sample(2,100,200,0); release(); assert(home==1);
    sample(3,100,300,0); sample(3,100,200,0); release(); assert(home==1 && returned==1);
    sample(3,100,300,0); sample(2,100,300,0); sample(2,100,180,0); release(); assert(returned==1);
    sample(2,100,300,0); sample(2,100,200,5); release(); assert(returned==1);
    sample(2,100,300,0); sample(2,250,210,0); release(); assert(returned==1);
    sample(2,100,300,0); tick+=1900; sample(2,100,200,0); release(); assert(returned==1);
    page=UI_PAGE_UI_UPGRADE; assert(!sample(2,100,300,0)); assert(!sample(2,100,100,0)); release();
    sample(2,100,300,0);
    points[0].y=160; points[1].y=300; tick+=100;
    lv_point_t middle={120,230};
    observer(NULL,LV_EVENT_PRESSING,&middle,2,NULL); release(); assert(returned==1);
    sample(2,100,300,0); sample(2,100,200,0); sample(0,0,0,0);
    page=UI_PAGE_SETTING; drain(); assert(returned==1); release();
    sample(4,100,300,0); sample(4,100,100,0); release(); assert(returned==1);
    /* Edge taps remain clicks; centre swipes never become edge navigation. */
    assert(!sample(1,5,200,0)); release(); assert(!back);
    assert(!sample(1,5,200,0)); assert(sample(1,110,200,0)); release(); assert(back==1);
    assert(!sample(1,1275,200,0)); assert(sample(1,1160,200,0)); release(); assert(back==2);
    sample(1,5,200,0); sample(1,60,200,0); release(); assert(back==2);
    sample(1,5,200,0); sample(1,130,200,0); sample(1,20,200,0); release(); assert(back==2);
    sample(1,5,100,0); sample(1,7,230,0); release(); assert(back==2);
    /* Late third finger cancels a recognized two-finger action before release. */
    sample(2,100,300,0); sample(2,100,200,0); sample(3,100,180,0); release(); assert(returned==1);
    /* Staggered release still produces exactly one action. */
    sample(2,100,300,0); sample(2,100,200,0); sample(1,100,200,0); release(); assert(returned==2);
    /* A lifted and re-added finger cancels, even after the threshold. */
    sample(2,100,300,0); sample(2,100,200,0); sample(1,100,200,0);
    sample(2,100,180,0); release(); assert(returned==2);
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
    /* Every page can own a single drag without disabling global multi-touch. */
    gesture_service_set_enabled(true);
    for(int editor=0;editor<2;editor++) {
        editor_active=editor;page=UI_PAGE_STANDBY_SETTING;unsigned e=returned,h=home;
        assert(!sample(1,200,200,0));
        assert(sample(2,200,240,0));assert(sample(2,200,120,0));
        sample(1,200,120,0);release();assert(returned==e+1);
        page=UI_PAGE_STANDBY_SETTING;
        sample(2,200,100,0);sample(2,200,220,0);release();assert(home==h+1);
    }
    /* Dirty/modal/busy policy is checked at dispatch, not by disabling recognition. */
    page=UI_PAGE_STANDBY_SETTING;unsigned e=returned,h=home;
    sample(2,200,100,0);sample(2,200,220,0);sample(0,0,0,0);
    policy_blocked=true;drain();assert(home==h);
    sample(2,200,240,0);sample(2,200,120,0);release();assert(returned==e);
    policy_blocked=false;
    /* Hidden owner must not affect another page, even when its policy blocks. */
    policy_blocked=true;page=UI_PAGE_MENU;
    sample(2,200,240,0);sample(2,200,120,0);release();assert(returned==e+1);
    gesture_service_clear_page_policy(UI_PAGE_STANDBY_SETTING);
    page=UI_PAGE_STANDBY_SETTING;unsigned calls_before=policy_calls;
    sample(2,200,100,0);sample(2,200,220,0);release();
    assert(home==h+1&&policy_calls==calls_before);
    /* Modal touches bypass the underlying Main quick-control pointer policy. */
    page=UI_PAGE_MAIN;password_modal=true;raw_owned=true;
    gesture_service_set_pointer_policy(UI_PAGE_MAIN,raw_policy);
    raw_before=raw_calls;assert(!sample(1,500,120,0));release();
    assert(raw_calls==raw_before&&password_modal);
    page=UI_PAGE_MAIN;unsigned returns_before=returned;
    sample(2,300,250,0);sample(2,300,100,0);release();
    assert(!password_modal&&returned==returns_before);
    puts("gesture: PASS (navigation without waiting for hint, cancellation, multi-touch and safety)");
}
