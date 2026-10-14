#!/usr/bin/env python3
"""Real LVGL input lifecycle plus production threaded buzzer using mocked UART."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess,tempfile
from test_main_view import function
from test_i18n_support import lvgl_source
root=Path(__file__).resolve().parents[1]
lvgl=lvgl_source(root)
with tempfile.TemporaryDirectory(prefix='un260-touch-sound-') as temp:
    w=Path(temp)
    flags=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{root}']
    exe=w/'buzzer'
    subprocess.run([*flags,'-Werror',str(root/'tools/test_buzzer_service.c'),str(root/'un260/app_service/buzzer_service.c'),str(root/'un260/protocol/protocol_frame.c'),'-pthread','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    (w/'lvgl').mkdir();(w/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    conf=w/'lv_conf.h';conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (4U*1024U*1024U)\n#define LV_USE_LOG 0\n#define LV_USE_THEME_DEFAULT 0\n#endif\n')
    sources=[root/'tools/test_touch_sound.c',root/'un260/gesture/touch_sound.c',*lvgl.joinpath('src').rglob('*.c')]
    (w/'actual_evdev_feedback.h').write_text(function((root/'lv_port_indev.c').read_text(),'evdev_feedback'))
    flags += [f'-I{w}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[w/f'{i}.o' for i in range(len(sources))]
    def compile(pair):subprocess.run([*flags,'-c',str(pair[0]),'-o',str(pair[1])],check=True,stdout=subprocess.DEVNULL)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(compile,zip(sources,objects)))
    exe=w/'touch';subprocess.run([*flags,*map(str,objects),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
