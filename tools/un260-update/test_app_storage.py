#!/usr/bin/env python3
"""Host-only initialization/recovery fault injection; NO real MTD commands.

NAND and UBI are fixture functions. Does not establish physical power-cut safety.
"""
import os
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_storage_sync import native_tool

SDK = Path(__file__).resolve().parents[2]
HELPER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
ORIGINAL_APP = b'\x7fELF\x01\x01\x01' + b'\0' * 57 + b'original-device-app'


class StorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-volume-test-')
        self.p = Path(self.temp.name)
        self.app = self.p / 'root/usr/local/bin/test_lvgl'
        self.app.parent.mkdir(parents=True)
        self.app.write_bytes(ORIGINAL_APP); self.app.chmod(0o755)
        self.state = self.p / 'root/etc/un260/app-storage'
        self.vol = self.p / 'volume'; self.vol.mkdir()
        self.usb = self.p / 'usb'; self.usb.mkdir()
        self.dev = self.p / 'dev'; self.dev.mkdir()
        self.nand = self.dev / 'mtd11'; self.nand.write_bytes(b'old NAND' * 512)
        for key, value in {'mtd/mtd11/name': 'ubisystem', 'mtd/mtd11/size': str(self.nand.stat().st_size),
                           'mtd/mtd11/erasesize': '131072', 'mtd/mtd11/writesize': '2048',
                           'mtd/mtd10/name': 'ubiroot', 'mtd/mtd10/size': '67108864',
                           'ubi/ubi0/mtd_num': '10'}.items():
            f = self.p / 'sys/class' / key; f.parent.mkdir(parents=True, exist_ok=True); f.write_text(value+'\n')
        (self.p / 'proc').mkdir()
        (self.p / 'proc/mounts').write_text('ubi0:rootfs / ubifs rw 0 0\n')
        (self.p / 'lock').mkdir(); (self.p / 'lock/pid').write_text('123\n')
        self.user = self.p / 'root/etc/ui_state/business'
        self.user.parent.mkdir(parents=True); self.user.write_bytes(b'USER-DATA')
        self.business_files = {}
        for name in ('count_history/record.bin', 'cashbook/book.json', 'workspace/draft.json',
                     'standby/photo.bin', 'photos/local.png', 'configs/settings.json',
                     'touch_calibration.cfg'):
            item = self.user.parent / name
            item.parent.mkdir(parents=True, exist_ok=True)
            item.write_bytes(('PRESERVE ' + name).encode())
            self.business_files[item] = item.read_bytes()
        calibration = self.p / 'root/etc/pointercal'
        calibration.write_bytes(b'PRESERVE TOUCH CALIBRATION')
        self.business_files[calibration] = calibration.read_bytes()

    def tearDown(self):
        self.assertEqual(self.user.read_bytes(), b'USER-DATA')
        for item, data in self.business_files.items():
            self.assertEqual(item.read_bytes(), data)
        self.temp.cleanup()

    def provision_factory_volume(self):
        self.state.mkdir(parents=True, exist_ok=True)
        (self.state / 'active').write_text('UN260_APP_VOLUME_V1\n')
        (self.vol / '.un260-owner').write_text('UN260_APP_VOLUME_V1\n')
        (self.vol / 'test_lvgl').write_bytes(ORIGINAL_APP)
        (self.vol / 'test_lvgl').chmod(0o755)
        self.app.write_text('#!/bin/sh\nexec /usr/local/bin/un260_app_storage --launch "$@"\n')
        self.app.chmod(0o755)
        for rel, value in (('ubi1/mtd_num', '11'), ('ubi1_0/name', 'un260_app')):
            item = self.p / 'sys/class/ubi' / rel
            item.parent.mkdir(parents=True, exist_ok=True)
            item.write_text(value + '\n')
        with (self.p / 'proc/mounts').open('a') as mounts:
            mounts.write(f'ubi1:un260_app {self.vol} ubifs rw 0 0\n')

    def run_helper(self, action, extra='', ok=True):
        overrides = {'SYS': self.p / 'sys', 'PROC': self.p / 'proc', 'DEV': self.dev,
                     'VOLUME': self.vol, 'STATE': self.state, 'APP': self.app, 'RAW_BYTES': 4096,
                     'UPDATER_STATE': self.p / 'root/var/lib/un260-updater', 'LOCK': self.p / 'lock'}
        variables = '\n'.join(f'{key}="{value}"' for key, value in overrides.items())
        runner = ('set -eu\nUN260_STORAGE_LIBRARY_ONLY=1\n. "' + str(HELPER) +
                  '"\n' + variables + '\n' + extra + '\n' + action)
        result = subprocess.run(SHELL, input=runner, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
        return result

    def test_symlink_metadata_refused(self):
        self.state.mkdir(parents=True)
        victim = self.p / 'victim'; victim.write_text('KEEP')
        (self.state / 'pending.tmp').symlink_to(victim)
        self.run_helper(action="prepare", ok=False)
        self.assertEqual(victim.read_text(), 'KEEP')
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_factory_enrollment_boots_without_usb_and_never_formats(self):
        from factory_layout import enroll, APP, OWNER
        base = self.p / 'build-target'
        (base / APP).parent.mkdir(parents=True)
        elf = b'\x7fELF-factory-application'
        (base / APP).write_bytes(elf)
        self.app.write_bytes(elf)
        # Factory input cannot contain customer data. Restore this test's
        # sentinel afterwards to also test that boot leaves customer data alone.
        self.user.unlink()
        for item in self.business_files:
            if self.user.parent in item.parents:
                item.unlink()
        for directory in sorted((p for p in self.user.parent.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
            directory.rmdir()
        self.user.parent.rmdir()
        backing = self.p / 'factory-volume'
        enroll(self.p / 'root', base, backing, 'factory-test')
        self.user.parent.mkdir(parents=True); self.user.write_bytes(b'USER-DATA')
        for item, data in self.business_files.items():
            item.parent.mkdir(parents=True, exist_ok=True); item.write_bytes(data)
        self.usb.rmdir()
        self.run_helper(action='prepare; prepare', extra='''
require_usb() { echo UNEXPECTED_USB >&2; exit 98; }
ubiformat() { echo UNEXPECTED_FORMAT >&2; exit 99; }
ubiattach() {
    mkdir -p "$SYS/class/ubi/ubi1" "$SYS/class/ubi/ubi1_0"
    printf '11\\n' > "$SYS/class/ubi/ubi1/mtd_num"
    printf 'un260_app\\n' > "$SYS/class/ubi/ubi1_0/name"
}
mount() {
    [ "$1" = -t ] && [ "$2" = ubifs ] && [ "$3" = ubi1:un260_app ] && [ "$4" = "$VOLUME" ] || exit 92
    cp "$VOLUME/../factory-volume/test_lvgl" "$VOLUME/test_lvgl"
    cp "$VOLUME/../factory-volume/.un260-owner" "$VOLUME/.un260-owner"
    printf 'ubi1:un260_app %s ubifs rw 0 0\\n' "$VOLUME" >> "$PROC/mounts"
}
''')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), elf)
        self.assertEqual((self.vol / '.un260-owner').read_bytes(), OWNER)
        self.assertEqual((self.state / 'active').read_bytes(), OWNER)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertFalse(self.usb.exists())
        (self.vol / '.un260-owner').write_bytes(b'OTHER APPLICATION')
        self.run_helper(action='prepare', ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def query_public(self, action, sync_tool=None, engine_sync=None):
        # Execute the actual public query, including its early dispatch before
        # the writeback guard; only path constants point into this fixture.
        script = HELPER.read_text()
        for old, new in [('STATE=/etc/un260/app-storage', 'STATE=' + str(self.state)),
                         ('APP=/usr/local/bin/test_lvgl', 'APP=' + str(self.app)),
                         ('LOCK=/tmp/un260-updater.lock', 'LOCK=' + str(self.p / 'lock')),
                         ('UPDATER_STATE=/var/lib/un260-updater', 'UPDATER_STATE=' + str(self.p / 'root/var/lib/un260-updater')),
                         ('SYS=/sys', 'SYS=' + str(self.p / 'sys')),
                         ('PROC=/proc', 'PROC=' + str(self.p / 'proc')),
                         ('DEV=/dev', 'DEV=' + str(self.dev)),
                         ('VOLUME=/mnt/un260-app', 'VOLUME=' + str(self.vol)),
                         ('RAW_BYTES=39845888', 'RAW_BYTES=4096'),
                         ('SYNC_TOOL=/usr/local/bin/un260_storage_sync', 'SYNC_TOOL=' + str(sync_tool or '/nonexistent-for-readonly-query'))]:
            script = script.replace(old, new)
        path = self.p / 'query-helper'; path.write_text(script)
        env = dict(os.environ)
        env.pop('UN260_ENGINE_SYNC', None)
        if engine_sync is not None:
            env['UN260_ENGINE_SYNC'] = str(engine_sync)
        return subprocess.run(SHELL + [str(path), action], capture_output=True, text=True, env=env)

    def test_current_query_managed_is_read_only_and_lock_sensitive(self):
        self.provision_factory_volume()
        self.vol.joinpath('test_lvgl').write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        before = {str(p): p.read_bytes() for p in self.p.rglob('*') if p.is_file() and not p.is_symlink()}
        # Layout verification is used under an updater lock; fallback launching
        # is forbidden until the updater has released its transaction/lock.
        self.assertEqual(self.query_public('--verify-layout').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.assertEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertNotEqual(self.query_public("--can-launch-legacy").returncode, 0)
        for path, data in before.items():
            if '/lock/' not in path:
                self.assertEqual(Path(path).read_bytes(), data)
        for artifact in (self.vol / 'test_lvgl.un260-new', self.vol / 'test_lvgl.migration-new', self.state / 'pending.tmp'):
            artifact.write_bytes(b'PENDING')
            self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
            artifact.unlink()

    def test_current_query_rejects_unowned_ro_malformed_managed_app(self):
        self.provision_factory_volume()
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.vol.joinpath('test_lvgl').write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        self.assertEqual(self.query_public('--can-launch-current').returncode, 0)
        owner = self.vol / '.un260-owner'
        owner.write_text('OTHER')
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        owner.write_text('UN260_APP_VOLUME_V1\n')
        mounts = self.p / 'proc/mounts'
        original_mounts = mounts.read_text()
        mounts.write_text(original_mounts.replace('ubifs rw', 'ubifs ro'))
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        mounts.write_text(original_mounts)
        self.vol.joinpath('test_lvgl').write_bytes(b'#!/bin/sh\nexit 0\n')
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertFalse((self.state / 'pending').exists())

    def prepare_public(self, action='--prepare-storage', engine_sync=None):
        return self.query_public(action, sync_tool=native_tool(), engine_sync=engine_sync)

    def release_fixture_lock(self):
        self.p.joinpath('lock/pid').unlink()
        self.p.joinpath('lock').rmdir()

    def engine_snapshot(self):
        result = subprocess.run(['mktemp', '-d', '/tmp/un260-update-engine.XXXXXX'],
                                capture_output=True, text=True, check=True)
        context = Path(result.stdout.strip())
        (self.p / 'lock/pid').write_text(str(os.getpid()) + '\n')
        self.addCleanup(lambda: shutil.rmtree(context) if context.exists() else None)
        tool = context / 'sync'
        shutil.copyfile(native_tool(), tool)
        tool.chmod(0o755)
        return context, tool

    def test_prepare_storage_missing_app_allows_repair_but_not_launch(self):
        self.provision_factory_volume()
        self.vol.joinpath('test_lvgl').unlink()
        self.usb.rename(self.p / 'removed-usb')
        self.release_fixture_lock()
        self.assertEqual(self.prepare_public().returncode, 0)
        self.assertNotEqual(self.prepare_public('--prepare').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertFalse(self.vol.joinpath('test_lvgl').exists())
        self.assertFalse(self.dev.joinpath('format-calls').exists())

    def test_prepare_storage_nonexecutable_app_allows_repair_but_not_launch(self):
        self.provision_factory_volume()
        self.vol.joinpath('test_lvgl').chmod(0o644)
        self.release_fixture_lock()
        self.assertEqual(self.prepare_public().returncode, 0)
        self.assertNotEqual(self.prepare_public('--prepare').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertEqual(self.vol.joinpath('test_lvgl').stat().st_mode & 0o777, 0o644)

    def test_prepare_storage_corrupt_regular_app_retains_prepare_semantics(self):
        self.provision_factory_volume()
        self.vol.joinpath('test_lvgl').write_bytes(b'broken application bytes')
        self.release_fixture_lock()
        self.assertEqual(self.prepare_public().returncode, 0)
        # --prepare retains the original regular+x contract; query checks ELF.
        self.assertEqual(self.prepare_public('--prepare').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)

    def test_prepare_storage_rejects_unsafe_app_shapes(self):
        self.provision_factory_volume()
        app = self.vol / 'test_lvgl'
        original = app.read_bytes()
        for shape in ('symlink', 'directory', 'fifo'):
            with self.subTest(shape=shape):
                app.unlink()
                if shape == 'symlink': app.symlink_to(self.app)
                elif shape == 'directory': app.mkdir()
                else: os.mkfifo(app)
                self.assertNotEqual(self.prepare_public().returncode, 0)
                if shape == 'directory': app.rmdir()
                else: app.unlink()
                app.write_bytes(original); app.chmod(0o755)

    def test_prepare_storage_rejects_wrong_owner_geometry_and_mount(self):
        self.provision_factory_volume()
        owner = self.vol / '.un260-owner'
        owner.write_text('OTHER')
        self.assertNotEqual(self.prepare_public().returncode, 0)
        owner.write_text('UN260_APP_VOLUME_V1\n')
        geometry = self.p / 'sys/class/mtd/mtd11/size'
        geometry.write_text('999\n')
        self.assertNotEqual(self.prepare_public().returncode, 0)
        geometry.write_text('4096\n')
        mounts = self.p / 'proc/mounts'
        original = mounts.read_text()
        for broken in (original.replace('ubifs rw', 'ubifs ro'),
                       original.replace('ubi1:un260_app', 'other-volume')):
            mounts.write_text(broken)
            self.assertNotEqual(self.prepare_public().returncode, 0)
        mounts.write_text(original)
        self.assertEqual(self.prepare_public().returncode, 0)

    def test_prepare_storage_rejects_root_launcher_and_marker_corruption(self):
        self.provision_factory_volume()
        original = self.app.read_bytes()
        self.app.write_bytes(b'not managed launcher')
        self.assertEqual(self.prepare_public().returncode, 0)
        self.assertNotEqual(self.prepare_public('--prepare').returncode, 0)
        self.app.unlink()
        self.assertEqual(self.prepare_public().returncode, 0)
        self.assertNotEqual(self.prepare_public('--prepare').returncode, 0)
        self.app.write_bytes(original); self.app.chmod(0o755)
        active = self.state / 'active'
        active.write_text('WRONG')
        self.assertNotEqual(self.prepare_public().returncode, 0)

    def test_prepare_storage_preserves_pending_migration_without_advancing(self):
        self.provision_factory_volume()
        for name in ('pending', 'pending.tmp', 'active.tmp'):
            with self.subTest(name=name):
                marker = self.state / name
                marker.write_bytes(b'PRESERVE INTENT')
                self.assertNotEqual(self.prepare_public().returncode, 0)
                self.assertEqual(marker.read_bytes(), b'PRESERVE INTENT')
                marker.unlink()
        self.assertFalse(self.dev.joinpath('format-calls').exists())

    def test_prepare_storage_before_enrollment_never_initializes(self):
        self.app.unlink()
        self.assertNotEqual(self.prepare_public().returncode, 0)
        self.assertFalse(self.p.joinpath('sys/class/ubi/ubi1').exists())
        self.assertFalse(self.dev.joinpath('format-calls').exists())
        self.assertFalse(self.state.exists())

    def test_retired_migration_entry_cannot_write_raw_storage(self):
        before = self.nand.read_bytes()
        self.assertNotEqual(self.query_public('--migrate').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-legacy').returncode, 0)
        self.assertEqual(self.nand.read_bytes(), before)
        self.assertFalse(self.state.exists())

    def test_prepare_storage_rejects_metadata_symlink_and_nondirectory(self):
        self.provision_factory_volume()
        active = self.state / 'active'
        data = active.read_bytes(); active.unlink()
        active.symlink_to(self.p / 'absent')
        self.assertNotEqual(self.prepare_public().returncode, 0)
        active.unlink(); active.write_bytes(data)
        self.assertEqual(self.prepare_public().returncode, 0)
        # The new route must not silently treat a state file as unenrolled.
        other = self.p / 'state-file'; other.write_bytes(b'unsafe')
        result = self.run_helper(action='STATE="$VOLUME/../state-file"; prepare_storage', ok=False)
        self.assertIn('state directory', result.stderr)

    def test_matching_engine_sync_snapshot_uses_verified_private_tool(self):
        self.provision_factory_volume()
        context, tool = self.engine_snapshot()
        self.assertEqual(self.prepare_public(engine_sync=tool).returncode, 0)
        self.assertTrue(context.is_dir())
        self.assertTrue(tool.is_file())

    def test_matching_engine_sync_rejects_untrusted_context_or_tool_modes(self):
        self.provision_factory_volume()
        context, tool = self.engine_snapshot()
        context.chmod(0o755)
        self.assertNotEqual(self.prepare_public(engine_sync=tool).returncode, 0)
        context.chmod(0o700); tool.chmod(0o644)
        self.assertNotEqual(self.prepare_public(engine_sync=tool).returncode, 0)
        tool.chmod(0o755); tool.unlink(); tool.symlink_to(native_tool())
        self.assertNotEqual(self.prepare_public(engine_sync=tool).returncode, 0)

    def test_matching_engine_sync_rejects_real_nested_six_character_context(self):
        self.provision_factory_volume()
        context, tool = self.engine_snapshot()
        for suffix in ('ab', 'cd', 'ef', 'gh'):
            outer = Path('/tmp/un260-update-engine.' + context.name[-2:] + suffix)
            # Four chars plus slash + one char is exactly six wildcard chars.
            try: outer.mkdir(mode=0o700)
            except FileExistsError: continue
            break
        else: self.fail('cannot allocate isolated nested context')
        self.addCleanup(lambda: shutil.rmtree(outer))
        nested = outer / 'x'; nested.mkdir(mode=0o700)
        nested_tool = nested / 'sync'
        shutil.copyfile(native_tool(), nested_tool); nested_tool.chmod(0o755)
        self.assertNotEqual(self.prepare_public(engine_sync=nested_tool).returncode, 0)

    def test_matching_engine_sync_rejects_unmatched_parent_lock(self):
        self.provision_factory_volume()
        context, tool = self.engine_snapshot()
        (self.p / 'lock/pid').write_text('1\n')
        self.assertNotEqual(self.prepare_public(engine_sync=tool).returncode, 0)
        (self.p / 'lock/pid').write_text(str(os.getpid()) + '\n')
        self.assertEqual(self.prepare_public(engine_sync=tool).returncode, 0)

    def test_matching_engine_sync_rejects_paths_and_bad_probe(self):
        self.provision_factory_volume()
        for value in ('', '/tmp/sync', '/usr/local/bin/un260_storage_sync',
                      '/tmp/un260-update-engine.abcdefg/sync',
                      '/tmp/un260-update-engine.abcdef/sub/sync'):
            with self.subTest(value=value):
                self.assertNotEqual(self.prepare_public(engine_sync=value).returncode, 0)
        context, tool = self.engine_snapshot()
        tool.write_text('#!/bin/sh\necho WRONG_PROBE\n'); tool.chmod(0o755)
        self.assertNotEqual(self.prepare_public(engine_sync=tool).returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
