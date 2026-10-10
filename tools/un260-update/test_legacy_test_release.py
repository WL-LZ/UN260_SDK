#!/usr/bin/env python3
"""Old-base application profile: real builders/parser, isolated filesystems."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from boot_bundle import sha, TREE, ACTIVE
from build_delta import load
from build_unified import build as unified, POLICY_V3, NESTED
from release_profile import check_profile
from verify_direct_release import verify
from test_app_volume_updater import VolumeUpdaterTests as Base, APP
from test_storage_sync import native_tool
from release_manifest import CORE, CORE_KEYS, CORE_DESCRIPTOR, RUNTIME, RUNTIME_DESCRIPTOR, RESCUE, RESCUE_NAMES, PLATFORM

SDK = Path(__file__).resolve().parents[2]
BUILDER = SDK / 'tools/un260-update/build_un260_update.sh'
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
OLD_HASH = '4fb0c53d1cc92d1027482d539d17792665a02980b905b07435fdafe4b348ef22'


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
        'usr/local/share/lvgl_data/locale_fonts/manifest.json': (b'{"fixture":true}', 0o644),
        'usr/local/bin/un260_storage_sync': (native_tool().read_bytes(), 0o755),
        'usr/local/bin/un260_resource_cleanup': (b'#!/bin/sh\n# UN260_STORAGE_SYNC_1\nexit 0\n', 0o755),
        'usr/local/bin/un260_upgrade_display': (b'fixture-display', 0o755),
        'usr/local/bin/un260_runtime_recover': (b'fixture-recover', 0o755),
        'usr/local/bin/un260_unpack': (b'UNPACKER STRUCTURAL FIXTURE', 0o755),
        'usr/bin/ui_update.sh': (UPDATER.read_bytes(), 0o755),
        'etc/init.d/S00lvgl': ((SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes(), 0o755),
        'usr/local/bin/un260_app_storage': ((SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage').read_bytes(), 0o755),
    }
    files[RUNTIME] = ((SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes(), 0o755)
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


def package(target, output, legacy=True, extra=None):
    command = ['bash', str(BUILDER), '--version', 'legacy-fixture', '--output', str(output), '--unified-route']
    if legacy:
        command += ['--legacy-test']
    if extra:
        command += ['--extra-root', str(extra)]
    env = dict(os.environ, TARGET_DIR=str(target))
    env.pop('UN260_UPDATE_BASELINE', None)
    return subprocess.run(command, env=env, capture_output=True, text=True, timeout=30)


class BuilderProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-legacy-profile-')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.target = self.path / 'target'
        fixture_target(self.target)
        self.output = self.path / 'application.upk'

    def test_explicit_profile_uses_same_app_without_changing_factory_target(self):
        before = {str(p.relative_to(self.target)): p.read_bytes() for p in self.target.rglob('*') if p.is_file()}
        result = package(self.target, self.output)
        self.assertEqual(result.returncode, 0, result.stderr)
        meta, files = load(self.output)
        check_profile(meta, files, legacy_test=True)
        self.assertEqual(verify(self.output, self.target, legacy_test=True), len(files))
        with self.assertRaisesRegex(ValueError, 'Explicit --legacy-test'):
            verify(self.output, self.target)
        self.assertEqual(files[APP][0], before[APP])
        self.assertEqual(before, {str(p.relative_to(self.target)): p.read_bytes() for p in self.target.rglob('*') if p.is_file()})

    def test_default_factory_profile_still_has_new_guard_and_bundle(self):
        result = package(self.target, self.output, legacy=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        meta, files = load(self.output)
        self.assertEqual(meta['storage_guard'], 'syncfs-boot-bundle-v1')
        self.assertTrue(meta['boot_bundle_id'])
        self.assertNotIn('release_profile', meta)
        self.assertEqual(verify(self.output, self.target), len(files))

    def test_early_boot_extras_rejected_before_output_publication(self):
        for name in ('usr/local/bin/un260_boot_light', 'usr/local/bin/un260_boot_player',
                     'usr/local/bin/un260_boot_launch', 'etc/un260/boot-mode',
                     'etc/un260/boot-active.previous', 'usr/local/lib/un260-boot/versions/fixture/player',
                     'usr/local/bin/early_init'):
            with self.subTest(name=name):
                extra = self.path / ('extra-' + str(len(list(self.path.glob('extra-*')))))
                path = extra / name
                path.parent.mkdir(parents=True)
                path.write_bytes(b'FORBIDDEN')
                result = package(self.target, self.output, extra=extra)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(self.output.exists())

    def test_unified_propagates_profile_and_exact_application(self):
        self.assertEqual(package(self.target, self.output).returncode, 0)
        outer = self.path / 'UN260_UPDATE.upk'
        unified(self.output, outer, POLICY_V3)
        meta, files = load(outer)
        self.assertEqual(meta['release_profile'], 'legacy-test')
        self.assertEqual(meta['storage_guard'], 'syncfs-v1')
        self.assertEqual(files[NESTED + 'application.upk'][0], self.output.read_bytes())
        result = subprocess.run(['python3', str(SDK / 'tools/un260-update/verify_unified_release.py'),
                                 '--unified', str(outer), '--full', str(self.output), '--target', str(self.target),
                                 '--migration-policy', POLICY_V3, '--legacy-test'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


class LegacyTransactions(Base):
    def setUp(self):
        super().setUp()
        self.target = self.p / 'target'
        fixture_target(self.target)
        self.legacy = self.p / 'legacy-application.upk'
        result = package(self.target, self.legacy)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.outer = self.p / 'legacy-unified.upk'
        unified(self.legacy, self.outer, POLICY_V3)

    def guarded(self):
        # Directory identities emulate mount/NAND only; held-fd sync/hash and
        # the actual production parser, journal, commit and receipts remain.
        return '''
GUARDED=1
APP_GUARDED=1
SYNC_TOOL=%s
exec 7< "$ROOT_PREFIX" 8< "$USB_MNT" 9< "$APP_VOLUME_DIR"
usb_barrier() { "$SYNC_TOOL" --fd 8 "$USB_MNT"; }
sh() {
    if [ "$1" = "$ROOT_PREFIX/usr/local/bin/un260_app_storage" ] && [ "$2" = --verify-layout ]; then
        echo fixture-layout-verified >> "$LOG"; return 0
    fi
    command sh "$@"
}
df() { printf 'FS Size Used Free Use Mount\\nfixture 200000 1 100000 1%% /\\n'; }
''' % native_tool()

    def actual_old(self):
        name = os.environ.get('UN260_TEST_LEGACY_UPDATER')
        if not name:
            self.skipTest('requires extracted, SHA-pinned Desktop IMG updater')
        path = Path(name)
        self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), OLD_HASH)
        return path

    def test_actual_old_updater_installs_legacy_full(self):
        self.run_update(package=self.legacy, source=self.actual_old(), extra=self.guarded())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'NEW LEGACY TEST APP')
        self.assertFalse((self.root / ACTIVE).exists())

    def test_actual_old_updater_installs_managed_unified_without_migration(self):
        _, log = self.run_update(package=self.outer, source=self.actual_old(), extra=self.guarded())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'NEW LEGACY TEST APP')
        self.assertIn('existing managed application volume; no partition initialization', log)
        self.assertFalse((self.root / ACTIVE).exists())

    def test_new_updater_old_base_installs_managed_unified(self):
        self.run_update(package=self.outer, extra=self.guarded())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'NEW LEGACY TEST APP')
        self.assertFalse((self.root / ACTIVE).exists())

    def test_new_base_refuses_legacy_outer_before_copy(self):
        platform = self.root / 'dev/.un260-boot-platform'
        platform.parent.mkdir(parents=True)
        platform.write_bytes(b'UN260_BOOT_PLATFORM_V1\n')
        _, log = self.run_update(package=self.outer, extra=self.guarded(), ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertIn('Package would downgrade boot bundle recovery', log)
        self.assertFalse((self.root / 'var/lib/un260-updater/transaction').exists())


if __name__ == '__main__':
    unittest.main()
