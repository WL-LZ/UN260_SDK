#!/usr/bin/env python3
"""Exercise the production device helper with private child processes only."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='un260-device-worker-') as directory:
    tmp = Path(directory)
    script = tmp / 'prepare'
    fixture = tmp / 'test.c'
    fixture.write_text('''
#include "un260/lv_drivers/startup_devices.h"
int main(void) { return startup_devices_prepare() ? 0 : 1; }
''')
    exe = tmp / 'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-I'+str(root),
                    '-DSTARTUP_DEVICE_SCRIPT="'+str(script)+'"',
                    '-DSTARTUP_DEVICE_TIMEOUT_MS=100', str(fixture),
                    str(root/'un260/lv_drivers/startup_devices.c'), '-o', str(exe)], check=True)
    env = dict(os.environ, UN260_STARTUP_PREPARE='0')
    assert subprocess.run([str(exe)], env=env, timeout=3).returncode == 0
    env['UN260_STARTUP_PREPARE'] = '1'
    assert subprocess.run([str(exe)], env=env, timeout=3).returncode == 1
    for name, body, expected in [
        ('success', '[ "$1" = prepare-devices ]', 0),
        ('failure', 'exit 9', 1),
        ('timeout', 'sleep 20', 1),
        ('term-ignored', "trap '' TERM\nsleep 20", 1),
    ]:
        script.write_text('#!/bin/sh\n'+body+'\n')
        script.chmod(0o755)
        assert subprocess.run([str(exe)], env=env, timeout=3).returncode == expected
        print('PASS device preparation', name)
    print('PASS disabled legacy path and spawn failure')
