/* Reuse existing storage/notice stubs while calling the actual history HTML writer. */
#define main existing_history_export_fixture_main
#include "test_history_export.c"
#undef main

static void ordinal_json(FILE *file,const char *text)
{
    fputc('"',file);
    for(const unsigned char *p=(const unsigned char *)text;*p;++p) {
        if(*p=='"'||*p=='\\')fputc('\\',file);
        if(*p<32)fprintf(file,"\\u%04x",*p);else fputc(*p,file);
    }
    fputc('"',file);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    ui_history_record_t record={.valid=true,.record_no=91,.pcs=85,.amount=425,.year=2026,.month=10,.day=13};
    strcpy(record.currency,"USD");
    history_record_detail_t detail={0};
    detail.reject_count=3;detail.saved_reject_pcs=6;
    detail.rejects[0]=(history_detail_reject_t){4,2,0x16};
    detail.rejects[1]=(history_detail_reject_t){9,1,0x1e};
    detail.rejects[2]=(history_detail_reject_t){20,3,0x2e};
    char path[1024];snprintf(path,sizeof(path),"%s/history-manifest.json",argv[1]);
    FILE *manifest=fopen(path,"wb");assert(manifest);fputc('[',manifest);
    for(size_t i=0;i<ui_lang_count();++i) {
        const ui_locale_t *locale=ui_lang_at(i);assert(locale);
        g_history_export_language=locale->id;
        snprintf(path,sizeof(path),"%s/history-%s.html",argv[1],locale->tag);
        assert(history_export_write_html_file(path,&record,&detail));
        if(i)fputc(',',manifest);
        fputs("{\"kind\":\"history\",\"scenario\":1,\"rejectCount\":6,\"path\":",manifest);ordinal_json(manifest,path);
        fputs(",\"tag\":",manifest);ordinal_json(manifest,locale->tag);
        fputs(",\"expected\":{\"No.\":",manifest);ordinal_json(manifest,ui_tr_for(locale->id,"No."));
        fputs("}}",manifest);
    }
    fputs("]\n",manifest);assert(!fclose(manifest));
    puts("history report producer: PASS (10 captured locales, original sparse reject ordinals)");
    return 0;
}
