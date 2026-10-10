/* Real setting request owners, negative reply branches and notice policy. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/setting_service.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/machine_state/machine_state.h"
#include "un260/machine_state/machine_fault.h"
#include "un260/counting/counting_session_state.h"
#include "un260/currency/currency_service.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/mode_codec.h"
#include "test_notice_sink.h"

typedef unsigned ui_page_t;
enum {UI_PAGE_MAIN=1,UI_PAGE_PURE=2,UI_PAGE_BOOT=0,UI_PAGE_STANDBY=3,UI_PAGE_COUNT=40};
#define UI_DATA_TOPIC_MACHINE_SETTINGS 1U
static ui_page_t page;
static uint64_t now=1000;
static unsigned guide_posts,sends,batch_failures,profile_rejections;
static uint8_t sent_command,sent_value;
static bool profile_owned,start_pending,busy,send_failed;
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
void uart_debug_printf(const char *format,...){(void)format;}
bool work_mode_service_request(uint8_t mode){(void)mode;assert(!"unrelated automatic/manual setting");return false;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t length)
{assert(payload&&length&&command!=0x3D&&command!=0x0A);++sends;sent_command=command;sent_value=payload[0];return send_failed?-1:length;}
static ui_page_t ui_manager_get_current_page(void){return page;}
static bool workspace_service_owns_command(uint8_t command){return profile_owned&&(command==4||command==0x3A);}
static void workspace_service_reject_command(uint8_t command){assert(command==4||command==0x3A);++profile_rejections;profile_owned=false;}
static bool counting_action_start_pending(void){return start_pending;}
static bool app_command_runtime_count_start_busy(void){return busy || start_pending;}
static void fault_popup_report_setting_rejection(uint8_t command,uint8_t reason)
{assert((command==3||command==4||command==6||command==0x3A)&&reason==0x80);++guide_posts;assert(!machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SETTING,reason,command},NULL));}
static void fault_popup_resolve_setting_rejection(uint8_t command)
{machine_fault_clear_code(MACHINE_FAULT_SETTING,command);}
static void page_01_mode_switch_refre(void){}
static void page_01_bottom_a_refresh_mode(bool animate){(void)animate;}
static void page_01_curr_img_refre(void){}
static void page_01_batch_refre(void){}
static void page_01_bottom_a_refresh_fo(bool animate){(void)animate;}
static void page_03_update_menu_button_states_refresh(void){}
static bool page_03_menu_is_visible(void){return false;}
static void page_03_menu_refresh_batch_number(void){}
static void smart_island_refresh_summary(void){}
static void ui_manager_publish_data_changed(uint32_t topic){(void)topic;}
static void app_fault_recovery_report(machine_fault_key_t key){(void)key;assert(!"not a preset stop");}
static void fault_popup_report_preset_full(void){assert(!"not a preset stop");}
static void page_03_batch_set_result(bool success,const setting_batch_result_t *result)
{assert(!success&&result);++batch_failures;}
#include "setting_rejection_under_test.inc"
static unsigned begin_notifications;
static void observed_request_begin(uint8_t command){++begin_notifications;setting_request_started(command);}

static void fixture(void)
{
    setting_service_cancel_all();currency_service_cancel_switch();currency_state_reset();machine_fault_clear();
    assert(currency_state_confirm_active_code("GBP"));
    now=1000;guide_posts=sends=batch_failures=profile_rejections=test_notice_count=0;
    profile_owned=start_pending=busy=send_failed=false;sent_command=sent_value=0;page=UI_PAGE_MAIN;
    memset(&setting_rejection,0,sizeof(setting_rejection));deferred_setting_rejection=0;app_setting_notice_init();
    begin_notifications=0;protocol_request_set_start_observer(observed_request_begin);
    machine_state_confirm_mode(MODE_MDC);machine_state_confirm_fo_mode(0);machine_state_confirm_batch(true,20);
    protocol_request_result_context_end();
}
static void request(uint8_t command)
{
    bool sent=command==4?setting_service_request_mode(MODE_SDC):command==6?
        setting_service_request_batch_number(30,true,20):setting_service_request_fo_mode(3);
    assert(sent&&sends==1);
}
static void reply(uint8_t command,uint8_t status,uint8_t response_mode,uint64_t received,bool malformed)
{
    uint8_t frame[]={0xFD,0xDF,command==0x3A?7:6,command,command==0x3A?response_mode:status,status,0};
    if(malformed)++frame[2];
    protocol_request_result_context_begin(received);
    /* This is the same pre-dispatch observer called for every RX frame. */
    assert(!app_fault_recovery_observe_setting_detail(command,frame,command==0x3A?7:6,NULL));
    app_setting_reply_action_t action=app_setting_reply_handle_basic(command,frame,command==0x3A?7:6);
    assert(action==APP_SETTING_REPLY_ACTION_NONE||status==1);
    protocol_request_result_context_end();
}
static void preserved(void)
{
    assert(machine_state_mode()==MODE_MDC&&machine_state_fo_mode()==0&&machine_state_batch_num()==20&&machine_state_batch_enabled());
}
static void test_matched_failures_and_profile(void)
{
    const uint8_t commands[]={4,6,0x3A};
    for(unsigned origin=0;origin<2;++origin)for(unsigned i=0;i<sizeof(commands);++i){
        fixture();page=origin?4:UI_PAGE_MAIN;request(commands[i]);now=9000;
        reply(commands[i],2,3,1050,false);preserved();
        assert(guide_posts==1&&!test_notice_count&&sends==1);
        assert(!machine_fault_count() && !machine_fault_first_unread(NULL));
        reply(commands[i],2,3,1051,false);preserved();assert(guide_posts==1&&!test_notice_count);
    }
    fixture();page=4;request(4);
    ui_notice_post(UI_NOTICE_PROGRESS,"settings.mode","Changing",NULL);assert(test_notice_visible);
    reply(4,2,0,1050,false);assert(guide_posts==1&&!test_notice_visible&&test_notice_count==1);
    fixture();page=4;profile_owned=true;request(4);reply(4,2,0,1050,false);
    assert(!guide_posts&&!test_notice_count&&profile_rejections==1);preserved();
}
static void test_wrong_shape_timeout_invalid_type(void)
{
    const uint8_t commands[]={4,6,0x3A};
    for(unsigned i=0;i<sizeof(commands);++i){
        fixture();reply(commands[i],2,3,1050,false);assert(!guide_posts);preserved();
        fixture();request(commands[i]);reply(commands[i],2,3,1050,true);assert(!guide_posts);preserved();
        reply(commands[i],2,3,1051,false);assert(guide_posts==1);preserved();
        fixture();request(commands[i]);reply(commands[i],2,3,6000,false);assert(!guide_posts);preserved();
    }
    fixture();request(0x3A);reply(0x3A,2,4,1050,false);assert(!guide_posts);preserved();
    reply(0x3A,2,3,1051,false);assert(guide_posts==1);preserved();
}
static void test_companion_boundaries(void)
{
    const uint8_t commands[]={4,6,0x3A};
    for(unsigned i=0;i<sizeof(commands);++i)for(unsigned outcome=0;outcome<6;++outcome){
        fixture();request(commands[i]);reply(commands[i],2,3,1050,false);
        counting_session_state_t session={0};uint8_t frame[]={0xFD,0xDF,6,0x0F,7,0};
        uint64_t received=1051;
        if(outcome==1)frame[4]=2;
        if(outcome==2)received=1850;
        if(outcome==3)session.start_confirmed=true;
        if(outcome==4){now=1051;assert(setting_service_request_add(true));}
        if(outcome==5)app_fault_recovery_cancel_setting_detail();
        protocol_request_result_context_begin(received);
        assert(app_fault_recovery_observe_setting_detail(0x0F,frame,6,&session)==(outcome==0));
        assert(!app_fault_recovery_observe_setting_detail(0x0F,frame,6,&session));
        protocol_request_result_context_end();assert(guide_posts==1);preserved();
    }
    fixture();app_setting_notice_result("settings.sound","Sound",false);assert(!guide_posts&&!test_notice_count);
    page=4;app_setting_notice_result("settings.sound","Sound",false);assert(!guide_posts&&test_notice_count==1);
}

static void detail(uint64_t received, bool expected)
{
    const uint8_t frame[]={0xFD,0xDF,6,0x0F,7,0};
    counting_session_state_t session={0};
    protocol_request_result_context_begin(received);
    bool matched=app_fault_recovery_observe_setting_detail(0x0F,frame,6,&session);
    protocol_request_result_context_end();
    assert(matched==expected);
}
static void test_setting_to_currency_generation(void)
{
    const uint8_t commands[]={4,6,0x3A};
    for(unsigned i=0;i<sizeof(commands);++i)for(unsigned outcome=0;outcome<5;++outcome) {
        fixture();request(commands[i]);reply(commands[i],2,3,1050,false);preserved();
        uint8_t index;assert(currency_state_find_code("EUR",&index));now=1070;
        assert(currency_service_request_switch(index,"EUR"));
        if(outcome==1) {
            send_failed=true;const uint8_t eur[]={'E','U','R'};
            assert(protocol_send(3,eur,3)<0);currency_service_cancel_switch();
        } else if(outcome==2) {
            now=1900;currency_switch_result_t result;
            assert(currency_service_take_switch_timeout(&result)&&result.timeout);
        } else if(outcome==3) currency_service_cancel_switch();
        else if(outcome==4) assert(!currency_service_request_switch(index,"EUR"));
        detail(1071,false);
        assert(guide_posts==1&&!machine_fault_count());
        if(outcome==0) {
            currency_switch_result_t result;protocol_request_result_context_begin(1080);
            assert(currency_service_take_switch_result(2,&result)&&result.controller_rejected);
            assert(app_fault_recovery_report_setting_rejection(3));
            protocol_request_result_context_end();detail(1081,true);detail(1082,false);
            assert(guide_posts==2);
        }
    }
    fixture();request(4);reply(4,2,0,1050,false);
    assert(!currency_service_request_switch(255,"ZZZ")&&!currency_service_request_switch(0,""));
    assert(setting_rejection.command==4);detail(1051,true);detail(1052,false);
}
static void test_busy_replies_defer_and_cancel(void)
{
    const uint8_t commands[]={4,6,0x3A};
    for(unsigned i=0;i<sizeof(commands);++i)for(unsigned outcome=0;outcome<5;++outcome) {
        fixture();request(commands[i]);busy=true;
        ui_notice_post(UI_NOTICE_PROGRESS,commands[i]==4?"settings.mode":commands[i]==6?"settings.batch":"settings.sorting","Changing",NULL);
        reply(commands[i],2,3,1050,false);preserved();
        assert(!guide_posts&&!test_notice_visible&&deferred_setting_rejection==commands[i]);
        present_deferred_setting_rejection();assert(!guide_posts);
        if(outcome==1) app_fault_recovery_cancel_setting_presentation();
        if(outcome==2) app_fault_recovery_resolve_setting_rejection(commands[i]);
        if(outcome==3) {now=1070;assert(setting_service_request_add(true));}
        if(outcome==4) machine_fault_report((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,1});
        busy=false;present_deferred_setting_rejection();
        assert(guide_posts==((outcome==0||outcome==3||outcome==4)?1U:0U));
        assert(!deferred_setting_rejection);preserved();
    }
    fixture();busy=true;assert(app_fault_recovery_report_setting_rejection(4));
    now=1070;assert(setting_service_request_mode(MODE_SDC));
    busy=false;present_deferred_setting_rejection();assert(!guide_posts&&!deferred_setting_rejection);
    fixture();busy=true;assert(app_fault_recovery_report_setting_rejection(6));
    app_setting_notice_result("settings.receipt","Receipt",true);
    assert(deferred_setting_rejection==6);
    app_fault_recovery_cancel_setting_presentation();
}


static void test_begin_observer_contract(void)
{
    fixture();assert(!setting_service_request_fo_mode(4)&&!begin_notifications&&!sends);
    protocol_request_t slot=PROTOCOL_REQUEST_INITIALIZER(800);
    assert(protocol_request_begin(&slot)&&!begin_notifications);
    assert(!protocol_request_begin_for_command(&slot,3)&&!begin_notifications);
    protocol_request_finish(&slot);
    assert(protocol_request_begin_for_command(&slot,3)&&begin_notifications==1);
    assert(!protocol_request_begin_for_command(&slot,4)&&begin_notifications==1);
    protocol_request_finish(&slot);
    protocol_request_set_start_observer(NULL);
    assert(protocol_request_begin_for_command(&slot,4)&&begin_notifications==1);
    protocol_request_finish(&slot);
    protocol_request_set_start_observer(observed_request_begin);
    assert(protocol_request_begin_for_command(&slot,6)&&begin_notifications==2);
    protocol_request_finish(&slot);
}


static void test_sort_all_four_wire_targets(void)
{
    for(uint8_t target=0;target<4;++target) {
        fixture();uint8_t previous=(target+3)%4;machine_state_confirm_fo_mode(previous);
        assert(setting_service_request_fo_mode(target));
        reply(0x3A,2,target,1050,false);
        assert(guide_posts==1&&machine_state_fo_mode()==previous&&!machine_fault_count());
    }
}


static void test_sort_zero_nak_and_owner_lifecycle(void)
{
    fixture();request(0x3A);assert(sent_command==0x3A&&sent_value==3);
    const uint8_t observed[]={0xFD,0xDF,0x07,0x3A,0x00,0x02,0x5C};
    protocol_request_result_context_begin(1050);
    assert(app_setting_reply_handle_basic(0x3A,observed,sizeof(observed))==APP_SETTING_REPLY_ACTION_NONE);
    protocol_request_result_context_end();assert(guide_posts==1);preserved();
    /* This controller reports 3A/00/02 even after requesting 01/02/03. The
     * pending command and original RX time own an ACK; the refused mode byte
     * is not the requested value. A failure never confirms a new FO value. */
    for(uint8_t target=0;target<4;++target)for(uint8_t response=0;response<4;++response) {
        fixture();uint8_t previous=(target+1)%4;machine_state_confirm_fo_mode(previous);
        assert(setting_service_request_fo_mode(target));
        reply(0x3A,2,response,1050,false);
        assert(guide_posts==1&&machine_state_fo_mode()==previous&&sends==1);
        reply(0x3A,2,response,1051,false);assert(guide_posts==1);
        now=1800;assert(!(setting_service_take_basic_timeouts()&SETTING_REQUEST_TIMEOUT_FO_MODE));
        assert(!setting_service_take_fo_mode_result(NULL));
    }
    fixture();request(0x3A);reply(0x3A,2,0,999,false);assert(!guide_posts);preserved();
    reply(0x3A,2,0,1799,false);assert(guide_posts==1);preserved();
    fixture();request(0x3A);reply(0x3A,2,0,1800,false);assert(!guide_posts);preserved();
    now=1800;assert(setting_service_take_basic_timeouts()==SETTING_REQUEST_TIMEOUT_FO_MODE);
    reply(0x3A,2,0,1050,false);assert(!guide_posts);preserved();
    fixture();request(0x3A);setting_service_cancel_all();reply(0x3A,2,0,1050,false);assert(!guide_posts);preserved();
    now=1100;assert(setting_service_request_fo_mode(1));
    reply(0x3A,2,0,1050,false);assert(!guide_posts);preserved();
    reply(0x3A,2,0,1101,false);assert(guide_posts==1);preserved();
    fixture();request(0x3A);page=7;reply(0x3A,2,0,1050,false);
    assert(page==7&&guide_posts==1&&!test_notice_count);preserved();
    fixture();request(0x3A);reply(0x3A,0,0,1050,false);assert(!guide_posts);preserved();
    reply(0x3A,2,0,1051,false);assert(guide_posts==1);preserved();
    fixture();request(0x3A);profile_owned=true;reply(0x3A,2,0,1050,false);
    assert(profile_rejections==1&&!guide_posts&&!test_notice_count);preserved();
    fixture();request(0x3A);assert(!setting_service_request_fo_mode(1)&&sends==1);
    reply(0x3A,2,0,1050,false);assert(guide_posts==1);preserved();
    fixture();send_failed=true;assert(!setting_service_request_fo_mode(3)&&sends==1);
    reply(0x3A,2,0,1050,false);assert(!guide_posts);preserved();
    send_failed=false;now=1100;assert(setting_service_request_fo_mode(3));
    reply(0x3A,2,0,1050,false);assert(!guide_posts);preserved();
    reply(0x3A,2,0,1101,false);assert(guide_posts==1);preserved();
    fixture();request(0x3A);
    const uint8_t short_frame[]={0xFD,0xDF,0x06,0x3A,0x02,0x00};
    protocol_request_result_context_begin(1050);
    assert(app_setting_reply_handle_basic(0x3A,short_frame,sizeof(short_frame))==APP_SETTING_REPLY_ACTION_NONE);
    protocol_request_result_context_end();assert(!guide_posts);preserved();
    reply(0x3A,2,0,1051,false);assert(guide_posts==1);preserved();
}
static void test_sort_success_and_boot_query_owner(void)
{
    for(uint8_t target=0;target<4;++target) {
        fixture();assert(setting_service_request_fo_mode(target));
        reply(0x3A,1,target,1050,false);
        assert(machine_state_fo_mode()==target&&!guide_posts&&sends==1);
        reply(0x3A,2,0,1051,false);assert(!guide_posts&&machine_state_fo_mode()==target);
        now=1800;assert(setting_service_take_basic_timeouts()==SETTING_REQUEST_TIMEOUT_NONE);
    }
    /* 05/XX is a Boot/status reply, never the completion of a set request.
     * XX=02 is the ORT state, not a negative set ACK. */
    fixture();request(0x3A);reply(0x3A,2,5,1050,false);
    assert(!guide_posts&&machine_state_fo_mode()==2);
    reply(0x3A,2,0,1051,false);assert(guide_posts==1&&machine_state_fo_mode()==2);
    fixture();request(0x3A);reply(0x3A,4,5,1050,false);assert(!guide_posts);preserved();
    reply(0x3A,2,0,1051,false);assert(guide_posts==1);preserved();
    fixture();request(0x3A);reply(0x3A,2,5,1050,false);now=1800;
    assert(setting_service_take_basic_timeouts()==SETTING_REQUEST_TIMEOUT_FO_MODE);
    fixture();reply(0x3A,2,5,1050,false);assert(!guide_posts&&machine_state_fo_mode()==2);
}

int main(void)
{
    test_matched_failures_and_profile();test_wrong_shape_timeout_invalid_type();test_companion_boundaries();test_setting_to_currency_generation();test_busy_replies_defer_and_cancel();test_begin_observer_contract();test_sort_all_four_wire_targets();test_sort_zero_nak_and_owner_lifecycle();test_sort_success_and_boot_query_owner();
    puts("PASS setting refusals: real matched 04/02, 06/02, 3A/valid-mode/02 use transient guides on Main/menu without physical blockers; setting-to-currency begin/sendfail/timeout/cancel invalidate old association; busy refusal defers to idle; profile ownership preserved; invalid type/shape/stale/unrequested retain state; generic 3A/00/02 bound to current owner; Boot query never consumes set owner; companion coalescing one-shot, new request/cancel/count-active/time bound; sound remains ordinary notice; no 3D/CLEAR/START");
}
