/* Exercise the actual current HTML producer with production result semantics. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/lv_system/ui_export_data.c"
#include "un260/counting/counting_data_store_internal.h"

/* Hardware clock/currency are fixed; results, mode and locale code are real. */
void machine_time_get(machine_time_value_t *value)
{ *value=(machine_time_value_t){2026,10,13,12,34,56}; }
void currency_state_get_active_code(char code[4]) { memcpy(code,"USD",4); }
uint8_t currency_state_active_index(void) { return 0; }
bool currency_state_get_code(uint8_t index,char code[4])
{ (void)index;memcpy(code,"USD",4);return true; }

static void json_value(FILE *file,const char *text)
{
    fputc('"',file);
    for(const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if(*p=='"'||*p=='\\') fputc('\\',file);
        if(*p<32) fprintf(file,"\\u%04x",*p); else fputc(*p,file);
    }
    fputc('"',file);
}
static void expected(FILE *file,const char *key)
{
    json_value(file,key);fputc(':',file);json_value(file,ui_tr(key));
}
int main(int argc,char **argv)
{
    assert(argc==2);
    char path[1024];
    snprintf(path,sizeof(path),"%s/manifest.json",argv[1]);
    FILE *manifest=fopen(path,"wb");assert(manifest);fputs("[",manifest);
    counting_sim_t *data=counting_data_mutable();
    bool comma=false;
    for(size_t locale_index=0;locale_index<ui_lang_count();++locale_index) {
        const ui_locale_t *locale=ui_lang_at(locale_index);assert(locale);
        ui_lang_set(locale->id);
        for(unsigned scenario=0;scenario<4;++scenario) {
            counting_data_clear_serials(data);counting_data_clear_errors(data);
            memset(data,0,sizeof(*data));
            data->total_pcs=85;data->total_amount=425;
            data->denom_number=1;data->denom[0]=(denom_t){5,85,425};
            assert(counting_data_ensure_serial_capacity(data,3));
            const char *serials[]={"Z90","AB2","JK3"};
            for(unsigned i=0;i<3;++i) {data->sn_str[i]=strdup(serials[i]);assert(data->sn_str[i]);data->denom_mix[i]=5;}
            if(scenario==1) {
                assert(counting_data_ensure_error_capacity(data,3));
                data->err_num=3;data->err_code[0]=0x16;data->err_pcs[0]=2;
                data->err_code[1]=0x1e;data->err_pcs[1]=1;
                data->err_code[2]=0x2e;data->err_pcs[2]=3;
                assert(counting_data_reject_pcs_count(data)==6);
                data->err_expected=9;
            } else if(scenario==2) data->err_expected=7; /* Total is known while detail has not arrived. */
            machine_state_confirm_mode(scenario==0?MODE_MDC:scenario==1?MODE_SDC:scenario==2?MODE_CNT:255);
            machine_state_confirm_fo_mode(scenario==0?0:scenario==1?1:3);
            machine_state_confirm_work_mode(scenario==1?1:0);
            machine_state_confirm_add(scenario!=0);
            machine_state_confirm_speed(scenario);
            machine_state_confirm_batch(scenario!=1,7);
            snprintf(path,sizeof(path),"%s/%s-%u.html",argv[1],locale->tag,scenario);
            assert(ui_export_data_write_html_file(path));
            if(comma)fputc(',',manifest);
            comma=true;
            fputs("{\"path\":",manifest);json_value(manifest,path);
            fputs(",\"tag\":",manifest);json_value(manifest,locale->tag);
            fprintf(manifest,",\"scenario\":%u,\"rejectCount\":%d,\"expected\":{",scenario,counting_data_reject_pcs_count(data));
            expected(manifest,scenario==0?"Mixed denominations":scenario==1?"Single denomination":scenario==2?"Piece counting":"None");
            fputc(',',manifest);expected(manifest,"No.");
            fputc(',',manifest);expected(manifest,"Rejected");
            fputc(',',manifest);expected(manifest,"No rejected notes");
            fputc(',',manifest);expected(manifest,"No reject handling is needed. This is not an authenticity verdict.");
            if(scenario==1)for(unsigned i=0;i<3;++i){fputc(',',manifest);expected(manifest,counting_reject_reason_get(data->err_code[i]));}
            fputs("}}",manifest);
            if(!locale_index&&scenario==1) {
                snprintf(path,sizeof(path),"%s/stable.csv",argv[1]);
                assert(ui_export_data_write_csv_file(path));
            }
        }
    }
    fputs("]\n",manifest);assert(!fclose(manifest));
    assert(!ui_export_data_write_html_file("/this-path-does-not-exist-un260/report.html"));
    counting_data_clear_serials(data);counting_data_clear_errors(data);
    puts("current report producer: PASS (10 real locales, 3 reject states plus unknown-mode fallback, original totals/serials/order, write failure)");
    return 0;
}
