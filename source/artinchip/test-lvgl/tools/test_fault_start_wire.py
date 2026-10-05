"""Production START owner + fault recovery + request tracker, UART captured."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
owner=function((root/'un260/app_service/app_command_runtime.c').read_text(),'app_command_runtime_request_count_start')
code=r'''
#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/app_service/upgrade_session.h"
#include "un260/lv_components/smart_island.h"
#include "un260/counting/counting_action_service.h"
#include "tools/test_maintenance_state.h"
static uint64_t now;static unsigned tx,refresh,resolved,repeated;static uint8_t cmds[64];
static bool history_ready=true,busy,modal;static uint8_t fail_cmd;
static upgrade_session_owner_t session_owner=UPGRADE_SESSION_NONE;
upgrade_session_owner_t upgrade_session_owner(void){return session_owner;}
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
bool counting_history_can_start(void){return history_ready;}
bool fault_popup_get_auto_enabled(void){return false;}
bool fault_popup_is_showing(void){return modal;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;return true;}
void fault_popup_stacker_cleared(void){}
void fault_popup_repeat_notice(machine_fault_key_t key){assert(machine_fault_find(key,NULL));++repeated;}
void fault_popup_resolve_key(machine_fault_key_t key){machine_fault_clear_key(key);++resolved;}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){(void)cb;}
static bool app_command_runtime_count_start_busy(void){return busy||counting_action_start_pending();}
static void page_01_main_refresh_start_state(void){++refresh;}
static void uart_debug_printf(const char *s){(void)s;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t len){assert(len==1&&*data==1&&tx<sizeof(cmds));cmds[tx++]=cmd;return cmd==fail_cmd?-1:6;}
''' + owner + r'''
static void reset(void){
 counting_action_cancel_all();app_fault_recovery_clear();machine_fault_clear();
 now=0;tx=refresh=resolved=repeated=0;history_ready=true;busy=modal=false;fail_cmd=0;
 session_owner=UPGRADE_SESSION_NONE;host_factory_reset_blocked=false;host_touch_calibration_busy=false;
}
static void report(machine_fault_key_t key){
 assert(machine_fault_report(key));app_fault_recovery_report(key);
}
static void finish_start_request(void){
 uint8_t response[]={0xFD,0xDF,7,0x0A,1,2,0};
 counting_action_handle_reply(0x0A,response,sizeof(response));
 assert(!counting_action_start_pending());
}
static void direct_start(machine_fault_key_t key){
 reset();report(key);
 assert(app_command_runtime_request_count_start());
 assert(tx==1&&cmds[0]==0x0A&&refresh==1&&counting_action_start_pending());
 assert(!app_command_runtime_request_count_start()&&tx==1);
 finish_start_request();
 assert(app_command_runtime_request_count_start()&&tx==2&&cmds[1]==0x0A);
}
static void clear_before_start(machine_fault_key_t key){
 reset();report(key);
 fail_cmd=0x3D;
 assert(!app_command_runtime_request_count_start());
 assert(tx==1&&cmds[0]==0x3D&&!refresh&&!counting_action_start_pending());
 fail_cmd=0;
 assert(app_command_runtime_request_count_start());
 assert(tx==3&&cmds[1]==0x3D&&cmds[2]==0x0A&&refresh==1);
 assert(!app_command_runtime_request_count_start()&&tx==3);
 finish_start_request();
 app_fault_recovery_handle_clear_result(1);
 assert(resolved==1&&!machine_fault_find(key,NULL));
 assert(app_command_runtime_request_count_start()&&tx==4&&cmds[3]==0x0A);
}
static void start_gates(void){
 reset();report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,2});
 history_ready=false;assert(!app_command_runtime_request_count_start()&&!tx);history_ready=true;
 busy=true;assert(!app_command_runtime_request_count_start()&&!tx);busy=false;
 modal=true;assert(!app_command_runtime_request_count_start()&&!tx);modal=false;
 assert(counting_action_request_clear()&&tx==1&&cmds[0]==0x3B);
 assert(!app_command_runtime_request_count_start()&&tx==1);
 now=999;assert(counting_action_take_timeouts()==COUNTING_ACTION_TIMEOUT_NONE);
 assert(counting_action_clear_pending()&&!app_command_runtime_request_count_start()&&tx==1);
 now=1000;assert(counting_action_take_timeouts()==COUNTING_ACTION_TIMEOUT_CLEAR);
 assert(!counting_action_clear_pending()&&app_command_runtime_request_count_start());
 assert(tx==3&&cmds[1]==0x3D&&cmds[2]==0x0A);
 now=5999;assert(counting_action_take_timeouts()==COUNTING_ACTION_TIMEOUT_NONE);
 assert(!app_command_runtime_request_count_start()&&tx==3);
 now=6000;assert(counting_action_take_timeouts()==COUNTING_ACTION_TIMEOUT_START);
 assert(!counting_action_start_pending());
 assert(app_command_runtime_request_count_start()&&tx==5&&cmds[3]==0x3D&&cmds[4]==0x0A);
}
static void clear_failures(void){
 machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
 reset();report(jam);assert(app_command_runtime_request_count_start()&&tx==2);
 finish_start_request();
 app_fault_recovery_handle_clear_result(2);
 assert(repeated==1&&machine_fault_find(jam,NULL)&&tx==2);
 assert(app_command_runtime_request_count_start()&&tx==4&&cmds[2]==0x3D&&cmds[3]==0x0A);
 finish_start_request();now=2999;app_fault_recovery_poll();assert(repeated==1&&tx==4);
 now=3000;app_fault_recovery_poll();assert(repeated==2&&machine_fault_find(jam,NULL)&&tx==4);
 assert(app_command_runtime_request_count_start()&&tx==6&&cmds[4]==0x3D&&cmds[5]==0x0A);
 /* A clear response has no fault id, so a different pending key blocks START. */
 reset();report(jam);assert(app_fault_recovery_request_clear(jam)&&tx==1);
 report((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
 assert(!app_command_runtime_request_count_start()&&tx==1&&!counting_action_start_pending());
 app_fault_recovery_handle_clear_result(1);
 assert(resolved==1&&app_command_runtime_request_count_start());
 assert(tx==3&&cmds[1]==0x3D&&cmds[2]==0x0A);
}
static void send_and_clear_guards(void){
 reset();report((machine_fault_key_t){MACHINE_FAULT_START,1,2});fail_cmd=0x0A;
 assert(!app_command_runtime_request_count_start()&&tx==1&&cmds[0]==0x0A);
 assert(!refresh&&!counting_action_start_pending());fail_cmd=0;
 assert(app_command_runtime_request_count_start()&&tx==2&&refresh==1);
 assert(counting_action_request_clear()&&tx==3&&cmds[2]==0x3B);
 assert(!counting_action_start_pending()&&counting_action_clear_pending());
 assert(!app_command_runtime_request_count_start()&&tx==3);
 uint8_t response[]={0xFD,0xDF,6,0x3B,1,0};
 counting_action_handle_reply(0x3B,response,sizeof(response));
 assert(!counting_action_clear_pending()&&app_command_runtime_request_count_start());
 assert(tx==4&&cmds[3]==0x0A);
}
static void upgrade_start_gate(void){
 const upgrade_session_owner_t owners[]={UPGRADE_SESSION_CONTROLLER,UPGRADE_SESSION_IMAGE,UPGRADE_SESSION_UI};
 for(unsigned i=0;i<sizeof(owners)/sizeof(owners[0]);++i){
  reset();machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};report(jam);
  session_owner=owners[i];
  assert(!app_command_runtime_request_count_start()&&!tx&&!refresh&&!counting_action_start_pending());
  assert(machine_fault_find(jam,NULL)); /* Neither 0x3D nor 0x0A is dispatched during an upgrade. */
  session_owner=UPGRADE_SESSION_NONE;
  assert(app_command_runtime_request_count_start());
  assert(tx==2&&cmds[0]==0x3D&&cmds[1]==0x0A&&refresh==1);
 }
}
static void maintenance_start_gate(void){
 bool *owners[]={&host_factory_reset_blocked,&host_touch_calibration_busy};
 for(unsigned i=0;i<sizeof(owners)/sizeof(owners[0]);++i){
  reset();machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};report(jam);
  *owners[i]=true;
  assert(!app_command_runtime_request_count_start()&&!tx&&!refresh&&!counting_action_start_pending());
  assert(machine_fault_find(jam,NULL));
  *owners[i]=false;
  assert(app_command_runtime_request_count_start());
  assert(tx==2&&cmds[0]==0x3D&&cmds[1]==0x0A&&refresh==1);
 }
}
int main(void){
 app_fault_recovery_init();
 /* START faults, including open channels and full pockets, are outside 3D's whitelist. */
 for(uint8_t code=1;code<=13;++code)direct_start((machine_fault_key_t){MACHINE_FAULT_START,2,code});
 direct_start((machine_fault_key_t){MACHINE_FAULT_START,1,2});
 for(uint8_t code=6;code<=7;++code)direct_start((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});
 for(uint8_t code=1;code<=5;++code)clear_before_start((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});
 clear_before_start((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
 start_gates();clear_failures();send_and_clear_guards();upgrade_start_gate();maintenance_start_gate();
 puts("PASS real START wire: door/full/no-note direct 0A; runtime jam/preset 3D before 0A; controller/image/UI upgrade blocks both commands, release resumes; send failure, busy/history/modal/clear guards; 1s Clear/5s Start timeouts; clear reject/timeout and pending-key isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-start-wire-') as d:
 w=Path(d);(w/'lvgl').mkdir();(w/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
 (w/'lv_conf.h').write_text('#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_USE_GPU_AIC 0\n#define LV_USE_GPU_AIC_GE 0\n')
 (w/'test.c').write_text(code)
 sources=['un260/app_service/app_fault_recovery.c','un260/machine_state/machine_fault.c','un260/counting/counting_action_service.c','un260/protocol/protocol_request.c']
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(w),'-I'+str(root),'-DLV_CONF_PATH='+str(w/'lv_conf.h'),str(w/'test.c'),*[str(root/p) for p in sources],'-o',str(w/'test')],check=True)
 subprocess.run([str(w/'test')],check=True)
