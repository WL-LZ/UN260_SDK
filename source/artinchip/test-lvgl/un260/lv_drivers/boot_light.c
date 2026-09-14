#define _GNU_SOURCE
#include "boot_light_protocol.h"
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
#include "aic_ui/generated_assets/boot_light_packed.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#ifdef BOOT_LIGHT_HOST_TEST
#define AICFB_WAIT_FOR_VSYNC 0x1234
#define UI_BOOT_ANIM_THEME 3
#define UI_BOOT_ANIM_THEME_C 3
#else
#include <video/artinchip_fb.h>
#include "un260/lv_core/page_00_boot_anim.h"
#endif

#define W 1280U
#define H 400U
#define BG_BYTES (W*H*4U)
#define ICON_BYTES (96U*96U*4U)
#define TEXT_BYTES (500U*64U*4U)
#define ASSET_BYTES (BG_BYTES+ICON_BYTES+2U*TEXT_BYTES)
#ifndef ASSET_PATH
#define ASSET_PATH "/usr/local/share/lvgl_data/boot_theme_c/boot-light.bin"
#endif
static volatile sig_atomic_t stopped;
static uint64_t now_ms(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000U+t.tv_nsec/1000000U;}
static void stop_signal(int sig) {(void)sig;stopped=1;}
static void trace(const char *stage)
{
    const char *enabled=getenv("UN260_BOOT_TRACE");
    if(enabled&&!strcmp(enabled,"1"))fprintf(stderr,"BOOT_LIGHT uptime_ms=%llu stage=%s\n",(unsigned long long)now_ms(),stage);
}
static float ease(float p) {if(p<=0)return 0;if(p>=1)return 1;if(p<.5f)return 4*p*p*p;float q=2-2*p;return 1-q*q*q/2;}
static float progress(uint32_t t,uint32_t start,uint32_t duration) {return t<=start?0:t-start>=duration?1:(float)(t-start)/duration;}
static void blend(uint8_t *dst,const uint8_t *src,unsigned count,unsigned opacity)
{
    for(unsigned i=0;i<count;i++,dst+=4,src+=4) {
        unsigned a=(src[3]*opacity+127)/255;
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
static void render(uint8_t *frame,unsigned stride,const uint8_t *assets,uint32_t elapsed,bool full)
{
    /* Both framebuffer pages own a background copy. Restore only the moving
     * area thereafter: no full-screen blend, decoder, or allocation per frame. */
    unsigned y0=full?0:78,y1=full?H:307,x0=full?0:390,x1=full?W:890;
    for(unsigned y=y0;y<y1;y++)memcpy(frame+y*stride+x0*4,assets+(y*W+x0)*4,(x1-x0)*4);
    float p=progress(elapsed,150,800),q=1-p;
    sprite(frame,stride,assets+BG_BYTES,96,96,592,82+(unsigned)(6*q*q*q+.5f),ease(p));
    sprite(frame,stride,assets+BG_BYTES+ICON_BYTES,500,64,390,219+(unsigned)(4*q*q*q+.5f),ease(p)*(1-ease(progress(elapsed,2700,600))));
    p=progress(elapsed,3450,700);q=1-p;
    sprite(frame,stride,assets+BG_BYTES+ICON_BYTES+TEXT_BYTES,500,64,390,219+(unsigned)(4*q*q*q+.5f),ease(p));
    for(unsigned i=0;i<3;i++) {
        unsigned phase=elapsed<4350?0:(elapsed-4350+1500-i*160)%1500;
        float jump=phase>=660?0:phase<240?ease((float)phase/240):1-ease((float)(phase-240)/420);
        float alpha=ease(progress(elapsed,4350+i*120,450))*(.4f+.6f*jump);
        unsigned x=619+i*17,y=296-(unsigned)(6*jump+.5f);
        for(unsigned yy=0;yy<7;yy++)for(unsigned xx=0;xx<7;xx++) {
            int dx=(int)xx-3,dy=(int)yy-3;
            if(dx*dx+dy*dy>12)continue;
            const uint8_t color[4]={0x20,0x58,0xf8,255};
            blend(frame+(y+yy)*stride+(x+xx)*4,color,1,(unsigned)(alpha*255+.5f));
        }
    }
}

#ifdef UN260_EARLY_INIT
int boot_light_run(void)
#else
int main(void)
#endif
{
#if UI_BOOT_ANIM_THEME != UI_BOOT_ANIM_THEME_C
    return 1;
#endif
    trace("main_enter");
    int status=1,lease=-1,fb=-1,server=-1,ipc_dir=-1;
    uint8_t *mapped=MAP_FAILED,*assets=NULL;
    struct fb_fix_screeninfo fix={0};struct fb_var_screeninfo var={0};
    bool bound=false,ready=false;
    prctl(PR_SET_NAME,"un260-boot",0,0,0);
    umask(077);signal(SIGTERM,stop_signal);signal(SIGINT,stop_signal);signal(SIGPIPE,SIG_IGN);
    lease=open(BOOT_LIGHT_LOCK,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(lease<0||flock(lease,LOCK_EX|LOCK_NB))goto done;
    unlink(BOOT_LIGHT_READY);
    assets=malloc(ASSET_BYTES);
    if(!assets)goto done;
    uLongf length=ASSET_BYTES;
    if(uncompress(assets,&length,boot_light_packed,sizeof(boot_light_packed))!=Z_OK||length!=ASSET_BYTES)goto done;
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
    uint64_t start=now_ms()-450U;bool initialized[2]={false,false};unsigned visible=var.yoffset>=H?1:0;
    while(!stopped&&now_ms()-start<20000U) {
        int client=accept4(server,NULL,NULL,SOCK_CLOEXEC|SOCK_NONBLOCK);
        if(client>=0) {
            struct pollfd p={client,POLLIN,0};char request;
            if(poll(&p,1,100)>0&&recv(client,&request,1,0)==1&&request=='T') {
                boot_light_reply_t reply={BOOT_LIGHT_MAGIC,(uint32_t)(now_ms()-start)};
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
            close(client);
        }
        uint32_t elapsed=(uint32_t)(now_ms()-start);
        if(initialized[0]&&initialized[1]&&
           ((elapsed>=980&&elapsed<2700)||(elapsed>=3330&&elapsed<3450))) {
            struct timespec pause={0,16000000};nanosleep(&pause,NULL);continue;
        }
        unsigned target=visible^1U;
        render(mapped+target*H*fix.line_length,fix.line_length,assets,elapsed,!initialized[target]);
        initialized[target]=true;var.yoffset=target*H;int zero=0;
        if(ioctl(fb,FBIOPAN_DISPLAY,&var)||ioctl(fb,AICFB_WAIT_FOR_VSYNC,&zero))goto done;
        visible=target;
        if(!ready) {
            int fd=open(BOOT_LIGHT_READY,O_CREAT|O_WRONLY|O_TRUNC|O_CLOEXEC,0600);
            if(fd<0)goto done;
            char pid[32];int n=snprintf(pid,sizeof(pid),"%ld\n",(long)getpid());
            bool ok=write(fd,pid,n)==n;close(fd);if(!ok)goto done;
            ready=true;
            trace("first_frame");
        }
        struct timespec pause={0,1000000};nanosleep(&pause,NULL);
    }
done:
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
    free(assets);
    if(lease>=0)close(lease);
    if(status)fprintf(stderr,"BOOT_LIGHT unavailable or expired; legacy UI retains fallback\n");
    return status;
}
