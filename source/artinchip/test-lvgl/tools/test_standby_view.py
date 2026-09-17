#!/usr/bin/env python3
"""Real LVGL renderer, production standby page/fonts; fake device actions only."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os, re, subprocess, tempfile, sys
from PIL import Image
from test_list_view import compiled_asset_sources
root=Path(__file__).resolve().parents[1]
lvgl=root.parents[1]/'third-party/lvgl-8.3.2'
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
for src,name,size in [('standby/celadon.png','wallpaper',(1280,400)),('standby/silver.png','silver',(1280,400)),('standby/champagne.png','champagne',(1280,400)),('standby/un260-mark.png','gear',(24,28))]:
    im=Image.open(root/'aic_ui/lvgl_data'/src).convert('RGBA').resize(size)
    (out/(name+'.bgra')).write_bytes(im.tobytes('raw','BGRA'))
fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|standby)_[a-zA-Z0-9_]+',(root/'un260/lv_core/page_34_standby.c').read_text())))
with tempfile.TemporaryDirectory(prefix='un260-standby-view-') as temp:
    work=Path(temp);(work/'lvgl').mkdir();(work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    (work/'lv_port_indev.h').write_text('#include "lvgl/lvgl.h"\nvoid lv_port_indev_set_drag_obj(lv_obj_t*,bool);\n')
    conf=work/'lv_conf.h';conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (8U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 1\n#define LV_USE_LOG 0\n#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({f});' for f in fonts)+'\n#endif\n')
    sources=[*compiled_asset_sources(),root/'tools/test_standby_view.c',root/'un260/font/scaled_font.c',*[root/'un260/font'/f'{f}.c' for f in fonts],*lvgl.joinpath('src').rglob('*.c')]
    common=['gcc','-DLVGL_DIR="L:/usr/local/share/lvgl_data/"','-std=gnu11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'view';subprocess.run([*common,*map(str,objects),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],env={**os.environ,'OUT':str(out)},check=True)
for file in out.glob('*.bgra'):
    if file.stem in ('wallpaper','gear','silver','champagne'):continue
    Image.frombytes('RGBA',(1280,400),file.read_bytes(),'raw','BGRA').convert('RGB').save(file.with_suffix('.png'))
