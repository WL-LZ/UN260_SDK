#define _GNU_SOURCE
#include "boot_trial.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

static bool hex(const char *s, size_t length)
{
    if(strlen(s)!=length)return false;
    for(size_t i=0;i<length;i++)
        if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    return true;
}
static bool uuid(const char *s)
{
    if(strlen(s)!=36)return false;
    for(unsigned i=0;i<36;i++) {
        if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}
        else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;
    }
    return true;
}
static bool directory(int fd, bool private)
{
    struct stat st;
    return fd>=0&&!fstat(fd,&st)&&S_ISDIR(st.st_mode)&&st.st_uid==geteuid()&&
           !(st.st_mode&(0022|S_ISUID|S_ISGID))&&(!private||(st.st_mode&0777)==0700);
}
static int open_journal(const char *path)
{
    /* Walk every component with O_NOFOLLOW, including the journal's parents. */
    char copy[PATH_MAX];
    if(path[0]!='/'||strlen(path)>=sizeof(copy))return -1;
    strcpy(copy,path);
    int fd=open("/",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    char *save=NULL,*part=strtok_r(copy,"/",&save);
    while(part&&fd>=0) {
        if(!strcmp(part,".")||!strcmp(part,"..")){close(fd);return -1;}
        int next=openat(fd,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        close(fd);fd=next;
        if(!directory(fd,false)){if(fd>=0)close(fd);return -1;}
        part=strtok_r(NULL,"/",&save);
    }
    if(!directory(fd,true)){if(fd>=0)close(fd);return -1;}
    return fd;
}
static int file(int dir, const char *name, size_t maximum, bool private)
{
    int fd=openat(dir,name,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    struct stat st;
    if(fd<0)return -1;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||
       (st.st_mode&(0022|S_ISUID|S_ISGID))||st.st_size<0||
       (uint64_t)st.st_size>maximum||(private&&(st.st_mode&0777)!=0600)) {
        close(fd);return -1;
    }
    return fd;
}
static bool text(int fd, char *out, size_t size)
{
    struct stat st;
    if(fd<0||fstat(fd,&st)||st.st_size<=0||(uint64_t)st.st_size>=size)return false;
    size_t done=0,bytes=(size_t)st.st_size;
    while(done<bytes) {
        ssize_t n=pread(fd,out+done,bytes-done,(off_t)done);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return false;
        done+=(size_t)n;
    }
    if(out[done-1]!='\n'||memchr(out,0,done))return false;
    out[done]=0;return true;
}
static bool read_named(int dir,const char *name,char *out,size_t size)
{
    int fd=file(dir,name,size-1,true);
    bool ok=text(fd,out,size);
    if(fd>=0)close(fd);
    return ok;
}
static bool hash_named(int dir,const char *name,const char *expected,size_t maximum)
{
    struct stat st;
    if(!strcmp(expected,"none"))
        return fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)<0&&errno==ENOENT;
    int fd=file(dir,name,maximum,true);
    char actual[65];
    bool ok=fd>=0&&boot_bundle_sha256_fd(fd,actual)&&!strcmp(actual,expected);
    if(fd>=0)close(fd);
    return ok;
}
static bool members(int fd)
{
    /* Transitional, rollback and confirmation records deliberately stay closed.
     * The updater alone interprets/replays the transaction and target inventory. */
    static const char *allowed[]={"state","committed","trial","trial.targets",
        "trial.boot-active","trial.boot-active.previous","trial.installed.fnv64",
        "trial.unified.completed","boot-publish","boot-gc"};
    int copy=dup(fd);if(copy<0)return false;
    DIR *d=fdopendir(copy);if(!d){close(copy);return false;}
    bool ok=true;struct dirent *e;
    errno=0;
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        bool known=false;
        for(size_t i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++)
            if(!strcmp(e->d_name,allowed[i])){known=true;break;}
        int item=known?file(fd,e->d_name,3145728,false):-1;
        if(item<0){ok=false;break;}
        close(item);errno=0;
    }
    if(errno)ok=false;
    closedir(d);return ok;
}
int boot_trial_visual_read(const char *journal,const char *boot_id,
                          boot_trial_visual_t *visual)
{
    memset(visual,0,sizeof(*visual));
    struct stat st;
    if(lstat(journal,&st)<0)return errno==ENOENT?0:-1;
    int dir=open_journal(journal),fd=-1,result=-1;
    if(dir<0)return -1;
    char record[1025],*fields[11],*cursor=record;
    static const char *keys[]={"boot_id=","app_sha256=","target_id=","boot_bundle_id=",
        "boot_manifest_sha256=","old_active_sha256=","old_previous_sha256=",
        "old_installed_fnv64_sha256=","old_unified_completed_sha256=","targets_count="};
    if(!members(dir)||!read_named(dir,"trial",record,sizeof(record)))goto done;
    for(unsigned i=0;i<11;i++) {
        fields[i]=cursor;char *end=strchr(cursor,'\n');if(!end)goto done;
        *end=0;cursor=end+1;
        if(i) {
            size_t n=strlen(keys[i-1]);if(strncmp(fields[i],keys[i-1],n))goto done;
            fields[i]+=n;
        }
    }
    if(*cursor||strcmp(fields[0],"UN260_UPDATE_TRIAL_V1")||!uuid(fields[1]))goto done;
    for(unsigned i=2;i<=9;i++)
        if(!hex(fields[i],64)&&(i<6||strcmp(fields[i],"none")))goto done;
    if(!strcmp(fields[6],"none")&&strcmp(fields[7],"none"))goto done;
    size_t count=0;
    if(!fields[10][0]||fields[10][0]=='0'||strlen(fields[10])>4)goto done;
    for(const char *p=fields[10];*p;p++) {
        if(*p<'0'||*p>'9')goto done;
        count=count*10+(unsigned)(*p-'0');
    }
    if(count>8192)goto done;
    fd=file(dir,"committed",0,false);if(fd<0)goto done;
    close(fd);fd=file(dir,"state",3145728,true);if(fd<0)goto done;
    close(fd);fd=-1;
    if(!hash_named(dir,"trial.targets",fields[3],3145728)||
       !hash_named(dir,"trial.boot-active",fields[6],400)||
       !hash_named(dir,"trial.boot-active.previous",fields[7],400)||
       !hash_named(dir,"trial.installed.fnv64",fields[8],400)||
       !hash_named(dir,"trial.unified.completed",fields[9],400))goto done;
    /* A same-boot service restart cannot turn installation into a trial boot. */
    fd=open(boot_id,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    char now[38]={0};
    ssize_t n=fd>=0?read(fd,now,sizeof(now)):-1;
    if(n!=37||now[36]!='\n')goto done;
    now[36]=0;if(!uuid(now)||!strcmp(now,fields[1]))goto done;
    visual->pending=true;
    strcpy(visual->bundle_id,fields[4]);strcpy(visual->manifest_sha256,fields[5]);
    result=1;
done:
    if(fd>=0)close(fd);
    close(dir);return result;
}
bool boot_trial_visual_matches(const boot_trial_visual_t *visual,const boot_bundle_t *bundle)
{
    return !visual->pending||(!bundle->previous&&!strcmp(visual->bundle_id,bundle->id)&&
                             !strcmp(visual->manifest_sha256,bundle->manifest_sha256));
}
int boot_trial_visual_lock(void)
{
    int fd=open("/tmp/un260-updater.flock",O_RDWR|O_CREAT|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC,0600);
    struct stat st;
    if(fd<0)return -1;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||
       (st.st_mode&07777)!=0600||st.st_nlink!=1||flock(fd,LOCK_EX|LOCK_NB)) {
        close(fd);return -1;
    }
    return fd;
}

bool boot_trial_visual_recovery_ready(const char *script,const char *checksum)
{
    /* Old fixed rescue runtimes do not release trial visuals before rollback.
     * A normal UPK cannot replace that immutable factory layer. Fail closed
     * until a matching IMG establishes both primary and rescue ownership. */
    int fd=file(AT_FDCWD,checksum,65,false);
    char expected[66],actual[65],header[1025];
    bool ok=text(fd,expected,sizeof(expected));
    if(fd>=0)close(fd);
    if(!ok||strlen(expected)!=65)return false;
    expected[64]=0;if(!hex(expected,64))return false;
    fd=file(AT_FDCWD,script,262144,false);
    if(fd<0)return false;
    ssize_t n=pread(fd,header,sizeof(header)-1,0);
    ok=n>0&&!memchr(header,0,(size_t)n);
    if(ok) {
        header[n]=0;
        ok=strstr(header,"\n# UN260_TRIAL_VISUAL_CLEANUP_V1\n")!=NULL&&
           boot_bundle_sha256_fd(fd,actual)&&!strcmp(expected,actual);
    }
    close(fd);return ok;
}
