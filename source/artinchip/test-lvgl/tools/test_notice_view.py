from test_i18n_support import with_i18n
"""Headless actual-LVGL notification rendering and lifecycle checks (ASan/UBSan)."""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import os
import subprocess

root = Path(__file__).resolve().parents[1]
lvgl = Path(os.environ.get('LVGL_SOURCE', root.parents[1] / 'third-party/lvgl-8.3.2'))
out = Path(os.environ.get('NOTICE_OUTPUT', '/tmp/un260-notice-renders'))
cache = Path('/tmp/un260-notice-host-cache')
out.mkdir(parents=True, exist_ok=True)
(cache / 'lvgl').mkdir(parents=True, exist_ok=True)
fonts = ['instrument_sans_semibold_22', 'instrument_sans_semibold_20', 'instrument_sans_semibold_28',
         'instrument_sans_medium_12', 'instrument_sans_medium_14', 'instrument_sans_medium_16',
         'instrument_sans_medium_18', 'instrument_sans_medium_24']

def write_changed(path, text):
    if not path.exists() or path.read_text() != text:
        path.write_text(text)

write_changed(cache / 'lvgl/lvgl.h', '#include "' + str(lvgl / 'lvgl.h') + '"\n')
conf = cache / 'lv_conf.h'
write_changed(conf, '''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (4U*1024U*1024U)
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_ASSERT_HANDLER __builtin_trap();
#define LV_FONT_CUSTOM_DECLARE ''' + ' '.join('LV_FONT_DECLARE(lv_font_' + f + ');' for f in fonts) + '\n#endif\n')
manifest = json.loads((root / 'aic_ui/font/message/manifest.json').read_text())
coverage = []
for size in (12, 14, 16, 18, 20, 22, 24, 28):
    coverage.append(f'static const uint32_t message_codepoints_{size}[] = {{' +
                    ','.join(str(point) for point in manifest['codepoints_by_size'][str(size)]) + '};')
coverage.append('static const struct {const uint32_t *points; unsigned count;} message_coverage[] = {')
for size in (12, 14, 16, 18, 24, 20, 22, 28):
    coverage.append(f'{{message_codepoints_{size},sizeof(message_codepoints_{size})/sizeof(uint32_t)}},')
coverage.append('};')
write_changed(cache / 'message_codepoints.h', '\n'.join(coverage)+'\n')
sources = [root / 'tools/test_notice_view.c', root / 'un260/lv_components/ui_notice_state.c']
sources += [root / 'un260/font/ui_message_font.c']
sources += [root / f'un260/font/lv_font_message_cjk_{size}.c' for size in (12, 14, 16, 18, 20, 22, 24, 28)]
sources += [root / ('un260/font/lv_font_' + font + '.c') for font in fonts]
sources += sorted(p for p in (lvgl / 'src').rglob('*.c') if p.name != 'qrcodegen.c')
sources = with_i18n(sources, root)
flags = ['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-fsanitize=address,undefined',
         '-fno-sanitize-recover=all', '-no-pie', f'-I{cache}', f'-I{root}', f'-I{lvgl}',
         f'-DLV_CONF_PATH={conf}']

def compile_one(source):
    key = hashlib.sha256((str(source) + repr(flags)).encode()).hexdigest()
    obj, dep = cache / (key + '.o'), cache / (key + '.d')
    dependencies = [source]
    if dep.exists():
        dependencies += [Path(p) for p in dep.read_text().replace('\\\n', ' ').split(':', 1)[1].split()]
    if not obj.exists() or not dep.exists() or any(not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns for p in dependencies):
        subprocess.run(flags + ['-MMD', '-MF', str(dep), '-c', str(source), '-o', str(obj)], check=True)
    return obj

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    objects = list(pool.map(compile_one, sources))
exe = cache / 'test_notice'
subprocess.run(['cc', '-fsanitize=address,undefined', '-no-pie', *map(str, objects), '-lm', '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True, env=dict(os.environ, NOTICE_OUTPUT=str(out)))
from PIL import Image
for path in out.glob('*.bgra'):
    Image.frombytes('RGBA', (1280, 400), path.read_bytes(), 'raw', 'BGRA').convert('RGB').save(path.with_suffix('.png'))
