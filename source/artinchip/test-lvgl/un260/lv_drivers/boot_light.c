#define _GNU_SOURCE
#include "boot_light_protocol.h"
#include "boot_frame_stats.h"
#include "un260/lv_core/boot_anim/boot_welcome_locale.h"
#ifdef UN260_BOOT_BUNDLE
#include "boot_theme_pack.h"
#else
#include "un260/font/boot_welcome_native.h"
#endif
#include "un260/lv_core/boot_anim/boot_welcome_timing.h"
#include "un260/lv_system/backlight_service.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <zlib.h>
#ifndef UN260_BOOT_BUNDLE
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#include "aic_ui/generated_assets/boot_light_d_packed.h"
#else
#include "aic_ui/generated_assets/boot_light_packed.h"
#endif
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#ifdef BOOT_LIGHT_HOST_TEST
#define AICFB_WAIT_FOR_VSYNC 0x1234

#else
#include <video/artinchip_fb.h>
#include "un260/lv_core/page_00_boot_anim.h"
#endif

#define W 1280U
#define H 400U
#define BG_BYTES (W*H*4U)
#define ICON_BYTES (96U*96U*4U)
#define TEXT_BYTES (500U*BOOT_TEXT_HEIGHT*4U)
#define ASSET_BYTES (BG_BYTES+ICON_BYTES+2U*TEXT_BYTES)
#define ACTIVE_X 390U
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
#define ACTIVE_Y 120U
#define ACTIVE_H 196U
#else
#define ACTIVE_Y 78U
#define ACTIVE_H 264U
#endif
#define ACTIVE_W 500U
#define ACTIVE_STRIDE (ACTIVE_W*4U)
#define ACTIVE_BYTES (ACTIVE_STRIDE*ACTIVE_H)
#define DOTS_X BOOT_DOT_X
#define DOTS_Y (BOOT_DOT_Y-6U)
#define DOTS_W (2U*BOOT_DOT_STEP+BOOT_DOT_SIZE)
#define DOTS_H 13U
typedef struct {
    bool initialized;
    bool settled;
} frame_state_t;
#ifndef ASSET_PATH
#define ASSET_PATH "/usr/local/share/lvgl_data/boot_theme_c/boot-light.bin"
#endif
static volatile sig_atomic_t stopped;
static uint64_t now_ms(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000U+t.tv_nsec/1000000U;}
static uint64_t now_us(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000U+t.tv_nsec/1000U;}
static void stop_signal(int sig) {(void)sig;stopped=1;}
static void trace(const char *stage)
{
    const char *enabled=getenv("UN260_BOOT_TRACE");
    if(enabled&&!strcmp(enabled,"1"))fprintf(stderr,"BOOT_LIGHT uptime_ms=%llu stage=%s\n",(unsigned long long)now_ms(),stage);
}
#include "un260/lv_core/boot_anim/boot_d_motion.h"
#ifndef UN260_BOOT_BUNDLE
static float ease(float p) {
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
return boot_d_smooth(p);
#endif
    if(p<=0)return 0;
    if(p>=1)return 1;
    if(p<.5f)return 4*p*p*p;
    float q=2-2*p;return 1-q*q*q/2;
}
#endif
static float progress(uint32_t t,uint32_t start,uint32_t duration) {return t<=start?0:t-start>=duration?1:(float)(t-start)/duration;}
static void blend(uint8_t *dst,const uint8_t *src,unsigned count,unsigned opacity)
{
    for(unsigned i=0;i<count;i++,dst+=4,src+=4) {
        unsigned a=(src[3]*opacity+127)/255;
        if(!a)continue;
        if(a==255){memcpy(dst,src,4);continue;}
        for(unsigned c=0;c<3;c++)dst[c]=(src[c]*a+dst[c]*(255-a)+127)/255;
        dst[3]=255;
    }
}
static void sprite(uint8_t *frame,unsigned stride,const uint8_t *src,unsigned w,unsigned h,unsigned x,unsigned y,float opacity)
{
    unsigned a=(unsigned)(255*opacity+.5f);
    if(!a)return;
    for(unsigned row=0;row<h;row++)blend(frame+(y+row)*stride+x*4,src+row*w*4,w,a);
}
#ifndef UN260_BOOT_BUNDLE
static void render(uint8_t *frame,unsigned stride,const uint8_t *assets,
                   uint8_t *canvas,uint32_t elapsed,frame_state_t *state)
{
    /* This BSP maps the framebuffer uncached. Blend in normal RAM and only
     * write completed rows to scanout; never read scanout pixels on the CPU. */
    if(!state->initialized)
        for(unsigned y=0;y<H;y++)memcpy(frame+y*stride,assets+y*W*4,W*4);
    bool dots_only=state->settled&&elapsed>=BOOT_WELCOME_SETTLED;
    unsigned x=dots_only?DOTS_X:ACTIVE_X,y=dots_only?DOTS_Y:ACTIVE_Y;
    unsigned width=dots_only?DOTS_W:ACTIVE_W,height=dots_only?DOTS_H:ACTIVE_H;
    for(unsigned row=0;row<height;row++)
        memcpy(canvas+(y+row-ACTIVE_Y)*ACTIVE_STRIDE+(x-ACTIVE_X)*4,
               assets+((y+row)*W+x)*4,width*4);
    if(!dots_only) {
#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_D
        float p=progress(elapsed,BOOT_ENTRANCE_START,BOOT_ENTRANCE_DURATION),q=1-p;
        sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES,96,96,592-ACTIVE_X,82-ACTIVE_Y+(unsigned)(BOOT_WELCOME_MOTION_SCALE*6*q*q*q+.5f),ease(p));
        sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES+ICON_BYTES,500,BOOT_TEXT_HEIGHT,390-ACTIVE_X,BOOT_TEXT_Y-ACTIVE_Y+(unsigned)(BOOT_WELCOME_MOTION_SCALE*4*q*q*q+.5f),ease(p)*(1-ease(progress(elapsed,BOOT_BRAND_EXIT_START,BOOT_BRAND_EXIT_DURATION))));
#else
        float p,q;
#endif
        p=progress(elapsed,BOOT_WELCOME_START,BOOT_WELCOME_DURATION);q=1-p;
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
        q=0;
#endif
        sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,500,BOOT_TEXT_HEIGHT,390-ACTIVE_X,BOOT_TEXT_Y-ACTIVE_Y+(unsigned)(BOOT_WELCOME_MOTION_SCALE*4*q*q*q+.5f),ease(p));
    }
    for(unsigned i=0;i<3;i++) {
        unsigned phase=elapsed<BOOT_DOT_START?0:(elapsed-BOOT_DOT_START+BOOT_DOT_PERIOD-i*160)%BOOT_DOT_PERIOD;
        float jump=phase>=660?0:phase<240?ease((float)phase/240):1-ease((float)(phase-240)/420);
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
        jump=boot_d_dot(elapsed,i);
#endif
        float alpha=ease(progress(elapsed,BOOT_DOT_START+i*120,450))*(.4f+.6f*jump);
        unsigned lift=6;
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
        alpha=ease(progress(elapsed,2300U,800U))*(.4f+.6f*jump);lift=5;
#endif
        unsigned x=BOOT_DOT_X+i*BOOT_DOT_STEP,y=BOOT_DOT_Y-(unsigned)(BOOT_WELCOME_MOTION_SCALE*lift*jump+.5f);
        for(unsigned yy=0;yy<BOOT_DOT_SIZE;yy++)for(unsigned xx=0;xx<BOOT_DOT_SIZE;xx++) {
            int dx=(int)xx-(int)(BOOT_DOT_SIZE/2),dy=(int)yy-(int)(BOOT_DOT_SIZE/2);
            if(dx*dx+dy*dy>BOOT_DOT_RADIUS_SQ)continue;
#if UI_BOOT_ANIM_THEME == UI_BOOT_ANIM_THEME_D
            const uint8_t color[4]={0x9e,0x87,0x7c,255};
#else
            const uint8_t color[4]={0x20,0x58,0xf8,255};
#endif
            blend(canvas+(y+yy-ACTIVE_Y)*ACTIVE_STRIDE+(x+xx-ACTIVE_X)*4,color,1,(unsigned)(alpha*255+.5f));
        }
    }
    for(unsigned row=0;row<height;row++)
        memcpy(frame+(y+row)*stride+x*4,
               canvas+(y+row-ACTIVE_Y)*ACTIVE_STRIDE+(x-ACTIVE_X)*4,width*4);
    state->initialized=true;
    state->settled=elapsed>=BOOT_WELCOME_SETTLED;
}
#else
static float scene_ease(const boot_theme_pack_t *pack,float p)
{
    if(pack->theme==4U)return boot_d_smooth(p);
    if(p<=0)return 0;
    if(p>=1)return 1;
    if(p<.5f)return 4*p*p*p;
    float q=2-2*p;return 1-q*q*q/2;
}
/* Compose each sprite against immutable background in cached RAM.  The
 * framebuffer remains write-only, including the localized corner labels. */
static void customer_sprite(uint8_t *frame,unsigned stride,const boot_customer_image_t *art,
                            const uint8_t *background,uint8_t *canvas,unsigned opacity)
{
    unsigned width=art->width,height=art->height,x=(unsigned)art->x,y=(unsigned)art->y;
    unsigned pitch=width*4U;
    for(unsigned row=0;row<height;row++) {
        uint8_t *dst=canvas+row*pitch;
        memcpy(dst,background+((y+row)*W+x)*4U,pitch);
        if(art->format==BOOT_CUSTOMER_BGRA)blend(dst,art->data+row*pitch,width,opacity);
        else for(unsigned col=0;col<width;col++) {
            uint8_t color[4]={(uint8_t)art->color,(uint8_t)(art->color>>8),
                (uint8_t)(art->color>>16),art->data[row*width+col]};
            blend(dst+col*4U,color,1U,opacity);
        }
        memcpy(frame+(y+row)*stride+x*4U,dst,pitch);
    }
}
static void render_customer(uint8_t *frame,unsigned stride,const boot_theme_pack_t *pack,
                            uint8_t *canvas,uint32_t elapsed,frame_state_t *state)
{
    const uint8_t *background=pack->customer[0].data;
    if(!state->initialized) {
        for(unsigned row=0;row<H;row++)memcpy(frame+row*stride,background+row*W*4U,W*4U);
        customer_sprite(frame,stride,&pack->customer[2],background,canvas,255U);
        customer_sprite(frame,stride,&pack->customer[3],background,canvas,255U);
    }
    if(!state->settled) {
        float alpha=scene_ease(pack,progress(elapsed,pack->scene.entrance_start,pack->scene.entrance_duration));
        customer_sprite(frame,stride,&pack->customer[1],background,canvas,(unsigned)(alpha*255U+.5f));
    }
    state->initialized=true;state->settled=elapsed>=pack->scene.welcome_settled;
}
static void render_bundle(uint8_t *frame,unsigned stride,boot_theme_pack_t *pack,
                          uint8_t *canvas,uint32_t elapsed,frame_state_t *state)
{
    if(pack->version==2U){render_customer(frame,stride,pack,canvas,elapsed,state);return;}
    const boot_theme_scene_t *s=&pack->scene;
    const uint8_t *assets=pack->data;
    if(pack->mode==BOOT_THEME_MODE_LOGO) {
        if(!state->initialized)for(unsigned row=0;row<H;row++)
            memcpy(frame+row*stride,assets+pack->logo_offset+row*W*4U,W*4U);
        state->initialized=true;state->settled=true;return;
    }
    if(!state->initialized)for(unsigned row=0;row<H;row++)
        memcpy(frame+row*stride,assets+row*W*4U,W*4U);
    bool dots_only=state->settled && elapsed>=s->welcome_settled;
    unsigned x=dots_only?s->dot_x:ACTIVE_X,y=dots_only?s->dot_y-6U:s->active_y;
    unsigned width=dots_only?2U*s->dot_step+s->dot_size:ACTIVE_W;
    unsigned height=dots_only?s->dot_size+6U:s->active_h;
    for(unsigned row=0;row<height;row++)
        memcpy(canvas+(y+row-s->active_y)*ACTIVE_STRIDE+(x-ACTIVE_X)*4U,
               assets+((y+row)*W+x)*4U,width*4U);
    if(!dots_only) {
        if(pack->theme==3U) {
            float p=progress(elapsed,s->entrance_start,s->entrance_duration),q=1-p;
            sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES,96,96,592-ACTIVE_X,
                   82U-s->active_y+(unsigned)(s->motion_scale*6U*q*q*q+.5f),scene_ease(pack,p));
            sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES+ICON_BYTES,500,128,0,
                   s->text_y-s->active_y+(unsigned)(s->motion_scale*4U*q*q*q+.5f),
                   scene_ease(pack,p)*(1-scene_ease(pack,progress(elapsed,s->brand_exit_start,s->brand_exit_duration))));
        }
        float p=progress(elapsed,s->welcome_start,s->welcome_duration),q=pack->theme==4U?0:1-p;
        sprite(canvas,ACTIVE_STRIDE,assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,500,128,0,
               s->text_y-s->active_y+(unsigned)(s->motion_scale*4U*q*q*q+.5f),scene_ease(pack,p));
    }
    for(unsigned i=0;i<3;i++) {
        unsigned phase=elapsed<s->dot_start?0:(elapsed-s->dot_start+s->dot_period-i*160U)%s->dot_period;
        float jump=phase>=660U?0:phase<240U?scene_ease(pack,(float)phase/240):1-scene_ease(pack,(float)(phase-240U)/420);
        unsigned lift=6U;
        float alpha=scene_ease(pack,progress(elapsed,s->dot_start+i*120U,450U))*(.4f+.6f*jump);
        if(pack->theme==4U) {
            /* Native waiting repeats; the shared legacy intro remains bounded. */
            uint32_t dot_phase=elapsed<s->dot_start?0:(elapsed-s->dot_start)%s->dot_period;
            jump=elapsed<s->dot_start?0:boot_d_dot(2500U+dot_phase*1800U/s->dot_period,i);
            alpha=scene_ease(pack,progress(elapsed,s->welcome_settled,800U))*(.4f+.6f*jump);lift=5U;
        }
        unsigned dx=s->dot_x+i*s->dot_step;
        unsigned dy=s->dot_y-(unsigned)(s->motion_scale*lift*jump+.5f);
        for(unsigned yy=0;yy<s->dot_size;yy++)for(unsigned xx=0;xx<s->dot_size;xx++) {
            int a=(int)xx-(int)(s->dot_size/2U),b=(int)yy-(int)(s->dot_size/2U);
            if((unsigned)(a*a+b*b)>s->dot_radius)continue;
            const uint8_t color[4]={pack->theme==4U?0x9e:0x20,pack->theme==4U?0x87:0x58,pack->theme==4U?0x7c:0xf8,255};
            blend(canvas+(dy+yy-s->active_y)*ACTIVE_STRIDE+(dx+xx-ACTIVE_X)*4U,color,1,(unsigned)(alpha*255+.5f));
        }
    }
    for(unsigned row=0;row<height;row++)
        memcpy(frame+(y+row)*stride+x*4U,
               canvas+(y+row-s->active_y)*ACTIVE_STRIDE+(x-ACTIVE_X)*4U,width*4U);
    state->initialized=true;state->settled=elapsed>=s->welcome_settled;
}
static bool decode_bundle_id(const char *text,uint8_t identity[32])
{
    if(!text || strlen(text)!=64U)return false;
    for(unsigned i=0;i<32;i++) {
        unsigned v=0;
        for(unsigned j=0;j<2;j++) {
            unsigned char c=(unsigned char)text[i*2U+j];
            if(c>='0'&&c<='9')v=v*16U+c-'0';
            else if(c>='a'&&c<='f')v=v*16U+c-'a'+10U;
            else return false;
        }
        identity[i]=(uint8_t)v;
    }
    return true;
}
#endif
static int display_ioctl(int fd,unsigned long request,void *arg)
{
    int result;
    do {result=ioctl(fd,request,arg);} while(result<0&&errno==EINTR&&!stopped);
    return result;
}
static void quiet_pause(void)
{
    struct timespec remaining={0,16000000};
    while(!stopped&&nanosleep(&remaining,&remaining)<0&&errno==EINTR) {}
}
#ifdef UN260_UPDATE_DISPLAY
#include "upgrade_display.h"
#endif

#ifdef UN260_EARLY_INIT
int boot_light_run(void)
#elif defined(UN260_BOOT_BUNDLE)
int main(int argc,char **argv)
#else
int main(void)
#endif
{
#if !UI_BOOT_EARLY_ENABLED && !defined(UN260_BOOT_BUNDLE)
    return 1;
#endif
    trace("main_enter");
#ifdef UN260_UPDATE_DISPLAY
    (void)render;(void)quiet_pause;
#endif
    int status=1,lease=-1,fb=-1,server=-1,ipc_dir=-1;
    uint8_t *mapped=MAP_FAILED,*canvas=NULL;
#ifndef UN260_BOOT_BUNDLE
    uint8_t *assets=NULL;
#endif
    struct fb_fix_screeninfo fix={0};struct fb_var_screeninfo var={0};
    bool bound=false,ready=false;
    const char *frame_trace_env=getenv("UN260_BOOT_FRAME_TRACE");
    bool frame_trace=frame_trace_env&&!strcmp(frame_trace_env,"1");
    boot_frame_stats_t frame_stats={0};
#ifdef UN260_BOOT_BUNDLE
    boot_theme_pack_t bundle={0};bool check_only=false,unified_ready=false;
    const char *theme_path=NULL,*id_text=NULL;int theme_fd=-1;uint32_t mode_override=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--check")){check_only=true;continue;}
        if(i+1>=argc)goto done;
        if(!strcmp(argv[i],"--theme"))theme_path=argv[++i];
        else if(!strcmp(argv[i],"--theme-fd")) {
            char *end=NULL;long n=strtol(argv[++i],&end,10);
            if(!end || end==argv[i] || *end || argv[i][0]<'0' || argv[i][0]>'9' || n<0 || n>65535)goto done;
            theme_fd=(int)n;
        }
        else if(!strcmp(argv[i],"--bundle-id"))id_text=argv[++i];
        else if(!strcmp(argv[i],"--mode")) {
            const char *value=argv[++i];
            if(!strcmp(value,"animation"))mode_override=BOOT_THEME_MODE_ANIMATION;
            else if(!strcmp(value,"logo"))mode_override=BOOT_THEME_MODE_LOGO;
            else goto done;
        }
        else goto done;
    }
    if(theme_fd>=0 ? !boot_theme_pack_load_fd(theme_fd,&bundle) :
       !theme_path || !boot_theme_pack_load_path(theme_path,&bundle))goto done;
    if(theme_fd>=0){close(theme_fd);theme_fd=-1;}
    if(id_text && !decode_bundle_id(id_text,bundle.identity))goto done;
    if(mode_override && mode_override!=bundle.mode)goto done;
    char language_tag[UI_LOCALE_TAG_CAPACITY]={0};
    (void)ui_locale_read_file(BOOT_WELCOME_LOCALE_PATH,language_tag);
    if(!boot_theme_pack_select_language(&bundle,language_tag))goto done;
    if(check_only) {
        printf("BOOT_THEME_CHECK theme=%u mode=%s languages=%u raw_bytes=%zu\n",bundle.theme,
               bundle.mode==BOOT_THEME_MODE_LOGO?"logo":"animation",bundle.language_count,bundle.data_bytes);
        status=0;goto done;
    }
    if(!boot_theme_pack_freeze(&bundle))goto done;
#endif
    bool locale_ready=true;
#ifndef UN260_BOOT_BUNDLE
    int locale_fd=-1;
    unsigned welcome_index=boot_welcome_read_index(BOOT_WELCOME_LOCALE_PATH);
#ifdef UN260_EARLY_INIT
    const char *locale_env=getenv("UN260_BOOT_LOCALE_FD");
    if(locale_env){locale_fd=atoi(locale_env);locale_ready=false;}
#endif
#endif
    prctl(PR_SET_NAME,"un260-boot",0,0,0);
    umask(077);signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);signal(SIGPIPE,SIG_IGN);
    lease=open(BOOT_LIGHT_LOCK,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(lease<0)goto done;
#ifdef UN260_UPDATE_DISPLAY
    for(unsigned retry=0;flock(lease,LOCK_EX|LOCK_NB);retry++) {
        if(retry>=100||stopped)goto done;
        usleep(100000);
    }
#else
    if(flock(lease,LOCK_EX|LOCK_NB))goto done;
#endif
    unlink(BOOT_LIGHT_READY);
#ifdef UN260_BOOT_BUNDLE
    unlink(BOOT_LIGHT_UNIFIED_READY);
#endif
#ifndef UN260_UPDATE_DISPLAY
#ifdef UN260_BOOT_BUNDLE
    if(bundle.mode==BOOT_THEME_MODE_ANIMATION) {
        canvas=malloc(boot_theme_pack_canvas_bytes(&bundle));
        if(!canvas)goto done;
    }
#else
    assets=malloc(ASSET_BYTES);
    if(!assets)goto done;
    uLongf length=ASSET_BYTES;
    if(uncompress(assets,&length,boot_light_packed,sizeof(boot_light_packed))!=Z_OK||length!=ASSET_BYTES)goto done;
    boot_welcome_native_apply(assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,welcome_index);
    canvas=malloc(ACTIVE_BYTES);
#endif
#ifndef UN260_BOOT_BUNDLE
    if(!canvas)goto done;
#endif
#endif
    trace("assets_ready");
    fb=open("/dev/fb0",O_RDWR|O_CLOEXEC);
    if(fb<0||ioctl(fb,FBIOGET_FSCREENINFO,&fix)||ioctl(fb,FBIOGET_VSCREENINFO,&var))goto done;
    if(var.xres!=W||var.yres!=H||var.bits_per_pixel!=32||var.xoffset||
       (var.yoffset!=0&&var.yoffset!=H)||var.red.offset!=16||var.green.offset!=8||var.blue.offset||
       fix.line_length<W*4||var.yres_virtual<H*2||fix.smem_len<(uint64_t)fix.line_length*H*2)goto done;
    mapped=mmap(NULL,fix.smem_len,PROT_READ|PROT_WRITE,MAP_SHARED,fb,0);
    if(mapped==MAP_FAILED)goto done;
    server=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    struct sockaddr_un address={.sun_family=AF_UNIX};strcpy(address.sun_path,BOOT_LIGHT_SOCKET);
    unlink(BOOT_LIGHT_SOCKET);
    if(server<0||bind(server,(struct sockaddr*)&address,sizeof(address))||listen(server,1))goto done;
    bound=true;
    /* Retain the directory across initramfs mount moves for safe cleanup. */
    char directory[sizeof(address.sun_path)];strcpy(directory,BOOT_LIGHT_SOCKET);
    char *slash=strrchr(directory,'/');if(!slash)goto done;*slash=0;
    ipc_dir=open(directory,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(ipc_dir<0)goto done;
    backlight_service_init();
    trace("display_ready");
    /* First scanout already contains a soft, visible brand mark; do not spend
       the first half second displaying only an empty background. */
    uint64_t lifetime_start=now_ms(),start=lifetime_start-450U;frame_state_t pages[2]={{0},{0}};unsigned visible=var.yoffset>=H?1:0;
    while(!stopped
#if !defined(UN260_UPDATE_DISPLAY) && !defined(UN260_BOOT_BUNDLE)
          &&now_ms()-lifetime_start<20000U
#endif
    ) {
#ifndef UN260_BOOT_BUNDLE
        if(!locale_ready) {
            uint32_t index=0; ssize_t n=read(locale_fd,&index,sizeof(index));
            if(n>=0 || (errno!=EAGAIN && errno!=EINTR)) {
                welcome_index=n==sizeof(index)&&index<BOOT_WELCOME_COUNT?index:0;
                close(locale_fd);locale_fd=-1;locale_ready=true;
#if !defined(UN260_UPDATE_DISPLAY) && !defined(UN260_BOOT_BUNDLE)
                boot_welcome_native_apply(assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,welcome_index);
#endif
                start=now_ms()-450U;
            }
        }
#endif
        int client=accept4(server,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);
        if(client>=0) {
            struct pollfd p={client,POLLIN,0};char request;
            if(poll(&p,1,100)>0&&recv(client,&request,1,0)==1) {
#ifdef UN260_BOOT_BUNDLE
                if(request==BOOT_LIGHT_PROBE || request==BOOT_LIGHT_UI_READY || request==BOOT_LIGHT_UI_ERROR) {
                    uint32_t elapsed_ms=(uint32_t)(now_ms()-start);
                    bool handoff=ready && (request==BOOT_LIGHT_UI_ERROR ||
                        (request==BOOT_LIGHT_UI_READY && (bundle.mode==BOOT_THEME_MODE_LOGO || elapsed_ms>=bundle.min_show_ms)));
                    boot_light_status_t reply={.magic=BOOT_LIGHT_UNIFIED_MAGIC,.abi=BOOT_LIGHT_UNIFIED_ABI,
                        .flags=(ready?BOOT_LIGHT_FIRST_FRAME:0U)|(handoff?BOOT_LIGHT_HANDOFF:0U)|
                               (request==BOOT_LIGHT_UI_ERROR?BOOT_LIGHT_ERROR:0U),
                        .mode=bundle.mode,.language=bundle.language,.elapsed_ms=elapsed_ms,.origin_ms=start};
                    memcpy(reply.theme_id,bundle.identity,sizeof(reply.theme_id));
                    union {struct cmsghdr alignment;char bytes[CMSG_SPACE(sizeof(int))];} control={0};
                    struct iovec iov={&reply,sizeof(reply)};
                    struct msghdr msg={.msg_iov=&iov,.msg_iovlen=1};
                    if(handoff) {
                        msg.msg_control=control.bytes;msg.msg_controllen=sizeof(control.bytes);
                        struct cmsghdr *c=CMSG_FIRSTHDR(&msg);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));
                        memcpy(CMSG_DATA(c),&fb,sizeof(fb));
                    }
                    bool sent=sendmsg(client,&msg,MSG_NOSIGNAL)==sizeof(reply);
                    close(client);client=-1;
                    if(handoff && sent){status=0;trace("handoff");break;}
                }
#endif
                if(request=='T') {
                boot_light_reply_t reply={locale_ready?BOOT_LIGHT_MAGIC:0,(uint32_t)(now_ms()-start)};
#ifdef UN260_UPDATE_DISPLAY
                reply.magic=0; /* Transfer the fd, but never adopt upgrade art as boot animation. */
#endif
#ifdef UN260_BOOT_BUNDLE
                reply.magic=0; /* A legacy client gets safe FD transfer, no duplicated animation. */
#endif
                union {struct cmsghdr alignment;char bytes[CMSG_SPACE(sizeof(int))];} control={0};
                struct iovec iov={&reply,sizeof(reply)};
                struct msghdr msg={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
                struct cmsghdr *c=CMSG_FIRSTHDR(&msg);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(sizeof(int));
                memcpy(CMSG_DATA(c),&fb,sizeof(fb));
                /* Sender stops drawing before releasing the lease. The passed
                 * fb reference prevents the vendor release hook firing early. */
                if(sendmsg(client,&msg,MSG_NOSIGNAL)==sizeof(reply)) {status=0;trace("handoff");}
                close(client);break;
                }
            }
            if(client>=0)close(client);
        }
        uint32_t elapsed=locale_ready?(uint32_t)(now_ms()-start):450U;
#ifndef UN260_UPDATE_DISPLAY
#ifdef UN260_BOOT_BUNDLE
        if(pages[0].initialized && pages[1].initialized && bundle.mode==BOOT_THEME_MODE_LOGO) {
            quiet_pause();continue;
        }
        if(pages[0].settled && pages[1].settled &&
           (bundle.version==2U || (elapsed>=bundle.scene.welcome_settled && elapsed<bundle.scene.dot_start))) {
            quiet_pause();continue;
        }
#else
        if(pages[0].settled&&pages[1].settled&&
           elapsed>=BOOT_WELCOME_SETTLED&&elapsed<BOOT_DOT_START) {
            quiet_pause();continue;
        }
#endif
#endif
        unsigned target=visible^1U;
        uint64_t work_started_us=0,work_us=0;
        if(frame_trace)work_started_us=now_us();
#ifdef UN260_UPDATE_DISPLAY
        upgrade_render(mapped+target*H*fix.line_length,fix.line_length);
        (void)elapsed;(void)pages;
#else
#ifdef UN260_BOOT_BUNDLE
        render_bundle(mapped+target*H*fix.line_length,fix.line_length,&bundle,canvas,elapsed,&pages[target]);
#else
        render(mapped+target*H*fix.line_length,fix.line_length,assets,canvas,elapsed,&pages[target]);
#endif
#endif
        if(frame_trace)work_us=now_us()-work_started_us;
        var.yoffset=target*H;int zero=0;
        if(display_ioctl(fb,FBIOPAN_DISPLAY,&var)||display_ioctl(fb,AICFB_WAIT_FOR_VSYNC,&zero))goto done;
        if(frame_trace)boot_frame_stats_record(&frame_stats,now_us(),work_us);
        visible=target;
        if(!ready) {
            char pid[32];int n=snprintf(pid,sizeof(pid),"%ld\n",(long)getpid());
#ifdef UN260_BOOT_BUNDLE
            int modern=open(BOOT_LIGHT_UNIFIED_READY,O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC,0600);
            if(modern<0)goto done;
            bool modern_ok=write(modern,pid,n)==n;close(modern);
            unified_ready=true;
            if(!modern_ok)goto done;
#endif
            int fd=open(BOOT_LIGHT_READY,O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC,0600);
            if(fd<0)goto done;
            bool ok=write(fd,pid,n)==n;close(fd);if(!ok)goto done;
            ready=true;
            trace("first_frame");
        }
#ifdef UN260_UPDATE_DISPLAY
        usleep(200000);
#endif
    }
done:
#ifdef UN260_BOOT_BUNDLE
    if(theme_fd>=0)close(theme_fd);
    if(unified_ready) {
        if(ipc_dir>=0)unlinkat(ipc_dir,strrchr(BOOT_LIGHT_UNIFIED_READY,'/')+1,0);
        else unlink(BOOT_LIGHT_UNIFIED_READY);
    }
#else
    if(locale_fd>=0)close(locale_fd);
#endif
    if(bound) {
        if(ipc_dir>=0)unlinkat(ipc_dir,strrchr(BOOT_LIGHT_SOCKET,'/')+1,0);
        else unlink(BOOT_LIGHT_SOCKET);
    }
    if(ready) {
        if(ipc_dir>=0)unlinkat(ipc_dir,strrchr(BOOT_LIGHT_READY,'/')+1,0);
        else unlink(BOOT_LIGHT_READY);
    }
    if(ipc_dir>=0)close(ipc_dir);
    if(server>=0)close(server);
    if(mapped!=MAP_FAILED)munmap(mapped,fix.smem_len);
    if(fb>=0)close(fb);
#ifdef UN260_BOOT_BUNDLE
    boot_theme_pack_free(&bundle);
#else
    free(assets);
#endif
    free(canvas);
    if(lease>=0)close(lease);
    if(frame_trace)boot_frame_stats_print(stderr,&frame_stats,"native",status?"stopped_or_failed":"handoff");
    if(status)fprintf(stderr,"BOOT_LIGHT unavailable or expired; legacy UI retains fallback\n");
    return status;
}
