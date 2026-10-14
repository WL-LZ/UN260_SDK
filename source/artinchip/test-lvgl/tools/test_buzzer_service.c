#include "un260/app_service/buzzer_service.h"
#include "un260/protocol/protocol_frame.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned on,off;
static int mode;
static uint64_t opened,closed;
static uint64_t ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000ULL+t.tv_nsec/1000000;}
int protocol_send(uint8_t command,const uint8_t *payload,uint16_t length){
 assert(command==0x4A&&length==1&&(*payload==1||*payload==2));
 uint8_t frame[6];assert(protocol_frame_build(frame,sizeof(frame),command,payload,length)==6);
 assert(frame[0]==0xfd&&frame[1]==0xdf&&frame[2]==6&&frame[3]==0x4a&&frame[4]==*payload&&frame[5]==0x0a);
 pthread_mutex_lock(&lock);int m=mode;
 if(*payload==1){on++;opened=ms();}else{off++;closed=ms();}
 pthread_mutex_unlock(&lock);
 if(m==0||m==4)buzzer_service_on_reply(*payload);
 if(m==2&&*payload==1)buzzer_service_on_reply(0);
 if(m==3&&*payload==1)return -1;
 return 6;
}
int protocol_send_if_idle(uint8_t cmd,const uint8_t *p,uint16_t n){return protocol_send(cmd,p,n);}
static unsigned count(bool opening){pthread_mutex_lock(&lock);unsigned n=opening?on:off;pthread_mutex_unlock(&lock);return n;}
static void wait_count(bool opening,unsigned n){for(unsigned i=0;i<1000&&count(opening)<n;i++)usleep(1000);assert(count(opening)==n);}
static void reset(int m){pthread_mutex_lock(&lock);on=off=0;mode=m;pthread_mutex_unlock(&lock);assert(buzzer_service_start());}
int main(void){
 buzzer_service_cancel(); /* Safe before the serial worker is initialized. */
 reset(0);assert(!buzzer_service_start());buzzer_service_click();wait_count(true,1);
 for(int i=0;i<100;i++)buzzer_service_click();
 usleep(100000);assert(count(true)==1&&count(false)==1);
 pthread_mutex_lock(&lock);assert(closed-opened>=30&&closed-opened<150);pthread_mutex_unlock(&lock);
 usleep(40000);buzzer_service_click();wait_count(true,2);wait_count(false,2);buzzer_service_stop();buzzer_service_stop();
 /* The caller/UI is asleep; worker still closes. No callbacks or UI timer. */
 reset(1);buzzer_service_click();usleep(550000);assert(count(true)==1&&count(false)==3);
 /* A delayed confirmed OFF repairs the service without a UART restart. */
 buzzer_service_on_reply(2);pthread_mutex_lock(&lock);mode=0;pthread_mutex_unlock(&lock);
 buzzer_service_click();wait_count(true,2);wait_count(false,4);buzzer_service_stop();
 /* No OFF acknowledgement: a new contact probes OFF only, never ON. */
 reset(1);buzzer_service_click();usleep(1100000);assert(count(true)==1&&count(false)==3);
 pthread_mutex_lock(&lock);mode=0;pthread_mutex_unlock(&lock);
 buzzer_service_click();wait_count(false,4);usleep(150000);assert(count(true)==1);
 buzzer_service_click();wait_count(true,2);wait_count(false,5);buzzer_service_stop();
 reset(2);buzzer_service_click();usleep(550000);assert(count(true)==1&&count(false)==3);buzzer_service_stop();
 reset(3);buzzer_service_click();usleep(550000);assert(count(true)==1&&count(false)==3);buzzer_service_stop();
 reset(0);buzzer_service_click();wait_count(true,1);buzzer_service_stop();assert(count(false)>=1);
 buzzer_service_click();usleep(20000);assert(count(true)==1);
 puts("PASS: exact 0x4A frames, 35ms independent close, repeat throttle, ACK, missing/failure ACK, failed ON, late ACK recovery, OFF-only resynchronization, stop/restart");
}
