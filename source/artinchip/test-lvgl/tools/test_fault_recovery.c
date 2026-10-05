#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/lv_components/smart_island.h"
static uint32_t now;static unsigned tx,shown,resolved,repeated;static bool auto_popup,modal,fail;
static smart_island_fault_phase_cb_t phase;
static machine_fault_key_t key={MACHINE_FAULT_START,2,9};
uint32_t app_clock_uptime_ms(void){return now;}
bool fault_popup_get_auto_enabled(void){return auto_popup;}
bool fault_popup_is_showing(void){return modal;}
void fault_popup_stacker_cleared(void){}
bool fault_popup_show_key(machine_fault_key_t k){assert(machine_fault_key_equal(k,key));++shown;return true;}
void fault_popup_repeat_notice(machine_fault_key_t k){(void)k;++repeated;}
void fault_popup_resolve_key(machine_fault_key_t k){machine_fault_clear_key(k);++resolved;}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){phase=cb;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t size){assert(cmd==0x3D&&size==1&&*data==1);++tx;return fail?-1:0;}
static void elapsed(uint32_t ms){now+=ms;app_fault_recovery_poll();}
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
  bool allowed=(src==MACHINE_FAULT_RUNTIME&&code>=1&&code<=5)||
               (src==MACHINE_FAULT_PRESET&&code==4);
  assert(tx==before+(allowed?1U:0U));
  if(allowed)app_fault_recovery_handle_clear_result(1);
 }
 app_fault_recovery_clear();machine_fault_clear();
 key=(machine_fault_key_t){MACHINE_FAULT_START,1,2};app_fault_recovery_report(key);
 unsigned before=tx;phase(key,SMART_ISLAND_FAULT_END);assert(app_fault_recovery_prepare_start());
 assert(tx==before); /* No banknotes and start-response faults need no clear. */
 jam=(machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,3};app_fault_recovery_clear();
 machine_fault_report(jam);app_fault_recovery_report(jam);before=tx;
 now=UINT32_MAX-999;
 fail=true;assert(!app_fault_recovery_request_clear(jam)&&tx==before+1);fail=false;
 assert(app_fault_recovery_request_clear(jam)&&tx==before+2);
 elapsed(3000);assert(repeated>=2&&machine_fault_find(jam,NULL));
 modal=true;assert(!app_fault_recovery_prepare_start());modal=false;
 assert(shown==0);
 test_latched_popup_modes();
 puts("PASS recovery service: jam/preset whitelist, report mode latched across Popup toggles, pending mode survives new reports and late failure/timeout, success-only resolve, no-command faults and send failure");
}
