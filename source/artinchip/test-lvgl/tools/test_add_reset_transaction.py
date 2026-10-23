#!/usr/bin/env python3
"""Compile the real ADD transaction owner; mock only history/transport/UI endpoints.

Each scenario runs in a fresh process so the production module has no reset API
solely for testing. This suite does not claim physical controller validation.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from test_main_view import function
from test_currency_modes import block

ROOT = Path(__file__).resolve().parents[1]
SCENARIOS = (
    'enable', 'disable', 'setting-failed', 'setting-timeout', 'clear-failed',
    'clear-timeout', 'setting-send', 'clear-send', 'no-owner', 'history-full',
    'boundary-wait', 'commit-wait', 'hardware-setting', 'hardware-clear',
    'boundary-timeout', 'unmatched-setting',
    'setting-cancel', 'clear-cancel', 'unchanged-value', 'old-clear-result',
)

runtime = (ROOT / 'un260/app_service/app_command_runtime.c').read_text()
count_runtime = (ROOT / 'un260/app_service/app_counting_runtime.c').read_text()
platform = (ROOT / 'un260/lv_system/platform_app.c').read_text()
hooks = '\n\n'.join((
    function(count_runtime, 'app_counting_runtime_reset_session'),
    function(platform, 'sim_reset_counting_data'),
    function(platform, 'sim_reset_counting_group'),
    function(runtime, 'app_command_runtime_add_clear_allowed'),
    function(runtime, 'app_command_runtime_prepare_add_reset'),
    function(runtime, 'app_command_runtime_commit_add_reset'),
))
hook_fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_session_state.h"
#include "un260/counting/counting_detail_state.h"
static counting_session_state_t g_counting_session;
static counting_detail_state_t g_counting_detail_state;
static bool g_add_result_cleared, history_ok=true, multi_selected;
static unsigned blockers, preserved_history=7, history_checks, invalidations;
static unsigned reports, multi_resets, ui_resets;
enum {BOOT_STAGE_DONE=1,UPGRADE_SESSION_NONE=0};
#define BIT(n) (1U<<(n))
static bool app_command_runtime_count_start_busy(void){return (blockers&BIT(0))!=0;}
static bool counting_action_clear_pending(void){return (blockers&BIT(1))!=0;}
static bool protocol_send_is_ready(void){return (blockers&BIT(2))==0;}
static bool runtime_guard_has_fault(void){return (blockers&BIT(3))!=0;}
static bool ui_factory_reset_blocks_navigation(void){return (blockers&BIT(4))!=0;}
static bool touch_calibration_busy(void){return (blockers&BIT(5))!=0;}
static bool workspace_service_applying(void){return (blockers&BIT(6))!=0;}
static int upgrade_session_owner(void){return (blockers&BIT(7))?1:UPGRADE_SESSION_NONE;}
static bool work_mode_service_diagnostic_active(void){return (blockers&BIT(8))!=0;}
static bool machine_state_aging_running(void){return (blockers&BIT(9))!=0;}
static bool motor_test_service_busy(void){return (blockers&BIT(10))!=0;}
static int boot_service_get_stage(void){return (blockers&BIT(11))?0:BOOT_STAGE_DONE;}
static bool protocol_frame_queue_has_pending(void){return (blockers&BIT(12))!=0;}
static bool counting_multi_query_busy(void){return (blockers&BIT(13))!=0;}
static bool counting_multi_extra_busy(void){return (blockers&BIT(14))!=0;}
static bool g_deferred_frame_blocked;
static uint32_t app_clock_uptime_ms(void){return 500;}
static uint32_t lv_tick_get(void){return 500;}
static bool counting_history_prepare_reset(counting_session_state_t *session,
    const counting_sim_t *data,uint32_t now)
{assert(session==&g_counting_session&&data==counting_data_current()&&now==500);++history_checks;return history_ok;}
void counting_history_current_invalidate(void){++invalidations;}
static void counting_report_reset(void){++reports;}
static void counting_multi_reset(void){++multi_resets;}
static bool currency_state_multi_selected(void){return multi_selected;}
static void uart_debug_printf(const char *text,...){(void)text;}
#define app_auto_qr_cancel() ((void)0)
#define ui_count_end_anim_cancel() ((void)0)
#define smart_island_notify_count_reset() ((void)0)
#define app_setting_runtime_cancel_mode_clear() ((void)0)
#define stop_counting_sim() ((void)0)
#define currency_state_clear_detected_result() ((void)0)
#define page_01_curr_img_refre() ((void)0)
#define page_01_main_refresh_start_state() ((void)0)
#define smart_island_clear_count_analysis() ((void)0)
#define page_01_main_scroll_reset() ((void)0)
#define page_02_list_report_reset() (++ui_resets)
#define ui_refresh_main_page() ((void)0)
#define smart_island_refresh_summary() ((void)0)
'''
hook_cases = r'''
static void seed(void)
{
    counting_sim_t *data=counting_data_mutable();
    counting_data_clear_serials(data);counting_data_clear_errors(data);
    memset(data,0,sizeof(*data));
    data->total_pcs=100;data->total_amount=10000;
    data->last_total_pcs=80;data->last_total_amount=8000;
    data->last_valid_pcs=79;data->last_issue_pcs=3;
    data->last_suspect_pcs=2;data->last_damaged_pcs=1;
    data->denom_number=1;data->denom[0]=(denom_t){100,100,10000};
    assert(counting_data_ensure_serial_capacity(data,2));
    data->sn_str[0]=malloc(8);assert(data->sn_str[0]);memcpy(data->sn_str[0],"ABC123",7);
    data->denom_mix[0]=100;
    assert(counting_data_ensure_error_capacity(data,1));
    data->err_code[0]=17;data->err_pcs[0]=2;data->err_num=1;data->err_expected=2;
    memset(&g_counting_detail_state,0,sizeof(g_counting_detail_state));
    g_counting_detail_state.live_denom_started=true;
    g_counting_detail_state.live_denom_number=1;
    g_counting_detail_state.live_denom[0]=data->denom[0];
    g_counting_detail_state.live_reject_started=true;
    g_counting_detail_state.live_reject_number=1;
    g_counting_detail_state.query_started=true;
    g_counting_detail_state.query_complete=true;
    g_counting_detail_state.query_deferred=true;
    g_counting_detail_state.query_tick=321;
    memset(&g_counting_session,0,sizeof(g_counting_session));
    g_counting_session.history_record.valid=true;
    g_add_result_cleared=false;reports=multi_resets=ui_resets=0;
}
int main(void)
{
    seed();assert(app_command_runtime_add_clear_allowed());
    for(unsigned bit=0;bit<15;++bit){
        blockers=BIT(bit);unsigned previous=history_checks;
        assert(!app_command_runtime_prepare_add_reset()&&history_checks==previous);
    }
    blockers=0;g_deferred_frame_blocked=true;
    assert(!app_command_runtime_prepare_add_reset());g_deferred_frame_blocked=false;
    history_ok=false;assert(!app_command_runtime_prepare_add_reset());
    assert(!app_command_runtime_commit_add_reset());
    assert(counting_data_current()->total_pcs==100&&counting_data_current()->sn_str);
    assert(g_counting_detail_state.live_denom_started&&!g_add_result_cleared);
    assert(!reports&&!multi_resets&&!ui_resets&&preserved_history==7);
    history_ok=true;blockers=BIT(0);unsigned before=history_checks;
    assert(!app_command_runtime_commit_add_reset()&&history_checks==before);blockers=0;
    for(unsigned currency=0;currency<2;++currency)for(unsigned drain=0;drain<2;++drain){
        seed();multi_selected=currency;g_counting_detail_state.query_pending=drain;
        assert(app_command_runtime_prepare_add_reset());
        assert(app_command_runtime_commit_add_reset());
        const counting_sim_t *data=counting_data_current();
        assert(!data->total_pcs&&data->total_amount==0&&!data->denom_number);
        assert(!data->last_total_pcs&&data->last_total_amount==0&&!data->last_valid_pcs);
        assert(!data->last_issue_pcs&&!data->last_suspect_pcs&&!data->last_damaged_pcs);
        assert(!data->sn_str&&!data->sn_capacity&&!data->denom_mix[0]);
        assert(!data->err_pcs&&!data->err_code&&!data->err_num&&!data->err_expected);
        assert(data->multi_currency_result==(currency!=0));
        assert(!g_counting_detail_state.live_denom_started&&!g_counting_detail_state.live_reject_started);
        assert(!g_counting_detail_state.live_denom_number&&!g_counting_detail_state.live_reject_number);
        assert(!g_counting_detail_state.query_started&&!g_counting_detail_state.query_complete);
        assert(!g_counting_detail_state.query_deferred&&g_counting_detail_state.query_failed);
        assert(g_counting_detail_state.query_pending==(drain!=0));
        assert(g_counting_detail_state.query_expired==(drain!=0)&&g_counting_detail_state.query_tick==321);
        assert(g_add_result_cleared&&!g_counting_session.history_record.valid);
        assert(reports==1&&multi_resets==1&&ui_resets==1&&preserved_history==7);
    }
    puts("PASS actual ADD admission/commit/reset: 16 blocker gates, history refusal preserves data, complete totals/details/last_* baselines clear, old query drain retained without publication, single/MULTI scope, historical records retained");
}
'''

basic = (ROOT / 'un260/app_service/app_setting_reply_basic.c').read_text()
add_start = basic.index('case 0x39:')
add_reply = '''static void actual_add_reply(const uint8_t *buf, uint8_t len)
{switch(0x39) {case 0x39:''' + block(basic, basic.index('{', add_start)) + '''
break;default:break;}}'''
reply_fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/protocol/protocol_frame.h"
#define UI_N_(text) text
static bool owner_busy, waiting_setting, pending, confirmed_add, target;
static unsigned taken, confirms, callbacks, ordinary_results;
static bool app_add_runtime_busy(void){return owner_busy;}
static bool app_add_runtime_waiting_setting(void){return waiting_setting;}
static bool setting_service_add_is_pending(void){return pending;}
static bool setting_service_take_add_result(bool *out)
{if(!pending)return false;pending=false;++taken;if(out)*out=target;return true;}
static void machine_state_confirm_add(bool enabled){confirmed_add=enabled;++confirms;}
static bool machine_state_add_enabled(void){return confirmed_add;}
static void app_add_runtime_setting_result(bool requested,bool success)
{assert(waiting_setting&&requested==target&&success);waiting_setting=false;++callbacks;}
static void app_setting_notice_result(const char *key,const char *title,bool success)
{assert(key&&title&&success);++ordinary_results;}
static void ui_notice_clear(const char *key){assert(key);}
static void page_01_bottom_a_refresh_add(bool animation){(void)animation;}
static void page_03_update_menu_button_states_refresh(void){}
static void smart_island_refresh_summary(void){}
static void uart_debug_printf(const char *format,...){(void)format;}
'''
reply_cases = r'''
int main(void)
{
    uint8_t boot[]={0xFD,0xDF,7,0x39,2,0,0};
    uint8_t success[]={0xFD,0xDF,6,0x39,0,0};
    uint8_t malformed[]={0xFD,0xDF,7,0x39,0,0,0};
    owner_busy=waiting_setting=pending=target=true;
    actual_add_reply(boot,sizeof(boot));
    assert(pending&&!taken&&!confirms&&!callbacks&&!confirmed_add);
    actual_add_reply(malformed,sizeof(malformed));
    assert(pending&&!taken&&!confirms&&!callbacks);
    actual_add_reply(success,sizeof(success));
    assert(!pending&&taken==1&&confirms==1&&callbacks==1&&confirmed_add);
    assert(owner_busy&&!waiting_setting&&!ordinary_results);
    /* Once 39 is confirmed its slot is released, while the 3B/new-group owner
       remains active. Boot sync cannot overwrite that confirmed ADD state. */
    for(unsigned i=0;i<3;++i){actual_add_reply(boot,sizeof(boot));assert(confirmed_add&&confirms==1&&callbacks==1);}
    actual_add_reply(success,sizeof(success));
    assert(confirmed_add&&confirms==1&&callbacks==1&&taken==1);
    owner_busy=false;pending=true;target=false;
    actual_add_reply(success,sizeof(success));
    assert(!confirmed_add&&confirms==2&&callbacks==1&&ordinary_results==1);
    boot[5]=1;actual_add_reply(boot,sizeof(boot));
    assert(confirmed_add&&confirms==3&&callbacks==1&&!pending);
    puts("PASS actual 39 reply route: boot sync cannot consume pending setting or overwrite ADD during clear/commit/retry; strict ACK shape, duplicate ignore, user callback and profile confirmation remain distinct");
}
'''

with tempfile.TemporaryDirectory(prefix='un260-add-reset-') as directory:
    work = Path(directory)
    binary = work / 'test'
    subprocess.run([
        os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
        '-no-pie', '-I' + str(ROOT),
        str(ROOT / 'tools/test_add_reset_transaction.c'),
        str(ROOT / 'un260/app_service/app_add_runtime.c'), '-o', str(binary),
    ], check=True)
    for scenario in SCENARIOS:
        subprocess.run([str(binary), scenario], check=True, timeout=10)
    hook_source = work / 'hooks.c'
    hook_source.write_text(hook_fixture + '\n' + hooks + '\n' + hook_cases)
    hook_binary = work / 'hooks'
    subprocess.run([
        os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
        '-no-pie', '-I' + str(ROOT), str(hook_source),
        str(ROOT / 'un260/counting/counting_data_store.c'), '-o', str(hook_binary),
    ], check=True)
    subprocess.run([str(hook_binary)], check=True, timeout=10)
    reply_source = work / 'reply.c'
    reply_source.write_text(reply_fixture + '\n' + add_reply + '\n' + reply_cases)
    reply_binary = work / 'reply'
    subprocess.run([
        os.environ.get('CC', 'cc'), '-std=c11', '-O1', '-g', '-Wall', '-Wextra',
        '-Werror', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
        '-no-pie', '-I' + str(ROOT), str(reply_source),
        str(ROOT / 'un260/protocol/protocol_frame.c'), '-o', str(reply_binary),
    ], check=True)
    subprocess.run([str(reply_binary)], check=True, timeout=10)
