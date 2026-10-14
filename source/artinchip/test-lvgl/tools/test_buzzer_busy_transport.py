#!/usr/bin/env python3
"""Exercise the real sender, RX parser and buzzer against a busy controller."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
serial=(root/'un260/app_service/app_serial_runtime.c').read_text()
names=['app_serial_runtime_observe_reply','protocol_send_is_ready']
if 'static int protocol_write_frame' in serial:names+=['protocol_write_frame','protocol_send_frame','protocol_send_if_idle']
names+=['protocol_send']
code=r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include "un260/app_service/buzzer_service.h"
#include "un260/protocol/protocol_send.h"
#include "un260/protocol/protocol_rx_service.h"
#if __has_include("un260/protocol/protocol_tx_gate.h")
#include "un260/protocol/protocol_tx_gate.h"
#endif
static int bus[2],input[2],fd4,fd6=9;
static atomic_uint opened,closed,dropped,queued;
static atomic_bool stop_controller,lose_business_ack,delay_on_ack;
static atomic_ullong business_reply_ms,on_sent_ms,on_reply_ms,off_sent_ms;
uint64_t app_clock_monotonic_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000ULL+t.tv_nsec/1000000;}
bool uart_tx_is_healthy(int fd){(void)fd;return true;}
bool runtime_guard_has_fault(void){return false;}
int uart_recv(int fd,char *buf,int len,int timeout){struct pollfd p={.fd=fd,.events=POLLIN};return poll(&p,1,timeout)>0?(int)read(fd,buf,len):0;}
void uart_printf(int fd,const char *fmt,...){(void)fd;(void)fmt;}
void uart_log_hex(int fd,const char *p,const uint8_t *d,size_t n,size_t l){(void)fd;(void)p;(void)d;(void)n;(void)l;}
int uart_send(int fd,const char *p,int n){assert(fd==fd4);return (int)write(bus[1],p,n);}
bool protocol_frame_queue_push_wait_at(const uint8_t *f,int len,uint32_t wait,uint64_t at){
 (void)wait;(void)at;assert(len==6||len==7);(void)f;atomic_fetch_add(&queued,1);return true;
}
static void reply(uint8_t cmd,uint8_t state){uint8_t f[]={0xFD,0xDF,6,cmd,state,0x0A};assert(write(input[1],f,6)==6);}
static void *controller(void *unused){
 (void)unused;uint64_t busy_until=0,on_ack_due=0;uint8_t business=0,frame[6];size_t have=0;
 while(!atomic_load(&stop_controller)){
  uint64_t now=app_clock_monotonic_ms();
  if(busy_until&&now>=busy_until){busy_until=0;if(!atomic_load(&lose_business_ack)){atomic_store(&business_reply_ms,now);reply(business,1);}}
  if(on_ack_due&&now>=on_ack_due){on_ack_due=0;atomic_store(&on_reply_ms,now);reply(0x4A,1);}
  struct pollfd p={.fd=bus[0],.events=POLLIN};if(poll(&p,1,1)<=0)continue;
  int n=read(bus[0],frame+have,6-have);assert(n>0);have+=(size_t)n;if(have<6)continue;have=0;
  now=app_clock_monotonic_ms();uint8_t cmd=frame[3],value=frame[4];
  if(busy_until){atomic_fetch_add(&dropped,1);continue;}
  if(cmd!=0x4A){business=cmd;busy_until=now+((cmd==0x38||cmd==0x39||cmd==0x3A)?1:50);continue;}
  if(value==1){atomic_fetch_add(&opened,1);atomic_store(&on_sent_ms,now);
   if(atomic_load(&delay_on_ack))on_ack_due=now+60;else{atomic_store(&on_reply_ms,now);reply(cmd,value);}}
  else{atomic_fetch_add(&closed,1);atomic_store(&off_sent_ms,now);reply(cmd,value);}
 }
 return NULL;
}
'''
test=r'''
int main(void){
 assert(pipe(input)==0&&pipe(bus)==0);fd4=bus[1];
 protocol_rx_service_set_observer(app_serial_runtime_observe_reply);
 assert(protocol_rx_service_start(input[0],9));assert(buzzer_service_start());pthread_t peer;assert(!pthread_create(&peer,NULL,controller,NULL));
 const uint8_t commands[]={0x04,0x06,0x16,0x38,0x39,0x3A,0x67};
 for(unsigned i=0;i<sizeof(commands);++i){
  uint8_t value=1;unsigned before=atomic_load(&opened);assert(protocol_send(commands[i],&value,1)==6);buzzer_service_click();usleep(220000);
  printf("command=%02X physical_on=%u dropped=%u\n",commands[i],atomic_load(&opened)-before,atomic_load(&dropped));fflush(stdout);
  assert(atomic_load(&opened)==before+1);assert(atomic_load(&dropped)==0);
  assert(atomic_load(&on_sent_ms)>=atomic_load(&business_reply_ms));
  assert(atomic_load(&off_sent_ms)>=atomic_load(&on_reply_ms)+30);
 }
 /* Delayed ON confirmation cannot be cut off before its accepted pulse. */
 atomic_store(&delay_on_ack,true);unsigned before=atomic_load(&opened);buzzer_service_click();usleep(250000);
 assert(atomic_load(&opened)==before+1);assert(atomic_load(&off_sent_ms)>=atomic_load(&on_reply_ms)+30);atomic_store(&delay_on_ack,false);
 /* Missing business ACK: expire the click instead of beeping long afterwards. */
 atomic_store(&lose_business_ack,true);uint8_t value=1;before=atomic_load(&opened);assert(protocol_send(0x04,&value,1)==6);buzzer_service_click();usleep(400000);
 assert(atomic_load(&opened)==before);atomic_store(&lose_business_ack,false);buzzer_service_click();usleep(200000);assert(atomic_load(&opened)==before+1);
 /* Muting during the bounded wait cancels the click before its command ACK. */
 before=atomic_load(&opened);assert(protocol_send(0x06,&value,1)==6);buzzer_service_click();usleep(10000);buzzer_service_cancel();usleep(200000);assert(atomic_load(&opened)==before);
 buzzer_service_click();usleep(180000);assert(atomic_load(&opened)==before+1);
 /* Stop while waiting for the lane must not send an ON after shutdown. */
 before=atomic_load(&opened);assert(protocol_send(0x06,&value,1)==6);buzzer_service_click();buzzer_service_stop();usleep(80000);assert(atomic_load(&opened)==before);
 atomic_store(&stop_controller,true);pthread_join(peer,NULL);protocol_rx_service_stop();protocol_rx_service_set_observer(NULL);
 close(input[0]);close(input[1]);close(bus[0]);close(bus[1]);puts("PASS busy controller: shared sender waits for every command reply, delayed ON ACK, bounded stale discard, recovery and shutdown");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-sound-busy-') as temp:
 p=Path(temp)/'test.c';p.write_text(code+'\n'.join(function(serial,n) for n in names)+test);exe=p.with_suffix('')
 sources=['un260/app_service/buzzer_service.c','un260/protocol/protocol_rx_service.c','un260/protocol/protocol_frame_parser.c','un260/protocol/protocol_frame.c','un260/protocol/stream_data_reply.c']
 if (root/'un260/protocol/protocol_tx_gate.c').exists():sources+=['un260/protocol/protocol_tx_gate.c']
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(root),str(p),*[str(root/f) for f in sources],'-pthread','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
