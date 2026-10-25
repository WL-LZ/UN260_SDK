#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/protocol/protocol_request.h"
#include "un260/lv_components/smart_island.h"
static bool report_active;
bool counting_report_transfer_active(void){return report_active;}
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
 now=3000;app_fault_recovery_poll();assert(repeated==old_repeated&&tx==before+1);
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1); /* Timeout does not replay or re-arm END. */
 assert(app_fault_recovery_request_clear(key)&&tx==before+2); /* Explicit retry remains available. */
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
 phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1); /* Waiting for another fault consumed the display budget. */
 assert(app_fault_recovery_request_clear(key)&&tx==before+2);
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
 /* A queued send failure cannot replace a newer, unrelated physical guide. */
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2},false);before=tx;
  app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);
  auto_popup=automatic!=0;machine_fault_report(full);app_fault_recovery_report(full);
  auto_popup=!automatic;machine_fault_report(controller);app_fault_recovery_report(controller);
  unsigned old_shown=shown,old_repeated=repeated;fail=true;
  app_fault_recovery_handle_clear_result(1);
  assert(tx==before+2&&machine_fault_find(full,NULL));
  assert(shown==old_shown&&repeated==old_repeated);
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
   assert(shown==old_shown+(outcome==1&&automatic?1U:0U));
   assert(repeated==old_repeated+(outcome==1&&!automatic?1U:0U));
   phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1); /* Display replay does not authorize another command. */
   assert(app_fault_recovery_request_clear(key)&&tx==before+2); /* Only an explicit retry does. */
   app_fault_recovery_handle_clear_result(1);
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
  assert(tx==before+1&&shown==old_shown&&repeated==old_repeated); /* A late old result cannot cover the current physical report. */
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
static void test_confirmed_pocket_no_replay(void)
{
 const uint8_t codes[]={5,7,8};
 for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic)
 for(unsigned outcome=0;outcome<2;++outcome){
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,codes[i]},automatic!=0);
  unsigned before=tx,old_shown=shown,old_repeated=repeated;
  app_fault_recovery_report(key);assert(tx==before+1);
  assert(app_fault_recovery_request_clear(key)&&tx==before+1);
  machine_fault_acknowledge(key); /* Same ownership boundary as a successful Confirm. */
  phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  if(outcome)app_fault_recovery_handle_clear_result(2);else elapsed(3000);
  machine_fault_record_t record;
  assert(machine_fault_find(key,&record)&&record.acknowledged);
  if(shown!=old_shown||repeated!=old_repeated)
   fprintf(stderr,"confirmed-pocket replay code=%u Popup=%u outcome=%s shown=%u repeated=%u\n",
    codes[i],automatic,outcome?"explicit FAIL":"no response timeout",shown-old_shown,repeated-old_repeated);
  assert(shown==old_shown&&repeated==old_repeated); /* No controller recovery proof and no new event. */
  elapsed(40000);assert(tx==before+1&&machine_fault_find(key,&record)&&record.acknowledged);
  machine_fault_report_event(key);app_fault_recovery_report(key);
  assert(tx==before+2&&machine_fault_find(key,&record)&&!record.acknowledged);
  app_fault_recovery_handle_clear_result(1);
 }
}
static void test_superseded_failure_no_old_presentation(void)
{
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,5},automatic!=0);
  machine_fault_key_t older=key,newer={MACHINE_FAULT_START,2,7};
  app_fault_recovery_report(older);machine_fault_acknowledge(older);
  machine_fault_report_event(newer);app_fault_recovery_report(newer);
  unsigned before=tx,old_shown=shown,old_repeated=repeated;
  app_fault_recovery_handle_clear_result(2);
  assert(tx==before+1&&machine_fault_find(newer,NULL));
  assert(shown==old_shown&&repeated==old_repeated); /* An old untagged FAIL is not a new fault event. */
  app_fault_recovery_handle_clear_result(1);
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,5},automatic!=0);
  older=key;app_fault_recovery_report(older);machine_fault_acknowledge(older);
  machine_fault_report_event(older);app_fault_recovery_report(older);
  before=tx;old_shown=shown;old_repeated=repeated;
  app_fault_recovery_handle_clear_result(2);
  assert(tx==before+1&&machine_fault_find(older,NULL));
  assert(shown==old_shown&&repeated==old_repeated); /* Same-key generation is also superseded. */
  app_fault_recovery_handle_clear_result(1);
 }
}

static const machine_fault_key_t batch_full={MACHINE_FAULT_PRESET,0,4};
static void batch_report(void)
{
 app_fault_recovery_report(batch_full);
 machine_fault_report_event(batch_full);
}
static void test_batch_authorization_and_retry(void)
{
 /* Hidden/cancelled presentation cannot begin recovery by itself. */
 fixture(batch_full,false);batch_report();unsigned before=tx;
 phase(batch_full,SMART_ISLAND_FAULT_CANCEL);elapsed(30000);
 phase(batch_full,SMART_ISLAND_FAULT_BEGIN);elapsed(30000);
 assert(tx==before&&machine_fault_find(batch_full,NULL));
 phase(batch_full,SMART_ISLAND_FAULT_END);assert(tx==before+1);
 phase(batch_full,SMART_ISLAND_FAULT_END);app_fault_recovery_poll();assert(tx==before+1);
 elapsed(2999);assert(tx==before+1);
 elapsed(1);assert(tx==before+2); /* No 3D reply: continue the authorised Batch handshake. */
 for(unsigned retry=0;retry<5;++retry){
  unsigned attempts=tx;batch_report();phase(batch_full,SMART_ISLAND_FAULT_END);
  elapsed(2999);assert(tx==attempts);elapsed(1);assert(tx==attempts+1);
 }
 /* Popup ON has no END authorization; Confirm arms recovery despite read acknowledgement. */
 fixture(batch_full,true);batch_report();before=tx;modal=true;
 phase(batch_full,SMART_ISLAND_FAULT_END);elapsed(30000);assert(tx==before);
 assert(app_fault_recovery_request_clear(batch_full)&&tx==before+1);
 machine_fault_acknowledge(batch_full);modal=false;
 phase(batch_full,SMART_ISLAND_FAULT_END);elapsed(2999);assert(tx==before+1);
 elapsed(1);assert(tx==before+2&&machine_fault_find(batch_full,NULL));
 app_fault_recovery_clear();elapsed(30000);assert(tx==before+2); /* Real START owns this same cancellation. */
 /* Preparing an explicit START preserves the established release-before-START path. */
 fixture(batch_full,false);batch_report();before=tx;
 assert(app_fault_recovery_prepare_start()&&tx==before+1);
 elapsed(3000);assert(tx==before+2);
 app_fault_recovery_handle_clear_result(1);
 assert(machine_fault_find(batch_full,NULL));elapsed(2999);assert(tx==before+2);
 elapsed(1);assert(tx==before+3);app_fault_recovery_stacker_cleared();
 elapsed(30000);assert(tx==before+3);
}
static void test_batch_failure_and_result_ordering(void)
{
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture(batch_full,automatic!=0);batch_report();unsigned before=tx;
  fail=true;assert(!app_fault_recovery_request_clear(batch_full)&&tx==before+1);
  /* A failed transport attempt consumes the same budget, even when polls are frequent. */
  for(unsigned i=0;i<2999;++i)elapsed(1);
  assert(tx==before+1);elapsed(1);assert(tx==before+2);
  fail=false;elapsed(2999);assert(tx==before+2);elapsed(1);assert(tx==before+3);
  elapsed(2500);app_fault_recovery_handle_clear_result(2);assert(tx==before+3);
  elapsed(499);assert(tx==before+3);elapsed(1);assert(tx==before+4);
  /* Repeated 06/04 is a newer controller report; an older untagged ACK cannot erase it. */
  batch_report();app_fault_recovery_handle_clear_result(1);
  assert(machine_fault_find(batch_full,NULL)&&tx==before+4);
  elapsed(2999);assert(tx==before+4);elapsed(1);assert(tx==before+5);
  app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(batch_full,NULL));
  elapsed(2999);assert(tx==before+5);elapsed(1);assert(tx==before+6);
  app_fault_recovery_stacker_cleared();elapsed(30000);assert(tx==before+6);
 }
 /* The deadline uses modular arithmetic through the uptime wrap. */
 fixture(batch_full,false);batch_report();now=UINT32_MAX-999;unsigned before=tx;
 assert(app_fault_recovery_request_clear(batch_full));elapsed(2999);assert(tx==before+1);
 elapsed(1);assert(tx==before+2);app_fault_recovery_stacker_cleared();
}
static void test_batch_physical_clear_and_other_faults(void)
{
 for(unsigned automatic=0;automatic<2;++automatic){
  fixture(batch_full,automatic!=0);batch_report();unsigned before=tx;
  assert(app_fault_recovery_request_clear(batch_full)&&tx==before+1);
  elapsed(3000);assert(tx==before+2); /* Late cash removal still has an active release handshake. */
  app_fault_recovery_stacker_cleared();app_fault_recovery_stacker_cleared();
  assert(!machine_fault_find(batch_full,NULL));elapsed(30000);assert(tx==before+2);
  /* 51 retains the old transaction. A new Batch must not consume its delayed ACK. */
  fixture(batch_full,automatic!=0);batch_report();before=tx;
  assert(app_fault_recovery_request_clear(batch_full));app_fault_recovery_stacker_cleared();
  batch_report();assert(app_fault_recovery_request_clear(batch_full)&&tx==before+1);
  app_fault_recovery_handle_clear_result(1);assert(machine_fault_find(batch_full,NULL)&&tx==before+1);
  elapsed(3000);assert(tx==before+2);app_fault_recovery_stacker_cleared();
  /* A jam terminates only Batch retries; the jam retains its own original END policy. */
  fixture(batch_full,false);batch_report();before=tx;phase(batch_full,SMART_ISLAND_FAULT_END);
  machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
  app_fault_recovery_report(jam);machine_fault_report(jam);elapsed(3000);assert(tx==before+1);
  phase(jam,SMART_ISLAND_FAULT_END);assert(tx==before+2);elapsed(30000);
  phase(jam,SMART_ISLAND_FAULT_END);assert(tx==before+2&&machine_fault_find(jam,NULL));
  assert(machine_fault_find(batch_full,NULL)); /* Do not clear independent records as a retry side effect. */
 }
 /* A different source may already own the untagged transaction when Confirm requests Batch. */
 machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};fixture(jam,false);
 app_fault_recovery_report(jam);unsigned before=tx;phase(jam,SMART_ISLAND_FAULT_END);
 batch_report();assert(!app_fault_recovery_request_clear(batch_full)&&tx==before+1);
 elapsed(2999);assert(tx==before+1);elapsed(1);assert(tx==before+2);
 assert(machine_fault_find(jam,NULL)&&machine_fault_find(batch_full,NULL));
 app_fault_recovery_stacker_cleared();elapsed(30000);assert(tx==before+2);
 /* Other PRESET keys are not allowed to inherit Batch retry behaviour. */
 const machine_fault_key_t other[]={{MACHINE_FAULT_PRESET,1,4},{MACHINE_FAULT_PRESET,0,3}};
 for(unsigned i=0;i<sizeof(other)/sizeof(other[0]);++i){
  fixture(other[i],false);app_fault_recovery_report(other[i]);before=tx;
  phase(other[i],SMART_ISLAND_FAULT_END);assert(app_fault_recovery_request_clear(other[i]));
  elapsed(30000);assert(tx==before);
 }
}


static void test_batch_new_sensor_boot_isolation(void)
{
 const machine_fault_key_t other[]={{MACHINE_FAULT_SENSOR,0,0},{MACHINE_FAULT_BOOT,2,5}};
 for(unsigned i=0;i<sizeof(other)/sizeof(other[0]);++i){
  fixture(batch_full,false);batch_report();unsigned before=tx,old_repeated=repeated,old_shown=shown;
  phase(batch_full,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  machine_fault_report(other[i]);app_fault_recovery_report(other[i]);
  app_fault_recovery_handle_clear_result(2);
  assert(repeated==old_repeated&&shown==old_shown); /* An old Batch FAIL cannot cover the new guide. */
  elapsed(30000);assert(tx==before+1&&machine_fault_find(other[i],NULL));
  phase(batch_full,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  app_fault_recovery_stacker_cleared();assert(!machine_fault_find(batch_full,NULL)&&machine_fault_find(other[i],NULL));
  /* The owner can already be invalid while an older Batch request still awaits its untagged result. */
  fixture(batch_full,false);batch_report();before=tx;old_repeated=repeated;old_shown=shown;
  phase(batch_full,SMART_ISLAND_FAULT_END);
  app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,0});
  machine_fault_report(other[i]);app_fault_recovery_report(other[i]);app_fault_recovery_handle_clear_result(2);
  assert(tx==before+1&&repeated==old_repeated&&shown==old_shown);
  elapsed(30000);assert(tx==before+1);
  /* BOOT/SENSOR presentation reports must leave an unrelated runtime's original recovery eligible. */
  const machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};fixture(jam,false);
  app_fault_recovery_report(jam);machine_fault_report(other[i]);app_fault_recovery_report(other[i]);before=tx;
  phase(jam,SMART_ISLAND_FAULT_END);assert(tx==before+1);
  app_fault_recovery_handle_clear_result(1);
  assert(!machine_fault_find(jam,NULL)&&machine_fault_find(other[i],NULL));
  /* A new full-pocket report keeps the same immediate latch handshake and pending owner. */
  fixture((machine_fault_key_t){MACHINE_FAULT_START,2,7},false);app_fault_recovery_report(key);before=tx;
  machine_fault_report(other[i]);app_fault_recovery_report(other[i]);app_fault_recovery_handle_clear_result(1);
  assert(machine_fault_find(key,NULL)&&machine_fault_find(other[i],NULL));
  phase(key,SMART_ISLAND_FAULT_END);assert(tx==before+1);app_fault_recovery_stacker_cleared();
 }
 app_fault_recovery_clear();machine_fault_clear();
}

int main(void){
 app_fault_recovery_init();auto_popup=false;
 test_batch_new_sensor_boot_isolation();
 test_batch_authorization_and_retry();test_batch_failure_and_result_ordering();test_batch_physical_clear_and_other_faults();
 app_fault_recovery_clear();machine_fault_clear();tx=shown=resolved=repeated=now=0;
 machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
 machine_fault_report(jam);app_fault_recovery_report(jam);
 phase(jam,SMART_ISLAND_FAULT_BEGIN);assert(tx==0);
 phase(jam,SMART_ISLAND_FAULT_END);assert(tx==1);
 app_fault_recovery_handle_clear_result(2);assert(repeated==1&&machine_fault_find(jam,NULL));
 phase(jam,SMART_ISLAND_FAULT_END);assert(tx==1); /* Failure presentation is not a new report. */
 assert(app_fault_recovery_request_clear(jam)&&tx==2);
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
 unsigned old_repeated=repeated;elapsed(3000);assert(repeated==old_repeated&&machine_fault_find(jam,NULL));
 modal=true;assert(!app_fault_recovery_prepare_start());modal=false;
 assert(shown==0);
 /* Batch clear waits for the report that was already sent, not a 3s retry. */
 fixture((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4},false);
 app_fault_recovery_report(key);before=tx;report_active=true;
 phase(key,SMART_ISLAND_FAULT_END);elapsed(100);assert(tx==before);
 report_active=false;elapsed(1);assert(tx==before+1);
 app_fault_recovery_stacker_cleared();elapsed(3001);assert(tx==before+1);
 /* Explicit Confirm after transfer completion consumes the queued send once. */
 fixture(batch_full,false);batch_report();before=tx;report_active=true;
 assert(app_fault_recovery_request_clear(batch_full)&&tx==before);
 report_active=false;assert(app_fault_recovery_request_clear(batch_full)&&tx==before+1);
 app_fault_recovery_handle_clear_result(1);elapsed(1);assert(tx==before+1);
 app_fault_recovery_stacker_cleared();
 /* START/clear and a new unrelated fault cancel a queued old Batch clear. */
 fixture((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4},false);
 app_fault_recovery_report(key);before=tx;report_active=true;
 phase(key,SMART_ISLAND_FAULT_END);app_fault_recovery_stacker_cleared();
 report_active=false;elapsed(1);assert(tx==before);
 fixture((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4},false);
 app_fault_recovery_report(key);report_active=true;phase(key,SMART_ISLAND_FAULT_END);
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,3});
 report_active=false;elapsed(1);assert(tx==before);
 test_latched_popup_modes();
 test_pocket_modes();test_pending_coalescing();test_pocket_send_failure();test_pocket_timeout_ordering();test_waiting_pocket();
 test_confirmed_pocket_no_replay();test_superseded_failure_no_old_presentation();
 puts("PASS recovery service: Batch END/Confirm/prestart authorization, bounded 3s retry without ACK, repeated 06/04, failed sends, superseded ACK, late/duplicate 51 and START cancellation, unrelated-fault isolation;  acknowledged 05/07/08 never replay after timeout/FAIL; superseded FAIL cannot present old generation; timeout releases in-flight only; one END budget with explicit retry; full pockets 05/07/08 auto-handshake in both Popup modes and while hidden; latch ACK retains physical faults; same-key pending coalescing; waiting pocket/generation survives old ACK/failure/timeout; 51 clears only genuine-pocket 07 and Batch; jam policy, no-command faults and send failure retained");
}
