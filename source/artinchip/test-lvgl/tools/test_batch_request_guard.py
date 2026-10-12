#!/usr/bin/env python3
"""Actual Batch request owner with real protocol_request timing and single flight."""
from pathlib import Path
import hashlib
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{.*?^\}", source, re.M | re.S)
    assert match, name
    return match.group()


HEAD = r'''
#include <assert.h>
#include <stdio.h>
#include "un260/app_service/setting_service.h"
#include "un260/protocol/protocol_request.h"
#define SETTING_BATCH_TRACE_ENABLE 0
static uint64_t now=100;
static bool fail_send,queued;
static unsigned sends,accepted;
static uint8_t sent;
uint64_t app_clock_monotonic_ms(void){return now;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t size){
 assert(command==6&&size==1);sends++;sent=payload[0];return fail_send?-1:6;
}
static void setting_request_accepted(uint8_t command){assert(command==6);accepted++;}
static bool pending_reply(uint8_t command,uint64_t start,uint64_t deadline){
 assert(command==6&&start==1000&&deadline==1800);return queued;
}
'''

TEST = r'''
int main(void){
 setting_batch_result_t result;
 assert(!setting_service_request_batch_number(0,false,200));
 assert(!setting_service_request_batch_switch(true,200,false,200));assert(sends==0);
 assert(setting_service_request_batch_switch(true,10,false,200));
 assert(sends==1&&accepted==1&&sent==10);
 assert(!setting_service_request_batch_switch(true,50,false,200));
 assert(!setting_service_request_batch_number(99,false,200));assert(sends==1);
 assert(!setting_service_batch_take_result(4,&result)); // Batch full is not an ACK.
 assert(!setting_service_batch_take_result(3,&result));
 assert(!setting_service_batch_take_result(1,NULL));
 protocol_request_result_context_begin(99);assert(!setting_service_batch_take_result(1,&result));
 protocol_request_result_context_end();
 assert(!setting_service_request_batch_number(99,false,200));assert(sends==1);
 protocol_request_result_context_begin(120);assert(setting_service_batch_take_result(1,&result));
 protocol_request_result_context_end();
 assert(result.type==SETTING_BATCH_REQUEST_SWITCH&&result.target.enable&&result.target.num==10);
 assert(!result.previous.enable&&result.previous.num==200);
 assert(!setting_service_batch_take_result(1,&result));
 now=200;assert(setting_service_request_batch_number(50,true,10));
 assert(sent==50&&setting_service_batch_take_result(2,&result));
 assert(result.type==SETTING_BATCH_REQUEST_NUMBER&&result.target.num==50&&result.previous.num==10);
 // A send error releases this owner immediately, without a fake successful reply.
 fail_send=true;assert(!setting_service_request_batch_switch(false,200,true,10));
 assert(!setting_service_batch_take_result(1,&result));fail_send=false;
 assert(setting_service_request_batch_switch(false,200,true,10));assert(sent==200);
 assert(setting_service_batch_take_result(1,&result)&&!result.target.enable&&result.target.num==200);
 // A valid queued reply that arrived before expiry survives delayed UI dispatch.
 now=1000;assert(setting_service_request_batch_switch(true,100,true,10));
 protocol_request_set_pending_reply_probe(pending_reply);queued=true;now=5000;
 assert(!setting_service_batch_take_timeout(&result));
 assert(!setting_service_request_batch_number(99,true,10));
 protocol_request_result_context_begin(1750);assert(setting_service_batch_take_result(1,&result));
 protocol_request_result_context_end();assert(result.target.num==100);
 // Without a queued on-time reply, real expiry releases exactly once.
 protocol_request_set_pending_reply_probe(NULL);now=6000;
 assert(setting_service_request_batch_switch(true,150,true,100));now=6800;
 assert(!setting_service_batch_take_result(1,&result));
 assert(setting_service_batch_take_timeout(&result)&&result.target.num==150&&result.previous.num==100);
 assert(!setting_service_batch_take_timeout(&result)&&!setting_service_batch_take_result(1,&result));
 now=7000;assert(setting_service_request_batch_switch(true,10,true,100));
 assert(setting_service_batch_take_result(1,&result));
 puts("PASS actual Batch request owner + protocol_request: numeric/OFF single-flight, no-data/Batch-full/old reply exclusion, send failure release, matched ACK, queued on-time response, true timeout release");
 return 0;
}
'''


def main():
    paths=[ROOT/'un260/app_service/setting_service.c',ROOT/'un260/protocol/protocol_request.c']
    before=[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
    source=paths[0].read_text()
    begin=source.index('typedef struct {\n    protocol_request_t request;\n    setting_batch_request_type_t type;')
    end=source.index('\ntypedef struct {',begin+1)
    timeout=re.search(r'^#define SETTING_REQUEST_TIMEOUT_MS .*$',source,re.M).group()
    functions='\n'.join(function(source,name) for name in ('setting_batch_request_begin',
        'setting_service_request_batch_number','setting_service_request_batch_switch',
        'setting_service_batch_take_result','setting_service_batch_take_timeout'))
    for optimization in ('-O0','-O2'):
        with tempfile.TemporaryDirectory(prefix='un260-batch-owner-') as directory:
            work=Path(directory);test=work/'test.c';binary=work/'test'
            test.write_text(HEAD+'\n'+timeout+'\n'+source[begin:end]+'\n'+functions+'\n'+TEST)
            subprocess.run(['cc','-std=gnu11',optimization,'-g','-Wall','-Wextra','-Werror',
                '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
                '-I'+str(ROOT),str(test),str(paths[1]),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
    assert before==[hashlib.sha256(p.read_bytes()).hexdigest() for p in paths]
    print('PASS O0/O2 ASan+UBSan; actual Batch service and protocol request inputs unchanged')


if __name__=='__main__':
    main()
