#include "app_standby_runtime.h"
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
void app_standby_runtime_enter(void){wake=false;armed=!touching;}
void app_standby_runtime_protocol_activity(void){activity=lv_tick_get();if(ui_manager_get_current_page()==UI_PAGE_STANDBY)wake=true;}
bool app_standby_runtime_touch(bool down){if(down||touching)activity=lv_tick_get();touching=down;
 if(ui_manager_get_current_page()==UI_PAGE_STANDBY){if(!armed){if(!down)armed=true;return true;}if(down){capture=true;wake=true;}}
 if(capture){if(!down)capture=false;return true;}return false;
}
void app_standby_runtime_poll(uint32_t now){
 if(!initialized){activity=now;initialized=true;standby_store_init();}
 ui_page_t page=ui_manager_get_current_page();
 if(page!=UI_PAGE_STANDBY_SETTING){char message[160];standby_store_poll(message,sizeof(message));}
 bool blocked=app_command_runtime_count_start_busy()||machine_state_aging_running()||fault_popup_is_showing()||fault_popup_get_pending_fault(NULL,NULL,NULL)||lv_upgrade_popup_is_showing()||lv_qr_popup_is_showing();
 if(page==UI_PAGE_STANDBY){if(blocked)wake=true;if(wake&&!ui_manager_is_transitioning()&&ui_page_35_standby_fade_out()){wake=false;activity=now;ui_manager_pop_page();}return;}
 /* Conservative allow-list: setup, result browsing and maintenance never sleep. */
 if((page!=UI_PAGE_MAIN&&page!=UI_PAGE_PURE)||blocked||touching||standby_store_busy()||ui_manager_is_transitioning()){activity=now;return;}
 unsigned min=standby_config()->minutes;if(min&&(uint32_t)(now-activity)>=min*60000U){activity=now;ui_manager_push_page(UI_PAGE_STANDBY);}
}
