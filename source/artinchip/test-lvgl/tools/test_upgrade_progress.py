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
#include <limits.h>
#include <sys/types.h>
#include "un260/lv_core/ui_upgrade_service.h"
#define UI_N_(s) (s)
#define UI_UPGRADE_STATUS_MAX_SIZE 512U
static char status_path[1024];
#define UI_UPGRADE_STATUS_FILE_PATH status_path
'''+ctx+'\nstatic ui_upgrade_service_ctx_t g_ui_upgrade_service;\n'
helpers='\n'.join(function(n) for n in ('ui_upgrade_service_set_status','ui_upgrade_service_stage_from_name','ui_upgrade_service_parse_int','ui_upgrade_service_parse_hash','ui_upgrade_service_load_status_file'))
test=r'''
static void load(const char *dir,const char *name){
 snprintf(status_path,sizeof(status_path),"%s/%s",dir,name);ui_upgrade_service_load_status_file();
}
static void reset(void){
 memset(&g_ui_upgrade_service,0,sizeof(g_ui_upgrade_service));
 g_ui_upgrade_service.running=true;
 g_ui_upgrade_service.child_pid=FIXTURE_WRITER;
 g_ui_upgrade_service.selected_hash=0x0123456789abcdefULL;
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
 reset();load(argv[1],"restart");
 assert(g_ui_upgrade_service.status.progress==96&&g_ui_upgrade_service.status.finished&&!g_ui_upgrade_service.status.success);
 assert(g_ui_upgrade_service.status.stage==UI_UPGRADE_STAGE_RESTART_REQUIRED);
 assert(strstr(g_ui_upgrade_service.status.result_text,"USB drive connected"));
 reset();load(argv[1],"restart_empty");
 assert(g_ui_upgrade_service.status.stage==UI_UPGRADE_STAGE_RESTART_REQUIRED&&!g_ui_upgrade_service.status.success);
 reset();load(argv[1],"step6");load(argv[1],"failure");
 assert(g_ui_upgrade_service.status.progress==34&&g_ui_upgrade_service.status.finished&&!g_ui_upgrade_service.status.success);
 reset();load(argv[1],"step6");
 ui_upgrade_service_ctx_t before=g_ui_upgrade_service;
 for(unsigned i=0;i<17;++i){char name[32];snprintf(name,sizeof(name),"invalid%u",i);load(argv[1],name);assert(!memcmp(&before,&g_ui_upgrade_service,sizeof(before)));}
 load(argv[1],"missing");assert(g_ui_upgrade_service.status.progress==70&&!g_ui_upgrade_service.status.finished);
 load(argv[1],"step6");assert(g_ui_upgrade_service.status.progress==70); /* cannot move backwards */
 puts("PASS actual shell -> C: intermediate stages, restart-required without final success, blank/absent pending, completion/failure, malformed rejection and monotonic progress");
}
'''
with tempfile.TemporaryDirectory(prefix='un260-upgrade-progress-') as temp:
    w=Path(temp)
    cases=[(2,'prepare','prepare',''),(5,'verify','verify_archive',''),(10,'extract','extract',''),
           (18,'verify','verify_manifest',''),(24,'verify','verify_checksum',''),(34,'preflight','preflight',''),
           (60,'install','install',''),(90,'install','install',''),(92,'sync','sync',''),(96,'finish','finish','')]
    shell=writer+'\nBUNDLE_FNV=0123456789abcdef\nLAST_STAGE=none\nPHASE_TIME=0\nSTART_TIME=0\nLOG="$1/log"\nSTATUS_TEMP="$1/temp"\n'
    for name,(p,stage,step,result) in [*((f'step{i}',c) for i,c in enumerate(cases)),('success',(100,'success','success','1')),('failure',(34,'fail','fail','0')),('restart',(96,'restart','restart','-1')),('restart_empty',(96,'restart','restart',''))]:
        shell+=f'STATUS_FILE="$1/{name}"\nwrite_status {p} {stage} {step} "{result}" "" || exit 1\n'
    subprocess.run(['sh','-c',shell,'writer',str(w)],check=True)
    base=(w/'step6').read_text()
    bad=[base.replace('success=','success=bad'),base.replace('success=','success=2'),
         base.replace('stage=install','stage=success'),base.replace('stage=install','stage=fail'),
         base.replace('success=','success=1'),base.replace('success=','success=0'),
         base.rstrip('\n'),base.replace('progress=60','progress=101'),
         (w/'restart').read_text().replace('success=-1','success=1'),
         (w/'restart').read_text().replace('success=-1','success=0'),
         (w/'restart').read_text().replace('success=-1','success=-2'),
         re.sub(r'writer_pid=\d+', 'writer_pid=1', base),
         base.replace('bundle_fnv64=0123456789abcdef','bundle_fnv64=0123456789abcdee'),
         re.sub(r'writer_pid=\d+\n', '', base),
         base.replace('bundle_fnv64=0123456789abcdef\n',''),
         base+'writer_pid=1\n',
         base.replace('bundle_fnv64=0123456789abcdef','bundle_fnv64=0123456789ABCDEF')]
    for i,text in enumerate(bad):(w/f'invalid{i}').write_text(text)
    (w/'missing').write_text(base.replace('success=\n','').replace('progress=60','progress=70'))
    writer_pid=int(re.search(r'writer_pid=(\d+)',base).group(1))
    (w/'test.c').write_text(header+'\n#define FIXTURE_WRITER %d\n'%writer_pid+helpers+test)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all','-no-pie','-I'+str(root),str(w/'test.c'),'-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test'),str(w)],check=True)
