#include "un260/data_collection/data_collection.h"
#include "un260/protocol/protocol_request.h"
#include "un260/lv_system/ui_i18n.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t clock_ms=100;
uint64_t app_clock_monotonic_ms(void){return clock_ms;}
void ui_message_key(ui_message_t *m,const char *key){memset(m,0,sizeof(*m));snprintf(m->key,sizeof(m->key),"%s",key);}
bool ui_message_render(const ui_message_t *m,char *out,size_t size){snprintf(out,size,"%s",m->key);return true;}
static data_collection_snapshot_t get(void){data_collection_snapshot_t s;data_collection_snapshot(&s);return s;}
static void rx(unsigned code){uint8_t b[]={0xFD,0xDF,6,0xC0,code,0x0A};clock_ms++;data_collection_reply_handle(b,6,clock_ms);}
static void off(void){assert(data_collection_request_begin(DATA_COLLECT_MODE_NONE,"disable",clock_ms));rx(0xFF);assert(get().mode==0&&!get().pending);}
static void on(data_collect_mode_t mode){clock_ms+=10;assert(data_collection_request_begin(mode,"enable",clock_ms));rx(mode==DATA_COLLECT_MODE_ALL?1:3);assert(get().mode==mode&&!get().pending);}
int main(void){
 rx(3);assert(get().mode==0);on(DATA_COLLECT_MODE_FALSE);
 data_collection_count_start();rx(4);rx(4);assert(get().phase==DATA_COLLECTION_COLLECTING&&get().counting);
 rx(7);assert(get().phase!=DATA_COLLECTION_SUCCESS);data_collection_count_end(12);assert(get().phase==DATA_COLLECTION_SUCCESS&&get().mode==DATA_COLLECT_MODE_FALSE);
 unsigned completed=get().completed;rx(7);rx(4);assert(get().completed==completed&&get().phase==DATA_COLLECTION_SUCCESS);
 data_collection_count_start();data_collection_count_end(8);assert(get().phase==DATA_COLLECTION_WAITING);rx(7);assert(get().phase==DATA_COLLECTION_SUCCESS&&get().completed==completed+1);
 data_collection_count_start();rx(6);data_collection_count_end(4);rx(7);assert(get().phase==DATA_COLLECTION_ERROR);
 assert(data_collection_request_begin(DATA_COLLECT_MODE_NONE,"disable",clock_ms));for(int i=0;i<20;i++){rx(4);rx(2);}assert(get().pending&&get().mode==DATA_COLLECT_MODE_FALSE);
 clock_ms+=4000;assert(data_collection_request_take_timeout(clock_ms));assert(get().phase==DATA_COLLECTION_CLOSE_UNKNOWN&&get().pending);rx(4);assert(get().pending);rx(0xFF);assert(get().mode==0&&!get().pending);
 on(DATA_COLLECT_MODE_ALL);data_collection_count_start();rx(2);data_collection_count_end(6);rx(2);rx(7);assert(get().phase!=DATA_COLLECTION_SUCCESS&&get().mode==DATA_COLLECT_MODE_ALL);off();
 clock_ms+=10;assert(data_collection_request_begin(DATA_COLLECT_MODE_FALSE,"enable",clock_ms));protocol_request_result_context_begin(clock_ms-1);rx(3);protocol_request_result_context_end();assert(get().pending);rx(3);assert(!get().pending);off();
 assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL,"enable",clock_ms));data_collection_request_cancel();assert(get().uncertain&&get().pending);rx(1);assert(get().mode==DATA_COLLECT_MODE_ALL);off();
 puts("capture state: ordering, duplicate progress, disable timeout, late ACK, all-mode uncertainty, send failure PASS");
}
