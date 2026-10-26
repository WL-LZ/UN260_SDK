/* Controller-authoritative 0B tests. Synthetic boundary cases are explicitly
 * distinct from the verbatim customer capture and legible screenshot rows.
 * RX has no documented checksum here: protocol_frame validates structure only.
 * ADD never changes A: each 0B stream directly displays controller quantities. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_reject_sn_reply.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_denom_query_service.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"
#include "un260/protocol/protocol_frame.h"

static counting_sim_t data;
static counting_session_state_t g_counting_session;
static counting_detail_state_t g_counting_detail_state;
static bool g_add_result_cleared, locked, g_no_note_motor_active, g_add_enabled;
static unsigned row_updates, full_updates, compact_updates, starts, ends, history_ready;
static unsigned sends[256], history_group_restarts;
static uint32_t now;
uint32_t app_clock_uptime_ms(void) { return now; }
void uart_debug_printf(const char *format, ...) { (void)format; }
int protocol_send(uint8_t command, const uint8_t *payload, uint16_t length)
{ assert(payload && length); ++sends[command]; return 0; }
void counting_history_current_invalidate(void) {}
bool counting_history_discard_pending(counting_session_state_t *state)
{ (void)state; return false; }
static void counting_history_restart_group(void) { assert(g_add_enabled);++history_group_restarts; }
static void page_01_main_set_counting_locked(bool value) { locked=value; }
static void smart_island_update_counting(int pcs, double amount) { (void)pcs; (void)amount; }
static void app_counting_runtime_on_main_data_changed(void) { ++full_updates; }
static void app_counting_runtime_on_live_denom_changed(void) { ++row_updates; }
static void history_frame(const char *tag,const uint8_t *frame,uint8_t len)
{ (void)tag; (void)frame; (void)len; }
static const counting_denom_reply_hooks_t g_counting_denom_hooks = {
    .on_history_frame=history_frame,
    .on_main_data_changed=app_counting_runtime_on_main_data_changed,
};
#define counting_data_current() (&data)
#define counting_data_mutable() (&data)
#define machine_state_add_enabled() g_add_enabled
#define currency_state_multi_selected() false
#define currency_state_restore_detected_result() false
#define ui_history_total_notes_counted_get() 0
#define ui_page_dynamic_capture_on_count_info(pcs,rejects,state) ((void)(pcs),(void)(rejects),(void)(state))
#define page_01_curr_img_refre() ((void)0)
#define smart_island_refresh_summary() ((void)0)
#define app_counting_runtime_refresh_compact(...) (++compact_updates)
#define counting_history_append_frame(...) ((void)0)
#define page_02_list_section_mark_dirty(...) ((void)0)
#define counting_history_capture_end(...) ((void)0)
#define smart_island_set_count_analysis(...) ((void)0)
#define smart_island_notify_count_end(...) (++ends)
#define app_counting_runtime_try_history_commit(...) ((void)0)
#define app_counting_runtime_main_page_active() false
#define ui_refresh_main_page() ((void)0)
#define app_auto_qr_on_end(...) ((void)0)
#define lv_tick_get() app_clock_uptime_ms()
#define lv_snprintf snprintf
/* Unrelated services remain idle; the real command poll owns query gating. */
typedef enum { BOOT_STAGE_DONE, BOOT_STAGE_FAIL } boot_stage_t;
#define boot_service_get_stage() BOOT_STAGE_DONE
#define counting_action_start_pending() false
#define counting_action_take_timeouts() 0U
#define COUNTING_ACTION_TIMEOUT_START 1U
#define COUNTING_ACTION_TIMEOUT_CLEAR 2U
#define app_counting_runtime_poll_reports counting_report_poll
#define motor_test_service_poll(...) ((void)0)
#define app_fault_recovery_poll() ((void)0)
#define page_01_main_refresh_start_state() ((void)0)
#define app_add_runtime_busy() false
#define app_add_runtime_poll() ((void)0)
#define app_setting_runtime_take_mode_clear() false
#define app_command_runtime_clear_counting_data(...) false
#define count_mode_service_poll() ((void)0)
#define app_counting_runtime_poll_history(...) ((void)0)
#define counting_multi_poll(...) ((void)0)
#define counting_multi_extra_poll(...) ((void)0)
#define counting_multi_query_busy() false
#define counting_multi_extra_busy() false
#define app_command_runtime_main_page_active() true
#include "controller_denom_runtime.inc"

#define CHECK(condition) do { if(!(condition)) { \
    fprintf(stderr,"FAIL %s:%d %s; A rows=%u updates=%u/%u sends0C=%u0D=%u phase=%u\n", \
      __func__,__LINE__,#condition,data.denom_number,row_updates,full_updates, \
      sends[0x0C],sends[0x0D],g_counting_session.phase);return false;} } while(0)

static unsigned pcs(unsigned value)
{
    for(unsigned i=0;i<data.denom_number;i++)
        if(data.denom[i].value==(int)value)return data.denom[i].pcs;
    return 0;
}
static bool has_face(unsigned value)
{
    for(unsigned i=0;i<data.denom_number;i++)if(data.denom[i].value==(int)value)return true;
    return false;
}
static uint64_t amount(void)
{
    uint64_t total=0;
    for(unsigned i=0;i<data.denom_number;i++)
        total+=counting_data_denom_amount(&data.denom[i]);
    return total;
}
static void on_start(const uint8_t *frame,uint8_t len)
{ (void)frame;(void)len;locked=true;++starts; }
static const counting_control_reply_hooks_t control_hooks={.on_start_success=on_start};
static void on_history_ready(void *context) { (void)context;++history_ready; }
static const counting_reject_sn_reply_hooks_t detail_hooks={
    .on_history_record_ready=on_history_ready,
};
static bool dispatch(const uint8_t *frame,unsigned length)
{
    if(!protocol_frame_is_valid(frame,length))return false;
    uint8_t cmd=frame[3];
    if(cmd==0x0A){
        command_start_prepare(cmd,frame,(uint8_t)length);
        counting_control_reply_dispatch(cmd,&g_counting_session,frame,(uint8_t)length,&control_hooks);
    }else if(cmd==0x0B){
        app_counting_runtime_handle_denom(&g_counting_detail_state,&g_counting_session,&data,frame,(uint8_t)length);
    }else if(cmd==0x0E){
        app_counting_runtime_handle_info(&g_counting_session,&data,frame,(uint8_t)length);
    }else if(cmd==0x49||cmd==0x0C||cmd==0x0D){
        counting_reject_sn_reply_dispatch(cmd,&g_counting_detail_state,&g_counting_session,&data,frame,(uint8_t)length,&detail_hooks);
    }
    ++now;
    return true;
}
static void synthetic(uint8_t cmd,const uint8_t *payload,unsigned size)
{
    uint8_t frame[32];
    int length=protocol_frame_build(frame,sizeof(frame),cmd,payload,(uint16_t)size);
    assert(length>0 && dispatch(frame,(unsigned)length));
}
static void begin_count(void)
{
    unsigned previous_starts=starts;
    const uint8_t payload[]={1,1};
    synthetic(0x0A,payload,sizeof(payload));
    assert(g_counting_session.start_confirmed&&starts==previous_starts+1);
}
static void boundary(uint8_t fill)
{
    uint8_t payload[11];memset(payload,fill,sizeof(payload));
    synthetic(0x0B,payload,sizeof(payload));
}
static void row(unsigned value,unsigned count)
{
    char fields[12];assert(value<=99999999U&&count<=999);
    assert(snprintf(fields,sizeof(fields),"%8u%3u",value,count)==11);
    synthetic(0x0B,(const uint8_t *)fields,11);
}
static void info(uint32_t money,unsigned count,unsigned status)
{
    uint8_t payload[8]={money>>24,money>>16,money>>8,money,count>>8,count,0,status};
    synthetic(0x0E,payload,sizeof(payload));
}
static void note(unsigned value,const char *serial)
{
    char fields[20];
    assert(snprintf(fields,sizeof(fields),"%7u%-12.12s",value,serial)==19);
    synthetic(0x49,(const uint8_t *)fields,19);
}
static void reset(void)
{
    counting_report_shutdown();
    counting_data_clear_serials(&data);counting_data_clear_errors(&data);
    memset(&data,0,sizeof(data));memset(&g_counting_session,0,sizeof(g_counting_session));
    memset(&g_counting_detail_state,0,sizeof(g_counting_detail_state));
    data.denom_number=3;data.denom[0].value=10;data.denom[1].value=50;data.denom[2].value=100;
    memset(sends,0,sizeof(sends));row_updates=full_updates=compact_updates=starts=ends=history_ready=0;
    now=100;locked=false;g_add_result_cleared=false;g_add_enabled=false;history_group_restarts=0;
}
static bool live_absolute(void)
{
    begin_count();boundary(0);
    CHECK(has_face(10)&&has_face(50)&&has_face(100));
    row(100,1);CHECK(pcs(100)==1&&amount()==100);
    unsigned after_one=row_updates+full_updates;CHECK(after_one>0);
    row(100,2);CHECK(pcs(100)==2&&amount()==200);
    row(100,3);CHECK(pcs(100)==3&&amount()==300);
    unsigned before_repeat=row_updates+full_updates;row(100,3);
    CHECK(pcs(100)==3&&amount()==300&&row_updates+full_updates==before_repeat);
    CHECK(sends[0x0C]==0&&ends==0&&locked);
    return true;
}
static bool begin_clears_counts_preserves_faces(void)
{
    data.denom[0].pcs=2;data.denom[0].amount=20;
    data.denom[1].pcs=3;data.denom[1].amount=150;
    data.total_pcs=5;data.total_amount=170;
    begin_count();
    CHECK(pcs(10)==2&&pcs(50)==3&&amount()==170);
    unsigned before=row_updates+full_updates;boundary(0);
    CHECK(data.denom_number==3&&has_face(10)&&has_face(50)&&has_face(100));
    CHECK(pcs(10)==0&&pcs(50)==0&&pcs(100)==0&&amount()==0);
    CHECK(row_updates+full_updates>before);
    row(100,1);CHECK(pcs(100)==1&&amount()==100);
    return true;
}
static bool rejected_start_retains_result(void)
{
    data.denom[2].pcs=2;data.denom[2].amount=200;
    data.total_pcs=2;data.total_amount=200;
    const uint8_t rejected[]={1,0};synthetic(0x0A,rejected,sizeof(rejected));
    CHECK(!g_counting_session.start_confirmed&&starts==0&&!locked);
    CHECK(pcs(100)==2&&amount()==200&&row_updates+full_updates==0);
    begin_count();boundary(0);row(100,1);
    CHECK(starts==1&&pcs(100)==1&&amount()==100);
    return true;
}
static bool decreasing_and_zero(void)
{
    begin_count();boundary(0);row(100,3);row(100,1);
    CHECK(pcs(100)==1&&amount()==100);
    row(100,0);CHECK(has_face(100)&&pcs(100)==0&&amount()==0);
    unsigned before=row_updates+full_updates;row(100,0);
    CHECK(row_updates+full_updates==before);
    return true;
}
static bool screenshot_prefix(void)
{
    /* Verbatim legible rows from controller-live-denom.png:
     * first5x1, first10x1 and next10x2. Tails are retained, not validated as CRC. */
    const uint8_t rows[][16]={
        {0xFD,0xDF,0x10,0x0B,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x35,0x20,0x20,0x31,0xBE},
        {0xFD,0xDF,0x10,0x0B,0x20,0x20,0x20,0x20,0x20,0x20,0x31,0x30,0x20,0x20,0x31,0x84},
        {0xFD,0xDF,0x10,0x0B,0x20,0x20,0x20,0x20,0x20,0x20,0x31,0x30,0x20,0x20,0x32,0xD7},
    };
    begin_count();boundary(0);
    CHECK(dispatch(rows[0],16));CHECK(pcs(5)==1&&amount()==5);
    info(5,1,1);note(5,"SYNTHETIC001");
    CHECK(dispatch(rows[1],16));CHECK(pcs(5)==1&&pcs(10)==1&&amount()==15);
    info(15,2,1);note(10,"SYNTHETIC002");
    CHECK(dispatch(rows[2],16));CHECK(pcs(5)==1&&pcs(10)==2&&amount()==25);
    CHECK(dispatch(rows[2],16));CHECK(pcs(10)==2&&amount()==25);
    return true;
}
static bool note_is_b_only(void)
{
    begin_count();boundary(0);row(100,1);
    unsigned previous=row_updates+full_updates;
    note(50,"SAME12345678");note(50,"SAME12345678");note(10,"");
    CHECK(pcs(100)==1&&pcs(50)==0&&pcs(10)==0&&amount()==100);
    CHECK(row_updates+full_updates==previous);
    CHECK(data.sn_capacity>=2&&data.sn_str[0]&&data.sn_str[1]);
    CHECK(!strcmp(data.sn_str[0],"SAME12345678")&&!strcmp(data.sn_str[1],"SAME12345678"));
    CHECK(data.denom_mix[0]==50&&data.denom_mix[1]==50);
    info(110,3,1);CHECK(pcs(100)==1&&amount()==100);
    return true;
}
static bool before_first_info(void)
{
    begin_count();CHECK(g_counting_session.phase==COUNTING_SESSION_IDLE);
    boundary(0);row(100,1);
    CHECK(pcs(100)==1&&amount()==100&&row_updates>0);
    CHECK(g_counting_session.start_confirmed&&locked&&ends==0);
    info(0,1,1);CHECK(pcs(100)==1&&amount()==100);
    info(100,1,1);CHECK(pcs(100)==1&&amount()==100);
    return true;
}
static bool end_before_stop(void)
{
    begin_count();boundary(0);row(100,1);info(100,1,1);boundary(255);
    CHECK(sends[0x0C]==0&&ends==0&&locked);
    info(0,0,2);CHECK(sends[0x0C]==1&&ends==1&&!locked);
    boundary(255);info(0,0,2);CHECK(sends[0x0C]==1&&ends==1);
    return true;
}
static bool stop_before_end(void)
{
    begin_count();boundary(0);row(100,1);info(100,1,1);info(0,0,2);
    CHECK(!locked&&ends==1&&sends[0x0C]==0);
    boundary(255);CHECK(sends[0x0C]==1&&pcs(100)==1&&amount()==100);
    boundary(255);info(0,0,2);CHECK(sends[0x0C]==1&&ends==1);
    return true;
}
static bool directory_zero_faces(void)
{
    data.denom[0].pcs=7;data.denom[1].pcs=4;
    counting_denom_query_expect_push(&g_counting_detail_state,now,true);
    boundary(0);row(20,0);row(50,0);boundary(255);
    CHECK(data.denom_number==2&&has_face(20)&&has_face(50)&&!has_face(100));
    CHECK(pcs(20)==0&&pcs(50)==0&&amount()==0);
    CHECK(g_counting_detail_state.query_complete&&sends[0x0C]==0&&starts==0);
    return true;
}
static bool malformed_fields(void)
{
    begin_count();boundary(0);row(100,1);
    const char *bad[]={"     1x0  2","      10 -1","    1 00  2","00000000002","     1001 2"};
    unsigned old_updates=row_updates+full_updates;
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){
        assert(strlen(bad[i])==11);synthetic(0x0B,(const uint8_t *)bad[i],11);
        CHECK(pcs(100)==1&&amount()==100);
    }
    CHECK(row_updates+full_updates==old_updates);
    row(100,2);CHECK(pcs(100)==2&&amount()==200);
    boundary(255);boundary(0);row(100,3);boundary(255);
    CHECK(pcs(100)==3&&amount()==300);
    return true;
}
static bool frame_lengths(void)
{
    begin_count();boundary(0);row(100,1);
    uint8_t b[17]={0xFD,0xDF,16,0x0B};
    memcpy(b+4,"     100  2",11);
    for(unsigned len=0;len<16;len++){
        app_counting_runtime_handle_denom(&g_counting_detail_state,&g_counting_session,&data,b,(uint8_t)len);
        CHECK(pcs(100)==1&&amount()==100);
    }
    b[2]=17;
    app_counting_runtime_handle_denom(&g_counting_detail_state,&g_counting_session,&data,b,17);
    CHECK(pcs(100)==1&&amount()==100);
    b[2]=16;b[15]=0xA7;CHECK(dispatch(b,16));CHECK(pcs(100)==2);
    b[0]=0;CHECK(!dispatch(b,16));b[0]=0xFD;b[2]=15;CHECK(!dispatch(b,16));
    return true;
}
static bool table_capacity(void)
{
    data.denom_number=0;begin_count();boundary(0);
    for(unsigned n=1;n<=COUNTING_DENOM_MAX_ITEMS;n++)row(n,n);
    CHECK(data.denom_number==COUNTING_DENOM_MAX_ITEMS);
    row(1,7);CHECK(pcs(1)==7&&data.denom_number==COUNTING_DENOM_MAX_ITEMS);
    uint64_t before=amount();row(999,1);
    CHECK(!has_face(999)&&amount()==before&&data.denom_number==COUNTING_DENOM_MAX_ITEMS);
    row(1,0);CHECK(has_face(1)&&pcs(1)==0);
    return true;
}
static bool max_row_amount(void)
{
    begin_count();boundary(0);row(99999999,999);
    CHECK(pcs(99999999)==999&&amount()==UINT64_C(99899999001));
    row(99999999,998);CHECK(amount()==UINT64_C(99799999002));
    return true;
}
static bool query_then_start(void)
{
    for(unsigned scenario=0;scenario<3;scenario++){
        if(scenario)reset();
        counting_denom_query_expect_push(&g_counting_detail_state,now,true);
        if(scenario>=1){boundary(0);row(50,0);}
        if(scenario==2){now+=1600;counting_denom_query_poll(&g_counting_detail_state,now,true,true);CHECK(g_counting_detail_state.query_expired);}
        begin_count();boundary(0);row(100,1);
        CHECK(pcs(100)==1&&amount()==100);
        CHECK(!g_counting_detail_state.query_pending&&!g_counting_detail_state.query_expired);
    }
    return true;
}
static bool complete_independent_reports(unsigned notes)
{
    uint8_t payload[20]={0};
    synthetic(0x0C,payload,2);synthetic(0x0C,payload,2);
    memset(payload,255,2);synthetic(0x0C,payload,2);
    CHECK(sends[0x0D]==1);
    memset(payload,0,sizeof(payload));synthetic(0x0D,payload,20);
    for(unsigned i=0;i<notes;i++){
        char text[20];assert(snprintf(text,sizeof(text),"%7uREPORT%06u",50,i+1)==19);
        payload[0]=(uint8_t)(i+1);memcpy(payload+1,text,19);
        synthetic(0x0D,payload,20);
    }
    memset(payload,255,sizeof(payload));synthetic(0x0D,payload,20);
    CHECK(history_ready==1&&g_counting_session.history_record.end_seen);
    CHECK(data.sn_capacity>0&&data.denom_mix[0]==50);
    return true;
}
static bool first_add_snapshot(void)
{
    g_add_enabled=true;begin_count();boundary(0);
    row(100,1);row(10,1);row(50,0);info(110,2,1);
    boundary(255);info(0,0,2);
    CHECK(complete_independent_reports(2));
    CHECK(pcs(100)==1&&pcs(10)==1&&amount()==110);
    return true;
}
static bool add_absolute_snapshots(void)
{
    CHECK(first_add_snapshot());begin_count();
    CHECK(pcs(100)==1&&pcs(10)==1&&amount()==110);
    boundary(0);CHECK(has_face(100)&&has_face(10)&&amount()==0);
    row(100,2);CHECK(pcs(100)==2&&amount()==200);
    row(10,3);row(50,0);CHECK(pcs(10)==3&&amount()==230);
    info(230,5,1);info(0,0,2);boundary(255);
    CHECK(starts==2&&ends==2&&sends[0x0C]==2&&!locked);
    CHECK(pcs(100)==2&&pcs(10)==3&&amount()==230);
    CHECK(data.total_pcs==5&&data.total_amount==230&&history_group_restarts==0);
    return true;
}
static bool add_next_snapshot_omits_face(void)
{
    CHECK(first_add_snapshot());begin_count();boundary(0);
    row(10,1);info(10,1,1);boundary(255);info(0,0,2);
    CHECK(starts==2&&ends==2&&sends[0x0C]==2&&!locked);
    CHECK(has_face(100)&&pcs(100)==0&&pcs(10)==1&&pcs(50)==0&&amount()==10);
    CHECK(data.total_pcs==1&&data.total_amount==10&&history_group_restarts==1);
    return true;
}
static bool malformed_a_still_gets_bc(void)
{
    begin_count();boundary(0);row(100,1);info(100,1,1);
    synthetic(0x0B,(const uint8_t *)"     1x0  2",11);
    CHECK(pcs(100)==1&&amount()==100);
    info(0,0,2);boundary(255);
    CHECK(!locked&&ends==1&&sends[0x0C]==1);
    CHECK(complete_independent_reports(1));
    CHECK(pcs(100)==1&&pcs(50)==0&&amount()==100);
    CHECK(data.total_pcs==1&&data.total_amount==100);
    return true;
}
static bool overflow_a_still_gets_bc(void)
{
    data.denom_number=0;begin_count();boundary(0);
    for(unsigned i=1;i<=COUNTING_DENOM_MAX_ITEMS;i++)row(i,1);
    row(COUNTING_DENOM_MAX_ITEMS+1,1);
    info(120,15,1);boundary(255);
    CHECK(sends[0x0C]==0&&data.denom_number==15&&amount()==120);
    info(0,0,2);
    CHECK(!locked&&ends==1&&sends[0x0C]==1);
    CHECK(complete_independent_reports(15));
    CHECK(data.denom_number==15&&amount()==120&&!has_face(50));
    CHECK(data.total_pcs==15&&data.total_amount==120);
    return true;
}
static bool background_query_suspended(void)
{
    begin_count();
    now+=10000;app_command_runtime_poll(now);
    CHECK(sends[0x0B]==0&&g_counting_session.start_confirmed);
    boundary(0);row(100,1);info(100,1,1);
    now+=10000;app_command_runtime_poll(now);
    CHECK(sends[0x0B]==0&&pcs(100)==1&&amount()==100&&locked);
    info(0,0,2);CHECK(!locked&&ends==1);
    return true;
}
static bool interleaved_keys(void)
{
    begin_count();boundary(0);row(100,1);row(10,2);row(50,1);row(100,2);
    CHECK(pcs(100)==2&&pcs(10)==2&&pcs(50)==1&&amount()==270);
    info(10,9,1);note(50,"OTHER0000001");
    CHECK(pcs(100)==2&&pcs(10)==2&&pcs(50)==1&&amount()==270);
    row(10,0);CHECK(pcs(100)==2&&pcs(50)==1&&pcs(10)==0&&amount()==250);
    return true;
}
static bool orphan_rows(void)
{
    begin_count();row(100,1);boundary(255);
    CHECK(pcs(100)==0&&amount()==0&&sends[0x0C]==0);
    boundary(0);row(100,1);boundary(255);row(100,7);
    CHECK(pcs(100)==1&&amount()==100&&sends[0x0C]==0);
    return true;
}
static bool capture_one_note(const char *path)
{
    FILE *file=fopen(path,"r");CHECK(file);
    char line[2048];unsigned frames=0,denom_rows=0;
    while(fgets(line,sizeof(line),file)){
        char *rx=strstr(line,"RX[");if(!rx)continue;
        unsigned size=0;CHECK(sscanf(rx,"RX[%u]",&size)==1&&size<=32);
        char *cursor=strchr(rx,':');CHECK(cursor);++cursor;
        uint8_t frame[32];
        for(unsigned i=0;i<size;i++){unsigned value;int used;CHECK(sscanf(cursor," %x%n",&value,&used)==1&&value<=255);frame[i]=(uint8_t)value;cursor+=used;}
        CHECK(dispatch(frame,size));++frames;
        if(frame[3]==0x49)CHECK(pcs(100)==0);
        if(frame[3]==0x0B&&frame[4]==0x20){
            ++denom_rows;CHECK(pcs(100)==1&&amount()==100&&ends==0);
        }
    }
    fclose(file);
    CHECK(frames==17&&denom_rows==1&&starts==1&&ends==1);
    CHECK(pcs(100)==1&&amount()==100&&data.total_pcs==1&&data.total_amount==100);
    CHECK(sends[0x0C]==1&&sends[0x0D]==1&&history_ready==1);
    CHECK(data.sn_capacity>=1&&data.sn_str[0]&&!strcmp(data.sn_str[0],"MG62981229A"));
    return true;
}
struct scenario {const char *name;bool (*run)(void);};
static const struct scenario cases[]={
 {"live_absolute",live_absolute},{"decreasing_and_zero",decreasing_and_zero},
 {"begin_clears_counts_preserves_faces",begin_clears_counts_preserves_faces},
 {"rejected_start_retains_result",rejected_start_retains_result},
 {"screenshot_prefix",screenshot_prefix},{"note_is_b_only",note_is_b_only},
 {"before_first_info",before_first_info},{"end_before_stop",end_before_stop},
 {"stop_before_end",stop_before_end},{"directory_zero_faces",directory_zero_faces},
 {"malformed_fields",malformed_fields},{"frame_lengths",frame_lengths},
 {"table_capacity",table_capacity},{"max_row_amount",max_row_amount},
 {"query_then_start",query_then_start},{"interleaved_keys",interleaved_keys},
 {"orphan_rows",orphan_rows},{"background_query_suspended",background_query_suspended},
 {"malformed_a_still_gets_bc",malformed_a_still_gets_bc},
 {"overflow_a_still_gets_bc",overflow_a_still_gets_bc},
 {"add_absolute_snapshots",add_absolute_snapshots},
 {"add_next_snapshot_omits_face",add_next_snapshot_omits_face},
};
int main(int argc,char **argv)
{
    if(argc<2)return 2;
    reset();bool ok=false,found=false;
    if(!strcmp(argv[1],"capture_one_note")&&argc==3){found=true;ok=capture_one_note(argv[2]);}
    else for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
        if(!strcmp(argv[1],cases[i].name)){found=true;ok=cases[i].run();break;}
    counting_report_shutdown();counting_data_clear_serials(&data);counting_data_clear_errors(&data);
    if(!found)return 2;
    if(ok)printf("PASS controller 0B %s\n",argv[1]);
    return ok?0:1;
}
