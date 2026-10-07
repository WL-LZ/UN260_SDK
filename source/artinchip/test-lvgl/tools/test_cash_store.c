#define _POSIX_C_SOURCE 200809L
#include "un260/storage/cash_journal_store.h"
#include "un260/storage/storage_worker.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
static int fail_directory_sync;
int __real_fsync(int);
int __wrap_fsync(int fd){struct stat st;if(fail_directory_sync&&fstat(fd,&st)==0&&S_ISDIR(st.st_mode)){fail_directory_sync=0;errno=EIO;return -1;}return __real_fsync(fd);}
static void drain(void){struct timespec wait={0,1000000};for(unsigned i=0;i<5000;++i){cash_journal_store_poll();cash_store_status_t s=cash_journal_store_status();if(s!=CASH_STORE_LOADING&&s!=CASH_STORE_SAVING)return;nanosleep(&wait,NULL);}assert(0);}
typedef struct {char code[4];uint32_t record;uint64_t recorded_at;int64_t amount;uint8_t count;cash_denom_t denoms[CASH_DENOMS];} old_snapshot_t;
typedef struct {uint32_t id,reverse_of,corrects,operator_id;uint64_t time;uint8_t kind;char operator_name[25],note[65];old_snapshot_t cash;} old_entry_t;
typedef struct {uint32_t version,count;old_entry_t entries[CASH_ENTRIES];} old_journal_t;
static void legacy_fixture(void){const cash_journal_t *j=cash_journal_store_get();old_journal_t *old=calloc(1,sizeof(*old));assert(old);old->version=1;old->count=j->count;for(unsigned i=0;i<j->count;++i){const cash_entry_t *a=&j->entries[i];old_entry_t *b=&old->entries[i];b->id=a->id;b->reverse_of=a->reverse_of;b->corrects=a->corrects;b->operator_id=a->operator_id;b->time=a->time;b->kind=a->kind;memcpy(b->operator_name,a->operator_name,25);memcpy(b->note,a->note,65);memcpy(b->cash.code,a->cash.code,4);b->cash.record=a->cash.record;b->cash.recorded_at=a->cash.recorded_at;b->cash.amount=a->cash.amount;b->cash.count=a->cash.count;memcpy(b->cash.denoms,a->cash.denoms,sizeof(b->cash.denoms));}uint32_t crc=~0U;const unsigned char *bytes=(const unsigned char *)old;for(size_t i=0;i<sizeof(*old);++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320U&-(crc&1U));}uint32_t header[]={0x434A3236U,1,sizeof(*old),~crc};FILE *f=fopen(UI_STATE_DIR "/cash_journal.v1","wb");assert(f&&fwrite(header,1,sizeof(header),f)==sizeof(header)&&fwrite(old,1,sizeof(*old),f)==sizeof(*old));assert(!fclose(f));free(old);}
int main(int argc,char **argv){assert(argc==2);cash_journal_store_init();drain();
 if(!strcmp(argv[1],"corrupt")){assert(cash_journal_store_status()==CASH_STORE_CORRUPT);assert(storage_worker_shutdown());puts("PASS corrupted journal is not replaced");return 0;}
 assert(cash_journal_store_status()==CASH_STORE_READY);
 if(!strcmp(argv[1],"legacy")){legacy_fixture();assert(storage_worker_shutdown());return 0;}
 if(!strcmp(argv[1],"read")){const cash_journal_t *j=cash_journal_store_get();assert(j->count==2&&cash_journal_balance(j,"USD")==150*CASH_SCALE);assert(storage_worker_shutdown());puts("PASS restart restores exact durable balance");return 0;}
 assert(cash_journal_store_get()->count==0);
 cash_entry_t e={.kind=CASH_OPENING,.time=1791244800,.cash={.code="USD",.amount=100*CASH_SCALE}};
 assert(cash_journal_store_submit(&e,0)==CASH_OK);assert(cash_journal_store_submit(&e,0)!=CASH_OK);drain();assert(cash_journal_store_status()==CASH_STORE_READY);
 e.kind=CASH_RECEIPT;e.cash.amount=50*CASH_SCALE;e.cash.record=12;e.cash.recorded_at=20261006090000ULL;fail_directory_sync=1;
 assert(cash_journal_store_submit(&e,0)==CASH_OK);drain();assert(cash_journal_store_status()==CASH_STORE_FAILED);
 assert(cash_journal_store_get()->count==1);assert(storage_worker_pending_status()==STORAGE_JOB_FAILED);
 assert(cash_journal_store_retry());drain();assert(cash_journal_store_status()==CASH_STORE_READY);assert(cash_journal_store_get()->count==2);
 assert(cash_journal_store_submit(&e,0)==CASH_DUPLICATE);assert(storage_worker_shutdown());puts("PASS async save, double submit, ambiguous rename/fsync failure, unpublished failed balance, idempotent retry, dedupe");return 0;
}
