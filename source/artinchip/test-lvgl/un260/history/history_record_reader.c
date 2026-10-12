#include "un260/lv_system/ui_history_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#ifndef UI_HISTORY_STORE_DIR
#define UI_HISTORY_STORE_DIR "/etc/ui_state/count_history"
#endif

bool ui_history_record_read_detail(const ui_history_record_t *rec,history_record_detail_t *out)
{
    if(!rec || !out)return false;
    if(rec->detail_bytes){
        if(rec->detail_bytes>HISTORY_SNAPSHOT_MAX_BYTES)return false;
        char path[256];snprintf(path,sizeof(path),UI_HISTORY_STORE_DIR "/detail-%u-%016" PRIx64 ".abc",rec->record_no,rec->detail_hash);
        FILE *fp=fopen(path,"rb");if(!fp)return false;
        unsigned char *bytes=malloc(rec->detail_bytes);
        bool ok=bytes && fread(bytes,1,rec->detail_bytes,fp)==rec->detail_bytes && fgetc(fp)==EOF && !ferror(fp);
        if(fclose(fp)!=0)ok=false;
        if(ok)ok=history_record_snapshot_decode(bytes,rec->detail_bytes,rec->detail_hash,rec->pcs,out);
        if(ok){
            if(out->denoms_complete && out->denomination_amount!=rec->detail_amount){history_record_detail_release(out);free(bytes);return false;}
            uint8_t codes[32]={0};
            for(size_t i=0;i<out->reject_count;i++){
                unsigned code=history_record_detail_reject_at(out,i)->code;codes[code/8]|=1U<<(code%8);
            }
            if(memcmp(codes,rec->detail_reject_codes,32)){history_record_detail_release(out);ok=false;}
        }
        free(bytes);return ok;
    }
    const history_detail_input_t input={
        .denom_text=rec->denom_text,.sn_detail_text=rec->sn_detail_text,.sn_text=rec->sn_text,
        .session_log=rec->session_log,.error_frame_text=rec->error_frame_text,.total_pcs=rec->pcs,
        .denoms_truncated=strlen(rec->denom_text)>=sizeof(rec->denom_text)-1,
        .serials_truncated=strlen(rec->sn_detail_text)>=sizeof(rec->sn_detail_text)-1,
        .legacy_serials_truncated=strlen(rec->sn_text)>=sizeof(rec->sn_text)-1,
        .log_truncated=strlen(rec->session_log)>=sizeof(rec->session_log)-1,.reject_log_complete=false};
    return history_record_detail_build(&input,out);
}
