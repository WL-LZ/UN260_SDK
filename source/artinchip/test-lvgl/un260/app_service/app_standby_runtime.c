#include "un260/lv_system/ui_i18n.h"
#include "app_standby_runtime.h"
#include "un260/lv_components/ui_notice.h"
#include "app_command_runtime.h"
#include "un260/storage/standby_store.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_34_standby.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/lv_upgrade_popup.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/machine_state/machine_state.h"
#include "lvgl/lvgl.h"
static uint32_t activity;
static bool initialized,capture,wake,touching,armed;
static const char *standby_operation_blocker(void)
{
 if(app_command_runtime_count_start_busy())return UI_N_("Finish counting before standby.");
 if(machine_state_aging_running())return UI_N_("Stop the aging test before standby.");
 /* Acknowledged records remain available for inspection until a controller
  * report resolves them. They are not a pending UI operation (e.g. No notes).
  * Keep the visible guide and unread reports guarded without discarding any
  * fault record or changing its protocol acknowledgement. */
 if(fault_popup_is_showing())return UI_N_("Review the machine guide before standby.");
 if(lv_upgrade_popup_is_showing())return UI_N_("Wait for the update to finish.");
 if(lv_qr_popup_is_showing())return UI_N_("Close the QR code before standby.");
 if(machine_fault_first_unread(NULL))return UI_N_("Review the machine guide before standby.");
 if(standby_store_busy())return UI_N_("Standby resources are busy. Try again shortly.");
 return NULL;
}
const char *app_standby_runtime_blocker(void)
{
 const char *reason=standby_operation_blocker();
 if(reason)return reason;
 return ui_manager_is_transitioning()?UI_N_("Wait for the current page to finish opening."):NULL;
}
bool app_standby_runtime_request(void)
{
 if(app_standby_runtime_blocker()){
  if(app_standby_runtime_needs_fault_review())(void)fault_popup_show_pending_now();
  return false;
 }
 ui_manager_push_page(UI_PAGE_STANDBY);
 return ui_manager_get_current_page()==UI_PAGE_STANDBY;
}
bool app_standby_runtime_needs_fault_review(void)
{
 return !ui_manager_is_transitioning()&&!app_command_runtime_count_start_busy()&&
  !machine_state_aging_running()&&!fault_popup_is_showing()&&
  !lv_upgrade_popup_is_showing()&&!lv_qr_popup_is_showing()&&
  machine_fault_first_unread(NULL);
}
void app_standby_runtime_enter(void){wake=false;armed=!touching;}
void app_standby_runtime_protocol_activity(void){activity=lv_tick_get();if(ui_manager_get_current_page()==UI_PAGE_STANDBY)wake=true;}
bool app_standby_runtime_touch(bool down){if(down||touching)activity=lv_tick_get();touching=down;
 if(ui_manager_get_current_page()==UI_PAGE_STANDBY){if(!armed){if(!down)armed=true;return true;}if(down){capture=true;wake=true;}}
 if(capture){if(!down)capture=false;return true;}return false;
}
void app_standby_runtime_poll(uint32_t now){
 if(!initialized){activity=now;initialized=true;standby_store_init();}
 ui_page_t page=ui_manager_get_current_page();
 if(page!=UI_PAGE_STANDBY_SETTING){char message[160];if(standby_store_poll(message,sizeof(message))) ui_notice_post_message(standby_store_last_success()?UI_NOTICE_SUCCESS:UI_NOTICE_ERROR,"settings.standby",UI_N_("Standby settings"),standby_store_message_info());}
 bool blocked=standby_operation_blocker()!=NULL;
 if(page==UI_PAGE_STANDBY){if(blocked)wake=true;if(wake&&!ui_manager_is_transitioning()&&ui_page_35_standby_fade_out()){wake=false;activity=now;ui_manager_pop_page();}return;}
 /* Conservative allow-list: setup, result browsing and maintenance never sleep. */
 if((page!=UI_PAGE_MAIN&&page!=UI_PAGE_PURE)||blocked||touching||ui_manager_is_transitioning()){activity=now;return;}
 unsigned min=standby_config()->minutes;if(min&&(uint32_t)(now-activity)>=min*60000U){activity=now;(void)app_standby_runtime_request();}
}
