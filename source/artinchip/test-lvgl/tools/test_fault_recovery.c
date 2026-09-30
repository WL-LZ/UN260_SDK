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
 puts("PASS recovery service: jam/preset clear whitelist, no-command faults, failure, result and timeout");
}
