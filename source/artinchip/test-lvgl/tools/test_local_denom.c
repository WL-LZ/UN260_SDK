/* Explicit per-note facts remain accurate without a live 0x0B stream. */
#define main report_regressions_main
#include "test_counting_report_sync.c"
#undef main
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_denom_reply.h"
static void total(unsigned amount, unsigned pcs) {
    data.total_amount=amount;data.total_pcs=pcs;
    counting_report_observe_total(&data,amount,pcs);
}
static unsigned denomination(unsigned value) {
    for(unsigned i=0;i<data.denom_number;++i)
        if(data.denom[i].value==(int)value)return data.denom[i].pcs;
    return 0;
}
static void denom_boundary(unsigned fill) {
    uint8_t b[16]={0xFD,0xDF,16,0x0B};memset(b+4,fill,11);
    counting_denom_reply_handle(&detail,&session,&data,b,sizeof(b),NULL);
}
static void denom_row(unsigned value,unsigned pcs) {
    uint8_t b[16]={0xFD,0xDF,16,0x0B};char text[12];
    snprintf(text,sizeof(text),"%8u%3u",value,pcs);memcpy(b+4,text,11);
    counting_denom_reply_handle(&detail,&session,&data,b,sizeof(b),NULL);
}
static void note(unsigned value) {
    uint8_t b[24]={0xFD,0xDF,24,0x49};char text[20];
    snprintf(text,sizeof(text),"%7u%-12s",value,"SAME12345678");memcpy(b+4,text,19);frame(b,24);
}
int main(int argc,char **argv) {
    reset();start(false);total(0,0);total(50,1);note(50);total(100,2);note(50);total(100,2);
    total(120,3);note(20);assert(denomination(50)==2 && denomination(20)==1);
    start(true);total(0,0);assert(data.total_pcs==3 && data.total_amount==120);
    total(120,3);total(170,4);note(50);assert(denomination(50)==3 && denomination(20)==1);
    start(true);total(0,0);stop();assert(data.total_pcs==4 && denomination(50)==3);
    reset();start(false);total(100,2);total(120,3);
    assert(denomination(50)==0 && denomination(20)==0); /* Totals never invent notes. */
    note(50);note(50);note(20);assert(denomination(50)==2 && denomination(20)==1);
    reset();start(true);total(0,0);total(50,1);note(50);assert(denomination(50)==1 && denomination(20)==0);
    start(false);total(0,0);assert(denomination(50)==0);
    reset();start(false);total(50,1);note(50);total(100,2);note(50);
    session.start_confirmed=false;session.phase=COUNTING_SESSION_FINISHED_WAIT_START;
    assert(!requests[0x0C]);
    denom_boundary(0);denom_row(20,0);denom_row(50,2);
    assert(denomination(50)==2 && !requests[0x0C]);
    denom_boundary(255);assert(denomination(50)==2 && requests[0x0C]==1);
    denom_boundary(255);assert(denomination(50)==2 && requests[0x0C]==1);
    mark(0x0C,0);mark(0x0C,255);mark(0x0D,0);row(1,false);row(2,false);mark(0x0D,255);
    assert(denomination(50)==2 && denomination(20)==0);
    start(true);total(0,0);total(150,3);note(50);assert(denomination(50)==3);
    session.start_confirmed=false;session.phase=COUNTING_SESSION_FINISHED_WAIT_START;
    denom_boundary(0);denom_row(50,3);denom_boundary(255);assert(denomination(50)==3);
    counting_report_reset();note(50);total(200,4);assert(denomination(50)==3);
    if(argc>=2) {
        reset();data.total_pcs=0;data.total_amount=0;
        FILE *f=fopen(argv[1],"r");assert(f);char line[1024];bool add=false;
        unsigned ends=0, live_count=0;
        while(fgets(line,sizeof(line),f)) {
            char *p=strstr(line,"RX[");if(!p)continue;
            unsigned n;assert(sscanf(p,"RX[%u]",&n)==1);if(n>32)continue;
            p=strchr(p,':')+1;uint8_t b[32];
            for(unsigned i=0;i<n;++i){unsigned v;int used;assert(sscanf(p," %x%n",&v,&used)==1);b[i]=v;p+=used;}
            if(b[3]==0x3B && b[4]==1)add=true;
            if(b[3]==0x0A && b[4]==1 && b[5]==1){start(add);continue;}
            if(b[3]==0x49){frame(b,n);if(argc==3)assert(denomination(100)==0);continue;}
            if(b[3]!=0x0E)continue;
            counting_info_reply_result_t result=counting_info_reply_handle(&session,&data,b,n,0);
            if(result.kind==COUNTING_INFO_REPLY_LIVE){
                counting_report_observe_total(&data,((uint32_t)b[4]<<24)|((uint32_t)b[5]<<16)|((uint32_t)b[6]<<8)|b[7],((unsigned)b[8]<<8)|b[9]);
                if(argc==3) assert(denomination(100)==0);
                ++live_count;
            }
            if(result.kind==COUNTING_INFO_REPLY_FINISHED){
                if(argc==3) {
                    assert(result.final_pcs==80 && result.final_amount==2600);
                    assert(denomination(100)==0 && denomination(50)==45 && denomination(10)==35);
                } else {
                    assert(denomination(50)==(unsigned)result.final_pcs);
                    assert(result.final_amount==50*result.final_pcs);
                }
                printf("PASS captured end %u: PCS=%d amount=%.0f; 100=%u 50=%u 10=%u; ADD=%d, before final 0B/0D\n",++ends,result.final_pcs,result.final_amount,denomination(100),denomination(50),denomination(10),add);
            }
        }
        fclose(f);assert(ends>=2 && live_count>100);
    }
    reset();counting_report_shutdown();
    puts("PASS local denomination: mixed values, duplicate totals, ADD continuation/reset/empty pass, gaps, final A replacement and reset isolation");
    return 0;
}
