#!/usr/bin/env python3
"""Production PID 1 paths with mocked syscalls; never mount or boot the host."""
from pathlib import Path
import hashlib
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
fixture = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "un260/boot/boot_bundle.h"
static jmp_buf finish;
static const char *scenario;
static int roots,moves,root_moves,chroots,forks,execs,kills,opens,closes,marker_reads;
static int phase,waits,attempt,theme_flags,unlinks,pin_opens,pin_writes,pin_renames,pinned;
static int platform_opens,platform_written,platform_renamed;
static int request_opens,request_closes;
static size_t pin_length;
static char pin_record[256];
static char commands[4096];
static bool is_case(const char *name){return !strcmp(scenario,name)||(!strncmp(scenario,"cli-",4)&&!strcmp(scenario+4,name));}
pid_t fake_getpid(void){return !strcmp(scenario,"not-pid1")?22:1;}
pid_t fake_fork(void){
#ifndef UN260_BOOT_LAUNCH_ONLY
 assert(roots&&moves==3&&root_moves==1&&chroots==1&&phase==4);
#else
 assert(phase==4&&roots==0&&moves==0&&chroots==0&&platform_opens==0);
#endif
 forks++;attempt++;
 if(!strcmp(scenario,"fork-fail")){errno=EAGAIN;return -1;}
 if(!strcmp(scenario,"child-exec-fail"))return 0;
 return 123+attempt;
}
int fake_mount(const char *a,const char *b,const char *c,unsigned long flags,const void *options){
 if(!strcmp(b,"/newroot")) {
  roots++;assert(!strcmp(a,"ubi0:rootfs")&&!strcmp(c,"ubifs")&&flags==MS_RDONLY);
  if(!strcmp(scenario,"rootflags"))assert(options&&!strcmp(options,"bulk_read"));
  if(!strcmp(scenario,"root-fail")||(!strcmp(scenario,"root-retry")&&roots<3)){errno=ENODEV;return -1;}
  phase=1;
 }
 if(!strncmp(b,"/newroot/",9)) {
  assert(flags==MS_MOVE&&phase>=1);moves++;phase=2;
  if(!strcmp(scenario,"move-fail")){errno=EINVAL;return -1;}
 }
 if(!strcmp(b,"/")){assert(!strcmp(a,".")&&moves==3&&flags==MS_MOVE);root_moves++;phase=3;}
 return 0;
}
int fake_open(const char *p,int flags,...){
 if(!strcmp(p,"/etc/un260/unified-request")||!strcmp(p,"/var/lib/un260-updater/unified.completed")) {
  assert(phase==4&&forks==0&&flags==(O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
  bool receipt=strstr(p,"unified.completed")!=NULL;
  if(strncmp(scenario,"request-",8)||!strcmp(scenario,"request-absent")||
     (receipt&&!strcmp(scenario,"request-receipt-missing"))){errno=ENOENT;return -1;}
  if((!receipt&&!strcmp(scenario,"request-symlink"))||(receipt&&!strcmp(scenario,"request-receipt-symlink"))){errno=ELOOP;return -1;}
  if(!receipt&&!strcmp(scenario,"request-open-error")){errno=EIO;return -1;}
  request_opens++;return receipt?18:17;
 }
 if(!strcmp(p,"/proc/cmdline"))return 10;
 if(!strcmp(p,BOOT_BUNDLE_READY_PATH)) {
  marker_reads++;
#ifdef UN260_BOOT_LAUNCH_ONLY
  if(!attempt&&strncmp(scenario,"cli-owner-",10)&&strncmp(scenario,"cli-stale-",10)){errno=ENOENT;return -1;}
#else
  assert(waits>0);
#endif
  if(is_case("visual-timeout")||(is_case("active-timeout")&&attempt==1)){errno=ENOENT;return -1;}
  return 11;
 }
 if(!strcmp(p,"/dev/.un260-early.log"))return 12;
 if(!strcmp(p,"/dev/.un260-boot-platform.1.new")) {
  assert(phase==4&&forks==0);platform_opens++;
  assert(flags==(O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC));
  if(!strcmp(scenario,"platform-open-fail")){errno=EACCES;return -1;}
  return 15;
 }
 if(!strcmp(p,"/proc/5555/comm"))return 16;
 if(!strcmp(p,BOOT_BUNDLE_LIVE_PATH ".1.new")) {
  assert(phase==4&&marker_reads>0&&waits>0);
  assert(flags==(O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC));
  va_list args;va_start(args,flags);assert(va_arg(args,int)==0600);va_end(args);
  pin_opens++;pin_length=0;
  if(!strcmp(scenario,"pin-open-fail")||(!strcmp(scenario,"active-pin-open-fail")&&attempt==1)) {
   errno=EACCES;return -1;
  }
  return 14;
 }
 assert(!strcmp(p,"/dev/console"));return 13;
}
int fake_fstat(int fd,struct stat *st){
 assert(fd==17||fd==18);memset(st,0,sizeof(*st));st->st_mode=S_IFREG|0644;st->st_uid=0;st->st_size=fd==17?130:65;
 const char *prefix=fd==17?"request-":"request-receipt-";
 const char *issue=!strncmp(scenario,prefix,strlen(prefix))?scenario+strlen(prefix):"";
 if(!strcmp(issue,"not-regular"))st->st_mode=S_IFDIR|0755;
 if(!strcmp(issue,"foreign-owner"))st->st_uid=1;
 if(!strcmp(issue,"group-write"))st->st_mode|=0020;
 if(!strcmp(issue,"setuid"))st->st_mode|=S_ISUID;
 if(!strcmp(issue,"setgid"))st->st_mode|=S_ISGID;
 if(!strcmp(issue,"truncated"))st->st_size--;
 if(!strcmp(issue,"extra-byte"))st->st_size++;
 if(!strcmp(issue,"stat-error")){errno=EIO;return -1;}
 return 0;
}
ssize_t fake_pread(int fd,void *output,size_t size,off_t offset){
 assert((fd==17||fd==18)&&offset==0);char text[130];
 if(fd==17){assert(size==130);memset(text,'a',64);text[64]='\n';memset(text+65,'b',64);text[129]='\n';}
 else {assert(size==65);memset(text,!strcmp(scenario,"request-pending")?'d':'c',64);text[64]='\n';}
 if((fd==17&&!strcmp(scenario,"request-invalid-hex"))||(fd==18&&!strcmp(scenario,"request-receipt-invalid-hex")))text[2]='z';
 if(fd==17&&!strcmp(scenario,"request-uppercase"))text[70]='B';
 if((fd==17&&!strcmp(scenario,"request-no-lf"))||(fd==18&&!strcmp(scenario,"request-receipt-no-lf")))text[size-1]='x';
 if(fd==17&&!strcmp(scenario,"request-first-line-no-lf"))text[64]='x';
 if((fd==17&&!strcmp(scenario,"request-read-error"))||(fd==18&&!strcmp(scenario,"request-receipt-read-error"))){errno=EIO;return -1;}
 if((fd==17&&!strcmp(scenario,"request-read-short"))||(fd==18&&!strcmp(scenario,"request-receipt-read-short")))size--;
 memcpy(output,text,size);return size;
}
bool boot_bundle_sha256_fd(int fd,char digest[65]){
 assert(fd==17);if(!strcmp(scenario,"request-sha-fail"))return false;
 memset(digest,'c',64);digest[64]=0;return true;
}
ssize_t fake_read(int fd,void *p,size_t size){
 if(fd==10){size_t n=strlen(commands);assert(n<size);memcpy(p,commands,n);return n;}
 if(fd==16){const char *comm=!strcmp(scenario,"cli-stale-comm")?"other\n":"un260-boot\n";size_t n=strlen(comm);assert(n<size);memcpy(p,comm,n);return n;}
 assert(fd==11);char marker[32];
#ifdef UN260_BOOT_LAUNCH_ONLY
 if(!attempt) {
  const char *text="5555\n";
  if(!strcmp(scenario,"cli-stale-junk"))text="5555\njunk";
  if(!strcmp(scenario,"cli-stale-negative"))text="-5555\n";
  if(!strcmp(scenario,"cli-stale-overflow"))text="999999999999999999999999\n";
  if(!strcmp(scenario,"cli-stale-one"))text="1\n";
  size_t n=strlen(text);assert(n<size);memcpy(p,text,n);return n;
 }
#endif
 int n=snprintf(marker,sizeof(marker),"%d\n",!strcmp(scenario,"wrong-marker")?999:123+attempt);
 if(!strcmp(scenario,"marker-junk"))n=snprintf(marker,sizeof(marker),"%d\njunk",123+attempt);
 if(!strcmp(scenario,"marker-negative"))n=snprintf(marker,sizeof(marker),"-%d\n",123+attempt);
 if(!strcmp(scenario,"marker-overflow"))n=snprintf(marker,sizeof(marker),"999999999999999999999999999\n");
 if(!strcmp(scenario,"marker-empty"))return 0;
 assert((size_t)n<size);memcpy(p,marker,n);return n;
}
ssize_t fake_write(int fd,const void *data,size_t size){
 if(fd==15) {
  assert(platform_opens==1&&phase==4&&forks==0);
  const char *record="UN260_BOOT_PLATFORM_V1\n";assert(size==strlen(record)&&!memcmp(data,record,size));
  if(!strcmp(scenario,"platform-write-fail")){errno=ENOSPC;return -1;}
  platform_written++;return size;
 }
 assert(fd==14&&pin_opens>0);pin_writes++;
 if(!strcmp(scenario,"pin-write-fail")||(!strcmp(scenario,"active-pin-write-fail")&&attempt==1)){errno=ENOSPC;return -1;}
 if(!strcmp(scenario,"pin-zero-write"))return 0;
 if(!strcmp(scenario,"pin-eintr")&&pin_writes==1){errno=EINTR;return -1;}
 if(!strcmp(scenario,"pin-short-write")&&size>11)size=11;
 assert(pin_length+size<sizeof(pin_record));memcpy(pin_record+pin_length,data,size);pin_length+=size;
 pin_record[pin_length]=0;return size;
}
int fake_rename(const char *from,const char *to){
 if(!strcmp(to,"/dev/.un260-boot-platform")) {
  assert(!strcmp(from,"/dev/.un260-boot-platform.1.new")&&platform_written==1);
  if(!strcmp(scenario,"platform-rename-fail")){errno=EROFS;return -1;}
  platform_renamed++;return 0;
 }
 assert(!strcmp(from,BOOT_BUNDLE_LIVE_PATH ".1.new")&&!strcmp(to,BOOT_BUNDLE_LIVE_PATH));
 char expected[256];char id[65],hash[65];memset(id,'a',64);id[64]=0;memset(hash,'b',64);hash[64]=0;
 snprintf(expected,sizeof(expected),"%s\n%s\n%s\nnone\nnone\n",BOOT_BUNDLE_ACTIVE_MAGIC,id,hash);
 assert(!strcmp(pin_record,expected));pin_renames++;
 if(!strcmp(scenario,"pin-rename-fail")||(!strcmp(scenario,"active-pin-rename-fail")&&attempt==1)){errno=EROFS;return -1;}
 pinned++;return 0;
}
int fake_close(int fd){
 assert(fd>=10&&fd<=22);closes++;
 if(fd==17||fd==18)request_closes++;
 if(fd==14&&(!strcmp(scenario,"pin-close-fail")||(!strcmp(scenario,"active-pin-close-fail")&&attempt==1))) {
  errno=EIO;return -1;
 }
 return 0;
}
int fake_dup2(int a,int b){assert(a==12||a==13);return b;}
int fake_mkdir(const char *p,mode_t mode){(void)p;(void)mode;return 0;}
int fake_chdir(const char *p){assert(!strcmp(p,"/newroot")||!strcmp(p,"/"));return 0;}
int fake_chroot(const char *p){assert(!strcmp(p,".")&&phase==3);chroots++;phase=4;return 0;}
int fake_unlink(const char *p){
 if(!strcmp(p,"/init")){unlinks++;return 0;}
 if(!strcmp(p,"/dev/.un260-boot-platform.1.new"))return 0;
 assert(!strcmp(p,BOOT_BUNDLE_LIVE_PATH ".1.new"));return 0;
}
int fake_usleep(useconds_t n){assert(n==10000||n==100000);return 0;}
int fake_access(const char *p,int flags){
 assert(flags==F_OK);
 if(!strncmp(p,"/newroot/",9))return !strcmp(scenario,"missing-mountpoints")?-1:0;
 assert(phase==4);
 if(strstr(p,"transaction"))return is_case("transaction")?0:-1;
 if(strstr(p,"pointercal"))return is_case("calibration")?-1:0;
 if(strstr(p,"boot-light.disabled"))return is_case("disabled")?0:-1;
 if(strstr(p,"early-display.disabled"))return is_case("early-disabled")?0:-1;
 assert(0);return -1;
}
pid_t fake_waitpid(pid_t child,int *status,int options){
 (void)status;assert(child==123+attempt);waits++;
 if(options==0)return child;
 assert(options==WNOHANG);
 if(is_case("visual-dead")||(is_case("active-dead")&&attempt==1))return child;
 return 0;
}
int fake_kill(pid_t child,int sig){
 if(sig==0){assert(child==5555);if(!strcmp(scenario,"cli-stale-dead")){errno=ESRCH;return -1;}return 0;}
 assert(child==123+attempt&&(sig==SIGTERM||sig==SIGKILL));kills++;return 0;
}
int fake_pause(void){longjmp(finish,2);return 0;}
int fake_dprintf(int fd,const char *format,...){(void)fd;(void)format;return 0;}
int fake_fcntl(int fd,int command,...){
 assert(fd==22);
 if(command==F_GETFD)return FD_CLOEXEC;
 assert(command==F_SETFD);va_list args;va_start(args,command);theme_flags=va_arg(args,int);va_end(args);return 0;
}
int fake_fexecve(int fd,char *const args[],char *const env[]){
 (void)env;assert(fd==21&&!strcmp(args[0],"un260_boot_player")&&!strcmp(args[1],"--theme-fd"));
 assert(!strcmp(args[2],"22")&&!strcmp(args[3],"--bundle-id")&&strlen(args[4])==64);
 assert(!strcmp(args[5],"--mode")&&!strcmp(args[6],"animation")&&!args[7]);assert(theme_flags==0);
 errno=ENOEXEC;return -1;
}
void fake_exit(int code){assert(code==127);longjmp(finish,3);}
int fake_execv(const char *p,char *const args[]){
 assert(phase==4&&!strcmp(p,"/linuxrc")&&!strcmp(args[0],p));
 assert((getenv("UN260_BOOT_UNIFIED_ACTIVE")!=NULL)==(pinned==1));execs++;longjmp(finish,1);return -1;
}
bool boot_bundle_open(const char *a,const char *v,bool previous_only,boot_bundle_t *b){
 assert(phase==4&&!strcmp(a,BOOT_BUNDLE_ACTIVE_PATH)&&!strcmp(v,BOOT_BUNDLE_VERSIONS_PATH));opens++;
 memset(b,0,sizeof(*b));b->player_fd=b->theme_fd=-1;
 if(is_case("bundle-missing")||!strcmp(scenario,"cli-check-missing"))return false;
 if(opens==2)assert(previous_only);
 b->previous=previous_only||!strcmp(scenario,"selected-previous");
 memset(b->id,'a',64);memset(b->manifest_sha256,'b',64);strcpy(b->mode,"animation");b->player_fd=21;b->theme_fd=22;return true;
}
void boot_bundle_close(boot_bundle_t *b){assert(b->player_fd==21&&b->theme_fd==22);b->player_fd=b->theme_fd=-1;}
#define main init_main
#define getpid fake_getpid
#define fork fake_fork
#define mount fake_mount
#define open fake_open
#define read fake_read
#define pread fake_pread
#define fstat fake_fstat
#define write fake_write
#define rename fake_rename
#define close fake_close
#define dup2 fake_dup2
#define mkdir fake_mkdir
#define chdir fake_chdir
#define chroot fake_chroot
#define unlink fake_unlink
#define usleep fake_usleep
#define access fake_access
#define waitpid fake_waitpid
#define kill fake_kill
#define pause fake_pause
#define dprintf fake_dprintf
#define fcntl fake_fcntl
#define fexecve fake_fexecve
#define _exit fake_exit
#define execv fake_execv
#include "tools/un260-startup/early_init.c"
#undef main
#ifdef UN260_BOOT_LAUNCH_ONLY
int main(int argc,char **argv){
 assert(argc==2);scenario=argv[1];phase=4;
 char *args[]={"un260_boot_launch",NULL,NULL};int count=1;
 if(!strncmp(scenario,"cli-check",9)){args[1]="--check";count=2;}
 if(!strcmp(scenario,"cli-invalid-option")){args[1]="--unknown";count=2;}
 int result=init_main(count,args);
 bool off=is_case("disabled")||is_case("early-disabled")||is_case("calibration")||is_case("transaction");
 bool check=!strncmp(scenario,"cli-check",9),owner=!strncmp(scenario,"cli-owner-",10);
 int expected=!strcmp(scenario,"cli-invalid-option")?2:off||is_case("bundle-missing")||is_case("visual-timeout")||!strcmp(scenario,"cli-check-missing")?1:0;
 assert(result==expected&&platform_opens==0&&platform_written==0&&platform_renamed==0&&roots==0&&moves==0&&chroots==0);
 assert(request_opens==0); /* CLI ignores pending requests after verified-current recovery. */
 if(off||owner||check||!strcmp(scenario,"cli-invalid-option"))assert(forks==0&&pinned==0);
 else if(!is_case("bundle-missing"))assert(forks==((is_case("active-timeout")||is_case("active-dead")||is_case("visual-timeout"))?2:1)&&pinned==(expected?0:1));
 if(owner)assert(opens==0);
 puts("PASS isolated root launcher flow");return 0;
}
#else
int main(int argc,char **argv){
 assert(argc==2);scenario=argv[1];strcpy(commands,"quiet root=ubi0:rootfs rootfstype=ubifs init=/linuxrc");
 unsetenv("UN260_BOOT_UNIFIED_ACTIVE");
 if(!strcmp(scenario,"not-pid1")){assert(init_main()==1&&roots==0&&forks==0);return 0;}
 if(!strcmp(scenario,"cmdline-off"))strcat(commands," un260.boot_light=0");
 if(!strcmp(scenario,"rootflags"))strcat(commands," rootflags=bulk_read");
 if(!strcmp(scenario,"frame-off"))strcat(commands," un260.boot_frames=0");
 if(!strcmp(scenario,"frame-on"))strcat(commands," un260.boot_frames=1");
 if(!strcmp(scenario,"missing-root"))strcpy(commands,"quiet");
 int outcome=setjmp(finish);if(!outcome)init_main();
 bool fatal_path=!strcmp(scenario,"root-fail")||!strcmp(scenario,"move-fail")||
   !strcmp(scenario,"missing-root")||!strcmp(scenario,"missing-mountpoints");
 assert(outcome==(!strcmp(scenario,"child-exec-fail")?3:fatal_path?2:1));
 assert(execs==(fatal_path||outcome==3?0:1));
 if(!fatal_path){assert(moves==3&&root_moves==1&&chroots==1&&unlinks==1);}
 assert(platform_opens==(fatal_path?0:1));
 assert(request_opens==request_closes);
 if(!strncmp(scenario,"request-",8)&&strcmp(scenario,"request-absent")&&strcmp(scenario,"request-completed"))assert(forks==0&&opens==0&&pinned==0);
 if(!strcmp(scenario,"request-completed")||!strcmp(scenario,"request-absent"))assert(forks==1&&pinned==1);
 if(!fatal_path&&!strncmp(scenario,"platform-",9))assert(platform_renamed==0&&execs==1&&pinned==1);
 if(!strcmp(scenario,"root-retry"))assert(roots==3);
 if(!strcmp(scenario,"root-fail"))assert(roots==300);
 if(!strcmp(scenario,"disabled")||!strcmp(scenario,"early-disabled")||!strcmp(scenario,"cmdline-off")||
    !strcmp(scenario,"calibration")||!strcmp(scenario,"transaction"))assert(forks==0&&opens==0&&kills==0);
 if(!strcmp(scenario,"active-timeout")||!strcmp(scenario,"active-dead")||!strcmp(scenario,"visual-timeout")||
    !strcmp(scenario,"visual-dead")||!strcmp(scenario,"fork-fail"))assert(forks==2&&opens==2);
 if(!strcmp(scenario,"selected-previous"))assert(forks==1&&opens==1);
 if(!strcmp(scenario,"visual-dead")||!strcmp(scenario,"active-dead"))assert(marker_reads<2);
 if(!strncmp(scenario,"active-pin-",11))assert(forks==2&&opens==2&&pinned==1&&kills>0);
 if(!strncmp(scenario,"pin-",4)&&strcmp(scenario,"pin-short-write")&&strcmp(scenario,"pin-eintr"))
  assert(forks==2&&opens==2&&pinned==0&&kills>0);
 if(!strcmp(scenario,"pin-short-write"))assert(pin_writes>10&&pinned==1);
 if(!strcmp(scenario,"pin-eintr"))assert(pin_writes==2&&pinned==1);
 if(!strcmp(scenario,"visual-timeout")||!strcmp(scenario,"visual-dead")||!strncmp(scenario,"marker-",7))assert(pin_opens==0);
 if(!strcmp(scenario,"frame-off"))assert(!strcmp(getenv("UN260_BOOT_FRAME_TRACE"),"0"));
 if(!strcmp(scenario,"frame-on"))assert(!strcmp(getenv("UN260_BOOT_FRAME_TRACE"),"1"));
 puts("PASS isolated PID1 control flow");return 0;
}
#endif
'''
cases = ['normal','not-pid1','root-retry','root-fail','rootflags','move-fail',
         'missing-root','missing-mountpoints','disabled','early-disabled',
         'cmdline-off','calibration','transaction','bundle-missing','fork-fail',
         'active-timeout','active-dead','visual-timeout','visual-dead',
         'wrong-marker','marker-junk','marker-negative','marker-overflow','marker-empty',
         'selected-previous','child-exec-fail','frame-off','frame-on',
         'active-pin-open-fail','active-pin-write-fail','active-pin-close-fail','active-pin-rename-fail',
         'pin-open-fail','pin-write-fail','pin-zero-write','pin-close-fail','pin-rename-fail',
         'pin-short-write','pin-eintr','platform-open-fail','platform-write-fail','platform-rename-fail',
         'request-absent','request-completed','request-pending','request-receipt-missing',
         'request-symlink','request-open-error','request-invalid-hex','request-uppercase',
         'request-no-lf','request-first-line-no-lf','request-read-short','request-read-error','request-sha-fail',
         'request-receipt-symlink','request-receipt-invalid-hex','request-receipt-no-lf',
         'request-receipt-read-short','request-receipt-read-error']
for prefix in ('request-','request-receipt-'):
    cases += [prefix+issue for issue in ('not-regular','foreign-owner','group-write','setuid','setgid','truncated','extra-byte','stat-error')]
cli_cases=['cli-normal','cli-check','cli-check-missing','cli-invalid-option',
           'cli-disabled','cli-early-disabled','cli-calibration','cli-transaction','cli-bundle-missing',
           'cli-owner-valid','cli-active-timeout','cli-active-dead','cli-visual-timeout',
           'cli-stale-dead','cli-stale-comm','cli-stale-junk',
           'cli-stale-negative','cli-stale-overflow','cli-stale-one','cli-request-pending']
with tempfile.TemporaryDirectory(prefix='un260-pid1-isolated-') as directory:
    tmp=Path(directory)
    (tmp/'test.c').write_text(fixture)
    subprocess.run(['cc','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=all','-I'+str(root),
                    '-I'+str(root/'source/artinchip/test-lvgl'),str(tmp/'test.c'),
                    '-o',str(tmp/'test')],check=True)
    for case in cases:
        subprocess.run([str(tmp/'test'),case],check=True)
    subprocess.run(['cc','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=all','-DUN260_BOOT_LAUNCH_ONLY=1','-I'+str(root),
                    '-I'+str(root/'source/artinchip/test-lvgl'),str(tmp/'test.c'),
                    '-o',str(tmp/'launch-test')],check=True)
    for case in cli_cases:
        subprocess.run([str(tmp/'launch-test'),case],check=True)
print(f'PASS {len(cases)} production PID1 and {len(cli_cases)} root CLI paths; every mount/exec/device operation mocked')

# Also run the actual private pending predicate against real temporary files
# and the production SHA implementation. Only paths and fstat UID are adapted
# for the unprivileged build host; no PID1 entry point is invoked.
request_fixture=r'''
#define _GNU_SOURCE
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fixture_open(const char *path,int flags,...) {
 const char *name=NULL;
 if(!strcmp(path,"/etc/un260/unified-request"))name="request";
 if(!strcmp(path,"/var/lib/un260-updater/unified.completed"))name="receipt";
 assert(name);char mapped[4096];snprintf(mapped,sizeof(mapped),"%s/%s",getenv("REQUEST_ROOT"),name);
 return open(mapped,flags);
}
static int fixture_root_stat(int fd,struct stat *st){int rc=fstat(fd,st);if(!rc)st->st_uid=0;return rc;}
#define open fixture_open
#define fstat fixture_root_stat
#define main unused_pid1_entry
#include "tools/un260-startup/early_init.c"
#undef main
static unsigned fds(void){
 DIR*d=opendir("/proc/self/fd");assert(d);unsigned n=0;struct dirent*e;
 while((e=readdir(d)))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))n++;
 closedir(d);return n;
}
int main(int argc,char**argv){assert(argc==2);unsigned before=fds();
 assert(unified_request_pending()==!strcmp(argv[1],"pending"));assert(fds()==before);return 0;}
'''
with tempfile.TemporaryDirectory(prefix='un260-request-readonly-') as directory:
    tmp=Path(directory);source=tmp/'request.c';source.write_text(request_fixture);exe=tmp/'request-check'
    subprocess.run(['cc','-O1','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                    '-fno-sanitize-recover=all','-I'+str(root),'-I'+str(root/'source/artinchip/test-lvgl'),
                    str(source),str(root/'source/artinchip/test-lvgl/un260/boot/boot_bundle.c'),'-o',str(exe)],check=True)
    request=b'a'*64+b'\n'+b'b'*64+b'\n'
    receipt=hashlib.sha256(request).hexdigest().encode()+b'\n'
    real_cases=[('absent',False),('completed',False),('missing-receipt',True),
                ('changed-request',True),('request-symlink',True),('receipt-symlink',True),
                ('request-no-lf',True),('receipt-no-lf',True),('request-extra-byte',True),
                ('request-group-write',True),('receipt-group-write',True),('receipt-wrong-sha',True)]
    for name,pending in real_cases:
        case=tmp/name;case.mkdir()
        if name!='absent':
            (case/'request').write_bytes(request);(case/'request').chmod(0o644)
        if name not in ('absent','missing-receipt'):
            (case/'receipt').write_bytes(receipt);(case/'receipt').chmod(0o644)
        if name=='changed-request':(case/'request').write_bytes(b'c'*64+b'\n'+b'b'*64+b'\n')
        if name.endswith('symlink'):
            kind=name.split('-')[0];(case/kind).rename(case/'saved');(case/kind).symlink_to(case/'saved')
        if name.endswith('no-lf'):
            kind=name.split('-')[0];path=case/kind;path.write_bytes(path.read_bytes()[:-1]+b'x')
        if name=='request-extra-byte':(case/'request').write_bytes(request+b'\n')
        if name.endswith('group-write'):(case/name.split('-')[0]).chmod(0o664)
        if name=='receipt-wrong-sha':(case/'receipt').write_bytes(b'd'*64+b'\n')
        subprocess.run([str(exe),'pending' if pending else 'completed'],
                       env=dict(os.environ,REQUEST_ROOT=str(case)),check=True)
    print(f'PASS {len(real_cases)} real temporary request/receipt/SHA/FD cases; no PID1 entry/mount/exec invoked')
