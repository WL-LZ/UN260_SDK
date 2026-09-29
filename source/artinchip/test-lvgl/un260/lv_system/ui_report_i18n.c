#include "ui_report_i18n.h"
#include "ui_i18n.h"
#include "un260/counting/counting_reject_reason.h"

static const char *const report_keys[]={
#include "ui_report_i18n_keys.inc"
};
static void json_string(FILE *file,const char *text)
{
    fputc('"',file);
    for(const unsigned char *p=(const unsigned char *)(text?text:"");*p;++p){
        if(*p=='"'||*p=='\\'){fputc('\\',file);fputc(*p,file);}
        else if(*p<32||*p=='<'||*p=='>'||*p=='&')fprintf(file,"\\u%04x",*p);
        else fputc(*p,file);
    }
    fputc('"',file);
}
static void entry(FILE *file,language_t locale,const char *key,bool *comma)
{
    if(*comma)fputc(',',file);
    *comma=true;
    json_string(file,key);fputc(':',file);json_string(file,ui_tr_for(locale,key));
}
void ui_report_i18n_write(FILE *file,language_t locale)
{
    if(!file)return;
    const ui_locale_t *info=NULL;
    for(size_t i=0;i<ui_lang_count();++i){const ui_locale_t *v=ui_lang_at(i);if(v&&v->id==locale){info=v;break;}}
    const char *tag=info?info->tag:"en";
    fputs("<script>(function(){'use strict';const text=",file);
    fputc('{',file);bool comma=false;
    for(size_t i=0;i<sizeof(report_keys)/sizeof(report_keys[0]);++i)entry(file,locale,report_keys[i],&comma);
    for(unsigned i=0;i<0x32;++i)entry(file,locale,counting_reject_reason_get((uint8_t)i),&comma);
    fputs("};window.reportLocale=",file);json_string(file,tag);
    fputs(";document.documentElement.lang=window.reportLocale;document.documentElement.dir=",file);
    json_string(file,info&&info->direction==UI_LANG_RTL?"rtl":"ltr");
    fputs(";window.reportTr=k=>Object.prototype.hasOwnProperty.call(text,k)?text[k]:k;"
          "window.reportMatches=n=>window.reportTr('Matches: %u').replace(/%(?:1\\$)?u/,String(n));"
          "document.querySelectorAll('[data-i18n]').forEach(e=>{const k=e.getAttribute('data-i18n')||e.textContent;e.textContent=window.reportTr(k);});"
          "document.querySelectorAll('[data-i18n-batch]').forEach(e=>{if(Number(e.dataset.i18nBatch)>=0)e.textContent=window.reportTr('BAT:%u').replace(/%(?:1\\$)?u/,e.dataset.i18nBatch);});"
          "document.querySelectorAll('[data-i18n-placeholder]').forEach(e=>{e.placeholder=window.reportTr(e.getAttribute('data-i18n-placeholder'));});"
          "const s=document.getElementById('searchStatus');if(s&&s.dataset.count!==undefined)s.textContent=window.reportMatches(s.dataset.count);"
          "})();</script>\n",file);
}
