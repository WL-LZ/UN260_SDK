#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/lv_components/ui_notice.h"
static uint64_t now;static unsigned tx,errors,removed;static bool auto_popup,modal,fail;
static machine_fault_key_t door={MACHINE_FAULT_START,2,9},full={MACHINE_FAULT_START,2,7};
static machine_fault_key_t batch={MACHINE_FAULT_BATCH,0,4},empty={MACHINE_FAULT_START,1,2};
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
bool fault_popup_get_auto_enabled(void){return auto_popup;}
bool fault_popup_is_showing(void){return modal;}
void fault_popup_stacker_cleared(void){++removed;}
void ui_notice_post_text(ui_notice_kind_t k,const char *key,const char *title,const char *body){assert(k==UI_NOTICE_ERROR&&key&&title&&body);++errors;}
void ui_notice_clear(const char *key){assert(key);}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t size){assert(cmd==0x3D&&size==1&&*data==1);++tx;return fail?-1:0;}
static void elapsed(uint32_t ms){now+=ms;app_fault_recovery_poll();}
static void reply(uint8_t result){uint8_t b[]={0xFD,0xDF,6,0x3D,result,0};app_fault_recovery_handle_reply(b,6);}
static void reset(void){app_fault_recovery_init();auto_popup=modal=fail=false;}
int main(void){
 reset();app_fault_recovery_report(door);elapsed(60000);assert(!tx);
 app_fault_recovery_confirm(door);assert(tx==1);
 assert(app_fault_recovery_start_status()==APP_FAULT_START_WAIT);reply(1);
 assert(app_fault_recovery_start_status()==APP_FAULT_START_READY);elapsed(10000);assert(tx==1);
 /* Neither repeated reports nor popup toggles grant clear permission. */
 app_fault_recovery_report(door);elapsed(60000);assert(tx==1);
 app_fault_recovery_confirm(door);assert(tx==2);reply(1);
 machine_fault_key_t held[]={full,batch,door,{MACHINE_FAULT_RUNTIME,0,7}};
 for(unsigned i=0;i<sizeof(held)/sizeof(held[0]);++i){
  reset();unsigned start=tx;
  app_fault_recovery_count_started();app_fault_recovery_report(held[i]);
  app_fault_recovery_count_finished();
  for(unsigned j=0;j<20;++j){auto_popup=(j%2)!=0;elapsed(5000);app_fault_recovery_report(held[i]);}
  assert(tx==start);
 }
 /* A physical removal after a request was sent is not lost on its timeout;
    it permits exactly one new request, not an autonomous retry loop. */
 reset();app_fault_recovery_report(full);unsigned retry_tx=tx;
 app_fault_recovery_confirm(full);app_fault_recovery_stacker_cleared();
 elapsed(2000);elapsed(0);assert(tx==retry_tx+2);
 elapsed(2000);elapsed(60000);assert(tx==retry_tx+2);
 /* New reports invalidate old confirmations deferred during counting. */
 reset();app_fault_recovery_count_started();app_fault_recovery_report(full);
 app_fault_recovery_confirm(full);app_fault_recovery_report(door);retry_tx=tx;
 app_fault_recovery_count_finished();elapsed(10000);assert(tx==retry_tx);
 reset();auto_popup=true;app_fault_recovery_report(full);unsigned before=tx;
 elapsed(20000);assert(tx==before);modal=true;app_fault_recovery_confirm(full);assert(tx==before+1);
 app_fault_recovery_confirm(full);assert(tx==before+1);reply(1);modal=false;
 app_fault_recovery_stacker_cleared();assert(tx==before+1);
 /* Taking money before Confirm also releases the controller, including batch.
    Duplicate 51 frames never create concurrent or post-failure retry storms. */
 for(int i=0;i<2;++i){reset();auto_popup=true;app_fault_recovery_report(i?batch:full);before=tx;
  app_fault_recovery_stacker_cleared();app_fault_recovery_stacker_cleared();assert(tx==before+1);
  reply(2);app_fault_recovery_stacker_cleared();assert(tx==before+1);
  assert(app_fault_recovery_start_status()==APP_FAULT_START_BLOCKED);
  app_fault_recovery_confirm(i?batch:full);assert(tx==before+2);reply(1);
 }
 reset();app_fault_recovery_report(door);before=tx;app_fault_recovery_stacker_cleared();assert(tx==before);
 app_fault_recovery_report(empty);assert(app_fault_recovery_prepare_start()==APP_FAULT_START_WAIT);assert(tx==before+1);reply(1);
 reset();before=tx;app_fault_recovery_report(empty);elapsed(10000);assert(tx==before);
 app_fault_recovery_confirm(empty);assert(tx==before+1);reply(1);
 /* Another source cannot erase the batch removal route, even after normal. */
 reset();app_fault_recovery_report(batch);
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2});
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,0});
 before=tx;app_fault_recovery_stacker_cleared();assert(tx==before+1);reply(1);
 /* Pending clear generation cannot clear a newly reported fault. */
 reset();app_fault_recovery_report(full);assert(app_fault_recovery_prepare_start()==APP_FAULT_START_WAIT);
 app_fault_recovery_report(door);reply(1);assert(app_fault_recovery_start_status()==APP_FAULT_START_BLOCKED);
 /* A failed, malformed, unrelated or late response cannot permit START. */
 reset();app_fault_recovery_report(full);fail=true;before=errors;
 assert(app_fault_recovery_prepare_start()==APP_FAULT_START_BLOCKED&&errors==before+1);
 fail=false;assert(app_fault_recovery_prepare_start()==APP_FAULT_START_WAIT);
 uint8_t bad[]={0xFD,0xDF,6,0x3D,1,0};bad[3]=0x06;app_fault_recovery_handle_reply(bad,6);
 bad[3]=0x3D;app_fault_recovery_handle_reply(bad,5);reply(3);
 assert(app_fault_recovery_start_status()==APP_FAULT_START_WAIT);
 elapsed(2000);before=tx;reply(1);elapsed(60000);assert(tx==before&&app_fault_recovery_start_status()==APP_FAULT_START_BLOCKED);
 /* Never clear a machine while a confirmed count is still running. */
 reset();app_fault_recovery_count_started();app_fault_recovery_report(batch);before=tx;
 app_fault_recovery_confirm(batch);app_fault_recovery_stacker_cleared();elapsed(10000);assert(tx==before);
 app_fault_recovery_count_finished();elapsed(0);assert(tx==before+1);reply(1);
 /* Runtime-normal is source-specific; unknown/no-note cannot erase batch. */
 reset();app_fault_recovery_report(batch);app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,0});
 assert(app_fault_recovery_prepare_start()==APP_FAULT_START_WAIT);reply(1);
 for(unsigned source=0;source<=MACHINE_FAULT_BATCH;++source)for(unsigned code=0;code<256;++code){
  reset();auto_popup=true;before=tx;
  machine_fault_key_t key={(machine_fault_source_t)source,2,(uint8_t)code};app_fault_recovery_report(key);
  app_fault_start_state_t state=app_fault_recovery_prepare_start();
  bool known=(source==MACHINE_FAULT_START&&code>=1&&code<=13)||(source==MACHINE_FAULT_RUNTIME&&code>=1&&code<=7)||(source==MACHINE_FAULT_BATCH&&code==4);
  assert(state==(known?APP_FAULT_START_WAIT:APP_FAULT_START_READY));assert(tx==before+(known?1U:0U));
 }
 puts("PASS recovery owner: event-only clearing, held batch/full/door, deferred intent isolation, removal-after-request retry, Confirm/removal, single-flight/ACK/failure/timeout, generation isolation, no-note, duplicate 51, running guard, source-specific clear, unknown whitelist");
}
