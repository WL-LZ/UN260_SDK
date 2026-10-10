#define _GNU_SOURCE
#include "boot_theme_pack.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static bool read_all(int fd, uint8_t *dst, size_t size)
{
    while(size) {
        ssize_t n=read(fd,dst,size);
        if(n<0 && errno==EINTR)continue;
        if(n<=0)return false;
        dst+=n;size-=(size_t)n;
    }
    return true;
}
void boot_theme_pack_free(boot_theme_pack_t *pack)
{
    if(!pack)return;
    free(pack->data);memset(pack,0,sizeof(*pack));
}
static bool valid_scene(const boot_theme_scene_t *s)
{
    /* This renderer's dirty canvas is 500 x active_h, x=390. Reject geometry
     * it cannot safely compose; a new layout can ship a new versioned player. */
    if(!s->entrance_duration || !s->brand_exit_duration || !s->welcome_duration ||
       !s->dot_period || s->dot_period>10000U || s->motion_scale>1U ||
       s->entrance_start>10000U || s->entrance_duration>10000U ||
       s->brand_exit_start>10000U || s->brand_exit_duration>10000U ||
       s->welcome_start>10000U || s->welcome_duration>10000U ||
       s->welcome_settled>10000U || s->dot_start>10000U ||
       s->active_y>400U || s->active_h>400U-s->active_y || !s->active_h ||
       s->text_y>400U-132U || s->text_y<s->active_y || s->text_y+132U>s->active_y+s->active_h ||
       s->dot_size<1U || s->dot_size>16U || s->dot_radius>128U ||
       s->dot_x<390U || s->dot_x>890U || s->dot_step>64U ||
       s->dot_x+2U*s->dot_step+s->dot_size>890U ||
       s->dot_y>400U || s->dot_y<s->active_y+6U || s->dot_y+s->dot_size>s->active_y+s->active_h)
        return false;
    return true;
}
bool boot_theme_pack_load_fd(int fd, boot_theme_pack_t *pack)
{
    uint8_t h[BOOT_THEME_PACK_HEADER_BYTES];uint8_t *packed=NULL;
    struct stat info;z_stream stream={0};bool inflated=false,ok=false;
    memset(pack,0,sizeof(*pack));
    if(fstat(fd,&info) || !S_ISREG(info.st_mode) ||
       info.st_size<(off_t)sizeof(h) || lseek(fd,0,SEEK_SET)<0 || !read_all(fd,h,sizeof(h)))goto done;
    if(memcmp(h,"UNBTPK1\0",8) || word(h+8)!=1U || word(h+12)!=sizeof(h) ||
       word(h+16)!=BOOT_THEME_WIDTH || word(h+20)!=BOOT_THEME_HEIGHT ||
       (word(h+24)!=3U && word(h+24)!=4U) ||
       (word(h+28)!=BOOT_THEME_MODE_ANIMATION && word(h+28)!=BOOT_THEME_MODE_LOGO))goto done;
    uint32_t raw_bytes=word(h+32),packed_bytes=word(h+36),raw_crc=word(h+40);
    if(!packed_bytes || packed_bytes>BOOT_THEME_PACK_MAX_PACKED ||
       raw_bytes>BOOT_THEME_PACK_MAX_RAW ||
       info.st_size!=(off_t)(sizeof(h)+packed_bytes) || word(h+48)!=BOOT_THEME_BASE_BYTES)goto done;
    pack->theme=word(h+24);pack->mode=word(h+28);pack->data_bytes=raw_bytes;
    pack->language_count=word(h+44);pack->logo_offset=word(h+52);
    pack->language_table=word(h+56);pack->native_offset=word(h+60);pack->native_bytes=word(h+64);
    pack->min_show_ms=word(h+68);memcpy(pack->identity,h+72,32);
    pack->scene=(boot_theme_scene_t){
        .entrance_start=word(h+104),.entrance_duration=word(h+108),
        .brand_exit_start=word(h+112),.brand_exit_duration=word(h+116),
        .welcome_start=word(h+120),.welcome_duration=word(h+124),.welcome_settled=word(h+128),
        .dot_start=word(h+132),.dot_period=word(h+136),.text_y=word(h+140),
        .dot_y=word(h+144),.dot_x=word(h+148),.dot_step=word(h+152),
        .dot_size=word(h+156),.dot_radius=word(h+160),.active_y=word(h+164),
        .active_h=word(h+168),.motion_scale=word(h+172)};
    if(word(h+176) || word(h+180) || pack->min_show_ms>10000U ||
       pack->language_count<1U || pack->language_count>32U ||
       pack->logo_offset!=BOOT_THEME_BASE_BYTES ||
       pack->language_table!=pack->logo_offset+BOOT_THEME_BG_BYTES ||
       pack->native_offset!=pack->language_table+pack->language_count*BOOT_THEME_LANGUAGE_BYTES ||
       pack->native_offset>raw_bytes || pack->native_bytes!=raw_bytes-pack->native_offset ||
       !valid_scene(&pack->scene))goto done;
    if(pack->theme==3U && (pack->scene.active_y>82U || pack->scene.active_y+pack->scene.active_h<184U))goto done;
    packed=malloc(packed_bytes);pack->data=malloc(raw_bytes);
    if(!packed || !pack->data || !read_all(fd,packed,packed_bytes))goto done;
    if(inflateInit(&stream)!=Z_OK)goto done;
    inflated=true;stream.next_in=packed;stream.avail_in=packed_bytes;
    stream.next_out=pack->data;stream.avail_out=raw_bytes;
    if(inflate(&stream,Z_FINISH)!=Z_STREAM_END || stream.total_out!=raw_bytes ||
       stream.total_in!=packed_bytes || crc32(0,pack->data,raw_bytes)!=raw_crc)goto done;
    for(unsigned i=0;i<pack->language_count;i++) {
        const uint8_t *entry=pack->data+pack->language_table+i*BOOT_THEME_LANGUAGE_BYTES;
        uint32_t offset=word(entry+16),w=word(entry+20),height=word(entry+24),y=word(entry+28);
        if(!entry[0] || !memchr(entry,0,16) || !w || w>500U || !height || height>128U ||
           y>128U-height || offset>pack->native_bytes ||
           (w*height+1U)/2U>pack->native_bytes-offset)goto done;
        for(unsigned j=0;j<i;j++)
            if(!strcmp((const char*)entry,(const char*)pack->data+pack->language_table+j*BOOT_THEME_LANGUAGE_BYTES))goto done;
    }
    ok=true;
done:
    if(inflated)inflateEnd(&stream);
    free(packed);
    if(!ok)boot_theme_pack_free(pack);
    return ok;
}
bool boot_theme_pack_load_path(const char *path, boot_theme_pack_t *pack)
{
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0){memset(pack,0,sizeof(*pack));return false;}
    bool ok=boot_theme_pack_load_fd(fd,pack);close(fd);return ok;
}
bool boot_theme_pack_select_language(boot_theme_pack_t *pack,const char *tag)
{
    if(!pack->data || pack->frozen)return false;
    unsigned index=0;
    if(tag)for(unsigned i=0;i<pack->language_count;i++) {
        const char *entry=(const char*)pack->data+pack->language_table+i*BOOT_THEME_LANGUAGE_BYTES;
        if(!strcmp(tag,entry)){index=i;break;}
    }
    const uint8_t *entry=pack->data+pack->language_table+index*BOOT_THEME_LANGUAGE_BYTES;
    uint32_t offset=word(entry+16),w=word(entry+20),h=word(entry+24),top=word(entry+28);
    uint8_t *dst=pack->data+BOOT_THEME_BG_BYTES+BOOT_THEME_ICON_BYTES+BOOT_THEME_TEXT_BYTES;
    memset(dst,0,BOOT_THEME_TEXT_BYTES);
    const uint8_t *src=pack->data+pack->native_offset+offset;
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++) {
        unsigned n=y*w+x;
        uint8_t *p=dst+((y+top)*500U+(500U-w)/2U+x)*4U;
        p[0]=pack->theme==4U?130U:245U;p[1]=pack->theme==4U?109U:244U;p[2]=pack->theme==4U?82U:242U;
        p[3]=((n&1U)?src[n/2U]&15U:src[n/2U]>>4)*17U;
    }
    pack->language=index;
    return true;
}
bool boot_theme_pack_freeze(boot_theme_pack_t *pack)
{
    if(!pack->data)return false;
    if(pack->frozen)return true;
    size_t used=BOOT_THEME_BASE_BYTES;
    if(pack->mode==BOOT_THEME_MODE_LOGO) {
        memmove(pack->data,pack->data+pack->logo_offset,BOOT_THEME_BG_BYTES);
        pack->logo_offset=0;used=BOOT_THEME_BG_BYTES;
    }
    uint8_t *smaller=realloc(pack->data,used);
    if(smaller)pack->data=smaller;
    pack->data_bytes=used;pack->frozen=true;
    return true;
}
