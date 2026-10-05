#!/usr/bin/env python3
from pathlib import Path
from test_i18n_support import with_i18n
import ctypes.util
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-factory-reset-') as directory:
    temp = Path(directory)
    data = temp / 'state'
    backlight = temp / 'backlight'
    device = backlight / 'test-bl'
    (device / 'device/of_node').mkdir(parents=True)
    (device / 'max_brightness').write_text('10\n')
    (device / 'brightness').write_text('2\n')
    (device / 'actual_brightness').write_text('2\n')
    (device / 'bl_power').write_text('0\n')
    (device / 'device/of_node/default-brightness-level').write_bytes(bytes([0, 0, 0, 1]))
    headers = root.parents[2] / 'output/d211_d213_devkitf/host/riscv64-linux-gnu/sysroot/usr/include'
    for name in ('png.h', 'pngconf.h', 'pnglibconf.h'):
        shutil.copyfile(headers / name, temp / name)
    sources = with_i18n([root / name for name in (
        'tools/test_ui_factory_reset.c', 'un260/storage/ui_factory_reset_store.c',
        'un260/storage/storage_worker.c', 'un260/storage/workspace_store.c',
        'un260/workspace/workspace_model.c', 'un260/lv_system/backlight_service.c')], root)
    command = ['cc', '-std=gnu11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
        '-Wno-misleading-indentation', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-no-pie',
        f'-I{root}', f'-I{temp}', f'-DUI_FACTORY_RESET_DIRECTORY="{data}"',
        f'-DWORKSPACE_DIRECTORY="{data}"', f'-DUN260_BACKLIGHT_STATE="{data}"',
        f'-DUN260_BACKLIGHT_ROOT="{backlight}"', *map(str, sources), '-l:' + ctypes.util.find_library('png16'),
        '-lpthread', '-Wl,--wrap=fsync,--wrap=write,--wrap=renameat', '-o', str(temp / 'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(temp / 'test')], check=True)
