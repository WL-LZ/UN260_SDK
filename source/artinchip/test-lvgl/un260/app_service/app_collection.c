#include "app_collection.h"
#include "app_command_runtime.h"
#include "un260/protocol/protocol_send.h"
#include "un260/lv_system/app_clock.h"
#include "un260/lv_system/ui_i18n.h"
bool app_collection_request(data_collect_mode_t mode)
{
 if(app_command_runtime_count_start_busy())return false;
 if(!data_collection_request_begin(mode,mode==DATA_COLLECT_MODE_NONE?
    UI_N_("Waiting for capture disable confirmation."):UI_N_("Waiting for capture enable confirmation."),app_clock_uptime_ms()))return false;
 uint8_t command=mode==DATA_COLLECT_MODE_NONE?0xFF:(uint8_t)mode;
 if(protocol_send(0xC0,&command,1)!=6){data_collection_request_cancel();return false;}
 return true;
}
