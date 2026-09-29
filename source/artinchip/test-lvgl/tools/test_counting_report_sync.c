#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_data_store.h"

static counting_sim_t data;
static counting_session_state_t session;
static counting_detail_state_t detail;
static unsigned requests[256], publications, completions;
static uint32_t now;
static bool send_failed;
uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t cmd, const uint8_t *p, uint16_t n)
{ assert(p && n); ++requests[cmd]; return send_failed ? -1 : 0; }
static void ready(void *p) { (void)p; ++publications; }
static void complete(void *p) { (void)p; ++completions; }
static counting_detail_reply_result_t frame(uint8_t *b, unsigned n)
{
    now += 1;
    counting_reject_sn_reply_hooks_t h = {.on_serial_report_ready=ready,.on_detail_complete=complete};
    return counting_reject_sn_reply_dispatch(b[3], &detail, &session, &data, b, n, &h);
}
static void mark(unsigned cmd, unsigned fill)
{
    unsigned n = cmd == 0x0D ? 25 : 7;
    uint8_t b[25] = {0xFD,0xDF,0,0}; b[2]=n;b[3]=cmd;memset(b+4,fill,n-5);frame(b,n);
}
static void row(unsigned n, bool empty)
{
    uint8_t b[25]={0xFD,0xDF,25,0x0D}; char text[20];
    b[4]=(uint8_t)n;snprintf(text,sizeof(text),"%7d%-12s",20,empty?"":"SAME12345678");
    memcpy(b+5,text,19);frame(b,25);
}
static void start(bool add)
{
    counting_report_begin(add,&data);session.start_confirmed=true;session.phase=COUNTING_SESSION_ACTIVE;
}
static void stop(void)
{
    session.start_confirmed=false;session.phase=COUNTING_SESSION_FINISHED_WAIT_START;
    session.history_record.valid=true;session.history_record.end_seen=false;
    counting_report_schedule(&session,now);
}
static void request_serial(void)
{
    stop();mark(0x0C,0);mark(0x0C,255);
}
static void live(void)
{
    uint8_t b[24]={0xFD,0xDF,24,0x49};char text[20];
    snprintf(text,sizeof(text),"%7d%-12s",50,"NEW123456789");memcpy(b+4,text,19);frame(b,24);
}
static void reset(void)
{
    counting_report_shutdown();counting_data_clear_serials(&data);counting_data_clear_errors(&data);
    memset(&data,0,sizeof(data));memset(&session,0,sizeof(session));memset(&detail,0,sizeof(detail));
    memset(requests,0,sizeof(requests));publications=completions=0;now=100;send_failed=false;
    data.total_pcs=725;data.total_amount=14500;data.denom_number=1;data.denom[0]=(denom_t){20,725,14500};
}
static void test_wrap_atomic(void)
{
    reset();start(true);request_serial();mark(0x0D,0);
    for(unsigned n=1;n<=725;++n)row(n,n==300);
    assert(!data.sn_str && !publications);
    mark(0x0D,255);assert(publications==1 && completions==1);
    assert(counting_data_serial_valid_count(&data)==724);
    assert(data.sn_str[254] && data.sn_str[255] && data.sn_str[511] && data.sn_str[724]);
    assert(data.sn_str[299]==NULL);
    /* Text duplicates are separate notes. Wire duplicates are idempotent. */
    ++data.total_pcs;start(true);request_serial();mark(0x0D,0);row(1,false);row(1,false);row(2,false);
    assert(counting_data_serial_valid_count(&data)==724);
    mark(0x0D,255);assert(counting_data_serial_valid_count(&data)==2 && publications==2);
}
static void test_empty_add(void)
{
    reset();start(true);request_serial();mark(0x0D,0);row(1,false);mark(0x0D,255);
    char **before=data.sn_str;
    start(true);request_serial();assert(requests[0x0C]==2 && requests[0x0D]==1);
    assert(!session.history_record.valid && data.sn_str==before && publications==1);
    start(true);live();request_serial();assert(requests[0x0D]==2);
    mark(0x0D,0);row(1,false);row(2,false);mark(0x0D,255);
    start(false);request_serial();assert(requests[0x0D]==3);
}
static void test_interleave(void)
{
    reset();start(false);request_serial();mark(0x0D,0);row(1,false);
    start(false);live();assert(!strcmp(data.sn_str[0],"NEW123456789"));
    row(2,false);row(3,false);mark(0x0D,255);
    assert(!strcmp(data.sn_str[0],"NEW123456789") && publications==0 && completions==0);
    assert(session.start_confirmed && session.phase==COUNTING_SESSION_ACTIVE);
    /* Even when the next run stops before the old stream ends, do not issue
     * a competing untagged query. Drain old traffic, then request current. */
    request_serial();mark(0x0D,0);row(1,false);start(true);live();stop();
    unsigned sent=requests[0x0C];row(2,false);mark(0x0D,255);
    counting_report_poll(&session,++now);assert(requests[0x0C]==sent+1);
    mark(0x0C,0);mark(0x0C,255);mark(0x0D,0);row(1,false);mark(0x0D,255);
    assert(publications==1);
    start(true);stop();mark(0x0C,0);start(false);live();mark(0x0C,255);
    assert(!strcmp(data.sn_str[0],"NEW123456789"));
}
static void test_failure(void)
{
    reset();start(true);request_serial();mark(0x0D,0);row(1,false);mark(0x0D,255);
    char **before=data.sn_str;
    ++data.total_pcs;start(true);request_serial();mark(0x0D,0);row(1,false);row(3,false);mark(0x0D,255);
    assert(data.sn_str==before && publications==1 && counting_report_take_failure());
    start(true);request_serial();mark(0x0D,0);row(1,false);
    now+=3001;counting_report_poll(&session,now);
    assert(counting_report_take_failure() && data.sn_str==before);
    start(false);live();stop();unsigned sent=requests[0x0C];
    now+=10000;counting_report_poll(&session,now);assert(requests[0x0C]==sent);
    mark(0x0D,255);counting_report_poll(&session,now);assert(requests[0x0C]==sent+1);
    counting_report_reset();mark(0x0C,255);
    start(false);send_failed=true;stop();assert(counting_report_take_failure());
    assert(!session.history_record.end_seen);
}
static void test_capacity(void)
{
    reset();start(false);request_serial();mark(0x0D,0);
    for(unsigned i=1;i<=10000;++i)row(i,false);
    mark(0x0D,255);assert(counting_data_serial_valid_count(&data)==10000);
    char **before=data.sn_str;++data.total_pcs;start(true);request_serial();mark(0x0D,0);
    for(unsigned i=1;i<=10001;++i)row(i,false);
    mark(0x0D,255);assert(data.sn_str==before && counting_report_take_failure());
    start(true);live();assert(counting_report_take_failure());
    live();assert(!counting_report_take_failure()); /* one capacity notice */
    start(false);assert(counting_data_serial_valid_count(&data)==0);
    assert(!data.sn_str); /* no prior serials in the new non-ADD history */
}
static void test_empty_reject_compatibility(void)
{
    /* Actual issue 001 capture: START / no-error zero row / END. */
    uint8_t empty[]={0xFD,0xDF,0x07,0x0C,0x00,0x00,0x46};
    uint8_t end[]={0xFD,0xDF,0x07,0x0C,0xFF,0xFF,0x6B};
    uint8_t reject[]={0xFD,0xDF,0x07,0x0C,0x14,0x01,0};
    reset();start(false);stop();assert(requests[0x0C]==1);
    assert(frame(empty,7)==COUNTING_DETAIL_REPLY_START);
    assert(frame(empty,7)==COUNTING_DETAIL_REPLY_IGNORED);
    assert(!requests[0x0D]&&!counting_report_take_failure());
    assert(frame(end,7)==COUNTING_DETAIL_REPLY_END);
    assert(requests[0x0D]==1&&!session.history_record.end_seen);
    frame(end,7);assert(requests[0x0D]==1); /* duplicate END cannot requery */
    mark(0x0D,0);row(1,false);mark(0x0D,255);
    assert(publications==1&&completions==1&&session.history_record.end_seen);
    assert(!counting_report_take_failure());
    /* A genuinely empty serial list remains valid. */
    start(false);stop();frame(empty,7);frame(empty,7);frame(end,7);
    mark(0x0D,0);mark(0x0D,255);assert(!counting_data_serial_valid_count(&data));

    /* Repeated zeros must not keep a dead transaction alive. */
    reset();start(true);stop();frame(empty,7);
    now+=2000;frame(empty,7);now+=1100;counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    frame(end,7);assert(!requests[0x0D]&&!completions);
    /* A fresh run recovers once the failed transaction has drained. */
    start(false);request_serial();assert(requests[0x0D]==1);
    mark(0x0D,0);row(1,false);mark(0x0D,255);
    char **before=data.sn_str;
    ++data.total_pcs;start(true);data.err_expected=1;stop();
    frame(empty,7);frame(empty,7);frame(end,7);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    assert(data.sn_str==before&&requests[0x0D]==1&&!session.history_record.end_seen);
    start(true);stop();frame(empty,7);frame(end,7);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT); /* missing row */
    start(true);stop();frame(empty,7);frame(reject,7);frame(empty,7);frame(end,7);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT); /* restart after data */
    assert(data.sn_str==before&&requests[0x0D]==1);
    data.err_expected=0;start(true);request_serial();mark(0x0D,0);
    now+=3001;counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_SERIAL);
    assert(!counting_report_take_failure());
    reset();start(false);send_failed=true;stop();
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    reset();start(false);stop();mark(0x0C,0);send_failed=true;mark(0x0C,255);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_SERIAL);
    puts("PASS issue 001: captured zero/zero/end, single serial request, real completion, bounded duplicate timeout, drain/recovery, missing and restarted reject list, phase-specific failures");
}
static void captured_reports(const char *path)
{
    FILE *input=fopen(path,"rb");assert(input);
    unsigned reports=0, complete_reports=0, rows=0, valid=0;int next;bool contiguous=true;
    while((next=fgetc(input))!=EOF) {
        unsigned n=(unsigned)next;
        uint8_t b[25];assert(n==25 && fread(b,1,n,input)==n);
        bool first=true,last=true;
        for(unsigned i=4;i<24;++i){first&=b[i]==0;last&=b[i]==255;}
        if(first){reset();start(false);request_serial();rows=valid=0;contiguous=true;}
        else if(!last) {
            ++rows;contiguous&=b[4]==(uint8_t)rows;
            bool has_text=false;
            for(unsigned i=12;i<24;++i)has_text|=b[i]!=' ' && b[i]!=0;
            char denom[8];memcpy(denom,b+5,7);denom[7]=0;
            if(has_text && atoi(denom)>0)++valid;
        }
        frame(b,n);
        if(last){
            assert(publications==(unsigned)contiguous);
            if(contiguous){assert(counting_data_serial_valid_count(&data)==(int)valid);++complete_reports;}
            printf("PASS captured report: %u wire rows, %u serial records, %s\n",rows,valid,
                contiguous?"original sequence retained":"incomplete capture rejected atomically");++reports;}
    }
    fclose(input);assert(reports>=2 && complete_reports>=2);reset();
}
int main(int argc,char **argv)
{
    test_wrap_atomic();test_empty_add();test_interleave();test_failure();test_capacity();test_empty_reject_compatibility();reset();
    if(argc==2)captured_reports(argv[1]);
    counting_report_shutdown();
    puts("PASS reports: 725/10000 rows, byte wrap incl 00/FF, original holes, duplicate frames/text, atomic publication, empty ADD reuse, interleaved runs, timeout/drain, overflow, failed send");
}
