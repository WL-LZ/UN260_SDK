#!/usr/bin/env python3
"""Exercise production routing, busy gates and settings migration without UI stubs for transport parsing."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[start:], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[match.start():start + token.end()]
    raise AssertionError(name)

protocol = (ROOT / 'un260/app_service/app_protocol_runtime.c').read_text()
command = (ROOT / 'un260/app_service/app_command_runtime.c').read_text()
ui = (ROOT / 'un260/app_service/app_ui_runtime.c').read_text()
counting = (ROOT / 'un260/app_service/app_counting_runtime.c').read_text()
cmake = (ROOT / 'CMakeLists.txt').read_text()
assert cmake.count('un260/lv_core/page_dynamic_capture.c') == 1
assert 'page_28_get_image' not in cmake and 'page_31_get_wave' not in cmake
assert ui.count('ui_page_dynamic_capture_poll(now_ms)') == 1
assert 'ui_page_28_' not in ui and 'ui_page_31_' not in ui
assert 'auto_wave_pending' not in counting and 'get_wave' not in counting
assert 'auto_wave_pending' not in (ROOT / 'un260/counting/counting_session_state.h').read_text()
assert 'auto_wave_pending' not in (ROOT / 'un260/counting/counting_info_reply.c').read_text()

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/settings_catalog.h"
#include "un260/protocol/stream_data_reply.h"
#define UI_N_(s) s
static unsigned routed;
static uint8_t routed_command;
static uint16_t routed_len;
static const uint8_t *routed_payload;
static bool capture_busy, mode_ready=true;
static void ui_page_dynamic_capture_on_frame(uint8_t cmd,const uint8_t *payload,uint16_t len)
{routed++;routed_command=cmd;routed_payload=payload;routed_len=len;}
static void uart_debug_printf(const char *fmt,...){(void)fmt;}
static bool ui_page_dynamic_capture_is_capturing(void){return capture_busy;}
static bool work_mode_service_diagnostic_ready(void){return mode_ready;}
static const char *work_mode_service_status_msgid(void){return "Mode waiting";}
static bool protocol_send_is_ready(void){return true;}
#define UPGRADE_SESSION_NONE 0
static int upgrade_session_owner(void){return 0;}
static bool app_command_runtime_count_start_busy(void){return false;}
typedef struct {bool session_active;} calibration_state_snapshot_t;
static void diagnostic_calibration_get_snapshot(calibration_state_snapshot_t *out){out->session_active=false;}
static bool diagnostic_calibration_allows_feed(void){return true;}
static bool machine_state_aging_running(void){return false;}
static bool motor_test_service_busy(void){return false;}
'''
code += function(protocol, 'handle_stream_data_reply') + '\n'
code += function(command, 'diagnostic_operation_blocker') + '\n'
code += r'''
int main(void){
 uint8_t frame[408]={0xfd,0xdf,(uint8_t)408,0x47,1,0,1};
 handle_stream_data_reply(0x47,frame,sizeof(frame));
 assert(routed==1&&routed_command==0x47&&routed_payload==frame+4&&routed_len==403);
 uint8_t wave[250]={0xfd,0xdf,250,0x48,10};
 handle_stream_data_reply(0x48,wave,sizeof(wave));
 assert(routed==2&&routed_command==0x48&&routed_payload==wave+4&&routed_len==245);
 wave[3]=0x49;handle_stream_data_reply(0x49,wave,sizeof(wave));assert(routed==2);
 capture_busy=true;assert(strstr(diagnostic_operation_blocker(false),"previous capture"));
 mode_ready=false;assert(strstr(diagnostic_operation_blocker(true),"previous capture"));
 capture_busy=false;assert(!strcmp(diagnostic_operation_blocker(false),"Mode waiting"));
 mode_ready=true;assert(!diagnostic_operation_blocker(false));
 size_t count;const settings_node_t *nodes=settings_catalog(&count);assert(settings_catalog_validate(nodes,count));
 const settings_node_t *capture=settings_catalog_find(nodes,count,"capture");
 assert(capture&&capture->page==UI_PAGE_IMAGE_GET&&!strcmp(capture->title,"Dynamic data capture"));
 assert(!strcmp(capture->hint,"Images and sensor signals"));
 assert(settings_catalog_find(nodes,count,"image")==capture&&settings_catalog_find(nodes,count,"wave")==capture);
 unsigned routes=0;for(size_t i=0;i<count;i++)if(nodes[i].page==UI_PAGE_IMAGE_GET||nodes[i].page==UI_PAGE_WAVE_GET)routes++;
 assert(routes==1);
 settings_node_t custom[2]={{.id="image"},{.id="capture"}};
 assert(settings_catalog_find(custom,2,"image")==custom);
 puts("PASS actual stream routing 47/48, uint16 payload length, capture RUN blocker, single settings entry and legacy IDs");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-dynamic-integration-') as directory:
    work = Path(directory)
    (work / 'test.c').write_text(code)
    for optimization in ('-O0','-O2'):
        binary=work/'test'
        subprocess.run([os.environ.get('CC','cc'),'-std=c11',optimization,'-Wall','-Wextra','-Werror','-Wno-missing-field-initializers',
                        '-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(ROOT),str(work/'test.c'),
                        str(ROOT/'un260/lv_core/settings_catalog.c'),str(ROOT/'un260/protocol/stream_data_reply.c'),
                        str(ROOT/'un260/protocol/protocol_frame.c'),'-o',str(binary)],check=True)
        subprocess.run([str(binary)],check=True)
