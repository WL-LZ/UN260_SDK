#!/usr/bin/env python3
"""Host-only initialization/recovery fault injection; NO real MTD commands.

NAND and UBI are fixture functions. Does not establish physical power-cut safety.
"""
import hashlib
import os
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
        self.approved = hashlib.sha256(self.nand.read_bytes()).hexdigest()
        self.erased = hashlib.sha256(b'\xff' * 4096).hexdigest()
        for key, value in {'mtd/mtd11/name': 'ubisystem', 'mtd/mtd11/size': str(self.nand.stat().st_size),
                           'mtd/mtd11/erasesize': '131072', 'mtd/mtd11/writesize': '2048',
                           'mtd/mtd10/name': 'ubiroot', 'mtd/mtd10/size': '67108864',
                           'ubi/ubi0/mtd_num': '10'}.items():
            f = self.p / 'sys/class' / key; f.parent.mkdir(parents=True, exist_ok=True); f.write_text(value+'\n')
        (self.p / 'proc').mkdir()
        uuid = self.p / 'proc/sys/kernel/random/uuid'
        uuid.parent.mkdir(parents=True)
        uuid.symlink_to('/proc/sys/kernel/random/uuid')
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

    def run_helper(self, action='migrate "$APPROVED"', cut=0, extra='', ok=True):
        runner = self.p / 'runner.sh'
        runner.write_text('''set -eu
UN260_STORAGE_LIBRARY_ONLY=1
. '%s'
SYS='%s/sys'
PROC='%s/proc'
DEV='%s/dev'
USB='%s/usb'
VOLUME='%s/volume'
STATE='%s/root/etc/un260/app-storage'
APP='%s/root/usr/local/bin/test_lvgl'
LOCK='%s/lock'
UPDATER_STATE='%s/root/var/lib/un260-updater'
APPROVED='%s'
ERASED_HASH='%s'
RAW_BYTES=4096
SYNC_TOOL='%s'
mkdir -p "$PROC/$$"
printf 'PPid: 123\n' > "$PROC/$$/status"
require_usb() { :; }
sync_count=0
sync() {
    sync_count=$((sync_count+1))
    [ "$sync_count" != '%s' ] || exit 88
}
df() { printf 'FS Size Used Free Use Mount\nfixture 100000 1 99000 1%%%% /\n'; }
ubiformat() {
    [ "$1" = "$DEV/mtd11" ] && [ "$2" = -y ] || exit 90
    printf 'format\n' >> "$DEV/format-calls"
    : > "$DEV/formatted"
}
ubiattach() {
    [ -f "$DEV/formatted" ] || return 1
    mkdir -p "$SYS/class/ubi/ubi1"
    printf '11\n' > "$SYS/class/ubi/ubi1/mtd_num"
}
ubimkvol() {
    [ "$1" = "$DEV/ubi1" ] && [ "$3" = un260_app ] || exit 91
    mkdir -p "$SYS/class/ubi/ubi1_0"
    printf 'un260_app\n' > "$SYS/class/ubi/ubi1_0/name"
}
mount() {
    [ "$1" = -t ] && [ "$2" = ubifs ] && [ "$3" = ubi1:un260_app ] && [ "$4" = "$VOLUME" ] || exit 92
    printf 'ubi1:un260_app %%s ubifs rw 0 0\n' "$VOLUME" >> "$PROC/mounts"
}
%s
%s
''' % (HELPER, self.p, self.p, self.p, self.p, self.p, self.p, self.p, self.p,
       self.p, self.approved, self.erased, native_tool(), cut, extra, action))
        r = subprocess.run(SHELL + [str(runner)], capture_output=True, text=True)
        if ok is not None:
            self.assertEqual(r.returncode == 0, ok, r.stdout + r.stderr)
        return r

    def assert_active(self):
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), ORIGINAL_APP)
        self.assertIn('--launch', self.app.read_text())
        self.assertEqual((self.state / 'active').read_text(), 'UN260_APP_VOLUME_V1\n')
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse(Path(str(self.app)+'.un260-volume-old').exists())
        self.assertEqual((self.dev / 'format-calls').read_text().splitlines(), ['format'])

    def test_migrate_then_prepare_and_idempotent(self):
        self.run_helper(); self.assert_active()
        self.run_helper(action='prepare'); self.run_helper(); self.assert_active()
        backup = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved)
        self.assertEqual((backup / 'mtd11.data.bin').read_bytes(), self.nand.read_bytes())
        name = 'test_lvgl.' + hashlib.sha256(ORIGINAL_APP).hexdigest() + '.before'
        self.assertEqual((backup / name).read_bytes(), ORIGINAL_APP)

    def test_fleet_usb_preserves_another_application_backup(self):
        backup = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved)
        backup.mkdir()
        legacy = backup / 'test_lvgl.before'
        legacy.write_bytes(b'ANOTHER MACHINE APP')
        self.run_helper()
        self.assert_active()
        self.assertEqual(legacy.read_bytes(), b'ANOTHER MACHINE APP')
        name = 'test_lvgl.' + hashlib.sha256(ORIGINAL_APP).hexdigest() + '.before'
        self.assertEqual((backup / name).read_bytes(), ORIGINAL_APP)

    def test_unknown_fingerprint_never_formats(self):
        self.nand.write_bytes(b'NEW DATA' * 512)
        self.run_helper(ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)

    def test_device_checksum_mmap_failure_avoided(self):
        # Reproduce the deployed sha256sum's device-path failure. Migration
        # must never ask it to open MTD by filename; the native reader streams.
        self.run_helper(extra='''sha256sum() {
    if [ "${1:-}" = "$DEV/mtd11" ]; then
        echo 'Use of mmap failed mapping 16777216 bytes /dev/mtd11 (-22)' >&2
        return 1
    fi
    command sha256sum "$@"
}''')
        self.assert_active()

    def test_existing_valid_backup_does_not_authorize_changed_nand(self):
        b = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved); b.mkdir()
        (b / 'mtd11.data.bin').write_bytes(self.nand.read_bytes())
        self.nand.write_bytes(b'NEW DATA' * 512)
        result = self.run_helper(ok=False)
        self.assertIn('NAND fingerprint differs', result.stderr)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertFalse((self.state / 'pending').exists())

    def test_native_read_failure_is_not_reported_as_changed_nand(self):
        fake = self.p / 'failed-reader'
        fake.write_text('#!/bin/sh\n[ "$1" = --probe-stream ] && { echo UN260_STORAGE_STREAM_1; exit 0; }\nexit 1\n')
        fake.chmod(0o755)
        result = self.run_helper(extra="SYNC_TOOL='%s'" % fake, ok=False)
        self.assertIn('NAND read/hash failed', result.stderr)
        self.assertNotIn('fingerprint differs', result.stderr)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertFalse((self.state / 'pending').exists())

    def test_checksum_failed_even_with_digest_output_refused(self):
        result = self.run_helper(extra='''sha256sum() {
    command sha256sum "$@"
    return 1
}''', ok=False)
        self.assertIn('Cannot hash original executable', result.stderr)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_usb_direct_read_failure_never_formats(self):
        self.run_helper(extra='backup_digest() { return 1; }', ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)

    def test_usb_flush_failure_never_formats(self):
        self.run_helper(extra="sync() { die 'injected USB writeback error'; }", ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)

    def test_geometry_mismatch_never_formats(self):
        (self.p / 'sys/class/mtd/mtd11/name').write_text('business-data\n')
        self.run_helper(ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_already_attached_unknown_partition_refused(self):
        d = self.p / 'sys/class/ubi/ubi1'; d.mkdir()
        (d / 'mtd_num').write_text('11\n')
        self.run_helper(ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_missing_usb_never_formats(self):
        self.run_helper(extra="require_usb() { die 'not mounted'; }", ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_unknown_existing_backup_preserved(self):
        b = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved); b.mkdir()
        (b / 'mtd11.data.bin').write_bytes(b'DO NOT DELETE')
        self.run_helper(ok=False)
        self.assertEqual((b / 'mtd11.data.bin').read_bytes(), b'DO NOT DELETE')
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_active_volume_mount_failure_never_formats(self):
        self.run_helper()
        self.run_helper(action='prepare', extra="mount_volume() { die 'mount failed'; }", ok=False)
        self.assert_active()

    def test_symlink_metadata_refused(self):
        self.state.mkdir(parents=True)
        victim = self.p / 'victim'; victim.write_text('KEEP')
        (self.state / 'pending.tmp').symlink_to(victim)
        self.run_helper(ok=False)
        self.assertEqual(victim.read_text(), 'KEEP')
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_boot_before_migration_does_not_attach_or_format(self):
        self.run_helper(action='prepare')
        self.assertFalse((self.p / 'sys/class/ubi/ubi1').exists())

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

    def test_overwritten_launcher_fails_closed(self):
        self.run_helper()
        self.app.write_bytes(b'OLD PACKAGE APP')
        self.run_helper(action='prepare', ok=False)
        self.assertEqual((self.dev / 'format-calls').read_text(), 'format\n')

    def test_corrupt_resume_backup_refused(self):
        self.run_helper(cut=4, ok=None)
        b = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved) / 'mtd11.data.bin'
        b.write_bytes(b'CORRUPT')
        self.run_helper(ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_partial_app_copy_can_retry(self):
        self.run_helper(extra='''cp() {
    case "$2" in *.migration-new) printf 'partial' > "$2"; return 1 ;; esac
    command cp "$@"
}''', ok=False)
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)
        self.run_helper(action='prepare')
        self.run_helper(); self.assert_active()

    def test_unknown_volume_is_not_claimed(self):
        self.run_helper(extra='''ubimkvol() {
    mkdir -p "$SYS/class/ubi/ubi1_0"
    printf 'user_data\n' > "$SYS/class/ubi/ubi1_0/name"
}''', ok=False)
        self.run_helper(ok=False)
        self.assertEqual((self.dev / 'format-calls').read_text(), 'format\n')
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)

    def test_journal_boundaries_resume(self):
        # Every ordered durability barrier (incl. after activation and unlink).
        for cut in range(1, 24):
            with self.subTest(cut=cut):
                if cut > 1:
                    self.tearDown(); self.setUp()
                result = self.run_helper(cut=cut, ok=None)
                self.assertIn(result.returncode, (0, 88), result.stdout + result.stderr)
                # Boot must never add a format call; early phases use root UI.
                calls = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                self.run_helper(action='prepare')
                after = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                self.assertEqual(calls, after)
                self.run_helper(); self.assert_active()

    def test_erased_nand_requires_explicit_v2_policy(self):
        self.nand.write_bytes(b'\xff' * 4096)
        result = self.run_helper(ok=False)
        self.assertIn('diagnostic exported and verified', result.stderr)
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse((self.dev / 'format-calls').exists())
        self.run_helper(action='migrate "$POLICY_V2"')
        self.assert_active()
        backups = list(self.usb.glob('UN260_APP_STORAGE_BACKUP_' + self.erased + '_*'))
        self.assertEqual(len(backups), 1)
        self.assertEqual((backups[0] / 'mtd11.data.bin').read_bytes(), b'\xff' * 4096)

    def test_v2_retains_reviewed_support_and_uses_unique_backup(self):
        old = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved)
        old.mkdir(); (old / 'mtd11.data.bin').write_bytes(b'PRESERVE OTHER BACKUP')
        self.run_helper(action='migrate "$POLICY_V2"', cut=1, ok=None)
        self.assertFalse((self.state / 'pending').exists())
        self.run_helper(action='migrate "$POLICY_V2"')
        self.assert_active()
        self.assertEqual(len(list(self.usb.glob('UN260_APP_STORAGE_BACKUP_' + self.approved + '_*'))), 2)
        self.assertEqual((old / 'mtd11.data.bin').read_bytes(), b'PRESERVE OTHER BACKUP')

    def pause_v2(self):
        result = self.run_helper(action='migrate "$POLICY_V2"', ok=None, extra='''
sync() { [ ! -e "$STATE/pending" ] || exit 88; }
''')
        self.assertEqual(result.returncode, 88, result.stderr)
        lines = (self.state / 'pending').read_text().splitlines()
        self.assertEqual(len(lines), 5)
        self.assertEqual(lines[0], 'un260-reviewed-or-erased-v2')
        self.assertEqual(lines[1], hashlib.sha256(self.nand.read_bytes()).hexdigest())
        self.assertEqual(lines[2], 'formatting')
        self.assertEqual(lines[3], hashlib.sha256(self.app.read_bytes()).hexdigest())
        self.assertRegex(lines[4], r'^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$')
        self.assertFalse((self.dev / 'format-calls').exists())
        return self.usb / ('UN260_APP_STORAGE_BACKUP_' + lines[1] + '_' + lines[4])

    def test_v2_erased_resume_binds_original_source_and_transaction(self):
        self.nand.write_bytes(b'\xff' * 4096)
        backup = self.pause_v2()
        # Resume is not allowed to relabel partially initialized NAND as a new
        # source. It must retain the pre-format source and original USB backup.
        self.nand.write_bytes(b'PARTIAL!' * 512)
        self.run_helper(action='prepare')
        self.run_helper(action='migrate "$POLICY_V2"')
        self.assert_active()
        self.assertEqual((backup / 'mtd11.data.bin').read_bytes(), b'\xff' * 4096)
        self.assertEqual(len(list(self.usb.glob('UN260_APP_STORAGE_BACKUP_*'))), 1)

    def test_legacy_request_cannot_resume_blank_v2(self):
        self.nand.write_bytes(b'\xff' * 4096)
        self.pause_v2()
        result = self.run_helper(ok=False)
        self.assertIn('does not authorize erased NAND', result.stderr)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_v2_resumes_existing_three_line_reviewed_journal(self):
        self.run_helper(ok=None, extra='sync() { [ ! -e "$STATE/pending" ] || exit 88; }')
        self.assertEqual(len((self.state / 'pending').read_text().splitlines()), 3)
        self.run_helper(action='migrate "$POLICY_V2"')
        self.assert_active()
        self.assertEqual(len(list(self.usb.glob('UN260_APP_STORAGE_BACKUP_*'))), 1)
        self.assertTrue((self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved)).is_dir())

    def test_old_helper_rejects_v2_journal(self):
        self.pause_v2()
        # Pin the actual R7 blob: using HEAD would stop testing an old helper
        # as soon as this compatibility change is committed.
        old = subprocess.check_output(['git', '-C', str(SDK), 'cat-file', 'blob',
                                      'ea76b36b08791021e6454ceed79e1c3f6330a4eb'])
        self.assertEqual(hashlib.sha256(old).hexdigest(),
                         '48c3b477e94d758404fa753cd2823d337c148921fd82038a210bf5fefdfa24bc')
        old_path = self.p / 'old-helper'; old_path.write_bytes(old)
        result = subprocess.run(SHELL + ['-c',
            'UN260_STORAGE_LIBRARY_ONLY=1; . "$1"; STATE="$2"; APPROVED="$3"; load_pending',
            'test', str(old_path), str(self.state), self.approved], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unapproved migration journal', result.stderr)

    def test_v2_wrong_backup_or_other_transaction_never_formats(self):
        self.nand.write_bytes(b'\xff' * 4096)
        backup = self.pause_v2()
        data = backup / 'mtd11.data.bin'
        data.write_bytes(b'old NAND' * 512)
        self.run_helper(action='migrate "$POLICY_V2"', ok=False)
        data.write_bytes(b'\xff' * 4096)
        backup.rename(backup.with_name(backup.name + '-other'))
        self.run_helper(action='migrate "$POLICY_V2"', ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_v2_malformed_journal_never_prepares_or_formats(self):
        self.pause_v2()
        pending = self.state / 'pending'
        original = pending.read_text()
        variants = [original + 'extra', original.replace('formatting', 'unreviewed'),
                    original.replace(self.approved, '0' * 64),
                    '\n'.join(original.splitlines()[:4]) + '\n../escape\n']
        for data in variants:
            pending.write_text(data)
            self.run_helper(action='prepare', ok=False)
            self.assertFalse((self.dev / 'format-calls').exists())

    def test_unknown_exports_verified_dump_without_overwriting(self):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        for attempt in range(2):
            result = self.run_helper(action='migrate "$POLICY_V2"', ok=False)
            self.assertIn('diagnostic exported and verified', result.stderr)
            self.assertFalse((self.state / 'pending').exists())
            self.assertFalse((self.dev / 'format-calls').exists())
        reports = list(self.usb.glob('UN260_STORAGE_DIAGNOSTIC_*'))
        self.assertEqual(len(reports), 2)
        for report in reports:
            self.assertEqual((report / 'mtd11.data.bin').read_bytes(), self.nand.read_bytes())
            self.assertIn('mtd11=ubisystem', (report / 'layout.txt').read_text())
            for line in (report / 'SHA256SUMS').read_text().splitlines():
                expected, name = line.split()
                self.assertEqual(hashlib.sha256((report / name).read_bytes()).hexdigest(), expected)

    def test_unknown_export_read_flush_and_readback_fail_closed(self):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        for failure in ["dd() { return 1; }", "sync() { die 'USB write failure'; }",
                        'backup_digest() { return 1; }']:
            result = self.run_helper(action='migrate "$POLICY_V2"', extra=failure, ok=False)
            self.assertNotIn('diagnostic exported and verified', result.stderr)
            self.assertFalse((self.state / 'pending').exists())
            self.assertFalse((self.dev / 'format-calls').exists())

    def test_one_non_erased_byte_is_not_blank(self):
        self.nand.write_bytes(b'\xff' * 4095 + b'\x00')
        result = self.run_helper(action='migrate "$POLICY_V2"', ok=False)
        self.assertIn('diagnostic exported and verified', result.stderr)
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_source_change_after_usb_readback_never_publishes_intent(self):
        result = self.run_helper(action='migrate "$POLICY_V2"', extra='''
backup_digest() {
    digest "$1"
    case "$1" in *.before) printf 'CHANGED!' > "$DEV/mtd11" ;; esac
}
''', ok=False)
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_v2_journal_boundaries_resume_reviewed_and_erased(self):
        for erased in (False, True):
            for cut in range(1, 24):
                with self.subTest(erased=erased, cut=cut):
                    self.tearDown(); self.setUp()
                    if erased:
                        self.nand.write_bytes(b'\xff' * 4096)
                    result = self.run_helper(action='migrate "$POLICY_V2"', cut=cut, ok=None)
                    self.assertIn(result.returncode, (0, 88), result.stderr)
                    calls = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                    self.run_helper(action='prepare')
                    after = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                    self.assertEqual(calls, after)
                    self.run_helper(action='migrate "$POLICY_V2"'); self.assert_active()

    def query_public(self, action):
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
                         ('SYNC_TOOL=/usr/local/bin/un260_storage_sync', 'SYNC_TOOL=/nonexistent-for-readonly-query')]:
            script = script.replace(old, new)
        path = self.p / 'query-helper'; path.write_text(script)
        return subprocess.run(SHELL + [str(path), action], capture_output=True, text=True)

    def query_legacy(self):
        return self.query_public('--can-launch-legacy')

    def pause_v3(self, phase='formatting'):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        self.assertEqual(self.nand.stat().st_size, 4096)
        result = self.run_helper(action='migrate "$POLICY_V3"', ok=None, extra='''
sync() {
    if [ -e "$STATE/pending" ] && [ "$(sed -n '3p' "$STATE/pending")" = '%s' ]; then exit 88; fi
}
''' % phase)
        self.assertEqual(result.returncode, 88, result.stderr)
        lines = (self.state / 'pending').read_text().splitlines()
        self.assertEqual(len(lines), 5)
        self.assertEqual(lines[0], 'un260-backed-up-legacy-v3')
        self.assertEqual(lines[2], phase)
        return self.usb / ('UN260_APP_STORAGE_BACKUP_' + lines[1] + '_' + lines[4])

    def test_v3_legacy_content_migrates_with_bound_complete_backup(self):
        original = b'UNKNOWN!' * 512
        self.nand.write_bytes(original)
        self.run_helper(action='migrate "$POLICY_V3"'); self.assert_active()
        backups = list(self.usb.glob('UN260_APP_STORAGE_BACKUP_*'))
        self.assertEqual(len(backups), 1)
        backup = backups[0]
        self.assertEqual((backup / 'mtd11.data.bin').read_bytes(), original)
        layout = (backup / 'layout.txt').read_text()
        self.assertIn('policy=un260-backed-up-legacy-v3', layout)
        self.assertIn('source_sha256=' + hashlib.sha256(original).hexdigest(), layout)
        self.assertIn('migration_id=' + backup.name[-36:], layout)
        self.assertIn('business_data=/etc/ui_state; retained in ubiroot', layout)
        for line in (backup / 'SHA256SUMS').read_text().splitlines():
            expected, name = line.split()
            self.assertEqual(hashlib.sha256((backup / name).read_bytes()).hexdigest(), expected)

    def test_v3_wrong_identity_or_corrupt_backup_never_formats(self):
        backup = self.pause_v3()
        originals = {name: (backup / name).read_bytes() for name in ('mtd11.data.bin', 'layout.txt', 'SHA256SUMS')}
        for name in originals:
            with self.subTest(name=name):
                (backup / name).write_bytes(b'WRONG BACKUP')
                self.run_helper(action='migrate "$POLICY_V3"', ok=False)
                self.assertFalse((self.dev / 'format-calls').exists())
                (backup / name).write_bytes(originals[name])
        layout = (backup / 'layout.txt').read_text()
        altered = layout.replace(backup.name[-36:], '00000000-0000-0000-0000-000000000000')
        (backup / 'layout.txt').write_text(altered)
        checks = (backup / 'SHA256SUMS').read_text()
        lines = checks.splitlines()
        lines[-1] = hashlib.sha256(altered.encode()).hexdigest() + '  layout.txt'
        (backup / 'SHA256SUMS').write_text('\n'.join(lines) + '\n')
        result = self.run_helper(action='migrate "$POLICY_V3"', ok=False)
        self.assertIn('identity mismatch', result.stderr)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_v3_old_request_rejects_pending_before_activation(self):
        for phase in ('formatting', 'app-ready'):
            for request in ('migrate "$APPROVED"', 'migrate "$POLICY_V2"'):
                with self.subTest(phase=phase, request=request):
                    self.tearDown(); self.setUp()
                    self.pause_v3(phase)
                    before = self.app.read_bytes()
                    calls = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                    result = self.run_helper(action=request, ok=False)
                    self.assertIn('does not authorize v3 journal', result.stderr)
                    self.assertEqual(self.app.read_bytes(), before)
                    self.assertEqual((self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b'', calls)

    def test_v3_boot_app_ready_recovers_without_usb_or_format(self):
        self.pause_v3('app-ready')
        self.usb.rename(self.p / 'removed-usb')
        self.run_helper(action='prepare', extra="require_usb() { die 'unexpected USB requirement'; }")
        self.assert_active()
        self.run_helper(action='prepare', extra="require_usb() { die 'unexpected USB requirement'; }")
        self.assert_active()

    def test_v3_resumes_older_v1_and_v2_journals(self):
        for request in ('migrate "$APPROVED"', 'migrate "$POLICY_V2"'):
            with self.subTest(request=request):
                self.tearDown(); self.setUp()
                self.run_helper(action=request, ok=None, extra='sync() { [ ! -e "$STATE/pending" ] || exit 88; }')
                self.run_helper(action='migrate "$POLICY_V3"'); self.assert_active()

    def test_v3_backup_read_write_metadata_failures_never_format(self):
        failures = ["dd() { return 1; }", "sync() { die 'USB writeback failed'; }",
                    'backup_digest() { return 1; }',
                    '''backup_digest() { case "$1" in */layout.txt) return 1 ;; *) digest "$1" ;; esac; }''',
                    '''backup_digest() { case "$1" in */SHA256SUMS) return 1 ;; *) digest "$1" ;; esac; }''']
        for failure in failures:
            with self.subTest(failure=failure):
                self.tearDown(); self.setUp()
                self.nand.write_bytes(b'UNKNOWN!' * 512)
                self.run_helper(action='migrate "$POLICY_V3"', extra=failure, ok=False)
                self.assertFalse((self.state / 'pending').exists())
                self.assertFalse((self.dev / 'format-calls').exists())

    def test_v3_changed_source_after_direct_readback_never_formats(self):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        self.run_helper(action='migrate "$POLICY_V3"', extra='''
backup_digest() {
    digest "$1"
    case "$1" in */SHA256SUMS) printf 'CHANGED!' > "$DEV/mtd11" ;; esac
}
''', ok=False)
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_v3_mounted_legacy_or_mountpoint_is_rejected(self):
        alias = self.dev / 'legacy-alias'
        alias.symlink_to(self.dev / 'mtd11')
        for source, mountpoint in [(str(self.dev / 'mtdblock11'), '/data'), ('mtd:ubisystem', '/data'),
                                   ('fixture', str(self.vol)), (str(alias), '/data')]:
            with self.subTest(source=source):
                mounts = self.p / 'proc/mounts'
                mounts.write_text('ubi0:rootfs / ubifs rw 0 0\n%s %s jffs2 rw 0 0\n' % (source, mountpoint))
                self.run_helper(action='migrate "$POLICY_V3"', ok=False)
                self.assertFalse((self.dev / 'format-calls').exists())

    def test_v3_resume_mounted_raw_partition_is_rejected(self):
        self.pause_v3()
        with (self.p / 'proc/mounts').open('a') as mount:
            mount.write('%s /data jffs2 rw 0 0\n' % (self.dev / 'mtdblock11'))
        self.run_helper(action='migrate "$POLICY_V3"', ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_old_helper_rejects_v3_journal(self):
        self.pause_v3()
        old = subprocess.check_output(['git', '-C', str(SDK), 'cat-file', 'blob',
                                      'ea76b36b08791021e6454ceed79e1c3f6330a4eb'])
        old_path = self.p / 'old-helper'; old_path.write_bytes(old)
        result = subprocess.run(SHELL + ['-c',
            'UN260_STORAGE_LIBRARY_ONLY=1; . "$1"; STATE="$2"; APPROVED="$3"; load_pending',
            'test', str(old_path), str(self.state), self.approved], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)

    def test_v3_journal_boundaries_resume(self):
        for cut in range(1, 26):
            with self.subTest(cut=cut):
                self.tearDown(); self.setUp()
                self.nand.write_bytes(b'UNKNOWN!' * 512)
                result = self.run_helper(action='migrate "$POLICY_V3"', cut=cut, ok=None)
                self.assertIn(result.returncode, (0, 88), result.stderr)
                calls = (self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b''
                self.run_helper(action='prepare')
                self.assertEqual((self.dev / 'format-calls').read_bytes() if (self.dev / 'format-calls').exists() else b'', calls)
                self.run_helper(action='migrate "$POLICY_V3"'); self.assert_active()

    def pause_initial_orphan(self, request='migrate "$POLICY_V3"'):
        result = self.run_helper(action=request, ok=None, extra='''
sync() {
    if [ -e "$STATE/pending.tmp" ] && [ ! -e "$STATE/pending" ]; then exit 88; fi
}
''')
        self.assertEqual(result.returncode, 88, result.stderr)
        self.assertTrue((self.state / 'pending.tmp').is_file())
        self.assertFalse((self.state / 'pending').exists())
        self.assertFalse((self.state / 'active').exists())
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_initial_orphan_cut_returns_to_current_ui_without_usb(self):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        original = self.nand.read_bytes()
        self.pause_initial_orphan()
        self.usb.rename(self.p / 'removed-usb')
        result = self.run_helper(action='prepare', extra="require_usb() { die 'USB must not be needed'; }")
        self.assertIn('unpublished initial migration retired', result.stdout)
        self.assertFalse((self.state / 'pending.tmp').exists())
        self.assertEqual(self.nand.read_bytes(), original)
        self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)
        self.assertFalse((self.dev / 'format-calls').exists())
        # S00's failed-continuation fallback may now launch the intact root UI.
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.assertEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertTrue(list(self.p.joinpath('removed-usb').glob('UN260_APP_STORAGE_BACKUP_*')))

    def test_initial_orphan_preserves_v1_v2_v3_compatibility(self):
        for request, source in [('migrate "$APPROVED"', b'old NAND' * 512),
                                ('migrate "$POLICY_V2"', b'old NAND' * 512),
                                ('migrate "$POLICY_V2"', b'\xff' * 4096),
                                ('migrate "$POLICY_V3"', b'UNKNOWN!' * 512)]:
            with self.subTest(request=request, source=source[:8]):
                self.tearDown(); self.setUp()
                self.nand.write_bytes(source)
                self.pause_initial_orphan(request)
                self.usb.rename(self.p / 'removed-usb')
                self.run_helper(action='prepare', extra="require_usb() { die 'USB must not be needed'; }")
                self.assertFalse((self.state / 'pending.tmp').exists())
                self.assertEqual(self.nand.read_bytes(), source)
                self.assertEqual(self.app.read_bytes(), ORIGINAL_APP)
                self.assertFalse((self.dev / 'format-calls').exists())

    def test_initial_orphan_changed_source_or_state_refuses_recovery(self):
        for change in ('source', 'app', 'phase', 'identity', 'active', 'active.tmp', 'transaction', 'artifact', 'attached', 'mounted'):
            with self.subTest(change=change):
                self.tearDown(); self.setUp()
                self.nand.write_bytes(b'UNKNOWN!' * 512)
                self.pause_initial_orphan()
                temporary = self.state / 'pending.tmp'
                if change == 'source': self.nand.write_bytes(b'CHANGED!' * 512)
                elif change == 'app': self.app.write_bytes(ORIGINAL_APP + b'CHANGED')
                elif change == 'phase': temporary.write_text(temporary.read_text().replace('formatting', 'volume-ready'))
                elif change == 'identity': temporary.write_text(temporary.read_text().replace('un260-backed-up-legacy-v3', 'unapproved'))
                elif change in ('active', 'active.tmp'): (self.state / change).write_text('UN260_APP_VOLUME_V1\n')
                elif change == 'transaction': self.p.joinpath('root/var/lib/un260-updater/transaction').mkdir(parents=True)
                elif change == 'artifact': Path(str(self.app) + '.un260-old').write_bytes(ORIGINAL_APP)
                elif change == 'attached':
                    attached = self.p / 'sys/class/ubi/ubi1'; attached.mkdir(); attached.joinpath('mtd_num').write_text('11\n')
                elif change == 'mounted':
                    with (self.p / 'proc/mounts').open('a') as mount: mount.write('%s /data jffs2 rw 0 0\n' % (self.dev / 'mtdblock11'))
                before = temporary.read_bytes()
                self.run_helper(action='prepare', ok=False)
                self.assertEqual(temporary.read_bytes(), before)
                self.assertFalse((self.dev / 'format-calls').exists())

    def test_initial_orphan_stream_read_failure_preserves_intent(self):
        self.pause_initial_orphan()
        fake = self.p / 'failed-reader'
        fake.write_text('#!/bin/sh\n[ "$1" = --probe-stream ] && { echo UN260_STORAGE_STREAM_1; exit 0; }\nexit 1\n')
        fake.chmod(0o755)
        result = self.run_helper(action='prepare', extra="SYNC_TOOL='%s'" % fake, ok=False)
        self.assertIn('NAND read/hash failed', result.stderr)
        self.assertTrue((self.state / 'pending.tmp').exists())
        self.assertFalse((self.dev / 'format-calls').exists())

    def test_initial_orphan_other_phase_never_claimed_as_initial(self):
        self.pause_v3('volume-ready')
        self.state.joinpath('pending').rename(self.state / 'pending.tmp')
        original = self.state.joinpath('pending.tmp').read_bytes()
        self.run_helper(action='prepare', ok=False)
        self.assertEqual(self.state.joinpath('pending.tmp').read_bytes(), original)
        self.assertEqual(self.dev.joinpath('format-calls').read_text(), 'format\n')

    def test_initial_orphan_retires_only_tmp_and_keeps_backup_for_retry(self):
        self.nand.write_bytes(b'UNKNOWN!' * 512)
        self.pause_initial_orphan()
        old = {p.name: p.read_bytes() for p in next(self.usb.glob('UN260_APP_STORAGE_BACKUP_*')).iterdir() if p.is_file()}
        old_directory = next(self.usb.glob('UN260_APP_STORAGE_BACKUP_*'))
        self.run_helper(action='prepare')
        self.run_helper(action='migrate "$POLICY_V3"'); self.assert_active()
        self.assertEqual(len(list(self.usb.glob('UN260_APP_STORAGE_BACKUP_*'))), 2)
        self.assertEqual({p.name: p.read_bytes() for p in old_directory.iterdir() if p.is_file()}, old)

    def test_current_query_managed_is_read_only_and_lock_sensitive(self):
        self.run_helper()
        self.vol.joinpath('test_lvgl').write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        before = {str(p): p.read_bytes() for p in self.p.rglob('*') if p.is_file() and not p.is_symlink()}
        # Layout verification is used under an updater lock; fallback launching
        # is forbidden until the updater has released its transaction/lock.
        self.assertEqual(self.query_public('--verify-layout').returncode, 0)
        self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.assertEqual(self.query_public('--can-launch-current').returncode, 0)
        self.assertNotEqual(self.query_legacy().returncode, 0)
        for path, data in before.items():
            if '/lock/' not in path:
                self.assertEqual(Path(path).read_bytes(), data)
        for artifact in (self.vol / 'test_lvgl.un260-new', self.vol / 'test_lvgl.migration-new', self.state / 'pending.tmp'):
            artifact.write_bytes(b'PENDING')
            self.assertNotEqual(self.query_public('--can-launch-current').returncode, 0)
            artifact.unlink()

    def test_current_query_rejects_unowned_ro_malformed_managed_app(self):
        self.run_helper()
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

    def test_legacy_query_is_read_only_and_requires_safe_elf(self):
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.app.write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        before = {str(p): p.read_bytes() for p in (self.p / 'root').rglob('*') if p.is_file()}
        self.assertEqual(self.query_legacy().returncode, 0)
        self.assertEqual(before, {str(p): p.read_bytes() for p in (self.p / 'root').rglob('*') if p.is_file()})
        self.assertFalse(self.state.exists())
        for data in (b'\x7fELF', b'#!/bin/sh\nexit 0\n', b'\x7fELF\x02\x02\x01' + b'\0' * 57):
            self.app.write_bytes(data)
            self.assertNotEqual(self.query_legacy().returncode, 0)

    def test_legacy_query_rejects_every_pending_artifact(self):
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.app.write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        items = [self.state / name for name in ('active', 'active.tmp', 'pending', 'pending.tmp')]
        items += [Path(str(self.app) + suffix) for suffix in
                  ('.un260-old', '.un260-new', '.un260-restore', '.un260-volume-old', '.un260-volume-new')]
        items += [self.p / 'lock', self.p / 'root/var/lib/un260-updater/transaction']
        for artifact in items:
            with self.subTest(artifact=artifact):
                artifact.parent.mkdir(parents=True, exist_ok=True)
                artifact.write_bytes(b'keep')
                self.assertNotEqual(self.query_legacy().returncode, 0)
                self.assertEqual(artifact.read_bytes(), b'keep')
                artifact.unlink()
                artifact.symlink_to(self.p / 'nonexistent')
                self.assertNotEqual(self.query_legacy().returncode, 0)
                artifact.unlink()

    def test_legacy_query_rejects_symlink_and_nonexecutable_app(self):
        (self.p / 'lock/pid').unlink(); (self.p / 'lock').rmdir()
        self.app.write_bytes(b'\x7fELF\x01\x01\x01' + b'\0' * 57)
        self.app.chmod(0o644)
        self.assertNotEqual(self.query_legacy().returncode, 0)
        self.app.chmod(0o755)
        saved = self.p / 'saved-app'; self.app.rename(saved); self.app.symlink_to(saved)
        self.assertNotEqual(self.query_legacy().returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
