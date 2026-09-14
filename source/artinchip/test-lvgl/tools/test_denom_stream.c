#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "un260/counting/counting_denom_query_service.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/protocol/protocol_send.h"

static uint32_t now;
static unsigned sends, rejects;
uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *fmt, ...) { (void)fmt; }
int protocol_send(uint8_t cmd, const uint8_t *data, uint16_t len)
{ assert(data && len == 1); if (cmd == 0x0B) sends++; else if (cmd == 0x0C) rejects++; return 0; }
static counting_detail_state_t d;
static counting_session_state_t s;
static counting_sim_t sim;
static void marker(unsigned char fill)
{
    uint8_t b[16] = {0xFD,0xDF,16,0x0B};
    memset(b+4,fill,11);
    counting_denom_reply_handle(&d,&s,&sim,b,sizeof(b),NULL);
}
static void row(int denom)
{
    uint8_t b[16] = {0xFD,0xDF,16,0x0B}; char text[12];
    snprintf(text,sizeof(text),"%08d%03d",denom,0); memcpy(b+4,text,11);
    counting_denom_reply_handle(&d,&s,&sim,b,sizeof(b),NULL);
}
static void reset(void)
{ memset(&d,0,sizeof(d));memset(&s,0,sizeof(s));memset(&sim,0,sizeof(sim));now=100;sends=rejects=0; }
static void poll(void) {counting_denom_query_poll(&d,now,true,true);}
int main(int argc, char **argv)
{
    if (argc == 2) {
        FILE *log = fopen(argv[1], "r"); assert(log);
        char line[1024]; unsigned frames = 0;
        reset(); counting_denom_query_trigger(&d,now,true);
        while (fgets(line,sizeof(line),log)) {
            char *p = strstr(line,"RX[16]: FD DF 10 0B");
            if (!p) continue;
            p = strchr(p,':')+1; uint8_t b[16];
            for(int i=0;i<16;i++){unsigned value;int n;
                assert(sscanf(p," %x%n",&value,&n)==1);b[i]=(uint8_t)value;p+=n;}
            now+=25;counting_denom_reply_handle(&d,&s,&sim,b,sizeof(b),NULL);poll();frames++;
        }
        fclose(log);assert(frames==194 && sends==1 && rejects==0);
        assert(d.query_failed && !d.query_complete && sim.denom_number==0);
        puts("PASS captured log: 194 frames, one request, oversized list rejected atomically");return 0;
    }
    reset(); counting_denom_query_expect_push(&d,now,true);
    assert(sends==0 && d.query_pending);marker(0);
    for(int i=0;i<200;i++){now+=25;row(100);poll();}
    marker(255);assert(sends==0 && d.query_complete && rejects==0);
    now+=5000;poll();assert(sends==0);

    reset(); counting_denom_query_expect_push(&d,now,true);
    now+=1499;poll();assert(sends==0);
    now++;poll();assert(sends==1 && !d.query_wait_push);
    marker(0);row(100);marker(255);assert(d.query_complete && rejects==0);

    reset();counting_denom_query_expect_push(&d,now,false);
    now+=10000;counting_denom_query_poll(&d,now,false,false);assert(sends==0);
    poll();assert(sends==0);now+=1499;poll();assert(sends==0);
    now++;poll();assert(sends==1);

    reset();counting_denom_query_expect_push(&d,now,false);
    marker(0);row(100);marker(255);now+=10000;poll();
    assert(sends==0 && d.query_complete);

    reset(); counting_denom_query_trigger(&d,now,true); marker(0);
    for(int i=0;i<400;i++){now+=25;row(100);poll();}
    assert(sends==1 && !d.query_expired);marker(255);
    assert(d.query_complete && sim.denom_number==1 && rejects==0);
    now+=10000;poll();assert(sends==1);

    reset();counting_denom_query_trigger(&d,now,true);marker(0);
    now+=1600;poll();assert(d.query_expired && sends==1);
    row(10);marker(255);now+=10000;poll();
    assert(!d.query_complete && sends==1 && rejects==0);

    reset();counting_denom_query_trigger(&d,now,true);
    for(int i=0;i<30;i++){now+=1600;poll();}
    assert(sends==3 && d.query_failed);marker(0);row(100);marker(255);
    assert(rejects==0 && !d.query_complete);

    reset();counting_denom_query_trigger(&d,now,true);marker(0);
    for(int i=1;i<=16;i++){now+=25;row(i);poll();}marker(255);
    assert(d.query_failed && sim.denom_number==0 && sends==1);

    reset();counting_denom_query_trigger(&d,now,true);marker(0);
    for(int i=0;i<601;i++){now+=100;row(100);poll();}
    assert(d.query_expired && sends==1);
    s.phase=COUNTING_SESSION_ACTIVE;marker(0);row(10);marker(255);
    assert(sim.denom_number==1 && rejects==1);

    reset();now=UINT32_MAX-1000;counting_denom_query_trigger(&d,now,true);marker(0);
    for(int i=0;i<100;i++){now+=50;row(100);poll();}marker(255);
    assert(d.query_complete && sends==1);
    puts("PASS: long stream, idle gap, bounded retries, overflow, hard deadline, counting recovery, clock wrap");
}
