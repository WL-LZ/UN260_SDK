#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/protocol/protocol_request.h"
#include "un260/lv_components/smart_island.h"
static uint32_t now;static unsigned tx,shown,resolved,repeated;static bool auto_popup,modal,fail;
static smart_island_fault_phase_cb_t phase;
static machine_fault_key_t key={MACHINE_FAULT_START,2,9};
uint32_t app_clock_uptime_ms(void){return now;}
uint64_t protocol_request_result_time_ms(void){return now;}
bool currency_service_take_switch_rejection_detail(uint8_t command,uint8_t code)
{(void)command;(void)code;return false;}
bool counting_action_start_pending(void){return false;}
bool app_command_runtime_count_start_busy(void){return false;}
void protocol_request_set_start_observer(protocol_request_start_observer_t observer){(void)observer;}
void fault_popup_resolve_setting_rejection(uint8_t command){(void)command;}
void fault_popup_setting_notice_finished(machine_fault_key_t key){(void)key;}
void fault_popup_report_setting_rejection(uint8_t command,uint8_t reason)
{machine_fault_report_event((machine_fault_key_t){MACHINE_FAULT_SETTING,reason,command});}
bool fault_popup_get_auto_enabled(void){return auto_popup;}
bool fault_popup_is_showing(void){return modal;}
void fault_popup_stacker_cleared(void){machine_fault_clear_key((machine_fault_key_t){MACHINE_FAULT_START,2,7});}
bool fault_popup_show_key(machine_fault_key_t k){assert(machine_fault_find(k,NULL));++shown;return true;}
void fault_popup_repeat_notice(machine_fault_key_t k){(void)k;++repeated;}
void fault_popup_resolve_key(machine_fault_key_t k){machine_fault_clear_key(k);++resolved;}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){phase=cb;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t size){assert(cmd==0x3D&&size==1&&*data==1);++tx;return fail?-1:0;}
static void elapsed(uint32_t ms){now+=ms;app_fault_recovery_poll();}
static void fixture(machine_fault_key_t fault,bool automatic)
{
 app_fault_recovery_clear();machine_fault_clear();now=0;modal=false;fail=false;
 auto_popup=automatic;key=fault;machine_fault_report(key);
}
static void test_pocket_modes(void)
{
 const uint8_t codes[]={5,7,8};
 for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic){
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,codes[i]},automatic!=0);
  modal=automatic!=0;unsigned before=tx,old_resolved=resolved;
  app_fault_recovery_report(key);assert(tx==before+1); /* Hidden and Popup ON need no UI action. */
  phase(key,SMART_ISLAND_FAULT_BEGIN);
  assert(app_fault_recovery_request_clear(key)&&tx==before+1); /* One outstanding request. */
  app_fault_recovery_handle_clear_result(1);
  assert(machine_fault_find(key,NULL)&&resolved==old_resolved); /* Latch ACK is not an empty pocket. */
  assert(app_fault_recovery_prepare_start()==!modal&&tx==before+1);
  phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2); /* Legacy completion handshake in both modes. */
  phase(key,SMART_ISLAND_FAULT_BEGIN);phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2);
  app_fault_recovery_stacker_cleared();
  assert(machine_fault_find(key,NULL)==(codes[i]!=7)); /* 0x51 proves only the genuine-note pocket. */
  app_fault_recovery_handle_clear_result(1);assert(tx==before+2&&resolved==old_resolved);
  assert(machine_fault_find(key,NULL)==(codes[i]!=7));
  if(codes[i]==7){phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2);}
 }
}
static void test_pending_coalescing(void)
{
 fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);
 app_fault_recovery_report(key);unsigned before=tx,old_repeated=repeated;
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1);
 now=1000;assert(app_fault_recovery_request_clear(key)&&tx==before+1);
 now=2999;app_fault_recovery_poll();assert(repeated==old_repeated&&tx==before+1);
 now=3000;app_fault_recovery_poll();assert(repeated==old_repeated+1&&tx==before+1);
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2); /* Coalescing did not extend the timeout. */
 app_fault_recovery_handle_clear_result(1);
}
static void test_pocket_send_failure(void)
{
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,7},automatic!=0);modal=automatic!=0;
  unsigned before=tx,old_repeated=repeated,old_shown=shown;
  fail=true;app_fault_recovery_report(key);assert(tx==before+1&&repeated==old_repeated);
  phase(key,SMART_ISLAND_FAULT_BEGIN);assert(tx==before+1&&repeated==old_repeated);
  phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2&&repeated==old_repeated+1);
  phase(key,SMART_ISLAND_FAULT_BEGIN);phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2);
  fail=false;app_fault_recovery_report(key);assert(tx==before+3);
  phase(key,SMART_ISLAND_FAULT_BEGIN);assert(tx==before+3);
  app_fault_recovery_handle_clear_result(2);assert(tx==before+3&&machine_fault_find(key,NULL));
  assert(shown==old_shown+(automatic?1U:0U));
  assert(repeated==old_repeated+(automatic?1U:2U)); /* Failure itself cannot recurse into a send. */
  phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+4);
  app_fault_recovery_handle_clear_result(2);phase(key,SMART_ISLAND_FAULT_END);elapsed(40000);
  assert(tx==before+4&&machine_fault_find(key,NULL)); /* Guide replay alone cannot authorize further retries. */
 }
}
static void test_pocket_timeout_ordering(void)
{
 for(unsigned automatic=0;automatic<2;++automatic)for(unsigned ended=0;ended<2;++ended){
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,7},automatic!=0);modal=automatic!=0;
  unsigned before=tx;app_fault_recovery_report(key);assert(tx==before+1);
  if(ended)phase(key,SMART_ISLAND_FAULT_END); /* END joins the unanswered first transaction. */
  elapsed(3000);assert(tx==before+1&&machine_fault_find(key,NULL));
  phase(key,SMART_ISLAND_FAULT_END);
  assert(tx==before+(ended?1U:2U));
  elapsed(3000);phase(key,SMART_ISLAND_FAULT_END);elapsed(40000);
  assert(tx==before+(ended?1U:2U)&&machine_fault_find(key,NULL));
 }
}
static void test_waiting_pocket(void)
{
 for(unsigned outcome=0;outcome<3;++outcome){
  machine_fault_key_t older={MACHINE_FAULT_RUNTIME,0,2};
  fixture(older,false);app_fault_recovery_report(older);unsigned before=tx,old_resolved=resolved;
  phase(older,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  auto_popup=true;key=(machine_fault_key_t){MACHINE_FAULT_START,2,5};machine_fault_report(key);
  app_fault_recovery_report(key);assert(tx==before+1); /* No island BEGIN: Main may be hidden. */
  if(outcome==0)app_fault_recovery_handle_clear_result(1);
  else if(outcome==1)app_fault_recovery_handle_clear_result(2);
  else elapsed(3000);
  assert(tx==before+2&&resolved==old_resolved+(outcome==0?1U:0U));
  assert(machine_fault_find(older,NULL)==(outcome!=0)&&machine_fault_find(key,NULL));
  app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(key,NULL));
 }
 /* A newer START report with the same key cannot consume the older generation's successful ACK. */
 fixture((machine_fault_key_t){MACHINE_FAULT_START,2,7},false);unsigned before=tx;
 app_fault_recovery_report(key);app_fault_recovery_report(key);assert(tx==before+1);
 app_fault_recovery_handle_clear_result(1);assert(tx==before+2&&machine_fault_find(key,NULL));
 app_fault_recovery_handle_clear_result(1);assert(app_fault_recovery_prepare_start()&&tx==before+2);
 /* 0x51 removes only 07, retains the in-flight transaction, and cannot let its ACK resolve a new jam. */
 fixture((machine_fault_key_t){MACHINE_FAULT_START,2,7},false);before=tx;app_fault_recovery_report(key);
 app_fault_recovery_stacker_cleared();assert(!machine_fault_find(key,NULL));
 key=(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,3};machine_fault_report(key);app_fault_recovery_report(key);
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1);
 app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(key,NULL)&&tx==before+1);
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+2);
 app_fault_recovery_handle_clear_result(1);assert(!machine_fault_find(key,NULL));
 /* An unrelated runtime report must not forget a queued reject-full request. */
 fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;
 app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);
 machine_fault_key_t full={MACHINE_FAULT_START,2,5};machine_fault_report(full);app_fault_recovery_report(full);
 machine_fault_key_t controller={MACHINE_FAULT_RUNTIME,0,7};machine_fault_report(controller);app_fault_recovery_report(controller);
 app_fault_recovery_handle_clear_result(1);assert(tx==before+2&&machine_fault_find(full,NULL));
 /* A replacement START/no-note removes the old pocket record, so queued work must be dropped. */
 fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;
 app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);
 machine_fault_report(full);app_fault_recovery_report(full);
 machine_fault_key_t no_note={MACHINE_FAULT_START,1,2};machine_fault_report(no_note);app_fault_recovery_report(no_note);
 app_fault_recovery_handle_clear_result(1);assert(tx==before+1&&!machine_fault_find(full,NULL));
 /* A recreated A followed by unrelated C must still reject the original A's successful ACK. */
 fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;unsigned old_resolved=resolved;
 machine_fault_key_t a=key;app_fault_recovery_report(a);phase(a,SMART_ISLAND_FAULT_END);
 app_fault_recovery_report(no_note);machine_fault_report(no_note);
 app_fault_recovery_report(a);machine_fault_report(a);
 machine_fault_key_t c={MACHINE_FAULT_PRESET,0,4};app_fault_recovery_report(c);machine_fault_report(c);
 app_fault_recovery_handle_clear_result(1);
 assert(tx==before+1&&resolved==old_resolved&&machine_fault_find(a,NULL)&&machine_fault_find(c,NULL));
 /* A queued send failure presents the pocket in its captured mode despite a later preference/report. */
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;
  app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);
  auto_popup=automatic!=0;machine_fault_report(full);app_fault_recovery_report(full);
  auto_popup=!automatic;machine_fault_report(controller);app_fault_recovery_report(controller);
  unsigned old_shown=shown,old_repeated=repeated;fail=true;
  app_fault_recovery_handle_clear_result(1);
  assert(tx==before+2&&machine_fault_find(full,NULL));
  assert(shown==old_shown+(automatic?1U:0U)&&repeated==old_repeated+(automatic?0U:1U));
  elapsed(40000);assert(tx==before+2);fail=false;
 }
 /* Taking only genuine notes cancels queued 07, and leaves queued reject/both-pocket reports. */
 const uint8_t codes[]={5,7,8};
 for(unsigned i=0;i<sizeof(codes);++i){
  fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;
  app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);
  full=(machine_fault_key_t){MACHINE_FAULT_START,2,codes[i]};machine_fault_report(full);app_fault_recovery_report(full);
  app_fault_recovery_stacker_cleared();app_fault_recovery_handle_clear_result(1);
  assert(tx==before+(codes[i]==7?1U:2U)&&machine_fault_find(full,NULL)==(codes[i]!=7));
 }
}
static void test_latched_popup_modes(void)
{
 for(unsigned automatic=0;automatic<2;++automatic)for(unsigned outcome=0;outcome<3;++outcome){
  app_fault_recovery_clear();machine_fault_clear();modal=false;auto_popup=automatic!=0;
  key=(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2};machine_fault_report(key);app_fault_recovery_report(key);
  unsigned before=tx,old_shown=shown,old_repeated=repeated,old_resolved=resolved;
  phase(key,SMART_ISLAND_FAULT_BEGIN);auto_popup=!automatic;
  phase(key,SMART_ISLAND_FAULT_END);
  assert(tx==before+(automatic?0U:1U)); /* The report owns this animation's clear policy. */
  if(automatic)assert(app_fault_recovery_request_clear(key));
  assert(tx==before+1);
  if(outcome==0)app_fault_recovery_handle_clear_result(1);
  else if(outcome==1)app_fault_recovery_handle_clear_result(2);
  else elapsed(3000);
  assert(tx==before+1); /* A result/timeout never itself retries the command. */
  if(outcome==0){
   assert(!machine_fault_find(key,NULL)&&resolved==old_resolved+1);
   assert(shown==old_shown&&repeated==old_repeated);
  }else{
   assert(machine_fault_find(key,NULL)&&resolved==old_resolved);
   assert(shown==old_shown+(automatic?1U:0U));
   assert(repeated==old_repeated+(automatic?0U:1U));
   phase(key,SMART_ISLAND_FAULT_END);
   assert(tx==before+(automatic?1U:2U));
  }
 }
 /* A newer report cannot overwrite the presentation mode of an older pending clear. */
 for(unsigned outcome=0;outcome<2;++outcome){
  app_fault_recovery_clear();machine_fault_clear();auto_popup=false;
  machine_fault_key_t older={MACHINE_FAULT_RUNTIME,0,2},newer={MACHINE_FAULT_PRESET,0,4};
  key=older;machine_fault_report(older);app_fault_recovery_report(older);
  unsigned before=tx,old_shown=shown,old_repeated=repeated;
  phase(older,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  auto_popup=true;machine_fault_report(newer);app_fault_recovery_report(newer);
  if(outcome==0)app_fault_recovery_handle_clear_result(2);else elapsed(3000);
  assert(tx==before+1&&shown==old_shown&&repeated==old_repeated+1);
  assert(machine_fault_find(older,NULL)&&machine_fault_find(newer,NULL));
  phase(newer,SMART_ISLAND_FAULT_END);assert(tx==before+1);
 }
 /* An explicit inspection of a key other than the current report captures
  * the preference at command dispatch, then keeps it through a later toggle. */
 app_fault_recovery_clear();machine_fault_clear();auto_popup=false;
 key=(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2};machine_fault_report(key);
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,2,9});
 auto_popup=true;unsigned before=tx,old_shown=shown,old_repeated=repeated;
 assert(app_fault_recovery_request_clear(key));auto_popup=false;
 app_fault_recovery_handle_clear_result(2);
 assert(tx==before+1&&shown==old_shown+1&&repeated==old_repeated&&machine_fault_find(key,NULL));
 app_fault_recovery_clear();machine_fault_clear();
}
int main(void){
 app_fault_recovery_init();auto_popup=false;
 machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
 machine_fault_report(jam);app_fault_recovery_report(jam);
 phase(jam,SMART_ISLAND_FAULT_BEGIN);assert(tx==0);
 phase(jam,SMART_ISLAND_FAULT_END);assert(tx==1);
 app_fault_recovery_handle_clear_result(2);assert(repeated==1&&machine_fault_find(jam,NULL));
 phase(jam,SMART_ISLAND_FAULT_END);assert(tx==2);
 app_fault_recovery_handle_clear_result(1);assert(resolved==1&&!machine_fault_find(jam,NULL));
 phase(jam,SMART_ISLAND_FAULT_END);assert(tx==2);
 /* Confirm and the animation share the exact whitelist for 0x3D/01. */
 for(unsigned src=0;src<=MACHINE_FAULT_PRESET;++src)for(unsigned code=0;code<16;++code){
  app_fault_recovery_clear();unsigned before=tx;
  machine_fault_key_t k={(machine_fault_source_t)src,(uint8_t)(src==MACHINE_FAULT_START?2:0),(uint8_t)code};
  assert(app_fault_recovery_request_clear(k));
  bool allowed=(src==MACHINE_FAULT_START&&(code==5||code==7||code==8))||
               (src==MACHINE_FAULT_RUNTIME&&code>=1&&code<=5)||
               (src==MACHINE_FAULT_PRESET&&code==4);
  assert(tx==before+(allowed?1U:0U));
  if(allowed)app_fault_recovery_handle_clear_result(1);
  if(src==MACHINE_FAULT_START){
   app_fault_recovery_clear();before=tx;k.type=1;
   assert(app_fault_recovery_request_clear(k)&&tx==before); /* Type 1 never inherits pocket codes 05/07/08. */
  }
 }
 app_fault_recovery_clear();machine_fault_clear();
 key=(machine_fault_key_t){MACHINE_FAULT_START,1,2};app_fault_recovery_report(key);
 unsigned before=tx;phase(key,SMART_ISLAND_FAULT_END);assert(app_fault_recovery_prepare_start());
 assert(tx==before); /* No banknotes and non-pocket start-response faults need no clear. */
 jam=(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,3};app_fault_recovery_clear();
 machine_fault_report(jam);app_fault_recovery_report(jam);before=tx;
 now=UINT32_MAX-999;
 fail=true;assert(!app_fault_recovery_request_clear(jam)&&tx==before+1);fail=false;
 assert(app_fault_recovery_request_clear(jam)&&tx==before+2);
 elapsed(3000);assert(repeated>=2&&machine_fault_find(jam,NULL));
 modal=true;assert(!app_fault_recovery_prepare_start());modal=false;
 assert(shown==0);
 test_latched_popup_modes();
 test_pocket_modes();test_pending_coalescing();test_pocket_send_failure();test_pocket_timeout_ordering();test_waiting_pocket();
 puts("PASS recovery service: full pockets 05/07/08 auto-handshake in both Popup modes and while hidden; latch ACK retains physical faults; same-key pending coalescing; waiting pocket/generation survives old ACK/failure/timeout; 51 only clears 07; jam/preset policy, no-command faults and send failure retained");
}
