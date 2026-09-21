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
static uint64_t now;
uint64_t app_clock_monotonic_ms(void){return now;}
bool counting_history_can_start(void){return true;}
int protocol_send(uint8_t c,const uint8_t*d,uint16_t n){(void)c;(void)d;return n+5;}
int main(void){
 uint8_t reply[]={0xfd,0xdf,7,0x0a,1,1,0};
 assert(counting_action_request_start());
 now=1001;assert(!counting_action_take_timeouts());
 now=1551;counting_action_handle_reply(0x0a,reply,7);
 assert(!counting_action_start_pending()&&!counting_action_take_timeouts());
 now=2000;assert(counting_action_request_start());
 now=7000;assert(counting_action_take_timeouts()&COUNTING_ACTION_TIMEOUT_START);
 assert(!counting_action_start_pending());
 puts("PASS captured 1551ms START ACK, real 5s timeout, retry unlocked");
}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);(p/'test.c').write_text(code)
    for opt in ('-O0','-O2'):
        subprocess.run(['cc','-std=c11',opt,'-Wall','-Wextra','-Werror',f'-I{root}',str(p/'test.c'),str(root/'un260/counting/counting_action_service.c'),str(root/'un260/protocol/protocol_request.c'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
