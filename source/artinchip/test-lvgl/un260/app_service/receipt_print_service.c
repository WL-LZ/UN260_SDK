#include "receipt_print_service.h"
#include "app_command_runtime.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_history_service.h"
#include "un260/currency/currency_state.h"
#include "un260/lv_system/ui_history_data.h"
#include "un260/lv_system/machine_time.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_i18n.h"
#include "un260/print/print_config.h"
#include "un260/protocol/protocol_send.h"
#include <string.h>
static receipt_print_status_t status;
static uint32_t sent_at;
static bool sent_once;
const receipt_print_status_t *receipt_print_status(void){return &status;}
const char *receipt_print_blocker(uint32_t id)
{
    if(app_command_runtime_count_start_busy()||print_config_pending())
        return UI_N_("Finish counting and receipt settings before printing.");
    if(sent_once&&app_clock_uptime_ms()-sent_at<2000U)
        return UI_N_("Wait a moment before printing again.");
    const counting_sim_t *sim=counting_data_current();
    if(!sim||sim->total_pcs<=0)return UI_N_("Count notes before printing.");
    if(currency_state_multi_selected()||!counting_data_monetary_result_supported(sim))
        return UI_N_("Export this result to print it on a computer.");
    if(id){
        ui_history_record_t record;
        char code[4];currency_state_get_active_code(code);
        if(id!=counting_history_current_record()||!counting_history_is_idle()||
           !ui_history_data_is_available()||ui_history_data_status()!=STORAGE_JOB_SUCCEEDED||
           !ui_history_record_get_by_no(id,&record)||record.multi.enabled||
           record.pcs!=(uint32_t)sim->total_pcs||strcmp(record.currency,code)||
           (record.detail_bytes?record.detail_amount:record.amount)!=(uint64_t)sim->total_amount)
            return UI_N_("Historical records: export HTML, then print on a computer.");
    }
    return NULL;
}
bool receipt_print_request(uint32_t id)
{
    if(receipt_print_blocker(id))return false;
    machine_time_value_t now;machine_time_get(&now);
    char code[4];currency_state_get_active_code(code);
    uint8_t payload[9]={(uint8_t)code[0],(uint8_t)code[1],(uint8_t)code[2],
        (uint8_t)(now.year>=2000?now.year-2000:now.year),now.month,now.day,now.hour,now.minute,now.second};
    status.record_no=id?id:counting_history_current_record();++status.revision;
    if(protocol_send(0x3C,payload,sizeof(payload))<0){status.state=RECEIPT_PRINT_FAILED;return false;}
    sent_at=app_clock_uptime_ms();sent_once=true;status.state=RECEIPT_PRINT_SENT;
    /* 0x3C has no transaction ID or defined success/error field. SENT never
     * becomes paper-output success based on an unsolicited completion frame. */
    return true;
}
