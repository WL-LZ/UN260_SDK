#!/usr/bin/env python3
"""Real request observer and notice queue: presentation must not change transport."""
from pathlib import Path
import subprocess, tempfile
from test_i18n_support import with_i18n

root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/app_service/setting_service.h"
static uint64_t now;
static unsigned sends,observed;
static uint8_t sent_command,sent_value,observed_command;
static int send_result;
uint64_t app_clock_monotonic_ms(void){return now;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t len){
    assert(len);++sends;sent_command=command;sent_value=payload[0];return send_result;
}
bool work_mode_service_request(uint8_t target){return target<2;}
static void accepted(uint8_t command){++observed;observed_command=command;}
int main(void){
    setting_service_set_request_observer(accepted);
    assert(setting_service_request_speed(0));
    assert(sends==1&&observed==1&&sent_command==0x16&&observed_command==0x16&&sent_value==3);
    assert(!setting_service_request_speed(1));assert(sends==1&&observed==1);
    uint8_t target;assert(setting_service_take_speed_result(&target)&&target==0);
    assert(!setting_service_take_speed_result(&target));assert(observed==1);
    send_result=-1;assert(!setting_service_request_speed(1));assert(sends==2&&observed==1);
    send_result=0;assert(setting_service_request_speed(1));assert(observed==2&&sent_value==2);
    now=801;assert(setting_service_take_basic_timeouts()==SETTING_REQUEST_TIMEOUT_SPEED);
    assert(observed==2);
    assert(setting_service_request_batch_number(50,false,0));
    assert(observed==3&&observed_command==0x06&&sent_value==50);
    assert(!setting_service_request_batch_number(100,false,0));assert(observed==3);
    setting_service_cancel_all();
    assert(setting_service_request_work_mode(1));assert(observed==4&&observed_command==0x38);
    assert(!setting_service_request_work_mode(2));assert(observed==4);
    setting_service_set_request_observer(NULL);
    assert(setting_service_request_add(true));assert(observed==4&&sent_command==0x39&&sent_value==1);
    puts("PASS request observer: accepted only, busy/send-fail/result/timeout/cancel unchanged, wire payload preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-notice-policy-') as temp:
    temp=Path(temp);source=temp/'observer.c';source.write_text(code)
    groups=[('observer',[source,root/'un260/app_service/setting_service.c',root/'un260/protocol/protocol_request.c',root/'un260/protocol/mode_codec.c']),
            ('queue',with_i18n([root/'tools/tests/test_ui_notice_state.c',root/'un260/lv_components/ui_notice_state.c'],root))]
    for name,sources in groups:
        exe=temp/name
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-fsanitize=address,undefined','-no-pie','-I'+str(root),*map(str,sources),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
