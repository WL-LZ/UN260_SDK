#include "app_collection.h"
#include "app_command_runtime.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_i18n.h"

bool app_collection_request(data_collect_mode_t mode)
{
    if (app_command_runtime_count_start_busy()) return false;
    const char *status;
    switch (mode) {
    case DATA_COLLECT_MODE_ALL: status=UI_N_("Requesting all-note collection"); break;
    case DATA_COLLECT_MODE_FALSE: status=UI_N_("Requesting rejected-note collection"); break;
    case DATA_COLLECT_MODE_NONE: status=UI_N_("Exiting collection mode..."); break;
    default: return false;
    }
    if (!data_collection_request_begin(mode,status,app_clock_uptime_ms())) return false;
    uint8_t command=mode==DATA_COLLECT_MODE_NONE ? 0xFF : (uint8_t)mode;
    if (protocol_send(0xC0,&command,1)!=6) {
        data_collection_request_cancel();
        data_collection_state_set_status(UI_N_("Command not sent. Check the controller connection."));
        return false;
    }
    return true;
}
