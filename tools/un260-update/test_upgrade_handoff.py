#!/usr/bin/env python3
"""Maintenance framebuffer lease, fd handoff, failure screen lifetime."""
import ast
import os
from pathlib import Path
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2] / 'source/artinchip/test-lvgl'
# Reuse the established fake framebuffer/real UNIX socket harness, not an
# alternative implementation of the display ownership protocol.
tree = ast.parse((ROOT / 'tools/test_boot_light.py').read_text())
fixture = next(ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
               and any(isinstance(t, ast.Name) and t.id == 'fixture' for t in n.targets))
fixture = fixture.replace('assert(result==1 && elapsed>0 && g_inherited_fb>=0);',
                          'assert(result==0 && elapsed==0 && g_inherited_fb>=0);')

with tempfile.TemporaryDirectory(prefix='un260-upgrade-handoff-') as directory:
    path = Path(directory)
    source = path / 'test.c'; source.write_text(fixture)
    executable = path / 'test'
    defines = ['-DUN260_UPDATE_DISPLAY=1']
    for name, value in [('BOOT_LIGHT_SOCKET', path / 'socket'), ('BOOT_LIGHT_LOCK', path / 'lock'),
                        ('BOOT_LIGHT_READY', path / 'ready'), ('UPGRADE_STATUS_PATH', path / 'status')]:
        defines.append('-D' + name + '="' + str(value) + '"')
    subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-fno-sanitize-recover=all', '-I' + str(ROOT), *defines, str(source),
                    '-Wl,--wrap=open', '-Wl,--wrap=ioctl', '-Wl,--wrap=sendmsg', '-lz', '-o', str(executable)], check=True)
    fb = path / 'fb'; fb.write_bytes(bytes(1280 * 400 * 4 * 2))
    (path / 'status').write_text('progress=34\nstage=fail\nsuccess=0\nmessage=USB missing\n')
    env = dict(os.environ, TEST_FB=str(fb), UN260_BOOT_LIGHT_ACTIVE='1',
               TEST_CLIENT_READY=str(path / 'client-ready'), TEST_CLIENT_RELEASE=str(path / 'release'))
    native = subprocess.Popen([str(executable)], env=env)
    client = None
    try:
        deadline = time.monotonic() + 5
        while not (path / 'ready').exists():
            assert native.poll() is None and time.monotonic() < deadline
            time.sleep(.01)
        # Paused screen must remain alive, without requiring the UI process.
        time.sleep(.3)
        assert native.poll() is None
        client = subprocess.Popen([str(executable), 'client'], env=env)
        assert native.wait(timeout=3) == 0
        deadline = time.monotonic() + 3
        while not (path / 'client-ready').exists():
            assert client.poll() is None and time.monotonic() < deadline
            time.sleep(.01)
        # UI now retains the fd and lease, despite the maintenance exit.
        (path / 'release').touch()
        assert client.wait(timeout=3) == 0
        assert not (path / 'ready').exists() and not (path / 'socket').exists()
    finally:
        (path / 'release').touch()
        if client is not None and client.poll() is None:
            client.terminate(); client.wait(timeout=3)
        if native.poll() is None:
            native.terminate(); native.wait(timeout=3)
print('PASS maintenance pause display, native-to-LVGL lease/fd transfer, cleanup; fake framebuffer only')
