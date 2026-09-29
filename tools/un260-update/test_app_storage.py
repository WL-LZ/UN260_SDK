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


class StorageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-volume-test-')
        self.p = Path(self.temp.name)
        self.app = self.p / 'root/usr/local/bin/test_lvgl'
        self.app.parent.mkdir(parents=True)
        self.app.write_bytes(b'ELF-original-device-app'); self.app.chmod(0o755)
        self.state = self.p / 'root/etc/un260/app-storage'
        self.vol = self.p / 'volume'; self.vol.mkdir()
        self.usb = self.p / 'usb'; self.usb.mkdir()
        self.dev = self.p / 'dev'; self.dev.mkdir()
        self.nand = self.dev / 'mtd11'; self.nand.write_bytes(b'old NAND' * 512)
        self.approved = hashlib.sha256(self.nand.read_bytes()).hexdigest()
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

    def tearDown(self):
        self.assertEqual(self.user.read_bytes(), b'USER-DATA')
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
APPROVED='%s'
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
df() { printf 'FS Size Used Free Use Mount\nfixture 100000 1 99000 1%% /\n'; }
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
       self.approved, native_tool(), cut, extra, action))
        r = subprocess.run(SHELL + [str(runner)], capture_output=True, text=True)
        if ok is not None:
            self.assertEqual(r.returncode == 0, ok, r.stdout + r.stderr)
        return r

    def assert_active(self):
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'ELF-original-device-app')
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
        name = 'test_lvgl.' + hashlib.sha256(b'ELF-original-device-app').hexdigest() + '.before'
        self.assertEqual((backup / name).read_bytes(), b'ELF-original-device-app')

    def test_fleet_usb_preserves_another_application_backup(self):
        backup = self.usb / ('UN260_APP_STORAGE_BACKUP_' + self.approved)
        backup.mkdir()
        legacy = backup / 'test_lvgl.before'
        legacy.write_bytes(b'ANOTHER MACHINE APP')
        self.run_helper()
        self.assert_active()
        self.assertEqual(legacy.read_bytes(), b'ANOTHER MACHINE APP')
        name = 'test_lvgl.' + hashlib.sha256(b'ELF-original-device-app').hexdigest() + '.before'
        self.assertEqual((backup / name).read_bytes(), b'ELF-original-device-app')

    def test_unknown_fingerprint_never_formats(self):
        self.nand.write_bytes(b'NEW DATA' * 512)
        self.run_helper(ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertEqual(self.app.read_bytes(), b'ELF-original-device-app')

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
        self.assertEqual(self.app.read_bytes(), b'ELF-original-device-app')

    def test_usb_flush_failure_never_formats(self):
        self.run_helper(extra="sync() { die 'injected USB writeback error'; }", ok=False)
        self.assertFalse((self.dev / 'format-calls').exists())
        self.assertEqual(self.app.read_bytes(), b'ELF-original-device-app')

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
        self.user.unlink(); self.user.parent.rmdir()
        backing = self.p / 'factory-volume'
        enroll(self.p / 'root', base, backing, 'factory-test')
        self.user.parent.mkdir(parents=True); self.user.write_bytes(b'USER-DATA')
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
        self.assertEqual(self.app.read_bytes(), b'ELF-original-device-app')
        self.run_helper(action='prepare')
        self.run_helper(); self.assert_active()

    def test_unknown_volume_is_not_claimed(self):
        self.run_helper(extra='''ubimkvol() {
    mkdir -p "$SYS/class/ubi/ubi1_0"
    printf 'user_data\n' > "$SYS/class/ubi/ubi1_0/name"
}''', ok=False)
        self.run_helper(ok=False)
        self.assertEqual((self.dev / 'format-calls').read_text(), 'format\n')
        self.assertEqual(self.app.read_bytes(), b'ELF-original-device-app')

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


if __name__ == '__main__':
    unittest.main(verbosity=2)
