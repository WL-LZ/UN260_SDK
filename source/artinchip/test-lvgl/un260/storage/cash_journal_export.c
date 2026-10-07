#include "cash_journal_export.h"
#include "cash_journal_store.h"
#include "storage_worker.h"
#include "usb_storage.h"
#include "un260/lv_components/ui_notice.h"
#include "un260/lv_system/ui_i18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#ifndef CASH_EXPORT_DIRECTORY
#define CASH_EXPORT_DIRECTORY USB_STORAGE_MOUNT_POINT
#endif
typedef struct {unsigned count;cash_entry_t entries[];} export_job_t;
static storage_job_id_t job;
static bool exported;
static void field(FILE *f,const char *s)
{
    fputc('"',f);
    /* Quoted CSV alone does not prevent spreadsheet formula interpretation. */
    if(strchr("=+-@\t\r\n",*s)&&*s)fputc('\'',f);
    for(;*s;++s){if(*s=='"')fputc('"',f);fputc(*s,f);}
    fputc('"',f);
}
static bool run(const void *payload,size_t size)
{
    const export_job_t *p=payload;exported=false;
    if(size!=sizeof(*p)+p->count*sizeof(cash_entry_t)||!usb_storage_prepare())return true;
    char path[192];int fd=-1;
    for(unsigned i=0;i<100&&fd<0;++i){snprintf(path,sizeof(path),CASH_EXPORT_DIRECTORY "/cash-journal-%llu-%u.csv",(unsigned long long)time(NULL),i);fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0644);}
    if(fd<0)return true;
    FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(path);return true;}
    fputs("\xEF\xBB\xBF" "ID,Time,Kind,Currency,Amount,Peer currency,Peer amount,Operator ID,Operator,Source count,Reverses,Corrects,Note\r\n",f);
    for(unsigned i=0;i<p->count;++i){const cash_entry_t *e=&p->entries[i];char a[40],b[40];cash_format(e->cash.amount,6,a,sizeof(a));cash_format(e->peer.amount,6,b,sizeof(b));
        fprintf(f,"%u,%llu,%u,%s,%s,%s,%s,%u,",e->id,(unsigned long long)e->time,e->kind,e->cash.code,a,e->peer.code,b,e->operator_id);
        field(f,e->operator_name);fprintf(f,",%u,%u,%u,",e->cash.record,e->reverse_of,e->corrects);field(f,e->note);fputs("\r\n",f);
    }
    bool ok=!ferror(f)&&fflush(f)==0&&fsync(fd)==0;if(fclose(f))ok=false;
    if(ok){int dir=open(CASH_EXPORT_DIRECTORY,O_RDONLY|O_DIRECTORY);ok=dir>=0&&fsync(dir)==0;if(dir>=0)close(dir);}
    if(!ok)unlink(path);exported=ok;
    /* Export failure must not block the durable journal storage lane. */
    return true;
}
bool cash_journal_export_start(const uint32_t *ids,unsigned count)
{
    const cash_journal_t *j=cash_journal_store_get();
    if(job||!j||!ids||!count||count>CASH_ENTRIES)return false;
    size_t size=sizeof(export_job_t)+count*sizeof(cash_entry_t);export_job_t *p=malloc(size);if(!p)return false;p->count=count;
    for(unsigned i=0;i<count;++i){if(!ids[i]||ids[i]>j->count){free(p);return false;}p->entries[i]=j->entries[ids[i]-1];}
    bool ok=storage_worker_submit(run,p,size,&job);free(p);return ok;
}
void cash_journal_export_poll(void)
{
    if(!job||storage_worker_status(job)!=STORAGE_JOB_SUCCEEDED)return;
    ui_notice_post_text(exported?UI_NOTICE_SUCCESS:UI_NOTICE_ERROR,"cash.export",UI_N_("Cash journal"),exported?UI_N_("Cash entries exported to USB."):UI_N_("Export failed. Check the USB drive and retry."));
    storage_worker_release(job);job=0;
}
