#include "workspace_store.h"
#include "usb_storage.h"
#include <pthread.h>
#include <png.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef WORKSPACE_DIRECTORY
#define WORKSPACE_DIRECTORY "/etc/ui_state"
#endif
#ifndef WORKSPACE_USB_DIRECTORY
#define WORKSPACE_USB_DIRECTORY USB_STORAGE_MOUNT_POINT
#endif
#define STORE WORKSPACE_DIRECTORY "/workspace.v1"
typedef enum { LOAD, SAVE, SCAN, IMPORT, SUPPORT } operation_t;
static char support_report[8192];
static workspace_model_t saved, transaction;
static workspace_avatar_t imported, preview;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_t worker;
static bool initialized,ready,busy,done,ok,uncertain_save;
static operation_t operation;
static unsigned source,revision;
static uint32_t found,images;
static char message[160],result[160];
static uint32_t checksum(const void *data,size_t size)
{
    const uint8_t *p=data;uint32_t crc=0xFFFFFFFFU;
    while(size--) {crc^=*p++;for(unsigned i=0;i<8;i++)crc=(crc>>1)^(0xEDB88320U&-(crc&1));}
    return ~crc;
}
static bool full_io(int fd,void *data,size_t bytes,bool writing)
{
    uint8_t *p=data;
    while(bytes) {ssize_t n=writing?write(fd,p,bytes):read(fd,p,bytes);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;bytes-=(size_t)n;}
    return true;
}
/* v1 is decoded explicitly; existing operator IDs, photos and rules are retained. */
typedef struct {uint32_t id;char name[25];uint8_t speed,work,add,sort,beep,batch_enabled,batch;} profile_v1_t;
typedef struct {uint32_t id;char name[25];uint8_t quick_enabled,profile_count,batch_count;uint8_t batches[10];profile_v1_t profiles[8];workspace_avatar_t avatar;} user_v1_t;
typedef struct {uint32_t version,next_id,active_id,user_count;user_v1_t users[8];} model_v1_t;
static bool load_v1(int fd,const uint32_t *header)
{
    model_v1_t *old=malloc(sizeof(*old));if(!old)return false;
    bool ok=full_io(fd,old,sizeof(*old),false)&&header[3]==checksum(old,sizeof(*old))&&old->version==1&&old->user_count<=WORKSPACE_USERS;
    if(ok){
        memset(&transaction,0,sizeof(transaction));transaction.version=3;transaction.next_id=old->next_id;transaction.active_id=old->active_id;transaction.user_count=old->user_count;
        for(unsigned i=0;i<old->user_count;i++){
            const user_v1_t *v=&old->users[i];workspace_user_t *u=&transaction.users[i];
            u->id=v->id;memcpy(u->name,v->name,sizeof(u->name));u->quick_enabled=v->quick_enabled;u->profile_count=v->profile_count;u->batch_count=v->batch_count;
            memcpy(u->batches,v->batches,sizeof(u->batches));u->avatar=v->avatar;
            for(unsigned j=0;j<WORKSPACE_PROFILES;j++){
                const profile_v1_t *p=&v->profiles[j];workspace_profile_t *n=&u->profiles[j];
                n->id=p->id;memcpy(n->name,p->name,sizeof(n->name));n->speed=p->speed;n->work=p->work;n->add=p->add;n->sort=p->sort;n->beep=p->beep;n->batch_enabled=p->batch_enabled;n->batch=p->batch;
            }
        }
        ok=workspace_model_valid(&transaction);
    }
    free(old);return ok;
}
/* v2 adds count mode and automatic QR; v3 appends optional operator metadata. */
typedef struct {uint32_t id;char name[25];uint8_t quick_enabled,profile_count,batch_count;uint8_t batches[10];uint8_t qr_after_count;workspace_profile_t profiles[8];workspace_avatar_t avatar;} user_v2_t;
typedef struct {uint32_t version,next_id,active_id,user_count;user_v2_t users[8];} model_v2_t;
_Static_assert(sizeof(user_v2_t)==16752,"v2 storage layout");
static bool load_v2(int fd,const uint32_t *header)
{
    model_v2_t *old=malloc(sizeof(*old));if(!old)return false;
    bool valid=full_io(fd,old,sizeof(*old),false)&&header[3]==checksum(old,sizeof(*old))&&old->version==2&&old->user_count<=WORKSPACE_USERS;
    if(valid){
        memset(&transaction,0,sizeof(transaction));transaction.version=3;transaction.next_id=old->next_id;transaction.active_id=old->active_id;transaction.user_count=old->user_count;
        for(unsigned i=0;i<old->user_count;i++)memcpy(&transaction.users[i],&old->users[i],sizeof(user_v2_t));
        valid=workspace_model_valid(&transaction);
    }
    free(old);return valid;
}
static bool load_model(void)
{
    int fd=open(STORE,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
    if(fd<0) {if(errno==ENOENT){workspace_defaults(&transaction);return true;}return false;}
    uint32_t header[4];struct stat st;
    bool valid=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&full_io(fd,header,sizeof(header),false)&&header[0]==0x31535755U;
    if(valid&&header[1]==1&&header[2]==sizeof(model_v1_t)&&st.st_size==(off_t)(sizeof(header)+sizeof(model_v1_t)))valid=load_v1(fd,header);
    else if(valid&&header[1]==2&&header[2]==sizeof(model_v2_t)&&st.st_size==(off_t)(sizeof(header)+sizeof(model_v2_t)))valid=load_v2(fd,header);
    else if(valid&&header[1]==3&&header[2]==sizeof(transaction)&&st.st_size==(off_t)(sizeof(header)+sizeof(transaction)))
        valid=full_io(fd,&transaction,sizeof(transaction),false)&&header[3]==checksum(&transaction,sizeof(transaction))&&workspace_model_valid(&transaction);
    else valid=false;
    close(fd);return valid;
}
static bool save_model(void)
{
    uncertain_save=false;
    if(mkdir(WORKSPACE_DIRECTORY,0755)&&errno!=EEXIST)return false;
    uint32_t header[]={0x31535755U,3,sizeof(transaction),checksum(&transaction,sizeof(transaction))};
    int dir=open(WORKSPACE_DIRECTORY,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return false;
    int fd=open(STORE ".tmp",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
    if(fd<0){close(dir);return false;}
    bool success=full_io(fd,header,sizeof(header),true)&&full_io(fd,&transaction,sizeof(transaction),true)&&!fsync(fd);
    if(close(fd))success=false;
    if(success&&rename(STORE ".tmp",STORE))success=false;
    if(success&&fsync(dir)){success=false;uncertain_save=true;}
    close(dir);if(!success)unlink(STORE ".tmp");return success;
}
static void image_path(char *path,size_t n,unsigned image)
{
    if(!image)snprintf(path,n,WORKSPACE_USB_DIRECTORY "/avatar.png");
    else snprintf(path,n,WORKSPACE_USB_DIRECTORY "/un260_avatar_%02u.png",image);
}
static int open_image(unsigned image)
{
    char path[256];image_path(path,sizeof(path),image);
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);struct stat st;
    if(fd>=0&&(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<33||st.st_size>2*1024*1024)){close(fd);return -1;}
    return fd;
}
static uint32_t be32(const uint8_t *p) {return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static bool import_image(void)
{
    int fd=open_image(source);if(fd<0)return false;
    struct stat st;if(fstat(fd,&st)){close(fd);return false;}
    uint8_t *bytes=malloc(st.st_size),*rgba=NULL;bool success=false;
    if(!bytes){close(fd);return false;}
    bool read_ok=full_io(fd,bytes,st.st_size,false);close(fd);
    png_image image={.version=PNG_IMAGE_VERSION};
    if(!read_ok||png_sig_cmp(bytes,0,8)||be32(bytes+16)<16||be32(bytes+20)<16||be32(bytes+16)>2048||be32(bytes+20)>2048)goto finish;
    if(!png_image_begin_read_from_memory(&image,bytes,st.st_size))goto finish;
    if(image.width>2048||image.height>2048)goto finish;
    image.format=PNG_FORMAT_RGBA;rgba=malloc(PNG_IMAGE_SIZE(image));
    if(!rgba||!png_image_finish_read(&image,NULL,rgba,0,NULL))goto finish;
    unsigned side=image.width<image.height?image.width:image.height;
    unsigned ox=(image.width-side)/2,oy=(image.height-side)/2;
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++) {
        unsigned s=((oy+y*side/64)*image.width+ox+x*side/64)*4,d=(y*64+x)*4,a=rgba[s+3];
        for(unsigned c=0;c<3;c++)imported.pixels[d+c]=(rgba[s+2-c]*a+250*(255-a))/255;
        imported.pixels[d+3]=255;
    }
    imported.present=1;success=true;
finish:
    png_image_free(&image);free(rgba);free(bytes);return success;
}
static bool export_support(void)
{
    if(!usb_storage_prepare())return false;
    int dir=open(WORKSPACE_USB_DIRECTORY,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);if(dir<0)return false;
    char name[96];int fd=-1;
    for(unsigned i=0;i<1000;i++){
        snprintf(name,sizeof(name),"UN260_support_%lld_%03u.txt",(long long)time(NULL),i);
        fd=openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);
        if(fd>=0||errno!=EEXIST)break;
    }
    bool success=fd>=0&&full_io(fd,support_report,strlen(support_report),true)&&!fsync(fd);
    if(fd>=0&&close(fd))success=false;
    if(success&&fsync(dir))success=false;
    if(!success&&fd>=0)unlinkat(dir,name,0);
    close(dir);
    if(success)snprintf(result,sizeof(result),"Saved to USB: %s",name);
    return success;
}
static void *run(void *unused)
{
    (void)unused;bool success=false;
    if(operation==LOAD){success=load_model();snprintf(result,sizeof(result),success?"Workspace loaded.":"Workspace could not be read. Existing file was not replaced.");}
    if(operation==SAVE){success=save_model();snprintf(result,sizeof(result),success?"Saved on this device.":uncertain_save?"Storage confirmation failed. Restart to check the saved workspace.":"Save failed. Check storage before trying again.");}
    if(operation==SUPPORT){success=export_support();if(!success)snprintf(result,sizeof(result),"Support export failed. Check USB space and connection, then retry.");}
    if(operation==SCAN||operation==IMPORT) {
        if(!usb_storage_prepare())snprintf(result,sizeof(result),"Insert a USB drive and try again.");
        else if(operation==SCAN) {
            found=0;for(unsigned i=0;i<=20;i++){int fd=open_image(i);if(fd>=0){uint8_t sig[8];if(full_io(fd,sig,8,false)&&!png_sig_cmp(sig,0,8))found|=1U<<i;close(fd);}}
            success=true;snprintf(result,sizeof(result),found?"Choose a photo to preview.":"No supported photo. Use avatar.png or un260_avatar_01.png (01-20).");
        } else {success=import_image();snprintf(result,sizeof(result),success?"Photo ready. Save to keep it on this device.":"Photo could not be read. Use PNG, 16-2048 px, at most 2 MB.");}
    }
    pthread_mutex_lock(&lock);ok=success;done=true;pthread_mutex_unlock(&lock);return NULL;
}
static bool start(operation_t op)
{
    if(busy)return false;
    operation=op;done=false;busy=true;
    if(pthread_create(&worker,NULL,run,NULL)){busy=false;snprintf(message,sizeof(message),"Background task could not start.");return false;}
    return true;
}
void workspace_store_init(void)
{
    if(initialized)return;
    workspace_defaults(&saved);snprintf(message,sizeof(message),"Loading workspace...");
    initialized=start(LOAD);
}
bool workspace_store_poll(void)
{
    if(!initialized){workspace_store_init();return false;}
    if(!busy)return false;
    pthread_mutex_lock(&lock);bool finished=done;pthread_mutex_unlock(&lock);if(!finished)return false;
    pthread_join(worker,NULL);
    if(ok&&(operation==LOAD||operation==SAVE)){saved=transaction;ready=true;revision++;}
    /* Rename succeeded but durability was not confirmed: do not allow another
     * write based on the old published model. Reload only on the next boot. */
    if(operation==SAVE&&!ok&&uncertain_save)ready=false;
    if(operation==SCAN)images=ok?found:0;
    if(operation==IMPORT&&ok)preview=imported;
    snprintf(message,sizeof(message),"%s",result);busy=false;return true;
}
bool workspace_store_ready(void){return ready;}
bool workspace_store_busy(void){return busy;}
const char *workspace_store_message(void){return message;}
const workspace_model_t *workspace_store_get(void){return &saved;}
bool workspace_store_save(const workspace_model_t *m)
{
    if(!ready||busy||!workspace_model_valid(m))return false;
    transaction=*m;return start(SAVE);
}
bool workspace_store_scan_usb(void){if(!ready||busy)return false;memset(&preview,0,sizeof(preview));return start(SCAN);}
uint32_t workspace_store_usb_images(void){return images;}
bool workspace_store_import_avatar(unsigned image)
{
    if(!ready||busy||image>20||!(images&(1U<<image)))return false;
    memset(&preview,0,sizeof(preview));
    source=image;return start(IMPORT);
}
const workspace_avatar_t *workspace_store_avatar(void){return &preview;}
unsigned workspace_store_revision(void){return revision;}
bool workspace_store_last_success(void){return !busy&&ok;}
bool workspace_store_export_support(const char *report)
{
    if(!ready||busy||!report||!*report||strnlen(report,sizeof(support_report))>=sizeof(support_report))return false;
    snprintf(support_report,sizeof(support_report),"%s",report);return start(SUPPORT);
}
