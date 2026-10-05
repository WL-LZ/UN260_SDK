#!/usr/bin/env python3
"""Production standby worker under syscall faults; isolated host files only."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
from test_i18n_support import with_i18n

root = Path(__file__).resolve().parents[1]
png_headers = Path(os.environ.get('UN260_SYSROOT_HEADERS') or
                   root.parents[2] / 'output/d211_d213_devkitf/host/riscv64-linux-gnu/sysroot/usr/include') / 'libpng16'
cases = ('file-sync', 'dir-open', 'dir-sync', 'dir-close', 'source-read',
         'source-close', 'missing-usb', 'import-sync', 'delete-config-sync',
         'delete-sync', 'delete-meta', 'delete-meta-eio', 'busy')
with tempfile.TemporaryDirectory(prefix='un260-standby-durability-') as temp:
    work = Path(temp)
    executable = work / 'test'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-I', str(root), '-I', str(png_headers),
                    '-DSTANDBY_STORE_DIRECTORY="%s"' % (work / 'state'),
                    '-DSTANDBY_USB_DIRECTORY="%s"' % (work / 'usb'),
                    str(root / 'tools/test_standby_store_durability.c'),
                    str(root / 'un260/storage/standby_store.c'), *map(str, with_i18n([], root)),
                    '-l:libpng16.so.16', '-pthread',
                    *['-Wl,--wrap=' + function for function in ('open', 'close', 'fsync', 'unlink', 'read', 'write')],
                    '-o', str(executable)], check=True)
    for case in cases:
        for directory in ('state', 'usb'):
            path = work / directory
            if path.exists():
                shutil.rmtree(path)
            path.mkdir()
        subprocess.run([str(executable), case], check=True)
        if case in ('dir-open', 'dir-sync', 'dir-close'):
            subprocess.run([str(executable), 'boot'], check=True)
