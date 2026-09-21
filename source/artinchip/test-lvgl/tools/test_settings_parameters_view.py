#!/usr/bin/env python3
"""Render ordinary settings using actual LVGL/fonts/assets; mock hardware edges only.

Host ASan/UBSan output is not board touch/display verification.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os, re, subprocess, sys, tempfile
from PIL import Image
from test_list_view import compiled_asset_sources

root = Path(__file__).resolve().parents[1]
lvgl = root.parents[1] / 'third-party/lvgl-8.3.2'
out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
pages = ['05_set_password', '20_set_print', '22_set_double_note', '23_set_flap', '24_set_reject_pocket',
         '25_set_serial_number', '27_set_cfd_level', '29_set_password', '30_set_factory',
         '33_set_brightness', '36_display_test']
parts = [f'un260/lv_core/page_{name}.c' for name in pages]
parts += ['un260/lv_components/lv_loading_orbit.c']
parts += ['un260/lv_components/lv_settings.c', 'un260/lv_components/lv_nav_button.c',
          'un260/lv_components/lv_damped_button.c', 'un260/lv_components/lv_pin_keypad.c',
          'un260/lv_components/lv_pin_input.c', 'un260/lv_system/ui_text_page.c',
          'un260/lv_system/ui_text_widget.c', 'un260/lv_system/ui_lang.c',
          'un260/print/print_config.c', 'un260/serial_number/serial_number.c',
          'un260/cfd/cfd.c', 'un260/protocol/protocol_request.c']
fonts = sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+',
                  ''.join((root / p).read_text() for p in parts))))
for name in ['user', 'settings']:
    im = Image.open(root / 'aic_ui/lvgl_data/backgrounds' / f'{name}.png').convert('RGBA')
    (out / f'{name}.bgra').write_bytes(im.tobytes('raw', 'BGRA'))
with tempfile.TemporaryDirectory(prefix='un260-settings-parameters-') as temp:
    work = Path(temp)
    (work / 'lvgl').mkdir()
    (work / 'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    (work / 'lv_drv_conf.h').write_text('/* Host rendering: no physical driver. */\n')
    conf = work / 'lv_conf.h'
    conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n'
                    '#define LV_MEM_SIZE (16U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 0\n'
                    '#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n'
                    '#define LV_FONT_MONTSERRAT_20 1\n#define LV_FONT_CUSTOM_DECLARE ' +
                    ' '.join(f'LV_FONT_DECLARE({f});' for f in fonts) + '\n#endif\n')
    port = (root / 'lv_port_indev.c').read_text()
    helper = re.search(r'void lv_port_indev_set_drag_obj\(.*?\n\}', port, re.S)
    assert helper, 'Missing production drag-registration helper'
    (work / 'drag.c').write_text('#include "lvgl/lvgl.h"\n' + helper.group() + '\n')
    sources = [root / 'tools/test_settings_parameters_view.c', work / 'drag.c',
               *compiled_asset_sources(), *[root / p for p in parts],
               *[root / 'un260/font' / f'{f}.c' for f in fonts], *lvgl.joinpath('src').rglob('*.c')]
    common = ['gcc', '-DLV_DRV_CONF_H', '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',
              '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-fsanitize=address,undefined',
              '-fno-sanitize-recover=all', f'-I{work}', f'-I{root}', f'-I{lvgl}', f'-DLV_CONF_PATH={conf}']
    objects = [work / f'{i}.o' for i in range(len(sources))]
    def build(pair):
        subprocess.run([*common, '-c', str(pair[0]), '-o', str(pair[1])], check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(build, zip(sources, objects)))
    exe = work / 'view'
    subprocess.run([*common, *map(str, objects), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], env={**os.environ, 'OUT': str(out), 'UN260_BACKGROUND_DIR': str(out)}, check=True)
for file in out.glob('*.bgra'):
    Image.frombytes('RGBA', (1280, 400), file.read_bytes(), 'raw', 'BGRA').convert('RGB').save(file.with_suffix('.png'))
