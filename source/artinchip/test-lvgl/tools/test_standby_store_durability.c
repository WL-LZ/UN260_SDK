#include "un260/storage/standby_store.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum fault_kind { NONE, FILE_SYNC, DIR_OPEN, DIR_SYNC, DIR_CLOSE, SOURCE_READ,
                 SOURCE_CLOSE, META_UNLINK, META_UNLINK_EIO, DELETE_SYNC, BUSY_WRITE };
enum descriptor_kind { UNKNOWN, LOCAL_DIR, SOURCE, LOCAL_TEMP };
static enum fault_kind fault;
static enum descriptor_kind descriptors[1024];
static unsigned removed;
static bool usb=true;
static atomic_bool writing_blocked, release_write;
bool usb_storage_prepare(void){return usb;}
int __real_open(const char *,int,...);
int __real_close(int);
int __real_fsync(int);
int __real_unlink(const char *);
ssize_t __real_read(int,void *,size_t);
ssize_t __real_write(int,const void *,size_t);
int __wrap_open(const char *path,int flags,...){
 mode_t mode=0;
 if(flags&O_CREAT){va_list args;va_start(args,flags);mode=(mode_t)va_arg(args,int);va_end(args);}
 if((flags&O_DIRECTORY)&&fault==DIR_OPEN){errno=EIO;return -1;}
 int fd=(flags&O_CREAT)?__real_open(path,flags,mode):__real_open(path,flags);
 if(fd>=0&&fd<(int)(sizeof(descriptors)/sizeof(descriptors[0])))
  descriptors[fd]=(flags&O_DIRECTORY)?LOCAL_DIR:
   (strstr(path,STANDBY_USB_DIRECTORY "/")?SOURCE:LOCAL_TEMP);
 return fd;
}
static enum descriptor_kind descriptor(int fd){
 return fd>=0&&fd<(int)(sizeof(descriptors)/sizeof(descriptors[0]))?descriptors[fd]:UNKNOWN;
}
int __wrap_close(int fd){
 enum descriptor_kind kind=descriptor(fd);int result=__real_close(fd);
 if((kind==LOCAL_DIR&&fault==DIR_CLOSE)||(kind==SOURCE&&fault==SOURCE_CLOSE)){
  errno=EIO;return -1;
 }
 return result;
}
int __wrap_fsync(int fd){
 enum descriptor_kind kind=descriptor(fd);
 if((kind==LOCAL_TEMP&&fault==FILE_SYNC)||(kind==LOCAL_DIR&&fault==DIR_SYNC)||
    (kind==LOCAL_DIR&&fault==DELETE_SYNC&&removed)){
  errno=EIO;return -1;
 }
 return __real_fsync(fd);
}
ssize_t __wrap_read(int fd,void *data,size_t bytes){
 if(descriptor(fd)==SOURCE&&fault==SOURCE_READ){errno=EIO;return -1;}
 return __real_read(fd,data,bytes);
}
ssize_t __wrap_write(int fd,const void *data,size_t bytes){
 if(descriptor(fd)==LOCAL_TEMP&&fault==BUSY_WRITE){
  atomic_store(&writing_blocked,true);
  while(!atomic_load(&release_write))usleep(1000);
 }
 return __real_write(fd,data,bytes);
}
int __wrap_unlink(const char *path){
 if(strstr(path,"standby_1.bin.source")&&(fault==META_UNLINK||fault==META_UNLINK_EIO)){
  errno=fault==META_UNLINK_EIO?EIO:EACCES;return -1;
 }
 int result=__real_unlink(path);
 if(result==0&&strstr(path,"standby_1.bin"))removed++;
 return result;
}
static void wait_job(void){
 char message[160];assert(standby_store_busy());
 for(unsigned i=0;i<10000;i++){
  if(standby_store_poll(message,sizeof(message))){assert(!standby_store_busy());return;}
  usleep(1000);
 }
 assert(!"worker timeout");
}
static void write_png(void){
 png_image image={0};image.version=PNG_IMAGE_VERSION;
 image.width=1280;image.height=400;image.format=PNG_FORMAT_RGBA;
 unsigned char *pixels=malloc(1280*400*4);assert(pixels);memset(pixels,255,1280*400*4);
 assert(png_image_write_to_file(&image,STANDBY_USB_DIRECTORY "/un260_delay_01.png",0,pixels,0,NULL));
 free(pixels);
}
static standby_config_t baseline(void){
 standby_store_init();
 standby_config_t cfg;standby_defaults(&cfg);cfg.minutes=7;
 assert(standby_store_storage_ready());assert(standby_store_save(&cfg));wait_job();
 assert(standby_store_last_success());assert(standby_store_storage_ready());return cfg;
}
int main(int argc,char **argv){
 assert(argc==2);
 if(!strcmp(argv[1],"boot")){
  assert(standby_store_storage_ready());assert(standby_config()->minutes==9);
  puts("PASS standby durability latch resets only in new process");return 0;
 }
 standby_config_t cfg=baseline();
 if(!strcmp(argv[1],"busy")){
  fault=BUSY_WRITE;cfg.minutes=88;assert(standby_store_save(&cfg));
  for(unsigned i=0;i<10000&&!atomic_load(&writing_blocked);i++)usleep(1000);
  assert(atomic_load(&writing_blocked));assert(standby_store_busy());
  assert(standby_store_storage_ready());assert(!standby_store_save(&cfg));
  assert(!standby_store_import());assert(!standby_store_delete(3));
  atomic_store(&release_write,true);wait_job();assert(standby_store_last_success());
  assert(standby_store_storage_ready());
 }else if(!strcmp(argv[1],"import-sync")){
  write_png();fault=DIR_SYNC;assert(standby_store_import());wait_job();
  assert(!standby_store_last_success());assert(!standby_store_storage_ready());
  assert(!standby_photo_exists(0));assert(standby_config()->minutes==7);
  fault=NONE;assert(standby_store_import());wait_job();assert(standby_store_last_success());
  assert(standby_photo_exists(0));assert(!standby_store_storage_ready());
 }else if(!strcmp(argv[1],"source-read")||!strcmp(argv[1],"source-close")||!strcmp(argv[1],"missing-usb")){
  write_png();fault=!strcmp(argv[1],"source-read")?SOURCE_READ:SOURCE_CLOSE;
  if(!strcmp(argv[1],"missing-usb")){fault=NONE;usb=false;}
  assert(standby_store_import());wait_job();assert(!standby_store_last_success());
  assert(standby_store_storage_ready());assert(!standby_photo_exists(0));
  assert(standby_config()->minutes==7);
 }else if(!strcmp(argv[1],"delete-sync")||!strcmp(argv[1],"delete-meta")||!strcmp(argv[1],"delete-meta-eio")||!strcmp(argv[1],"delete-config-sync")){
  write_png();assert(standby_store_import());wait_job();assert(standby_store_last_success());
  cfg.layout[0][0].photo=3;cfg.layout[0][0].scheduled=0;
  assert(standby_store_save(&cfg));wait_job();
  fault=!strcmp(argv[1],"delete-sync")?DELETE_SYNC:(!strcmp(argv[1],"delete-config-sync")?DIR_SYNC:
   (!strcmp(argv[1],"delete-meta")?META_UNLINK:META_UNLINK_EIO));
  assert(standby_store_delete(3));wait_job();assert(!standby_store_last_success());
  bool config_failed=fault==DIR_SYNC;
  assert(standby_photo_exists(0)==config_failed);
  assert(standby_config()->layout[0][0].photo==(config_failed?3:1));
  assert(standby_config()->layout[0][0].scheduled==!config_failed);
  assert(standby_store_storage_ready()==(fault==META_UNLINK));
 }else{
  fault=!strcmp(argv[1],"file-sync")?FILE_SYNC:(!strcmp(argv[1],"dir-open")?DIR_OPEN:
   (!strcmp(argv[1],"dir-sync")?DIR_SYNC:DIR_CLOSE));
  cfg.minutes=88;assert(standby_store_save(&cfg));wait_job();
  assert(!standby_store_last_success());assert(standby_config()->minutes==7);
  assert(standby_store_storage_ready()==(fault==FILE_SYNC));
  FILE *input=fopen(STANDBY_STORE_DIRECTORY "/standby.cfg","rb");assert(input);
  standby_config_t on_disk;assert(fread(&on_disk,1,sizeof(on_disk),input)==sizeof(on_disk));fclose(input);
  assert(on_disk.minutes==(fault==FILE_SYNC?7:88));
  bool was_ready=standby_store_storage_ready();fault=NONE;cfg.minutes=9;
  assert(standby_store_save(&cfg));wait_job();assert(standby_store_last_success());
  assert(standby_config()->minutes==9);assert(standby_store_storage_ready()==was_ready);
 }
 printf("PASS standby fault %s\n",argv[1]);return 0;
}
