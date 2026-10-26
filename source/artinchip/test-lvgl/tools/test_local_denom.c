/* Compatibility regression entry: controller 0B is the only A source. */
#define main report_regressions_main
#include "test_counting_report_sync.c"
#undef main
#include "un260/counting/counting_denom_reply.h"
static unsigned quantity(unsigned value)
{
    for(unsigned i=0;i<data.denom_number;++i)
        if(data.denom[i].value==(int)value)return data.denom[i].pcs;
    return 0;
}
static void boundary(unsigned fill)
{
    uint8_t b[16]={0xFD,0xDF,16,0x0B};memset(b+4,fill,11);
    counting_denom_reply_handle(&detail,&session,&data,b,sizeof(b),NULL);
}
static void denom(unsigned value,unsigned pcs)
{
    uint8_t b[16]={0xFD,0xDF,16,0x0B};char text[12];
    snprintf(text,sizeof(text),"%8u%3u",value,pcs);memcpy(b+4,text,11);
    counting_denom_reply_handle(&detail,&session,&data,b,sizeof(b),NULL);
}
static void live_serial(unsigned value)
{
    uint8_t b[24]={0xFD,0xDF,24,0x49};char text[20];
    snprintf(text,sizeof(text),"%7u%-12s",value,"SAME12345678");memcpy(b+4,text,19);frame(b,24);
}
int main(void)
{
    reset();data.denom_number=2;data.denom[0]=(denom_t){10,4,40};data.denom[1]=(denom_t){50,10,500};
    start(false);counting_denom_reply_begin_count(&detail);boundary(0);
    assert(data.denom_number==2&&data.denom[0].value==10&&quantity(10)==0&&quantity(50)==0);
    live_serial(100);live_serial(100);assert(quantity(100)==0&&quantity(50)==0);
    denom(50,1);assert(quantity(50)==1);denom(50,2);assert(quantity(50)==2);
    denom(50,2);assert(quantity(50)==2);denom(50,0);assert(quantity(50)==0);
    denom(10,3);denom(50,12);boundary(255);
    assert(quantity(10)==3&&quantity(50)==12);live_serial(100);assert(quantity(100)==0);
    start(true);counting_denom_reply_begin_count(&detail);boundary(0);
    assert(quantity(10)==0&&quantity(50)==0);
    denom(10,3);denom(50,13);assert(quantity(10)==3&&quantity(50)==13);
    boundary(255);denom(50,99);assert(quantity(50)==13);
    reset();counting_report_shutdown();
    puts("PASS controller denomination: START clears counts/preserves values, absolute/duplicate/decreasing rows, ADD has no local base, 49 never writes A, orphan row ignored");
    return 0;
}
