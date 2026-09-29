#include "un260/lv_system/ui_i18n.h"
#include "un260/lv_system/ui_message.h"
#include "cashbook_store.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <dirent.h>
#include "usb_storage.h"
#ifndef CASHBOOK_DIRECTORY
#define CASHBOOK_DIRECTORY "/etc/ui_state"
#endif
#define JOURNAL CASHBOOK_DIRECTORY "/cashbook.v1"
#define MAGIC 0x31424355U
#define CHECKPOINT 0x32424355U
#ifndef CASHBOOK_USB_DIRECTORY
#define CASHBOOK_USB_DIRECTORY USB_STORAGE_MOUNT_POINT
#endif
typedef struct {uint32_t magic,version,size,crc;uint64_t time;} header_t;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_t thread;
static cashbook_t *published,*draft;
static cashbook_t empty;
static cashbook_command_t command,failed_command;
static uint32_t failed_sequence;
static bool rejected;
static bool initialized,busy,complete,success,ready,loading,fatal;
typedef enum {JOB_COMMAND,JOB_ARCHIVE,JOB_SCAN,JOB_VIEW,JOB_EXPORT} job_t;
static job_t job;
static cashbook_t *archive_view,*view_draft;
static cashbook_archive_t archives[CASHBOOK_ARCHIVES],archive_draft[CASHBOOK_ARCHIVES];
static unsigned archive_count,archive_draft_count;
static uint32_t archive_id;
static char message[320],result[320];
static ui_message_t message_info,result_info;
static uint32_t crc(const void *data,size_t n)
{const unsigned char *p=data;uint32_t c=~0U;while(n--){c^=*p++;for(unsigned i=0;i<8;i++)c=(c>>1)^(0xEDB88320U&-(c&1));}return ~c;}
static bool io(int fd,void *data,size_t n,bool write_it)
{char *p=data;while(n){ssize_t r=write_it?write(fd,p,n):read(fd,p,n);if(r<0&&errno==EINTR)continue;if(r<=0)return false;p+=r;n-=r;}return true;}
static bool load(void)
{
    cashbook_defaults(draft);
    int fd=open(JOURNAL,O_RDWR|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0)return errno==ENOENT;
    struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)){close(fd);return false;}
    off_t valid=0;bool ok=true;cashbook_command_t entry;
    while(valid<st.st_size){
        header_t h;
        if(st.st_size-valid<(off_t)sizeof(h))break;
        if(!io(fd,&h,sizeof(h),false)){ok=false;break;}
        if(valid==0&&h.magic==CHECKPOINT&&h.version==1&&h.size==sizeof(*draft)){
            if(st.st_size<(off_t)(sizeof(h)+sizeof(*draft))||!io(fd,draft,sizeof(*draft),false)||h.crc!=crc(draft,sizeof(*draft))||!cashbook_valid(draft)){ok=false;break;}
            valid=sizeof(h)+sizeof(*draft);continue;
        }
        if(h.magic!=MAGIC||h.version!=1||h.size!=sizeof(entry)){ok=false;break;}
        if(st.st_size-valid<(off_t)(sizeof(h)+sizeof(entry)))break;
        if(!io(fd,&entry,sizeof(entry),false)||h.crc!=crc(&entry,sizeof(entry))||!cashbook_apply(draft,&entry,result,sizeof(result))){ok=false;break;}
        valid+=sizeof(h)+sizeof(entry);
    }
    /* Only an incomplete final write is recoverable. A complete corrupt frame is not discarded. */
    if(ok&&valid<st.st_size)ok=!ftruncate(fd,valid)&&!fsync(fd);
    if(close(fd))ok=false;
    return ok&&cashbook_valid(draft);
}
static bool append(void)
{
    if(mkdir(CASHBOOK_DIRECTORY,0755)&&errno!=EEXIST)return false;
    int dir=open(CASHBOOK_DIRECTORY,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return false;
    int fd=open(JOURNAL,O_WRONLY|O_APPEND|O_CREAT|O_NOFOLLOW|O_NONBLOCK,0600);
    if(fd<0){close(dir);return false;}
    struct stat st;bool ok=!fstat(fd,&st)&&S_ISREG(st.st_mode);
    header_t h={.magic=MAGIC,.version=1,.size=sizeof(command),.crc=crc(&command,sizeof(command)),.time=(uint64_t)time(NULL)};
    if(ok){
        /* From the first write onward, a failure has uncertain durability. Fail closed. */
        fatal=true;ok=io(fd,&h,sizeof(h),true)&&io(fd,&command,sizeof(command),true)&&!fsync(fd)&&!fsync(dir);
    }
    if(close(fd))ok=false;
    close(dir);
    if(ok)fatal=false;
    return ok;
}
#include "cashbook_archives.inc"
static void *work(void *unused)
{
    (void)unused;bool ok;
    if(loading){ok=load();ui_message_key(&result_info,ok?UI_N_("Records ready."):UI_N_("Records could not be recovered. Original journal retained; counting history is unchanged."));}
    else if(job==JOB_ARCHIVE)ok=archive_period();
    else if(job==JOB_SCAN)ok=scan_archives();
    else if(job==JOB_VIEW)ok=read_archive();
    else if(job==JOB_EXPORT)ok=export_view();
    else{
        *draft=*published;
        ok=cashbook_apply(draft,&command,result,sizeof(result));ui_message_key(&result_info,result);
        if(ok&&draft->sequence!=published->sequence){
            ok=append();
            if(!ok)ui_message_key(&result_info,UI_N_("Record save failed. Restart to verify storage before changing totals."));
        }
    }
    pthread_mutex_lock(&mutex);success=ok;complete=true;pthread_mutex_unlock(&mutex);return NULL;
}
static bool begin(void)
{
    complete=false;busy=true;
    if(pthread_create(&thread,NULL,work,NULL)){busy=false;ui_message_key(&message_info,UI_N_("Record worker could not start."));return false;}
    return true;
}
void cashbook_store_init(void)
{
    if(initialized)return;
    initialized=true;cashbook_defaults(&empty);
    published=calloc(1,sizeof(*published));draft=calloc(1,sizeof(*draft));
    if(!published||!draft){free(published);free(draft);published=draft=NULL;ui_message_key(&message_info,UI_N_("Not enough memory to open records."));return;}
    cashbook_defaults(published);loading=true;ui_message_key(&message_info,UI_N_("Loading records..."));begin();
}
bool cashbook_store_poll(void)
{
    if(!busy)return false;
    pthread_mutex_lock(&mutex);bool done=complete;pthread_mutex_unlock(&mutex);if(!done)return false;
    pthread_join(thread,NULL);busy=false;
    if(success&&(loading||job==JOB_COMMAND||job==JOB_ARCHIVE)){cashbook_t *old=published;published=draft;draft=old;ready=true;}
    if(success&&job==JOB_SCAN){memcpy(archives,archive_draft,sizeof(archives));archive_count=archive_draft_count;}
    if(job==JOB_VIEW){if(success){free(archive_view);archive_view=view_draft;view_draft=NULL;}else{free(view_draft);view_draft=NULL;}}
    if(!success&&(loading||fatal))ready=false;
    if(!loading&&job==JOB_COMMAND){rejected=!success;if(rejected){failed_command=command;failed_sequence=published->sequence;}}
    loading=false;message_info=result_info;return true;
}
bool cashbook_store_ready(void){return ready;}
bool cashbook_store_busy(void){return busy;}
bool cashbook_store_last_success(void){return success;}
const cashbook_t *cashbook_store_get(void){return published?published:&empty;}
const char *cashbook_store_message(void){ui_message_render(&message_info,message,sizeof(message));return message;}
const ui_message_t *cashbook_store_message_info(void){return &message_info;}
bool cashbook_store_submit(const cashbook_command_t *c)
{
    if(!c||!ready||busy)return false;
    if(rejected&&failed_sequence==published->sequence&&!memcmp(c,&failed_command,sizeof(*c)))return false;
    command=*c;job=JOB_COMMAND;fatal=false;ui_message_key(&message_info,UI_N_("Saving record..."));return begin();
}
const cashbook_t *cashbook_store_view(void){return archive_view?archive_view:cashbook_store_get();}
bool cashbook_store_view_archived(void){return archive_view!=NULL;}
void cashbook_store_close_view(void){if(!busy){free(archive_view);archive_view=NULL;}}
unsigned cashbook_store_archives(const cashbook_archive_t **items){if(items)*items=archives;return archive_count;}
static bool start_job(job_t next,const char *status)
{if(!ready||busy)return false;job=next;fatal=false;ui_message_key(&message_info,status);return begin();}
bool cashbook_store_scan_archives(void){return start_job(JOB_SCAN,UI_N_("Looking for archived periods..."));}
bool cashbook_store_open_archive(uint32_t id)
{
    if(!ready||busy)return false;
    bool found=false;
    for(unsigned i=0;i<archive_count;i++)if(archives[i].id==id)found=true;
    if(!found)return false;
    view_draft=malloc(sizeof(*view_draft));if(!view_draft)return false;archive_id=id;
    if(start_job(JOB_VIEW,UI_N_("Opening archived records...")))return true;
    free(view_draft);view_draft=NULL;return false;
}
bool cashbook_store_archive(void){if(archive_view)return false;return start_job(JOB_ARCHIVE,UI_N_("Archiving closed period..."));}
bool cashbook_store_export(void){return start_job(JOB_EXPORT,UI_N_("Exporting record summary to USB..."));}
