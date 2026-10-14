#!/usr/bin/env python3
"""Real RX/worker integration: ACK observed while UI dispatch is blocked."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
observer=function((root/'un260/app_service/app_serial_runtime.c').read_text(),'app_serial_runtime_observe_reply')
code=r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include "un260/app_service/buzzer_service.h"
#include "un260/protocol/protocol_rx_service.h"
#include "un260/protocol/protocol_tx_gate.h"
static int input[2];
static atomic_uint on,off,queued;
uint64_t app_clock_monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000ULL+t.tv_nsec/1000000;}
int uart_recv(int fd,char *buf,int len,int timeout){struct pollfd p={.fd=fd,.events=POLLIN};return poll(&p,1,timeout)>0?(int)read(fd,buf,len):0;}
void uart_printf(int fd,const char *fmt,...){(void)fd;(void)fmt;}
void uart_log_hex(int fd,const char *p,const uint8_t *d,size_t n,size_t l){(void)fd;(void)p;(void)d;(void)n;(void)l;}
bool protocol_frame_queue_push_wait_at(const uint8_t *f,int len,uint32_t wait,uint64_t at){
 (void)wait;(void)at;assert(len==6&&f[3]==0x4A);atomic_fetch_add(&queued,1);
 /* Deliberately no UI-side app_protocol_runtime_handle_reply call. */
 return true;
}
int protocol_send(uint8_t cmd,const uint8_t *p,uint16_t n){
 assert(cmd==0x4A&&n==1);if(*p==1)atomic_fetch_add(&on,1);else atomic_fetch_add(&off,1);
 uint8_t ack[]={0xFD,0xDF,6,0x4A,*p,0x0A};assert(write(input[1],ack,6)==6);return 6;
}
'''
code+='\nint protocol_send_if_idle(uint8_t cmd,const uint8_t *p,uint16_t n){return protocol_send(cmd,p,n); }\n'
test=r'''
int main(void){
 assert(pipe(input)==0);protocol_rx_service_set_observer(app_serial_runtime_observe_reply);
 assert(protocol_rx_service_start(input[0],9));assert(buzzer_service_start());
 for(unsigned i=1;i<=3;++i){buzzer_service_click();usleep(500000);
  assert(atomic_load(&on)==i&&atomic_load(&off)==i&&atomic_load(&queued)==i*2);}
 buzzer_service_stop();protocol_rx_service_stop();protocol_rx_service_set_observer(NULL);
 close(input[0]);close(input[1]);puts("PASS actual RX + buzzer: UI remains blocked for 500ms per click, OFF ACKs consumed before queue, next clicks still work, stop lifecycle");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-sound-rx-') as temp:
 p=Path(temp)/'test.c';p.write_text(code+observer+test);exe=p.with_suffix('')
 sources=['un260/protocol/protocol_tx_gate.c','un260/app_service/buzzer_service.c','un260/protocol/protocol_rx_service.c','un260/protocol/protocol_frame_parser.c','un260/protocol/protocol_frame.c','un260/protocol/stream_data_reply.c']
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),str(p),*[str(root/f) for f in sources],'-pthread','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
