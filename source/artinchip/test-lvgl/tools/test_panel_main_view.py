#!/usr/bin/env python3
"""Render production panel views with real LVGL, panel assets and locale fonts.

Only controller/peripheral endpoints are stubbed in the C fixture. This is a
host layout/lifecycle test; it does not validate board touch or serial timing.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse, hashlib, os, re, subprocess
from PIL import Image
from test_i18n_support import with_i18n

ROOT=Path(__file__).resolve().parents[1]

def function(source,name):
    found=re.search(r'^[^\n;{}]*\b'+name+r'\([^;{}]*\)\s*\{.*?^\}',source,re.M|re.S)
    assert found,name
    return found.group()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl-dir',type=Path,required=True)
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--work-dir',type=Path,default=Path('/tmp/un260-panel-main-host'))
    parser.add_argument('--harness',type=Path)
    parser.add_argument('--extra-source',type=Path,action='append',default=[])
    parser.add_argument('--sanitize',default='address,undefined',choices=['none','undefined','address,undefined'])
    args=parser.parse_args();work=args.work_dir.resolve();work.mkdir(parents=True,exist_ok=True)
    lvgl=args.lvgl_dir.resolve();out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    (work/'lvgl').mkdir(exist_ok=True)
    (work/'lvgl/lvgl.h').write_text('#include "'+str(lvgl/'lvgl.h')+'"\n')
    sources=[args.harness.resolve() if args.harness else ROOT/'tools/test_panel_main_view.c']
    sources += [ROOT/p for p in ['un260/lv_core/panel_entry.c','un260/lv_components/ui_panel.c',
        'un260/lv_resources/panel/panel_assets.c','un260/font/ui_message_font.c','un260/font/ui_locale_font_bank.c',
        'un260/lv_system/ui_text_page.c','un260/lv_system/ui_text_widget.c','un260/counting/counting_data_store.c','un260/counting/counting_multi.c',
        'un260/machine_state/machine_state.c','un260/machine_state/machine_fault.c',
        'un260/currency/currency_state.c','un260/currency/currency_metadata.c']]
    sources += [p.resolve() for p in args.extra_source]
    sources=with_i18n(sources,ROOT)
    sources += sorted((ROOT/'un260/font/panel').glob('lv_font_panel_*.c'))
    refs=set()
    for p in sources: refs.update(re.findall(r'\blv_font_(?:instrument_sans|manrope|main|message_cjk)_[a-zA-Z0-9_]+',p.read_text()))
    sources += [ROOT/'un260/font'/f'{name}.c' for name in sorted(refs) if (ROOT/'un260/font'/f'{name}.c').exists()]
    conf='''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (32U * 1024U * 1024U)
#define LV_USE_BIDI 1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 83
#define LV_FS_STDIO_PATH ""
#define LV_USE_LOG 0
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_FONT_CUSTOM_DECLARE '''+' '.join('LV_FONT_DECLARE('+n+')' for n in sorted(refs))+'\n#endif\n'
    (work/'lv_conf.h').write_text(conf)
    (work/'actual_amount.inc').write_text(function((ROOT/'un260/lv_system/platform_app.c').read_text(),'format_amount_with_comma'))
    flags=['-std=c11','-O1','-g','-Wall','-Wextra','-ffunction-sections','-fdata-sections',
        '-D_POSIX_C_SOURCE=200809L','-D_DEFAULT_SOURCE','-DLV_DRV_CONF_H','-DLV_CONF_PATH='+str(work/'lv_conf.h'),'-I'+str(work),'-I'+str(ROOT),'-I'+str(ROOT/'aic_ui'),
        '-I'+str(lvgl.parent),'-I'+str(lvgl),'-DLVGL_DIR="L:/usr/local/share/lvgl_data/"','-DUN260_LOCALE_FONT_DIR="S:'+str(ROOT/'aic_ui/lvgl_data/locale_fonts')+'/"']
    if args.sanitize!='none':flags += ['-fsanitize='+args.sanitize,'-fno-omit-frame-pointer']
    sources += sorted((lvgl/'src').rglob('*.c'))
    def compile_one(source):
        included=b''
        for path in re.findall(r'#include\s+"([^"]+\.c)"',source.read_text()):
            candidate=ROOT/path
            if candidate.exists():included+=candidate.read_bytes()
        key=hashlib.sha256((str(source)+'\n'+str(flags)+'\n'+conf).encode()+source.read_bytes()+included).hexdigest()[:20]
        obj=work/(key+'.o')
        # Header changes affect all fixture objects; LVGL cache stays reusable.
        stamp=max((p.stat().st_mtime for p in (ROOT/'un260').rglob('*') if p.suffix in ('.h','.inc')),default=0)
        if not obj.exists() or (str(source).startswith(str(ROOT)) and obj.stat().st_mtime<stamp):
            subprocess.run(['gcc',*flags,'-c',str(source),'-o',str(obj)],check=True,cwd=ROOT)
        return str(obj)
    with ThreadPoolExecutor(max_workers=min(8,os.cpu_count() or 4)) as pool:objects=list(pool.map(compile_one,sources))
    binary=work/'panel-view';subprocess.run(['gcc',*flags,*objects,'-Wl,--gc-sections','-lm','-lz','-o',str(binary)],check=True)
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    result=subprocess.run([str(binary),str(out)],env=env,text=True,capture_output=True,timeout=90)
    (out/'validation.log').write_text(result.stdout+result.stderr)
    print(result.stdout+result.stderr,end='')
    for bmp in out.glob('*.bmp'):
        with Image.open(bmp) as im:im.save(bmp.with_suffix('.png'))
    result.check_returncode()
    print('Panel renders:',out)
if __name__=='__main__':main()
