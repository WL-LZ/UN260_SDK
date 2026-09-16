#include "scaled_font.h"
#include <string.h>
static int scaled(int n,unsigned percent){return n<0?-((-n*(int)percent+50)/100):(n*(int)percent+50)/100;}
static bool glyph_dsc(const lv_font_t *font,lv_font_glyph_dsc_t *out,uint32_t letter,uint32_t next){
    scaled_font_t *s=(scaled_font_t*)font->dsc;
    if(!lv_font_get_glyph_dsc(s->base,out,letter,next))return false;
    out->adv_w=scaled(out->adv_w,s->percent);out->box_w=scaled(out->box_w,s->percent);out->box_h=scaled(out->box_h,s->percent);
    out->ofs_x=scaled(out->ofs_x,s->percent);out->ofs_y=scaled(out->ofs_y,s->percent);out->bpp=8;
    return (uint32_t)out->box_w*out->box_h<=s->capacity;
}
static unsigned alpha(const uint8_t *pixels,unsigned index,unsigned bpp){
    unsigned bit=index*bpp,mask=(1U<<bpp)-1;
    return ((pixels[bit/8]>>(8-bpp-bit%8))&mask)*255/mask;
}
static const uint8_t *glyph_bitmap(const lv_font_t *font,uint32_t letter){
    scaled_font_t *s=(scaled_font_t*)font->dsc;lv_font_glyph_dsc_t d;
    if(!lv_font_get_glyph_dsc(s->base,&d,letter,0))return NULL;
    unsigned w=scaled(d.box_w,s->percent),h=scaled(d.box_h,s->percent);
    if(!w||!h||w*h>s->capacity||!(d.bpp==1||d.bpp==2||d.bpp==4||d.bpp==8))return NULL;
    const uint8_t *src=lv_font_get_glyph_bitmap(s->base,letter);if(!src)return NULL;
    /* Pixel-centred bilinear alpha preserves thin stems better than nearest-neighbour. */
    for(unsigned y=0;y<h;y++){
        int fy=((2*y+1)*d.box_h*128/h)-128;fy=LV_CLAMP(0,fy,((int)d.box_h-1)*256);
        unsigned y0=fy/256,y1=LV_MIN(y0+1,d.box_h-1),wy=fy%256;
        for(unsigned x=0;x<w;x++){
            int fx=((2*x+1)*d.box_w*128/w)-128;fx=LV_CLAMP(0,fx,((int)d.box_w-1)*256);
            unsigned x0=fx/256,x1=LV_MIN(x0+1,d.box_w-1),wx=fx%256;
            unsigned a=alpha(src,y0*d.box_w+x0,d.bpp)*(256-wx)+alpha(src,y0*d.box_w+x1,d.bpp)*wx;
            unsigned b=alpha(src,y1*d.box_w+x0,d.bpp)*(256-wx)+alpha(src,y1*d.box_w+x1,d.bpp)*wx;
            s->pixels[y*w+x]=(a*(256-wy)+b*wy+32768)/65536;
        }
    }
    return s->pixels;
}
bool scaled_font_init(scaled_font_t *s,const lv_font_t *base,unsigned percent,uint8_t *pixels,uint32_t capacity){
    if(!s||!base||!pixels||!capacity||percent<25||percent>140)return false;
    memset(s,0,sizeof(*s));s->base=base;s->percent=percent;s->pixels=pixels;s->capacity=capacity;
    s->font=*base;s->font.dsc=s;s->font.get_glyph_dsc=glyph_dsc;s->font.get_glyph_bitmap=glyph_bitmap;
    s->font.line_height=scaled(base->line_height,percent);s->font.base_line=scaled(base->base_line,percent);
    s->font.underline_position=scaled(base->underline_position,percent);s->font.underline_thickness=LV_MAX(1,scaled(base->underline_thickness,percent));s->font.fallback=NULL;
    return true;
}
