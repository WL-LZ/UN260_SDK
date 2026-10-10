/* Maintenance-only display. No LVGL, protocol, motor or touch dependency.
 * Uses boot_light.c's existing framebuffer validation and display lease. */
#ifndef UN260_UPGRADE_DISPLAY_H
#define UN260_UPGRADE_DISPLAY_H
#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_update_message.h"
/* Fixed localized labels are pre-shaped offline and frozen into this executable.
 * They remain usable when the normal UI or installed font assets cannot start. */
#include "un260/storage/ui_locale_reader.h"
#include "un260/font/upgrade_native_art.h"
#include <zlib.h>
#ifndef UPGRADE_LOCALE_PATH
#define UPGRADE_LOCALE_PATH "/etc/ui_state/language.cfg"
#endif
static const char *upgrade_copy(const char *key) { return key; }
#ifndef UPGRADE_STATUS_PATH
#define UPGRADE_STATUS_PATH "/tmp/ui_update.status"
#endif
static const unsigned char upgrade_glyphs[36][5] = {
 {126,17,17,17,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},
 {127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},
 {0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},
 {127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},
 {62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},
 {63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},
 {3,4,120,4,3},{97,81,73,69,67},{62,81,73,69,62},{0,66,127,64,0},
 {98,81,73,73,70},{34,65,73,73,54},{24,20,18,127,16},{39,69,69,69,57},
 {60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}
};
static void upgrade_rect(uint8_t *pixels,unsigned stride,int x,int y,int w,int h,uint32_t color)
{
    for(int row=y;row<y+h&&row<400;row++) for(int col=x;col<x+w&&col<1280;col++)
        if(row>=0&&col>=0)memcpy(pixels+(size_t)row*stride+(size_t)col*4,&color,4);
}
/* Cache each immutable 4bpp word only on first use; no per-frame decode. */
static unsigned char *upgrade_native_cache[UPGRADE_ART_KEY_COUNT];
static unsigned upgrade_native_locale;
static int upgrade_native_locale_ready;
static int upgrade_native_key(const char *key)
{
    for(unsigned n=0;n<UPGRADE_ART_KEY_COUNT;n++)
        if(!strcmp(key,upgrade_art_keys[n]))return (int)n;
    return -1;
}
static int upgrade_native_text(uint8_t *pixels,unsigned stride,int x,int y,const char *key,uint32_t color)
{
    int index=upgrade_native_key(key);
    if(index<0)return 0;
    if(!upgrade_native_locale_ready) {
        char tag[UI_LOCALE_TAG_CAPACITY];
        if(ui_locale_read_file(UPGRADE_LOCALE_PATH,tag))
            for(unsigned n=0;n<UPGRADE_ART_LOCALE_COUNT;n++)
                if(!strcmp(tag,upgrade_art_tags[n])) { upgrade_native_locale=n;break; }
        upgrade_native_locale_ready=1;
    }
    unsigned entry=upgrade_native_locale*UPGRADE_ART_KEY_COUNT+(unsigned)index;
    unsigned w=upgrade_art_rows[entry].w,h=upgrade_art_rows[entry].h;
    size_t size=((size_t)w*h+1)/2;
    if(!w||!h||w>1152||h>56||size>32256)return 0;
    if(!upgrade_native_cache[index]) {
        unsigned char *data=malloc(size);
        if(!data)return 0;
        uLongf decoded=(uLongf)size;
        if(uncompress(data,&decoded,upgrade_art_pixels+upgrade_art_rows[entry].offset,
                      upgrade_art_rows[entry].bytes)!=Z_OK||decoded!=size) {free(data);return 0;}
        upgrade_native_cache[index]=data;
    }
    const unsigned char *data=upgrade_native_cache[index];
    for(unsigned row=0;row<h;row++)for(unsigned column=0;column<w;column++) {
        int px=x+(int)column,py=y+(int)row;
        if(px<0||px>=1280||py<0||py>=400)continue;
        unsigned position=row*w+column;
        unsigned alpha=((position&1)?data[position/2]&15:data[position/2]>>4)*17U;
        if(!alpha)continue;
        uint8_t *destination=pixels+(size_t)py*stride+(size_t)px*4;
        for(unsigned channel=0;channel<3;channel++)
            destination[channel]=(uint8_t)((((color>>(channel*8))&255U)*alpha+destination[channel]*(255U-alpha)+127U)/255U);
        destination[3]=255;
    }
    return 1;
}
static void upgrade_text(uint8_t *pixels,unsigned stride,int x,int y,const char *text,int scale,uint32_t color)
{
    if(upgrade_native_text(pixels,stride,x,y,text,color))return;
    for(;*text&&x<1220;text++,x+=6*scale) {
        unsigned char c=(unsigned char)*text;
        if(c>='a'&&c<='z')c-=32;
        int index=c>='A'&&c<='Z'?c-'A':c>='0'&&c<='9'?26+c-'0':-1;
        if(index>=0)for(int col=0;col<5;col++)for(int row=0;row<7;row++)
            if(upgrade_glyphs[index][col]&(1U<<row))
                upgrade_rect(pixels,stride,x+col*scale,y+row*scale,scale,scale,color);
    }
}
static void upgrade_render(uint8_t *pixels,unsigned stride)
{
    const char *fault=getenv("UN260_RUNTIME_FAULT_DISPLAY");
    if(fault && !strcmp(fault,"1")) {
        /* Frozen regional words remain usable without LVGL/assets. */
        upgrade_rect(pixels,stride,0,0,1280,400,0xfff3f5f6);
        upgrade_text(pixels,stride,64,42,upgrade_copy(UI_N_("UN260 PROTECTION MODE")),4,0xffb76313);
        upgrade_text(pixels,stride,64,116,upgrade_copy(UI_N_("STOP FEEDING BANKNOTES")),4,0xff20313b);
        upgrade_text(pixels,stride,64,184,upgrade_copy(UI_N_("KEEP NOTES SEPARATE AND RECOUNT")),3,0xff20313b);
        upgrade_text(pixels,stride,64,242,upgrade_copy(UI_N_("COUNT AND CONTROLLER STATE REQUIRE CHECKING")),3,0xff20313b);
        upgrade_text(pixels,stride,64,320,upgrade_copy(UI_N_("CONTACT SERVICE  DO NOT RESUME COUNTING")),3,0xff576c79);
        return;
    }
    int progress=0,failed=0,success=0,waiting=0;
    char line[256],message[160];
    snprintf(message,sizeof(message),"%s",upgrade_copy(UI_N_("CHECKING THE SAME USB PACKAGE")));
    FILE *status=fopen(UPGRADE_STATUS_PATH,"r");
    if(status) {
        while(fgets(line,sizeof(line),status)) {
            if(!strncmp(line,"progress=",9))progress=atoi(line+9);
            if(!strncmp(line,"stage=fail",10))failed=1;
            if(!strncmp(line,"success=1",9))success=1;
            if(!strncmp(line,"message=",8)&&line[8]!='\n') {
                snprintf(message,sizeof(message),"%.150s",line+8);
                message[strcspn(message,"\r\n")]=0;
            }
        }
        fclose(status);
    }
    waiting=!strcmp(message,UI_N_("Insert USB drive with update/UN260_UPDATE.upk"));
    if(progress<0)progress=0;
    if(progress>100)progress=100;
    uint32_t accent=failed?0xffb76313:0xff195bbb;
    upgrade_rect(pixels,stride,0,0,1280,400,0xfff3f5f6);
    upgrade_text(pixels,stride,64,42,upgrade_copy(UI_N_("UN260 SYSTEM UPDATE")),3,0xff576c79);
    upgrade_text(pixels,stride,64,104,upgrade_copy((failed||waiting)?UI_N_("UPDATE PAUSED"):success?UI_N_("UPDATE COMPLETE"):UI_N_("UPDATING YOUR DEVICE")),5,accent);
    upgrade_text(pixels,stride,64,179,upgrade_copy(failed?UI_N_("KEEP BACKUPS AND CHECK THE ERROR"):UI_N_("KEEP USB AND POWER CONNECTED")),3,0xff20313b);
    upgrade_rect(pixels,stride,64,238,1152,12,0xffdce3e8);
    upgrade_rect(pixels,stride,64,238,1152*progress/100,12,accent);
    /* Known recovery hints use frozen localized artwork. Detailed engine text
     * stays in the log; unknown future text is a stable service code, never an
     * unreadable partial UTF-8 or printf-format string. */
    if(upgrade_native_key(message)>=0)upgrade_text(pixels,stride,64,283,message,2,0xff576c79);
    else if(failed) {
        uint32_t code=2166136261U;
        for(const unsigned char *c=(const unsigned char *)message;*c;c++)code=(code^*c)*16777619U;
        char diagnostic[24];snprintf(diagnostic,sizeof(diagnostic),"E %08X",code);
        upgrade_text(pixels,stride,64,283,diagnostic,2,0xff576c79);
    }
    upgrade_text(pixels,stride,64,353,upgrade_copy(failed?UI_N_("RAM LOG UI UPDATE LOG  USB COPY MAY BE UNAVAILABLE"):UI_N_("BACKUP   STORAGE   APPLICATION   VERIFY")),2,0xff576c79);
}
#endif
