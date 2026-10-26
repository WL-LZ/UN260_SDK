/* External UART replay: each received 0B row is an absolute controller value.
 * Captures containing only a final 0B prove final projection, not live timing.
 * No private capture content or 49-derived denomination oracle is stored here. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"

static counting_sim_t data;
static counting_session_state_t session;
static counting_detail_state_t detail;
static denom_t expected[COUNTING_DENOM_MAX_ITEMS];
static unsigned expected_count, run_id, source_line, failures, completed, incomplete;
static unsigned row_checks, live_rows, final_only_runs, serial_completions, clear_count;
static unsigned restart_count, total_frames, stops, ends, current_live_rows;
static bool add, have_stop, have_end, have_table;
static uint32_t now;
uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *fmt, ...) { (void)fmt; }
int protocol_send(uint8_t cmd, const uint8_t *payload, uint16_t len)
{ (void)cmd; assert(payload && len); return 0; }
bool counting_history_discard_pending(counting_session_state_t *state)
{ (void)state; return false; }
void counting_history_current_invalidate(void) {}
static void require(bool condition, const char *message)
{
    if (!condition) { ++failures; fprintf(stderr,"FAIL run=%u line=%u: %s\n",run_id,source_line,message); }
}
static unsigned pcs_for(const denom_t *rows,unsigned count,int value)
{
    for(unsigned i=0;i<count;++i)if(rows[i].value==value)return rows[i].pcs;
    return 0;
}
static void check_table(void)
{
    for(unsigned side=0;side<2;++side) {
        const denom_t *keys=side?data.denom:expected;
        unsigned n=side?data.denom_number:expected_count;
        for(unsigned i=0;i<n;++i) {
            unsigned want=pcs_for(expected,expected_count,keys[i].value);
            unsigned actual=pcs_for(data.denom,data.denom_number,keys[i].value);
            if(want!=actual) {
                fprintf(stderr,"  denom=%d expected=%u got=%u\n",keys[i].value,want,actual);
                require(false,"A differs from received absolute 0B rows");
            }
        }
    }
}
static void history_ready(void *context)
{ (void)context; ++serial_completions; if(have_table)check_table(); }
static void consume_restart(void)
{
    if(counting_report_take_group_restart())++restart_count;
    require(restart_count<=1,"history group reset notified more than once per pass");
}
static void clear_result(void)
{
    counting_report_reset(); counting_data_clear_serials(&data); counting_data_clear_errors(&data);
    denom_t old[COUNTING_DENOM_MAX_ITEMS];unsigned n=data.denom_number;
    memcpy(old,data.denom,sizeof(old));memset(&data,0,sizeof(data));data.denom_number=n;
    for(unsigned i=0;i<n;++i)data.denom[i].value=old[i].value;
    memset(&session,0,sizeof(session));memset(&detail,0,sizeof(detail));
    expected_count=0;have_table=false;++clear_count;
}
static void finish_run(void)
{
    if(!run_id)return;
    if(have_stop&&have_end) {
        ++completed;if(!current_live_rows)++final_only_runs;
        printf("CHECK run=%u ADD=%u rows_before_STOP=%u totals=%d/%.0f A_complete=1\n",
               run_id,add,current_live_rows,data.total_pcs,data.total_amount);
    } else {
        ++incomplete;
        printf("INCOMPLETE run=%u STOP=%u complete0B=%u; received prefixes checked only\n",run_id,have_stop,have_end);
    }
}
static void receive(uint8_t *b,unsigned n)
{
    ++now;++total_frames;
    if(b[3]==0x3B&&n==6&&b[4]==1){clear_result();return;}
    if(!run_id)return;
    if(b[3]==0x0A) {
        if(n==7&&b[4]==1&&b[5]==1) {
            counting_report_begin(add,&data);counting_denom_reply_begin_count(&detail);
            detail.live_reject_started=false;
        }
        counting_control_reply_dispatch(b[3],&session,b,n,NULL);return;
    }
    denom_t before[COUNTING_DENOM_MAX_ITEMS];memcpy(before,data.denom,sizeof(before));
    unsigned before_count=data.denom_number;
    if(b[3]==0x0E&&n==13) {
        int before_pcs=data.total_pcs;double before_amount=data.total_amount;
        unsigned wire_pcs=((unsigned)b[8]<<8)|b[9];
        uint32_t wire_amount=((uint32_t)b[4]<<24)|((uint32_t)b[5]<<16)|((uint32_t)b[6]<<8)|b[7];
        counting_info_reply_result_t result=counting_info_reply_handle(&session,&data,b,n,0);
        if(result.kind==COUNTING_INFO_REPLY_LIVE) {
            counting_report_observe_total(&data,wire_amount,wire_pcs);consume_restart();
        } else if(result.kind==COUNTING_INFO_REPLY_FINISHED) {
            have_stop=true;++stops;
            require(result.final_pcs==(wire_pcs?(int)wire_pcs:before_pcs),"STOP quantity is not its 0E source");
            require(result.final_amount==(wire_pcs?(double)wire_amount:before_amount),"STOP amount is not its 0E source");
            counting_report_schedule(&session,now);
        }
        require(before_count==data.denom_number&&!memcmp(before,data.denom,sizeof(before)),"0E wrote A rows");
        return;
    }
    if(b[3]==0x0B&&n==16) {
        int before_pcs=data.total_pcs;double before_amount=data.total_amount;
        counting_denom_reply_result_t result=counting_denom_reply_handle(&detail,&session,&data,b,n,NULL);
        require(data.total_pcs==before_pcs&&data.total_amount==before_amount,"0B changed 0E totals");
        if(result==COUNTING_DENOM_REPLY_START) {
            expected_count=0;have_table=true;
            require(data.denom_number==before_count,"START discarded denomination catalogue");
            for(unsigned i=0;i<before_count;++i)
                require(data.denom[i].value==before[i].value,"START changed a denomination value");
            check_table();
        } else if(result==COUNTING_DENOM_REPLY_DATA&&have_table) {
            char denomination[9]={0},quantity[4]={0};
            memcpy(denomination,b+4,8);memcpy(quantity,b+12,3);
            unsigned value=(unsigned)strtoul(denomination,NULL,10),pcs=(unsigned)strtoul(quantity,NULL,10);
            unsigned slot=0;while(slot<expected_count&&expected[slot].value!=(int)value)++slot;
            assert(slot<COUNTING_DENOM_MAX_ITEMS);
            expected[slot]=(denom_t){.value=(int)value,.pcs=(uint16_t)pcs};
            if(slot==expected_count)++expected_count;
            ++row_checks;if(!have_stop){++live_rows;++current_live_rows;}
            check_table();
        } else if(result==COUNTING_DENOM_REPLY_SESSION_END) {
            have_end=true;++ends;check_table();
        }
        return;
    }
    if(b[3]==0x49||b[3]==0x0C||b[3]==0x0D) {
        counting_reject_sn_reply_hooks_t hooks={.on_history_record_ready=history_ready};
        counting_reject_sn_reply_dispatch(b[3],&detail,&session,&data,b,n,&hooks);
        require(before_count==data.denom_number&&!memcmp(before,data.denom,sizeof(before)),"49/0C/0D changed A rows");
        consume_restart();
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);FILE *input=fopen(argv[1],"r");assert(input);char line[2048];
    while(fgets(line,sizeof(line),input)) {
        if(line[0]=='R') {
            finish_run();unsigned enabled;
            assert(sscanf(line,"R %u %u %u",&run_id,&enabled,&source_line)==3);add=enabled!=0;
            restart_count=current_live_rows=0;have_stop=have_end=have_table=false;expected_count=0;
        } else if(line[0]=='F') {
            int used;assert(sscanf(line,"F %u %n",&source_line,&used)==1);
            char *p=line+used;uint8_t b[256];unsigned n=0,value;
            while(sscanf(p," %x%n",&value,&used)==1){assert(n<sizeof(b)&&value<=255);b[n++]=value;p+=used;}
            assert(n>=5&&n==b[2]);receive(b,n);
        }
    }
    finish_run();fclose(input);
    counting_report_shutdown();counting_data_clear_serials(&data);counting_data_clear_errors(&data);
    require(completed>0,"capture has no complete START/STOP/0B run");
    printf("%s controller A capture: complete=%u incomplete=%u rows=%u rows_before_STOP=%u final_only_runs=%u clear=%u STOP=%u END=%u history_ready=%u frames=%u failures=%u\n",
           failures?"FAIL":"PASS",completed,incomplete,row_checks,live_rows,final_only_runs,clear_count,stops,ends,serial_completions,total_frames,failures);
    return failures?1:0;
}
