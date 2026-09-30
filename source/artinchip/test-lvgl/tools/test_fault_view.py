from test_i18n_support import with_i18n
"""Render the real LVGL fault component and check its lifecycle under sanitizers."""
from pathlib import Path
import concurrent.futures
import hashlib
import os
import subprocess

root = Path(__file__).resolve().parents[1]
lvgl = Path(os.environ.get('LVGL_SOURCE', root.parents[1] / 'third-party/lvgl-8.3.2'))
out = Path(os.environ.get('FAULT_OUTPUT', '/tmp/un260-fault-renders'))
cache = Path('/tmp/un260-fault-host-cache')
out.mkdir(parents=True, exist_ok=True)
(cache / 'lvgl').mkdir(parents=True, exist_ok=True)
fonts = ['instrument_sans_medium_' + str(n) for n in (12,14,16,18,24)] + ['instrument_sans_semibold_' + str(n) for n in (12,14,20,22,28)]
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
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_ASSERT_HANDLER __builtin_trap();
#define LV_FONT_CUSTOM_DECLARE ''' + ' '.join('LV_FONT_DECLARE(lv_font_' + f + ');' for f in fonts) + '\n#endif\n')
sources = [root / p for p in ['tools/test_fault_view.c','un260/machine_state/machine_fault.c','un260/machine_state/machine_state.c',
 'un260/lv_components/fault_guide/fault_guide_catalog.c','un260/lv_components/fault_guide/machine_fault_view.c',
 'aic_ui/generated_fault_guide/machine_fault_assets.c','un260/font/ui_message_font.c']]
# Execute the actual application navigation owner with the real popup.
from test_main_view import function
boot_owner = cache / 'boot_confirm_owner.c'
write_changed(boot_owner, '#include "un260/app_service/app_boot_runtime.h"\n'
    '#include "un260/lv_core/lv_page_manager.h"\n'
    '#include "un260/lv_components/lv_fault_popup.h"\n' +
    function((root/'un260/app_service/app_boot_runtime.c').read_text(), 'app_boot_runtime_confirm_fault'))
sources.append(boot_owner)
sources += [root / ('un260/font/lv_font_' + f + '.c') for f in fonts]
sources += [root / ('un260/font/lv_font_message_cjk_' + str(n) + '.c') for n in (12,14,16,18,20,22,24,28)]
sources += sorted(p for p in (lvgl / 'src').rglob('*.c') if p.name != 'qrcodegen.c')
sources = with_i18n(sources, root)
flags = ['cc','-std=gnu11','-g','-O1','-Wall','-Wextra','-fsanitize=address,undefined','-fno-sanitize-recover=all',
         '-no-pie',f'-I{cache}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
def compile_one(source):
    key = hashlib.sha256((str(source)+repr(flags)).encode()).hexdigest()
    obj, dep = cache/(key+'.o'), cache/(key+'.d')
    dependencies = [source]
    if dep.exists():
        dependencies += [Path(p) for p in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
    if not obj.exists() or not dep.exists() or any(not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns for p in dependencies):
        subprocess.run(flags+['-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)],check=True)
    return obj
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    objects=list(pool.map(compile_one,sources))
exe=cache/'test_fault'
subprocess.run(['cc','-fsanitize=address,undefined','-no-pie',*map(str,objects),'-lm','-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True,env=dict(os.environ,FAULT_OUTPUT=str(out)))
from PIL import Image
for path in out.glob('*.bgra'):
    Image.frombytes('RGBA',(1280,400),path.read_bytes(),'raw','BGRA').convert('RGB').save(path.with_suffix('.png'))
