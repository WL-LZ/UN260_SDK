#include "app_auto_qr.h"
#include "app_command_runtime.h"
#include "un260/storage/workspace_store.h"
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_01_main_quick.h"
#include "un260/lv_core/settings_detail_ui.h"
#include "un260/lv_components/lv_qr_popup.h"
#include "un260/lv_components/lv_fault_popup.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_system/ui_qr_data.h"
static char snapshot[3072];
static uint32_t ended_at;
static bool enabled,pending;
static uint32_t owned;
void app_auto_qr_cancel(void)
{pending=false;enabled=false;if(owned&&owned==lv_qr_popup_generation())lv_qr_popup_hide();owned=0;}
void app_auto_qr_on_start(void)
{
    app_auto_qr_cancel();const workspace_user_t *u=workspace_active(workspace_store_get());
    enabled=workspace_store_ready()&&u&&u->qr_after_count;
}
void app_auto_qr_on_end(uint32_t now)
{
    /* Freeze at END; do not read a later session's live result from a timer. */
    pending=enabled&&ui_manager_get_current_page()==UI_PAGE_MAIN&&ui_qr_data_build_summary(snapshot,sizeof(snapshot));
    ended_at=now;enabled=false;
}
void app_auto_qr_poll(uint32_t now)
{
    if(owned&&owned!=lv_qr_popup_generation())owned=0;
    if(!pending)return;
    if(ui_manager_get_current_page()!=UI_PAGE_MAIN||app_command_runtime_count_start_busy()||now-ended_at>4000){pending=false;return;}
    if(now-ended_at<350||ui_manager_is_transitioning()||app_command_runtime_result_pending())return;
    if(fault_popup_is_showing()||fault_popup_get_pending_fault(NULL,NULL,NULL)||smart_island_is_expanded()||page_01_main_quick_is_open()||settings_detail_overlay_is_open()||lv_qr_popup_is_showing())return;
    for(lv_indev_t *i=lv_indev_get_next(NULL);i;i=lv_indev_get_next(i))if(i->proc.state==LV_INDEV_STATE_PRESSED)return;
    /* Never cover an unrelated top-layer dialog or an in-progress gesture. */
    for(unsigned i=0;i<lv_obj_get_child_cnt(lv_layer_top());i++){
        lv_obj_t *child=lv_obj_get_child(lv_layer_top(),i);
        if(lv_obj_is_visible(child)&&lv_obj_has_flag(child,LV_OBJ_FLAG_CLICKABLE))return;
    }
    pending=false;if(lv_qr_popup_show(snapshot))owned=lv_qr_popup_generation();
}
