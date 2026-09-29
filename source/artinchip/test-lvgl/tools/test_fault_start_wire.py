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
#include "un260/lv_components/smart_island.h"
#include "un260/counting/counting_action_service.h"
static uint64_t now;static unsigned tx,refresh;static uint8_t cmds[32];
static bool history_ready=true,busy,modal,fail_ack;
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
bool counting_history_can_start(void){return history_ready;}
bool fault_popup_get_auto_enabled(void){return false;}
bool fault_popup_is_showing(void){return modal;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;return true;}
void fault_popup_stacker_cleared(void){}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){(void)cb;}
static bool app_command_runtime_count_start_busy(void){return busy||counting_action_start_pending();}
static void page_01_main_refresh_start_state(void){++refresh;}
static void uart_debug_printf(const char *s){(void)s;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t len){assert(len==1&&*data==1);cmds[tx++]=cmd;return fail_ack&&cmd==0x3D?-1:6;}
''' + owner + r'''
int main(void){
 machine_fault_key_t full={MACHINE_FAULT_START,2,7};
 app_fault_recovery_init();app_fault_recovery_report(full);
 history_ready=false;assert(!app_command_runtime_request_count_start()&&!tx);
 history_ready=true;busy=true;assert(!app_command_runtime_request_count_start()&&!tx);busy=false;
 modal=true;assert(!app_command_runtime_request_count_start()&&!tx);modal=false;
 fail_ack=true;assert(!app_command_runtime_request_count_start()&&tx==1&&cmds[0]==0x3D&&!counting_action_start_pending());
 fail_ack=false;assert(app_command_runtime_request_count_start());assert(tx==3&&cmds[1]==0x3D&&cmds[2]==0x0A&&refresh==1);
 assert(!app_command_runtime_request_count_start()&&tx==3);
 uint8_t response[]={0xFD,0xDF,7,0x0A,1,2,0};counting_action_handle_reply(0x0A,response,7);
 app_fault_recovery_report((machine_fault_key_t){MACHINE_FAULT_START,1,2});
 assert(app_command_runtime_request_count_start());assert(tx==4&&cmds[3]==0x0A);
 now=5000;assert(counting_action_take_timeouts()==COUNTING_ACTION_TIMEOUT_START);
 app_fault_recovery_report(full);app_fault_recovery_stacker_cleared();
 assert(app_command_runtime_request_count_start());assert(tx==5&&cmds[4]==0x0A);
 counting_action_cancel_all();app_fault_recovery_report(full);assert(counting_action_request_clear());
 unsigned before=tx;assert(!app_command_runtime_request_count_start()&&tx==before);
 puts("PASS real START wire: 3D before 0A, failure/busy/history/modal/clear guards, no-note direct Start, 5s timeout unchanged, 51 clears latch");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-start-wire-') as d:
 w=Path(d);(w/'lvgl').mkdir();(w/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
 (w/'lv_conf.h').write_text('#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_USE_GPU_AIC 0\n#define LV_USE_GPU_AIC_GE 0\n')
 (w/'test.c').write_text(code)
 sources=['un260/app_service/app_fault_recovery.c','un260/machine_state/machine_fault.c','un260/counting/counting_action_service.c','un260/protocol/protocol_request.c']
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(w),'-I'+str(root),'-DLV_CONF_PATH='+str(w/'lv_conf.h'),str(w/'test.c'),*[str(root/p) for p in sources],'-o',str(w/'test')],check=True)
 subprocess.run([str(w/'test')],check=True)
