#include "un260/storage/protocol_log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
static bool usb_ok;
bool usb_storage_prepare(void){return usb_ok;}
static protocol_log_snapshot_t state(void){protocol_log_snapshot_t s;protocol_log_snapshot(&s);return s;}
static void wait_done(void){for(int n=0;n<1000;n++){if(!state().busy)return;usleep(1000);}assert(!"worker timeout");}
static char *read_file(const char *path){FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);char *s=calloc(n+1,1);assert(s);assert(fread(s,1,n,f)==(size_t)n);fclose(f);return s;}
int main(void){
 assert(protocol_log_init());wait_done();protocol_log_ports(4,5);
#ifdef TEST_LOG_LIMIT
 assert(protocol_log_start());wait_done();unsigned char excess[1024]={0};
 protocol_log_record(4,false,excess,sizeof(excess));
 for(unsigned i=0;i<1000&&(state().active||state().busy);i++)usleep(1000);
 assert(!state().active&&state().state==PLOG_ERROR&&state().dropped==sizeof(excess));
 char limit_path[512];snprintf(limit_path,sizeof(limit_path),"%s/protocol_logs/log_000000000001.txt",UI_STATE_DIR);
 char *limit_text=read_file(limit_path);assert(strstr(limit_text,"dropped=1024"));free(limit_text);
 protocol_log_shutdown();puts("PASS protocol logger limit: recording stops, queued loss is reported, local file is retained");return 0;
#endif
 uint8_t bytes[]={0xFD,0xDF,6,0xC0,4,0x0A};
 for(unsigned i=0;i<6;i++){
  assert(protocol_log_start());wait_done();assert(state().active);uint64_t id=state().current;
  assert(!protocol_log_delete(id));protocol_log_record(4,true,bytes,sizeof(bytes));protocol_log_record(5,false,bytes,sizeof(bytes));protocol_log_record(6,false,bytes,sizeof(bytes));
  assert(protocol_log_stop());wait_done();assert(!state().active&&state().state==PLOG_IDLE);
  char path[512];snprintf(path,sizeof(path),"%s/protocol_logs/log_%012llu.txt",UI_STATE_DIR,(unsigned long long)id);char *s=read_file(path);assert(strstr(s,"UART4 TX FD DF 06 C0 04 0A")&&strstr(s,"UART5 RX FD DF 06 C0 04 0A")&&!strstr(s,"UART6"));free(s);
 }
 protocol_log_snapshot_t s=state();assert(s.count==5&&s.files[0].id==6&&s.files[4].id==2);
 assert(protocol_log_export(6));wait_done();assert(state().state==PLOG_ERROR&&state().count==5);
 usb_ok=true;assert(protocol_log_export(6));wait_done();assert(state().state==PLOG_EXPORTED);
 char target[512];snprintf(target,sizeof(target),"%s/protocol_000000000006_000.txt",PROTOCOL_LOG_USB_DIR);char *body=read_file(target);assert(strstr(body,"UART4 TX FD DF 06 C0 04 0A"));free(body);
 assert(protocol_log_export(6));wait_done();snprintf(target,sizeof(target),"%s/protocol_000000000006_001.txt",PROTOCOL_LOG_USB_DIR);body=read_file(target);free(body);
 assert(protocol_log_delete(3));wait_done();assert(state().count==4);
 assert(protocol_log_start());wait_done();protocol_log_record(4,false,bytes,sizeof(bytes));assert(protocol_log_export(state().current));wait_done();assert(!state().active&&state().state==PLOG_EXPORTED);
 protocol_log_shutdown();assert(protocol_log_init());wait_done();assert(state().count==5&&!state().active);
 protocol_log_shutdown();puts("PASS protocol logger: real worker/file I/O, RX/TX, excluded log UART, five-file rotation, export-stop, USB failure/retry, collision-safe filenames, deletion and restart");
}
