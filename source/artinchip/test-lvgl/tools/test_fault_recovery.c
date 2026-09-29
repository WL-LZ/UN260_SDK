#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/lv_components/smart_island.h"
static uint32_t now;static unsigned tx,shown;static bool auto_popup,modal,fail;
static smart_island_fault_phase_cb_t phase;
static machine_fault_key_t key={MACHINE_FAULT_START,2,9};
uint32_t app_clock_uptime_ms(void){return now;}
bool fault_popup_get_auto_enabled(void){return auto_popup;}
bool fault_popup_is_showing(void){return modal;}
void fault_popup_stacker_cleared(void){}
bool fault_popup_show_key(machine_fault_key_t k){assert(machine_fault_key_equal(k,key));++shown;return true;}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){phase=cb;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t size){assert(cmd==0x3D&&size==1&&*data==1);++tx;return fail?-1:0;}
static void elapsed(uint32_t ms){now+=ms;app_fault_recovery_poll();}
int main(void){
 app_fault_recovery_init();app_fault_recovery_report(key);
 for(unsigned i=0;i<3;++i){phase(key,SMART_ISLAND_FAULT_BEGIN);assert(tx==i);phase(key,SMART_ISLAND_FAULT_END);elapsed(1999);assert(tx==i);elapsed(1);assert(tx==i+1);app_fault_recovery_report(key);}
 phase(key,SMART_ISLAND_FAULT_END);elapsed(2000);assert(tx==3&&shown==1);
 /* No endless retry: only a fresh report/presentation can arm a new cycle. */
 elapsed(60000);assert(tx==3&&shown==1);
 app_fault_recovery_clear();phase(key,SMART_ISLAND_FAULT_END);elapsed(2000);assert(tx==3);
 app_fault_recovery_report(key);phase(key,SMART_ISLAND_FAULT_END);phase(key,SMART_ISLAND_FAULT_CANCEL);elapsed(2000);assert(tx==3);
 phase(key,SMART_ISLAND_FAULT_END);auto_popup=true;elapsed(2000);assert(tx==3);
 /* Explicit Start is independent of popup Confirm and has no automatic retries. */
 fail=true;assert(!app_fault_recovery_prepare_start());assert(tx==4);
 fail=false;assert(app_fault_recovery_prepare_start());assert(tx==5);
 assert(app_fault_recovery_prepare_start());assert(tx==5);
 app_fault_recovery_report(key);modal=true;assert(!app_fault_recovery_prepare_start());assert(tx==5);modal=false;
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,1,2});assert(app_fault_recovery_prepare_start());assert(tx==5);
 for(unsigned src=0;src<4;++src)for(unsigned code=0;code<256;++code){
  app_fault_recovery_clear();unsigned before=tx;
  machine_fault_key_t k={(machine_fault_source_t)src,2,(uint8_t)code};app_fault_recovery_report(k);
  phase(k,SMART_ISLAND_FAULT_BEGIN);phase(k,SMART_ISLAND_FAULT_END);elapsed(2000);assert(tx==before);
  assert(app_fault_recovery_prepare_start());
  bool allowed=(src==MACHINE_FAULT_START&&code>=1&&code<=13)||(src==MACHINE_FAULT_RUNTIME&&code>=1&&code<=7);
  assert(tx==before+(allowed?1U:0U));
 }
 auto_popup=false;app_fault_recovery_clear();key=(machine_fault_key_t){MACHINE_FAULT_START,2,7};app_fault_recovery_report(key);unsigned before=tx;
 for(unsigned i=0;i<5;++i){phase(key,SMART_ISLAND_FAULT_BEGIN);phase(key,SMART_ISLAND_FAULT_END);app_fault_recovery_report(key);}assert(tx==before+10);
 app_fault_recovery_clear();key=(machine_fault_key_t){MACHINE_FAULT_START,2,9};app_fault_recovery_report(key);now=UINT32_MAX-999;phase(key,SMART_ISLAND_FAULT_END);before=tx;elapsed(2000);assert(tx==before+1);
 puts("PASS recovery service: legacy cadence, bounded retry, no-note/unknown isolation, explicit Start, transport failure, cancellation, wraparound");
}
