#!/usr/bin/env python3
"""One physical bundle across old bootstrap, reboot continuation and failures.

Real parser/transactions, host directory volume fixture. No board/NAND claims.
"""
import hashlib
import io
import os
from pathlib import Path
import tarfile
import unittest
from test_app_volume_updater import VolumeUpdaterTests as Base, APP
from test_storage_bootstrap import OLD_UPDATER, SDK, UPDATER_REL
from build_unified import build, REQUEST, DISPLAY, APPROVED, BRIDGE, NESTED
from test_storage_sync import ui_fingerprint

NO_OPTIONAL_TOOLS = '''
cmp() { echo UNEXPECTED_CMP >&2; return 127; }
stat() { echo UNEXPECTED_STAT >&2; return 127; }
tail() { echo UNEXPECTED_TAIL >&2; return 127; }
'''


class UnifiedTests(Base):
    def setUp(self):
        super().setUp()
        self.files['usr/local/bin/un260_resource_cleanup'] = (b'#!/bin/sh\n# UN260_STORAGE_SYNC_1\nexit 0\n', 0o755)
        self.files[DISPLAY] = (b'DISPLAY', 0o755)
        # Emulate the migration boundary; real helper is independently covered
        # by test_app_storage fault injection and bounded native MTD reads.
        self.files['usr/local/bin/un260_app_storage'] = (
            ('#!/bin/sh\n# UN260_STORAGE_SYNC_1 UN260_MTD_STREAM_1 UN260_APP_VOLUME_V1\n'
             'if [ "$1" = --migrate ]; then\n'
             ' echo migrate >> "%s/migration-calls"\n'
             ' [ ! -f "%s/fail-migration" ] || exit 1\nfi\nexit 0\n' % (self.p, self.p)).encode(), 0o755)
        self.pack()
        self.unified = self.p / 'UN260_UPDATE.upk'
        build(self.full, self.unified, APPROVED)
        self.old = self.p / 'old.sh'; self.old.write_text(OLD_UPDATER)

    def boot(self, ok=True, extra=''):
        # setup functions source current updater exactly as next boot would.
        return self.run_update(package=self.unified, extra='UNIFIED_RESUME=1\n' + extra, ok=ok)

    def bridge(self):
        self.run_update(package=self.unified, source=self.old)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertTrue((self.root / REQUEST).is_file())
        self.assertFalse((self.root / NESTED / 'application.upk').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())

    def real_cleanup_fixture(self):
        """Use the actual helper, including production's //root path shape."""
        demo_rel = 'usr/local/share/ge_data/clock.bmp'
        self.demo = self.root / demo_rel
        self.demo.parent.mkdir(parents=True, exist_ok=True)
        self.demo.write_bytes(b'demo-data')
        self.custom = self.demo.parent / 'custom.png'
        self.custom.write_bytes(b'USER-IMAGE')
        mounts = self.p / 'mounts'
        mounts.write_text('/dev/sda1 %s vfat rw 0 0\n' % self.usb)
        digest = hashlib.sha256(b'demo-data').hexdigest()
        # A realistic-sized fixed manifest, not just the former no-op mock.
        manifest = '%s|9|0644|%s\n' % (digest, demo_rel)
        manifest += ''.join('%s|9|0644|usr/local/share/ge_data/absent_%d.bmp\n' % (digest, i)
                            for i in range(56))
        helper = (SDK / 'tools/un260-update/resource_cleanup.sh.in').read_text()
        for old, new in [('@APP_SHA256@', '-'), ('@MANIFEST@', manifest.rstrip()),
                ('ROOT=/\n', 'ROOT=/%s\n' % self.root),
                ('USB=/mnt/usb\n', 'USB=%s\n' % self.usb),
                ('MOUNTS=/proc/mounts\n', 'MOUNTS=%s\n' % mounts),
                ('LOCK=/tmp/un260-resource-cleanup.lock\n', 'LOCK=%s\n' % (self.p / 'cleanup-lock')),
                ('UPDATER_LOCK=/tmp/un260-updater.lock\n', 'UPDATER_LOCK=%s\n' % (self.root / 'update-lock')),
                ('[ "$(id -u)" = 0 ]', '[ 0 = 0 ]'),
                ('set -eu\n', 'set -eu\ndirname() { busybox dirname "$@"; }\n')]:
            helper = helper.replace(old, new)
        self.files['usr/local/bin/un260_resource_cleanup'] = (helper.encode(), 0o755)
        self.pack()
        build(self.full, self.unified, APPROVED)

    def test_real_cleanup_double_slash_then_application_and_retry(self):
        self.real_cleanup_fixture()
        self.bridge()
        self.boot(extra=NO_OPTIONAL_TOOLS)
        self.assertFalse(self.demo.exists())
        saved = self.usb / 'UN260_RESOURCE_BACKUP_20260924/files/usr/local/share/ge_data/clock.bmp'
        self.assertEqual(saved.read_bytes(), b'demo-data')
        self.assertEqual(self.custom.read_bytes(), b'USER-IMAGE')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertFalse((self.p / 'cleanup-lock').exists())
        self.assertIn('CLEANUP COMPLETE.', (self.root / 'log').read_text())
        self.boot()
        self.assertEqual(saved.read_bytes(), b'demo-data')

    def test_real_cleanup_failure_pauses_before_application(self):
        self.real_cleanup_fixture()
        saved = self.usb / 'UN260_RESOURCE_BACKUP_20260924/files/usr/local/share/ge_data/clock.bmp'
        saved.parent.mkdir(parents=True)
        saved.write_bytes(b'WRONG-BACKUP')
        self.bridge()
        self.boot(ok=False)
        self.assertEqual(self.demo.read_bytes(), b'demo-data')
        self.assertEqual(self.custom.read_bytes(), b'USER-IMAGE')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertIn('Storage maintenance failed', (self.root / 'status').read_text())
        self.assertFalse((self.p / 'cleanup-lock').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/transaction').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())

    @unittest.skipUnless(os.environ.get('UN260_TEST_R5_UPDATER'), 'requires frozen delivered R5 updater')
    def test_paused_r5_accepts_repaired_helper_without_running_old_cleanup(self):
        self.real_cleanup_fixture()
        self.bridge()
        r5 = Path(os.environ['UN260_TEST_R5_UPDATER'])
        self.assertEqual(hashlib.sha256(r5.read_bytes()).hexdigest(),
                         'e73544179d1f61c517cab2b6c3270ea1343ad408a96c7962a1a9b5e414985b18')
        # A known-paused R5 device has no live updater/cleanup and has a
        # pending request for the old package. The manual outer install must
        # replace the helper without executing it or deleting that request.
        installed = self.root / 'usr/local/bin/un260_resource_cleanup'
        installed.write_text('#!/bin/sh\n# UN260_STORAGE_SYNC_1\necho OLD_HELPER_MUST_NOT_RUN >&2\nexit 99\n')
        (self.root / REQUEST).write_text('0' * 64 + '\n' + '1' * 64 + '\n')
        result, log = self.run_update(package=self.unified, source=r5, extra=NO_OPTIONAL_TOOLS)
        self.assertNotIn('OLD_HELPER_MUST_NOT_RUN', result.stderr + log)
        self.assertTrue(self.demo.exists())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.boot(extra=NO_OPTIONAL_TOOLS)
        self.assertFalse(self.demo.exists())
        self.assertEqual(self.custom.read_bytes(), b'USER-IMAGE')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_one_package_old_bootstrap_then_auto_continuation(self):
        self.bridge()
        self.boot()
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        request_hash = hashlib.sha256((self.root / REQUEST).read_bytes()).hexdigest()
        self.assertEqual((self.root / 'var/lib/un260-updater/unified.completed').read_text().strip(), request_hash)
        self.assertIn('success=1', (self.root / 'status').read_text())
        marker = self.root / 'var/lib/un260-updater/installed.fnv64'
        # Boot has no --bundle-fnv parameter. Persist the original public
        # archive identity, never migration.upk/application.upk from inside it.
        self.assertEqual(marker.read_text().strip(), ui_fingerprint(self.unified.read_bytes()))
        self.assertNotEqual(marker.read_text().strip(), ui_fingerprint(self.full.read_bytes()))
        self.run_update(action='! unified_pending')

    def test_failed_fingerprint_does_not_claim_success_or_start_migration(self):
        self.bridge()
        self.boot(ok=False, extra='BUNDLE_PATH="$ROOT_PREFIX/missing-package.upk"\n')
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/installed.fnv64').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertIn('Cannot fingerprint selected USB package', (self.root / 'status').read_text())

    def test_no_pending_request_is_a_normal_boot(self):
        self.run_update(action='! unified_pending')
        self.assertFalse((self.p / 'migration-calls').exists())

    def test_continuation_without_optional_board_commands(self):
        self.bridge()
        result, _ = self.boot(extra=NO_OPTIONAL_TOOLS)
        self.assertNotIn('UNEXPECTED_', result.stderr)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_request_hash_read_failure_is_not_wrong_usb(self):
        self.bridge()
        self.boot(ok=False, extra=NO_OPTIONAL_TOOLS + '''
sha256sum() {
    case "$1" in */etc/un260/unified-request) return 1 ;; esac
    command sha256sum "$@"
}''')
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertIn('Cannot read continuation request', (self.root / 'log').read_text())

    def test_bridge_hash_read_failure_never_starts_migration(self):
        self.bridge()
        self.boot(ok=False, extra=NO_OPTIONAL_TOOLS + '''
sha256sum() {
    case "$1" in */usr/local/bin/un260_upgrade_display) return 1 ;; esac
    command sha256sum "$@"
}''')
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertIn('Cannot read installed bridge component', (self.root / 'log').read_text())

    def test_usb_cleanup_failure_stops_before_mkdir_or_unpack(self):
        self.bridge()
        (self.root / 'log').write_text('')
        result, _ = self.boot(ok=False, extra='''
cleanup_usb_work() { return 1; }
mkdir() { echo UNEXPECTED_STAGING_MKDIR >&2; command mkdir "$@"; }
''')
        self.assertNotIn('UNEXPECTED_STAGING_MKDIR', result.stderr)
        self.assertIn('USB staging cleanup failed', (self.root / 'status').read_text())
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertNotIn('payload/', (self.root / 'log').read_text())

    def test_usb_staging_eio_is_logged_and_preserves_state(self):
        self.bridge()
        request = (self.root / REQUEST).read_bytes()
        self.boot(ok=False, extra='''
mkdir() {
    case "$*" in *.un260_stage*) echo 'mkdir: Input/output error' >&2; return 1 ;; esac
    command mkdir "$@"
}''')
        self.assertIn('Input/output error', (self.root / 'log').read_text())
        self.assertIn('USB staging write failed', (self.root / 'status').read_text())
        self.assertEqual((self.root / REQUEST).read_bytes(), request)
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())

    def test_staging_writeback_failure_prevents_unpack(self):
        self.bridge()
        (self.root / 'log').write_text('')
        self.boot(ok=False, extra='usb_barrier() { return 1; }')
        self.assertIn('USB staging writeback failed', (self.root / 'status').read_text())
        self.assertNotIn('payload/', (self.root / 'log').read_text())
        self.assertFalse((self.p / 'migration-calls').exists())

    @unittest.skipUnless(os.environ.get('UN260_TEST_R4_UPDATER'), 'requires frozen delivered R4 updater')
    def test_paused_r4_accepts_new_bridge_before_restart(self):
        r4 = Path(os.environ['UN260_TEST_R4_UPDATER'])
        self.assertEqual(hashlib.sha256(r4.read_bytes()).hexdigest(),
                         'a4890367e8281e424fd0329d5329c5ee7962966a24ad43465cd7e99886daacc1')
        self.bridge()
        (self.root / UPDATER_REL.split('rootfs_overlay/')[1]).write_bytes(r4.read_bytes())
        (self.root / REQUEST).write_text('0' * 64 + '\n' + '1' * 64 + '\n')
        # Explicit repair from a paused R4 device installs only the new bridge;
        # no deletion of pending requests, no checksum bypass, no NAND writes.
        result, _ = self.run_update(package=self.unified, source=r4, extra=NO_OPTIONAL_TOOLS)
        self.assertNotIn('UNEXPECTED_', result.stderr)
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.boot(extra=NO_OPTIONAL_TOOLS)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_current_updater_bootstraps_same_small_subset(self):
        self.run_update(package=self.unified)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertFalse((self.root / 'var/lib/un260-updater/transaction').exists())
        self.boot()

    def test_changed_request_or_wrong_usb_refused(self):
        self.bridge()
        (self.root / REQUEST).write_text('0' * 64 + '\n' + '1' * 64 + '\n')
        self.boot(ok=False)
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertIn('same USB', (self.root / 'log').read_text())

    def test_normal_package_cannot_replace_pending_unified_request(self):
        self.bridge()
        self.run_update(package=self.full, extra='UNIFIED_RESUME=1', ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_migration_failure_never_publishes_completion(self):
        self.bridge()
        (self.p / 'fail-migration').touch()
        self.boot(ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertNotIn('success=1', (self.root / 'status').read_text())
        (self.p / 'fail-migration').unlink()
        self.boot()

    def test_incomplete_bridge_cannot_start_migration(self):
        self.bridge()
        (self.root / DISPLAY).write_bytes(b'wrong display')
        self.boot(ok=False)
        self.assertFalse((self.p / 'migration-calls').exists())

    def test_final_application_validated_before_any_migration(self):
        self.files['etc/ui_state/forbidden'] = (b'BUSINESS OVERWRITE', 0o644)
        self.pack()
        build(self.full, self.unified, APPROVED)
        self.bridge()
        self.boot(ok=False)
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_resume_after_application_copy_failure_keeps_same_bundle(self):
        self.bridge()
        self.boot(ok=False, extra='''cp() {
    case "$2" in *test_lvgl.un260-new) return 1 ;; esac
    command cp "$@"
}''')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.boot()
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_usb_flush_failure_after_install_never_completes(self):
        self.bridge()
        self.boot(ok=False, extra='usb_barrier() { [ "$UNIFIED_RUNNING" != 0 ] || [ "$LAST_STAGE" != finish ]; }')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertIn('success=0', (self.root / 'status').read_text())

    def test_missing_or_other_fingerprint_rejected_by_builder(self):
        before = self.unified.read_bytes()
        with self.assertRaises(ValueError): build(self.full, self.unified, '0' * 64)
        self.assertEqual(before, self.unified.read_bytes())

    def test_boot_continuation_precedes_application_launch(self):
        startup = self.files['etc/init.d/S00lvgl'][0].decode()
        self.assertLess(startup.index('--continue-unified'), startup.index('"$DAEMON" $DAEMONOPTS'))
        self.assertIn('grep -q UN260_UNIFIED_1', startup)
        self.assertIn('Upgrade display unavailable; no storage migration attempted', startup)

    def test_large_nested_payload_is_not_root_staging(self):
        self.bridge()
        plan = (self.root / 'log').read_text()
        self.assertNotIn('path=' + NESTED, plan)
        self.assertNotIn('path=' + APP, plan)


# Base's ordinary package regression cases have their own dedicated suite.
for name in list(vars(Base)):
    if name.startswith('test_') and name not in vars(UnifiedTests):
        setattr(UnifiedTests, name, None)

if __name__ == '__main__':
    unittest.main(verbosity=2)
