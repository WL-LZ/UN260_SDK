#define _POSIX_C_SOURCE 200809L
#include "cash_journal_store.h"
#include "storage_worker.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#ifndef UI_STATE_DIR
#define UI_STATE_DIR "/etc/ui_state"
#endif
#define JOURNAL_FILE UI_STATE_DIR "/cash_journal.v1"
#define JOURNAL_TEMP UI_STATE_DIR "/cash_journal.v1.tmp"
typedef struct { uint32_t magic,version,size,crc; } file_header_t;
static cash_journal_t *published,*loaded;
static storage_job_id_t job;
static cash_store_status_t state=CASH_STORE_LOADING;
static bool initiated,load_job,load_ok;
static unsigned revision;
static uint32_t checksum(const void *data,size_t size)
{
    uint32_t crc=~0U;const unsigned char *p=data;
    while(size--){crc^=*p++;for(unsigned b=0;b<8;++b)crc=(crc>>1)^(0xEDB88320U&-(crc&1U));}
    return ~crc;
}
static bool transfer(int fd,void *data,size_t size,bool writing)
{
    unsigned char *p=data;
    while(size){ssize_t n=writing?write(fd,p,size):read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=(size_t)n;}
    return true;
}
/* Decode the original wire layout explicitly; never overwrite a failed migration. */
typedef struct {char code[4];uint32_t record;uint64_t recorded_at;int64_t amount;uint8_t count;cash_denom_t denoms[CASH_DENOMS];} snapshot_v1_t;
typedef struct {uint32_t id,reverse_of,corrects,operator_id;uint64_t time;uint8_t kind;char operator_name[25],note[65];snapshot_v1_t cash;} entry_v1_t;
typedef struct {uint32_t version,count;entry_v1_t entries[CASH_ENTRIES];} journal_v1_t;
static bool load(const void *unused,size_t bytes)
{
    (void)unused;(void)bytes;load_ok=false;
    int fd=open(JOURNAL_FILE,O_RDONLY);
    if(fd<0){if(errno==ENOENT){loaded->version=2;load_ok=true;}return true;}
    file_header_t h;struct stat st;
    if(fstat(fd,&st)==0&&transfer(fd,&h,sizeof(h),false)&&h.magic==0x434A3236U&&st.st_size==(off_t)(sizeof(h)+h.size)) {
        if(h.version==2&&h.size==sizeof(*loaded)&&transfer(fd,loaded,sizeof(*loaded),false)&&h.crc==checksum(loaded,sizeof(*loaded))&&cash_journal_valid(loaded))load_ok=true;
        else if(h.version==1&&h.size==sizeof(journal_v1_t)) {
            journal_v1_t *old=malloc(sizeof(*old));
            if(old&&transfer(fd,old,sizeof(*old),false)&&h.crc==checksum(old,sizeof(*old))&&old->version==1&&old->count<=CASH_ENTRIES){
                memset(loaded,0,sizeof(*loaded));loaded->version=2;loaded->count=old->count;
                for(unsigned i=0;i<old->count;++i){const entry_v1_t *a=&old->entries[i];cash_entry_t *b=&loaded->entries[i];
                    b->id=a->id;b->reverse_of=a->reverse_of;b->corrects=a->corrects;b->operator_id=a->operator_id;b->time=a->time;b->kind=a->kind;
                    memcpy(b->operator_name,a->operator_name,sizeof(b->operator_name));memcpy(b->note,a->note,sizeof(b->note));
                    memcpy(b->cash.code,a->cash.code,4);b->cash.record=a->cash.record;b->cash.recorded_at=a->cash.recorded_at;b->cash.amount=a->cash.amount;b->cash.count=a->cash.count;
                    memcpy(b->cash.denoms,a->cash.denoms,sizeof(b->cash.denoms));b->cash.original_amount=a->cash.amount;
                }
                load_ok=cash_journal_valid(loaded);
            }free(old);
        }
    }
    close(fd);return true; /* A bad read is fail-closed, not a retry-blocked FIFO. */
}
static bool save(const void *snapshot,size_t size)
{
    if(size!=sizeof(cash_journal_t)||!cash_journal_valid(snapshot))return false;
    if(mkdir(UI_STATE_DIR,0700)!=0&&errno!=EEXIST)return false;
    int fd=open(JOURNAL_TEMP,O_WRONLY|O_CREAT|O_TRUNC,0600);if(fd<0)return false;
    file_header_t h={0x434A3236U,2,(uint32_t)size,checksum(snapshot,size)};
    bool ok=transfer(fd,&h,sizeof(h),true)&&transfer(fd,(void *)snapshot,size,true)&&fsync(fd)==0;
    if(close(fd)!=0)ok=false;
    if(!ok)return false;
    if(rename(JOURNAL_TEMP,JOURNAL_FILE)!=0)return false;
    fd=open(UI_STATE_DIR,O_RDONLY|O_DIRECTORY);if(fd<0)return false;
    ok=fsync(fd)==0;close(fd);return ok;
}
void cash_journal_store_init(void)
{
    if(initiated)return;
    if(!published)published=calloc(1,sizeof(*published));
    if(!loaded)loaded=calloc(1,sizeof(*loaded));
    if(!published||!loaded){state=CASH_STORE_CORRUPT;initiated=true;return;}
    unsigned char token=0;
    if(storage_worker_init()&&storage_worker_submit(load,&token,sizeof(token),&job)){initiated=true;load_job=true;state=CASH_STORE_LOADING;}
}
bool cash_journal_store_poll(void)
{
    if(!initiated){cash_journal_store_init();return false;}
    if(!job)return false;
    storage_job_status_t status=storage_worker_status(job);
    if(status==STORAGE_JOB_PENDING)return false;
    if(status!=STORAGE_JOB_SUCCEEDED){if(state==CASH_STORE_FAILED)return false;state=CASH_STORE_FAILED;return true;}
    if(load_job){if(load_ok){*published=*loaded;state=CASH_STORE_READY;}else state=CASH_STORE_CORRUPT;free(loaded);loaded=NULL;load_job=false;}
    else if(storage_worker_copy_completed(job,published,sizeof(*published)))state=CASH_STORE_READY;
    else state=CASH_STORE_CORRUPT;
    storage_worker_release(job);job=0;++revision;return true;
}
cash_store_status_t cash_journal_store_status(void){return state;}
const cash_journal_t *cash_journal_store_get(void){return published;}
unsigned cash_journal_store_revision(void){return revision;}
cash_error_t cash_journal_store_submit(const cash_entry_t *e,uint32_t correction)
{
    if(state!=CASH_STORE_READY||job||!published)return CASH_INVALID;
    cash_journal_t *draft=malloc(sizeof(*draft));if(!draft)return CASH_INVALID;*draft=*published;
    cash_error_t error=correction?cash_journal_correct(draft,correction,e,e->time):cash_journal_append(draft,e);
    if(error==CASH_OK){if(storage_worker_submit(save,draft,sizeof(*draft),&job))state=CASH_STORE_SAVING;else error=CASH_INVALID;}
    free(draft);return error;
}
bool cash_journal_store_retry(void)
{
    if(state!=CASH_STORE_FAILED||!job)return false;
    if(!storage_worker_retry(job))return false;
    state=load_job?CASH_STORE_LOADING:CASH_STORE_SAVING;return true;
}
