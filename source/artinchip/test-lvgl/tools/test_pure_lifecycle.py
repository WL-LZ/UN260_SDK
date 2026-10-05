#!/usr/bin/env python3
"""Exercise production PURE lifecycle and theme transitions on real LVGL."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess, tempfile
from test_i18n_support import lvgl_source
root = Path(__file__).resolve().parents[1]
lvgl = lvgl_source(root)
with tempfile.TemporaryDirectory(prefix='un260-pure-lifecycle-') as temp:
    work = Path(temp)
    (work/'lvgl').mkdir()
    (work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    conf=work/'lv_conf.h'
    conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (8U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 1\n#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n#endif\n')
    sources=[root/'tools/test_pure_lifecycle.c',*[root/'un260/font'/f'{name}.c' for name in ('MONTSERRAT_EXTRABOLD_35','MONTSERRAT_MEDIUM_15','lv_font_rajdhani_27','lv_font_rajdhani_142','lv_font_rajdhani_194')],*lvgl.joinpath('src').rglob('*.c')]
    common=['cc','-std=gnu11','-O1','-g','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair): subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(build,zip(sources,objects)))
    exe=work/'test'
    subprocess.run([*common,*map(str,objects),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
