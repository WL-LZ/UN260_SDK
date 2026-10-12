#!/usr/bin/env python3
"""Complete production USB result popup on real LVGL and released locale fonts."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse, hashlib, json, os, re, subprocess, sys
from test_i18n_support import with_i18n, lvgl_source

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
work = Path('/tmp/un260-upgrade-ack-host-cache'); work.mkdir(exist_ok=True)
lvgl = lvgl_source(root)
def write_changed(path, text):
    if not path.exists() or path.read_text() != text: path.write_text(text)
(work / 'lvgl').mkdir(exist_ok=True)
write_changed(work / 'lvgl/lvgl.h', '#include "' + str(lvgl / 'lvgl.h') + '"\n')
source = (root / 'un260/lv_components/lv_upgrade_popup.c').read_text()
font_source = (root / 'un260/font/ui_message_font.c').read_text()
fonts = sorted(set(re.findall(r'lv_font_instrument_sans_[a-z]+_\d+', source + font_source)))
conf = work / 'lv_conf.h'
write_changed(conf, '''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_COLOR_SCREEN_TRANSP 1
#define LV_MEM_SIZE (24U*1024U*1024U)
#define LV_USE_BIDI 1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 83
#define LV_FS_STDIO_PATH ""
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_ASSERT_HANDLER __builtin_trap();
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_CUSTOM_DECLARE ''' + ' '.join('LV_FONT_DECLARE(' + f + ');' for f in fonts) + '\n#endif\n')
theme = (root / 'un260/lv_core/settings_detail_ui.c').read_text()
write_changed(work / 'actual_upgrade_theme.h', re.search(r'lv_color_t settings_theme_color_hex\(.*?\n\}', theme, re.S)[0] + '\n')
fragment = root / 'i18n/fragments/usb_upgrade_ack_20261010.json'
translations = json.loads(fragment.read_text())
key = 'Restart the machine later to use the update.'
write_changed(work / 'upgrade_ack_translations.h', 'static const struct {const char *tag,*text;} ack_translations[]={\n' +
              ',\n'.join('{' + json.dumps(tag) + ',' + json.dumps(values[key], ensure_ascii=False) + '}' for tag, values in translations.items()) + '\n};\n')
sources = [root / p for p in ['tools/test_upgrade_success_ack.c', 'un260/lv_components/lv_damped_button.c',
    'un260/app_service/upgrade_session.c', 'un260/font/ui_message_font.c', 'un260/font/ui_locale_font_bank.c',
    'un260/lv_system/ui_text_widget.c', 'un260/lv_system/ui_text_page.c']]
sources += [root / ('un260/font/' + f + '.c') for f in fonts]
sources += [root / ('un260/font/lv_font_message_cjk_' + str(n) + '.c') for n in [12, 14, 16, 18, 20, 22, 24, 28]]
sources += sorted(p for p in (lvgl / 'src').rglob('*.c') if p.name != 'qrcodegen.c')
sources = with_i18n(sources, root)
flags = ['cc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Wno-misleading-indentation',
    '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
    '-DUI_STATE_DIR="/tmp/un260-i18n-tests"', '-DUN260_LOCALE_FONT_DIR="S:' + str(root / 'aic_ui/lvgl_data/locale_fonts') + '/"',
    '-I' + str(work), '-I' + str(root), '-I' + str(lvgl), '-DLV_CONF_PATH=' + str(conf)]
dependency_paths = set(sources) | {fragment}
def compile_one(source):
    key = hashlib.sha256((str(source) + repr(flags)).encode()).hexdigest()
    obj, dep = work / (key + '.o'), work / (key + '.d')
    deps = [source]
    if dep.exists(): deps += [Path(p) for p in dep.read_text().replace('\\\n', ' ').split(':', 1)[1].split()]
    if not obj.exists() or not dep.exists() or any(not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns for p in deps):
        subprocess.run(flags + ['-MMD', '-MF', str(dep), '-c', str(source), '-o', str(obj)], check=True)
    return obj, [Path(p) for p in dep.read_text().replace('\\\n', ' ').split(':', 1)[1].split()]
with ThreadPoolExecutor(max_workers=4) as pool: compiled = list(pool.map(compile_one, sources))
for _, deps in compiled: dependency_paths.update(deps)
dependency_paths.update((root / 'aic_ui/lvgl_data/locale_fonts').rglob('*.bin'))
def identity(): return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dependency_paths) if p.is_file()}
before = identity()
exe = work / 'test'
subprocess.run(['cc', '-fsanitize=address,undefined', '-no-pie', *map(str, (p for p, _ in compiled)), '-lm', '-lz', '-o', str(exe)], check=True)
result = subprocess.run([str(exe)], env=dict(os.environ, UPGRADE_ACK_OUTPUT=str(out)))
after = identity()
(out / 'dependency-identity.json').write_text(json.dumps({'unchanged': before == after, 'before': before, 'after': after}, indent=2) + '\n')
from PIL import Image
for path in out.glob('*.bgra'):
    Image.frombytes('RGBA', (1280, 400), path.read_bytes(), 'raw', 'BGRA').convert('RGB').save(path.with_suffix('.png'))
assert before == after, 'Test dependencies changed during execution'
result.check_returncode()
