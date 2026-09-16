#!/usr/bin/env python3
"""Host integration test: production storage code with isolated filesystem/USB."""
from pathlib import Path
import subprocess
import tempfile
import struct
root = Path(__file__).resolve().parents[1]
png_headers = root.parents[2] / 'output/d211_d213_devkitf/host/riscv64-linux-gnu/sysroot/usr/include/libpng16'
with tempfile.TemporaryDirectory(prefix="un260-standby-test-") as temp:
    work = Path(temp)
    (work / 'usb').mkdir()
    exe = work / 'test'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-I', str(root), '-I', str(png_headers), f'-DSTANDBY_STORE_DIRECTORY="{work}/state"',
                    f'-DSTANDBY_USB_DIRECTORY="{work}/usb"',
                    str(root/'tools/test_standby_store.c'),
                    str(root/'un260/storage/standby_store.c'), '-l:libpng16.so.16', '-pthread', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    old=struct.pack('<IHBBB3x',1,37,1,0,0)
    for mode in range(2):
        for i in range(3):
            old+=struct.pack('<IHH8B',0x345678,850 if i==1 else 54,80,15,0,0,1,1,1,1,75 if i==1 else 100)
    assert len(old)==108
    (work/'state/standby.cfg').write_bytes(old)
    subprocess.run([str(exe),'migration'],check=True)
