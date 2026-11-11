#!/usr/bin/env python3
"""Production UPK builder and factory contract in isolated filesystems."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from boot_bundle import sha, TREE, ACTIVE
from build_delta import load
from verify_direct_release import verify
from factory_layout import APP
from test_storage_sync import native_tool
from release_manifest import CORE, CORE_KEYS, CORE_DESCRIPTOR, RUNTIME, RUNTIME_DESCRIPTOR, RESCUE, RESCUE_NAMES, PLATFORM

SDK = Path(__file__).resolve().parents[2]
BUILDER = SDK / 'tools/un260-update/build_un260_update.sh'
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'


def recovery_display_fixture():
    # Structural static RISC-V fixture only; never executed as target code.
    data = bytearray(124)
    data[:7] = b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHIQQQIHHHHHH', data, 16, 2, 243, 1, 0x10000, 64, 0, 0, 64, 56, 1, 0, 0, 0)
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 120, 0x10000, 0x10000, 4, 4, 1)
    return bytes(data)


def fixture_target(target):
    files = {
        APP: (b'NEW LEGACY TEST APP', 0o755),
        'usr/local/lib/liblvgl.so': (b'NEW LIBRARY', 0o755),
        'usr/local/share/lvgl_data/locale_fonts/fixture.bin': (b'{"fixture":true}', 0o644),
        'usr/local/bin/un260_storage_sync': (native_tool().read_bytes(), 0o755),
        'usr/local/bin/un260_resource_cleanup': (b'#!/bin/sh\n# UN260_STORAGE_SYNC_1\nexit 0\n', 0o755),
        'usr/local/bin/un260_upgrade_display': (b'fixture-display', 0o755),
        'usr/local/bin/un260_runtime_recover': (b'fixture-recover', 0o755),
        'usr/local/bin/un260_unpack': (b'UNPACKER STRUCTURAL FIXTURE', 0o755),
        'usr/bin/ui_update.sh': (UPDATER.read_bytes(), 0o755),
        'etc/init.d/S00lvgl': ((SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes(), 0o755),
        'usr/local/bin/un260_app_storage': ((SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage').read_bytes(), 0o755),
    }
    helper = recovery_display_fixture()
    files['usr/local/lib/un260-startup/trial-display'] = (helper, 0o755)
    runtime = (SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes()
    files[RUNTIME] = (runtime.replace(b'@TRIAL_DISPLAY_SHA256@', sha(helper).encode()), 0o755)
    core_manifest = ('UN260_UPDATE_CORE_V1\n' + ''.join(key + sha(files[name][0]) + '\n'
                     for key, name in zip(CORE_KEYS, CORE))).encode('ascii')
    files[CORE_DESCRIPTOR] = (core_manifest, 0o644)
    files[RUNTIME_DESCRIPTOR] = ((sha(files[RUNTIME][0]) + '\n').encode('ascii'), 0o644)
    for name, (data, mode) in files.items():
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode)
    factory = {'usr/local/bin/un260_update_dispatch': (b'FIXED DISPATCH STRUCTURAL FIXTURE', 0o755),
               'etc/un260/updater-platform': ((PLATFORM + '\n').encode('ascii'), 0o644),
               RESCUE + '/manifest': (core_manifest, 0o644),
               RESCUE + '/startup-entry.sha256': ((sha(files['etc/init.d/S00lvgl'][0]) + '\n').encode('ascii'), 0o644)}
    for original, name in zip(CORE, RESCUE_NAMES):
        factory[RESCUE + '/' + name] = (files[original][0], 0o600 if name == 'engine.sh' else 0o755)
    for name, data in (('startup.sh', files[RUNTIME][0]), ('watch.sh', b'#!/bin/sh\n# frozen watchdog fixture\n'),
                       ('display', recovery_display_fixture())):
        factory[RESCUE + '/' + name] = (data, 0o755)
        factory[RESCUE + '/' + name.split('.')[0] + '.sha256'] = ((sha(data) + '\n').encode('ascii'), 0o644)
    for name, (data, mode) in factory.items():
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode)
    # Valid structural static ELF/header fixture. It is never executed.
    elf = bytearray(120)
    elf[:7] = b'\x7fELF\x02\x01\x01'
    struct.pack_into('<Q', elf, 32, 64)
    struct.pack_into('<HH', elf, 54, 56, 1)
    struct.pack_into('<I', elf, 64, 1)
    player = bytes(elf)
    theme = b'UNBTPK1\0' + bytes(20) + struct.pack('<I', 1)
    identity = sha(('player_sha256=' + sha(player) + '\ntheme_sha256=' + sha(theme) + '\nmode=animation\n').encode())
    manifest = ('schema=un260-boot-bundle-v1\nid=' + identity + '\nprotocol=1\nplayer_sha256=' + sha(player) +
                '\nplayer_bytes=120\ntheme_sha256=' + sha(theme) + '\ntheme_bytes=32\nmode=animation\n').encode()
    for name, data, mode in [('player', player, 0o755), ('theme.pack', theme, 0o644), ('manifest', manifest, 0o644)]:
        path = target / TREE / identity / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode)
    active = target / ACTIVE
    active.parent.mkdir(parents=True, exist_ok=True)
    active.write_text('UN260_BOOT_ACTIVE_V1\n' + identity + '\n' + sha(manifest) + '\nnone\nnone\n')
    active.chmod(0o644)
    return files


def package(target, output, extra=None):
    command = ['bash', str(BUILDER), '--version', 'factory-fixture', '--output', str(output)]
    if extra:
        command += ['--extra-root', str(extra)]
    env = dict(os.environ, TARGET_DIR=str(target))
    env.pop('UN260_UPDATE_BASELINE', None)
    return subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)


class ReleasePackageTests(unittest.TestCase):
    def test_builder_accepts_only_the_factory_full_profile(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            target = root / 'target'
            fixture_target(target)
            output = root / 'full.upk'
            result = package(target, output)
            self.assertEqual(result.returncode, 0, result.stderr)
            meta, files = load(output)
            self.assertGreater(verify(output, target), 0)
            self.assertNotIn('release_profile', meta)
            self.assertIn('usr/local/lib/un260-startup/trial-display', files)
            previous = output.read_bytes()
            for flag in ('--legacy-root', '--unified-route', '--legacy-test'):
                result = subprocess.run(['bash', str(BUILDER), '--output', str(output), flag],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0, flag)
                self.assertIn('Unknown option', result.stderr)
                self.assertEqual(output.read_bytes(), previous)

    def test_extra_root_cannot_replace_factory_launcher(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            target = root / 'target'
            fixture_target(target)
            extra = root / 'extra'
            path = extra / 'usr/local/bin/un260_boot_launch'
            path.parent.mkdir(parents=True)
            path.write_bytes(b'UNEXPECTED')
            output = root / 'full.upk'
            result = package(target, output, extra=extra)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
