#include "un260/protocol/protocol_tx_gate.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
static uint64_t now=1000;
uint64_t app_clock_monotonic_ms(void){return now;}
static unsigned writes;
static bool fail;
static atomic_bool contended;
static const uint8_t optional[]={0xFD,0xDF,6,0x4A,1,0x0A};
static int write_frame(const uint8_t *f,uint16_t n){assert(f&&n==6);++writes;return fail?-1:n;}
static void *try_optional(void *unused){(void)unused;assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);atomic_store(&contended,true);return NULL;}
static int write_contended(const uint8_t *f,uint16_t n){pthread_t t;assert(!pthread_create(&t,NULL,try_optional,NULL));pthread_join(t,NULL);assert(atomic_load(&contended));return write_frame(f,n);}
int main(void){
 const uint8_t a[]={0xFD,0xDF,6,0x04,1,0x0A},b[]={0xFD,0xDF,6,0x16,1,0x0A};
 protocol_tx_gate_reset();assert(protocol_tx_gate_send(a,6,false,write_contended)==6);
 assert(protocol_tx_gate_send(b,6,false,write_frame)==6);assert(writes==2);now+=50;
 assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);
 protocol_tx_gate_on_reply(0x67);now+=10;assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);
 protocol_tx_gate_on_reply(0x04);now+=10;assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);
 protocol_tx_gate_on_reply(0x16);assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);
 now+=10;assert(protocol_tx_gate_send(optional,6,true,write_frame)==6);assert(writes==3);
 fail=true;assert(protocol_tx_gate_send(a,6,false,write_frame)==-1);fail=false;now+=10;assert(protocol_tx_gate_send(optional,6,true,write_frame)==6);
 assert(protocol_tx_gate_send(a,6,false,write_frame)==6);now+=299;assert(protocol_tx_gate_send(optional,6,true,write_frame)==0);
 now++;assert(protocol_tx_gate_send(optional,6,true,write_frame)==6);
 assert(protocol_tx_gate_send(a,6,false,write_frame)==6);protocol_tx_gate_reset();assert(protocol_tx_gate_send(optional,6,true,write_frame)==6);
 assert(protocol_tx_gate_send(NULL,6,true,write_frame)==-1);
 puts("PASS TX gate: all normal commands tracked, matching replies, multiple outstanding commands, atomic write admission, normal priority, failed write, timeout, reset");
}
