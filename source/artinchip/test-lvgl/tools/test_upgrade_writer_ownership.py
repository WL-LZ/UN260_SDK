#!/usr/bin/env python3
"""Production GUI owner with real fork/exec/waitpid and competing status writers."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'un260/lv_core/ui_upgrade_service.c').read_text()


def function(name):
    match = re.search(r'^(?:static )?[\w \n*]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    start = source.index('{', match.start())
    depth = 0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]', source[start:], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth: return source[match.start():start + token.end()]
    raise AssertionError(name)


names = ('ui_upgrade_service_file_exists', 'ui_upgrade_service_hash_file_fnv1a64',
         'ui_upgrade_service_hash_cache_clear', 'ui_upgrade_service_hash_file_cached',
         'ui_upgrade_service_parse_hash', 'ui_upgrade_service_read_installed_bundle_hash',
         'ui_upgrade_service_get_package_hash_status', 'ui_upgrade_service_set_status',
         'ui_upgrade_service_stage_from_name', 'ui_upgrade_service_parse_int',
         'ui_upgrade_service_load_status_file', 'ui_upgrade_service_child_busy',
         'ui_upgrade_service_update_child_state', 'ui_upgrade_service_reset',
         'ui_upgrade_service_detect', 'ui_upgrade_service_can_reinstall',
         'ui_upgrade_service_launch', 'ui_upgrade_service_start',
         'ui_upgrade_service_prepare_data_poll', 'ui_upgrade_service_poll')
types = re.search(r'typedef struct \{.*?\} ui_upgrade_service_ctx_t;', source, re.S).group()
globals_ = source[source.index('static ui_upgrade_service_ctx_t g_ui_upgrade_service;'):
                  source.index('static bool ui_upgrade_service_file_exists')]
header = r'''
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "un260/lv_core/ui_upgrade_service.h"
#include "un260/storage/usb_storage.h"
#include "un260/app_service/upgrade_session.h"
#include "un260/app_service/app_command_runtime.h"
#include "un260/lv_system/ui_lang.h"
#include "un260/lv_system/ui_update_message.h"
#define UI_N_(s) (s)
static char package_path[512], installed_path[512], status_path[512], script_path[512];
#define UI_UPGRADE_BUNDLE_FILE_PATH package_path
#define UI_UPGRADE_INSTALLED_HASH_PATH installed_path
#define UI_UPGRADE_STATUS_FILE_PATH status_path
#define UI_UPGRADE_SCRIPT_PATH script_path
#define UI_UPGRADE_STATUS_MAX_SIZE 512U
#define UI_UPGRADE_DATA_WAIT_TIMEOUT_MS 30000U
void usb_storage_refresh(usb_storage_status_t *s){*s=(usb_storage_status_t){true,true};}
app_ui_upgrade_readiness_t app_command_runtime_prepare_ui_upgrade(void){return APP_UI_UPGRADE_READY;}
uint32_t app_clock_uptime_ms(void){return 0;}
language_t ui_lang_get(void){return LANGUAGE_EN;}
const char *ui_tr(const char *s){return s;}
void ui_update_message_render(language_t l,const char *s,char *out,size_t n){(void)l;snprintf(out,n,"%s",s?s:"");}
'''
test = r'''
static void write_file(const char *path,const char *data){FILE *f=fopen(path,"w");assert(f);assert(fputs(data,f)>=0);assert(!fclose(f));}
static void script(unsigned kind){
 char s[2048];
 const char *writer=kind==1?"1":"$$";
 const char *hash=kind==2?"0000000000000000":"$2";
 if(kind==4)snprintf(s,sizeof(s),"#!/bin/sh\necho 'Updater already running or stale lock' >&2\nexit 75\n");
 else snprintf(s,sizeof(s),"#!/bin/sh\nprintf 'progress=100\\nstage=success\\nstep=success\\nsuccess=1\\nmessage=owned success\\nwriter_pid=%%s\\nbundle_fnv64=%%s\\n' \"%s\" \"%s\" > '%s.tmp'\nmv '%s.tmp' '%s'\nsleep .08\nexit %u\n",writer,hash,status_path,status_path,status_path,kind==3?7U:0U);
 write_file(script_path,s);
}
static void reap(void){
 for(unsigned i=0;i<200&&g_ui_upgrade_service.child_pid>0;++i){ui_upgrade_service_poll(NULL);usleep(5000);}
 assert(g_ui_upgrade_service.child_pid<0&&upgrade_session_owner()==UPGRADE_SESSION_NONE);
}
int main(int argc,char **argv){
 assert(argc==2);
 snprintf(package_path,sizeof(package_path),"%s/UN260_UPDATE.upk",argv[1]);
 snprintf(installed_path,sizeof(installed_path),"%s/installed",argv[1]);
 snprintf(status_path,sizeof(status_path),"%s/status",argv[1]);
 snprintf(script_path,sizeof(script_path),"%s/dispatcher",argv[1]);
 write_file(package_path,"FORMAL PACKAGE");
 for(unsigned kind=0;kind<5;++kind){
  ui_upgrade_service_reset();script(kind);
  write_file(status_path,"progress=100\nstage=success\nstep=success\nsuccess=1\nmessage=external stale success\nwriter_pid=1\nbundle_fnv64=0000000000000000\n");
  ui_upgrade_detect_info_t detect;ui_upgrade_service_detect(&detect);
  g_ui_upgrade_pkg_hash_cache.hash=0; /* stale cache must not cross start boundary */
  assert(ui_upgrade_service_start()==UI_UPGRADE_START_OK);
  uint64_t actual;assert(ui_upgrade_service_hash_file_fnv1a64(package_path,&actual));
  assert(g_ui_upgrade_service.selected_hash==actual);
  ui_upgrade_service_status_t view;ui_upgrade_service_poll(&view);
  assert(view.running&&!view.finished&&!view.success&&upgrade_session_owner()==UPGRADE_SESSION_UI);
  assert(!strstr(view.result_text,"external stale"));
  assert(ui_upgrade_service_start()==UI_UPGRADE_START_BUSY);
  reap();ui_upgrade_service_poll(&view);
  assert(view.finished&&view.success==(kind==0));
  if(kind==1||kind==2)assert(strstr(view.result_text,"did not report a completed result"));
  if(kind==3)assert(strstr(view.result_text,"did not finish successfully"));
  if(kind==4){
   assert(!strcmp(view.result_text,"Wait for the update to finish."));
   char text[256];FILE *f=fopen(status_path,"r");assert(f);size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;fclose(f);
   assert(strstr(text,"external stale success")); /* GUI never removed active owner's state */
  }
 }
 ui_upgrade_service_reset();
 puts("PASS real GUI fork/exec/waitpid: own PID+FNV only, stale/mismatched status rejection, exit0-only success, busy75 preserves shared status, NULL global poll and fresh start hash");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-gui-writer-') as name:
    path = Path(name)
    code = path / 'test.c'
    code.write_text(header + types + '\n' + globals_ + '\n' + '\n'.join(function(n) for n in names) + test)
    exe = path / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-no-pie', '-I' + str(ROOT), str(code),
                    str(ROOT / 'un260/app_service/upgrade_session.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(path)], check=True)
