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
static unsigned reject_publications, reject_analyses;
static uint32_t now;
static bool send_failed;
uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t cmd, const uint8_t *p, uint16_t n)
{ assert(p && n); ++requests[cmd]; return send_failed ? -1 : 0; }
static void ready(void *p) { (void)p; ++publications; }
static void complete(void *p) { (void)p; ++completions; }
static void reject_ready(void *p) { (void)p; ++reject_publications; }
static void reject_analysis(void *p) { (void)p; ++reject_analyses; }
static counting_detail_reply_result_t frame(uint8_t *b, unsigned n)
{
    now += 1;
    counting_reject_sn_reply_hooks_t h = {.on_serial_report_ready=ready,.on_detail_complete=complete,
        .on_reject_report_changed=reject_ready,.on_reject_analysis_ready=reject_analysis};
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
    memset(requests,0,sizeof(requests));publications=completions=0;
    reject_publications=reject_analyses=0;now=100;send_failed=false;
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

static counting_detail_reply_result_t reject_row(unsigned code, unsigned pcs)
{
    uint8_t b[7]={0xFD,0xDF,7,0x0C,(uint8_t)code,(uint8_t)pcs,0};
    return frame(b,sizeof(b));
}
static unsigned reject_pcs(unsigned code)
{
    for (unsigned i=0;i<data.err_num;++i)
        if(data.err_code[i]==code)return data.err_pcs[i];
    return 0;
}
static void live_reject_snapshot(unsigned code,unsigned pcs)
{
    mark(0x0C,0);reject_row(code,pcs);mark(0x0C,255);
}
static void test_live_reject_atomic(void)
{
    reset();start(false);data.err_expected=1;
    /* No START marker or confirmed count: no model, notifications or query. */
    reject_row(0x1C,1);mark(0x0C,255);
    assert(!data.err_num&&!reject_publications&&!requests[0x0D]);
    mark(0x0C,0);reject_row(0x1C,1);
    assert(!data.err_num&&!reject_publications);
    mark(0x0C,255);
    assert(data.err_num==1&&reject_pcs(0x1C)==1&&reject_publications==1);
    assert(!requests[0x0C]&&!requests[0x0D]&&!publications&&!completions&&!reject_analyses);
    /* Exact user capture: complete snapshots are 1 -> 2 -> 3, never 1+2+3. */
    data.err_expected=2;mark(0x0C,0);reject_row(0x1C,2);
    assert(data.err_num==1&&reject_pcs(0x1C)==1&&reject_publications==1);
    mark(0x0C,255);assert(data.err_num==1&&reject_pcs(0x1C)==2&&reject_publications==2);
    data.err_expected=3;live_reject_snapshot(0x1C,3);
    assert(data.err_num==1&&reject_pcs(0x1C)==3&&reject_publications==3);
    unsigned sent=reject_publications;mark(0x0C,255);mark(0x0C,255);
    assert(reject_publications==sent&&!requests[0x0D]);
    /* A complete update may contain multiple reasons and repeated code rows.
     * The last count for a reason replaces it; publication is still atomic. */
    data.err_expected=4;mark(0x0C,0);reject_row(0x1C,2);reject_row(0x15,1);reject_row(0x1C,3);
    assert(data.err_num==1&&reject_pcs(0x1C)==3&&!reject_pcs(0x15));
    mark(0x0C,255);
    assert(data.err_num==2&&reject_pcs(0x1C)==3&&reject_pcs(0x15)==1);
    assert(counting_data_reject_pcs_count(&data)==4);
    data.err_expected=0;mark(0x0C,0);
    assert(data.err_num==2);mark(0x0C,255);assert(!data.err_num);
    assert(!requests[0x0C]&&!requests[0x0D]&&!publications&&!completions&&!reject_analyses);
    puts("PASS live reject: captured 1C 1/2/3, atomic multi-reason snapshots, duplicate code/end, empty snapshot, no final query or analysis side effects");
}
static void test_live_reject_add_and_owner(void)
{
    reset();
    mark(0x0C,0);reject_row(0x1C,7);mark(0x0C,255);
    assert(!data.err_num&&!reject_publications);
    start(false);data.err_expected=2;live_reject_snapshot(0x1C,2);
    unsigned before=reject_publications;
    start(true);
    assert(data.err_num==1&&reject_pcs(0x1C)==2&&data.err_expected==2);
    /* ADD preserves the last complete result while awaiting a cumulative
     * controller snapshot. New snapshots replace totals, never add them. */
    data.err_expected=3;mark(0x0C,0);reject_row(0x1C,3);
    assert(reject_pcs(0x1C)==2);mark(0x0C,255);
    assert(data.err_num==1&&reject_pcs(0x1C)==3&&reject_publications==before+1);
    start(false);
    assert(!data.err_num&&!data.err_expected);
    data.err_expected=1;live_reject_snapshot(0x15,1);
    assert(data.err_num==1&&reject_pcs(0x15)==1&&!reject_pcs(0x1C));
    /* Old partial snapshot must not attach to a new START generation. */
    mark(0x0C,0);reject_row(0x1C,9);before=reject_publications;
    start(false);data.err_expected=1;reject_row(0x15,8);mark(0x0C,255);
    assert(!data.err_num&&reject_publications==before);
    live_reject_snapshot(0x1C,1);assert(data.err_num==1&&reject_pcs(0x1C)==1);
    /* A failed/unconfirmed start cannot supply live detail ownership. */
    session.start_confirmed=false;session.phase=COUNTING_SESSION_IDLE;
    before=reject_publications;live_reject_snapshot(0x1C,9);
    assert(reject_pcs(0x1C)==1&&reject_publications==before&&!requests[0x0D]);
    counting_report_reset();session.start_confirmed=true;session.phase=COUNTING_SESSION_ACTIVE;
    live_reject_snapshot(0x1C,9);assert(reject_publications==before);
    puts("PASS live reject ADD/owner: ADD retains and replaces cumulative totals, non-ADD resets, unconfirmed/reset/orphan/previous-generation traffic ignored");
}
static void test_live_reject_final_and_drain(void)
{
    reset();start(false);data.err_expected=2;live_reject_snapshot(0x1C,2);
    stop();assert(requests[0x0C]==1&&!requests[0x0D]);
    mark(0x0C,0);reject_row(0x1C,2);mark(0x0C,255);
    assert(requests[0x0D]==1&&!session.history_record.end_seen);
    mark(0x0C,255);assert(requests[0x0D]==1);
    mark(0x0D,0);row(1,false);mark(0x0D,255);
    assert(publications==1&&completions==1&&session.history_record.end_seen);
    unsigned before=reject_publications;live_reject_snapshot(0x1C,9);
    assert(reject_pcs(0x1C)==2&&reject_publications==before&&requests[0x0D]==1);
    /* Untagged old query replies remain drain-owned through their END.
     * They must not update the current live model or schedule its serials. */
    reset();start(false);stop();mark(0x0C,0);
    start(true);data.err_expected=1;before=reject_publications;
    reject_row(0x1C,9);mark(0x0C,255);
    assert(!data.err_num&&reject_publications==before&&!requests[0x0D]);
    live_reject_snapshot(0x1C,1);
    assert(data.err_num==1&&reject_pcs(0x1C)==1&&!requests[0x0D]);
    stop();assert(requests[0x0C]==2);mark(0x0C,0);reject_row(0x1C,1);mark(0x0C,255);
    assert(requests[0x0D]==1);mark(0x0D,0);mark(0x0D,255);
    assert(completions==1&&session.history_record.end_seen);
    /* A serial query from the previous pass has a different command and
     * must drain without overwriting freshly accepted current rejects. */
    reset();start(false);request_serial();mark(0x0D,0);row(1,false);
    start(true);data.err_expected=2;live_reject_snapshot(0x1C,2);
    assert(!data.err_num); /* One untagged transaction is still drain-owned. */
    row(2,false);mark(0x0D,255);
    assert(!data.err_num&&!publications&&!completions);
    live_reject_snapshot(0x1C,2);assert(reject_pcs(0x1C)==2);
    puts("PASS live/final ownership: one final 0C->0D query, only final serial completes history, completed/orphan and stale reject/serial query tails cannot mutate current result");
}


static void test_live_reject_stop_timeout(void)
{
    reset();start(false);data.err_expected=1;live_reject_snapshot(0x1C,1);
    mark(0x0C,0);reject_row(0x1C,2);data.err_expected=2;
    unsigned before=reject_publications;
    stop();assert(!requests[0x0C]&&!requests[0x0D]);
    /* STOP preserves the in-flight live snapshot's owner and queues final
     * reconciliation. Its END publishes once, then enables the final query. */
    mark(0x0C,255);
    assert(reject_pcs(0x1C)==2&&reject_publications==before+1);
    assert(requests[0x0C]==1&&!requests[0x0D]&&!reject_analyses);
    before=reject_publications;mark(0x0C,255);mark(0x0C,255);
    assert(requests[0x0C]==1&&!requests[0x0D]&&!reject_analyses&&reject_publications==before);
    mark(0x0C,0);reject_row(0x1C,2);mark(0x0C,255);
    assert(requests[0x0D]==1&&reject_analyses==1);
    mark(0x0D,0);row(1,false);mark(0x0D,255);
    assert(completions==1&&session.history_record.end_seen);

    reset();start(false);data.err_expected=1;live_reject_snapshot(0x1C,1);
    mark(0x0C,0);reject_row(0x1C,9);before=reject_publications;
    now+=3001;counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    assert(reject_pcs(0x1C)==1&&reject_publications==before);
    reject_row(0x15,8);mark(0x0C,255);
    assert(reject_pcs(0x1C)==1&&!reject_pcs(0x15)&&reject_publications==before);
    assert(!requests[0x0C]&&!requests[0x0D]);
    live_reject_snapshot(0x1C,2);assert(reject_pcs(0x1C)==2);

    reset();start(false);data.err_expected=1;live_reject_snapshot(0x1C,1);
    mark(0x0C,0);reject_row(0x1C,9);stop();
    now+=3001;counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT&&!requests[0x0C]);
    mark(0x0C,255);counting_report_poll(&session,++now);
    assert(reject_pcs(0x1C)==1&&requests[0x0C]==1&&!requests[0x0D]);
    mark(0x0C,0);reject_row(0x1C,1);mark(0x0C,255);
    assert(requests[0x0D]==1);mark(0x0D,0);mark(0x0D,255);
    assert(completions==1&&!counting_report_take_failure());
    puts("PASS live STOP/timeout: partial snapshot finishes under its owner before final query; timeout preserves previous result, stale END drains, then final reconciliation resumes");
}


static void test_live_reject_duplicate_deadline(void)
{
    reset();start(false);
    /* Detail can precede the first 0E totals in the same count. */
    live_reject_snapshot(0x1C,1);
    assert(data.err_num==1&&reject_pcs(0x1C)==1&&counting_data_reject_pcs_count(&data)==1);
    mark(0x0C,0);reject_row(0x1C,2);
    unsigned before=reject_publications;
    now+=2000;reject_row(0x1C,2);now+=1100;counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    mark(0x0C,255);
    assert(reject_pcs(0x1C)==1&&reject_publications==before);
    /* A new complete frame restores freshness after the timed-out tail drains. */
    live_reject_snapshot(0x1C,3);assert(reject_pcs(0x1C)==3);
    mark(0x0C,0);now+=2000;mark(0x0C,0);now+=1100;
    counting_report_poll(&session,now);
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_REJECT);
    mark(0x0C,255);assert(reject_pcs(0x1C)==3);
    puts("PASS live progress freshness: early detail before 0E, identical data and repeated empty START do not extend a dead snapshot deadline");
}


static void test_live_reject_serial_capacity(void)
{
    reset();assert(counting_data_ensure_serial_capacity(&data,COUNTING_DATA_MAX_ITEMS));
    data.sn_str[COUNTING_DATA_MAX_ITEMS-1]=malloc(5);assert(data.sn_str[COUNTING_DATA_MAX_ITEMS-1]);
    strcpy(data.sn_str[COUNTING_DATA_MAX_ITEMS-1],"LAST");
    data.denom_mix[COUNTING_DATA_MAX_ITEMS-1]=20;
    start(true);data.err_expected=1;live_reject_snapshot(0x1C,1);
    live();assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_CAPACITY);
    live_reject_snapshot(0x1C,2);
    assert(reject_pcs(0x1C)==2&&!counting_report_take_failure());
    /* A capacity failure while collecting C invalidates only that incomplete
     * snapshot. Drain its END, retain prior C and recover next full snapshot. */
    start(true);mark(0x0C,0);reject_row(0x1C,3);
    unsigned before=reject_publications;live();
    assert(counting_report_take_failure()==COUNTING_REPORT_FAILURE_CAPACITY);
    mark(0x0C,255);assert(reject_pcs(0x1C)==2&&reject_publications==before);
    live_reject_snapshot(0x1C,3);
    assert(reject_pcs(0x1C)==3&&!counting_report_take_failure()&&!requests[0x0D]);
    puts("PASS live C recovery around B capacity: capacity notice stays one-shot, partial C fails safely, next full C snapshot recovers after END drain");
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
    test_wrap_atomic();test_empty_add();test_interleave();test_failure();test_capacity();test_empty_reject_compatibility();
    test_live_reject_atomic();test_live_reject_add_and_owner();test_live_reject_final_and_drain();test_live_reject_stop_timeout();test_live_reject_duplicate_deadline();test_live_reject_serial_capacity();reset();
    if(argc==2)captured_reports(argv[1]);
    counting_report_shutdown();
    puts("PASS reports: 725/10000 rows, byte wrap incl 00/FF, original holes, duplicate frames/text, atomic publication, empty ADD reuse, interleaved runs, timeout/drain, overflow, failed send");
}

void counting_history_current_invalidate(void){}
