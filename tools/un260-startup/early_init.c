/* Stable PID 1: mount the real root and launch a verified root-resident player.
 * Customer artwork, languages and animation code belong to the root bundle. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "un260/boot/boot_bundle.h"
#include "boot_trial.h"
static boot_trial_visual_t trial_visual;
extern char **environ;
static unsigned long long milliseconds(void)
{
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (unsigned long long)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void milestone(const char *name)
{
    dprintf(STDERR_FILENO,"EARLY_INIT uptime_ms=%llu stage=%s\n",milliseconds(),name);
}
#ifndef UN260_BOOT_LAUNCH_ONLY
/* Unified requests remain installed after completion. Defer branding only
 * while their durable receipt is missing, different or unsafe to read. */
static int request_file(const char *path,size_t bytes)
{
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return -1;
    struct stat st;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=0||
       (st.st_mode&(022|S_ISUID|S_ISGID))||st.st_size!=(off_t)bytes) {
        close(fd);errno=EINVAL;return -1;
    }
    return fd;
}
static bool digest_line(const char *text)
{
    for(unsigned i=0;i<64;i++)
        if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return false;
    return text[64]=='\n';
}
static bool unified_request_pending(void)
{
    char request[130],receipt[65],digest[65];
    int fd=request_file("/etc/un260/unified-request",sizeof(request));
    if(fd<0)return errno!=ENOENT;
    bool valid=pread(fd,request,sizeof(request),0)==(ssize_t)sizeof(request)&&
               digest_line(request)&&digest_line(request+65)&&
               boot_bundle_sha256_fd(fd,digest);
    close(fd);
    if(!valid)return true;
    fd=request_file("/var/lib/un260-updater/unified.completed",sizeof(receipt));
    if(fd<0)return true;
    valid=pread(fd,receipt,sizeof(receipt),0)==(ssize_t)sizeof(receipt)&&
          digest_line(receipt)&&!memcmp(receipt,digest,64);
    close(fd);
    return !valid;
}
static void argument(const char *cmd,const char *key,char *out,size_t size)
{
    size_t n=strlen(key);
    for(const char *p=cmd;*p;) {
        while(*p==' '||*p=='\n')p++;
        const char *end=p;while(*end&&*end!=' '&&*end!='\n')end++;
        if((size_t)(end-p)>n&&!strncmp(p,key,n)) {
            size_t len=(size_t)(end-p)-n;
            if(len<size){memcpy(out,p+n,len);out[len]=0;}
        }
        p=end;
    }
}
#endif
static void stop_child(pid_t child)
{
    if(child<=0||waitpid(child,NULL,WNOHANG)==child)return;
    kill(child,SIGTERM);
    for(unsigned i=0;i<50;i++) {
        if(waitpid(child,NULL,WNOHANG)==child)return;
        usleep(10000);
    }
    kill(child,SIGKILL);waitpid(child,NULL,0);
}
#ifndef UN260_BOOT_LAUNCH_ONLY
static void fatal(const char *operation)
{
    int saved=errno;
    dprintf(STDERR_FILENO,"EARLY_INIT failure=%s errno=%d\n",operation,saved);
    int fd=open("/dev/console",O_WRONLY|O_NOCTTY);
    if(fd>=0){dprintf(fd,"UN260 early init failed: %s: %s. Use a matching recovery image.\n",operation,strerror(saved));close(fd);}
    for(;;)pause();
}
#endif
static bool first_frame(pid_t child)
{
    char value[32];int fd=open(BOOT_BUNDLE_READY_PATH,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)return false;
    ssize_t count=read(fd,value,sizeof(value)-1);close(fd);
    if(count<=0)return false;
    value[count]=0;
    if(value[0]<'0'||value[0]>'9')return false;
    char *end=NULL;errno=0;long owner=strtol(value,&end,10);
    return !errno&&end!=value&&(*end==0||(*end=='\n'&&end[1]==0))&&owner==child;
}
static bool write_ram_record(const char *destination,const char *record,size_t length)
{
    char path[128];
    snprintf(path,sizeof(path),"%s.%ld.new",destination,(long)getpid());
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return false;
    size_t done=0;
    while(done<(size_t)length) {
        ssize_t count=write(fd,record+done,(size_t)length-done);
        if(count<0&&errno==EINTR)continue;
        if(count<=0){close(fd);unlink(path);return false;}
        done+=(size_t)count;
    }
    if(close(fd)||rename(path,destination)){unlink(path);return false;}
    return true;
}
static bool trial_visual_ready(void)
{
    if(boot_trial_visual_read("/var/lib/un260-updater/transaction",
        "/proc/sys/kernel/random/boot_id",&trial_visual)<0)return false;
    if(trial_visual.pending&&(!boot_trial_visual_recovery_ready(
        "/usr/local/lib/un260-startup/runtime.sh","/etc/un260/startup-runtime.sha256")||
        !boot_trial_visual_recovery_ready("/usr/local/lib/un260-updater/rescue/startup.sh",
        "/usr/local/lib/un260-updater/rescue/startup.sha256"))) {
        milestone("trial_visual_requires_rescue_base");return false;
    }
    return true;
}
static bool pin_live_bundle(const boot_bundle_t *bundle)
{
    char record[256];
    int length=snprintf(record,sizeof(record),"%s\n%s\n%s\nnone\nnone\n",
                        BOOT_BUNDLE_ACTIVE_MAGIC,bundle->id,bundle->manifest_sha256);
    return write_ram_record(BOOT_BUNDLE_LIVE_PATH,record,(size_t)length);
}
static pid_t start_visual(bool previous_only,bool *selected_previous)
{
    boot_bundle_t bundle;
    if(!boot_bundle_open(BOOT_BUNDLE_ACTIVE_PATH,BOOT_BUNDLE_VERSIONS_PATH,previous_only,&bundle))return -1;
    *selected_previous=bundle.previous;
    if(!boot_trial_visual_matches(&trial_visual,&bundle)) {
        boot_bundle_close(&bundle);milestone("trial_visual_bundle_mismatch");return -1;
    }
    dprintf(STDERR_FILENO,"EARLY_INIT bundle=%s source=%s\n",bundle.id,bundle.previous?"previous":"active");
    milestone("player_launch");
    pid_t child=fork();
    if(child==0) {
        int flags=fcntl(bundle.theme_fd,F_GETFD);
        if(flags<0||fcntl(bundle.theme_fd,F_SETFD,flags&~FD_CLOEXEC)<0)_exit(126);
        char fd_text[24];snprintf(fd_text,sizeof(fd_text),"%d",bundle.theme_fd);
        char *argv[]={"un260_boot_player","--theme-fd",fd_text,"--bundle-id",bundle.id,"--mode",bundle.mode,NULL};
        fexecve(bundle.player_fd,argv,environ);
        dprintf(STDERR_FILENO,"EARLY_INIT player_exec_failed errno=%d\n",errno);
        _exit(127);
    }
    boot_bundle_close(&bundle);
    if(child<0){milestone("player_fork_failed");return -1;}
    for(unsigned i=0;i<150;i++) {
        /* Reap before trusting a marker: a renderer that exited cannot retain
         * display ownership even if its final marker survived. */
        if(waitpid(child,NULL,WNOHANG)==child){milestone("player_exited");return -1;}
        if(first_frame(child)) {
            if(!pin_live_bundle(&bundle)) {
                stop_child(child);milestone("bundle_pin_failed");return -1;
            }
            milestone("brand_first_frame");return child;
        }
        usleep(10000);
    }
    stop_child(child);milestone("player_first_frame_timeout");return -1;
}
#ifndef UN260_BOOT_LAUNCH_ONLY
int main(void)
{
    if(getpid()!=1){fprintf(stderr,"early init must be PID 1\n");return 1;}
    mkdir("/dev",0755);mkdir("/proc",0555);mkdir("/sys",0555);mkdir("/newroot",0755);
    if(mount("devtmpfs","/dev","devtmpfs",MS_NOSUID,NULL))fatal("mount devtmpfs");
    if(mount("proc","/proc","proc",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))fatal("mount proc");
    if(mount("sysfs","/sys","sysfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,NULL))fatal("mount sysfs");
    int log=open("/dev/.un260-early.log",O_CREAT|O_WRONLY|O_TRUNC,0600);
    if(log>=0){dup2(log,STDOUT_FILENO);dup2(log,STDERR_FILENO);if(log>2)close(log);}
    milestone("init_enter");
    char cmd[4096]={0},root[256]="",type[64]="ubifs",options[512]="",init[256]="/linuxrc",disable[8]="",frame_trace[8]="";
    int fd=open("/proc/cmdline",O_RDONLY|O_CLOEXEC);
    if(fd<0)fatal("read cmdline");
    ssize_t n=read(fd,cmd,sizeof(cmd)-1);close(fd);if(n<=0)fatal("empty cmdline");
    argument(cmd,"root=",root,sizeof(root));argument(cmd,"rootfstype=",type,sizeof(type));
    argument(cmd,"rootflags=",options,sizeof(options));argument(cmd,"init=",init,sizeof(init));
    argument(cmd,"un260.boot_light=",disable,sizeof(disable));
    argument(cmd,"un260.boot_frames=",frame_trace,sizeof(frame_trace));
    if(!strcmp(frame_trace,"1"))setenv("UN260_BOOT_FRAME_TRACE","1",1);
    else if(!strcmp(frame_trace,"0"))setenv("UN260_BOOT_FRAME_TRACE","0",1);
    else unsetenv("UN260_BOOT_FRAME_TRACE");
    if(!root[0]||init[0]!='/'){errno=EINVAL;fatal("root/init arguments");}
    milestone("root_mount_begin");
    unsigned tries=0;
    while(mount(root,"/newroot",type,MS_RDONLY,options[0]?options:NULL)) {
        if(++tries>=300)fatal("mount rootfs");
        usleep(100000);
    }
    milestone("root_mounted");
    if(access("/newroot/dev",F_OK)||access("/newroot/proc",F_OK)||access("/newroot/sys",F_OK)) {
        errno=ENOENT;fatal("rootfs mountpoints");
    }
    /* Move once before launching. The root player is born with final paths,
     * while its /dev socket, lease and retained framebuffer FD remain stable. */
    milestone("switch_root");
    if(mount("/dev","/newroot/dev",NULL,MS_MOVE,NULL)||
       mount("/proc","/newroot/proc",NULL,MS_MOVE,NULL)||
       mount("/sys","/newroot/sys",NULL,MS_MOVE,NULL))fatal("move virtual filesystems");
    unlink("/init");
    if(chdir("/newroot")||mount(".","/",NULL,MS_MOVE,NULL)||chroot(".")||chdir("/"))fatal("switch root");
    /* Only this stable PID 1 attests the new image base. A root service on an
     * old embedded-artwork kernel must never claim compatible early boot. */
    static const char platform[]="UN260_BOOT_PLATFORM_V1\n";
    if(write_ram_record("/dev/.un260-boot-platform",platform,sizeof(platform)-1))
        milestone("boot_platform_ready");
    else milestone("boot_platform_unavailable");
    bool off=!strcmp(disable,"0")||access("/etc/un260/boot-light.disabled",F_OK)==0||
             access("/etc/un260/early-display.disabled",F_OK)==0||
             access("/etc/pointercal",F_OK)!=0||
             !trial_visual_ready()||
             unified_request_pending();
    if(!off) {
        bool previous=false;
        pid_t child=start_visual(false,&previous);
        if(child<0&&!previous){child=start_visual(true,&previous);}
        if(child<0)milestone("visual_fallback_to_application");
        else setenv("UN260_BOOT_UNIFIED_ACTIVE","1",1);
    } else milestone("visual_deferred");
    int console=open("/dev/console",O_RDWR|O_NOCTTY);
    if(console>=0){for(int i=0;i<3;i++)dup2(console,i);if(console>2)close(console);}
    unsetenv("UN260_BOOT_TRACE");
    char *argv[]={init,NULL};execv(init,argv);
    fatal("exec real init");return 1;
}
#else
/* Late service/restart fallback uses the same version verification and spawn
 * contract as PID 1, never a second compiled copy of customer artwork. */
static int launch_main(int argc,char **argv)
{
    if(argc==2&&!strcmp(argv[1],"--check")) {
        boot_bundle_t bundle;
        if(!boot_bundle_open(BOOT_BUNDLE_ACTIVE_PATH,BOOT_BUNDLE_VERSIONS_PATH,false,&bundle))return 1;
        boot_bundle_close(&bundle);return 0;
    }
    if(argc!=1)return 2;
    if(access("/etc/un260/boot-light.disabled",F_OK)==0||
       access("/etc/un260/early-display.disabled",F_OK)==0||
       access("/etc/pointercal",F_OK)!=0||
       !trial_visual_ready())return 1;
    /* Do not kill or restart a valid owner, including the maintenance visual. */
    char text[32]={0};int ready=open(BOOT_BUNDLE_READY_PATH,O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if(ready>=0) {
        ssize_t count=read(ready,text,sizeof(text)-1);close(ready);
        if(count>0) {
            char *end=NULL;errno=0;long owner=strtol(text,&end,10);
            if(text[0]>='0'&&text[0]<='9'&&!errno&&owner>1&&owner<=INT_MAX&&end!=text&&
               (*end==0||(*end=='\n'&&end[1]==0))&&kill((pid_t)owner,0)==0) {
                char path[64],comm[32]={0};
                snprintf(path,sizeof(path),"/proc/%ld/comm",owner);
                int proc=open(path,O_RDONLY|O_CLOEXEC);
                if(proc>=0) {
                    ssize_t bytes=read(proc,comm,sizeof(comm)-1);close(proc);
                    if(bytes==11&&!memcmp(comm,"un260-boot\n",11))return 0;
                }
            }
        }
    }
    bool previous=false;
    pid_t child=start_visual(false,&previous);
    if(child<0&&!previous)child=start_visual(true,&previous);
    return child>0?0:1;
}
int main(int argc,char **argv)
{
    /* --check is also used inside an already-locked upgrade transaction. */
    if(argc!=1)return launch_main(argc,argv);
    int lock=boot_trial_visual_lock();if(lock<0)return 1;
    int result=launch_main(argc,argv);
    close(lock);return result;
}
#endif
