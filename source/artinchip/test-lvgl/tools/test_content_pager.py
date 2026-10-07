#!/usr/bin/env python3
"""Run the production pager against real LVGL with ASan/UBSan and allocation tracing."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lvgl-dir', type=Path, default=Path(os.environ.get('LVGL_SOURCE', root.parents[1] / 'third-party/lvgl-8.3.2')))
    parser.add_argument('--sanitize', choices=('none', 'undefined', 'address,undefined'), default='address,undefined')
    parser.add_argument('--harness', type=Path, help='Standalone real-LVGL motion harness')
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get('CC') or shutil.which('cc') or shutil.which('gcc')
    if not compiler or not (lvgl / 'lvgl.h').is_file():
        raise SystemExit('Requires host GCC and the production LVGL source')
    cache = Path(tempfile.gettempdir()) / 'un260-content-pager-host-cache'
    (cache / 'lvgl').mkdir(parents=True, exist_ok=True)

    def write_changed(path, text):
        if not path.exists() or path.read_text() != text:
            path.write_text(text)

    write_changed(cache / 'lvgl/lvgl.h', '#include "' + (lvgl / 'lvgl.h').as_posix() + '"\n')
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
#define LV_USE_LOG 0
#define LV_USE_ASSERT_MALLOC 0
#endif
''')
    sources = [args.harness.resolve()] if args.harness else [root / 'tools/test_content_pager.c', root / 'un260/lv_components/lv_content_pager.c', root / 'un260/lv_components/lv_capsule_pagination.c']
    sources += sorted(p for p in (lvgl / 'src').rglob('*.c') if p.name != 'qrcodegen.c')
    flags = [compiler, '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-no-pie',
             '-I' + str(cache), '-I' + str(root), '-I' + str(lvgl), '-DLV_CONF_PATH=' + str(conf)]
    if args.sanitize != 'none':
        flags += ['-fsanitize=' + args.sanitize, '-fno-sanitize-recover=all']

    def compile_one(source):
        key = hashlib.sha256((str(source) + repr(flags)).encode()).hexdigest()
        obj, dep = cache / (key + '.o'), cache / (key + '.d')
        dependencies = [source]
        if dep.exists():
            dependencies += [Path(p) for p in dep.read_text().replace('\\\n', ' ').split(':', 1)[1].split()]
        if not obj.exists() or not dep.exists() or any(not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns for p in dependencies):
            subprocess.run([*flags, '-MMD', '-MF', str(dep), '-c', str(source), '-o', str(obj)], check=True)
        return obj

    with ThreadPoolExecutor(max_workers=4) as pool:
        objects = list(pool.map(compile_one, sources))
    executable = cache / 'test-content-pager'
    subprocess.run([*flags, *map(str, objects), '-Wl,--wrap=lv_mem_alloc', '-Wl,--wrap=lv_obj_create', '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True, timeout=60)


if __name__ == '__main__':
    main()
