#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
static int cut(const char *site) {const char *s=getenv("UN260_HISTORY_CUT");return s && !strcmp(s,site);}
int renameat(int oldfd,const char *old,int newfd,const char *name)
{
    int (*real)(int,const char*,int,const char*)=dlsym(RTLD_NEXT,"renameat");
    const char *release=getenv("UN260_RETURN_RELEASE");
    if (release && !strcmp(old,"index.cfg.tmp") && !strcmp(name,"index.cfg")) {
        int marker=open(getenv("UN260_RETURN_READY"),O_CREAT|O_EXCL|O_WRONLY,0600);
        if (marker<0 || close(marker)) _exit(90);
        unsigned i=0;
        while(access(release,F_OK) && i++<12000) usleep(1000);
        if(access(release,F_OK)) _exit(91);
    }
    if (!strcmp(old,"index.cfg.tmp") && !strcmp(name,"index.cfg") && cut("restore-prepublish")) _exit(86);
    int rc=real(oldfd,old,newfd,name);
    if (!rc && !strcmp(old,"index.cfg.tmp") && !strcmp(name,"index.cfg") && cut("restore-published")) _exit(87);
    if (!rc && !strcmp(name,".committed-index-v1.cfg") && cut("backup")) _exit(81);
    return rc;
}
int renameat2(int oldfd,const char *old,int newfd,const char *name,unsigned flags)
{
    int (*real)(int,const char*,int,const char*,unsigned)=dlsym(RTLD_NEXT,"renameat2");
    if (!strcmp(old,".un260-index-switch-v1") && !strcmp(name,"index.cfg") && cut("before-exchange")) _exit(82);
    int rc=real(oldfd,old,newfd,name,flags);
    if (!rc && !strcmp(old,".un260-index-switch-v1") && !strcmp(name,"index.cfg") && cut("exchange")) _exit(83);
    return rc;
}
int unlinkat(int fd,const char *name,int flags)
{
    int (*real)(int,const char*,int)=dlsym(RTLD_NEXT,"unlinkat");
    int rc=real(fd,name,flags);
    if (!rc && !strcmp(name,".un260-index-switch-v1") && cut("release")) _exit(84);
    return rc;
}
int rename(const char *old,const char *name)
{
    int (*real)(const char*,const char*)=dlsym(RTLD_NEXT,"rename");
    int rc=real(old,name);
    if (!rc && strstr(old,"/index.cfg.tmp") && cut("commit")) _exit(85);
    return rc;
}
