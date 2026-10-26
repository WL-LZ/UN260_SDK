#include "boot_customer_art.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

extern const unsigned char boot_customer_blob[];
extern const size_t boot_customer_blob_size, boot_customer_blob_raw_size;
static struct {
    unsigned owners;
    uint8_t *pixels;
    uint8_t *tinted[15];
    lv_img_dsc_t images[15];
    lv_coord_t x[15], y[15];
} art;

static void clear(void)
{
    for(unsigned id=1;id<15;id++) {
        if(art.images[id].data)lv_img_cache_invalidate_src(&art.images[id]);
        free(art.tinted[id]);
    }
    free(art.pixels);
    memset(&art,0,sizeof(art));
}

bool boot_customer_art_acquire(const char *language_tag)
{
    if(art.owners) { art.owners++; return true; }
    if(!boot_customer_blob_raw_size || boot_customer_blob_raw_size>8U*1024U*1024U ||
       boot_customer_blob_size>2U*1024U*1024U)return false;
    art.pixels=malloc(boot_customer_blob_raw_size);
    if(!art.pixels)return false;
    uLongf length=boot_customer_blob_raw_size;
    boot_customer_catalog_t cat;
    if(uncompress(art.pixels,&length,boot_customer_blob,boot_customer_blob_size)!=Z_OK ||
       length!=boot_customer_blob_raw_size || !boot_customer_catalog_open(&cat,art.pixels,length)) {
        clear();return false;
    }
    for(unsigned id=1;id<15;id++) {
        boot_customer_image_t image;
        if(!boot_customer_catalog_find(&cat,id,language_tag,&image)) {clear();return false;}
        const uint8_t *data=image.data;
        if(image.format==BOOT_CUSTOMER_A8) {
            size_t count=image.width*image.height;
            uint8_t *p=malloc(count*4);
            if(!p) {clear();return false;}
            art.tinted[id]=p;data=p;
            uint32_t rgb=image.color;
            if(id==BOOT_CUSTOMER_ART_READY_GRAY)rgb=0x353734;
            if(id==BOOT_CUSTOMER_ART_READY_ORANGE)rgb=0xE86F30;
            for(size_t n=0;n<count;n++) {
                p[n*4]=(uint8_t)rgb;p[n*4+1]=(uint8_t)(rgb>>8);
                p[n*4+2]=(uint8_t)(rgb>>16);p[n*4+3]=image.data[n];
            }
        }
        lv_img_dsc_t *d=&art.images[id];
        /* The approved background is fully opaque. Ordinary copy blending
         * avoids an alpha pass beneath each local animation update. */
        d->header.cf=id==BOOT_CUSTOMER_ART_BG?LV_IMG_CF_TRUE_COLOR:LV_IMG_CF_TRUE_COLOR_ALPHA;
        d->header.w=image.width;d->header.h=image.height;
        d->data_size=image.width*image.height*4;d->data=data;
        art.x[id]=image.x;art.y[id]=image.y;
    }
    art.owners=1;
    return true;
}

const lv_img_dsc_t *boot_customer_art_image(unsigned id)
{ return art.owners && id>0 && id<15 ? &art.images[id] : NULL; }
bool boot_customer_art_position(unsigned id, lv_coord_t *x, lv_coord_t *y)
{
    if(!art.owners || !id || id>=15)return false;
    if(x)*x=art.x[id];
    if(y)*y=art.y[id];
    return true;
}
void boot_customer_art_release(void)
{ if(art.owners && !--art.owners)clear(); }
