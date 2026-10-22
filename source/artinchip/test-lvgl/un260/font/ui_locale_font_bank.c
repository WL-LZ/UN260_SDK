#include "ui_locale_font_bank.h"
#include "ui_font_fallback.h"
#include <string.h>
#include <stdio.h>
#include <zlib.h>
#ifndef UN260_LOCALE_FONT_DIR
#define UN260_LOCALE_FONT_DIR LVGL_DIR "locale_fonts/"
#endif
struct font_asset { unsigned pixels; uint32_t bytes, crc; const char *file; };
#include "ui_locale_font_assets.inc"
struct composed_font { const lv_font_t *base; unsigned pixels; lv_font_t font; };
struct locale_bank {
    language_t language;
    const struct font_asset *assets;
    size_t count;
    lv_font_t *fonts[UN260_LOCALE_FONT_SIZE_COUNT];
    bool ready;
    struct composed_font composed[128];
};
static struct locale_bank banks[] = UN260_LOCALE_BANKS;
/* Font decoding makes many one-byte reads and seeks. Keep its read cache
 * private: image/DMA filesystem policy must not change with language support. */
static lv_fs_drv_t font_reader;
static lv_fs_drv_t *font_source;
static bool font_reader_ready(lv_fs_drv_t *drv)
{ (void)drv; return !font_source->ready_cb || font_source->ready_cb(font_source); }
static void *font_reader_open(lv_fs_drv_t *drv,const char *path,lv_fs_mode_t mode)
{ (void)drv; return mode==LV_FS_MODE_RD ? font_source->open_cb(font_source,path,mode) : NULL; }
static lv_fs_res_t font_reader_close(lv_fs_drv_t *drv,void *file)
{ (void)drv; return font_source->close_cb(font_source,file); }
static lv_fs_res_t font_reader_read(lv_fs_drv_t *drv,void *file,void *buf,uint32_t size,uint32_t *read)
{ (void)drv; return font_source->read_cb(font_source,file,buf,size,read); }
static lv_fs_res_t font_reader_seek(lv_fs_drv_t *drv,void *file,uint32_t pos,lv_fs_whence_t whence)
{ (void)drv; return font_source->seek_cb(font_source,file,pos,whence); }
static lv_fs_res_t font_reader_tell(lv_fs_drv_t *drv,void *file,uint32_t *pos)
{ (void)drv; return font_source->tell_cb(font_source,file,pos); }
static bool buffered_font_path(char *path)
{
    lv_fs_drv_t *source=lv_fs_get_drv(path[0]);
    if(!source || !source->open_cb || !source->close_cb || !source->read_cb ||
       !source->seek_cb || !source->tell_cb) return false;
    if(!font_source) {
        char letter;
        for(letter='Z';letter>='A' && lv_fs_get_drv(letter);--letter) {}
        if(letter<'A') return false;
        lv_fs_drv_init(&font_reader);
        font_source=source;
        font_reader.letter=letter;
        font_reader.cache_size=4096;
        font_reader.ready_cb=font_reader_ready;
        font_reader.open_cb=font_reader_open;
        font_reader.close_cb=font_reader_close;
        font_reader.read_cb=font_reader_read;
        font_reader.seek_cb=font_reader_seek;
        font_reader.tell_cb=font_reader_tell;
        lv_fs_drv_register(&font_reader);
    }
    if(font_source!=source) return false;
    path[0]=font_reader.letter;
    return true;
}
static struct locale_bank *find_bank(language_t language)
{
    for (size_t i=0;i<sizeof(banks)/sizeof(banks[0]);++i)
        if(banks[i].language==language) return &banks[i];
    return NULL;
}
static bool asset_path(char *path,size_t capacity,const struct font_asset *asset)
{
    int n=snprintf(path,capacity,"%s%s",UN260_LOCALE_FONT_DIR,asset->file);
    return n>0&&(size_t)n<capacity;
}
static bool asset_valid(const struct font_asset *asset)
{
    char path[256]; lv_fs_file_t file;
    if(!asset_path(path,sizeof(path),asset)||lv_fs_open(&file,path,LV_FS_MODE_RD)!=LV_FS_RES_OK) return false;
    unsigned char bytes[2048]; uint32_t total=0,read=0;
    uLong crc=crc32(0L,Z_NULL,0); bool ok=true;
    do {
        if(lv_fs_read(&file,bytes,sizeof(bytes),&read)!=LV_FS_RES_OK) {ok=false;break;}
        if(read>asset->bytes-total) {ok=false;break;}
        total+=read;crc=crc32(crc,bytes,read);
    } while(read);
    if(lv_fs_close(&file)!=LV_FS_RES_OK) ok=false;
    return ok&&total==asset->bytes&&(uint32_t)crc==asset->crc;
}
bool ui_locale_font_prepare(language_t language)
{
    if(language==LANGUAGE_EN) return true;
    struct locale_bank *bank=find_bank(language);
    if(!bank) return false;
    if(bank->ready) return true;
    for(size_t i=0;i<bank->count;++i) if(!asset_valid(&bank->assets[i])) return false;
    bool added[UN260_LOCALE_FONT_SIZE_COUNT]={false};
    for(size_t i=0;i<bank->count;++i) {
        if(bank->fonts[i]) continue;
        char path[256];
        if(!asset_path(path,sizeof(path),&bank->assets[i]) || !buffered_font_path(path)) goto failed;
        bank->fonts[i]=lv_font_load(path);
        if(!bank->fonts[i]) goto failed;
        added[i]=true;
    }
    bank->ready=true; return true;
failed:
    for(size_t i=0;i<bank->count;++i) {
        if(added[i]) {lv_font_free(bank->fonts[i]);bank->fonts[i]=NULL;}
    }
    return false;
}
bool ui_locale_font_prepare_size(language_t language,unsigned pixels)
{
    if(language==LANGUAGE_EN) return true;
    struct locale_bank *bank=find_bank(language);
    if(!bank) return false;
    for(size_t i=0;i<bank->count;++i) {
        if(bank->assets[i].pixels!=pixels) continue;
        if(bank->fonts[i]) return true;
        char path[256];
        if(!asset_valid(&bank->assets[i]) || !asset_path(path,sizeof(path),&bank->assets[i]) ||
           !buffered_font_path(path)) return false;
        bank->fonts[i]=lv_font_load(path);
        return bank->fonts[i]!=NULL;
    }
    return false;
}
const lv_font_t *ui_locale_font_get(language_t language,unsigned pixels)
{
    struct locale_bank *bank=find_bank(language);
    if(!bank) return NULL;
    for(size_t i=0;i<bank->count;++i)
        if(bank->assets[i].pixels==pixels) return bank->fonts[i];
    return NULL;
}

const lv_font_t *ui_locale_font_compose(const lv_font_t *base,unsigned pixels,language_t language)
{
    if(!base||language==LANGUAGE_EN) return base;
    struct locale_bank *bank=find_bank(language);
    const lv_font_t *fallback=ui_locale_font_get(language,pixels);
    if(!bank||!fallback) return NULL;
    for(size_t i=0;i<sizeof(bank->composed)/sizeof(bank->composed[0]);++i) {
        struct composed_font *item=&bank->composed[i];
        if(item->base==base&&item->pixels==pixels) return &item->font;
        if(!item->base) {
            ui_font_compose_fallback(&item->font,base,fallback);
            item->pixels=pixels;item->base=base;return &item->font;
        }
    }
    return NULL;
}
