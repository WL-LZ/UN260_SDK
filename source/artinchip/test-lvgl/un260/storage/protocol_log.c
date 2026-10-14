#include "protocol_log.h"
#include "usb_storage.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#ifndef UI_STATE_DIR
#define UI_STATE_DIR "/etc/ui_state"
#endif
#define LOG_DIR UI_STATE_DIR "/protocol_logs"
#define QUEUE_SIZE 128
#define CHUNK 512
#ifndef PROTOCOL_LOG_USB_DIR
#define PROTOCOL_LOG_USB_DIR USB_STORAGE_MOUNT_POINT
#endif
#ifndef SESSION_LIMIT
#define SESSION_LIMIT (16U*1024U*1024U)
#endif
typedef struct {uint64_t time;unsigned len;unsigned char port,tx,data[CHUNK];} record_t;
static record_t queue[QUEUE_SIZE];
static unsigned head,tail,queued;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake=PTHREAD_COND_INITIALIZER;
static pthread_t thread;
static bool started,quit;
static int ports[2]={-1,-1},command;
static uint64_t command_id,next_id;
static protocol_log_snapshot_t snapshot;
static FILE *file;
static uint64_t bytes;
static void path(char *out,size_t size,uint64_t id){snprintf(out,size,LOG_DIR "/log_%012llu.txt",(unsigned long long)id);}
static int newest(const void *a,const void *b){uint64_t x=((const protocol_log_file_t*)a)->id,y=((const protocol_log_file_t*)b)->id;return x<y?1:x>y?-1:0;}
static bool scan(void)
{
 protocol_log_file_t list[PROTOCOL_LOG_KEEP+1]={0};unsigned n=0;
 DIR *dir=opendir(LOG_DIR);if(!dir)return false;struct dirent *e;
 while((e=readdir(dir))){unsigned long long id;char extra;if(sscanf(e->d_name,"log_%llu.txt%c",&id,&extra)!=1)continue;
   char name[48];snprintf(name,sizeof(name),"log_%012llu.txt",id);if(strcmp(name,e->d_name))continue;
   char p[256];path(p,sizeof(p),id);struct stat st;if(lstat(p,&st)||!S_ISREG(st.st_mode))continue;
   if(id>next_id)next_id=id;list[n]=(protocol_log_file_t){.id=id,.bytes=st.st_size};snprintf(list[n].name,sizeof(list[n].name),"%s",name);n++;
   qsort(list,n,sizeof(list[0]),newest);if(n>PROTOCOL_LOG_KEEP){path(p,sizeof(p),list[--n].id);if(unlink(p)){closedir(dir);return false;}}
 }
 closedir(dir);pthread_mutex_lock(&lock);snapshot.count=n;memcpy(snapshot.files,list,n*sizeof(list[0]));snapshot.revision++;pthread_mutex_unlock(&lock);return true;
}
static bool finish(void)
{
 bool ok=true;if(file){pthread_mutex_lock(&lock);uint64_t dropped=snapshot.dropped;pthread_mutex_unlock(&lock);if(fprintf(file,"# END bytes=%llu dropped=%llu\n",(unsigned long long)bytes,(unsigned long long)dropped)<0)ok=false;if(fflush(file)||fsync(fileno(file)))ok=false;if(fclose(file))ok=false;file=NULL;}
 return scan()&&ok;
}
static bool copy_usb(uint64_t id)
{
 if(!usb_storage_prepare())return false;
 char src[256],dst[256],tmp[280];path(src,sizeof(src),id);
 int in=open(src,O_RDONLY|O_NOFOLLOW);if(in<0)return false;
 int out=-1;for(unsigned n=0;n<1000;n++){
   snprintf(dst,sizeof(dst),PROTOCOL_LOG_USB_DIR "/protocol_%012llu_%03u.txt",(unsigned long long)id,n);
   if(access(dst,F_OK)==0)continue;snprintf(tmp,sizeof(tmp),"%s.part",dst);out=open(tmp,O_CREAT|O_EXCL|O_WRONLY,0600);if(out>=0)break;if(errno!=EEXIST)break;
 }
 if(out<0){close(in);return false;}
 bool ok=true;char buf[16384];ssize_t n;
 while((n=read(in,buf,sizeof(buf)))!=0){if(n<0){if(errno==EINTR)continue;ok=false;break;}size_t done=0;
   while(done<(size_t)n){ssize_t w=write(out,buf+done,(size_t)n-done);if(w<0&&errno==EINTR)continue;if(w<=0){ok=false;break;}done+=w;}if(!ok)break;
 }
 if(fsync(out))ok=false;if(close(out))ok=false;close(in);
 if(ok&&rename(tmp,dst))ok=false;
 if(ok){int dir=open(PROTOCOL_LOG_USB_DIR,O_RDONLY|O_DIRECTORY);if(dir<0||fsync(dir))ok=false;if(dir>=0)close(dir);}
 if(!ok)unlink(tmp);return ok;
}
static void result(protocol_log_state_t state){pthread_mutex_lock(&lock);snapshot.busy=false;snapshot.state=state;snapshot.revision++;pthread_mutex_unlock(&lock);}
static void *run(void *unused)
{
 (void)unused;
 bool ready=(mkdir(UI_STATE_DIR,0755)==0||errno==EEXIST)&&(mkdir(LOG_DIR,0700)==0||errno==EEXIST)&&scan();result(ready?PLOG_IDLE:PLOG_ERROR);
 for(;;){
   pthread_mutex_lock(&lock);while(!queued&&!command&&!quit){
     struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_sec++;
     if(pthread_cond_timedwait(&wake,&lock,&until)==ETIMEDOUT&&file){
       pthread_mutex_unlock(&lock);bool ok=fflush(file)==0;pthread_mutex_lock(&lock);
       if(!ok){snapshot.active=false;snapshot.dropped++;command=2;snapshot.busy=true;break;}
     }
   }
   record_t item;bool have=queued>0;int cmd=0;uint64_t id=0;bool exit_now=quit&&!queued&&!command;
   if(have){item=queue[tail];tail=(tail+1)%QUEUE_SIZE;queued--;}
   else if(command){cmd=command;id=command_id;command=0;}
   pthread_mutex_unlock(&lock);if(exit_now)break;
   if(have&&file){
     char line[CHUNK*3+100];int n=snprintf(line,sizeof(line),"%llu.%03llu UART%u %s ",(unsigned long long)(item.time/1000),(unsigned long long)(item.time%1000),item.port,item.tx?"TX":"RX");
     for(unsigned i=0;i<item.len;i++)n+=snprintf(line+n,sizeof(line)-n,"%02X%s",item.data[i],i+1==item.len?"\n":" ");
     bool ok=bytes+(unsigned)n<=SESSION_LIMIT&&fwrite(line,1,n,file)==(size_t)n;bytes+=ok?n:0;
     if(!ok){
       /* Keep admission closed until the failed session is flushed and closed. */
       pthread_mutex_lock(&lock);snapshot.active=false;snapshot.busy=true;snapshot.dropped+=item.len;
       while(queued){snapshot.dropped+=queue[tail].len;tail=(tail+1)%QUEUE_SIZE;queued--;}
       pthread_mutex_unlock(&lock);finish();result(PLOG_ERROR);
     }
     pthread_mutex_lock(&lock);if(snapshot.count&&snapshot.files[0].id==snapshot.current){if(snapshot.files[0].bytes/1024!=bytes/1024)snapshot.revision++;snapshot.files[0].bytes=bytes;}pthread_mutex_unlock(&lock);
     continue;
   }
   if(cmd==1){
     bool ok=!file&&(mkdir(LOG_DIR,0700)==0||errno==EEXIST);char p[256];id=++next_id;path(p,sizeof(p),id);
     int fd=ok?open(p,O_CREAT|O_EXCL|O_WRONLY,0600):-1;file=fd>=0?fdopen(fd,"w"):NULL;if(fd>=0&&!file)close(fd);
     ok=file!=NULL;bytes=0;if(ok){time_t now=time(NULL);ok=fprintf(file,"UN260 protocol log | started %lld\nRaw UART chunks; timestamps are monotonic milliseconds. TX records contain bytes accepted by the driver.\n",(long long)now)>0&&fflush(file)==0&&scan();}
     pthread_mutex_lock(&lock);snapshot.active=ok;snapshot.current=ok?id:0;snapshot.dropped=0;pthread_mutex_unlock(&lock);
     if(!ok&&file)finish();result(ok?PLOG_ACTIVE:PLOG_ERROR);
   }else if(cmd==2||cmd==3){bool ok=finish();if(cmd==3&&ok)ok=copy_usb(id);pthread_mutex_lock(&lock);bool dropped=snapshot.dropped!=0;pthread_mutex_unlock(&lock);result(ok?(cmd==3?PLOG_EXPORTED:dropped?PLOG_ERROR:PLOG_IDLE):PLOG_ERROR);}
   else if(cmd==4){char p[256];path(p,sizeof(p),id);bool ok=unlink(p)==0&&scan();result(ok?PLOG_IDLE:PLOG_ERROR);}
 }
 finish();pthread_mutex_lock(&lock);snapshot.active=false;pthread_mutex_unlock(&lock);return NULL;
}
bool protocol_log_init(void)
{
 if(started)return true;snapshot.busy=true;snapshot.state=PLOG_BUSY;quit=false;
 if(pthread_create(&thread,NULL,run,NULL)){snapshot.busy=false;snapshot.state=PLOG_ERROR;return false;}started=true;return true;
}
void protocol_log_shutdown(void)
{
 if(!started)return;pthread_mutex_lock(&lock);snapshot.active=false;quit=true;pthread_cond_signal(&wake);pthread_mutex_unlock(&lock);pthread_join(thread,NULL);started=false;
}
void protocol_log_ports(int controller,int bridge){pthread_mutex_lock(&lock);ports[0]=controller;ports[1]=bridge;pthread_mutex_unlock(&lock);}
void protocol_log_snapshot(protocol_log_snapshot_t *out){if(!out)return;pthread_mutex_lock(&lock);*out=snapshot;pthread_mutex_unlock(&lock);}
static bool submit(int cmd,uint64_t id)
{
 if(!started&&!protocol_log_init())return false;
 pthread_mutex_lock(&lock);bool exists=false;for(unsigned i=0;i<snapshot.count;i++)if(snapshot.files[i].id==id)exists=true;
 bool ok=!snapshot.busy&&!command&&(cmd==1?!snapshot.active:cmd==2?snapshot.active:exists&&(!snapshot.active||cmd==3&&snapshot.current==id));
 if(ok){command=cmd;command_id=id;snapshot.busy=true;if(cmd==2||cmd==3)snapshot.active=false;snapshot.revision++;pthread_cond_signal(&wake);}
 pthread_mutex_unlock(&lock);return ok;
}
bool protocol_log_start(void){return submit(1,0);}
bool protocol_log_stop(void){return submit(2,0);}
bool protocol_log_export(uint64_t id){return submit(3,id);}
bool protocol_log_delete(uint64_t id){return submit(4,id);}
void protocol_log_record(int fd,bool tx,const void *data,size_t length)
{
 if(!data||!length)return;pthread_mutex_lock(&lock);
 if(!snapshot.active||(fd!=ports[0]&&fd!=ports[1])||fd<0){pthread_mutex_unlock(&lock);return;}
 size_t chunks=(length+CHUNK-1)/CHUNK;
 if(chunks>QUEUE_SIZE-queued){snapshot.dropped+=length;snapshot.active=false;snapshot.state=PLOG_ERROR;command=2;command_id=0;snapshot.busy=true;snapshot.revision++;pthread_cond_signal(&wake);pthread_mutex_unlock(&lock);return;}
 struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
 const unsigned char *p=data;
 while(length){record_t *r=&queue[head];size_t n=length>CHUNK?CHUNK:length;r->time=(uint64_t)now.tv_sec*1000+now.tv_nsec/1000000;r->port=fd==ports[0]?4:5;r->tx=tx;r->len=n;memcpy(r->data,p,n);p+=n;length-=n;head=(head+1)%QUEUE_SIZE;queued++;}
 pthread_cond_signal(&wake);pthread_mutex_unlock(&lock);
}
