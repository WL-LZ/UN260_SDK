"""Production START owner + fault recovery + request tracker, UART captured."""
from pathlib import Path
import subprocess,tempfile
from test_main_view import function
root=Path(__file__).resolve().parents[1]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
owner=function((root/'un260/app_service/app_command_runtime.c').read_text(),'app_command_runtime_request_count_start')
control_source=(root/'un260/app_service/app_counting_runtime.c').read_text()
control_owners='\n'.join(function(control_source,name) for name in
 ('app_counting_runtime_on_start_failure','app_counting_runtime_on_runtime_fault'))
control_dispatch_owner=function(control_source,'app_counting_runtime_handle_control')
basic_source=(root/"un260/app_service/app_setting_reply_basic.c").read_text()
start=basic_source.index("case 0x04:")
from test_currency_modes import block
basic_mode="app_setting_reply_action_t app_setting_reply_handle_basic(uint8_t cmd,const uint8_t *buf,uint8_t len){app_setting_reply_action_t actions=0;switch(cmd){case 0x04:"+block(basic_source,basic_source.index("{",start))+"break;default:break;}return actions;}"
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/app_fault_recovery.h"
#include "un260/app_service/setting_service.h"
#include "un260/protocol/mode_codec.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/app_service/app_setting_reply.h"
#include "un260/app_service/upgrade_session.h"
#include "un260/lv_components/smart_island.h"
#include "un260/counting/counting_action_service.h"
#include "un260/counting/counting_control_reply.h"
#include "un260/counting/counting_history_service.h"
#include "un260/data_collection/data_collection.h"
#include "un260/app_service/work_mode_service.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_text.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/machine_state/machine_state.h"
#include "tools/test_maintenance_state.h"
static uint64_t now;static unsigned tx,refresh,resolved,repeated;static uint8_t cmds[64];
static bool history_ready=true,busy,modal,auto_popup;static uint8_t fail_cmd;
static unsigned controller_starts,operation_guides;
static counting_session_state_t race_session;
static upgrade_session_owner_t session_owner=UPGRADE_SESSION_NONE;
upgrade_session_owner_t upgrade_session_owner(void){return session_owner;}
uint64_t app_clock_monotonic_ms(void){return now;}
uint32_t app_clock_uptime_ms(void){return (uint32_t)now;}
bool counting_history_can_start(void){return history_ready;}
bool fault_popup_get_auto_enabled(void){return auto_popup;}
bool fault_popup_is_showing(void){return modal;}
bool fault_popup_show_key(machine_fault_key_t key){(void)key;return true;}
void fault_popup_stacker_cleared(void){machine_fault_clear_key((machine_fault_key_t){MACHINE_FAULT_START,2,7});modal=false;}
void fault_popup_repeat_notice(machine_fault_key_t key){assert(machine_fault_find(key,NULL));++repeated;}
void fault_popup_resolve_key(machine_fault_key_t key){machine_fault_clear_key(key);++resolved;}
void smart_island_register_fault_phase_cb(smart_island_fault_phase_cb_t cb){(void)cb;}
bool app_command_runtime_count_start_busy(void){return busy||counting_action_start_pending()||race_session.start_confirmed||race_session.phase==COUNTING_SESSION_ACTIVE;}
static void page_01_main_refresh_start_state(void){++refresh;}
void uart_debug_printf(const char *s,...){(void)s;}
int protocol_send(uint8_t cmd,const uint8_t *data,uint16_t len){assert(len==1&&data&&tx<sizeof(cmds));if(cmd==0x3D||cmd==0x0A)assert(*data==1);cmds[tx++]=cmd;return cmd==fail_cmd?-1:6;}
bool diagnostic_calibration_feed_failed(uint8_t type,uint8_t code){(void)type;(void)code;return false;}
void work_mode_service_hold_operation(uint32_t owner,bool active){(void)owner;(void)active;assert(false);}
void cis_calib_ui_refresh(void){assert(false);}
const char *get_counting_error_desc(uint8_t type,uint8_t code){(void)type;return machine_start_error_desc(code);}
const char *get_system_error_desc(uint8_t code){return machine_runtime_error_desc(code);}
void fault_popup_report_start_fault(uint8_t type,uint8_t code){machine_fault_report_event((machine_fault_key_t){MACHINE_FAULT_START,type,code});modal=auto_popup;}
void fault_popup_report_start_no_note(void){fault_popup_report_start_fault(1,2);}
void fault_popup_report_runtime_fault(uint8_t code){machine_fault_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});modal=auto_popup;}
void fault_popup_record_runtime_notice(uint8_t code){machine_fault_report((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});}
bool currency_service_take_switch_rejection_detail(uint8_t command,uint8_t code)
{(void)command;(void)code;return false;}
void fault_popup_resolve_setting_rejection(uint8_t command){(void)command;}
void fault_popup_setting_notice_finished(machine_fault_key_t key){(void)key;}
void fault_popup_report_setting_rejection(uint8_t command,uint8_t reason)
{assert(command==4&&reason==0x80);++operation_guides;assert(!machine_fault_count());}
void fault_popup_clear_runtime(void){machine_fault_clear_source(MACHINE_FAULT_START);machine_fault_clear_source(MACHINE_FAULT_RUNTIME);modal=false;}
void smart_island_restore_idle(void){}
const char *ui_text_msgid(ui_text_id_t id){(void)id;return "Count error";}
void smart_island_notify_warning_level(const char *text,smart_island_warning_level_t level){(void)text;(void)level;}
data_collect_mode_t data_collection_state_mode(void){return DATA_COLLECT_MODE_NONE;}
void data_collection_state_set_message(const ui_message_t *message){(void)message;assert(false);}
void page_06_data_collection_refresh(void){assert(false);}
void ui_message_text(ui_message_t *message,const char *key,const char *argument){(void)message;(void)key;(void)argument;assert(false);}
void ui_message_uint(ui_message_t *message,const char *key,uint32_t value){(void)message;(void)key;(void)value;assert(false);}
void ui_notice_post_message(ui_notice_kind_t kind,const char *key,const char *title,const ui_message_t *message){(void)kind;(void)key;(void)title;(void)message;assert(false);}
bool counting_history_discard_pending(counting_session_state_t *session){(void)session;return false;}
static void control_error(const char *tag,const uint8_t *buf,uint8_t len){(void)tag;(void)buf;(void)len;}
static void control_started(const uint8_t *buf,uint8_t len){(void)buf;(void)len;++controller_starts;app_fault_recovery_clear();fault_popup_clear_runtime();}
static void page_01_mode_switch_refre(void){}
static void page_01_bottom_a_refresh_mode(bool animate){(void)animate;}
static void page_01_curr_img_refre(void){}
void smart_island_refresh_summary(void){}
#define UI_DATA_TOPIC_MACHINE_SETTINGS 1U
static void ui_manager_publish_data_changed(uint32_t topic){(void)topic;}
bool work_mode_service_request(uint8_t mode){(void)mode;return false;}
static void app_setting_notice_result(const char *key,const char *title,bool success)
{(void)title;assert(!strcmp(key,"settings.mode"));if(success)app_fault_recovery_resolve_setting_rejection(4);else assert(app_fault_recovery_report_setting_rejection(4));}
''' + control_owners + r'''
static const counting_control_reply_hooks_t g_counting_control_hooks={
 .on_start_success=control_started,.on_error_frame=control_error,
 .on_start_failure=app_counting_runtime_on_start_failure,.on_runtime_fault=app_counting_runtime_on_runtime_fault
};
''' + control_dispatch_owner + '\n' + owner + '\n' + basic_mode + r'''
static void reset(void){
 counting_action_cancel_all();setting_service_cancel_all();app_fault_recovery_clear();machine_fault_clear();
 memset(&race_session,0,sizeof(race_session));operation_guides=0;
 now=0;tx=refresh=resolved=repeated=controller_starts=0;history_ready=true;busy=modal=auto_popup=false;fail_cmd=0;
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
 bool batch=key.source==MACHINE_FAULT_PRESET&&key.type==0&&key.code==4;
 if(batch)assert(!resolved&&machine_fault_find(key,NULL)); /* ACK only releases the Batch latch. */
 else assert(resolved==1&&!machine_fault_find(key,NULL));
 assert(app_command_runtime_request_count_start()&&tx==4&&cmds[3]==0x0A);
 if(batch){
  app_fault_recovery_stacker_cleared();assert(resolved==1&&!machine_fault_find(key,NULL));
  unsigned before=tx;app_fault_recovery_stacker_cleared();now+=30000;app_fault_recovery_poll();
  assert(tx==before&&resolved==1); /* Physical recovery stops Batch retries; duplicate 51 is inert. */
 }
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
 assert(app_command_runtime_request_count_start()&&tx==4&&cmds[3]==0x0A); /* Existing 3D is coalesced. */
 finish_start_request();app_fault_recovery_poll();
 assert(app_command_runtime_request_count_start()&&tx==6&&cmds[4]==0x3D&&cmds[5]==0x0A);
}
static void clear_failures(void){
 machine_fault_key_t jam={MACHINE_FAULT_RUNTIME,0,2};
 reset();report(jam);assert(app_command_runtime_request_count_start()&&tx==2);
 finish_start_request();
 app_fault_recovery_handle_clear_result(2);
 assert(repeated==1&&machine_fault_find(jam,NULL)&&tx==2);
 assert(app_command_runtime_request_count_start()&&tx==4&&cmds[2]==0x3D&&cmds[3]==0x0A);
 finish_start_request();now=2999;app_fault_recovery_poll();assert(repeated==1&&tx==4);
 now=3000;app_fault_recovery_poll();assert(repeated==1&&machine_fault_find(jam,NULL)&&tx==4); /* No ACK is not a new fault. */
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
static void full_pocket_control(void){
 const uint8_t codes[]={5,7,8};
 for(unsigned i=0;i<sizeof(codes);++i)for(unsigned automatic=0;automatic<2;++automatic){
  reset();auto_popup=automatic!=0;counting_session_state_t session={0};
  const uint8_t full[]={0xFD,0xDF,7,0x0A,2,codes[i],0};
  machine_fault_key_t key={MACHINE_FAULT_START,2,codes[i]};
  app_counting_runtime_handle_control(0x0A,&session,full,sizeof(full));
  assert(tx==1&&cmds[0]==0x3D&&modal==(automatic!=0)); /* RX path, no Confirm/CLEAR/START request. */
  assert(machine_fault_find(key,NULL)&&!counting_action_start_pending());
  app_counting_runtime_handle_control(0x0A,&session,full,sizeof(full));
  assert(tx==1); /* Repeated reports coalesce the pending wire transaction. */
  app_fault_recovery_handle_clear_result(1);
  assert(tx==2&&cmds[1]==0x3D&&!resolved&&machine_fault_find(key,NULL)); /* New report generation needs its own ACK. */
  app_fault_recovery_handle_clear_result(1);
  assert(tx==2&&!resolved&&machine_fault_find(key,NULL)&&modal==(automatic!=0));
  const uint8_t recovered[]={0xFD,0xDF,6,0x0F,0,0};
  app_counting_runtime_handle_control(0x0F,&session,recovered,sizeof(recovered));
  assert(tx==2&&!machine_fault_find(key,NULL)&&!modal);
  app_counting_runtime_handle_control(0x0A,&session,full,sizeof(full));assert(tx==3&&cmds[2]==0x3D);
  const uint8_t started[]={0xFD,0xDF,7,0x0A,1,1,0};
  app_counting_runtime_handle_control(0x0A,&session,started,sizeof(started));
  assert(controller_starts==1&&session.start_confirmed&&!machine_fault_count()&&!modal&&tx==3);
  app_fault_recovery_handle_clear_result(1);assert(tx==3&&!machine_fault_count());
 }
 for(uint8_t code=1;code<=13;++code)if(code!=5&&code!=7&&code!=8){
  reset();counting_session_state_t session={0};const uint8_t other[]={0xFD,0xDF,7,0x0A,2,code,0};
  app_counting_runtime_handle_control(0x0A,&session,other,sizeof(other));assert(!tx);
 }
 reset();counting_session_state_t session={0};const uint8_t no_note[]={0xFD,0xDF,7,0x0A,1,2,0};
 app_counting_runtime_handle_control(0x0A,&session,no_note,sizeof(no_note));assert(!tx);
}

static void setting_ack_after_real_start(void)
{
 reset();machine_state_confirm_mode(MODE_MDC);
 assert(setting_service_request_mode(MODE_SDC)&&setting_service_mode_is_pending()&&tx==1&&cmds[0]==4);
 const uint8_t started[]={0xFD,0xDF,7,0x0A,1,1,0};
 app_counting_runtime_handle_control(0x0A,&race_session,started,sizeof(started));
 assert(race_session.start_confirmed&&setting_service_mode_is_pending());
 const uint8_t negative[]={0xFD,0xDF,6,4,2,0};
 assert(app_setting_reply_handle_basic(4,negative,sizeof(negative))==APP_SETTING_REPLY_ACTION_NONE);
 assert(!setting_service_mode_is_pending()&&machine_state_mode()==MODE_MDC&&!operation_guides&&!machine_fault_count());
 app_fault_recovery_poll();assert(!operation_guides);
 race_session.start_confirmed=false;race_session.phase=COUNTING_SESSION_FINISHED_WAIT_START;
 app_fault_recovery_poll();assert(operation_guides==1&&tx==1);
 reset();machine_state_confirm_mode(MODE_MDC);assert(setting_service_request_mode(MODE_SDC));
 app_counting_runtime_handle_control(0x0A,&race_session,started,sizeof(started));
 const uint8_t success[]={0xFD,0xDF,6,4,1,0};
 app_setting_reply_handle_basic(4,success,sizeof(success));
 assert(!setting_service_mode_is_pending()&&machine_state_mode()==MODE_SDC&&!operation_guides);
}

int main(void){
 app_fault_recovery_init();
 setting_ack_after_real_start();
 /* Non-pocket START faults preserve their direct explicit START behavior. */
 for(uint8_t code=1;code<=13;++code)if(code!=5&&code!=7&&code!=8)direct_start((machine_fault_key_t){MACHINE_FAULT_START,2,code});
 direct_start((machine_fault_key_t){MACHINE_FAULT_START,1,2});
 for(uint8_t code=6;code<=7;++code)direct_start((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});
 for(uint8_t code=1;code<=5;++code)clear_before_start((machine_fault_key_t){MACHINE_FAULT_RUNTIME,0,code});
 clear_before_start((machine_fault_key_t){MACHINE_FAULT_PRESET,0,4});
 start_gates();clear_failures();send_and_clear_guards();upgrade_start_gate();maintenance_start_gate();full_pocket_control();
 puts("PASS production CONTROL+START wire: real 0A success preserves pending04; late04/02 defers presentation without committing, late04/01 still confirms; RX full pockets 05/07/08 auto-send only 3D in both Popup modes without Confirm; repeated pending reports coalesce, old ACK cannot clear new generation; latch ACK retains physical fault until 0F00/real START; non-pocket direct 0A, jam policy, Batch ACK retains record until 51 and stops retries, upgrade/maintenance gates, send failure and timeouts retained");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-start-wire-') as d:
 w=Path(d);(w/'lvgl').mkdir();(w/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
 (w/'lv_conf.h').write_text('#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_USE_GPU_AIC 0\n#define LV_USE_GPU_AIC_GE 0\n')
 (w/'test.c').write_text(code)
 sources=['un260/app_service/app_fault_recovery.c','un260/machine_state/machine_fault.c','un260/machine_state/machine_state.c','un260/counting/counting_control_reply.c','un260/counting/counting_action_service.c','un260/protocol/protocol_request.c','un260/protocol/mode_codec.c','un260/app_service/setting_service.c']
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie','-I'+str(w),'-I'+str(root),'-DLV_CONF_PATH='+str(w/'lv_conf.h'),str(w/'test.c'),*[str(root/p) for p in sources],'-o',str(w/'test')],check=True)
 subprocess.run([str(w/'test')],check=True)
