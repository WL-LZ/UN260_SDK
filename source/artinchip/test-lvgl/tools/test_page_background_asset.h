#ifndef TEST_PAGE_BACKGROUND_ASSET_H
#define TEST_PAGE_BACKGROUND_ASSET_H
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "aic_ui/compiled_asset.h"
/* Test-only decoder bridge for the exact generated PNGs, outside LVGL's heap
 * just like the production DMA image cache. Never linked into firmware. */
static un260_compiled_asset_t test_backgrounds[2]={
    {"backgrounds/user.png",1280,400,5120,1,NULL},
    {"backgrounds/settings.png",1280,400,5120,1,NULL}
};
static void test_background_release(void){for(unsigned i=0;i<2;i++){free((void*)test_backgrounds[i].pixels);test_backgrounds[i].pixels=NULL;}}
static const un260_compiled_asset_t *test_page_asset_find(const char *src){
    const un260_compiled_asset_t *a=un260_compiled_asset_find(src);if(a)return a;
    const char *prefix="L:/usr/local/share/lvgl_data/";
    if(strncmp(src,prefix,strlen(prefix)))return NULL;
    for(unsigned i=0;i<2;i++)if(!strcmp(src+strlen(prefix),test_backgrounds[i].name)){
        if(!test_backgrounds[i].pixels){
            static bool registered;if(!registered){atexit(test_background_release);registered=true;}
            const char *dir=getenv("UN260_BACKGROUND_DIR");assert(dir);
            char p[2048];snprintf(p,sizeof(p),"%s/%s.bgra",dir,i?"settings":"user");
            FILE*f=fopen(p,"rb");assert(f);unsigned char*data=malloc(1280*400*4);assert(data);
            assert(fread(data,1,1280*400*4,f)==1280*400*4);fclose(f);test_backgrounds[i].pixels=data;
        }
        return &test_backgrounds[i];
    }return NULL;
}
#endif
