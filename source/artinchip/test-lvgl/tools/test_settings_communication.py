#!/usr/bin/env python3
"""Replay controller frames against production settings request/dispatch code.

UART, time and view edges are isolated; the print parser, request services,
CFD response projection and page teardown functions are compiled unchanged.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
parser.add_argument('--baseline', type=Path)
args = parser.parse_args()
root = args.root

def read(name):
    return (root / name).read_text()

def function(source, name):
    signature = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert signature, name
    start = source.index('{', signature.start())
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    depth = 0
    for token in tokens.finditer(source, start):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[signature.start():token.end()]
    raise AssertionError(name)

reply_path = 'un260/app_service/app_setting_reply_detail.c'
reply = (args.baseline / reply_path).read_text() if args.baseline else read(reply_path)
cfd = read('un260/lv_core/page_27_set_cfd_level.c')
motor = read('un260/lv_core/page_17_motor_test.c')
settings = read('un260/lv_core/page_06_settings.c')
assert 'ui_page_27_set_cfd_level_query' not in settings
assert 'cfd_query();' in function(cfd, 'ui_page_27_set_cfd_level_create')
assert 'cfd_service_cancel_update' not in function(cfd, 'ui_page_27_set_cfd_level_destroy')

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/print/print_config.h"
#include "un260/cfd/cfd.h"
#include "un260/protocol/auxiliary_reply.h"
static uint64_t now_ms;
static int send_result;
static unsigned sends, print_replies, boot_replies;
static uint8_t last_cmd, last_payload[32];
static print_config_request_result_t last_print;
uint64_t app_clock_monotonic_ms(void) { return now_ms; }
int protocol_send(uint8_t cmd, const uint8_t *payload, uint16_t len) {
    sends++; last_cmd=cmd; assert(len<=sizeof(last_payload));
    memcpy(last_payload, payload, len); return send_result;
}
static void uart_debug_printf(const char *fmt, ...) { (void)fmt; }
static void ui_page_20_set_print_on_reply(const print_config_request_result_t *r) {
    print_replies++; last_print=*r;
}
static void ui_page_20_set_print_on_boot_setting(const uint8_t *data, uint16_t len) {
    assert(data && len>=2); boot_replies++;
}
'''
code += function(reply, 'setting_reply_handle_print')
code += r'''
static void print_request(uint8_t sub, uint8_t content) {
    print_config_value_t value; print_config_get(&value); value.content=content;
    uint8_t payload[]={sub, content};
    assert(print_config_request(sub, payload, sizeof(payload), &value));
    assert(last_cmd==0x41 && last_payload[0]==sub);
}
static unsigned popup_count;
static void poll_print_timeout(void) {
    print_config_request_result_t result;
    if (print_config_take_timeout(&result)) {
        ui_page_20_set_print_on_reply(&result); popup_count++;
    }
}
static void test_print(void) {
    const uint8_t captured[]={0xFD,0xDF,0x06,0x41,0x01,0x04};
    print_config_value_t before, after;
    /* Exact user frame: previous code rejects it and fires an 800ms popup. */
    print_config_get(&before); now_ms=1000; print_request(1,2);
    now_ms+=20; setting_reply_handle_print(captured,sizeof(captured));
    if (EXPECT_BASELINE) {
        assert(print_replies==0);
        now_ms+=800; poll_print_timeout();
        assert(popup_count==1 && last_print.timeout);
        puts("REPRODUCED baseline: captured success ACK discarded, pending request times out and requests communication popup");
        return;
    }
    assert(print_replies==1 && last_print.success && !last_print.timeout);
    print_config_get(&after); assert(after.content==2);
    now_ms+=1000; poll_print_timeout(); assert(popup_count==0);
    setting_reply_handle_print(captured,sizeof(captured)); assert(print_replies==1);
    /* Status-only result follows the outstanding header/spacing sub-command. */
    for (uint8_t sub=2;sub<=3;sub++) {
        print_request(sub,3); setting_reply_handle_print(captured,sizeof(captured));
        assert(last_print.sub_command==sub && last_print.success);
    }
    /* Reject and malformed statuses never publish the requested configuration. */
    uint8_t failure[]={0xFD,0xDF,6,0x41,0,0};
    print_config_get(&before); print_request(2,1);
    setting_reply_handle_print(failure,sizeof(failure));
    assert(!last_print.success && !last_print.timeout);
    print_config_get(&after); assert(memcmp(&before,&after,sizeof(before))==0);
    now_ms+=1000; poll_print_timeout(); assert(popup_count==0);
    print_request(3,1); unsigned previous=print_replies;
    uint8_t invalid[]={0xFD,0xDF,6,0x41,0xFE,0};
    setting_reply_handle_print(invalid,sizeof(invalid));
    setting_reply_handle_print(captured,5); assert(print_replies==previous);
    now_ms+=800; poll_print_timeout(); assert(popup_count==1);
    setting_reply_handle_print(captured,sizeof(captured)); assert(!last_print.success);
    print_config_get(&after); assert(memcmp(&before,&after,sizeof(before))==0);
    /* Deadline reached before polling: a late ACK cannot succeed. */
    print_request(2,1); now_ms+=800;
    setting_reply_handle_print(captured,sizeof(captured)); poll_print_timeout();
    assert(popup_count==2 && last_print.timeout);
    /* Explicit cancellation/shutdown and send failure leave no later popup. */
    print_request(2,1); print_config_cancel_request(); previous=print_replies;
    setting_reply_handle_print(captured,sizeof(captured)); assert(print_replies==previous);
    send_result=-1; uint8_t payload[]={1,1};
    assert(!print_config_request(1,payload,2,&before)); send_result=0;
    now_ms+=1000; poll_print_timeout(); assert(popup_count==2);
    /* Existing extended replies and boot synchronization keep their routing. */
    uint8_t extended[]={0xFD,0xDF,7,0x41,2,1,0};
    print_request(2,1); setting_reply_handle_print(extended,sizeof(extended));
    assert(last_print.success && last_print.sub_command==2);
    uint8_t boot[]={0xFD,0xDF,7,0x41,1,3,0};
    setting_reply_handle_print(boot,sizeof(boot)); assert(boot_replies==1);
    puts("PASS print: captured ACK, all subcommands, reject, malformed, timeout, late/duplicate/cancelled ACK, send failure, legacy routing");
}
typedef int lv_obj_t;
static lv_obj_t object;
static lv_obj_t *cfd_level_page, *currency_label;
static unsigned selected_scene, cfd_refreshes, deletes;
static bool cfd_draft_valid;
static cfd_state_value_t cfd_draft;
#define CFD_CELL_COUNT (CFD_SCENE_COUNT * CFD_ITEM_COUNT)
static lv_obj_t *level_cells[CFD_CELL_COUNT], *level_cell_labels[CFD_CELL_COUNT];
static lv_obj_t *table_rows[CFD_SCENE_COUNT], *table_row_labels[CFD_SCENE_COUNT];
static struct { lv_obj_t *card, *check; } scene_items[CFD_SCENE_COUNT];
static bool lv_obj_is_valid(lv_obj_t *p) { return p!=NULL; }
static void lv_obj_del(lv_obj_t *p) { assert(p); deletes++; }
static void cfd_refresh_view(void) { cfd_refreshes++; }
'''
for name in ('cfd_normalize_level', 'ui_page_27_set_cfd_level_on_info',
             'ui_page_27_set_cfd_level_destroy'):
    code += function(cfd, name)
code += r'''
static void test_cfd(void) {
    const uint8_t captured[]={0xFD,0xDF,0x15,0x45,0x43,0x4E,0x59,1,
                             3,3,3,3,3,3,3,3,3,3,3,3,0xF0};
    cfd_level_page=&object; assert(cfd_service_request_query("CNY"));
    ui_page_27_set_cfd_level_on_info(captured+4,sizeof(captured)-4);
    assert(!cfd_service_busy() && cfd_refreshes==1);
    now_ms+=1000; assert(!cfd_service_take_query_timeout());
    assert(cfd_service_request_query("USD"));
    ui_page_27_set_cfd_level_on_info(captured+4,sizeof(captured)-4);
    assert(cfd_service_busy());
    ui_page_27_set_cfd_level_destroy(); assert(!cfd_service_busy());
    now_ms+=1000; assert(!cfd_service_take_query_timeout());
    ui_page_27_set_cfd_level_on_info(captured+4,sizeof(captured)-4);
    assert(cfd_refreshes==1); /* hidden late reply does not redraw */
    cfd_level_page=&object; assert(cfd_service_request_query("CNY"));
    now_ms+=800; assert(cfd_service_take_query_timeout());
    /* Destroy only cancels reads: accepted and missing write ACKs still settle. */
    cfd_state_value_t value; cfd_state_get(&value);
    assert(cfd_service_request_update(&value,0));
    ui_page_27_set_cfd_level_destroy(); assert(cfd_service_busy());
    ui_page_27_set_cfd_level_on_info(captured+4,sizeof(captured)-4);
    assert(!cfd_service_busy() && cfd_refreshes==1);
    assert(cfd_service_request_update(&value,0));
    ui_page_27_set_cfd_level_destroy(); now_ms+=800;
    assert(cfd_service_take_update_timeout());
    puts("PASS CFD: captured reply, currency mismatch, close/read cancellation, reopen, hidden ACK, write timeout retained");
}
static lv_obj_t *motor_test_page;
static struct { uint8_t cmd_g; uint8_t stop_cmd[2]; } motors[]={
    {0x52,{0,0}}, {0x53,{0,0}}, {0x54,{1,2}}};
'''
# The real destructor resets status_label pointers, retained in the fixture.
code = code.replace('uint8_t stop_cmd[2]; } motors[]=',
                    'uint8_t stop_cmd[2]; lv_obj_t *status_label; } motors[]=')
code = code.replace('{0x52,{0,0}}, {0x53,{0,0}}, {0x54,{1,2}}',
                    '{0x52,{0,0},NULL}, {0x53,{0,0},NULL}, {0x54,{1,2},NULL}')
code += r'''
#define UI_TEXT_SETTINGS_MOTOR_STOP 0
#define LV_EVENT_CLICKED 1
typedef int lv_event_t;
static int lv_event_get_code(lv_event_t *e) { return *e; }
static const char *ui_text_get(int id) { (void)id; return "STOP"; }
static void motor_status_set(void *item,const char *text) { (void)item; (void)text; }
static unsigned stop_commands[3];
static bool settings_detail_send_command(uint8_t cmd,const uint8_t *data,uint16_t len) {
    assert(cmd>=0x52 && cmd<=0x54 && len==2);
    assert(data[0]==(cmd==0x54?1:0) && data[1]==(cmd==0x54?2:0));
    stop_commands[cmd-0x52]++; return true;
}
void ui_page_17_motor_test_destroy(void);
static void ui_manager_pop_page(void) { ui_page_17_motor_test_destroy(); }
'''
for name in ('motor_test_stop_all', 'motor_test_esc_cb', 'ui_page_17_motor_test_destroy'):
    code += function(motor, name)
code += r'''
static void test_motor(void) {
    ui_page_17_motor_test_destroy(); assert(stop_commands[0]==0);
    motor_test_page=&object; lv_event_t event=0;
    motor_test_esc_cb(&event); assert(stop_commands[0]==0);
    event=LV_EVENT_CLICKED; motor_test_esc_cb(&event);
    for (unsigned i=0;i<3;i++) assert(stop_commands[i]==1);
    ui_page_17_motor_test_destroy(); assert(stop_commands[0]==1);
    motor_test_page=&object; ui_page_17_motor_test_destroy();
    for (unsigned i=0;i<3;i++) {
        assert(stop_commands[i]==2);
        uint8_t frame[]={0xFD,0xDF,6,(uint8_t)(0x52+i),1,0};
        auxiliary_reply_result_t r=auxiliary_reply_dispatch(frame[3],frame,6);
        assert(r.kind==AUXILIARY_REPLY_MOTOR_ACK && r.value==1);
        assert(auxiliary_reply_dispatch(frame[3],frame,5).kind==AUXILIARY_REPLY_INVALID);
        assert(auxiliary_reply_dispatch(frame[3],frame,7).kind==AUXILIARY_REPLY_INVALID);
    }
    puts("PASS motor: ESC and direct exit stop once, no-page/repeated teardown silent, all three ACKs, malformed replies");
}
int main(void) { test_print(); test_cfd(); test_motor(); return 0; }
'''

with tempfile.TemporaryDirectory(prefix='un260-settings-communication-') as directory:
    work = Path(directory)
    (work/'test.c').write_text('#define EXPECT_BASELINE '+str(int(bool(args.baseline)))+'\n'+code)
    sources = ['un260/print/print_config.c', 'un260/cfd/cfd.c',
               'un260/protocol/protocol_request.c', 'un260/protocol/auxiliary_reply.c']
    for opt in ('-O0','-O2'):
        binary = work/'test'
        subprocess.run(['cc','-std=c11',opt,'-Wall','-Wextra','-Werror',
                        '-fsanitize=undefined','-fno-sanitize-recover=all',
                        '-I'+str(root),str(work/'test.c'),
                        *[str(root/s) for s in sources],'-o',str(binary)], check=True)
        subprocess.run([str(binary)],check=True)
