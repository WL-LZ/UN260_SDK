#include "data_collection.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/protocol/protocol_request.h"
#include <string.h>

static data_collection_snapshot_t state;
static protocol_request_t request=PROTOCOL_REQUEST_INITIALIZER(DATA_COLLECTION_REQUEST_TIMEOUT_MS);
static ui_message_t message={.key=UI_N_("Please select a collection mode.")};
static char text[320];
static uint64_t mode_at,run_at;
static bool run_started,finish_seen;
static void phase(data_collection_phase_t p,const char *key){state.phase=p;ui_message_key(&message,key);}
void data_collection_snapshot(data_collection_snapshot_t *out){if(out)*out=state;}
data_collect_mode_t data_collection_state_mode(void){return state.mode;}
uint16_t data_collection_state_pcs(void){return state.pcs;}
const char *data_collection_state_status(void){ui_message_render(&message,text,sizeof(text));return text;}
void data_collection_state_set_status(const char *key){ui_message_key(&message,key);}
void data_collection_state_set_message(const ui_message_t *m){if(m)message=*m;}
void data_collection_state_reset_pcs(void){state.pcs=0;}
void data_collection_state_select_mode(data_collect_mode_t mode,const char *key){state.mode=mode;state.pcs=0;state.phase=DATA_COLLECTION_READY;data_collection_state_set_status(key);}
void data_collection_state_exit(const char *key){state.mode=DATA_COLLECT_MODE_NONE;state.target=DATA_COLLECT_MODE_NONE;state.pending=false;state.uncertain=false;run_started=finish_seen=false;phase(DATA_COLLECTION_OFF,key);}
bool data_collection_request_pending(void){return state.pending;}
bool data_collection_request_begin(data_collect_mode_t target,const char *key,uint32_t now)
{
 (void)now;
 if(target!=DATA_COLLECT_MODE_NONE&&target!=DATA_COLLECT_MODE_ALL&&target!=DATA_COLLECT_MODE_FALSE)return false;
 if(state.counting)return false;
 if(state.pending&&(!state.uncertain||(target!=state.target&&target!=DATA_COLLECT_MODE_NONE)))return false;
 if(!state.pending&&target!=DATA_COLLECT_MODE_NONE&&state.mode!=DATA_COLLECT_MODE_NONE)return false;
 protocol_request_finish(&request);
 if(!protocol_request_begin_for_command(&request,0xC0))return false;
 state.pending=true;state.uncertain=false;state.target=target;
 phase(target==DATA_COLLECT_MODE_NONE?DATA_COLLECTION_CLOSING:DATA_COLLECTION_OPENING,key);
 return true;
}
void data_collection_request_cancel(void)
{
 if(!state.pending)return;
 /* A failed send cannot establish the controller state, including a partial TX. */
 state.uncertain=true;
 phase(state.target==DATA_COLLECT_MODE_NONE?DATA_COLLECTION_CLOSE_UNKNOWN:DATA_COLLECTION_OPEN_UNKNOWN,
       UI_N_("Command not sent. Check the controller connection."));
}
bool data_collection_request_take_timeout(uint32_t now)
{
 (void)now;if(!state.pending||state.uncertain||!protocol_request_timeout_ready(&request))return false;
 state.uncertain=true;
 phase(state.target==DATA_COLLECT_MODE_NONE?DATA_COLLECTION_CLOSE_UNKNOWN:DATA_COLLECTION_OPEN_UNKNOWN,
       state.target==DATA_COLLECT_MODE_NONE?UI_N_("Disable not confirmed. Retry or check the connection."):UI_N_("Enable not confirmed. Retry or request disabling."));
 return true;
}
static void complete(void)
{
 if(state.mode!=DATA_COLLECT_MODE_FALSE||!run_started||!finish_seen||state.counting||state.pending||state.phase==DATA_COLLECTION_ERROR)return;
 if(state.phase!=DATA_COLLECTION_SUCCESS){state.completed++;phase(DATA_COLLECTION_SUCCESS,UI_N_("Run complete. Capture mode remains enabled."));}
}
void data_collection_count_start(void)
{
 state.counting=true;state.pcs=0;finish_seen=false;run_started=state.mode!=DATA_COLLECT_MODE_NONE&&!state.pending;
 run_at=protocol_request_result_time_ms();
 if(run_started)phase(DATA_COLLECTION_COUNTING,UI_N_("Counting started..."));
}
void data_collection_count_end(uint16_t pcs)
{
 state.counting=false;if(!run_started)return;state.pcs=pcs;
 if(state.pending||state.phase==DATA_COLLECTION_ERROR)return;
 phase(DATA_COLLECTION_WAITING,UI_N_("Counting ended. Waiting for capture completion."));complete();
}
data_collection_reply_result_t data_collection_reply_handle(const uint8_t *buf,uint8_t len,uint32_t now)
{
 (void)now;if(!buf||len!=6||buf[0]!=0xFD||buf[1]!=0xDF||buf[2]!=6||buf[3]!=0xC0||buf[5]!=0x0A)return DATA_COLLECTION_REPLY_INVALID;
 uint8_t code=buf[4];uint64_t at=protocol_request_result_time_ms();
 if(state.pending){
   if(at<request.started_ms)return DATA_COLLECTION_REPLY_IGNORED;
   uint8_t expected=state.target==DATA_COLLECT_MODE_ALL?1:state.target==DATA_COLLECT_MODE_FALSE?3:0xFF;
   if(code==expected){
     data_collect_mode_t target=state.target;state.pending=false;state.uncertain=false;protocol_request_finish(&request);mode_at=at;
     if(target==DATA_COLLECT_MODE_NONE){data_collection_state_exit(UI_N_("Collection mode exited."));return DATA_COLLECTION_REPLY_EXITED;}
     run_started=finish_seen=false;data_collection_state_select_mode(target,UI_N_("Capture enabled. Return to Main to count."));
     return DATA_COLLECTION_REPLY_REQUEST_CONFIRMED;
   }
   /* Progress and USB replies can never release ownership of a disable request. */
   if(code==6&&state.target!=DATA_COLLECT_MODE_NONE){state.uncertain=true;phase(DATA_COLLECTION_OPEN_UNKNOWN,UI_N_("USB not ready. Collection mode error."));}
   return DATA_COLLECTION_REPLY_IGNORED;
 }
 if(state.mode==DATA_COLLECT_MODE_NONE||at<mode_at)return DATA_COLLECTION_REPLY_IGNORED;
 if(code==6){finish_seen=false;phase(DATA_COLLECTION_ERROR,UI_N_("USB not ready. Collection mode error."));return DATA_COLLECTION_REPLY_STATUS_UPDATED;}
 if(!run_started||at<run_at||state.phase==DATA_COLLECTION_SUCCESS||state.phase==DATA_COLLECTION_ERROR)return DATA_COLLECTION_REPLY_IGNORED;
 if(code==4||code==2){phase(DATA_COLLECTION_COLLECTING,UI_N_("Collecting data. Capture mode remains enabled."));return DATA_COLLECTION_REPLY_STATUS_UPDATED;}
 if(code==7&&state.mode==DATA_COLLECT_MODE_FALSE){finish_seen=true;complete();return DATA_COLLECTION_REPLY_STATUS_UPDATED;}
 return DATA_COLLECTION_REPLY_IGNORED;
}
