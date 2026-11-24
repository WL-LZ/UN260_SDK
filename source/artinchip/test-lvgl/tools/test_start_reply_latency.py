"""Replay the supplied 1551 ms START latency against production request code."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/counting/counting_action_service.h"
#include "un260/protocol/protocol_request.h"
static uint64_t now;
static bool feedback_busy, send_fails;
static unsigned starts;
uint64_t app_clock_monotonic_ms(void){return now;}
bool counting_history_can_start(void){return true;}
int protocol_send(uint8_t c,const uint8_t*d,uint16_t n){(void)c;(void)d;return n+5;}

int protocol_send_when_feedback_clear(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    if (feedback_busy) return 0;
    if (send_fails) return -1;
    ++starts;
    ++now; /* The RX thread can queue a reply before this call returns. */
    return protocol_send(cmd, payload, len);
}

int main(void){
 uint8_t reply[]={0xfd,0xdf,7,0x0a,1,1,0};
 assert(counting_action_request_start());
 now=1001;assert(!counting_action_take_timeouts());
 now=1551;counting_action_handle_reply(0x0a,reply,7);
 assert(!counting_action_start_pending()&&!counting_action_take_timeouts());
 now=2000;assert(counting_action_request_start());
 now=7000;assert(counting_action_take_timeouts()&COUNTING_ACTION_TIMEOUT_START);
 assert(!counting_action_start_pending());
 now=8000;feedback_busy=true;assert(counting_action_request_start()&&counting_action_start_queued());
 assert(!counting_action_request_start());counting_action_handle_reply(0x0a,reply,7);
 now+=6000;assert(!counting_action_take_timeouts()&&counting_action_start_queued());
 unsigned previous=starts;uint64_t before_send=now;
 feedback_busy=false;assert(!counting_action_take_timeouts()&&starts==previous+1);
 protocol_request_result_context_begin(before_send-1);counting_action_handle_reply(0x0a,reply,7);
 assert(counting_action_start_pending());
 protocol_request_result_context_begin(before_send);counting_action_handle_reply(0x0a,reply,7);
 protocol_request_result_context_end();assert(!counting_action_start_pending());
 feedback_busy=true;assert(counting_action_request_start());counting_action_cancel_all();
 previous=starts;feedback_busy=false;assert(!counting_action_take_timeouts()&&starts==previous);
 feedback_busy=true;assert(counting_action_request_start());assert(counting_action_request_clear());
 assert(!counting_action_start_pending());counting_action_cancel_all();
 assert(counting_action_request_start());feedback_busy=false;send_fails=true;
 assert(counting_action_take_timeouts()==COUNTING_ACTION_SEND_FAILED_START);
 assert(!counting_action_start_pending()&&!counting_action_take_timeouts());
 puts("PASS captured 1551ms START ACK, real 5s timeout, retry unlocked");
}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'test.c').write_text(code)
    for opt in ('-O0','-O2'):
        subprocess.run(['cc','-std=c11',opt,'-Wall','-Wextra','-Werror',f'-I{root}',str(p/'test.c'),str(root/'un260/counting/counting_action_service.c'),str(root/'un260/protocol/protocol_request.c'),str(root/'un260/protocol/protocol_frame.c'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
