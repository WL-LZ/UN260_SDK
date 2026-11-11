#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <inttypes.h>
/* v2-v5 indexes own the historical records. v5 indexes include every field
 * of the former diagnostic slot mirrors. Only a matching mirror is disposable.
 * Immutable ABC data can live on the separately owned internal application
 * volume; its canonical name remains readable by previous firmware. */
#include <dirent.h>
#include <stdbool.h>

#define HISTORY_SLOTS 1000U
#define HISTORY_FIELDS 256U
#include "../un260/history/history_object_store.h"
#include "un260_storage_io.h"
#include "un260_history_space.h"

struct history_space_row {
    unsigned slot;
    uint32_t id;
    char detail[96];
    uint64_t hash;
    unsigned bytes;
    struct stat mirror;
    bool mirror_matches;
};

static int history_number(const char *text, unsigned *out)
{
    char *end;
    errno=0;
    unsigned long n=strtoul(text,&end,10);
    if (!*text || *text<'0' || *text>'9' || *end || errno || n>UINT32_MAX) return -1;
    *out=(unsigned)n;
    return 0;
}

static bool history_same_name(int dir,const char *name,const struct stat *original)
{
    struct stat current;
    return !fstatat(dir,name,&current,AT_SYMLINK_NOFOLLOW) && storage_same_regular_file(original,&current);
}

/* The committed index is normally a root file. The only allowed exception is
 * the exact recovery object on the owner-checked, different internal mount. */
struct history_index_check {
    struct stat identity;
    int objects;
    const char *prefix;
    bool recovered;
};

static bool history_index_current(int dir,const struct history_index_check *index)
{
    if (!index->recovered) return history_same_name(dir,"index.cfg",&index->identity);
    return index->objects>=0 && history_recovery_index_link(dir,index->prefix) &&
        history_same_name(index->objects,HISTORY_INDEX_BACKUP,&index->identity);
}

/* Use full field values; the index and old mirror write the same fields in
 * different orders. Unknown, truncated and stale mirrors are left untouched. */
static bool history_mirror_matches(int dir,struct history_space_row *row,char **fields,unsigned count)
{
    char name[32],prefix[32],line[8192];
    snprintf(name,sizeof(name),"%02u.rec",row->slot);
    snprintf(prefix,sizeof(prefix),"slot%02u_",row->slot);
    int fd=openat(dir,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if (fd<0) return false;
    if (fstat(fd,&row->mirror) || !S_ISREG(row->mirror.st_mode) || row->mirror.st_nlink!=1 ||
        row->mirror.st_uid!=geteuid() || row->mirror.st_size>65536) { close(fd); return false; }
    FILE *stream=fdopen(fd,"r");
    if (!stream) { close(fd); return false; }
    unsigned matches=0;
    bool seen[HISTORY_FIELDS]={false},ok=true;
    while (ok && fgets(line,sizeof(line),stream)) {
        size_t len=strlen(line),n=strlen(prefix);
        if (!len || line[len-1]!='\n' || strncmp(line,prefix,n)) { ok=false; break; }
        line[len-1]=0;
        unsigned i=0;
        while (i<count && strcmp(fields[i],line+n)) i++;
        if (i==count || seen[i]) { ok=false; break; }
        seen[i]=true;
        matches++;
    }
    if (ferror(stream)) ok=false;
    fclose(stream);
    return ok && matches==count && history_same_name(dir,name,&row->mirror);
}

static int history_finish_row(int dir,struct history_space_row *row,char **fields,unsigned count)
{
    if (!row->slot || row->slot>HISTORY_SLOTS || !row->id || !count) return -1;
    unsigned mask=0;
    for (unsigned i=0;i<count;i++) {
        const char *f=fields[i];
        unsigned value;
        if (!strcmp(f,"valid=1")) mask|=1;
        else if (!strcmp(f,"selected=0") || !strcmp(f,"selected=1")) mask|=2;
        else if (!strncmp(f,"pcs=",4) && !history_number(f+4,&value)) mask|=4;
        else if (!strncmp(f,"amount=",7) && !history_number(f+7,&value)) mask|=8;
        else if (!strncmp(f,"currency=",9) && strlen(f+9)<=3) mask|=16;
        else if (!strncmp(f,"time=",5) && strlen(f+5)==19) mask|=32;
    }
    if (mask!=63) return -1;
    row->mirror_matches=history_mirror_matches(dir,row,fields,count);
    if (row->bytes) snprintf(row->detail,sizeof(row->detail),"detail-%u-%016" PRIx64 ".abc",row->id,row->hash);
    return 0;
}

/* Read one bounded record at a time. No changes until the complete index,
 * all declared slots and the index inode have been checked. */
static int history_read_index(int dir,struct history_space_row *rows,unsigned *count,struct history_index_check *index)
{
    int fd=openat(dir,"index.cfg",O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if (fd<0 && errno==ELOOP && index->objects>=0 && history_recovery_index_link(dir,index->prefix)) {
        fd=openat(index->objects,HISTORY_INDEX_BACKUP,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        index->recovered=true;
    }
    if (fd<0) return -1;
    struct stat *identity=&index->identity;
    if (fstat(fd,identity) || !S_ISREG(identity->st_mode) || identity->st_uid!=geteuid() ||
        (index->recovered && identity->st_nlink!=1) || identity->st_size>32*1024*1024) { close(fd); return -1; }
    FILE *in=fdopen(fd,"r");
    if (!in) { close(fd); return -1; }
    char line[8192],*fields[HISTORY_FIELDS];
    unsigned nfields=0,declared=0,version=0,magic=0,loaded=0,capacity=0,next_id=0,next_slot=0;
    bool have_row=false;
    int result=-1;
    while (fgets(line,sizeof(line),in)) {
        size_t len=strlen(line);
        if (!len || line[len-1]!='\n') goto done;
        line[len-1]=0;
        char *eq=strchr(line,'=');
        if (!eq) goto done;
        if (strncmp(line,"record",6) || line[6]<'0' || line[6]>'9') {
            if (have_row) goto done;
            *eq++=0;
            unsigned number;
            if (history_number(eq,&number)) goto done;
            if (!strcmp(line,"record_count")) declared=number;
            else if (!strcmp(line,"version")) version=number;
            else if (!strcmp(line,"magic")) magic=number;
            else if (!strcmp(line,"capacity")) capacity=number;
            else if (!strcmp(line,"next_record_no")) next_id=number;
            else if (!strcmp(line,"next_slot_no")) next_slot=number;
            else if (strcmp(line,"total_notes_counted")) goto done;
            continue;
        }
        char *field=strchr(line,'_');
        if (!field || field>=eq) goto done;
        *field++=0;
        unsigned index;
        if (history_number(line+6,&index) || index>=HISTORY_SLOTS) goto done;
        if (index!=loaded) {
            if (!have_row || index!=loaded+1 || history_finish_row(dir,&rows[loaded],fields,nfields)) goto done;
            for (unsigned i=0;i<nfields;i++) free(fields[i]);
            nfields=0;
            loaded++;
        }
        have_row=true;
        if (nfields>=HISTORY_FIELDS) goto done;
        for (unsigned i=0;i<nfields;i++) {
            size_t key=(size_t)(eq-field)+1;
            if (!strncmp(fields[i],field,key)) goto done;
        }
        fields[nfields]=strdup(field);
        if (!fields[nfields]) goto done;
        nfields++;
        struct history_space_row *row=&rows[index];
        if (!strncmp(field,"slot_no=",8)) {
            if (history_number(eq+1,&row->slot)) goto done;
        } else if (!strncmp(field,"record_no=",10)) {
            if (history_number(eq+1,&row->id)) goto done;
        } else if (!strncmp(field,"detail=",7)) {
            char *v=eq+1;
            if (strlen(v)<18 || v[16]!=',' || !storage_is_lower_hex(v,16)) goto done;
            v[16]=0;
            row->hash=strtoull(v,NULL,16);
            if (history_number(v+17,&row->bytes) || row->bytes>800152) goto done;
        }
    }
    if (ferror(in) || magic!=0x48495354 || version!=5 || !next_id || !next_slot ||
        next_slot>HISTORY_SLOTS || capacity<100 || capacity>HISTORY_SLOTS || declared>capacity) goto done;
    if (have_row) {
        if (history_finish_row(dir,&rows[loaded],fields,nfields)) goto done;
        loaded++;
    }
    if (loaded!=declared) goto done;
    for (unsigned i=0;i<loaded;i++) {
        if (rows[i].id>=next_id) goto done;
        for (unsigned j=0;j<i;j++)
            if (rows[i].id==rows[j].id || rows[i].slot==rows[j].slot) goto done;
    }
    if (!history_index_current(dir,index)) goto done;
    *count=loaded;
    result=0;
done:
    for (unsigned i=0;i<nfields;i++) free(fields[i]);
    fclose(in);
    return result;
}

/* Compare the persisted bytes with the still-open original, including the
 * hash from the immutable ABC name. No old pathname is changed on mismatch. */
static bool history_object_equal(int source,int object,const struct history_space_row *row)
{
    unsigned char a[16384],b[sizeof(a)];
    struct stat st;
    if (fstat(object,&st) || !S_ISREG(st.st_mode) || st.st_nlink!=1 ||
        st.st_uid!=geteuid() || st.st_size!=row->bytes) return false;
    uint64_t hash=UINT64_C(14695981039346656037);
    for (off_t at=0;at<st.st_size;) {
        size_t n=(size_t)(st.st_size-at);
        if (n>sizeof(a)) n=sizeof(a);
        if (pread(source,a,n,at)!=(ssize_t)n || pread(object,b,n,at)!=(ssize_t)n || memcmp(a,b,n)) return false;
        if (at==0 && (n<32 || memcmp(a,"ABC1\1\0\0\0",8))) return false;
        for (size_t i=0;i<n;i++) hash=(hash^b[i])*UINT64_C(1099511628211);
        at+=(off_t)n;
    }
    return hash==row->hash;
}

static bool history_link_points_to(int dir,const char *name,const char *target)
{
    char path[PATH_MAX];
    ssize_t n=readlinkat(dir,name,path,sizeof(path));
    return n==(ssize_t)strlen(target) && !memcmp(path,target,(size_t)n);
}

static void history_sweep_objects(int dir,int objects,const char *prefix,
                                 const struct history_space_row *rows,unsigned count,const struct history_index_check *index)
{
    int duplicate=fcntl(objects,F_DUPFD_CLOEXEC,3);
    DIR *stream=duplicate<0?NULL:fdopendir(duplicate);
    if (!stream) { if (duplicate>=0) close(duplicate); return; }
    struct dirent *entry;
    bool changed=false;
    while ((entry=readdir(stream))) {
        const char *name=entry->d_name;
        unsigned id;
        uint64_t hash;
        int used=0;
        if (sscanf(name,"detail-%u-%16" SCNx64 ".abc%n",&id,&hash,&used)!=2 || !id || used<=0 || name[used]) continue;
        char canon[96],target[PATH_MAX];
        snprintf(canon,sizeof(canon),"detail-%u-%016" PRIx64 ".abc",id,hash);
        if (strcmp(name,canon) || !history_object_target(dir,prefix,name,target,sizeof(target),false)) continue;
        bool retained=history_link_points_to(dir,name,target);
        for (unsigned i=0;i<count && !retained;i++) retained=!strcmp(rows[i].detail,name);
        if (retained) continue;
        struct stat st;
        if (fstatat(objects,name,&st,AT_SYMLINK_NOFOLLOW) || !S_ISREG(st.st_mode) ||
            st.st_uid!=geteuid() || st.st_nlink!=1) continue;
        if (!history_index_current(dir,index)) break;
        if (history_same_name(objects,name,&st) && !unlinkat(objects,name,0)) changed=true;
    }
    closedir(stream);
    if (changed) (void)fsync(objects);
}

static bool history_publish_object(int dir,int objects,const char *prefix,const struct history_space_row *row,
                                   const struct history_index_check *index,uint64_t app_floor)
{
    char target[PATH_MAX],swap[128];
    if (!history_object_target(dir,prefix,row->detail,target,sizeof(target),true)) return false;
    snprintf(swap,sizeof(swap),".un260-swap-%s",row->detail);
    struct stat st;
    int object=openat(objects,row->detail,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    if (history_link_points_to(dir,row->detail,target)) {
        /* Recover a power cut after publication and before old-inode cleanup. */
        int old=openat(dir,swap,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
        if (old>=0) {
            if (object>=0 && !fstat(old,&st) && S_ISREG(st.st_mode) && st.st_size==row->bytes &&
                history_object_equal(old,object,row) && history_same_name(dir,swap,&st) &&
                !fsync(dir) && !unlinkat(dir,swap,0)) (void)fsync(dir);
            close(old);
        }
        if (object>=0) close(object);
        return false;
    }
    int source=openat(dir,row->detail,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
    bool ok=false;
    if (source<0 || fstat(source,&st) || !S_ISREG(st.st_mode) || st.st_uid!=geteuid() ||
        st.st_size!=row->bytes || st.st_nlink!=1) goto done;
    if (object<0) {
        if (history_free_kb(objects)<app_floor+(row->bytes+1023U)/1024U+8) goto done;
        /* Directory is private and locked. This one scratch name cannot hold
         * the only committed copy: publication always renames it first. */
        struct stat scratch;
        if (!fstatat(objects,".incoming",&scratch,AT_SYMLINK_NOFOLLOW)) {
            if (!S_ISREG(scratch.st_mode) || scratch.st_uid!=geteuid() || scratch.st_nlink!=1 ||
                unlinkat(objects,".incoming",0)) goto done;
        } else if (errno!=ENOENT) goto done;
        int tmp=openat(objects,".incoming",O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC,0600);
        if (tmp<0) goto done;
        char buf[16384];
        ssize_t n;
        while ((n=read(source,buf,sizeof(buf)))>0 && !storage_write_all(tmp,buf,(size_t)n)) {}
        bool copied=n==0 && !fsync(tmp) && history_object_equal(source,tmp,row);
        if (close(tmp) || !copied || renameat2(objects,".incoming",objects,row->detail,RENAME_NOREPLACE) ||
            fsync(objects)) goto done;
        object=openat(objects,row->detail,O_RDONLY|O_NONBLOCK|O_NOFOLLOW|O_CLOEXEC);
    }
    if (object<0 || !history_object_equal(source,object,row) || fsync(objects) ||
        !history_index_current(dir,index) || !history_same_name(dir,row->detail,&st)) goto done;
    if (!history_link_points_to(dir,swap,target) && symlinkat(target,dir,swap)) goto done;
    /* EXCHANGE requires the source still to exist. If interrupted, both
     * names survive; a later run discards the byte-verified old inode. */
    if (renameat2(dir,swap,dir,row->detail,RENAME_EXCHANGE)) goto done;
    struct stat exchanged;
    if (fstatat(dir,swap,&exchanged,AT_SYMLINK_NOFOLLOW) ||
        exchanged.st_dev!=st.st_dev || exchanged.st_ino!=st.st_ino) {
        (void)renameat2(dir,swap,dir,row->detail,RENAME_EXCHANGE);
        (void)fsync(dir);
        goto done;
    }
    if (!fsync(dir) && !unlinkat(dir,swap,0) && !fsync(dir)) ok=true;
done:
    if (object>=0) close(object);
    if (source>=0) close(source);
    return ok;
}

int prepare_history_space(const char *path,const char *app_path,const char *root_text,const char *app_text)
{
    unsigned root_goal,app_need,count=0,mirrors=0,moved=0;
    if (history_number(root_text,&root_goal) || history_number(app_text,&app_need)) return 1;
    int dir=storage_open_absolute(path,1);
    if (dir<0) return errno==ENOENT?0:1;
    struct history_space_row *rows=calloc(HISTORY_SLOTS,sizeof(*rows));
    int app=storage_open_absolute(app_path,1),objects=-1;
    char prefix[PATH_MAX];
    struct stat root_stat,app_stat;
    bool separate=app>=0 && !fstat(dir,&root_stat) && !fstat(app,&app_stat) && root_stat.st_dev!=app_stat.st_dev &&
        history_owned_marker(app,".un260-owner","UN260_APP_VOLUME_V1\n") &&
        snprintf(prefix,sizeof(prefix),"%s/%s",app_path,HISTORY_OBJECTS)<(int)sizeof(prefix);
    struct stat named;
    bool linked=!fstatat(dir,"index.cfg",&named,AT_SYMLINK_NOFOLLOW) && S_ISLNK(named.st_mode);
    if (linked && separate) objects=history_objects_open(app);
    struct history_index_check index={.objects=objects,.prefix=prefix};
    if (!rows || history_read_index(dir,rows,&count,&index)) { errno=EINVAL; goto fail; }
    if (!linked && separate) objects=history_objects_open(app);
    if (objects>=0) history_sweep_objects(dir,objects,prefix,rows,count,&index);
    for (unsigned i=0;i<count && history_free_kb(dir)<root_goal;i++) {
        struct history_space_row *r=&rows[i];
        char name[32];
        snprintf(name,sizeof(name),"%02u.rec",r->slot);
        if (!history_index_current(dir,&index)) break;
        if (r->mirror_matches && history_same_name(dir,name,&r->mirror) && !unlinkat(dir,name,0)) mirrors++;
    }
    if (mirrors && fsync(dir)) goto fail;
    unsigned app_floor=app_need>HISTORY_APP_FLOOR_KB?app_need:HISTORY_APP_FLOOR_KB;
    if (objects>=0) {
        for (unsigned i=0;i<count;i++) {
            struct history_space_row *r=&rows[i];
            if (!r->bytes) continue;
            if (history_free_kb(dir)>=root_goal) break;
            if (history_publish_object(dir,objects,prefix,r,&index,app_floor)) moved++;
        }
    }
    printf("History space: %u records, %u matched diagnostic mirrors retired, %u immutable detail objects placed; root free %" PRIu64 " KiB\n",
           count,mirrors,moved,history_free_kb(dir));
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    free(rows); close(dir);
    return 0;
fail:
    if (objects>=0) close(objects);
    if (app>=0) close(app);
    free(rows); close(dir);
    return 1;
}
