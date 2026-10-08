#include "un260/lv_system/ui_lang.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool ready,store_ok;
static unsigned preparations,writes;
static char saved[32];
static bool prepare(language_t lang){assert(lang!=LANGUAGE_EN);++preparations;return ready;}
bool ui_locale_store_write(const char *tag){++writes;if(!store_ok)return false;snprintf(saved,sizeof(saved),"%s",tag);return true;}
int main(void){
 ui_lang_init();assert(ui_lang_is_available(ui_lang_find("ar")));
 assert(!ui_lang_save(LANGUAGE_AR)&&writes==0);
 ui_lang_restore("ar");assert(ui_lang_get()==LANGUAGE_EN);
 ui_lang_set_resource_preparer(prepare);
 assert(!ui_lang_prepare_resources(UI_LANGUAGE_INVALID)&&preparations==0);
 assert(!ui_lang_save(LANGUAGE_AR)&&writes==0&&preparations==1);
 ui_lang_restore("ar");assert(ui_lang_get()==LANGUAGE_EN&&preparations==2);
 ready=true;uint32_t generation=ui_lang_generation();
 assert(!ui_lang_save(LANGUAGE_AR)&&writes==1);
 assert(ui_lang_get()==LANGUAGE_EN&&ui_lang_generation()==generation);
 store_ok=true;assert(ui_lang_save(LANGUAGE_AR));assert(!strcmp(saved,"ar"));
 assert(ui_lang_get()==LANGUAGE_AR&&ui_lang_generation()==generation+1);
 ready=false;generation=ui_lang_generation();unsigned before=writes;
 assert(!ui_lang_save(LANGUAGE_JA)&&writes==before);
 assert(ui_lang_get()==LANGUAGE_AR&&ui_lang_generation()==generation);
 ui_lang_restore("ja");assert(ui_lang_get()==LANGUAGE_EN);
 before=preparations;assert(ui_lang_save(LANGUAGE_EN));assert(preparations==before);
 ready=true;for(size_t i=0;i<ui_lang_count();++i){assert(ui_lang_save(ui_lang_at(i)->id));assert(ui_lang_get()==ui_lang_at(i)->id);}
 puts("PASS locale resource preflight: missing assets, failed storage, restore fallback, all locale saves");
 return 0;
}
