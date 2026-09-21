#!/usr/bin/env python3
"""Compile production calibration routing/callbacks with the real diagnostic service."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "un260/lv_core/page_09_cis_cala.c").read_text()

def function(name):
    match = re.search(r"^(?:static )?[\w *]+\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    assert match, name
    start = source.index("{", match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[start:], re.S):
        if token.group() == "{": depth += 1
        elif token.group() == "}":
            depth -= 1
            if not depth: return source[match.start():start+token.end()]
    raise AssertionError(name)

# Layout is exercised by the actual-LVGL suite. This smaller protocol test
# extracts only production callbacks and uses the real diagnostic state machine.
assert not re.search(r'\b(?:settings_detail_send_command|protocol_send|diagnostic_calibration_begin)\s*\(',
                     function("ui_page_cis_calib_create"))

code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "un260/diagnostic/diagnostic.h"
#include "un260/app_service/work_mode_service.h"
typedef int lv_obj_t;
typedef int lv_event_t;
typedef int lv_timer_t;
typedef int gesture_action_t;
typedef struct { lv_obj_t *root; } lv_settings_frame_t;
enum { LV_EVENT_CLICKED=1, UI_PAGE_CIS_CALIB=9, UI_PAGE_MAIN=1,
       GESTURE_ACTION_HOME=1, GESTURE_ACTION_EXIT_PAGE=2, GESTURE_ACTION_OTHER=3 };
static lv_obj_t object,*cis_page,*status_title,*status_detail,*start_button,*mode_retry_button;
static lv_settings_frame_t frame;
static lv_timer_t timer_object,*status_timer;
static bool selected_white_balance,send_ok=true,gate=true,leave_home,send_failed;
static uint8_t sent_cmd;
static uint32_t now_ms=100,holds;
static unsigned sends,pops,homes,refreshes,deleted,timers_deleted,policy_clears;
static void (*leave_confirm)(void *);
static int lv_event_get_code(lv_event_t *e){return *e;}
static uint32_t app_clock_uptime_ms(void){return now_ms;}
static bool settings_detail_send_command(uint8_t cmd,const uint8_t *sub,uint16_t len){
    assert(len==1&&sub[0]==1);sent_cmd=cmd;sends++;return send_ok;
}
bool work_mode_service_diagnostic_ready(void){return gate;}
const char *app_command_runtime_calibration_blocker(void){return gate?NULL:"Waiting for manual mode";}
void work_mode_service_hold_operation(uint32_t owner,bool active){
    if(active)holds|=owner;else holds&=~owner;
}
static void cis_calib_ui_refresh(void){refreshes++;}
static void ui_manager_pop_page(void){pops++;}
static bool ui_manager_suspend_to_home(void){homes++;return true;}
static void lv_label_set_text(lv_obj_t *label,const char *text){assert(label&&text&&*text);}
static void lv_obj_del(lv_obj_t *page){assert(page==&object);deleted++;}
static void lv_timer_del(lv_timer_t *timer){assert(timer==&timer_object);timers_deleted++;}
static void gesture_service_clear_page_policy(unsigned page){assert(page==UI_PAGE_CIS_CALIB);policy_clears++;}
static void settings_detail_dialog_hide(void){leave_confirm=NULL;}
static bool settings_detail_dialog_show(const char *title,const char *body,
    const char *accept,const char *cancel,void (*cb)(void *),void (*cancel_cb)(void *),void *data){
    assert(title&&body&&accept&&cancel&&cb);assert(!cancel_cb&&!data);leave_confirm=cb;return true;
}
'''
for name in ("calibration_leave", "calibration_leave_warning", "calibration_gesture",
             "ui_page_cis_calib_select", "cis_back", "cis_start",
             "ui_page_cis_calib_destroy"):
    code += function(name)
code += r'''
/* No layout stubs: opening binds callback-owned objects only. Creation and
 * pointer interaction are covered by the separate actual-LVGL renderer test. */
static void open_callback_fixture(bool white){
    assert(!cis_page);ui_page_cis_calib_select(white);
    cis_page=status_title=status_detail=start_button=mode_retry_button=&object;
    frame.root=&object;status_timer=&timer_object;
}
static diagnostic_reply_result_t status_reply(uint8_t cmd,uint8_t status,uint8_t len){
    const uint8_t reply[]={0xFD,0xDF,6,cmd,status,0};
    const diagnostic_reply_hooks_t hooks={cis_calib_ui_refresh};
    return diagnostic_reply_dispatch(cmd,reply,len,now_ms,&hooks);
}
static void test_targets_and_callbacks(void){
    lv_event_t click=LV_EVENT_CLICKED,other=0;
    calibration_state_snapshot_t state;
    for(unsigned white=0;white<2;white++){
        unsigned before=sends;open_callback_fixture(white);assert(sends==before);
        ui_page_cis_calib_select(!white);assert(selected_white_balance==(bool)white);
        cis_start(&other);assert(sends==before);
        gate=false;cis_start(&click);assert(sends==before);gate=true;
        cis_start(&click);assert(sends==before+1&&sent_cmd==(white?0x5F:0x5B));
        assert(holds&WORK_MODE_OPERATION_CALIBRATION);
        diagnostic_calibration_get_snapshot(&state);
        assert(state.session_active&&state.target==(white?CALIB_TARGET_CB:CALIB_TARGET_CIS));
        cis_start(&click);assert(sends==before+1);
        assert(!diagnostic_calibration_begin(white?CALIB_TARGET_CIS:CALIB_TARGET_CB,now_ms));
        unsigned before_pops=pops;cis_back(&other);cis_back(&click);
        assert(pops==before_pops&&leave_confirm);leave_confirm=NULL;
        assert(calibration_gesture(GESTURE_ACTION_HOME));
        assert(calibration_gesture(GESTURE_ACTION_EXIT_PAGE));
        assert(!calibration_gesture(GESTURE_ACTION_OTHER));
        assert(status_reply(0x5B,2,5)==DIAGNOSTIC_REPLY_INVALID);
        assert(status_reply(0x5B,0xFE,6)==DIAGNOSTIC_REPLY_IGNORED);
        if(!white)assert(status_reply(0x5F,2,6)==DIAGNOSTIC_REPLY_IGNORED);
        else assert(status_reply(0x5B,3,6)==DIAGNOSTIC_REPLY_IGNORED);
        unsigned before_refresh=refreshes;
        assert(status_reply(0x5B,1,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
        assert(status_reply(0x5B,2,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
        assert(refreshes==before_refresh+2);
        diagnostic_calibration_get_snapshot(&state);
        assert(!state.session_active&&!state.timed_out);
        assert(white?state.cb_state==CB_CALIB_SUCCESS:state.cis_state==CIS_CALIB_SUCCESS);
        assert(!calibration_gesture(GESTURE_ACTION_HOME));
        cis_back(&click);assert(pops==before_pops+1);
        ui_page_cis_calib_destroy();assert(!cis_page&&!status_timer&&!mode_retry_button);
        assert(status_reply(0x5B,2,6)==DIAGNOSTIC_REPLY_IGNORED);
    }
    /* Older firmware may send a 0x5F result, only for the CB target. */
    open_callback_fixture(true);cis_start(&click);
    assert(status_reply(0x5F,5,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
    diagnostic_calibration_get_snapshot(&state);assert(state.cb_state==CB_CALIB_FAIL_IR&&!state.session_active);
    ui_page_cis_calib_destroy();
    /* CIS-specific upper/lower/IR failure codes remain distinct. */
    for(uint8_t result=3;result<=5;result++){
        open_callback_fixture(false);cis_start(&click);
        assert(status_reply(0x5B,result,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
        diagnostic_calibration_get_snapshot(&state);
        assert(state.cis_state==(cis_calib_state_t)result&&!state.session_active);
        ui_page_cis_calib_destroy();
    }
}
static void test_send_timeout_and_late_reply(void){
    lv_event_t click=LV_EVENT_CLICKED;
    calibration_state_snapshot_t state;
    open_callback_fixture(true);send_ok=false;holds=0;
    cis_start(&click);diagnostic_calibration_get_snapshot(&state);
    assert(send_failed&&!state.session_active&&state.cb_state==CB_CALIB_IDLE&&!holds);
    send_ok=true;cis_start(&click);assert(!send_failed&&(holds&1));
    uint32_t started=now_ms;
    assert(!diagnostic_calibration_poll(started+DIAGNOSTIC_CALIBRATION_TIMEOUT_MS-1));
    now_ms=started+DIAGNOSTIC_CALIBRATION_TIMEOUT_MS;
    assert(diagnostic_calibration_poll(now_ms));
    diagnostic_calibration_get_snapshot(&state);
    assert(state.timed_out&&state.session_active&&state.cb_state==CB_CALIB_RUNNING);
    assert(!diagnostic_calibration_poll(now_ms+1)); /* report one timeout */
    unsigned before=sends;cis_start(&click);assert(sends==before);
    unsigned before_pops=pops;cis_back(&click);assert(pops==before_pops&&leave_confirm);
    leave_confirm(NULL);assert(pops==before_pops+1);settings_detail_dialog_hide();
    assert(calibration_gesture(GESTURE_ACTION_HOME)&&leave_confirm);
    leave_confirm(NULL);assert(homes==1);
    ui_page_cis_calib_destroy();
    diagnostic_calibration_get_snapshot(&state);
    assert(state.session_active&&state.timed_out&&state.cb_state==CB_CALIB_RUNNING&&(holds&1));
    unsigned before_deleted=deleted;ui_page_cis_calib_destroy();assert(deleted==before_deleted);
    now_ms++;
    assert(status_reply(0x5B,2,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
    diagnostic_calibration_get_snapshot(&state);
    assert(!state.session_active&&!state.timed_out&&state.cb_state==CB_CALIB_SUCCESS);
    assert(status_reply(0x5B,2,6)==DIAGNOSTIC_REPLY_IGNORED);
    /* A late running update clears timeout and refreshes the silence deadline. */
    open_callback_fixture(false);cis_start(&click);started=now_ms;
    now_ms+=DIAGNOSTIC_CALIBRATION_TIMEOUT_MS;
    assert(diagnostic_calibration_poll(now_ms));
    assert(status_reply(0x5B,1,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
    diagnostic_calibration_get_snapshot(&state);
    assert(state.session_active&&!state.timed_out&&state.cis_state==CIS_CALIB_RUNNING);
    assert(!diagnostic_calibration_poll(now_ms+DIAGNOSTIC_CALIBRATION_TIMEOUT_MS-1));
    /* Forced destruction also retains a non-timeout running session. */
    ui_page_cis_calib_destroy();diagnostic_calibration_get_snapshot(&state);
    assert(state.session_active&&!state.timed_out&&state.cis_state==CIS_CALIB_RUNNING);
    assert(status_reply(0x5B,4,6)==DIAGNOSTIC_REPLY_CALIBRATION_UPDATED);
    diagnostic_calibration_get_snapshot(&state);
    assert(!state.session_active&&state.cis_state==CIS_CALIB_FAIL_LOWER);
    assert(deleted==timers_deleted&&deleted==policy_clears);
}
int main(void){
    test_targets_and_callbacks();test_send_timeout_and_late_reply();
    puts("PASS calibration: production callbacks, manual gate, explicit targets, short/invalid frames, 5F/5B compatibility, mutual exclusion, send failure, timeout retains session, Back/Home confirmation, late terminal/running replies and repeated destroy");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="un260-calibration-route-") as temp:
    work = Path(temp)
    (work / "test.c").write_text(code)
    for opt in ("-O0", "-O2"):
        subprocess.run(["cc", "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=undefined", "-fno-sanitize-recover=all", "-I"+str(root),
                        str(work/"test.c"), str(root/"un260/diagnostic/diagnostic.c"),
                        "-o", str(work/"test")], check=True)
        subprocess.run([str(work/"test")], check=True)
