#include "lvgl.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/storage/ui_locale_store.h"
#include "un260/font/ui_locale_font_bank.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv)
{
    assert(argc==3);
    lv_init();ui_lang_init();ui_lang_set_resource_preparer(ui_locale_font_prepare);
    assert(ui_lang_get()==LANGUAGE_EN);
    const ui_locale_t *expected=ui_lang_find(argv[2]);assert(expected);
    if(!strcmp(argv[1],"save")) {
        assert(ui_lang_save(expected->id));
    } else {
        assert(!strcmp(argv[1],"restore"));
        char tag[UI_LOCALE_TAG_CAPACITY];assert(ui_locale_store_read(tag));
        ui_lang_restore(tag);
    }
    assert(ui_lang_get()==expected->id);
    if(expected->id!=LANGUAGE_EN)assert(ui_locale_font_get(expected->id,16));
    printf("PASS fresh process %s %s with real storage and font preflight\n",argv[1],argv[2]);
    return 0;
}
