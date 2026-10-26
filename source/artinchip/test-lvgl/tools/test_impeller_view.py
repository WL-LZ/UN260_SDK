#!/usr/bin/env python3
"""Actual impeller settings page, LVGL and released locale fonts.

Only controller/navigation/keyboard-overlay edges are mocked. Protocol owner
and wire behavior have a separate service regression; this is not board QA.
One compatible object cache is reused by all scenarios and subsequent runs.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import os
from pathlib import Path
import re
import subprocess
from PIL import Image
from test_i18n_support import with_i18n, lvgl_source
from test_list_view import compiled_asset_sources

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--cache',type=Path)
    args=parser.parse_args()
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    work=out/'build';work.mkdir(exist_ok=True)
    cache=(args.cache or out/'runtime-cache').resolve();cache.mkdir(parents=True,exist_ok=True)
    temp=out/'temp';temp.mkdir(exist_ok=True);os.environ['TMPDIR']=str(temp)
    lvgl=lvgl_source(ROOT).resolve()
    (work/'lvgl').mkdir(exist_ok=True)
    (work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    (work/'lv_drv_conf.h').write_text('/* No host physical driver. */\n')
    parts=['tools/test_impeller_view.c','un260/lv_core/page_40_set_impeller.c',
        'un260/font/ui_message_font.c','un260/font/ui_locale_font_bank.c',
        'un260/lv_components/lv_settings.c','un260/lv_components/lv_nav_button.c',
        'un260/lv_components/lv_damped_button.c','un260/lv_system/ui_text_widget.c','un260/lv_system/ui_text_page.c']
    parts += [f'un260/font/lv_font_message_cjk_{n}.c' for n in [12,14,16,18,20,22,24,28]]
    fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+',''.join((ROOT/p).read_text() for p in parts))))
    conf=cache/'lv_conf.h'
    text=('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n'
        '#define LV_MEM_SIZE (16U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 0\n'
        '#define LV_USE_BIDI 1\n#define LV_USE_ARABIC_PERSIAN_CHARS 1\n'
        '#define LV_USE_FS_STDIO 1\n#define LV_FS_STDIO_LETTER 83\n#define LV_FS_STDIO_PATH ""\n'
        '#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_MONTSERRAT_20 1\n'
        '#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({f});' for f in fonts)+'\n#endif\n')
    if conf.exists() and conf.read_text()!=text:raise SystemExit('Incompatible LVGL cache; choose another --cache')
    if not conf.exists():conf.write_text(text)
    port=(ROOT/'lv_port_indev.c').read_text()
    helper=re.search(r'void lv_port_indev_set_drag_obj\(.*?\n\}',port,re.S)
    assert helper
    drag=work/'drag.c';drag.write_text('#include "lvgl/lvgl.h"\n'+helper.group()+'\n')
    common=['gcc','-DLV_DRV_CONF_H','-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',
        '-DUI_STATE_DIR="'+str(out/'state')+'"','-std=gnu11','-O1','-g','-Wall','-Wextra',
        '-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie',
        f'-I{work}',f'-I{ROOT}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}',
        '-DUN260_LOCALE_FONT_DIR="S:'+str(ROOT/'aic_ui/lvgl_data/locale_fonts')+'/"']
    sources=with_i18n([drag,*compiled_asset_sources(),*[ROOT/p for p in parts],
        *[ROOT/'un260/font'/f'{f}.c' for f in fonts]],ROOT)
    jobs=[];objects=[];reused=0
    for source in [*sources,*sorted((lvgl/'src').rglob('*.c'))]:
        stable=(lvgl/'src') in source.parents
        obj=(cache if stable else work)/(hashlib.sha256(str(source).encode()).hexdigest()[:20]+'.o')
        objects.append(obj)
        dep=obj.with_suffix('.d')
        valid=False
        if stable and obj.exists() and dep.exists():
            paths=dep.read_text().replace('\\\n',' ').split()[1:]
            valid=all(Path(p).exists() and Path(p).stat().st_mtime<=obj.stat().st_mtime for p in paths)
        if valid:reused+=1
        else:jobs.append((source,obj,dep))
    def compile_one(job):
        source,obj,dep=job
        subprocess.run([*common,'-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(compile_one,jobs))
    exe=work/'impeller-view'
    subprocess.run([*common,*map(str,objects),'-lm','-lz','-o',str(exe)],check=True)
    for name in ['user','settings']:
        im=Image.open(ROOT/'aic_ui/lvgl_data/backgrounds'/f'{name}.png').convert('RGBA')
        (out/f'{name}.bgra').write_bytes(im.tobytes('raw','BGRA'))
    result=subprocess.run([str(exe)],env={**os.environ,'OUT':str(out),'UN260_BACKGROUND_DIR':str(out)})
    for file in out.glob('*.bgra'):
        Image.frombytes('RGBA',(1280,400),file.read_bytes(),'raw','BGRA').convert('RGB').save(file.with_suffix('.png'))
    print(f'Compatible LVGL objects reused: {reused}; compiled sources: {len(jobs)}',flush=True)
    result.check_returncode()
if __name__=='__main__':main()
