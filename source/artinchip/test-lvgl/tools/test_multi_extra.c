#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_multi_extra.h"
static unsigned sends;
static unsigned command;
static unsigned bytes;
static unsigned char payload[4];
int protocol_send(unsigned char cmd,const unsigned char *data,unsigned short len)
{sends++;command=cmd;bytes=len;memcpy(payload,data,len);return 0;}
static void finished(void)
{
    unsigned char f[16]={0xfd,0xdf,16,14,'C','N','Y',0,0,0,10,0,2,4,1};
    counting_multi_begin(false);assert(counting_multi_info(f,16));
    unsigned char end[13]={0xfd,0xdf,13,14,0,0,0,0,0,0,0,2};
    assert(counting_multi_info(end,13));
}
static void reply(unsigned char *f,unsigned len,unsigned now)
{assert(counting_multi_extra_reply(f[3],f,len,now));}
int main(void)
{
    finished();assert(!sends);
    assert(counting_multi_serial_request(0,1));
    assert(command==13&&bytes==4&&!payload[0]&&!memcmp(payload+1,"CNY",3));
    unsigned char f[25]={0xfd,0xdf,25,13};reply(f,25,2);
    f[4]=84;memcpy(f+5,"      0ABCD45678901",19);reply(f,25,3);reply(f,25,4);
    f[4]=4;memcpy(f+5,"      5EFGH12345678",19);reply(f,25,5);
    assert(counting_multi_serials(0)->status==MULTI_DETAIL_LOADING);
    memset(f+4,255,20);reply(f,25,6);
    const multi_serial_cache_t *s=counting_multi_serials(0);
    assert(s->status==MULTI_DETAIL_READY&&s->count==2);
    assert(s->rows[0].number==4&&s->rows[1].number==84&&s->rows[1].value==0);
    assert(!strcmp(s->rows[1].text,"ABCD45678901"));
    assert(counting_multi_reject_request(7));assert(command==12&&bytes==1&&payload[0]==1);
    unsigned char r[7]={0xfd,0xdf,7,12};reply(r,7,8);r[4]=28;r[5]=4;reply(r,7,9);
    r[4]=r[5]=255;reply(r,7,10);
    assert(counting_multi_rejects()->count==1&&counting_multi_rejects()->rows[0].pcs==4);
    assert(counting_multi_serial_request(0,11));
    counting_multi_extra_poll(2512);assert(counting_multi_serials(0)->status==MULTI_DETAIL_TIMEOUT);
    assert(!counting_multi_reject_request(2513));
    finished();assert(counting_multi_serials(0)->status==MULTI_DETAIL_NONE);
    reply(f,25,2514);assert(!counting_multi_extra_busy());
    assert(counting_multi_serials(0)->status==MULTI_DETAIL_NONE);
    assert(counting_multi_serial_request(0,2515));
    memset(f+4,0,20);reply(f,25,2516);f[4]=1;memcpy(f+5,"      5EFGH12345678",19);reply(f,25,2517);
    f[12]='X';reply(f,25,2518);memset(f+4,255,20);reply(f,25,2519);
    assert(counting_multi_serials(0)->status==MULTI_DETAIL_INVALID);
    counting_multi_reset();counting_multi_extra_revision();
    puts("PASS: on-demand serial/reject ownership, original NO, unknown denomination, FF publication, timeout/reset quarantine");
}
