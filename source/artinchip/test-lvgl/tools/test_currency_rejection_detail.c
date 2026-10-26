/* Currency exception only: real request/state/reply and production dispatch. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "un260/currency/currency_state.h"
#include "un260/currency/currency_service.h"
#include "un260/currency/currency_reply.h"
#include "un260/counting/counting_session_state.h"
#include "un260/counting/counting_detail_state.h"
#include "un260/counting/counting_data_types.h"
#include "un260/machine_state/machine_fault.h"
#include "un260/protocol/protocol_request.h"
#include "un260/protocol/protocol_frame.h"
#include "test_notice_sink.h"

#define UI_DATA_TOPIC_CURRENCY_CATALOG 1U
static counting_session_state_t g_counting_session;
static counting_detail_state_t g_counting_detail_state;
static uint32_t g_count_end_generation;
static counting_sim_t count_data, original_count_data;
static char serial0[]="SN-A1234567",serial1[]="SN-B7654321";
static char *serials[]={serial0,serial1};
static uint8_t reject_pcs[]={1},reject_codes[]={0x10};
static uint64_t now;
static unsigned uart_calls,expected_uart_calls,control_calls,island_calls,recovery_calls,clear_calls,resets,projections;
static bool start_pending;
static currency_switch_result_t projected_result;

uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
void uart_debug_printf(const char *format,...){(void)format;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t length)
{assert(command==0x03&&payload&&length==3);++uart_calls;return -1;}
static bool counting_action_start_pending(void){return start_pending;}
static bool app_command_runtime_count_start_busy(void){return start_pending || g_counting_session.start_confirmed || g_counting_session.phase==COUNTING_SESSION_ACTIVE;}
static counting_sim_t *counting_data_mutable(void){return &count_data;}
static void fault_popup_report_setting_rejection(uint8_t command,uint8_t reason)
{ assert(command==3 && reason==0x80); ++island_calls;
  assert(!machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SETTING,reason,command},NULL)); }
static void fault_popup_resolve_setting_rejection(uint8_t command)
{ machine_fault_clear_code(MACHINE_FAULT_SETTING,command); }
static void ui_manager_publish_data_changed(uint32_t topic){(void)topic;}
static bool counting_history_prepare_reset(counting_session_state_t *s,counting_sim_t *d,uint32_t tick)
{(void)s;(void)d;(void)tick;return true;}
static bool counting_history_prepare_start(counting_session_state_t *s,counting_sim_t *d,uint32_t tick)
{(void)s;(void)d;(void)tick;return true;}
static bool app_counting_runtime_reset_session(counting_session_state_t *s,const char *reason)
{(void)s;(void)reason;++resets;return true;}
static void sim_reset_for_currency(counting_sim_t *d){(void)d;++resets;}
static void counting_action_cancel_all(void){}
static bool counting_action_request_clear(void){++clear_calls;return true;}
static void app_currency_runtime_expect_denom_refresh(counting_detail_state_t *s){(void)s;}
static void counting_denom_query_invalidate(counting_detail_state_t *s){(void)s;}
static void page_07_curr_apply_switch_result(const currency_switch_result_t *result)
{projected_result=*result;++projections;}
static bool app_protocol_runtime_handle_reply(uint8_t command,const uint8_t *frame,uint16_t length)
{(void)command;(void)frame;(void)length;return false;}
static bool app_command_runtime_handle_stacker_clear(const uint8_t *frame,uint8_t length)
{(void)frame;(void)length;return true;}
static void app_fault_recovery_handle_clear_result(uint8_t code){(void)code;++recovery_calls;}
static void page_01_main_set_counting_locked(bool locked){(void)locked;}
static void counting_report_begin(bool add,counting_sim_t *data){(void)add;(void)data;}
static bool machine_state_add_enabled(void){return false;}
static bool counting_report_discard_stale(uint8_t command,const uint8_t *frame,uint8_t length)
{(void)command;(void)frame;(void)length;return false;}
static void counting_action_handle_reply(uint8_t command,const uint8_t *frame,uint8_t length)
{(void)command;(void)frame;(void)length;}
static void app_currency_runtime_handle_detected(counting_detail_state_t *detail,const uint8_t *frame,uint8_t length)
{(void)detail;(void)frame;(void)length;}
static void counting_denom_reply_begin_count(counting_detail_state_t *detail){(void)detail;}
static bool app_setting_runtime_handle_reply(uint8_t command,uint8_t *frame,uint8_t length)
{(void)command;(void)frame;(void)length;return false;}
static void app_boot_runtime_handle_reply(counting_session_state_t *s,uint8_t command,const uint8_t *frame,uint8_t length)
{(void)s;(void)command;(void)frame;(void)length;}
static void app_counting_runtime_handle_info(counting_session_state_t *s,counting_sim_t *d,const uint8_t *frame,uint8_t length)
{(void)s;(void)d;(void)frame;(void)length;}
static void app_counting_runtime_handle_control(uint8_t command,counting_session_state_t *s,const uint8_t *frame,uint8_t length)
{
    (void)s;++control_calls;
    if(command==0x0F&&length==6&&frame[4])
        machine_fault_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,frame[4]});
}
static void app_counting_runtime_handle_denom(counting_detail_state_t *d,counting_session_state_t *s,counting_sim_t *data,const uint8_t *frame,uint8_t length)
{(void)d;(void)s;(void)data;(void)frame;(void)length;}
static void app_counting_runtime_handle_detail(uint8_t command,counting_detail_state_t *d,counting_session_state_t *s,counting_sim_t *data,const uint8_t *frame,uint8_t length)
{(void)command;(void)d;(void)s;(void)data;(void)frame;(void)length;}

/* Isolated rejection fixture: no ADD reset, no-note run or mode request. */
static bool g_add_result_cleared;
static bool app_command_runtime_handle_no_note_motion(uint8_t cmd,const uint8_t *frame,uint8_t len){(void)cmd;(void)frame;(void)len;return false;}
static bool app_add_runtime_before_count_start(void){return true;}
static void app_add_runtime_poll(void){}
static bool counting_denom_query_complete(counting_detail_state_t *state){state->query_pending=false;return true;}
static void count_mode_service_currency_result(const currency_switch_result_t *result){(void)result;}
static void count_mode_service_currency_sync(void){}
static void count_mode_service_count_started(void){}
static unsigned mode_clear_cancellations;
static void app_setting_runtime_cancel_mode_clear(void){mode_clear_cancellations++;}
#include "currency_rejection_under_test.inc"

static void preserved(void)
{
    char selected[4];currency_state_get_selected_code(selected);assert(!strcmp(selected,"GBP"));
    assert(!memcmp(&count_data,&original_count_data,sizeof(count_data)));
    assert(!strcmp(serial0,"SN-A1234567")&&!strcmp(serial1,"SN-B7654321"));
    assert(reject_pcs[0]==1&&reject_codes[0]==0x10);
    assert(uart_calls==expected_uart_calls&&!recovery_calls&&!clear_calls&&!resets);
}
static void fixture(void)
{
    currency_service_cancel_switch();currency_state_reset();machine_fault_clear();
    assert(currency_state_confirm_active_code("GBP"));
    memset(&g_counting_session,0,sizeof(g_counting_session));
    memset(&g_counting_detail_state,0,sizeof(g_counting_detail_state));
    memset(&count_data,0,sizeof(count_data));
    count_data.total_pcs=3;count_data.total_amount=90;count_data.denom_number=2;
    count_data.denom[0]=(denom_t){20,2,40};count_data.denom[1]=(denom_t){50,1,50};
    count_data.sn_str=serials;count_data.sn_capacity=2;count_data.denom_mix[0]=20;count_data.denom_mix[1]=50;
    count_data.err_pcs=reject_pcs;count_data.err_code=reject_codes;count_data.err_capacity=1;
    count_data.err_num=count_data.err_expected=1;
    original_count_data=count_data;
    now=1000;uart_calls=expected_uart_calls=control_calls=island_calls=recovery_calls=clear_calls=resets=projections=0;
    start_pending=false;test_notice_count=0;g_count_end_generation=0;
    memset(&setting_rejection,0,sizeof(setting_rejection));deferred_setting_rejection=0;
    protocol_request_set_start_observer(setting_request_started);
    present_deferred_setting_rejection();
    protocol_request_result_context_end();
}
static void begin_switch(void)
{
    uint8_t index;assert(currency_state_find_code("EUR",&index));
    assert(currency_service_request_switch(index,"EUR"));
}
static void dispatch_at(uint8_t *frame,uint16_t length,uint64_t received)
{
    protocol_request_result_context_begin(received);
    assert(app_command_runtime_dispatch(frame[3],frame,length));
    protocol_request_result_context_end();
}
static void currency_result(uint8_t status,uint64_t received)
{
    uint8_t frame[]={0xFD,0xDF,6,3,status,0};dispatch_at(frame,sizeof(frame),received);
}
static void runtime_fault(uint8_t code,uint64_t received)
{
    uint8_t frame[]={0xFD,0xDF,6,0x0F,code,0};dispatch_at(frame,sizeof(frame),received);
}
static void rejected(void){begin_switch();currency_result(2,1050);assert(projections==1&&!projected_result.success);}
static void ordinary_runtime(uint8_t code,uint64_t arrival)
{
    unsigned calls=control_calls;runtime_fault(code,arrival);
    if(control_calls!=calls+1||test_notice_count)
        fprintf(stderr,"ordinary runtime failed code=%u arrival=%llu control=%u old=%u notice=%u island=%u\n",code,(unsigned long long)arrival,control_calls,calls,test_notice_count,island_calls);
    assert(control_calls==calls+1&&!test_notice_count);
    assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code},NULL));
    preserved();
}
static void test_log_pair_and_one_shot(void)
{
    const uint64_t ages[]={0,1,799};
    for(unsigned i=0;i<sizeof(ages)/sizeof(ages[0]);++i) {
        fixture();rejected();now=9000; /* A backed-up UI must use RX arrival, not processing time. */
        runtime_fault(2,1050+ages[i]);
        assert(!control_calls&&machine_fault_count()==0&&island_calls==1&&!test_notice_count);
        assert(!machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SETTING,0x80,3},NULL));
        preserved();
        runtime_fault(2,1051+ages[i]);
        assert(control_calls==1&&machine_fault_count()==1&&island_calls==1&&!test_notice_count);
        preserved();
    }
}
static void test_missing_stale_reversed_and_delayed(void)
{
    fixture();ordinary_runtime(2,1050);
    fixture();currency_result(2,1050);assert(!projections);ordinary_runtime(2,1051);
    fixture();begin_switch();ordinary_runtime(2,1049);currency_result(2,1050);ordinary_runtime(2,1051);
    fixture();rejected();ordinary_runtime(2,1851);
    fixture();rejected();ordinary_runtime(2,1850); /* The 800ms boundary is exclusive. */
    fixture();rejected();ordinary_runtime(2,1049);
    fixture();rejected();runtime_fault(7,1051);assert(!control_calls&&island_calls==1);ordinary_runtime(2,1052);
    fixture();rejected();uint8_t other[]={0xFD,0xDF,6,0x15,1,0};
    dispatch_at(other,sizeof(other),1051);ordinary_runtime(2,1052);
    fixture();begin_switch();currency_result(2,1800);assert(!projections);ordinary_runtime(2,1801);
    fixture();begin_switch();uint8_t short_reply[]={0xFD,0xDF,5,3,2};
    dispatch_at(short_reply,sizeof(short_reply),1050);assert(!projections);ordinary_runtime(2,1051);
    for(unsigned code=1;code<=7;++code)if(code!=2&&code!=7) {
        fixture();rejected();ordinary_runtime((uint8_t)code,1051);ordinary_runtime(2,1052);
    }
}
static void test_cancel_new_request_timeout_and_success(void)
{
    fixture();begin_switch();currency_service_cancel_switch();currency_result(2,1050);ordinary_runtime(2,1051);
    fixture();begin_switch();
    assert(protocol_send(0x03,(const uint8_t *)"EUR",3)<0);expected_uart_calls=1;
    currency_service_cancel_switch(); /* The real selector's failed-send cancellation boundary. */
    currency_result(2,1050);ordinary_runtime(2,1051);
    fixture();rejected();currency_service_cancel_switch();ordinary_runtime(2,1051);
    fixture();rejected();begin_switch();ordinary_runtime(2,1051);
    fixture();begin_switch();now=1800;currency_switch_result_t result;
    assert(currency_service_take_switch_timeout(&result)&&result.timeout);ordinary_runtime(2,1801);
    fixture();begin_switch();currency_switch_result_t result2;
    protocol_request_result_context_begin(1050);
    assert(currency_service_take_switch_result(1,&result2)&&result2.success);
    protocol_request_result_context_end();ordinary_runtime(2,1051);
    /* A duplicate stale rejection must break adjacency without arming a second marker. */
    fixture();rejected();currency_result(2,1051);ordinary_runtime(2,1052);
    /* A status=01 whose target no longer exists is an invariant rejection,
     * not a raw controller currency rejection; it must not arm the exception. */
    fixture();begin_switch();currency_state_begin_list_sync();
    assert(currency_state_append_list_code(1,"GBP"));assert(currency_state_append_list_code(2,"USD"));
    assert(currency_state_finish_list_sync());currency_result(1,1050);
    assert(projections==1&&!projected_result.success);ordinary_runtime(2,1051);
}
static void test_active_pending_and_existing_faults(void)
{
    for(unsigned gate=0;gate<4;++gate) {
        fixture();rejected();
        if(gate==0)g_counting_session.start_confirmed=true;
        else if(gate==1)g_counting_session.phase=COUNTING_SESSION_ACTIVE;
        else if(gate==2)start_pending=true;
        else machine_fault_report((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,1});
        ordinary_runtime(2,1051);
        if(gate==3)assert(machine_fault_find((machine_fault_key_t){MACHINE_FAULT_SENSOR,0,1},NULL));
        machine_fault_clear();g_counting_session.start_confirmed=false;
        g_counting_session.phase=COUNTING_SESSION_IDLE;start_pending=false;
        ordinary_runtime(2,1052); /* Even a blocked exception consumes its one-frame marker. */
    }
    fixture();rejected();const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
    machine_fault_report(jam);ordinary_runtime(2,1051);assert(machine_fault_find(jam,NULL));
}
static void test_malformed_detail_consumes_adjacency(void)
{
    fixture();rejected();uint8_t long_frame[]={0xFD,0xDF,7,0x0F,2,0,0};
    dispatch_at(long_frame,sizeof(long_frame),1051);
    assert(control_calls==1&&island_calls==1&&!test_notice_count);ordinary_runtime(2,1052);
    fixture();rejected();uint8_t wrong_length[]={0xFD,0xDF,7,0x0F,2,0};
    dispatch_at(wrong_length,sizeof(wrong_length),1051);
    assert(control_calls==1&&island_calls==1&&!test_notice_count);ordinary_runtime(2,1052);
    fixture();rejected();uint8_t fault[]={0xFD,0xDF,6,0x0F,2,0};
    protocol_request_result_context_begin(1051);
    assert(!app_fault_recovery_observe_setting_detail(0x0F,fault,sizeof(fault),NULL));
    protocol_request_result_context_end();ordinary_runtime(2,1052);
}
static void test_malformed_currency_ack_preserves_pending(void)
{
    for(unsigned malformed=0;malformed<3;++malformed) {
        fixture();begin_switch();
        uint8_t frame[]={0xFD,0xDF,6,3,2,0,0};
        uint8_t length=6;
        if(malformed==0)frame[3]=0x04; /* Wrong command cannot consume a currency request. */
        else if(malformed==1){length=7;frame[2]=7;} /* Actual oversized ACK. */
        else frame[2]=7; /* Declared length disagrees with the actual six bytes. */
        protocol_request_result_context_begin(1050);
        assert(currency_reply_handle(frame,length).kind==CURRENCY_REPLY_INVALID);
        protocol_request_result_context_end();
        assert(currency_service_switch_pending());
        dispatch_at(frame,length,1050);
        assert(currency_service_switch_pending()&&!projections);
        ordinary_runtime(2,1051); /* Malformed ACK never arms the context marker. */
        assert(currency_service_switch_pending());
        machine_fault_clear();
        currency_result(2,1052); /* The original request still accepts its valid response. */
        assert(!currency_service_switch_pending()&&projections==1&&!projected_result.success);
        unsigned routed=control_calls;runtime_fault(2,1053);
        assert(control_calls==routed&&machine_fault_count()==0&&island_calls==1&&!test_notice_count);
        preserved();
    }
}
int main(void)
{
    test_log_pair_and_one_shot();test_missing_stale_reversed_and_delayed();
    test_cancel_new_request_timeout_and_success();test_active_pending_and_existing_faults();
    test_malformed_detail_consumes_adjacency();test_malformed_currency_ack_preserves_pending();
    puts("PASS currency rejection detail: accepted 03/02 is a transient operation guide without a physical record, adjacent 0F/02 is coalesced; RX arrival boundary, duplicate/stale/reversed/delayed/intervening/cancel/new-request/timeout/success/count-active/pending/existing-fault/malformed fallbacks; selected currency, count, amount, serials and rejects retained; no 3D/CLEAR/START");
}
