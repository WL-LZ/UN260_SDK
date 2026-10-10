#define _GNU_SOURCE
#include "startup_visual.h"
#include "boot_light_protocol.h"
#include "un260/boot/boot_bundle.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* Process-lifetime ownership. Do not close the inherited framebuffer early:
 * this BSP's fb release callback releases imported display DMA buffers. */
static int g_lease=-1,g_inherited_fb=-1;
static bool g_unified,g_snapshot_valid;
static boot_light_status_t g_snapshot;

#ifndef STARTUP_VISUAL_LIVE_BUNDLE
#define STARTUP_VISUAL_LIVE_BUNDLE BOOT_BUNDLE_LIVE_PATH
#endif
static bool root_frame_shown(void)
{
    static const char prefix[]=BOOT_BUNDLE_ACTIVE_MAGIC "\n";
    enum {length=sizeof(prefix)-1+65+65+10};
    char record[length];struct stat st;
    int fd=open(STARTUP_VISUAL_LIVE_BUNDLE,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return false;
    bool valid=fstat(fd,&st)==0&&S_ISREG(st.st_mode)&&st.st_uid==0&&
               !(st.st_mode&(0022|S_ISUID|S_ISGID))&&st.st_size==length&&
               read(fd,record,sizeof(record))==(ssize_t)sizeof(record);
    close(fd);
    if(!valid||memcmp(record,prefix,sizeof(prefix)-1))return false;
    const char *id=record+sizeof(prefix)-1,*hash=id+65;
    for(unsigned i=0;i<64;i++)
        if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f'))||
           !((hash[i]>='0'&&hash[i]<='9')||(hash[i]>='a'&&hash[i]<='f')))return false;
    return id[64]=='\n'&&hash[64]=='\n'&&!memcmp(hash+65,"none\nnone\n",10);
}

static int try_lease(void)
{
    if(g_lease<0)g_lease=open(BOOT_LIGHT_LOCK,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(g_lease<0)return -1;
    if(flock(g_lease,LOCK_EX|LOCK_NB)==0)return 1;
    return 0;
}

static int status_exchange(char request,boot_light_status_t *status,int *received_fd)
{
    *received_fd=-1;
    int sock=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    struct sockaddr_un address={.sun_family=AF_UNIX};
    strcpy(address.sun_path,BOOT_LIGHT_SOCKET);
    if(sock<0)return 0;
    int result=0;
    if(connect(sock,(struct sockaddr*)&address,sizeof(address))==0) {
        struct pollfd p={sock,POLLIN,0};
        if(send(sock,&request,1,MSG_NOSIGNAL|MSG_DONTWAIT)==1&&poll(&p,1,40)>0) {
            struct iovec iov={status,sizeof(*status)};
            union {struct cmsghdr alignment;char bytes[CMSG_SPACE(sizeof(int)*4)];} control={0};
            struct msghdr msg={.msg_iov=&iov,.msg_iovlen=1,
                .msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
            ssize_t n=recvmsg(sock,&msg,MSG_CMSG_CLOEXEC|MSG_DONTWAIT);
            for(struct cmsghdr *c=CMSG_FIRSTHDR(&msg);c;c=CMSG_NXTHDR(&msg,c)) {
                if(c->cmsg_level!=SOL_SOCKET||c->cmsg_type!=SCM_RIGHTS||
                   c->cmsg_len<CMSG_LEN(sizeof(int)))continue;
                size_t count=(c->cmsg_len-CMSG_LEN(0))/sizeof(int);
                for(size_t i=0;i<count;i++) {
                    int fd;memcpy(&fd,(char*)CMSG_DATA(c)+i*sizeof(int),sizeof(fd));
                    if(*received_fd<0)*received_fd=fd;
                    else close(fd);
                }
            }
            if(n==sizeof(*status)&&!(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC)))result=1;
        }
    }
    close(sock);
    if(!result&&*received_fd>=0){close(*received_fd);*received_fd=-1;}
    return result;
}

static bool status_valid(const boot_light_status_t *status)
{
    return status->magic==BOOT_LIGHT_UNIFIED_MAGIC&&
        status->abi==BOOT_LIGHT_UNIFIED_ABI&&
        (status->flags&BOOT_LIGHT_FIRST_FRAME)&&
        (status->mode==BOOT_LIGHT_MODE_ANIMATION||status->mode==BOOT_LIGHT_MODE_LOGO);
}

static long marker_pid(const char *path)
{
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return -1;
    char text[32]={0};ssize_t bytes=read(fd,text,sizeof(text)-1);close(fd);
    if(bytes<=0||bytes==(ssize_t)sizeof(text)-1)return -1;
    char *end=NULL;errno=0;long pid=strtol(text,&end,10);
    if(errno||pid<=1||pid>INT_MAX)return -1;
    while(*end=='\r'||*end=='\n'||*end==' '||*end=='\t')end++;
    return *end?-1:pid;
}

static bool unified_owner_active(void)
{
    long owner=marker_pid(BOOT_LIGHT_UNIFIED_READY);
    return owner>1&&owner==marker_pid(BOOT_LIGHT_READY)&&
        (kill((pid_t)owner,0)==0||errno==EPERM);
}

static int free_lease_fallback(bool ui_prepared)
{
    int acquired=try_lease();
    if(acquired==1&&!ui_prepared)return 0;
    if(acquired==1){g_unified=false;g_snapshot_valid=false;}
    return acquired;
}

int startup_visual_begin(boot_light_status_t *snapshot)
{
    bool shown=root_frame_shown();
    bool modern=unified_owner_active();
    for(unsigned retry=0;retry<(modern?3U:1U);retry++) {
        boot_light_status_t status={0};int received=-1;
        if(status_exchange(BOOT_LIGHT_PROBE,&status,&received)) {
            if(received>=0)close(received); /* P never transfers ownership. */
            if(status.magic!=BOOT_LIGHT_UNIFIED_MAGIC) {
                if(!modern&&!shown)return 0;
            } else {
                modern=true;
                if(status_valid(&status)&&!(status.flags&BOOT_LIGHT_HANDOFF)) {
                    g_snapshot=status;g_unified=true;g_snapshot_valid=true;
                    if(snapshot)*snapshot=status;
                    return 1;
                }
            }
        }
        if(modern&&retry<2U)usleep(10000);
    }
    if(shown) {
        /* The renderer can die after its first frame but before the UI starts.
         * Keep preparing self-test instead of replaying unrelated LVGL art. */
        g_unified=true;g_snapshot_valid=false;
        return 1;
    }
    if(!modern)return 0;
    /* Known root players never fall through to legacy T on a slow probe.
     * If it died, only an actually free lease allows the ordinary UI fallback. */
    int acquired=free_lease_fallback(true);
    if(acquired!=0)return acquired==1?0:-1;
    g_unified=true;g_snapshot_valid=false;
    return 1;
}

int startup_visual_poll(bool ready,bool error)
{
    if(!g_unified)return -1;
    boot_light_status_t status={0};int received=-1;
    char request=!g_snapshot_valid?BOOT_LIGHT_PROBE:
                 error?BOOT_LIGHT_UI_ERROR:ready?BOOT_LIGHT_UI_READY:BOOT_LIGHT_PROBE;
    if(!status_exchange(request,&status,&received)) {
        /* A dead renderer cannot leave us waiting forever. Never substitute a
         * missing socket for the real lock: a slow renderer may still draw. */
        return free_lease_fallback(ready||error);
    }
    if(!g_snapshot_valid) {
        if(received>=0)close(received);
        if(!status_valid(&status)||(status.flags&BOOT_LIGHT_HANDOFF))return free_lease_fallback(ready||error);
        g_snapshot=status;g_snapshot_valid=true;
        return 0;
    }
    if(!status_valid(&status)||status.origin_ms!=g_snapshot.origin_ms||
       status.mode!=g_snapshot.mode||status.language!=g_snapshot.language||
       memcmp(status.theme_id,g_snapshot.theme_id,sizeof(status.theme_id))) {
        if(received>=0)close(received);
        return free_lease_fallback(ready||error);
    }
    if(!(status.flags&BOOT_LIGHT_HANDOFF)) {
        if(received>=0)close(received);
        return 0;
    }
    if((!ready&&!error)||received<0) {
        if(received>=0)close(received);
        return free_lease_fallback(ready||error);
    }
    if(g_inherited_fb<0)g_inherited_fb=received;
    else close(received);
    /* The player stopped before sending the FD, but closes its lease just
     * after the reply. Wait only for that bounded release, never draw early. */
    for(unsigned retry=0;retry<100;retry++) {
        int acquired=try_lease();
        if(acquired<0)return -1;
        if(acquired==1){g_unified=false;return 1;}
        usleep(5000);
    }
    return 0;
}

int startup_visual_acquire(uint32_t *elapsed_ms)
{
    const char *enabled=getenv("UN260_BOOT_LIGHT_ACTIVE");
    if(!enabled||strcmp(enabled,"1"))return 0;
    int adopted=0;
    int sock=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0);
    struct sockaddr_un address={.sun_family=AF_UNIX};strcpy(address.sun_path,BOOT_LIGHT_SOCKET);
    if(sock>=0&&connect(sock,(struct sockaddr*)&address,sizeof(address))==0) {
        const char request='T';
        struct pollfd p={sock,POLLIN,0};
        if(send(sock,&request,1,MSG_NOSIGNAL)==1&&poll(&p,1,500)>0) {
            boot_light_reply_t reply={0};struct iovec iov={&reply,sizeof(reply)};
            union {struct cmsghdr alignment;char bytes[CMSG_SPACE(sizeof(int))];} control={0};
            struct msghdr msg={.msg_iov=&iov,.msg_iovlen=1,.msg_control=control.bytes,.msg_controllen=sizeof(control.bytes)};
            ssize_t n=recvmsg(sock,&msg,MSG_CMSG_CLOEXEC);
            struct cmsghdr *c=CMSG_FIRSTHDR(&msg);
            if(c&&c->cmsg_level==SOL_SOCKET&&c->cmsg_type==SCM_RIGHTS&&c->cmsg_len==CMSG_LEN(sizeof(int)))
                memcpy(&g_inherited_fb,CMSG_DATA(c),sizeof(int));
            if(n==sizeof(reply)&&!(msg.msg_flags&(MSG_TRUNC|MSG_CTRUNC))&&g_inherited_fb>=0&&
               reply.magic==BOOT_LIGHT_MAGIC&&reply.elapsed_ms<=20000U) {
                *elapsed_ms=reply.elapsed_ms;adopted=1;
            }
        }
    }
    if(sock>=0)close(sock);
    for(unsigned retry=0;retry<100;retry++) {
        int acquired=try_lease();
        if(acquired<0)return -1;
        if(acquired==1)return adopted;
        usleep(5000);
    }
    /* Never start a second writer merely because the handshake timed out. */
    return -1;
}

#ifndef STARTUP_VISUAL_UI_HEALTH
#define STARTUP_VISUAL_UI_HEALTH "/tmp/un260-ui.health"
#endif

void startup_visual_note_ui_presented(void)
{
    static bool attempted;
    if(attempted)return;
    attempted=true;
    char temporary[]=STARTUP_VISUAL_UI_HEALTH ".XXXXXX";
    char record[32];
    int length=snprintf(record,sizeof(record),"%ld\n",(long)getpid());
    if(length<=0||(size_t)length>=sizeof(record))return;
    int fd=mkstemp(temporary);
    if(fd<0)return;
    bool valid=fcntl(fd,F_SETFD,FD_CLOEXEC)==0&&fchmod(fd,0600)==0;
    size_t written=0;
    while(valid&&written<(size_t)length) {
        ssize_t count=write(fd,record+written,(size_t)length-written);
        if(count<0&&errno==EINTR)continue;
        if(count<=0){valid=false;break;}
        written+=(size_t)count;
    }
    if(close(fd)<0)valid=false;
    if(valid&&rename(temporary,STARTUP_VISUAL_UI_HEALTH)==0)return;
    unlink(temporary);
}
