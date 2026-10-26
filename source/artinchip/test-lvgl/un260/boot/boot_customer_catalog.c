#include "boot_customer_catalog.h"
#include <string.h>

#define HEADER 32U
#define ENTRY 64U
#define MAX_BYTES (8U*1024U*1024U)
static uint32_t u32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }

bool boot_customer_catalog_open(boot_customer_catalog_t *out, const uint8_t *p, size_t n)
{
    if (!out) return false;
    memset(out,0,sizeof(*out));
    if (!p || n<HEADER || n>MAX_BYTES || memcmp(p,"UNBCAT1\0",8) ||
        u32(p+8)!=n || !u32(p+12) || u32(p+12)>64 || u32(p+16)!=ENTRY ||
        u32(p+20)!=HEADER+u32(p+12)*ENTRY || u32(p+20)>n || u32(p+24) || u32(p+28)) return false;
    unsigned count=u32(p+12), end=u32(p+20), required=0;
    for (unsigned i=0;i<count;i++) {
        const uint8_t *e=p+HEADER+i*ENTRY;
        unsigned id=u32(e), fmt=u32(e+4), w=u32(e+8), h=u32(e+12);
        unsigned off=u32(e+16), bytes=u32(e+20);
        int32_t x=(int32_t)u32(e+24), y=(int32_t)u32(e+28);
        if (!id || id>13 || (fmt!=1 && fmt!=2) || !w || !h || w>1280 || h>400 ||
            bytes!=w*h*(fmt==1?4U:1U) || off!=end || off>n || bytes>n-off ||
            x<0 || y<0 || x>1280-(int32_t)w || y>400-(int32_t)h ||
            u32(e+32)>0xFFFFFFU || u32(e+36) || u32(e+56) || u32(e+60) || !memchr(e+40,0,16)) return false;
        bool title=id>=9 && id<=12;
        if (title ? (!e[40] || fmt!=2) : e[40]!=0) return false;
        if (id==1 && (w!=1280 || h!=400 || fmt!=1)) return false;
        if (id>=4 && id<=8 && (w!=200 || h!=220 || fmt!=1)) return false;
        for(unsigned k=0;k<i;k++) {
            const uint8_t *other=p+HEADER+k*ENTRY;
            if(u32(other)==id && !strcmp((const char *)other+40,(const char *)e+40)) return false;
        }
        if (!title || !strcmp((const char *)e+40,"en")) required|=1U<<id;
        end=off+bytes;
    }
    if (end!=n || required!=0x3FFEU) return false;
    out->data=p;out->size=n;out->count=count;
    return true;
}

bool boot_customer_catalog_find(const boot_customer_catalog_t *cat, unsigned id,
                                const char *tag, boot_customer_image_t *out)
{
    if(!cat || !cat->data || !out) return false;
    if(id==BOOT_CUSTOMER_ART_READY_ORANGE)id=BOOT_CUSTOMER_ART_READY_GRAY;
    const uint8_t *found=NULL;
    for(unsigned i=0;i<cat->count;i++) {
        const uint8_t *e=cat->data+HEADER+i*ENTRY;
        if(u32(e)!=id)continue;
        const char *lang=(const char *)e+40;
        if(!*lang || (tag && !strcmp(lang,tag))) {found=e;break;}
        if(!strcmp(lang,"en"))found=e;
    }
    if(!found)return false;
    out->data=cat->data+u32(found+16);out->width=u32(found+8);out->height=u32(found+12);
    out->format=u32(found+4);out->x=(int32_t)u32(found+24);out->y=(int32_t)u32(found+28);out->color=u32(found+32);
    return true;
}
