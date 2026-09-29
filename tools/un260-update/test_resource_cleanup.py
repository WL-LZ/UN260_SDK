#!/usr/bin/env python3
"""Host-only cleanup safety tests. Production script has no root override."""
import hashlib
import importlib.util
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import unittest
from test_storage_sync import native_tool

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('release_resources', HERE / 'release_resources.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class CleanupTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='un260-cleanup-test-')
        self.dir = Path(self.tmp.name)
        self.root = self.dir / 'board'
        self.usb = self.dir / 'usb'
        self.root.mkdir()
        self.usb.mkdir()
        self.mounts = self.dir / 'mounts'
        self.mounts.write_text('/dev/sda1 %s vfat rw 0 0\n' % self.usb)
        self.entries = []
        for rel, data in [('usr/local/share/ge_data/alpha_src.bmp', b'sdk' * 100),
                          ('usr/local/share/ge_data/clock.bmp', b'clock' * 100),
                          ('usr/local/share/lvgl_data/ui_icons/box_18.png', b'icon' * 100)]:
            file = self.root / rel
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_bytes(data)
            self.entries.append(dict(path=rel, sha256=hashlib.sha256(data).hexdigest(), bytes=len(data), mode='0644'))
        self.user = self.root / 'usr/local/share/ge_data/my_picture.png'
        self.user.write_bytes(b'USER-OWNED-DO-NOT-TOUCH')
        self.state = self.root / 'etc/ui_state/count_history/record'
        self.state.parent.mkdir(parents=True)
        self.state.write_bytes(b'BUSINESS-DATA')
        self.script = self.dir / 'cleanup.sh'
        self.app = self.root / 'usr/local/bin/test_lvgl'
        self.app.parent.mkdir(parents=True)
        self.app.write_bytes(b'NEW-APP')
        guard = self.app.parent / 'un260_storage_sync'
        guard.write_bytes(native_tool().read_bytes()); guard.chmod(0o755)
        text = (HERE / 'resource_cleanup.sh.in').read_text()
        manifest = '\n'.join('%s|%d|%s|%s' % (e['sha256'], e['bytes'], e['mode'], e['path']) for e in self.entries)
        # Test-only substitutions in a temporary copy; no escape hatch ships.
        text = text.replace('@MANIFEST@', manifest).replace('@APP_SHA256@', hashlib.sha256(b'NEW-APP').hexdigest())
        text = text.replace('ROOT=/\n', 'ROOT=%s\n' % self.root)
        text = text.replace('USB=/mnt/usb\n', 'USB=%s\n' % self.usb)
        text = text.replace('MOUNTS=/proc/mounts\n', 'MOUNTS=%s\n' % self.mounts)
        text = text.replace('LOCK=/tmp/un260-resource-cleanup.lock\n', 'LOCK=%s\n' % (self.dir / 'lock'))
        text = text.replace('UPDATER_LOCK=/tmp/un260-updater.lock\n', 'UPDATER_LOCK=%s\n' % (self.dir / 'updater-lock'))
        text = text.replace('[ "$(id -u)" = 0 ]', '[ 0 = 0 ]')
        self.script.write_text(text)

    def tearDown(self):
        self.assertEqual(self.user.read_bytes(), b'USER-OWNED-DO-NOT-TOUCH')
        self.assertEqual(self.state.read_bytes(), b'BUSINESS-DATA')
        self.tmp.cleanup()

    def run_script(self, mode, ok=True, env=None):
        shell = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
        process = subprocess.Popen(shell + [str(self.script), mode], text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, env=env, start_new_session=True)
        try:
            output, _ = process.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            # Kill only this test fixture's process group, including a loop in
            # a pipeline child, so a regression cannot leak host processes.
            os.killpg(process.pid, signal.SIGKILL)
            output, _ = process.communicate()
            self.fail('Cleanup did not terminate: ' + output)
        self.assertEqual(process.returncode == 0, ok, output)
        return output

    def busybox_double_root(self, automatic=False):
        # The old fixture used /tmp/... instead of production ROOT=/ and hid
        # the //usr join. Force the actual BusyBox utility, not just its shell.
        text = self.script.read_text().replace('ROOT=%s\n' % self.root,
                                               'ROOT=/%s\n' % self.root)
        text = text.replace('set -eu\n', 'set -eu\ndirname() { busybox dirname "$@"; }\n', 1)
        if automatic:
            text = text.replace(hashlib.sha256(b'NEW-APP').hexdigest(), '-')
        self.script.write_text(text)

    def test_busybox_double_root_check_apply_retry_restore(self):
        self.busybox_double_root()
        self.run_script('--check')
        self.assertTrue(self.file().exists())
        self.run_script('--apply')
        self.assertFalse(self.file().exists())
        self.assertTrue(self.saved().exists())
        self.run_script('--apply')
        self.run_script('--restore')
        self.assertTrue(self.file().exists())
        self.run_script('--restore')

    def test_busybox_double_root_automatic_keeps_ui_assets(self):
        self.busybox_double_root(automatic=True)
        self.run_script('--apply')
        self.assertFalse(self.file().exists())
        self.assertTrue(self.file(2).exists())
        self.run_script('--restore')
        self.assertTrue(self.file().exists())

    def test_busybox_double_root_symlink_source_kept(self):
        self.busybox_double_root()
        self.file().unlink()
        self.file().symlink_to(self.user)
        self.run_script('--apply')
        self.assertTrue(self.file().is_symlink())

    def test_busybox_double_root_symlink_ancestor_kept(self):
        self.busybox_double_root()
        original = self.file().parent
        moved = self.root / 'sdk-preserved'
        original.rename(moved)
        original.symlink_to(moved, target_is_directory=True)
        self.run_script('--apply')
        self.assertTrue(self.file().exists())
        self.assertTrue(self.file(1).exists())

    def test_path_walk_root_forms_and_nonprogress_fail_closed(self):
        source = (HERE / 'resource_cleanup.sh.in').read_text()
        predicate = 'safe_path() {' + source.split('safe_path() {', 1)[1].split('\ncheck_usb()', 1)[0]
        for prefix in ('/', '//', '///', '////'):
            for path in (prefix, prefix + str(self.file()).lstrip('/')):
                result = subprocess.run(['busybox', 'sh', '-c',
                    'dirname() { busybox dirname "$@"; }\n' + predicate + '\nsafe_path "$1"',
                    'test-path', path], capture_output=True, timeout=3)
                self.assertEqual(result.returncode, 0, (path, result.stderr))
        for shim in ('dirname() { printf "%s\\n" "$1"; }', 'dirname() { return 1; }'):
            result = subprocess.run(['busybox', 'sh', '-c', shim + '\n' + predicate + '\nsafe_path /usr/x'],
                                    capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)

    def file(self, i=0):
        return self.root / self.entries[i]['path']

    def saved(self, i=0):
        return self.usb / 'UN260_RESOURCE_BACKUP_20260924/files' / self.entries[i]['path']

    def test_check_no_writes(self):
        self.run_script('--check')
        self.assertTrue(self.file().exists())
        self.assertFalse(self.saved().exists())

    def test_apply_retry_and_restore(self):
        original = self.file().read_bytes()
        self.run_script('--apply')
        self.assertFalse(self.file().exists())
        self.assertEqual(self.saved().read_bytes(), original)
        self.run_script('--apply')
        self.run_script('--restore')
        self.assertEqual(self.file().read_bytes(), original)
        self.run_script('--restore')

    def test_modified_image_is_kept(self):
        self.file().write_bytes(b'USER-REPLACEMENT')
        self.assertIn('KEEP unknown/modified', self.run_script('--apply'))
        self.assertEqual(self.file().read_bytes(), b'USER-REPLACEMENT')

    def test_unmounted_usb_refused(self):
        self.mounts.write_text('')
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_writeback_failure_keeps_sources(self):
        guard = self.app.parent / 'un260_storage_sync'
        guard.write_text('#!/bin/sh\n[ "$1" = --probe ] && { echo UN260_STORAGE_SYNC_1; exit 0; }\nexit 1\n')
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_direct_read_failure_keeps_sources(self):
        guard = self.app.parent / 'un260_storage_sync'
        guard.write_text('#!/bin/sh\ncase "$1" in --probe) echo UN260_STORAGE_SYNC_1;; --fd) exit 0;; *) exit 1;; esac\n')
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_symlink_source_kept(self):
        self.file().unlink()
        self.file().symlink_to(self.user)
        self.run_script('--apply')
        self.assertTrue(self.file().is_symlink())

    def test_symlink_backup_refused(self):
        backup = self.usb / 'UN260_RESOURCE_BACKUP_20260924'
        backup.symlink_to(self.root)
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_backup_collision_no_deletion(self):
        self.saved(1).parent.mkdir(parents=True)
        self.saved(1).write_bytes(b'WRONG-BACKUP')
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())
        self.assertTrue(self.file(1).exists())

    def test_backup_copy_failure_no_deletion(self):
        # BusyBox ash may dispatch applets internally, bypassing a PATH shim.
        self.script.write_text(self.script.read_text().replace('set -eu\n', 'set -eu\ncp() { return 1; }\n', 1))
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_insufficient_usb_space_no_deletion(self):
        self.script.write_text(self.script.read_text().replace('set -eu\n',
            "set -eu\ndf() { printf 'Filesystem Blocks Used Available Use Mount\\n/dev/sda1 100 100 0 100 /usb\\n'; }\n", 1))
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_symlink_plan_no_deletion(self):
        plan = self.usb / 'UN260_RESOURCE_BACKUP_20260924/plan.pending'
        plan.parent.mkdir(parents=True)
        plan.symlink_to(self.user)
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_restore_never_overwrites_new_user_file(self):
        self.run_script('--apply')
        self.file().write_bytes(b'NEW-USER-IMAGE')
        self.run_script('--restore', False)
        self.assertEqual(self.file().read_bytes(), b'NEW-USER-IMAGE')
        self.assertFalse(self.file(1).exists())

    def test_pending_upgrade_refused(self):
        journal = self.root / 'var/lib/un260-updater/transaction'
        journal.parent.mkdir(parents=True)
        journal.write_text('unfinished')
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_updater_in_preflight_refused(self):
        (self.dir / 'updater-lock').mkdir()
        self.run_script('--apply', False)
        self.assertTrue(self.file().exists())

    def test_cleanup_releases_shared_lock_after_failure(self):
        self.mounts.write_text('')
        self.run_script('--apply', False)
        self.assertFalse((self.dir / 'updater-lock').exists())

    def test_build_prune_prevalidates_all_files(self):
        self.file(1).write_bytes(b'CHANGED')
        with self.assertRaises(RuntimeError):
            release.prune(self.root, self.entries)
        self.assertTrue(self.file().exists())

    def test_build_prune_symlink_refused(self):
        self.file().unlink()
        self.file().symlink_to(self.user)
        with self.assertRaises(RuntimeError):
            release.prune(self.root, self.entries)

    def test_build_prune_exact_files_only(self):
        self.assertEqual(release.prune(self.root, self.entries), 3)
        self.assertFalse(self.file().exists())
        self.assertEqual(release.prune(self.root, self.entries), 0)

    def test_old_firmware_keeps_icons_then_retry_removes(self):
        self.app.write_bytes(b'OLDER-APP')
        self.run_script('--apply')
        self.assertFalse(self.file().exists())
        self.assertTrue(self.file(2).exists())
        self.app.write_bytes(b'NEW-APP')
        self.run_script('--apply')
        self.assertFalse(self.file(2).exists())
        self.assertTrue(self.saved(2).exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
