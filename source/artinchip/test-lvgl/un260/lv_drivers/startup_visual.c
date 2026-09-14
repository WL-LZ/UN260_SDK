#define _GNU_SOURCE
#include "startup_visual.h"
#include "boot_light_protocol.h"
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* Process-lifetime ownership. Do not close the inherited framebuffer early:
 * this BSP's fb release callback releases imported display DMA buffers. */
static int g_lease=-1,g_inherited_fb=-1;
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
    g_lease=open(BOOT_LIGHT_LOCK,O_CREAT|O_RDWR|O_CLOEXEC,0600);
    if(g_lease<0)return -1;
    for(unsigned retry=0;retry<100;retry++) {
        if(flock(g_lease,LOCK_EX|LOCK_NB)==0)return adopted;
        usleep(5000);
    }
    /* Never start a second writer merely because the handshake timed out. */
    return -1;
}
