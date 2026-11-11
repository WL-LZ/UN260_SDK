#define _GNU_SOURCE
#include "history_object_store.h"
#include "history_work_file.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

uint64_t history_free_kb(int fd)
{
    struct statvfs space;
    if (fstatvfs(fd,&space)) return 0;
    return (uint64_t)space.f_bavail*space.f_frsize/1024;
}

bool history_owned_marker(int dir,const char *name,const char *value)
{
    char buf[80];
    int fd=openat(dir,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if (fd<0) return false;
    struct stat st;
    ssize_t n=-1;
    if (!fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_uid==geteuid() && st.st_size==(off_t)strlen(value))
        n=read(fd,buf,sizeof(buf));
    close(fd);
    return n==(ssize_t)strlen(value) && !memcmp(buf,value,(size_t)n);
}

/* An interrupted creator leaves only a unique staging directory, never an
 * unowned final name. A later creator may publish without touching that
 * directory or claiming an unrelated pre-existing final directory. */
static int history_create_objects(int app)
{
    static const char owner[]="UN260_HISTORY_OBJECTS_1\n";
    char stage[128];
    snprintf(stage,sizeof(stage),"/proc/self/fd/%d/.un260-history-init-XXXXXX",app);
    if (!mkdtemp(stage)) return -1;
    const char *name=strrchr(stage,'/')+1;
    int dir=openat(app,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (dir<0) return -1;
    int marker=openat(dir,".owner",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    bool ok=marker>=0 && write(marker,owner,sizeof(owner)-1)==(ssize_t)(sizeof(owner)-1) && !fsync(marker);
    if (marker>=0 && close(marker)) ok=false;
    if (ok) ok=fsync(dir)==0;
    if (ok && !renameat2(app,name,app,HISTORY_OBJECTS,RENAME_NOREPLACE)) {
        close(dir);
        return fsync(app);
    }
    int error=errno;
    (void)unlinkat(dir,".owner",0);
    close(dir);
    (void)unlinkat(app,name,AT_REMOVEDIR);
    return ok && error==EEXIST?0:-1;
}

static int open_objects_dir(int app,bool create,bool lock)
{
    int dir=openat(app,HISTORY_OBJECTS,O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW);
    if (create && dir<0 && errno==ENOENT && !history_create_objects(app))
        dir=openat(app,HISTORY_OBJECTS,O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOFOLLOW);
    if (dir<0) return -1;
    struct stat st;
    if (fstat(dir,&st) || st.st_uid!=geteuid() || (st.st_mode&0777)!=0700 ||
        !history_owned_marker(dir,".owner","UN260_HISTORY_OBJECTS_1\n") || (lock && flock(dir,LOCK_EX|LOCK_NB))) {
        close(dir);
        return -1;
    }
    return dir;
}

int history_objects_open(int app)
{return open_objects_dir(app,true,true);}

static bool history_object_matches_memory(int fd,const unsigned char *bytes,size_t size)
{
    struct stat st;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1 || st.st_uid!=geteuid() ||
        st.st_size!=(off_t)size) return false;
    unsigned char data[4096];
    for (size_t pos=0;pos<size;) {
        size_t n=size-pos<sizeof(data)?size-pos:sizeof(data);
        if (pread(fd,data,n,(off_t)pos)!=(ssize_t)n || memcmp(data,bytes+pos,n)) return false;
        pos+=n;
    }
    return true;
}

/* Root records use one small, verified indirection rather than allocating
 * a filesystem data block for every long absolute target on low-space roots. */
bool history_object_target(int root,const char *prefix,const char *name,char *target,size_t capacity,bool create)
{
    static const char alias[]=".un260-objects";
    char actual[PATH_MAX];
    ssize_t n=readlinkat(root,alias,actual,sizeof(actual));
    if (n<0 && errno==ENOENT && create) {
        if (symlinkat(prefix,root,alias) && errno!=EEXIST) return false;
        if (fsync(root)) return false;
        n=readlinkat(root,alias,actual,sizeof(actual));
    }
    if (n!=(ssize_t)strlen(prefix) || memcmp(actual,prefix,(size_t)n)) return false;
    return snprintf(target,capacity,"%s/%s",alias,name)<(int)capacity;
}

/* Use the same private object directory as upgrade-time placement. Current
 * and previous applications read the canonical per-record path in root. */
int history_place_new_detail(const char *history_path,const char *name,
                                           const unsigned char *bytes,size_t size)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (root<0) return 0;
    int app=-1,objects=-1,tmp=-1;
    int result=0;
    struct stat history_fs,app_fs;
    struct stat existing;
    bool named=fstatat(root,name,&existing,AT_SYMLINK_NOFOLLOW)==0;
    if (!named && errno!=ENOENT) { result=-1; goto done; }
    if (named && !S_ISLNK(existing.st_mode)) {
        if (!S_ISREG(existing.st_mode) || existing.st_uid!=geteuid() || existing.st_nlink!=1) result=-1;
        goto done;
    }
    /* A present link is accepted only after its owned destination and bytes
     * have been verified, even when root recovered ample space meanwhile. */
    if (named) result=-1;
    if (!named && history_free_kb(root)>=HISTORY_ROOT_FLOOR_KB+(size+1023)/1024) goto done;
    app=open(UI_HISTORY_APP_VOLUME,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (app<0 || fstat(root,&history_fs) || fstat(app,&app_fs) || history_fs.st_dev==app_fs.st_dev ||
        !history_owned_marker(app,".un260-owner","UN260_APP_VOLUME_V1\n")) goto done;
    objects=history_objects_open(app);
    if (objects<0) goto done;
    if (named) {
        char expected[PATH_MAX],actual[PATH_MAX];
        if (!history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,name,expected,sizeof(expected),false)) goto done;
        ssize_t length=readlinkat(root,name,actual,sizeof(actual));
        if (length!=(ssize_t)strlen(expected) || memcmp(actual,expected,(size_t)length)) goto done;
        int present=openat(objects,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
        if (present<0) goto done;
        if (history_object_matches_memory(present,bytes,size) && !fsync(objects) && !fsync(root)) result=1;
        close(present);
        goto done;
    }
    /* Unlink a verified interrupted scratch file before creating a new one.
     * Never truncate a hardlink or an unexpected node in place. */
    if (!fstatat(objects,".new-detail",&existing,AT_SYMLINK_NOFOLLOW)) {
        if (!S_ISREG(existing.st_mode) || existing.st_uid!=geteuid() || unlinkat(objects,".new-detail",0)) goto done;
    } else if (errno!=ENOENT) goto done;
    int persisted=openat(objects,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    if (persisted>=0) {
        bool same=history_object_matches_memory(persisted,bytes,size);
        close(persisted);
        if (!same || fsync(objects)) goto done;
        goto publish;
    }
    if (errno!=ENOENT || history_free_kb(app)<HISTORY_APP_FLOOR_KB+(size+1023)/1024+8) goto done;
    tmp=openat(objects,".new-detail",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if (tmp<0 || fstat(tmp,&existing) || !S_ISREG(existing.st_mode) ||
        existing.st_nlink!=1 || existing.st_uid!=geteuid()) goto done;
    size_t pos=0;
    while (pos<size) {
        ssize_t wrote=write(tmp,bytes+pos,size-pos);
        if (wrote<0 && errno==EINTR) continue;
        if (wrote<=0) goto done;
        pos+=(size_t)wrote;
    }
    if (fsync(tmp) || !history_object_matches_memory(tmp,bytes,size)) goto done;
    /* Never overwrite an existing object of unknown provenance. */
    if (linkat(objects,".new-detail",objects,name,0)) goto done;
    if (unlinkat(objects,".new-detail",0) || fsync(objects)) goto done;
publish: ;
    char target[PATH_MAX];
    if (!history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,name,target,sizeof(target),true)) goto done;
    if (symlinkat(target,root,name)) goto done;
    result=fsync(root)?-1:1;
done:
    if (tmp>=0) close(tmp);
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    close(root);
    return result;
}

/* The caller has already durably removed this reference from the index. */
void history_remove_detail(const char *history_path,const char *name)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (root<0) return;
    char expected[PATH_MAX],link[PATH_MAX];
    bool known=history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,name,expected,sizeof(expected),false);
    ssize_t len=readlinkat(root,name,link,sizeof(link));
    bool managed=known && len==(ssize_t)strlen(expected) && !memcmp(link,expected,(size_t)len);
    if (!managed) { (void)unlinkat(root,name,0); close(root); return; }
    int app=open(UI_HISTORY_APP_VOLUME,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    int objects=app>=0 && history_owned_marker(app,".un260-owner","UN260_APP_VOLUME_V1\n")?history_objects_open(app):-1;
    if (!unlinkat(root,name,0) && !fsync(root) && objects>=0) {
        if (!unlinkat(objects,name,0)) (void)fsync(objects);
    }
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    close(root);
}


/* A full v5 index must be replaced atomically. Only on actual ENOSPC may
 * the last committed bytes temporarily live in the owned internal app store.
 * Neither the previous UI nor the new UI needs a different index format. */
#define INDEX_SWITCH ".un260-index-switch-v1"
#define INDEX_COPY_TMP ".index-copy-v1.tmp"
#define INDEX_MAX_BYTES (32U*1024U*1024U)

/* Managed backup names alone never prove ownership of their contents. All
 * released history formats write this prefix and a supported version line. */
static bool index_signature(int fd,const struct stat *st)
{
    static const char magic[]="magic=1212765012\nversion=";
    unsigned char bytes[sizeof(magic)+1];
    if (st->st_size<(off_t)sizeof(bytes) || pread(fd,bytes,sizeof(bytes),0)!=(ssize_t)sizeof(bytes)) return false;
    return !memcmp(bytes,magic,sizeof(magic)-1) && bytes[sizeof(magic)-1]>='2' &&
           bytes[sizeof(magic)-1]<='5' && bytes[sizeof(magic)]=='\n';
}

static bool own_index(int fd,struct stat *st)
{
    return fd>=0 && !fstat(fd,st) && S_ISREG(st->st_mode) && st->st_uid==geteuid() &&
           st->st_nlink==1 && st->st_size>0 && st->st_size<=INDEX_MAX_BYTES && index_signature(fd,st);
}

static bool same_index_inode(const struct stat *a,const struct stat *b)
{
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_size==b->st_size &&
        a->st_uid==b->st_uid && a->st_mode==b->st_mode && a->st_nlink==b->st_nlink &&
        a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
}

static bool index_name_current(int root,const char *name,const struct stat *st)
{
    struct stat current;
    return !fstatat(root,name,&current,AT_SYMLINK_NOFOLLOW) && same_index_inode(st,&current);
}

static bool same_index_bytes(int first,int second)
{
    struct stat a,b;
    if (!own_index(first,&a) || !own_index(second,&b) || a.st_size!=b.st_size) return false;
    char x[8192],y[sizeof(x)];
    for (off_t offset=0;offset<a.st_size;) {
        size_t n=(size_t)(a.st_size-offset);
        if (n>sizeof(x)) n=sizeof(x);
        if (pread(first,x,n,offset)!=(ssize_t)n || pread(second,y,n,offset)!=(ssize_t)n ||
            memcmp(x,y,n)) return false;
        offset+=(off_t)n;
    }
    return true;
}

static bool index_link_matches(int root,const char *name,const char *target)
{
    char actual[PATH_MAX];
    ssize_t n=readlinkat(root,name,actual,sizeof(actual));
    return n==(ssize_t)strlen(target) && !memcmp(actual,target,(size_t)n);
}

static int open_index_objects(int root,int *app,bool create,bool lock)
{
    struct stat r,a;
    *app=open(UI_HISTORY_APP_VOLUME,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (*app<0 || fstat(root,&r) || fstat(*app,&a) || r.st_dev==a.st_dev ||
        !history_owned_marker(*app,".un260-owner","UN260_APP_VOLUME_V1\n")) return -1;
    return open_objects_dir(*app,create,lock);
}

/* Trial confirmation and rollback use a single nonblocking dispatcher lock.
 * Check for a pending transaction before taking it, then recheck under lock,
 * so history never waits in front of the current trial's health decision. */
static bool updater_is_idle(void)
{
    struct stat st;
    if (lstat(HISTORY_UPDATER_TRANSACTION,&st)==-1 && errno==ENOENT) return true;
    errno=EBUSY;
    return false;
}

int history_index_upgrade_lock(void)
{
    if (!updater_is_idle()) return -1;
    int dir=open(HISTORY_UPDATER_LOCK_DIR,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (dir<0) return -1;
    int fd=openat(dir,"un260-updater.flock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    close(dir);
    if (fd<0) return -1;
    struct stat st;
    if (fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1 ||
        st.st_uid!=geteuid() || (st.st_mode&07777)!=0600) {
        close(fd); errno=EINVAL; return -1;
    }
    if (flock(fd,LOCK_EX|LOCK_NB) || !updater_is_idle()) {
        int error=errno;
        close(fd); errno=error; return -1;
    }
    return fd;
}

static bool copy_index(int source,int dest,off_t size)
{
    char buf[8192];
    for (off_t offset=0;offset<size;) {
        size_t n=(size_t)(size-offset);
        if (n>sizeof(buf)) n=sizeof(buf);
        if (pread(source,buf,n,offset)!=(ssize_t)n || write(dest,buf,n)!=(ssize_t)n) return false;
        offset+=(off_t)n;
    }
    return !fsync(dest) && same_index_bytes(source,dest);
}

static bool save_previous_index(int source,int objects,const struct stat *st)
{
    int prior=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (prior>=0) {
        struct stat old;
        bool valid=own_index(prior,&old);
        bool equal=valid && same_index_bytes(source,prior);
        close(prior);
        if (!valid) return false;
        if (equal) return fsync(objects)==0;
    } else if (errno!=ENOENT) return false;
    char scratch_name[64]=INDEX_COPY_TMP;
    int tmp=openat(objects,scratch_name,O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);
    if (tmp<0 && errno==EEXIST) {
        struct stat scratch;
        int old=openat(objects,scratch_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        bool reusable=own_index(old,&scratch) && index_name_current(objects,scratch_name,&scratch);
        if (old>=0) close(old);
        if (reusable && !unlinkat(objects,scratch_name,0))
            tmp=openat(objects,scratch_name,O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);
        else {
            /* An ambiguous short prefix, unrelated scratch or a link is not
             * ours to remove. This unique file has the same atomic rename. */
            char pattern[128];
            snprintf(pattern,sizeof(pattern),"/proc/self/fd/%d/.un260-index-write-XXXXXX",objects);
            tmp=mkstemp(pattern);
            if (tmp>=0) {
                (void)fcntl(tmp,F_SETFD,FD_CLOEXEC);
                snprintf(scratch_name,sizeof(scratch_name),"%s",strrchr(pattern,'/')+1);
            }
        }
    }
    if (tmp<0) return false;
    if (history_free_kb(objects)<HISTORY_INDEX_APP_RESERVE_KB+((uint64_t)st->st_size+1023)/1024+8) {
        close(tmp); (void)unlinkat(objects,scratch_name,0);return false;
    }
    /* UBIFS reports the available volume after data and metadata writeback.
     * Check that number too before lending any space from the app reserve. */
    bool ok=copy_index(source,tmp,st->st_size) && history_free_kb(objects)>=HISTORY_INDEX_APP_RESERVE_KB;
    struct stat scratch;
    bool ours=!fstat(tmp,&scratch);
    if (close(tmp)) ok=false;
    if (!ours || !index_name_current(objects,scratch_name,&scratch)) return false;
    if (!ok || renameat(objects,scratch_name,objects,HISTORY_INDEX_BACKUP)) {
        (void)unlinkat(objects,scratch_name,0);
        return false;
    }
    return fsync(objects)==0;
}

/* Drop the exchanged old inode only after exactly its bytes and the published
 * name are durable on their respective filesystems. Leave foreign names alone. */
static bool finish_index_exchange(int root,int backing)
{
    int displaced=openat(root,INDEX_SWITCH,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (displaced<0) return false;
    struct stat st;
    bool ok=own_index(displaced,&st) && same_index_bytes(displaced,backing) &&
        !fsync(root) && index_name_current(root,INDEX_SWITCH,&st) &&
        !unlinkat(root,INDEX_SWITCH,0) && !fsync(root);
    close(displaced);
    return ok;
}

bool history_release_index_headroom(const char *history_path)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (root<0) return false;
    int app=-1,objects=open_index_objects(root,&app,true,true),source=-1,backing=-1;
    bool released=false;
    char target[PATH_MAX];
    if (objects<0 || !history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,
                            HISTORY_INDEX_BACKUP,target,sizeof(target),true)) goto done;
    if (index_link_matches(root,"index.cfg",target)) {
        backing=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        released=backing>=0 && finish_index_exchange(root,backing);
        goto done;
    }
    source=openat(root,"index.cfg",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    struct stat old;
    if (!own_index(source,&old)) goto done;
    /* An old firmware can replace index.cfg after a cut while the old inode
     * is still under the switch name. Verify and retire that inode against
     * its own backup before replacing the backup with this newer index. */
    struct stat switch_stat;
    if (!fstatat(root,INDEX_SWITCH,&switch_stat,AT_SYMLINK_NOFOLLOW)) {
        if (S_ISREG(switch_stat.st_mode)) {
            backing=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if (backing<0 || !finish_index_exchange(root,backing)) goto done;
            close(backing); backing=-1;
        } else if (!index_link_matches(root,INDEX_SWITCH,target)) goto done;
    } else if (errno!=ENOENT) goto done;
    if (!save_previous_index(source,objects,&old)) goto done;
    backing=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (backing<0 || !same_index_bytes(source,backing) || !index_name_current(root,"index.cfg",&old)) goto done;
    if (!index_link_matches(root,INDEX_SWITCH,target) && symlinkat(target,root,INDEX_SWITCH)) goto done;
    if (renameat2(root,INDEX_SWITCH,root,"index.cfg",RENAME_EXCHANGE)) goto done;
    struct stat displaced;
    if (fstatat(root,INDEX_SWITCH,&displaced,AT_SYMLINK_NOFOLLOW) ||
        displaced.st_ino!=old.st_ino || displaced.st_dev!=old.st_dev) {
        (void)renameat2(root,INDEX_SWITCH,root,"index.cfg",RENAME_EXCHANGE);
        (void)fsync(root);
        goto done;
    }
    released=finish_index_exchange(root,backing);
done:
    if (backing>=0) close(backing);
    if (source>=0) close(source);
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    close(root);
    return released;
}

/* Repatriate the exact previously committed bytes. A cut after exchange
 * can reuse the displaced original inode. After its release, use the same
 * owned scratch policy as the normal writer and verify both names again. */
static bool restore_index(int root,int objects,int backing,const struct stat *backup,const char *target)
{
    int displaced=openat(root,INDEX_SWITCH,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (displaced>=0) {
        struct stat old;
        bool ok=own_index(displaced,&old) && same_index_bytes(displaced,backing) &&
            index_name_current(root,INDEX_SWITCH,&old) && index_link_matches(root,"index.cfg",target) &&
            index_name_current(objects,HISTORY_INDEX_BACKUP,backup) &&
            !renameat2(root,INDEX_SWITCH,root,"index.cfg",RENAME_EXCHANGE) && !fsync(root);
        close(displaced);
        return ok;
    }
    if (errno!=ENOENT) return false;
    char path[128];
    snprintf(path,sizeof(path),"/proc/self/fd/%d/index.cfg.tmp",root);
    int tmp=history_open_work_file(path,sizeof(path),"magic=1212765012\n");
    if (tmp<0) return false;
    const char *name=strrchr(path,'/')+1;
    bool copied=copy_index(backing,tmp,backup->st_size);
    struct stat st;
    bool owned=!fstat(tmp,&st) && index_name_current(root,name,&st);
    if (close(tmp)) copied=false;
    if (!owned) return false;
    if (!copied || !index_name_current(objects,HISTORY_INDEX_BACKUP,backup) ||
        !index_link_matches(root,"index.cfg",target) || renameat(root,name,root,"index.cfg")) {
        (void)unlinkat(root,name,0);
        return false;
    }
    return !fsync(root);
}

void history_finish_index_headroom(const char *history_path)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (root<0) return;
    int app=-1,objects=open_index_objects(root,&app,false,true),backing=-1;
    struct stat current,backup,swap;
    if (objects<0 || fstatat(root,"index.cfg",&current,AT_SYMLINK_NOFOLLOW)) goto done;
    backing=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (!own_index(backing,&backup)) goto done;
    if (S_ISLNK(current.st_mode)) {
        char target[PATH_MAX];
        if (!history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,
                HISTORY_INDEX_BACKUP,target,sizeof(target),false) ||
            !index_link_matches(root,"index.cfg",target) || !restore_index(root,objects,backing,&backup,target) ||
            fstatat(root,"index.cfg",&current,AT_SYMLINK_NOFOLLOW)) goto done;
    }
    if (!S_ISREG(current.st_mode) || current.st_uid!=geteuid() || current.st_nlink!=1 || fsync(root)) goto done;
    if (!fstatat(root,INDEX_SWITCH,&swap,AT_SYMLINK_NOFOLLOW)) {
        char target[PATH_MAX];
        if (S_ISLNK(swap.st_mode) && history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,
                                HISTORY_INDEX_BACKUP,target,sizeof(target),false) &&
            index_link_matches(root,INDEX_SWITCH,target)) {
            if (unlinkat(root,INDEX_SWITCH,0) || fsync(root)) goto done;
        } else if (!finish_index_exchange(root,backing)) goto done;
    } else if (errno!=ENOENT) goto done;
    if (index_name_current(root,"index.cfg",&current) && index_name_current(objects,HISTORY_INDEX_BACKUP,&backup) &&
        !unlinkat(objects,HISTORY_INDEX_BACKUP,0)) (void)fsync(objects);
done:
    if (backing>=0) close(backing);
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    close(root);
}


bool history_recovery_index_link(int root,const char *prefix)
{
    char expected[PATH_MAX];
    return history_object_target(root,prefix,HISTORY_INDEX_BACKUP,expected,sizeof(expected),false) &&
           index_link_matches(root,"index.cfg",expected);
}

bool history_has_index_recovery(const char *history_path)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (root<0) return false;
    struct stat named;
    char target[PATH_MAX];
    bool found=history_object_target(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS,
        HISTORY_INDEX_BACKUP,target,sizeof(target),false) &&
        !fstatat(root,target,&named,AT_SYMLINK_NOFOLLOW) && S_ISREG(named.st_mode);
    if (found) {
        int app=-1,objects=open_index_objects(root,&app,false,false);
        int fd=objects>=0 ? openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC) : -1;
        found=own_index(fd,&named);
        if (fd>=0) close(fd);
        if (objects>=0) close(objects);
        if (app>=0) close(app);
    }
    close(root);
    return found;
}

int history_read_recovery_index(const char *history_path)
{
    int root=open(history_path,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
    if (root<0) return -1;
    int app=-1,objects=open_index_objects(root,&app,false,false),fd=-1;
    struct stat st;
    if (objects>=0 && history_recovery_index_link(root,UI_HISTORY_APP_VOLUME "/" HISTORY_OBJECTS)) {
        fd=openat(objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if (fd>=0 && !own_index(fd,&st)) { close(fd); fd=-1; }
    }
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    close(root);
    if (fd<0) errno=EIO;  /* A broken reference is not a new empty store. */
    return fd;
}
