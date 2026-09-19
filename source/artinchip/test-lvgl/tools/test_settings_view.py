#!/usr/bin/env python3
"""Actual production settings, fonts, PNG resources and LVGL renderer; hardware edges captured."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os,re,subprocess,tempfile,sys
from PIL import Image
from test_list_view import compiled_asset_sources
root=Path(__file__).resolve().parents[1]
def function(source,name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',source[source.index('{',match.start()):],re.S):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[match.start():source.index('{',match.start())+token.end()]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
for name in ['user','settings']:
    im=Image.open(root/'aic_ui/lvgl_data/backgrounds'/f'{name}.png').convert('RGBA')
    (out/f'{name}.bgra').write_bytes(im.tobytes('raw','BGRA'))
parts=['un260/lv_core/page_06_settings.c','un260/lv_core/page_11_timeset.c','un260/lv_core/page_21_set_language.c',
       'un260/lv_components/lv_settings.c','un260/lv_components/lv_nav_button.c']
fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+',''.join((root/p).read_text() for p in parts))))
with tempfile.TemporaryDirectory(prefix='un260-settings-view-') as temp:
    work=Path(temp);(work/'lvgl').mkdir();(work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    port=(root/'lv_port_indev.c').read_text()
    (work/'actual_settings_pointer.h').write_text(function(port,'evdev_feedback')+'\n'+function(port,'lv_port_indev_set_drag_obj'))
    conf=work/'lv_conf.h';conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (16U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 0\n#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({f});' for f in fonts)+'\n#endif\n')
    sources=[*compiled_asset_sources(),root/'tools/test_settings_view.c',root/'un260/lv_components/lv_settings.c',
      root/'un260/lv_components/lv_nav_button.c',root/'un260/lv_components/lv_damped_button.c',
      root/'un260/lv_core/settings_catalog.c',root/'un260/lv_system/machine_time.c',root/'un260/lv_system/ui_lang.c',
      root/'un260/data_collection/data_collection.c',root/'un260/device_info/device_info.c',
      *[root/'un260/font'/f'{f}.c' for f in fonts],*lvgl.joinpath('src').rglob('*.c')]
    common=['gcc','-DLVGL_DIR="L:/usr/local/share/lvgl_data/"','-std=gnu11','-O1','-g','-Wall','-Wextra',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'view';subprocess.run([*common,*map(str,objects),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],env={**os.environ,'OUT':str(out),'UN260_BACKGROUND_DIR':str(out)},check=True)
for file in out.glob('*.bgra'):
    Image.frombytes('RGBA',(1280,400),file.read_bytes(),'raw','BGRA').convert('RGB').save(file.with_suffix('.png'))
