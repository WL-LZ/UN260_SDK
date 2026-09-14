#!/usr/bin/env python3
"""Exercise production PID-1 control flow without mounting or touching devices."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
fixture=r'''
#define _GNU_SOURCE
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
static jmp_buf finish;
static const char *scenario;
static int root_tries,moves,execs,kills,unlinks;
static char commands[4096];
pid_t fake_getpid(void){return 1;}
pid_t fake_fork(void){return !strcmp(scenario,"fork-fail")?-1:123;}
int fake_mount(const char*a,const char*b,const char*c,unsigned long d,const void*e){
 (void)d;(void)e;
 if(!strcmp(b,"/newroot")){root_tries++;assert(!strcmp(a,"ubi0:rootfs")&&!strcmp(c,"ubifs"));
  if(!strcmp(scenario,"root-fail")||(!strcmp(scenario,"root-retry")&&root_tries<3)){errno=ENODEV;return -1;}}
 if(!strncmp(b,"/newroot/",9)){moves++;if(!strcmp(scenario,"move-fail")){errno=EINVAL;return -1;}}
 return 0;
}
int fake_open(const char*p,int f,...){(void)f;return !strcmp(p,"/proc/cmdline")?10:11;}
ssize_t fake_read(int fd,void*p,size_t n){assert(fd==10);size_t len=strlen(commands);assert(len<n);memcpy(p,commands,len);return len;}
int fake_close(int f){(void)f;return 0;}
int fake_dup2(int a,int b){(void)a;return b;}
int fake_mkdir(const char*a,mode_t b){(void)a;(void)b;return 0;}
int fake_chdir(const char*a){(void)a;return 0;}
int fake_chroot(const char*a){assert(!strcmp(a,"."));return 0;}
int fake_unlink(const char*a){assert(!strcmp(a,"/init"));unlinks++;return 0;}
int fake_usleep(useconds_t n){(void)n;return 0;}
int fake_access(const char*p,int m){(void)m;
 if(strstr(p,"/transaction"))return !strcmp(scenario,"recovery")?0:-1;
 if(strstr(p,"/pointercal"))return !strcmp(scenario,"calibration")?-1:0;
 if(strstr(p,"disabled"))return !strcmp(scenario,"disabled")?0:-1;
 if(strstr(p,".ready"))return !strcmp(scenario,"visual-timeout")?-1:0;
 return 0;
}
pid_t fake_waitpid(pid_t p,int*s,int o){(void)s;(void)o;return !strcmp(scenario,"visual-dead")?p:0;}
int fake_kill(pid_t p,int s){assert(p==123);(void)s;kills++;return 0;}
int fake_pause(void){longjmp(finish,2);return 0;}
int fake_dprintf(int fd,const char*f,...){(void)fd;(void)f;return 0;}
int fake_execv(const char*p,char*const*a){assert(!strcmp(p,"/linuxrc")&&!strcmp(a[0],p));execs++;longjmp(finish,1);return -1;}
int boot_light_run(void){assert(0);return 1;}
#define main init_main
#define getpid fake_getpid
#define fork fake_fork
#define mount fake_mount
#define open fake_open
#define read fake_read
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
#define execv fake_execv
#include "tools/un260-startup/early_init.c"
#undef main
int main(int argc,char**argv){assert(argc==2);scenario=argv[1];
 strcpy(commands,"quiet root=ubi0:rootfs rootfstype=ubifs init=/linuxrc");
 if(!strcmp(scenario,"cmdline-off"))strcat(commands," un260.boot_light=0");
 if(!strcmp(scenario,"missing-root"))strcpy(commands,"quiet");
 int result=setjmp(finish);if(!result)init_main();
 bool bad=!strcmp(scenario,"root-fail")||!strcmp(scenario,"move-fail")||!strcmp(scenario,"missing-root");
 assert(result==(bad?2:1));assert(execs==(bad?0:1));
 if(!bad){assert(moves==3&&unlinks==1);}
 if(!strcmp(scenario,"root-retry"))assert(root_tries==3);
 if(!strcmp(scenario,"root-fail"))assert(root_tries==300);
 if(!strcmp(scenario,"disabled")||!strcmp(scenario,"visual-timeout")||!strcmp(scenario,"calibration")||!strcmp(scenario,"recovery"))assert(kills>0);
 puts("PASS early init control-flow, no host mounts performed");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-early-test-') as d:
 p=Path(d);(p/'test.c').write_text(fixture)
 subprocess.run(['cc','-O1','-Wall','-Wextra','-Werror','-I'+str(root),'-I'+str(root/'source/artinchip/test-lvgl'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 for case in ['normal','fork-fail','root-retry','root-fail','disabled','visual-timeout','visual-dead','move-fail','missing-root','cmdline-off','calibration','recovery']:
  subprocess.run([str(p/'test'),case],check=True)
print('12 early init paths passed; physical boot remains board validation')
