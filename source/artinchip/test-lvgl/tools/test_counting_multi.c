#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_multi.h"
static unsigned sends;static uint8_t last[4];static int fail_send;
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t n){assert(cmd==0x0b&&n==4&&data[0]==0);memcpy(last,data,n);sends++;return fail_send?-1:9;}
static void live(const char *code,unsigned amount,unsigned pcs,unsigned reject){uint8_t f[16]={0xfd,0xdf,16,14};memcpy(f+4,code,3);f[7]=amount>>24;f[8]=amount>>16;f[9]=amount>>8;f[10]=amount;f[11]=pcs>>8;f[12]=pcs;f[13]=reject;f[14]=1;assert(counting_multi_info(f,16));}
static void global(unsigned pcs,unsigned reject,unsigned status){uint8_t f[13]={0xfd,0xdf,13,14};f[8]=pcs>>8;f[9]=pcs;f[10]=reject;f[11]=status;assert(counting_multi_info(f,13));}
static void marker(unsigned value,unsigned time){uint8_t f[16]={0xfd,0xdf,16,11};memset(f+4,value,11);counting_multi_denom(f,16,time);}
static void item(unsigned value,unsigned pcs,unsigned time){uint8_t f[16]={0xfd,0xdf,16,11};char d[12];snprintf(d,sizeof(d),"%8u%3u",value,pcs);memcpy(f+4,d,11);counting_multi_denom(f,16,time);}
int main(void){
 const counting_multi_t *m=counting_multi_current();counting_multi_begin(false);global(0,0,0);
 assert(!counting_multi_latest());
 live("USD",100,1,9);live("USD",532,14,9);live("USD",532,14,9);live("CNY",5,1,9);live("CNY",10,2,9);
 assert(m->count==2&&m->total_pcs==16&&m->currencies[0].amount==532);assert(!counting_multi_request(0,0));
 assert(counting_multi_latest()==&m->currencies[1]);
 assert(counting_multi_latest()->amount==10);
 global(16,9,1);global(0,0,2);assert(!m->counting&&m->total_pcs==16&&m->reject==9);
 assert(counting_multi_request(0,10));counting_multi_poll(9);assert(m->currencies[0].status==MULTI_DETAIL_LOADING);
 assert(!memcmp(last,"\0USD",4));assert(!counting_multi_request(1,10));
 marker(0,11);item(100,3,12);item(100,3,12);item(50,3,13);item(20,2,14);item(10,3,15);item(5,2,16);item(2,1,17);
 assert(m->currencies[0].status==MULTI_DETAIL_LOADING);marker(255,18);
 assert(m->currencies[0].status==MULTI_DETAIL_READY&&m->currencies[0].denom_count==6);
 assert(counting_multi_request(1,20));marker(0,21);item(5,2,22);marker(255,23);assert(m->currencies[1].status==MULTI_DETAIL_READY);
 counting_multi_begin(true);assert(!counting_multi_latest());global(0,0,0);live("CNY",15,3,9);global(17,9,1);global(0,0,2);
 assert(m->count==2&&m->currencies[0].amount==532&&m->currencies[1].pcs==3&&m->total_pcs==17);
 assert(counting_multi_request(0,30));marker(0,31);item(100,1,32);counting_multi_poll(2600);
 assert(m->currencies[0].status==MULTI_DETAIL_TIMEOUT&&counting_multi_query_busy());assert(!counting_multi_request(1,2700));
 counting_multi_reset();assert(m->count==0&&counting_multi_query_busy());marker(255,2800);assert(!counting_multi_query_busy()&&m->count==0);
 counting_multi_begin(false);live("CNY",10,2,0);global(0,0,2);assert(counting_multi_request(0,3000));marker(0,3001);item(5,1,3002);marker(255,3003);assert(m->currencies[0].status==MULTI_DETAIL_INVALID&&m->currencies[0].amount==10);
 assert(counting_multi_request(0,3010));marker(0,3011);marker(255,3012);assert(m->currencies[0].status==MULTI_DETAIL_EMPTY);
 fail_send=1;assert(!counting_multi_request(0,3020));assert(!counting_multi_query_busy());fail_send=0;
 assert(counting_multi_request(0,3030));marker(0,3031);item(5,2,3032);item(5,3,3033);marker(255,3034);assert(m->currencies[0].status==MULTI_DETAIL_INVALID);
 assert(counting_multi_request(0,0xfffffff0u));counting_multi_poll(2600);assert(m->currencies[0].status==MULTI_DETAIL_TIMEOUT);marker(255,2601);
 counting_multi_reset();counting_multi_begin(false);uint8_t bad[16]={0xfd,0xdf,16,14,'u','s','d'};assert(!counting_multi_info(bad,16));assert(!counting_multi_info(bad,15));
 counting_multi_reset();counting_multi_begin(false);live("USD",100,1,0);live("CNY",10,2,0);
 unsigned before=sends;counting_multi_prefetch(4000);assert(sends==before);
 global(0,0,2);counting_multi_prefetch(3751);assert(sends==before);
 counting_multi_prefetch(4000);assert(sends==before);
 counting_multi_prefetch(4001);assert(sends==before+1&&!memcmp(last,"\0USD",4));
 counting_multi_prefetch(4002);assert(sends==before+1);
 marker(0,4003);item(100,1,4004);marker(255,4005);
 counting_multi_prefetch(4006);assert(sends==before+1);
 counting_multi_prefetch(4255);assert(sends==before+2&&!memcmp(last,"\0CNY",4));
 marker(0,4256);item(5,2,4257);marker(255,4258);
 counting_multi_prefetch(4508);counting_multi_prefetch(4509);assert(sends==before+2);
 assert(m->currencies[0].status==MULTI_DETAIL_READY&&m->currencies[1].status==MULTI_DETAIL_READY);
 counting_multi_begin(true);assert(m->currencies[0].status==MULTI_DETAIL_NONE);
 counting_multi_prefetch(4020);assert(sends==before+2);live("CNY",15,3,0);global(0,0,2);
 counting_multi_prefetch(3771);counting_multi_prefetch(4021);assert(sends==before+3);
 counting_multi_reset();counting_multi_prefetch(4022);marker(255,4023);counting_multi_prefetch(4024);
 assert(sends==before+3&&m->count==0);
 counting_multi_begin(false);live("USD",100,1,0);live("CNY",5,1,0);global(0,0,2);
 counting_multi_prefetch(4750);
 fail_send=1;counting_multi_prefetch(5000);fail_send=0;
 assert(m->currencies[0].status==MULTI_DETAIL_INVALID);counting_multi_prefetch(5250);
 assert(!memcmp(last,"\0CNY",4));counting_multi_poll(8000);before=sends;
 counting_multi_prefetch(8001);assert(sends==before&&counting_multi_query_busy());
 marker(255,8002);counting_multi_prefetch(8003);assert(sends==before);
 counting_multi_reset();
 /* Closed inconsistent / empty replies recover without a page or Refresh. */
 counting_multi_begin(false);live("CNY",60,12,0);global(0,0,2);
 before=sends;counting_multi_prefetch(9000);counting_multi_prefetch(9250);
 assert(sends==before+1);marker(0,9251);item(5,1,9252);marker(255,9253);
 assert(m->currencies[0].status==MULTI_DETAIL_INVALID);
 counting_multi_prefetch(9503);assert(sends==before+1);
 counting_multi_prefetch(10253);assert(sends==before+2);
 marker(0,10254);marker(0,10255);item(5,12,10256);marker(255,10257);
 assert(m->currencies[0].status==MULTI_DETAIL_READY);
 counting_multi_prefetch(10507);assert(sends==before+2);
 counting_multi_begin(false);live("CNY",60,12,0);global(0,0,2);
 before=sends;counting_multi_prefetch(10000);
 for(unsigned n=0;n<3;n++) {
   unsigned t=10250+n*n*1100;counting_multi_prefetch(t);
   marker(0,t+1);marker(255,t+2);
 }
 counting_multi_prefetch(16000);assert(sends==before+3);
 assert(m->currencies[0].status==MULTI_DETAIL_EMPTY);
 counting_multi_reset();
 counting_multi_begin(true);live("USD",100,1,0);global(0,0,2);
 uint32_t group=m->group_generation;assert(m->add&&m->passes==1);
 counting_multi_begin(true);assert(m->group_generation==group&&m->passes==2);
 global(0,0,2);counting_multi_begin(false);
 assert(m->group_generation!=group&&m->passes==1&&m->count==0);
 /* Captured CNY reply: live 15/3, then a complete all-zero catalog. */
 counting_multi_begin(false);live("CNY",15,3,1);global(0,0,1);global(0,0,2);
 before=sends;counting_multi_prefetch(20000);counting_multi_prefetch(20250);
 marker(0,20251);item(100,0,20252);item(50,0,20253);item(20,0,20254);
 item(10,0,20255);item(5,0,20256);item(1,0,20257);marker(255,20258);
 assert(m->currencies[0].status==MULTI_DETAIL_INVALID && m->currencies[0].denom_count==0);
 counting_multi_prefetch(20508);assert(sends==before+1);
 counting_multi_prefetch(21258);assert(sends==before+2);
 marker(0,21259);item(5,3,21260);marker(255,21261);
 assert(m->currencies[0].status==MULTI_DETAIL_READY && m->currencies[0].amount==15);
 puts("PASS MULTI protocol, sequential prefetch/cache, ADD closure, clear cancellation, failed send progression, timeout quarantine and late FF");return 0;
}
