#!/usr/bin/env python3
"""Check ui_panel's actual OOM contract using real LVGL and production abort policy.
This host test is deliberately not a claim of board-level OOM recovery.
"""
import argparse,concurrent.futures,hashlib,os,resource,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl-dir',required=True,type=Path)
    parser.add_argument('--sanitize',choices=['address,undefined','none'],default='address,undefined')
    parser.add_argument('--work-dir',type=Path,default=Path('/tmp/un260-panel-create-failure'))
    args=parser.parse_args();lvgl=args.lvgl_dir.resolve();work=args.work_dir.resolve();work.mkdir(parents=True,exist_ok=True)
    (work/'lvgl').mkdir(exist_ok=True);(work/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
    config="""#ifndef LV_CONF_H
#define LV_CONF_H
#include <stdlib.h>
#define LV_COLOR_DEPTH 32
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_NULL 1
#define LV_USE_LOG 0
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#endif
"""
    (work/'lv_conf.h').write_text(config)
    flags=['-std=gnu11','-O1','-g','-Wall','-Wextra','-no-pie',
           '-DLV_DRV_CONF_H','-DLV_CONF_PATH='+str(work/'lv_conf.h'),'-I'+str(work),'-I'+str(ROOT),'-I'+str(lvgl)]
    flags+=['-DPANEL_FAILURE_SANITIZED='+('0' if args.sanitize=='none' else '1')]
    if args.sanitize!='none':flags+=['-fsanitize='+args.sanitize,'-fno-sanitize-recover=all']
    sources=[ROOT/'tools/test_panel_create_failure.c',ROOT/'un260/lv_components/ui_panel.c',ROOT/'un260/lv_resources/panel/panel_assets.c']
    sources+=sorted((ROOT/'un260/font/panel').glob('lv_font_panel_*.c'))+sorted((lvgl/'src').rglob('*.c'))
    def compile_one(source):
        key=hashlib.sha256((str(source)+str(flags)+config).encode()+source.read_bytes()).hexdigest()[:20];obj=work/(key+'.o')
        if not obj.exists():subprocess.run(['gcc',*flags,'-c',str(source),'-o',str(obj)],check=True)
        return str(obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(8,os.cpu_count() or 4)) as pool:objects=list(pool.map(compile_one,sources))
    binary=work/'panel-create-failure'
    wrappers=['lv_mem_alloc','lv_label_create','lv_timer_create']
    subprocess.run(['gcc',*flags,*objects,*['-Wl,--wrap='+name for name in wrappers],'-lm','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    result=subprocess.run([str(binary)],env=env,cwd=work,text=True,capture_output=True,timeout=30)
    (work/'validation.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');result.check_returncode()
if __name__=='__main__':main()
