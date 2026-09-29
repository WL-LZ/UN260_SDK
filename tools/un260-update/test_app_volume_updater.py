#!/usr/bin/env python3
"""Exercise real UPK parser/planner and rollback with a separate app path.

Two host directories emulate filesystems, not actual UBIFS power-loss behavior.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_storage_bootstrap import archive, SDK, UPDATER_REL, SHELL, OLD_UPDATER
from build_storage_bootstrap import build as build_bridge, APPROVED
from build_delta import build as build_delta
from test_storage_sync import native_tool, ui_fingerprint

CAPABILITY = 'storage_layout=app-volume-v1\nstorage_guard=syncfs-v1\n'
APP = 'usr/local/bin/test_lvgl'


class VolumeUpdaterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-volume-updater-')
        self.p = Path(self.temp.name)
        self.root = self.p / 'root'; self.usb = self.p / 'usb'; self.vol = self.p / 'app-volume'
        self.usb.mkdir(); self.vol.mkdir()
        self.files = {APP: (b'old app', 0o755), 'usr/local/lib/liblvgl.so': (b'old library', 0o755),
            'usr/bin/ui_update.sh': ((SDK / UPDATER_REL).read_bytes(), 0o755),
            'usr/local/bin/un260_storage_sync': (native_tool().read_bytes(), 0o755),
            'usr/local/bin/un260_app_storage': ((SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage').read_bytes(), 0o755),
            'etc/init.d/S00lvgl': ((SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes(), 0o755)}
        for name, (data, mode) in self.files.items():
            dest = self.vol / 'test_lvgl' if name == APP else self.root / name
            dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(data); dest.chmod(mode)
        self.launcher = self.root / APP; self.launcher.write_bytes(b'FIXED LAUNCHER'); self.launcher.chmod(0o755)
        self.business = self.root / 'etc/ui_state/count_history/record'
        self.business.parent.mkdir(parents=True); self.business.write_bytes(b'BUSINESS')
        self.base = self.p / 'base.upk'; archive(self.base, self.files, metadata_extra=CAPABILITY)
        self.files[APP] = (b'NEW APP' + b'x' * (2 * 1024 * 1024), 0o755)
        self.files['usr/local/lib/liblvgl.so'] = (b'new library', 0o755)
        self.full = self.p / 'full.upk'; self.pack()

    def pack(self, capability=CAPABILITY):
        archive(self.full, self.files, metadata_extra=capability)

    def tearDown(self):
        self.assertEqual(self.launcher.read_bytes(), b'FIXED LAUNCHER')
        self.assertEqual(self.business.read_bytes(), b'BUSINESS')
        self.temp.cleanup()

    def run_update(self, package=None, extra='', action='install_bundle', ok=True, source=None, managed=1):
        runner = self.p / 'run.sh'
        runner.write_text('''set -eu
UN260_UPDATER_LIBRARY_ONLY=1
. '%s'
ROOT_PREFIX='%s'
APP_VOLUME=%s
APP_VOLUME_DIR='%s'
USB_MNT='%s'
UPDATE_DIR=$USB_MNT/update
BUNDLE_PATH='%s'
STAGE_DIR=$UPDATE_DIR/.un260_stage
BACKUP_DIR=$UPDATE_DIR/.un260_backup
BACKUP_STATE=$BACKUP_DIR/state.tsv
PLAN_FILE=$UPDATE_DIR/.un260_plan
SEEN_TARGETS=$UPDATE_DIR/.un260_seen_targets
ARCHIVE_LIST=$UPDATE_DIR/.un260_archive.list
ARCHIVE_TYPES=$UPDATE_DIR/.un260_archive.types
CHECKSUM_TARGETS=$UPDATE_DIR/.un260_checksum_targets
PAYLOAD_FILES=$UPDATE_DIR/.un260_payload_files
INSTALLED_STATE_DIR=$ROOT_PREFIX/var/lib/un260-updater
INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64
JOURNAL=$INSTALLED_STATE_DIR/transaction
STATUS_FILE=$ROOT_PREFIX/status
STATUS_TEMP=$ROOT_PREFIX/status.tmp
LOG=$ROOT_PREFIX/log
RESULT_FILE=$ROOT_PREFIX/result
LOCK_DIR=$ROOT_PREFIX/update-lock
mkdir -p "$LOCK_DIR"
printf '%%s\n' "$$" > "$LOCK_DIR/pid"
trap 'rm -f "$LOCK_DIR/pid"; rmdir "$LOCK_DIR"' 0
sync() { :; }
df() {
    free=4096
    [ "$2" != "$USB_MNT" ] || free=100000
    [ "$2" != "$APP_VOLUME_DIR" ] || free=${APP_FREE:-20000}
    printf 'FS Size Used Free Use Mount\nfixture 100000 1 %%s 1%% /\n' "$free"
}
%s
%s
''' % (source or SDK / UPDATER_REL, self.root, managed, self.vol, self.usb, package or self.full, extra, action))
        r = subprocess.run(SHELL + [str(runner)], capture_output=True, text=True)
        log = (self.root / 'log').read_text() if (self.root / 'log').exists() else ''
        if ok is not None:
            self.assertEqual(r.returncode == 0, ok, r.stdout + r.stderr + log)
        return r, log

    def test_full_and_repeat_with_low_root_free(self):
        _, log = self.run_update()
        self.assertIn('root_required=2053KB', log)
        self.assertIn('App preflight:', log)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.run_update()
        self.assertEqual((self.root / 'var/lib/un260-updater/installed.fnv64').read_text().strip(),
                         ui_fingerprint(self.full.read_bytes()))

    def test_low_volume_free_refuses_before_writes(self):
        self.run_update(extra='APP_FREE=2100', ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertEqual((self.root / 'usr/local/lib/liblvgl.so').read_bytes(), b'old library')

    def test_failed_backup_readback_rolls_back(self):
        self.run_update(extra='backup_digest() { return 1; }', ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertIn('success=0', (self.root / 'status').read_text())

    def test_final_usb_flush_never_reports_success(self):
        self.run_update(extra='usb_barrier() { [ "$LAST_STAGE" != finish ]; }', ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertIn('success=0', (self.root / 'status').read_text())
        self.assertIn('result=fail', (self.root / 'result').read_text())
        self.assertFalse((self.root / 'var/lib/un260-updater/installed.fnv64').exists())

    def test_result_write_failure_never_reports_success(self):
        self.run_update(extra='write_result() { return 1; }', ok=False)
        self.assertIn('success=0', (self.root / 'status').read_text())

    def test_copy_failure_rolls_back_both_filesystems(self):
        self.run_update(extra='''cp() {
    case "$2" in *liblvgl.so.un260-new) return 1 ;; esac
    command cp "$@"
}''', ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertEqual((self.root / 'usr/local/lib/liblvgl.so').read_bytes(), b'old library')

    def test_crash_before_commit_recovers_without_usb(self):
        r, _ = self.run_update(extra='''mv() {
    command mv "$@"
    case "$*" in *test_lvgl.un260-new*) exit 88 ;; esac
}''', ok=None)
        self.assertEqual(r.returncode, 88)
        self.usb.rename(self.p / 'disconnected-usb')
        self.run_update(action='recover_transaction')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.assertEqual((self.root / 'usr/local/lib/liblvgl.so').read_bytes(), b'old library')

    def test_crash_after_commit_finishes_cleanup(self):
        r, _ = self.run_update(extra='recover_transaction() { exit 88; }', ok=None)
        self.assertEqual(r.returncode, 88)
        self.usb.rename(self.p / 'disconnected-usb')
        self.run_update(action='recover_transaction')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])
        self.assertEqual((self.root / 'usr/local/lib/liblvgl.so').read_bytes(), b'new library')

    def test_delta_uses_real_app_not_launcher(self):
        delta = self.p / 'delta.upk'; build_delta(self.base, self.full, delta)
        self.run_update(package=delta)
        self.run_update(package=delta)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])

    def test_old_package_refused(self):
        self.pack(capability='')
        self.run_update(ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_declared_capability_cannot_install_old_recovery(self):
        self.files['usr/bin/ui_update.sh'] = (b'OLD UPDATER', 0o755)
        self.pack(); self.run_update(ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_delete_helper_is_prohibited(self):
        del self.files['usr/local/bin/un260_app_storage']; self.pack()
        delta = self.p / 'delta.upk'; build_delta(self.base, self.full, delta)
        self.run_update(package=delta, ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_symlink_volume_destination_refused(self):
        f = self.vol / 'test_lvgl'; f.unlink(); f.symlink_to(self.business)
        self.run_update(ok=False)

    def test_runtime_marker_not_writable_by_package(self):
        self.files['etc/un260/app-storage/active'] = (b'EVIL', 0o644)
        self.pack(); self.run_update(ok=False)

    def test_migration_cannot_be_bundled_with_app_replacement(self):
        self.pack(capability=CAPABILITY + 'storage_migration=BAD\npackage_type=storage-migration\n')
        self.run_update(ok=False)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')

    def test_three_upk_handoff_from_old_installer(self):
        # The helper's NAND state machine has its own suite; here emulate only
        # its API to test the real old/new installers and all three real archives.
        marker = self.root / 'etc/un260/app-storage/active'
        helper = ('''#!/bin/sh
# UN260_APP_VOLUME_V1: test fixture only, NO hardware operations.
# UN260_STORAGE_SYNC_1
# UN260_MTD_STREAM_1
set -eu
case "$1" in
  --prepare) exit 0 ;;
  --migrate)
    [ "$2" = '%s' ]
    [ ! -f '%s' ] || exit 0
    mkdir -p '%s'
    cp '%s' '%s'
    printf 'FIXED LAUNCHER' > '%s'
    printf 'UN260_APP_VOLUME_V1\n' > '%s'
    ;;
  *) exit 2 ;;
esac
''' % (APPROVED, marker, marker.parent, self.launcher, self.vol / 'test_lvgl', self.launcher, marker)).encode()
        self.files['usr/local/bin/un260_app_storage'] = (helper, 0o755)
        self.files['usr/local/bin/un260_resource_cleanup'] = (b'#!/bin/sh\nexit 0\n', 0o755)
        self.pack()
        bridge = self.p / 'bridge.upk'; migration = self.p / 'migration.upk'
        build_bridge(self.full, bridge); build_bridge(self.full, migration, migrate=True)
        old_source = self.p / 'old.sh'; old_source.write_text(OLD_UPDATER)
        (self.root / 'usr/bin/ui_update.sh').write_text(OLD_UPDATER)
        (self.root / 'usr/local/bin/un260_app_storage').unlink()
        self.launcher.write_bytes(b'old app'); (self.vol / 'test_lvgl').unlink()
        self.run_update(package=bridge, source=old_source, managed=0)
        self.assertEqual(self.launcher.read_bytes(), b'old app')
        self.assertFalse(marker.exists())
        # New on-disk updater handles the explicit migration request.
        source = self.root / 'usr/bin/ui_update.sh'
        self.run_update(package=migration, source=source, managed=0, action='prepare_app_storage; install_bundle')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), b'old app')
        self.run_update(source=source, managed=0, action='prepare_app_storage; install_bundle')
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.files[APP][0])


if __name__ == '__main__':
    unittest.main(verbosity=2)
