#include "support_report.h"
#include "un260/device_info/device_info.h"
#include "un260/machine_state/machine_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_reject_reason.h"
#include "un260/storage/cashbook_store.h"
#include "un260/lv_system/machine_time.h"
#include <stdio.h>
#include <stdarg.h>
static bool add(char *out,size_t capacity,size_t *used,const char *format,...)
{
    if(*used>=capacity)return false;
    va_list args;va_start(args,format);
    int n=vsnprintf(out+*used,capacity-*used,format,args);va_end(args);
    if(n<0||(size_t)n>=capacity-*used)return false;
    *used+=(size_t)n;
    return true;
}
bool support_report_build(char *out,size_t capacity)
{
    if(!out||!capacity)return false;
    out[0]=0;size_t used=0;machine_time_value_t now;machine_time_get(&now);
    bool remote=device_info_is_valid();
    if(!add(out,capacity,&used,"UN260 SUPPORT REPORT v1\nDevice clock: %04u-%02u-%02u %02u:%02u:%02u\nRemote versions verified: %s\nDisplay: %.80s\nController: %.80s\nImage: %.80s\nFPGA: %.80s\nController boot: %.80s\nImage boot: %.80s\n\n",
        now.year,now.month,now.day,now.hour,now.minute,now.second,remote?"yes":"no",device_info_display_app(),remote?device_info_main_app():"Unavailable",remote?device_info_image_app():"Unavailable",remote?device_info_fpga():"Unavailable",remote?device_info_main_boot():"Unavailable",remote?device_info_image_boot():"Unavailable"))return false;
    if(!add(out,capacity,&used,"Confirmed settings\nMode: %u\nSpeed: %u\nStart: %u\nADD: %u\nSort: %u\nSound: %u\nBatch enabled: %u\nBatch size: %u\nReject capacity: %u\nRecords ready: %u\nRecords pending write: %u\n\nCurrent reject reasons (not historical fault logs)\n",
        machine_state_mode(),machine_state_speed(),machine_state_work_mode(),machine_state_add_enabled(),machine_state_fo_mode(),machine_state_buzzer_enabled(),machine_state_batch_enabled(),machine_state_batch_num(),machine_state_reject_pocket_max(),cashbook_store_ready(),cashbook_store_busy()))return false;
    const counting_sim_t *sim=counting_data_current();unsigned counts[256]={0};int count=counting_data_error_detail_count(sim);
    if(sim&&sim->err_code&&sim->err_pcs)for(int i=0;i<count;i++)counts[sim->err_code[i]]+=sim->err_pcs[i];
    for(unsigned i=0;i<256;i++)if(counts[i]&&!add(out,capacity,&used,"0x%02X / %.40s / %u\n",i,counting_reject_reason_get(i),counts[i]))return false;
    return add(out,capacity,&used,"\nExcluded: serial numbers, note images, raw protocol frames, operator names/photos, passwords, monetary totals.\n");
}
