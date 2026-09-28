#include "workspace_service.h"
#include "setting_service.h"
#include "work_mode_service.h"
#include "app_command_runtime.h"
#include "un260/storage/workspace_store.h"
#include "un260/counting/counting_history_service.h"
#include "un260/counting/counting_cashbook.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_multi.h"
#include "un260/machine_state/machine_state.h"
#include "un260/lv_system/user_cfg.h"
#include "un260/protocol/protocol_send.h"
#include "un260/boot/boot_service.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {bool active,waiting;unsigned step,confirmed;uint32_t since;workspace_profile_t target;char message[160];} apply;
static const char *const steps[]={"Speed","Sorting","Sound","Batch","ADD","Start method","Count mode"};
static unsigned value(unsigned step,const workspace_profile_t *p)
{
    switch(step){case 0:return p->speed;case 1:return p->sort;case 2:return p->beep;
        case 3:return p->batch_enabled?p->batch:0;case 4:return p->add;case 5:return p->work;default:return p->mode;}
}
static unsigned actual(unsigned step)
{
    switch(step){case 0:return machine_state_speed();case 1:return machine_state_fo_mode();case 2:return machine_state_buzzer_enabled();
        case 3:return machine_state_batch_enabled()?machine_state_batch_num():0;case 4:return machine_state_add_enabled();case 5:return machine_state_work_mode();default:return machine_state_mode()==MODE_SDC?2:machine_state_mode()==MODE_CNT?3:1;}
}
static bool request(unsigned step,unsigned target)
{
    switch(step){case 0:return setting_service_request_speed(target);case 1:return setting_service_request_fo_mode(target);
        case 2:return setting_service_request_beep(target);case 3:return setting_service_request_batch_switch(target!=0,target?target:200,machine_state_batch_enabled(),machine_state_batch_num());
        case 4:return setting_service_request_add(target);case 5:return setting_service_request_work_mode(target);default:return setting_service_request_mode(target==2?MODE_SDC:target==3?MODE_CNT:MODE_MDC);}
}
void workspace_service_init(void){workspace_store_init();}
static const char *machine_blocker(void)
{
    if(app_command_runtime_count_start_busy()||machine_state_aging_running())return "Finish counting before changing the workspace.";
    if(work_mode_service_diagnostic_active())return "Finish the diagnostic operation first.";
    if(!counting_history_can_start())return "The last result is still being saved. Try again shortly.";
    return NULL;
}
const char *workspace_service_switch_blocker(void)
{
    if(!workspace_store_ready())return "The workspace is not available yet.";
    if(workspace_store_busy())return "A workspace save or photo import is in progress.";
    if(apply.active)return "Wait for the profile to finish applying.";
    if(counting_cashbook_verify_group())return "Finish or cancel the armed recount before switching workspaces.";
    if(app_command_runtime_result_pending())return "Wait for the last counting result to finish.";
    if(counting_data_current()->multi_currency_result&&counting_multi_current()->total_pcs)
        return "Finish and clear the MULTI group before switching operator or applying a profile.";
    if(machine_state_add_enabled()&&counting_data_current()->total_pcs>0)
        return "Finish and clear the ADD session before switching operator or applying a profile.";
    return machine_blocker();
}
bool workspace_service_switch(uint32_t id)
{
    if(workspace_service_switch_blocker())return false;
    /* Heap snapshot, not a large per-callback stack allocation. */
    const workspace_model_t *saved=workspace_store_get();
    if(id==saved->active_id)return true;
    workspace_model_t *next=malloc(sizeof(*next));if(!next)return false;
    *next=*saved;bool ok=workspace_find(next,id)!=NULL;
    next->active_id=id;if(ok)ok=workspace_store_save(next);free(next);return ok;
}
bool workspace_service_apply(const workspace_profile_t *p,uint32_t now)
{
    if(!p||workspace_service_switch_blocker()||!protocol_send_is_ready()||boot_service_get_stage()!=BOOT_STAGE_DONE)return false;
    if(p->mode>3||p->speed>2||p->sort>3||p->beep>1||p->add>1||p->work>1||p->batch>200||(p->batch_enabled&&(!p->batch||p->batch==200)))return false;
    apply.target=*p;if(!apply.target.mode)apply.target.mode=actual(6);apply.active=true;apply.waiting=false;apply.step=apply.confirmed=0;apply.since=now;
    snprintf(apply.message,sizeof(apply.message),"Applying profile...");return true;
}
bool workspace_service_poll(uint32_t now)
{
    bool changed=workspace_store_poll();
    if(!apply.active)return changed;
    /* A matching confirmed value is evidence; sending a command is not. */
    if(apply.waiting&&actual(apply.step)==value(apply.step,&apply.target)) {
        apply.waiting=false;apply.confirmed++;apply.step++;changed=true;
    }
    while(!apply.waiting&&apply.step<7&&actual(apply.step)==value(apply.step,&apply.target))apply.step++;
    if(apply.step==7) {apply.active=false;snprintf(apply.message,sizeof(apply.message),"Profile applied. Device values confirmed.");return true;}
    const char *blocked=machine_blocker();
    if(blocked||(apply.waiting&&(uint32_t)(now-apply.since)>1800)) {
        snprintf(apply.message,sizeof(apply.message),"Stopped at %s. %u changes confirmed. Check actual values before retrying.",steps[apply.step],apply.confirmed);
        apply.active=false;return true;
    }
    if(!apply.waiting) {
        if(!request(apply.step,value(apply.step,&apply.target))) {
            snprintf(apply.message,sizeof(apply.message),"%s could not be sent. %u changes confirmed; remaining steps were not sent.",steps[apply.step],apply.confirmed);
            apply.active=false;return true;
        }
        apply.waiting=true;apply.since=now;snprintf(apply.message,sizeof(apply.message),"Waiting for %s confirmation...",steps[apply.step]);changed=true;
    }return changed;
}
bool workspace_service_applying(void){return apply.active;}
void workspace_service_cancel_apply(void)
{
    if(!apply.active)return;
    apply.active=false;snprintf(apply.message,sizeof(apply.message),"Remaining steps cancelled. An already-sent command may still complete.");
}
const char *workspace_service_apply_message(void){return apply.message;}
bool workspace_service_quick_enabled(void)
{
    const workspace_user_t *u=workspace_active(workspace_store_get());
    return !workspace_store_ready()||!u||u->quick_enabled;
}
bool workspace_service_set_quick_enabled(bool enabled)
{
    if(!workspace_store_ready()||workspace_store_busy())return false;
    workspace_model_t *next=malloc(sizeof(*next));if(!next)return false;
    *next=*workspace_store_get();workspace_user_t *u=workspace_find(next,next->active_id);
    bool ok=u!=NULL;if(ok){u->quick_enabled=enabled;ok=workspace_store_save(next);}free(next);return ok;
}
bool workspace_service_batch_next(void)
{
    /* Changing the Batch cycle during an idle ADD session does not change
     * ownership or clear totals. Do not inherit the operator-switch blocker. */
    if(!workspace_store_ready()||workspace_store_busy()||apply.active||machine_blocker()||!protocol_send_is_ready())return false;
    const workspace_user_t *u=workspace_active(workspace_store_get());
    uint8_t target=workspace_next_batch(u,machine_state_batch_enabled()?machine_state_batch_num():0);
    return setting_service_request_batch_switch(target!=0,target?target:200,machine_state_batch_enabled(),machine_state_batch_num());
}
