#!/usr/bin/env python3
"""Production affine/session/storage tests using isolated host state files."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-touch-calibration-') as temp:
    work = Path(temp)
    state = work / 'state'
    exe = work / 'test'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-fno-sanitize-recover=undefined', '-I', str(root),
                    '-DTOUCH_CALIBRATION_STORE_DIR="%s"' % state,
                    str(root / 'tools/test_touch_calibration.c'),
                    str(root / 'un260/input/touch_calibration_math.c'),
                    str(root / 'un260/storage/touch_calibration_store.c'),
                    str(root / 'un260/app_service/touch_calibration_service.c'),
                    '-pthread', '-lm', *['-Wl,--wrap='+name for name in ('open','fsync','write','rename')],
                    '-o', str(exe)], check=True)
    def run(case): subprocess.run([str(exe), case], check=True, timeout=15)
    for case in ('math', 'session', 'validation', 'fault', 'busy', 'store'):
        if state.exists(): shutil.rmtree(state)
        state.mkdir()
        run(case)
        if case in ('session', 'busy'): run('boot')
        if case == 'fault': run('disabled-boot')
    cfg = state / 'touch_calibration.cfg'
    data = cfg.read_bytes()
    corrupted = bytearray(data)
    corrupted[25] ^= 1
    cfg.write_bytes(corrupted)
    run('bad-boot')
    cfg.write_bytes(data[:30])
    run('bad-boot')
    cfg.write_bytes(data + b'extra')
    run('bad-boot')
    cfg.unlink()
    os.mkfifo(cfg)
    run('bad-boot')
    cfg.unlink()
    temporary = state / 'touch_calibration.cfg.tmp'
    os.mkfifo(temporary)
    run('fifo-temp')
    # A reader makes O_WRONLY open succeed; fstat must still reject a FIFO.
    reader = os.open(temporary, os.O_RDWR | os.O_NONBLOCK)
    try: run('fifo-temp')
    finally: os.close(reader)
print('PASS calibration host verification; board touch accuracy remains a hardware check')
