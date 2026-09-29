#!/usr/bin/env python3
"""Feed actual shell write_status output into the production C status reader."""
from pathlib import Path
import re, subprocess, tempfile

root=Path(__file__).resolve().parents[1]
repo=root.parents[2]
source=(root/'un260/lv_core/ui_upgrade_service.c').read_text()
script=(repo/'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh').read_text()
def function(name):
    m=re.search(r'^static [^;{}\n]+\b'+name+r'\([^;{}]*\)\s*\{.*?^\}',source,re.M|re.S)
    assert m,name
    return m.group()
ctx=re.search(r'typedef struct \{\s*bool running;.*?\} ui_upgrade_service_ctx_t;',source,re.S).group()
writer=script[script.index('write_status()\n{'):]
writer=writer[:writer.index('\n}')+2]
header='''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include "un260/lv_core/ui_upgrade_service.h"
#define UI_N_(s) (s)
#define UI_UPGRADE_STATUS_MAX_SIZE 512U
static char status_path[1024];
#define UI_UPGRADE_STATUS_FILE_PATH status_path
'''+ctx+'\nstatic ui_upgrade_service_ctx_t g_ui_upgrade_service;\n'
helpers='\n'.join(function(n) for n in ('ui_upgrade_service_set_status','ui_upgrade_service_stage_from_name','ui_upgrade_service_parse_int','ui_upgrade_service_load_status_file'))
test=r'''
static void load(const char *dir,const char *name){
 snprintf(status_path,sizeof(status_path),"%s/%s",dir,name);ui_upgrade_service_load_status_file();
}
static void reset(void){
 memset(&g_ui_upgrade_service,0,sizeof(g_ui_upgrade_service));
 g_ui_upgrade_service.running=true;
 ui_upgrade_service_set_status(true,false,false,0,UI_UPGRADE_STAGE_PREPARE,"prepare","");
}
int main(int argc,char **argv){
 assert(argc==2);reset();
 const int progress[]={2,5,10,18,24,34,60,90,92,96};
 const ui_upgrade_stage_t stages[]={UI_UPGRADE_STAGE_PREPARE,UI_UPGRADE_STAGE_VERIFY,
 UI_UPGRADE_STAGE_EXTRACT,UI_UPGRADE_STAGE_VERIFY,UI_UPGRADE_STAGE_VERIFY,
 UI_UPGRADE_STAGE_PREFLIGHT,UI_UPGRADE_STAGE_INSTALL,UI_UPGRADE_STAGE_INSTALL,
 UI_UPGRADE_STAGE_SYNC,UI_UPGRADE_STAGE_FINISH};
 for(unsigned i=0;i<sizeof(progress)/sizeof(progress[0]);++i){
  char name[32];snprintf(name,sizeof(name),"step%u",i);load(argv[1],name);
  assert(g_ui_upgrade_service.status.progress==progress[i]);
  assert(g_ui_upgrade_service.status.stage==stages[i]);
  assert(g_ui_upgrade_service.status.running&&!g_ui_upgrade_service.status.finished);
  ui_upgrade_service_ctx_t before=g_ui_upgrade_service;load(argv[1],name);
  assert(!memcmp(&before,&g_ui_upgrade_service,sizeof(before)));
 }
 load(argv[1],"success");assert(g_ui_upgrade_service.status.progress==100&&g_ui_upgrade_service.status.finished&&g_ui_upgrade_service.status.success);
 reset();load(argv[1],"step6");load(argv[1],"failure");
 assert(g_ui_upgrade_service.status.progress==34&&g_ui_upgrade_service.status.finished&&!g_ui_upgrade_service.status.success);
 reset();load(argv[1],"step6");
 ui_upgrade_service_ctx_t before=g_ui_upgrade_service;
 for(unsigned i=0;i<8;++i){char name[32];snprintf(name,sizeof(name),"invalid%u",i);load(argv[1],name);assert(!memcmp(&before,&g_ui_upgrade_service,sizeof(before)));}
 load(argv[1],"missing");assert(g_ui_upgrade_service.status.progress==70&&!g_ui_upgrade_service.status.finished);
 load(argv[1],"step6");assert(g_ui_upgrade_service.status.progress==70); /* cannot move backwards */
 puts("PASS actual shell -> C: all intermediate stages, blank/absent pending result, completion/failure, malformed/partial rejection, unchanged cache, monotonic progress");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-upgrade-progress-') as temp:
    w=Path(temp)
    cases=[(2,'prepare','prepare',''),(5,'verify','verify_archive',''),(10,'extract','extract',''),
           (18,'verify','verify_manifest',''),(24,'verify','verify_checksum',''),(34,'preflight','preflight',''),
           (60,'install','install',''),(90,'install','install',''),(92,'sync','sync',''),(96,'finish','finish','')]
    shell=writer+'\nLAST_STAGE=none\nPHASE_TIME=0\nSTART_TIME=0\nLOG="$1/log"\nSTATUS_TEMP="$1/temp"\n'
    for name,(p,stage,step,result) in [*((f'step{i}',c) for i,c in enumerate(cases)),('success',(100,'success','success','1')),('failure',(34,'fail','fail','0'))]:
        shell+=f'STATUS_FILE="$1/{name}"\nwrite_status {p} {stage} {step} "{result}" "" || exit 1\n'
    subprocess.run(['sh','-c',shell,'writer',str(w)],check=True)
    base=(w/'step6').read_text()
    bad=[base.replace('success=','success=bad'),base.replace('success=','success=2'),
         base.replace('stage=install','stage=success'),base.replace('stage=install','stage=fail'),
         base.replace('success=','success=1'),base.replace('success=','success=0'),
         base.rstrip('\n'),base.replace('progress=60','progress=101')]
    for i,text in enumerate(bad):(w/f'invalid{i}').write_text(text)
    (w/'missing').write_text(base.replace('success=\n','').replace('progress=60','progress=70'))
    (w/'test.c').write_text(header+helpers+test)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-no-pie','-I'+str(root),str(w/'test.c'),'-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test'),str(w)],check=True)
