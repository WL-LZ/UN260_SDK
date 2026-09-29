"""Exercise the production app notice owner and malformed FO reply boundary."""
from pathlib import Path
import subprocess
import tempfile
import os

root = Path(__file__).resolve().parents[1]
source = (root / 'un260/app_service/app_ui_runtime.c').read_text()
owner = source[source.index('/* User-requested work only:'):source.index('void app_ui_runtime_init(')]
reply = (root / 'un260/app_service/app_setting_reply_basic.c').read_text()
begin = reply.index('    case 0x3A:')
brace = reply.index('{', begin)
end, depth = brace + 1, 1
while depth:
    if reply[end] == '{': depth += 1
    elif reply[end] == '}': depth -= 1
    end += 1
fo = 'static void route_fo(const uint8_t *buf, int len) { switch (0x3A) {\n' + reply[begin:end] + '\n} }\n'
fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "un260/app_service/app_ui_runtime.h"
#include "un260/app_service/workspace_service.h"
#include "un260/lv_components/ui_notice.h"
static bool ws_busy, ws_ok=true, records_busy, records_ok=true, applying;
static workspace_apply_result_t profile_result=WORKSPACE_APPLY_IDLE;
bool workspace_store_busy(void){return ws_busy;}
bool workspace_store_last_success(void){return ws_ok;}
const char *workspace_store_message(void){return ws_ok?"Saved":"Storage failed";}
const char *workspace_service_batch_save_message(void){return "Cycle saved; active Batch awaits confirmation";}
bool cashbook_store_busy(void){return records_busy;}
bool cashbook_store_last_success(void){return records_ok;}
const char *cashbook_store_message(void){return records_ok?"Exported":"USB unavailable";}
bool workspace_service_applying(void){return applying;}
workspace_apply_result_t workspace_service_apply_result(void){return profile_result;}
const char *workspace_service_apply_message(void){return "Profile result";}
static ui_notice_state_t queue;
static unsigned posts;
static ui_notice_kind_t last_kind;
static char last_key[64],last_detail[320];
void ui_notice_clear(const char *key){ui_notice_state_remove(&queue,key,false);}
void ui_notice_post(ui_notice_kind_t kind,const char *key,const char *title,const char *detail){
    ui_notice_config_t config={kind,key,title,detail,0};
    ui_notice_state_post(&queue,&config);posts++;last_kind=kind;
    snprintf(last_key,sizeof(last_key),"%s",key);snprintf(last_detail,sizeof(last_detail),"%s",detail);
}
static unsigned fo_consumed,fo_confirmed,fo_notice;
static bool fo_pending=true;
static void uart_debug_printf(const char *format,...){(void)format;}
static void machine_state_confirm_fo_mode(uint8_t mode){(void)mode;fo_confirmed++;}
static uint8_t machine_state_fo_mode(void){return 1;}
static bool setting_service_take_fo_mode_result(uint8_t *value){
    fo_consumed++;if(!fo_pending)return false;fo_pending=false;if(value)*value=1;return true;
}
static void page_01_bottom_a_refresh_fo(bool animated){(void)animated;}
static void smart_island_refresh_summary(void){}
static void page_03_update_menu_button_states_refresh(void){}
static void app_setting_notice_result(const char *key,const char *title,bool success){
    (void)key;(void)title;(void)success;fo_notice++;
}
'''
main = r'''
int main(void){
 ui_notice_state_init(&queue);
 /* Startup load/maintenance edges have no registered user request. */
 ws_busy=records_busy=true;app_ui_runtime_poll_operation_notices();
 ws_busy=records_busy=false;app_ui_runtime_poll_operation_notices();assert(posts==0);
 app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,"No-op");assert(posts==0);
 ws_busy=true;app_ui_runtime_notice_started(APP_UI_NOTICE_WORKSPACE_STORE,"Saving");
 assert(posts==1&&last_kind==UI_NOTICE_PROGRESS&&!strcmp(last_key,"workspace.store"));
 /* Dismissal and page lifetime do not destroy service observation. */
 ui_notice_state_remove(&queue,"workspace.store",true);
 assert(!queue.has_active);
 app_ui_runtime_poll_operation_notices();assert(posts==1);
 ws_busy=false;ws_ok=false;app_ui_runtime_poll_operation_notices();
 assert(posts==2&&last_kind==UI_NOTICE_ERROR&&queue.has_active);
 assert(!strcmp(last_key,"workspace.store"));
 app_ui_runtime_poll_operation_notices();assert(posts==2);
 ws_busy=true;records_busy=true;
 app_ui_runtime_notice_started(APP_UI_NOTICE_BATCH_SAVE,"Saving cycle");
 app_ui_runtime_notice_started(APP_UI_NOTICE_RECORD_STORE,"Exporting records");
 ws_busy=false;ws_ok=true;app_ui_runtime_poll_operation_notices();
 assert(last_kind==UI_NOTICE_SUCCESS&&!strcmp(last_key,"workspace.batch"));
 assert(strstr(last_detail,"awaits confirmation"));
 records_busy=false;records_ok=false;app_ui_runtime_poll_operation_notices();
 assert(last_kind==UI_NOTICE_ERROR&&!strcmp(last_key,"records.store"));
 /* A matched negative ACK has already ended the profile before this poll.
  * Only this owner posts its final ERROR; a second poll is silent. */
 applying=true;profile_result=WORKSPACE_APPLY_PENDING;
 app_ui_runtime_notice_started(APP_UI_NOTICE_PROFILE_APPLY,"Applying profile");
 unsigned rejected_posts=posts;
 applying=false;profile_result=WORKSPACE_APPLY_FAILED;app_ui_runtime_poll_operation_notices();
 assert(posts==rejected_posts+1&&last_kind==UI_NOTICE_ERROR&&!strcmp(last_key,"workspace.apply"));
 app_ui_runtime_poll_operation_notices();assert(posts==rejected_posts+1);
 /* Cancellation is a final informational result, not a successful apply. */
 applying=true;profile_result=WORKSPACE_APPLY_PENDING;
 app_ui_runtime_notice_started(APP_UI_NOTICE_PROFILE_APPLY,"Applying profile");
 applying=false;profile_result=WORKSPACE_APPLY_CANCELLED;app_ui_runtime_poll_operation_notices();
 assert(last_kind==UI_NOTICE_INFO&&!strcmp(last_key,"workspace.apply"));
 applying=true;app_ui_runtime_notice_started(APP_UI_NOTICE_PROFILE_APPLY,"Applying profile");
 applying=false;profile_result=WORKSPACE_APPLY_UNCONFIRMED;app_ui_runtime_poll_operation_notices();
 assert(last_kind==UI_NOTICE_WARNING);
 unsigned before=posts;app_ui_runtime_poll_operation_notices();assert(posts==before);
 /* A 6-byte frame contains only type then checksum, never a result byte. */
 uint8_t truncated[]={0xfd,0xdf,6,0x3a,1,1};route_fo(truncated,6);
 assert(fo_pending&&!fo_consumed&&!fo_confirmed&&!fo_notice);
 uint8_t accepted[]={0xfd,0xdf,7,0x3a,1,1,0};route_fo(accepted,7);
 assert(!fo_pending&&fo_consumed==1&&fo_confirmed==1&&fo_notice==1);
 route_fo(accepted,7);assert(fo_notice==1&&fo_confirmed==1);
 puts("PASS global async notice owner: explicit requests only, background completion, dismissal, independent keys, cancel/unconfirmed, FO length boundary");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-operation-notice-') as tmp:
    code, exe = Path(tmp)/'owner.c', Path(tmp)/('owner.exe' if os.name=='nt' else 'owner')
    code.write_text(fixture + owner + fo + main)
    command=[os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror',
             '-I'+str(root),str(code),str(root/'un260/lv_components/ui_notice_state.c'),'-o',str(exe)]
    if os.name!='nt':command+=['-fsanitize=address,undefined','-no-pie']
    subprocess.run(command,check=True)
    subprocess.run([str(exe)],check=True)
