/* Minimal PID 1 for the UN260 NAND image. No protocol or self-test ownership. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
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
#include "un260/lv_drivers/boot_light_protocol.h"

int boot_light_run(void);
static unsigned long long milliseconds(void)
{
    struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);
    return (unsigned long long)t.tv_sec*1000+t.tv_nsec/1000000;
}
static void milestone(const char *name)
{
    dprintf(STDERR_FILENO,"EARLY_INIT uptime_ms=%llu stage=%s\n",milliseconds(),name);
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
static void stop_child(pid_t child)
{
    if(child<=0)return;
    if(waitpid(child,NULL,WNOHANG)==child)return;
    kill(child,SIGTERM);
    for(unsigned i=0;i<50;i++) {
        if(waitpid(child,NULL,WNOHANG)==child)return;
        usleep(10000);
    }
    kill(child,SIGKILL);waitpid(child,NULL,0);
}
static void fatal(const char *operation)
{
    int saved=errno;
    dprintf(STDERR_FILENO,"EARLY_INIT failure=%s errno=%d\n",operation,saved);
    int fd=open("/dev/console",O_WRONLY|O_NOCTTY);
    if(fd>=0){dprintf(fd,"UN260 early init failed: %s: %s. Use previous system image for recovery.\n",operation,strerror(saved));close(fd);}
    /* PID 1 must not exit or reset repeatedly on a missing/corrupt rootfs. */
    for(;;)pause();
}
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
    int fd=open("/proc/cmdline",O_RDONLY);
    if(fd<0)fatal("read cmdline");
    ssize_t n=read(fd,cmd,sizeof(cmd)-1);close(fd);if(n<=0)fatal("empty cmdline");
    argument(cmd,"root=",root,sizeof(root));argument(cmd,"rootfstype=",type,sizeof(type));
    argument(cmd,"rootflags=",options,sizeof(options));argument(cmd,"init=",init,sizeof(init));
    argument(cmd,"un260.boot_light=",disable,sizeof(disable));
    argument(cmd,"un260.boot_frames=",frame_trace,sizeof(frame_trace));
    /* Independent from milestone logging. Retain this explicit opt-in across
       switch-root; absence leaves the real init free to use its file flag. */
    if(!strcmp(frame_trace,"1"))setenv("UN260_BOOT_FRAME_TRACE","1",1);
    else unsetenv("UN260_BOOT_FRAME_TRACE");
    if(!root[0]||init[0]!='/'){errno=EINVAL;fatal("root/init arguments");}
    pid_t child=-1;
    if(strcmp(disable,"0")) {
        child=fork();
        if(child==0){setenv("UN260_BOOT_TRACE","1",1);_exit(boot_light_run());}
    }
    milestone("root_mount_begin");
    /* Preserve kernel root= and rootflags=; retry transient UBI readiness.
       No shell parsing, no storage repair, formatting or persistent writes. */
    unsigned tries=0;
    while(mount(root,"/newroot",type,MS_RDONLY,options[0]?options:NULL)) {
        if(++tries>=300){stop_child(child);fatal("mount rootfs");}
        usleep(100000);
    }
    milestone("root_mounted");
    bool off=access("/newroot/etc/un260/boot-light.disabled",F_OK)==0||
             access("/newroot/etc/un260/early-display.disabled",F_OK)==0||
             access("/newroot/etc/pointercal",F_OK)!=0||
             access("/newroot/var/lib/un260-updater/transaction",F_OK)==0;
    if(off){stop_child(child);child=-1;}
    /* Finish the first submission before moving /dev: renderer opens its
       IPC directory before this marker and retains it across the move. */
    if(child>0) {
        unsigned i;
        for(i=0;i<150;i++) {
            if(access(BOOT_LIGHT_READY,F_OK)==0)break;
            if(waitpid(child,NULL,WNOHANG)==child){child=-1;break;}
            usleep(10000);
        }
        if(i==150){stop_child(child);child=-1;}
    }
    if(access("/newroot/dev",F_OK)||access("/newroot/proc",F_OK)||access("/newroot/sys",F_OK)) {
        stop_child(child);errno=ENOENT;fatal("rootfs mountpoints");
    }
    milestone("switch_root");
    /* /dev is the shared IPC surface; /run and /tmp are remounted by init. */
    if(mount("/dev","/newroot/dev",NULL,MS_MOVE,NULL)||
       mount("/proc","/newroot/proc",NULL,MS_MOVE,NULL)||
       mount("/sys","/newroot/sys",NULL,MS_MOVE,NULL))fatal("move virtual filesystems");
    /* Only our generated initramfs executable is removed, never rootfs data. */
    unlink("/init");
    if(chdir("/newroot")||mount(".","/",NULL,MS_MOVE,NULL)||chroot(".")||chdir("/"))fatal("switch root");
    int console=open("/dev/console",O_RDWR|O_NOCTTY);
    if(console>=0){for(int i=0;i<3;i++)dup2(console,i);if(console>2)close(console);}
    unsetenv("UN260_BOOT_TRACE");
    char *argv[]={init,NULL};execv(init,argv);
    fatal("exec real init");return 1;
}
