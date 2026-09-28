#define _GNU_SOURCE
#include "un260/storage/cashbook_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <errno.h>
static atomic_bool fail_sync;
int __real_fsync(int);
int __wrap_fsync(int fd){if(atomic_exchange(&fail_sync,false)){errno=EIO;return -1;}return __real_fsync(fd);}
bool usb_storage_prepare(void){return true;}
static void wait_job(void)
{for(unsigned i=0;i<10000&&cashbook_store_busy();i++){cashbook_store_poll();usleep(1000);}assert(!cashbook_store_busy());}
static void send(cashbook_command_t c)
{assert(cashbook_store_submit(&c));wait_job();}
int main(int argc,char **argv)
{
    cashbook_store_init();wait_job();
    if(argc>1&&!strcmp(argv[1],"corrupt")){assert(!cashbook_store_ready());puts("PASS corrupt journal fails closed");return 0;}
    assert(cashbook_store_ready());
    if(argc>1&&!strcmp(argv[1],"reload")){assert(cashbook_store_get()->run_count==1&&cashbook_store_get()->runs[0].result.source==2);puts("PASS checkpoint plus journal replay");return 0;}
    if(argc>1&&!strcmp(argv[1],"sync-failure")){
        atomic_store(&fail_sync,true);send((cashbook_command_t){.operation=CASHBOOK_SET_DAY,.day=20260925});
        assert(!cashbook_store_ready());assert(cashbook_store_get()->business_day!=20260925);puts("PASS uncertain durability never published");return 0;
    }
    cashbook_result_t r={.source=1,.day=20260923,.operator_name="Do not export names",.pcs=100,.currencies=1,.complete=true,.money={{.code="CNY",.pcs=100,.amount=10000}}};
    send((cashbook_command_t){.operation=CASHBOOK_INGEST,.result=r});assert(cashbook_store_get()->run_count==1);
    assert(cashbook_store_archive());wait_job();assert(cashbook_store_get()->run_count==1&&cashbook_store_ready());
    send((cashbook_command_t){.operation=CASHBOOK_SELECT,.group=1,.run=1});
    send((cashbook_command_t){.operation=CASHBOOK_CLOSE,.day=20260923});
    assert(cashbook_store_export());wait_job();assert(strstr(cashbook_store_message(),"Saved to USB"));
    assert(cashbook_store_archive());wait_job();assert(cashbook_store_get()->run_count==0&&cashbook_store_ready());
    assert(cashbook_store_scan_archives());wait_job();const cashbook_archive_t *items;assert(cashbook_store_archives(&items)==1);
    assert(cashbook_store_open_archive(items[0].id));wait_job();assert(cashbook_store_view_archived()&&cashbook_store_view()->run_count==1&&cashbook_store_get()->run_count==0);
    r.source=2;r.day=20260924;send((cashbook_command_t){.operation=CASHBOOK_INGEST,.result=r});
    assert(cashbook_store_get()->runs[0].result.source==2&&cashbook_store_view()->runs[0].result.source==1);
    cashbook_store_close_view();assert(!cashbook_store_view_archived());
    puts("PASS durable ledger: worker publication, pending archive guard, export, archive checkpoint, read-only archive independent of live ingestion");
}
