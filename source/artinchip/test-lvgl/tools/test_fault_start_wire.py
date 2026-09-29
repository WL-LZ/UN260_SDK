"""Production START owner + fault recovery + request tracker, UART captured."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
source=(root/'un260/app_service/app_command_runtime.c').read_text()
owner=function(source,'app_command_runtime_request_count_start')+'\n'+function(source,'app_command_runtime_poll_recovery_start')
code=r'''
#include <assert.h>
#include <stdio.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/counting/counting_action_service.h"
static uint64_t now;static unsigned tx,refresh;static uint8_t cmds[32];
static bool history_ready=true,busy,modal,fail_ack,g_start_waiting_for_clear;
static ui_page_t g_start_clear_origin, current_page=UI_PAGE_MAIN;
ui_page_t ui_manager_get_current_page(void){return current_page;}
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
bool counting_history_can_start(void){return history_ready;}
bool fault_popup_get_auto_enabled(void){return false;}
bool fault_popup_is_showing(void){return modal;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;return true;}
void fault_popup_stacker_cleared(void){}
void ui_notice_clear(const char *s){(void)s;}
void ui_notice_post_text(ui_notice_kind_t k,const char*a,const char*b,const char*c){(void)k;(void)a;(void)b;(void)c;}
static bool app_command_runtime_count_start_busy(void){return g_start_waiting_for_clear||busy||counting_action_start_pending();}
static void page_01_main_refresh_start_state(void){++refresh;}
static void uart_debug_printf(const char *s){(void)s;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t len){assert(len==1&&*data==1);cmds[tx++]=cmd;return fail_ack&&cmd==0x3D?-1:6;}
''' + owner + r'''
static void ack(uint8_t result){uint8_t b[]={0xFD,0xDF,6,0x3D,result,0};app_fault_recovery_handle_reply(b,6);app_command_runtime_poll_recovery_start();}
int main(void){
 machine_fault_key_t full={MACHINE_FAULT_START,2,7};
 app_fault_recovery_init();app_fault_recovery_report(full);
 history_ready=false;assert(!app_command_runtime_request_count_start()&&!tx);
 history_ready=true;busy=true;assert(!app_command_runtime_request_count_start()&&!tx);busy=false;
 modal=true;assert(!app_command_runtime_request_count_start()&&!tx);modal=false;
 fail_ack=true;assert(!app_command_runtime_request_count_start()&&tx==1&&cmds[0]==0x3D&&!counting_action_start_pending());
 fail_ack=false;assert(app_command_runtime_request_count_start());assert(tx==2&&cmds[1]==0x3D&&g_start_waiting_for_clear);
 assert(!app_command_runtime_request_count_start()&&tx==2);app_command_runtime_poll_recovery_start();assert(tx==2);
 ack(1);assert(tx==3&&cmds[2]==0x0A&&!g_start_waiting_for_clear);
 counting_action_cancel_all();app_fault_recovery_report(full);assert(app_command_runtime_request_count_start());ack(2);
 assert(tx==4&&!g_start_waiting_for_clear&&!counting_action_start_pending());
 assert(app_command_runtime_request_count_start());now+=2000;app_fault_recovery_poll();app_command_runtime_poll_recovery_start();
 assert(tx==5&&!g_start_waiting_for_clear);ack(1);assert(tx==5);
 assert(app_command_runtime_request_count_start());history_ready=false;ack(1);assert(tx==6&&!g_start_waiting_for_clear);history_ready=true;
 app_fault_recovery_report(full);assert(app_command_runtime_request_count_start());current_page=UI_PAGE_MENU;ack(1);assert(tx==7&&!g_start_waiting_for_clear);current_page=UI_PAGE_MAIN;
 /* Auto recovery has no explicit START intent: never manufacture one. */
 app_fault_recovery_report(full);app_fault_recovery_stacker_cleared();ack(1);assert(tx==8&&cmds[7]==0x3D);
 assert(app_command_runtime_request_count_start());assert(tx==9&&cmds[8]==0x0A);
 counting_action_cancel_all();app_fault_recovery_report(full);assert(counting_action_request_clear());
 unsigned before=tx;assert(!app_command_runtime_request_count_start()&&tx==before);
 puts("PASS real START owner: one explicit intent, 3D ACK before 0A, failure/timeout/late response, busy/history/modal/clear guards, removal/Confirm never auto-send START");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-start-wire-') as d:
 w=Path(d);(w/'lvgl').mkdir();(w/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
 (w/'lv_conf.h').write_text('#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_USE_GPU_AIC 0\n#define LV_USE_GPU_AIC_GE 0\n')
 (w/'test.c').write_text(code)
 sources=['un260/app_service/app_fault_recovery.c','un260/machine_state/machine_fault.c','un260/counting/counting_action_service.c','un260/protocol/protocol_request.c','un260/protocol/protocol_frame.c']
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(w),'-I'+str(root),'-DLV_CONF_PATH='+str(w/'lv_conf.h'),str(w/'test.c'),*[str(root/p) for p in sources],'-o',str(w/'test')],check=True)
 subprocess.run([str(w/'test')],check=True)
