#include "un260/lv_system/ui_message.h"
#include "un260/lv_system/ui_report_i18n.h"
#include "un260/lv_components/ui_notice_state.h"
#include "un260/data_collection/data_collection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int locale,lookups;
static const ui_locale_t locales[]={{.id=0,.tag="en"},{.id=1,.tag="zh-Hans"}};
size_t ui_lang_count(void){return 2;}
const ui_locale_t *ui_lang_at(size_t n){return n<2?&locales[n]:NULL;}
const char *ui_tr_for(language_t language,const char *key)
{
    ++lookups;
    if(!language)return key;
    if(!strcmp(key,"Saved: %s"))return "%s 已保存";
    if(!strcmp(key,"Waiting for %s confirmation..."))return "正在等待%s确认";
    if(!strcmp(key,"Speed"))return "速度";
    if(!strcmp(key,"Saved"))return "已保存";
    if(!strcmp(key,"Serial Number"))return "冠字号";
    if(!strcmp(key,"Collection reply: 0x%02X"))return "采集响应：0x%02X";
    if(!strcmp(key,"Matches: %u"))return "匹配：%1$u";
    if(!strcmp(key,"BAT:%u"))return "预置：%1$u";
    if(!strcmp(key,"Amount"))return "</script><script>alert('translation')</script>";
    return key;
}
const char *ui_tr(const char *key){return ui_tr_for(locale,key);}

int main(int argc,char **argv)
{
    char output[512];ui_message_t m;
    ui_message_string(&m,"Saved: %s","Saved");
    assert(!lookups); /* Construction and background publication do not translate. */
    assert(ui_message_render(&m,output,sizeof(output))&&!strcmp(output,"Saved: Saved"));
    locale=1;
    assert(ui_message_render(&m,output,sizeof(output))&&!strcmp(output,"Saved 已保存"));
    assert(ui_message_source(&m,output,sizeof(output))&&!strcmp(output,"Saved: Saved"));
    ui_message_text(&m,"Waiting for %s confirmation...","Speed");
    assert(ui_message_render(&m,output,sizeof(output))&&!strcmp(output,"正在等待速度确认"));
    ui_message_literal(&m,"Saved");
    assert(ui_message_render(&m,output,sizeof(output))&&!strcmp(output,"Saved"));
    ui_message_literal(&m,"中文abc");
    for(size_t n=1;n<7;++n){char short_text[8];ui_message_render(&m,short_text,n);assert(strlen(short_text)%3==0);}
    ui_message_uint3_int64(&m,"%u / %u / %u / %lld",1,2,3,INT64_C(5000000000));
    assert(ui_message_render(&m,output,sizeof(output))&&!strcmp(output,"1 / 2 / 3 / 5000000000"));
    ui_message_t different=m;assert(ui_message_equal(&m,&different));
    different.amount++;assert(!ui_message_equal(&m,&different));

    ui_notice_state_t state;ui_notice_state_init(&state);
    ui_message_string(&m,"Saved: %s","bank.txt");
    ui_notice_config_t config={.kind=UI_NOTICE_INFO,.key="export",.title="Saved",.localized=true,.message=&m};
    assert(ui_notice_state_post(&state,&config));
    ui_message_literal(&m,"changed caller buffer");
    assert(ui_message_render(&state.active.message,output,sizeof(output))&&!strcmp(output,"bank.txt 已保存"));
    assert(!ui_notice_state_elapse(&state,1000));
    uint32_t revision=state.active.revision,remaining=state.active.remaining_ms;
    locale=0;ui_message_render(&state.active.message,output,sizeof(output));
    assert(!strcmp(output,"Saved: bank.txt"));
    assert(state.active.revision==revision&&state.active.remaining_ms==remaining&&state.active.repeats==1);
    config=(ui_notice_config_t){.kind=UI_NOTICE_PROGRESS,.key="save",.title="Saved",.localized=true};
    assert(ui_notice_state_post(&state,&config));assert(state.queued_count==1);
    assert(ui_notice_state_remove(&state,"save",true));
    locale=1;assert(!ui_notice_state_post(&state,&config)); /* Locale does not revoke acknowledgement. */
    config.kind=UI_NOTICE_SUCCESS;assert(ui_notice_state_post(&state,&config));

    const uint8_t unknown_reply[]={0,0,0,0,0xab,0};
    data_collection_reply_handle(unknown_reply,sizeof(unknown_reply),1);
    assert(!strcmp(data_collection_state_status(),"采集响应：0xAB"));
    locale=0;assert(!strcmp(data_collection_state_status(),"Collection reply: 0xAB"));
    assert(data_collection_request_begin(DATA_COLLECT_MODE_ALL,"Waiting",100));
    locale=1;data_collection_request_cancel();
    assert(!strcmp(data_collection_state_status(),"采集响应：0xAB"));

    assert(argc>1);FILE *report=fopen(argv[1],"w+b");assert(report);ui_report_i18n_write(report,1);rewind(report);
    char *html=malloc(32768);assert(html);size_t count=fread(html,1,32767,report);html[count]=0;
    assert(strstr(html,"\\u003c/script\\u003e"));assert(!strstr(html,"<script>alert("));
    fclose(report);
    FILE *f=fopen(argv[1],"wb");assert(f);fputs("<!doctype html><h1 data-i18n='Saved'>Saved</h1><p id='data'>Saved</p><div id='searchStatus' data-count='3'>3 matches</div>",f);fwrite(html,1,count,f);fclose(f);
    free(html);
    puts("i18n message descriptors, queued state, collection restore and report escaping: PASS");
    return 0;
}
