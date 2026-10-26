/* Real AUTO currency detection and runtime 0B routing. UI/history/UART effects
 * are probes; currency state, denomination/query/report and MULTI models stay
 * production implementations. Run each scenario separately to report all. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/currency/currency_state.h"
#include "un260/currency/currency_reply.h"
#include "un260/counting/counting_data_store_internal.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_multi.h"

static counting_detail_state_t detail;
static counting_session_state_t session;
static unsigned refreshes,images,summaries,requests;
#define PAGE_02_SECTION_A 0
static void page_02_list_section_mark_dirty(unsigned section){assert(section==0);}
static void ui_refresh_main_page(void){refreshes++;}
uint32_t app_clock_uptime_ms(void){static uint32_t now=100;return ++now;}
void uart_debug_printf(const char *s,...){(void)s;}
void counting_history_current_invalidate(void){}
int protocol_send(uint8_t c,const uint8_t *p,uint16_t n){(void)c;(void)p;(void)n;requests++;return 0;}
static void page_01_curr_img_refre(void){images++;}
static void smart_island_refresh_summary(void){summaries++;}
static void app_counting_runtime_on_main_data_changed(void){refreshes++;}
static void app_counting_runtime_on_live_denom_changed(void){refreshes++;}
static const counting_denom_reply_hooks_t g_counting_denom_hooks={
    .on_main_data_changed=app_counting_runtime_on_main_data_changed
};
#include "controller_denom_currency_runtime.inc"
#define DATA (*counting_data_mutable())
static void detect(const char *code,unsigned cmd){
 uint8_t b[8]={0xFD,0xDF,8,0};b[3]=cmd;memcpy(b+4,code,3);
 app_currency_runtime_handle_detected(&detail,b,sizeof(b));
}
static void effective(const char *expected){
 char code[4];currency_state_get_effective_code(code);assert(!strcmp(code,expected));
}
static void wire(unsigned value,unsigned pcs,int marker){
 uint8_t b[16]={0xFD,0xDF,16,0x0B};char text[16];
 if(marker>=0)memset(b+4,marker,11);
 else{snprintf(text,sizeof(text),"%8u%3u",value,pcs);memcpy(b+4,text,11);}
 app_counting_runtime_handle_denom(&detail,&session,&DATA,b,sizeof(b));
}
static int find(unsigned value){
 for(unsigned i=0;i<DATA.denom_number;++i)if(DATA.denom[i].value==(int)value)return (int)i;
 return -1;
}
static void expect(unsigned value,unsigned pcs){
 int i=find(value);assert(i>=0 && DATA.denom[i].pcs==pcs);
}
static void begin(bool add){
 counting_report_begin(add,&DATA);counting_denom_reply_begin_count(&detail);
 session.start_confirmed=true;session.phase=COUNTING_SESSION_ACTIVE;
}
static void auto_fixture(bool prior){
 currency_state_reset();assert(currency_state_confirm_auto_selection());
 if(prior){
  detect("USD",0x49);
  DATA.denom_number=3;
  DATA.denom[0]=(denom_t){.value=1,.pcs=3};
  DATA.denom[1]=(denom_t){.value=5,.pcs=2};
  DATA.denom[2]=(denom_t){.value=10,.pcs=1};
 }
}
static void auto_case(bool prior,bool same,bool data_first,bool add){
 auto_fixture(prior);begin(add);
 const char *code=prior && same?"USD":"EUR";
 if(!data_first)detect(code,0x49);
 wire(0,0,0);wire(5,1,-1);wire(5,2,-1);wire(10,1,-1);
 if(data_first)detect(code,0x49);
 wire(0,0,255);effective(code);expect(5,2);expect(10,1);
 unsigned old=refreshes;detect(code,0x49);detect(code,0x50);
 expect(5,2);expect(10,1);assert(refreshes==old && !requests);
 if(prior && !same && find(1)>=0){
  fprintf(stderr,"AUTO CROSS-CURRENCY LEAK: effective=%s prior USD-only face=1 remains pcs=%u; order=%s ADD=%d\n",
    code,DATA.denom[find(1)].pcs,data_first?"0B-before-detection":"detection-before-0B",add);
  exit(1);
 }
 if(prior && same)expect(1,0);
 if(!prior)assert(DATA.denom_number==2);
}
static void hidden_same_currency(void){
 auto_fixture(true);currency_state_clear_detected_result();effective("AUT");
 begin(false);wire(0,0,0);wire(5,2,-1);
 unsigned before=refreshes;detect("USD",0x49);
 effective("USD");expect(1,0);expect(5,2);expect(10,0);
 assert(refreshes==before);wire(0,0,255);
}
static void idle_cross_currency(void){
 auto_fixture(true);detect("EUR",0x49);
 effective("EUR");assert(DATA.denom_number==0 && !requests);
}
static void multi_info(const char *code,unsigned amount,unsigned pcs){
 uint8_t b[16]={0xFD,0xDF,16,0x0E};memcpy(b+4,code,3);
 b[7]=amount>>24;b[8]=amount>>16;b[9]=amount>>8;b[10]=amount;
 b[11]=pcs>>8;b[12]=pcs;b[14]=1;assert(counting_multi_info(b,sizeof(b)));
}
static void multi_case(bool drain_after_selection){
 currency_state_reset();assert(currency_state_confirm_multi_selection());
 DATA.multi_currency_result=true;
 DATA.denom_number=1;DATA.denom[0]=(denom_t){.value=777,.pcs=8};
 counting_multi_reset();counting_multi_begin(false);
 multi_info("USD",50,1);multi_info("EUR",20,1);
 uint8_t stop[13]={0xFD,0xDF,13,0x0E};stop[9]=2;stop[11]=2;
 assert(counting_multi_info(stop,sizeof(stop)));assert(counting_multi_request(0,100));
 if(drain_after_selection){
  currency_state_reset();DATA.multi_currency_result=false;counting_multi_reset();
 }
 wire(0,0,0);wire(50,1,-1);wire(0,0,255);
 assert(!counting_multi_query_busy());expect(777,8);assert(DATA.denom_number==1);
 if(!drain_after_selection){
  const counting_multi_t *m=counting_multi_current();
  assert(m->count==2 && m->currencies[0].status==MULTI_DETAIL_READY);
  assert(m->currencies[0].denom_count==1 && m->currencies[0].denom[0].value==50);
  assert(m->currencies[1].pcs==1 && m->currencies[1].amount==20);
  unsigned old=images;detect("USD",0x49);assert(images==old);effective("MUL");
 }
}
int main(int argc,char **argv){
 assert(argc==2);
 const char *name=argv[1];
 if(!strcmp(name,"auto-first-detection-before"))auto_case(false,false,false,false);
 else if(!strcmp(name,"auto-first-data-before"))auto_case(false,false,true,false);
 else if(!strcmp(name,"auto-same-detection-before"))auto_case(true,true,false,false);
 else if(!strcmp(name,"auto-same-data-before"))auto_case(true,true,true,false);
 else if(!strcmp(name,"auto-same-add"))auto_case(true,true,true,true);
 else if(!strcmp(name,"auto-cross-detection-before"))auto_case(true,false,false,false);
 else if(!strcmp(name,"auto-cross-data-before"))auto_case(true,false,true,false);
 else if(!strcmp(name,"auto-cross-add"))auto_case(true,false,true,true);
 else if(!strcmp(name,"auto-hidden-same"))hidden_same_currency();
 else if(!strcmp(name,"auto-cross-idle"))idle_cross_currency();
 else if(!strcmp(name,"multi-independent-table"))multi_case(false);
 else if(!strcmp(name,"multi-drain-after-switch"))multi_case(true);
 else return 2;
 counting_report_shutdown();counting_data_clear_serials(&DATA);counting_data_clear_errors(&DATA);
 printf("PASS %s\n",name);return 0;
}
