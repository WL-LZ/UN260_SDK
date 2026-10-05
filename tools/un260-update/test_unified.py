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

REAL_GUARD_PROGRESS = r'''
GUARDED=1
USB_GUARDED=1
APP_GUARDED=1
SYNC_TOOL=$ROOT_PREFIX/usr/local/bin/un260_storage_sync
exec 7< "$ROOT_PREFIX" 8< "$USB_MNT" 9< "$APP_VOLUME_DIR"
# Host directories replace mount identity/NAND only. Native held-fd checks,
# syncfs, hashes, transactions and the final markers remain real.
usb_barrier() { "$SYNC_TOOL" --fd 8 "$USB_MNT"; }
: > "$ROOT_PREFIX/progress.trace"
mv() {
    command mv "$@" || return
    case "$*" in *"$STATUS_TEMP"*)
        tr '\n' '|' < "$STATUS_FILE" >> "$ROOT_PREFIX/progress.trace"
        printf '\n' >> "$ROOT_PREFIX/progress.trace" ;;
    esac
}
'''


class UnifiedTests(Base):
    def setUp(self):
        super().setUp()
        self.files['usr/local/bin/un260_resource_cleanup'] = (b'#!/bin/sh\n# UN260_STORAGE_SYNC_1\nexit 0\n', 0o755)
        self.files[DISPLAY] = (b'DISPLAY', 0o755)
        # Emulate the migration boundary; real helper is independently covered
        # by test_app_storage fault injection and bounded native MTD reads.
        self.files['usr/local/bin/un260_app_storage'] = (
            ('#!/bin/sh\n# UN260_STORAGE_SYNC_1 UN260_MTD_STREAM_1 UN260_APP_VOLUME_V1 UN260_STORAGE_POLICY_V2 UN260_STORAGE_POLICY_V3\n'
             'if [ "$1" = --migrate ]; then\n'
             ' echo migrate >> "%s/migration-calls"\n'
             ' [ ! -f "%s/fail-migration" ] || exit 1\n'
             ' mkdir -p "%s/etc/un260/app-storage"\n'
             ' echo owned > "%s/etc/un260/app-storage/active"\n'
             'fi\n'
             'if [ "$1" = --verify-layout ]; then\n'
             ' [ ! -f "%s/fail-layout" ] || exit 1\nfi\nexit 0\n' %
             (self.p, self.p, self.root, self.root, self.p)).encode(), 0o755)
        self.pack()
        self.unified = self.p / 'UN260_UPDATE.upk'
        build(self.full, self.unified, APPROVED)
        self.old = self.p / 'old.sh'; self.old.write_text(OLD_UPDATER)

    def boot(self, ok=True, extra=''):
        # setup functions source current updater exactly as next boot would.
        return self.run_update(package=self.unified, extra='APP_VOLUME=0\nprepare_app_storage\nUNIFIED_RESUME=1\n' + extra, ok=ok)

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

    @unittest.skipUnless(os.environ.get('UN260_TEST_R7_UPDATER'), 'requires frozen delivered R7 updater')
    def test_real_r7_bridge_failure_then_explicit_new_package_and_boot(self):
        r7 = Path(os.environ['UN260_TEST_R7_UPDATER'])
        self.assertEqual(hashlib.sha256(r7.read_bytes()).hexdigest(),
                         '3ec13ccaf10d6991f8ca4f0e1e503d0ff2fd49e6b8f9cd00905532b1b13750ed')
        self.run_update(package=self.unified, source=r7, managed=0)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        (self.p / 'fail-migration').touch()
        self.boot(ok=False)
        before = (self.root / REQUEST).read_bytes()
        self.files[APP] = (b'REPAIRED AFTER R7 FAILURE', 0o755)
        self.pack(); build(self.full, self.unified, APPROVED)
        self.run_update(package=self.unified, source=r7, managed=0)
        self.assertNotEqual((self.root / REQUEST).read_bytes(), before)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        (self.p / 'fail-migration').unlink()
        self.boot()
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertIn('success=1', (self.root / 'status').read_text())

    def test_current_updater_bootstraps_same_small_subset(self):
        self.run_update(package=self.unified, managed=0)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertFalse((self.root / 'var/lib/un260-updater/transaction').exists())
        self.assertIn('stage=restart', (self.root / 'status').read_text())
        self.assertIn('success=-1', (self.root / 'status').read_text())
        self.assertIn('result=pending', (self.root / 'result').read_text())
        self.assertFalse((self.root / 'var/lib/un260-updater/installed.fnv64').exists())
        self.boot()

    def test_managed_volume_installs_one_public_package_without_migration(self):
        self.run_update(package=self.unified, extra=NO_OPTIONAL_TOOLS)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertIn('success=1', (self.root / 'status').read_text())
        self.assertEqual((self.root / 'var/lib/un260-updater/installed.fnv64').read_text().strip(),
                         ui_fingerprint(self.unified.read_bytes()))

    def progress_rows(self):
        return [dict(field.split('=', 1) for field in row.rstrip('|').split('|'))
                for row in (self.root / 'progress.trace').read_text().splitlines()]

    def assert_final_progress(self):
        rows = self.progress_rows()
        values = [int(row['progress']) for row in rows]
        self.assertEqual(values, sorted(values), rows)
        self.assertEqual([row['stage'] for row in rows[-2:]], ['finish', 'success'])
        self.assertEqual(values[-2:], [96, 100])
        self.assertTrue(all(value < 96 for value in values[:-2]), rows)
        self.assertEqual(sum(row['stage'] == 'finish' for row in rows), 1)
        self.assertIn('result=success', (self.usb / 'UN260_UPDATE_RESULT.txt').read_text())
        # USB log is copied BEFORE its final barrier; its pending wording is
        # also present after a fully successful operation, not failure evidence.
        self.assertIn('final USB flush pending', (self.usb / 'ui_update.log').read_text())
        self.assertTrue((self.root / 'var/lib/un260-updater/unified.completed').is_file())

    def test_unified_progress_changed_and_nochange_with_real_guard(self):
        self.run_update(package=self.unified, extra=REAL_GUARD_PROGRESS)
        self.assert_final_progress()
        self.assertFalse((self.p / 'migration-calls').exists())
        _, log = self.run_update(package=self.unified, extra=REAL_GUARD_PROGRESS)
        self.assert_final_progress()
        self.assertIn('Install plan: changed=0', log)
        self.assertFalse((self.p / 'migration-calls').exists())

    def test_boot_migration_progress_with_real_guard(self):
        self.bridge()
        self.boot(extra=REAL_GUARD_PROGRESS)
        self.assert_final_progress()
        self.assertEqual((self.p / 'migration-calls').read_text().splitlines(), ['migrate'])

    def test_tiny_bridge_restart_progress_with_real_guard(self):
        self.run_update(package=self.unified, managed=0, extra=REAL_GUARD_PROGRESS)
        rows = self.progress_rows()
        values = [int(row['progress']) for row in rows]
        self.assertEqual(values, sorted(values), rows)
        self.assertEqual([row['stage'] for row in rows[-2:]], ['finish', 'restart'])
        self.assertEqual(values[-2:], [96, 96])
        self.assertTrue(all(value < 96 for value in values[:-2]), rows)
        self.assertEqual(rows[-1]['success'], '-1')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertFalse((self.root / 'var/lib/un260-updater/installed.fnv64').exists())
        self.assertIn('result=pending', (self.usb / 'UN260_UPDATE_RESULT.txt').read_text())

    def test_unified_failure_retains_mapped_progress_with_real_guard(self):
        (self.p / 'fail-layout').touch()
        self.run_update(package=self.unified, extra=REAL_GUARD_PROGRESS, ok=False)
        rows = self.progress_rows()
        self.assertEqual(rows[-1]['stage'], 'fail')
        self.assertEqual(rows[-1]['progress'], rows[-2]['progress'])
        self.assertGreaterEqual(int(rows[-1]['progress']), 65)
        self.assertLess(int(rows[-1]['progress']), 96)
        self.assertNotIn('finish', [row['stage'] for row in rows])
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())

    def interrupted_old_progress_retry(self, committed):
        old = Path(os.environ['UN260_TEST_PRE_PROGRESS_UPDATER'])
        self.assertEqual(hashlib.sha256(old.read_bytes()).hexdigest(),
                         '95097e98778baa4fff3e5aee9668a04d46786485d266b0b6edfe3413594e3447')
        self.run_update(package=self.unified, extra=REAL_GUARD_PROGRESS)
        # Same archive, but an installed file needs repair; the old managed
        # bridge reaches 96 before this application's transaction even starts.
        (self.vol / 'test_lvgl').write_bytes(b'DAMAGED CURRENT APP')
        if committed:
            interrupt = r'''
rm() {
    case "$*" in *test_lvgl.un260-old*)
        [ ! -f "$JOURNAL/committed" ] || exit 88 ;;
    esac
    command rm "$@"
}
'''
        else:
            interrupt = r'''
mv() {
    command mv "$@" || return
    case "$*" in *test_lvgl.un260-new*) exit 88 ;; esac
    case "$*" in *"$STATUS_TEMP"*)
        tr '\n' '|' < "$STATUS_FILE" >> "$ROOT_PREFIX/progress.trace"
        printf '\n' >> "$ROOT_PREFIX/progress.trace" ;;
    esac
}
'''
        result, _ = self.run_update(package=self.unified, source=old,
                                    extra=REAL_GUARD_PROGRESS + interrupt, ok=None)
        self.assertEqual(result.returncode, 88, result.stderr)
        self.assertIn('96', [row['progress'] for row in self.progress_rows()])
        journal = self.root / 'var/lib/un260-updater/transaction'
        self.assertTrue(journal.is_dir())
        self.assertEqual((journal / 'committed').exists(), committed)
        self.run_update(package=self.unified, extra=REAL_GUARD_PROGRESS,
                        action='recover_transaction\ninstall_bundle')
        self.assert_final_progress()
        self.assertFalse(journal.exists())
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertFalse((self.p / 'migration-calls').exists())
        self.assertEqual((self.root / 'var/lib/un260-updater/installed.fnv64').read_text().strip(),
                         ui_fingerprint(self.unified.read_bytes()))

    @unittest.skipUnless(os.environ.get('UN260_TEST_PRE_PROGRESS_UPDATER'), 'requires frozen pre-fix updater')
    def test_old_96_uncommitted_interruption_recovers_then_retries(self):
        self.interrupted_old_progress_retry(False)

    @unittest.skipUnless(os.environ.get('UN260_TEST_PRE_PROGRESS_UPDATER'), 'requires frozen pre-fix updater')
    def test_old_96_committed_interruption_recovers_then_retries(self):
        self.interrupted_old_progress_retry(True)

    @unittest.skipUnless(os.environ.get('UN260_TEST_PRE_PROGRESS_UPDATER'), 'requires frozen pre-fix updater')
    def test_old_running_updater_installs_fix_then_new_progress_applies(self):
        old = Path(os.environ['UN260_TEST_PRE_PROGRESS_UPDATER'])
        self.assertEqual(hashlib.sha256(old.read_bytes()).hexdigest(),
                         '95097e98778baa4fff3e5aee9668a04d46786485d266b0b6edfe3413594e3447')
        self.run_update(package=self.unified, source=old, extra=REAL_GUARD_PROGRESS)
        rows = self.progress_rows()
        values = [int(row['progress']) for row in rows]
        self.assertEqual(values[-1], 100)
        first_96 = values.index(96)
        self.assertTrue(any(value < 96 for value in values[first_96+1:]))
        self.assertEqual(rows[-1]['success'], '1')
        self.assertEqual((self.root / 'usr/bin/ui_update.sh').read_bytes(),
                         self.files['usr/bin/ui_update.sh'][0])
        self.assertEqual((self.root / 'var/lib/un260-updater/installed.fnv64').read_text().strip(),
                         ui_fingerprint(self.unified.read_bytes()))
        request_hash = hashlib.sha256((self.root / REQUEST).read_bytes()).hexdigest()
        self.assertEqual((self.root / 'var/lib/un260-updater/unified.completed').read_text().strip(), request_hash)
        self.run_update(package=self.unified, source=self.root / 'usr/bin/ui_update.sh',
                        extra=REAL_GUARD_PROGRESS)
        self.assert_final_progress()
        self.assertFalse((self.p / 'migration-calls').exists())

    def test_explicit_new_package_replaces_failed_request_after_recovery(self):
        self.bridge()
        previous = (self.root / REQUEST).read_bytes()
        self.files[APP] = (b'ANOTHER APP', 0o755)
        self.pack()
        build(self.full, self.unified, APPROVED)
        self.boot(ok=False)
        self.assertEqual((self.root / REQUEST).read_bytes(), previous)
        self.run_update(package=self.unified)
        self.assertNotEqual((self.root / REQUEST).read_bytes(), previous)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'ANOTHER APP')
        self.assertFalse((self.p / 'migration-calls').exists())

    def test_final_layout_failure_rolls_back_application_and_never_completes(self):
        (self.p / 'fail-layout').touch()
        self.run_update(package=self.unified, ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.assertNotIn('success=1', (self.root / 'status').read_text())
        (self.p / 'fail-layout').unlink()
        self.run_update(package=self.unified)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_same_package_repairs_changed_content_and_mode(self):
        self.run_update(package=self.unified)
        (self.vol / 'test_lvgl').write_bytes(b'BROKEN APP')
        (self.root / 'usr/local/lib/liblvgl.so').chmod(0o644)
        self.run_update(package=self.unified)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertEqual((self.root / 'usr/local/lib/liblvgl.so').stat().st_mode & 0o777, 0o755)

    def owned_resource_package(self):
        self.files['usr/local/share/lvgl_data/current.png'] = (b'CURRENT RESOURCE', 0o644)
        self.pack()
        with tarfile.open(self.full) as source:
            entries = [(item, source.extractfile(item).read() if item.isfile() else None)
                       for item in source.getmembers()]
        with tarfile.open(self.full, 'w:gz', format=tarfile.USTAR_FORMAT) as target:
            directory = tarfile.TarInfo('payload/usr/local/share/lvgl_data')
            directory.type = tarfile.DIRTYPE; directory.mode = 0o755
            target.addfile(directory)
            for item, data in entries:
                if item.name == 'install.tsv':
                    data = b''.join(line + b'\n' for line in data.splitlines()
                                    if b'|usr/local/share/lvgl_data/' not in line)
                    data += b'tree|0755|usr/local/share/lvgl_data\n'
                    item.size = len(data)
                target.addfile(item, io.BytesIO(data) if data is not None else None)
        build(self.full, self.unified, APPROVED)

    def test_owned_static_tree_converges_and_failed_final_check_restores_retired_files(self):
        self.owned_resource_package()
        stale = self.root / 'usr/local/share/lvgl_data/retired/old.png'
        stale.parent.mkdir(parents=True); stale.write_bytes(b'OLD STATIC RESOURCE')
        user_image = self.root / 'etc/ui_state/standby_photo.bin'
        user_image.write_bytes(b'USER PHOTO')
        calibration = self.root / 'etc/pointercal'
        calibration.write_bytes(b'CALIBRATION')
        (self.p / 'fail-layout').touch()
        self.run_update(package=self.unified, ok=False)
        self.assertEqual(stale.read_bytes(), b'OLD STATIC RESOURCE')
        self.assertFalse((stale.parents[1] / 'current.png').exists())
        self.assertEqual(user_image.read_bytes(), b'USER PHOTO')
        self.assertEqual(calibration.read_bytes(), b'CALIBRATION')
        (self.p / 'fail-layout').unlink()
        self.run_update(package=self.unified)
        self.assertFalse(stale.exists())
        self.assertEqual((stale.parents[1] / 'current.png').read_bytes(), b'CURRENT RESOURCE')
        stale.write_bytes(b'REINTRODUCED STATIC RESOURCE')
        self.run_update(package=self.unified)
        self.assertFalse(stale.exists())
        self.assertEqual(user_image.read_bytes(), b'USER PHOTO')
        self.assertEqual(calibration.read_bytes(), b'CALIBRATION')

    def test_owned_static_tree_refuses_symlink_before_application_changes(self):
        self.owned_resource_package()
        resource = self.root / 'usr/local/share/lvgl_data/custom.png'
        resource.parent.mkdir(parents=True)
        resource.symlink_to(self.business)
        self.run_update(package=self.unified, ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertIn('Unsupported installed static resource entry', (self.root / 'status').read_text())

    def test_final_verification_detects_post_copy_corruption_and_rolls_back(self):
        self.run_update(package=self.unified, ok=False, extra='''
mv() {
    command mv "$@" || return $?
    case "$*" in *test_lvgl.un260-new*) printf CORRUPT > "$APP_VOLUME_DIR/test_lvgl" ;; esac
}''')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertIn('Installed target checksum differs', (self.root / 'status').read_text())
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())

    def test_failed_transaction_recovered_before_explicit_new_package(self):
        self.bridge()
        old_request = (self.root / REQUEST).read_bytes()
        journal = self.root / 'var/lib/un260-updater/transaction'
        journal.mkdir(parents=True)
        (journal / 'state').write_text('1|usr/local/lib/liblvgl.so\n')
        library = self.root / 'usr/local/lib/liblvgl.so'
        old_link = library.with_name(library.name + '.un260-old')
        os.link(library, old_link)
        library.unlink(); library.write_bytes(b'INTERRUPTED REPLACEMENT')
        self.files[APP] = (b'RETRIED NEW APP', 0o755)
        self.pack(); build(self.full, self.unified, APPROVED)
        self.run_update(package=self.unified, action='recover_transaction; install_bundle')
        self.assertNotEqual((self.root / REQUEST).read_bytes(), old_request)
        self.assertEqual(library.read_bytes(), self.files['usr/local/lib/liblvgl.so'][0])
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'RETRIED NEW APP')
        self.assertFalse(old_link.exists()); self.assertFalse(journal.exists())

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
