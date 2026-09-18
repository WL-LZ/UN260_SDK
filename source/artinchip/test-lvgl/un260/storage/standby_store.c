#include "standby_store.h"
#include "usb_storage.h"
#include <pthread.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef STANDBY_STORE_DIRECTORY
#define STANDBY_STORE_DIRECTORY "/etc/ui_state"
#endif
#ifndef STANDBY_USB_DIRECTORY
#define STANDBY_USB_DIRECTORY USB_STORAGE_MOUNT_POINT
#endif
#define DIR STANDBY_STORE_DIRECTORY
#define CFG DIR "/standby.cfg"
#define PIXELS (1280U * 400U)
static standby_config_t saved;
static bool initialized, busy, done, success, config_written;
static pthread_t thread;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct { bool importing, deleting; unsigned photo; standby_config_t cfg; } job;
static char result[160];
static const char *paths[] = {
 "L:/usr/local/share/lvgl_data/standby/silver.png",
 "L:/usr/local/share/lvgl_data/standby/celadon.png",
 "L:/usr/local/share/lvgl_data/standby/champagne.png",
 "L:" DIR "/standby_1.bin", "L:" DIR "/standby_2.bin",
 "L:" DIR "/standby_3.bin", "L:" DIR "/standby_4.bin",
 "L:" DIR "/standby_5.bin", "L:" DIR "/standby_6.bin",
 "L:/usr/local/share/lvgl_data/standby/mist.png" };
const char *standby_photo_path(unsigned photo) { return paths[photo < STANDBY_PHOTO_COUNT ? photo : 1]; }
void standby_defaults(standby_config_t *c) {
 memset(c,0,sizeof(*c)); c->version=2; c->minutes=1;
 const uint32_t colors[]={0x14232D,0xEDF1EC,0x30243C};
 for(int m=0;m<2;m++) for(int i=0;i<3;i++) {
  standby_layout_t *p=&c->layout[m][i];p->x=44+328*i;p->y=64;
  p->color=colors[i];p->date_bits=14;p->greeting=1;p->scheduled=1;p->photo=1;p->date_style=i;p->scale_percent=100;
  p->date_x=p->x;p->date_y=230;p->dial_x=i?75:830;p->dial_y=50;p->auto_text=1;p->text_color=m?0xFFFFFF:0x304957;
 }
}
bool standby_config_valid(const standby_config_t *c) {
 if(c->version!=2||c->minutes>1440||c->mode>1||c->active[0]>2||c->active[1]>2)return false;
 for(int m=0;m<2;m++)for(int i=0;i<3;i++) {
  const standby_layout_t*p=&c->layout[m][i];
  if(p->scale_percent&&(p->scale_percent<75||p->scale_percent>140))return false;
  if(p->x>1240||p->y<40||p->y>290||p->date_x>1240||p->date_y<40||p->date_y>290||p->dial_x>1024||p->dial_y<40||p->dial_y>78||p->color>0xFFFFFF||p->text_color>0xFFFFFF||p->auto_text>1||p->date_bits>15||p->date_style>2||p->hour12>1||p->greeting>1||p->scheduled>1||p->photo>=STANDBY_PHOTO_COUNT||p->light_text>1)return false;
 }return true;
}
void standby_store_init(void) {
 if(initialized)return;
 initialized=true;standby_defaults(&saved);
 FILE*f=fopen(CFG,"rb");if(!f)return;standby_config_t c={0};
 size_t bytes=fread(&c,1,sizeof(c),f);bool eof=fgetc(f)==EOF;fclose(f);
 if(eof&&bytes==sizeof(c)&&standby_config_valid(&c)){saved=c;return;}
 /* Preserve the exact v1 wire layout; never reinterpret enlarged entries. */
 typedef struct {uint32_t color;uint16_t x,y;uint8_t date_bits,date_style,hour12,greeting,scheduled,photo,light_text,scale_percent;} old_layout_t;
 typedef struct {uint32_t version;uint16_t minutes;uint8_t mode,active[2],reserved[3];old_layout_t layout[2][3];} old_config_t;
 if(eof&&bytes==sizeof(old_config_t)&&c.version==1){
  old_config_t old;memcpy(&old,&c,sizeof(old));standby_config_t migrated;standby_defaults(&migrated);
  migrated.minutes=old.minutes;migrated.mode=old.mode;memcpy(migrated.active,old.active,2);
  for(unsigned m=0;m<2;m++)for(unsigned i=0;i<3;i++){
   old_layout_t *a=&old.layout[m][i];standby_layout_t *b=&migrated.layout[m][i];
   if(a->scale_percent&&(a->scale_percent<75||a->scale_percent>140))return;
   unsigned old_scale=a->scale_percent?a->scale_percent:100;
   if(a->x<24||a->x>1256-526*old_scale/100||a->y<50||a->y>330-200*old_scale/100)return;
   memcpy(b,a,sizeof(*a));unsigned z=standby_layout_scale(b);
   b->x=a->x>10?a->x-10:0;b->y=a->y>16?a->y-16:40;b->date_x=a->x;b->date_y=a->y+163*z/100-20;
   b->dial_x=a->x>360?75:830;b->dial_y=50;b->text_color=a->light_text?0xFFFFFF:0x304957;b->auto_text=1;standby_layout_clamp(b);
  }
  if(standby_config_valid(&migrated))saved=migrated;
 }
}
const standby_config_t *standby_config(void){standby_store_init();return &saved;}
static bool atomic_write(const char *path,const void*data,size_t bytes) {
 char tmp[160];snprintf(tmp,sizeof(tmp),"%s.tmp",path);
 if(mkdir(DIR,0755)&&errno!=EEXIST)return false;
 int fd=open(tmp,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);if(fd<0)return false;
 const unsigned char*p=data;size_t n=0;bool ok=true;
 while(n<bytes){ssize_t w=write(fd,p+n,bytes-n);if(w<0&&errno==EINTR)continue;if(w<=0){ok=false;break;}n+=(size_t)w;}
 if(ok&&fsync(fd))ok=false;
 if(close(fd))ok=false;
 if(ok&&rename(tmp,path))ok=false;
 if(ok){int d=open(DIR,O_RDONLY|O_DIRECTORY);if(d>=0){fsync(d);close(d);}}
 if(!ok)unlink(tmp);
 return ok;
}
bool standby_photo_exists(unsigned slot){struct stat s;return slot<6&&stat(paths[slot+3]+2,&s)==0&&S_ISREG(s.st_mode)&&s.st_size==(off_t)(4+PIXELS*4);}
static unsigned be32(const unsigned char*p){return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3];}
static bool import_one(const char*path,unsigned slot,unsigned source) {
 int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return false;struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size<24||st.st_size>5*1024*1024){close(fd);return false;}
 size_t size=(size_t)st.st_size;unsigned char*input=malloc(size);if(!input){close(fd);return false;}
 size_t n=0;while(n<size){ssize_t r=read(fd,input+n,size-n);if(r<0&&errno==EINTR)continue;if(r<=0)break;n+=(size_t)r;}close(fd);
 bool ok=false;png_image image;memset(&image,0,sizeof(image));image.version=PNG_IMAGE_VERSION;
 unsigned char*rgba=NULL;unsigned char*out=NULL;
 if(n!=size||png_sig_cmp(input,0,8)||be32(input+16)<640||be32(input+16)>4096||be32(input+20)<200||be32(input+20)>2160)goto finish;
 if(!png_image_begin_read_from_memory(&image,input,size))goto finish;
 if(image.width>4096||image.height>2160)goto finish;
 image.format=PNG_FORMAT_RGBA;rgba=malloc(PNG_IMAGE_SIZE(image));out=malloc(4+PIXELS*4);if(!rgba||!out)goto finish;
 if(!png_image_finish_read(&image,NULL,rgba,0,NULL))goto finish;
 /* LVGL true-colour 32-bit binary: BGRA with opaque alpha. One-time crop. */
 uint32_t header=4U|(1280U<<10)|(400U<<21);memcpy(out,&header,4);
 unsigned cw=image.width,ch=image.height;
 if(cw*400>ch*1280)cw=ch*1280/400;else ch=cw*400/1280;
 unsigned ox=(image.width-cw)/2,oy=(image.height-ch)/2;
 for(unsigned y=0;y<400;y++)for(unsigned x=0;x<1280;x++){
  unsigned src=((oy+y*ch/400)*image.width+ox+x*cw/1280)*4,dst=4+(y*1280+x)*4,a=rgba[src+3];
  for(unsigned k=0;k<3;k++)out[dst+k]=(rgba[src+2-k]*a+240*(255-a))/255;
  out[dst+3]=255;
 }
 char meta[160];snprintf(meta,sizeof(meta),"%s.source",paths[slot+3]+2);
 ok=atomic_write(meta,&source,sizeof(source))&&atomic_write(paths[slot+3]+2,out,4+PIXELS*4);
finish:png_image_free(&image);free(input);free(rgba);free(out);return ok;
}
static bool imported_source(unsigned source){for(unsigned i=0;i<6;i++){if(!standby_photo_exists(i))continue;char p[160];snprintf(p,sizeof(p),"%s.source",paths[i+3]+2);FILE*f=fopen(p,"rb");unsigned value=0;if(f){size_t n=fread(&value,1,sizeof(value),f);fclose(f);if(n==sizeof(value)&&value==source)return true;}}return false;}
static void* run(void*unused){(void)unused;bool ok;char message[160];
 if(job.deleting){ok=atomic_write(CFG,&job.cfg,sizeof(job.cfg));config_written=ok;if(ok){ok=unlink(paths[job.photo]+2)==0||errno==ENOENT;char meta[160];snprintf(meta,sizeof(meta),"%s.source",paths[job.photo]+2);if(ok)unlink(meta);}snprintf(message,sizeof(message),ok?"Imported photo deleted. Referenced layouts use daily rotation.":"Delete failed. Please try again.");}
 else if(job.importing){unsigned found=0,added=0,failed=0;
  if(!usb_storage_prepare()){ok=false;snprintf(message,sizeof(message),"USB is not available. Insert a drive and try again.");}
  else {for(unsigned i=1;i<=99;i++){
   char path[128];snprintf(path,sizeof(path),STANDBY_USB_DIRECTORY "/un260_delay_%02u.png",i);
   if(access(path,F_OK))continue;
   found++;if(imported_source(i)){failed++;continue;}
   unsigned slot=0;while(slot<6&&standby_photo_exists(slot))slot++;
   if(slot>=6){failed++;continue;}if(import_one(path,slot,i))added++;else failed++;
  }ok=added>0;snprintf(message,sizeof(message),"Found %u. Imported %u. Skipped %u. Maximum 6 photos.",found,added,failed);}
 }else {ok=atomic_write(CFG,&job.cfg,sizeof(job.cfg));snprintf(message,sizeof(message),ok?"Standby settings saved.":"Save failed. Previous settings were kept.");}
 pthread_mutex_lock(&lock);success=ok;snprintf(result,sizeof(result),"%s",message);done=true;pthread_mutex_unlock(&lock);return NULL;
}
bool standby_store_busy(void){return busy;}
static bool start(bool importing,const standby_config_t*c){if(busy)return false;if(c&&!standby_config_valid(c))return false;
 job.importing=importing;job.deleting=false;config_written=false;if(c)job.cfg=*c;done=false;busy=true;if(pthread_create(&thread,NULL,run,NULL)){busy=false;return false;}return true;}
bool standby_store_save(const standby_config_t*c){return start(false,c);}
bool standby_store_import(void){return start(true,NULL);}
bool standby_store_delete(unsigned photo){if(busy||photo<3||photo>8)return false;job.cfg=*standby_config();for(unsigned m=0;m<2;m++)for(unsigned i=0;i<3;i++)if(job.cfg.layout[m][i].photo==photo){job.cfg.layout[m][i].photo=1;job.cfg.layout[m][i].scheduled=1;}job.importing=false;job.deleting=true;job.photo=photo;done=false;busy=true;if(pthread_create(&thread,NULL,run,NULL)){busy=false;return false;}return true;}
bool standby_store_poll(char*message,unsigned capacity){if(!busy)return false;pthread_mutex_lock(&lock);bool ready=done;pthread_mutex_unlock(&lock);if(!ready)return false;
 pthread_join(thread,NULL);if((success&&!job.importing)||(job.deleting&&config_written))saved=job.cfg;snprintf(message,capacity,"%s",result);busy=false;return true;}
