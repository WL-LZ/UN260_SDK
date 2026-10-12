#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/app_command_runtime.h"
#include "un260/app_service/upgrade_session.h"

typedef void lv_event_t;
typedef struct { unsigned period; } lv_timer_t;
enum { LV_STATE_DISABLED, UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_START_SCRIPT,
       UI_TEXT_WIDGET_UPGRADE_POPUP_FAIL_DESC };
enum { UPGRADE_POPUP_STATE_IDLE, UPGRADE_POPUP_STATE_PROMPT,
       UPGRADE_POPUP_STATE_PROGRESS, UPGRADE_POPUP_STATE_SUCCESS,
       UPGRADE_POPUP_STATE_FAIL, UPGRADE_POPUP_STATE_RESTART };
#define UPGRADE_POPUP_STATUS_TIMER_MS 100
#define UPGRADE_POPUP_REBOOT_DELAY_MS 1000
#define UPGRADE_POPUP_DATA_WAIT_TIMEOUT_MS 30000U
static struct {
    int state;
    lv_timer_t *reboot_timer, *result_timer;
    bool reboot_reserved, reboot_clear_sent, result_success, result_restart_required, prompt_hiding;
    uint32_t reboot_prepare_started_ms;
    void *success_btn, *success_btn_later;
    char result_text[128];
} g_upgrade_popup;
static lv_timer_t timer_storage;
static app_ui_upgrade_readiness_t readiness;
static unsigned sends, reboots, failures, hides, creates, successes, restarts;
static bool create_ok;
static uint32_t now_ms;
uint32_t app_clock_uptime_ms(void) { return now_ms; }
static const char *ui_tr(const char *text) { return text; }
static const char *ui_text_get(int id) { (void)id; return "Could not start"; }
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void) { return readiness; }
static void lv_obj_add_state(void *obj, int state) { (void)obj; (void)state; }
static void lv_timer_del(lv_timer_t *timer) { assert(timer==&timer_storage); }
static void lv_timer_set_period(lv_timer_t *timer, unsigned period) { timer->period=period; }
static lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), unsigned period, void *data) {
    (void)callback; (void)data; creates++;timer_storage.period=period;
    return create_ok?&timer_storage:NULL;
}
static void upgrade_popup_show_fail(const char *text) { assert(text&&text[0]);failures++; }
static void upgrade_popup_show_success(void) { successes++; }
static void lv_upgrade_popup_show_restart_required(const char *text) {
    assert(!strcmp(text,"Restart pending"));restarts++;
}
static void upgrade_popup_hide(void) { hides++; }
static bool settings_detail_send_command(uint8_t command, const uint8_t *data, unsigned length) {
    assert(command==0x3B&&length==1&&data[0]==1);sends++;return true;
}
static void ui_upgrade_service_reboot(void) { reboots++; }
#include "actual_upgrade_popup_functions.h"

static void reset(void) {
    upgrade_session_end(upgrade_session_owner());
    memset(&g_upgrade_popup,0,sizeof(g_upgrade_popup));
    g_upgrade_popup.state=UPGRADE_POPUP_STATE_RESTART;
    readiness=APP_UI_UPGRADE_READY;create_ok=true;
    now_ms=0;
    sends=reboots=failures=hides=creates=successes=restarts=0;
}
static void test_wait_durable_clear_delay_and_recheck(void) {
    reset();readiness=APP_UI_UPGRADE_WAIT;upgrade_popup_on_reboot_click(NULL);
    assert(g_upgrade_popup.reboot_reserved&&creates==1&&timer_storage.period==100);
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
    upgrade_popup_on_reboot_click(NULL);assert(creates==1);
    upgrade_popup_reboot_timer_cb(&timer_storage);assert(!sends&&!reboots);
    readiness=APP_UI_UPGRADE_READY;upgrade_popup_reboot_timer_cb(&timer_storage);
    assert(sends==1&&!reboots&&timer_storage.period==1000);
    readiness=APP_UI_UPGRADE_WAIT;upgrade_popup_reboot_timer_cb(&timer_storage);
    assert(sends==1&&!reboots);
    readiness=APP_UI_UPGRADE_READY;upgrade_popup_reboot_timer_cb(&timer_storage);
    assert(sends==1&&reboots==1&&!g_upgrade_popup.reboot_timer);
    assert(upgrade_session_owner()==UPGRADE_SESSION_UI);
}
static void test_busy_failure_and_cancel(void) {
    const app_ui_upgrade_readiness_t errors[]={APP_UI_UPGRADE_BUSY,APP_UI_UPGRADE_FAILED};
    for(unsigned sent=0;sent<2;sent++) for(unsigned i=0;i<2;i++) {
        reset();upgrade_popup_on_reboot_click(NULL);
        if(sent) upgrade_popup_reboot_timer_cb(&timer_storage);
        readiness=errors[i];upgrade_popup_reboot_timer_cb(&timer_storage);
        assert(sends==sent&&!reboots&&failures==1&&!g_upgrade_popup.reboot_timer);
        assert(!g_upgrade_popup.reboot_reserved&&upgrade_session_owner()==UPGRADE_SESSION_NONE);
    }
    reset();readiness=APP_UI_UPGRADE_WAIT;upgrade_popup_on_reboot_click(NULL);
    upgrade_popup_on_reboot_later_click(NULL);
    assert(hides==1&&!sends&&!reboots&&!g_upgrade_popup.reboot_timer);
    assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
    reset();assert(upgrade_session_begin(UPGRADE_SESSION_CONTROLLER));
    upgrade_popup_on_reboot_click(NULL);
    assert(failures==1&&!creates&&!sends&&upgrade_session_owner()==UPGRADE_SESSION_CONTROLLER);
    reset();create_ok=false;upgrade_popup_on_reboot_click(NULL);
    assert(failures==1&&!sends&&!reboots&&upgrade_session_owner()==UPGRADE_SESSION_NONE);
    for(unsigned sent=0;sent<2;sent++) {
        reset();now_ms=UINT32_MAX-1000U;upgrade_popup_on_reboot_click(NULL);
        if(sent) upgrade_popup_reboot_timer_cb(&timer_storage);
        readiness=APP_UI_UPGRADE_WAIT;now_ms+=29999U;
        upgrade_popup_reboot_timer_cb(&timer_storage);assert(!failures&&!reboots);
        now_ms+=1U;upgrade_popup_reboot_timer_cb(&timer_storage);
        assert(failures==1&&sends==sent&&!reboots&&!g_upgrade_popup.reboot_timer);
        assert(upgrade_session_owner()==UPGRADE_SESSION_NONE);
        readiness=APP_UI_UPGRADE_READY;upgrade_popup_on_reboot_click(NULL);
        upgrade_popup_reboot_timer_cb(&timer_storage);upgrade_popup_reboot_timer_cb(&timer_storage);
        assert(reboots==1);
    }
}
static void test_success_confirm_only_acknowledges(void) {
    const app_ui_upgrade_readiness_t states[]={APP_UI_UPGRADE_READY, APP_UI_UPGRADE_WAIT,
        APP_UI_UPGRADE_BUSY, APP_UI_UPGRADE_FAILED};
    for(unsigned owner=UPGRADE_SESSION_NONE; owner<=UPGRADE_SESSION_IMAGE; owner++) {
        for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);i++) {
            reset();g_upgrade_popup.state=UPGRADE_POPUP_STATE_SUCCESS;readiness=states[i];
            if(owner!=UPGRADE_SESSION_NONE) assert(upgrade_session_begin(owner));
            upgrade_popup_on_result_confirm_click(NULL);
            assert(hides==1&&!sends&&!reboots&&!creates&&!failures);
            assert(!g_upgrade_popup.reboot_timer&&!g_upgrade_popup.reboot_reserved);
            assert(upgrade_session_owner()==owner);
        }
    }
    for(int state=UPGRADE_POPUP_STATE_IDLE;state<=UPGRADE_POPUP_STATE_FAIL;state++) {
        if(state==UPGRADE_POPUP_STATE_SUCCESS) continue;
        reset();g_upgrade_popup.state=state;upgrade_popup_on_result_confirm_click(NULL);
        assert(!hides&&!sends&&!reboots&&!creates&&!failures);
    }
    reset();g_upgrade_popup.prompt_hiding=true;upgrade_popup_on_result_confirm_click(NULL);
    assert(!hides&&!sends&&!reboots&&!creates&&!failures);
    reset();upgrade_popup_on_result_confirm_click(NULL);
    assert(creates==1&&g_upgrade_popup.reboot_reserved);
    upgrade_popup_on_reboot_later_click(NULL);
    assert(hides==1&&!sends&&!reboots&&!g_upgrade_popup.reboot_timer);
}
int main(void) {
    test_success_confirm_only_acknowledges();
    test_wait_durable_clear_delay_and_recheck();test_busy_failure_and_cancel();
    reset();strcpy(g_upgrade_popup.result_text,"Restart pending");
    g_upgrade_popup.result_restart_required=true;upgrade_popup_result_timer_cb(NULL);
    assert(restarts==1&&!successes&&!failures);
    g_upgrade_popup.result_restart_required=false;g_upgrade_popup.result_success=true;
    upgrade_popup_result_timer_cb(NULL);assert(successes==1);
    g_upgrade_popup.result_success=false;upgrade_popup_result_timer_cb(NULL);assert(failures==1);
    puts("PASS success Confirm only acknowledges with zero commands/reboots across readiness/owners; popup restart ownership, durable WAIT before clear and reboot, original clear/delay, BUSY/FAILED/cancel/timer failure, distinct restart/final success");
}
