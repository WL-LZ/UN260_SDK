#!/usr/bin/env python3
"""Old updater -> tiny bootstrap -> normal update with automatic SDK cleanup.

All paths and df results are isolated host fixtures, not NAND power-cut tests.
"""
import hashlib
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
import build_storage_bootstrap as bootstrap
from test_storage_sync import native_tool

HERE = Path(__file__).resolve().parent
SDK = HERE.parents[1]
UPDATER_REL = 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
OLD_UPDATER = subprocess.check_output(['git', '-C', str(SDK), 'show', 'b632ca19d:' + UPDATER_REL]).decode()
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']


def archive(path, files, corrupt=False, metadata_extra=''):
    checks = ''.join('%s  payload/%s\n' % (hashlib.sha256(d).hexdigest(), p)
                     for p, (d, _) in sorted(files.items())).encode()
    metadata = ('format=UN260_UPGRADE\nschema=1\nproduct=UN260\nversion=test\npackage_id=%s\n%s'
                % (hashlib.sha256(checks).hexdigest(), metadata_extra)).encode()
    entries = {'manifest.ini': (metadata, 0o644), 'checksums.sha256': (checks, 0o644),
               'install.tsv': (''.join('file|%04o|%s\n' % (m, p) for p, (_, m) in files.items()).encode(), 0o644)}
    entries.update({'payload/' + p: (d, m) for p, (d, m) in files.items()})
    if corrupt:
        entries['payload/usr/local/bin/test_lvgl'] = (b'BAD', 0o755)
    with tarfile.open(path, 'w:gz', format=tarfile.USTAR_FORMAT) as out:
        directory = tarfile.TarInfo('payload'); directory.type = tarfile.DIRTYPE; directory.mode = 0o755; out.addfile(directory)
        for name, (data, mode) in entries.items():
            info = tarfile.TarInfo(name); info.mode = mode; info.size = len(data)
            out.addfile(info, io.BytesIO(data))


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-bootstrap-')
        self.path = Path(self.temp.name)
        self.root, self.usb = self.path / 'root', self.path / 'usb'
        self.usb.mkdir()
        self.demo = self.root / 'usr/local/share/ge_data/clock.bmp'
        self.demo.parent.mkdir(parents=True)
        self.demo.write_bytes(b'demo-data')
        self.user = self.root / 'etc/ui_state/count_history/record'
        self.user.parent.mkdir(parents=True); self.user.write_bytes(b'BUSINESS')
        self.image = self.demo.parent / 'custom.png'; self.image.write_bytes(b'USER-IMAGE')
        self.app = self.root / 'usr/local/bin/test_lvgl'
        self.app.parent.mkdir(parents=True); self.app.write_bytes(b'OLD-UI'); self.app.chmod(0o755)
        self.old = self.path / 'old.sh'; self.old.write_text(OLD_UPDATER)
        self.updater = self.root / 'usr/bin/ui_update.sh'
        self.updater.parent.mkdir(parents=True)
        self.updater.write_text(OLD_UPDATER); self.updater.chmod(0o755)
        self.mounts = self.path / 'mounts'; self.mounts.write_text('/dev/sda1 %s vfat rw 0 0\n' % self.usb)
        helper = (HERE / 'resource_cleanup.sh.in').read_text()
        helper = helper.replace('@APP_SHA256@', '-').replace('@MANIFEST@', '%s|9|0644|usr/local/share/ge_data/clock.bmp' % hashlib.sha256(b'demo-data').hexdigest())
        for a, b in [('ROOT=/\n', 'ROOT=%s\n' % self.root), ('USB=/mnt/usb\n', 'USB=%s\n' % self.usb),
                     ('MOUNTS=/proc/mounts\n', 'MOUNTS=%s\n' % self.mounts),
                     ('LOCK=/tmp/un260-resource-cleanup.lock\n', 'LOCK=%s\n' % (self.path / 'cleanup-lock')),
                     ('UPDATER_LOCK=/tmp/un260-updater.lock\n', 'UPDATER_LOCK=%s\n' % (self.path / 'update-lock')),
                     ('[ "$(id -u)" = 0 ]', '[ 0 = 0 ]')]:
            helper = helper.replace(a, b)
        self.new_app = b'NEW-UI' + b'x' * (8 * 1024 * 1024)
        self.files = {'usr/local/bin/un260_storage_sync': (native_tool().read_bytes(), 0o755),
                      'usr/bin/ui_update.sh': ((SDK / UPDATER_REL).read_bytes(), 0o755),
                      'usr/local/bin/un260_resource_cleanup': (helper.encode(), 0o755),
                      'usr/local/bin/un260_app_storage': ((SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage').read_bytes(), 0o755),
                      'etc/init.d/S00lvgl': ((SDK / 'package/artinchip/test-lvgl/S20test_lvgl').read_bytes(), 0o755),
                      'usr/local/bin/test_lvgl': (self.new_app, 0o755)}
        self.full = self.path / 'full.upk'; archive(self.full, self.files)
        self.bridge = self.path / 'bridge.upk'; bootstrap.build(self.full, self.bridge)

    def tearDown(self):
        self.assertEqual(self.user.read_bytes(), b'BUSINESS')
        self.assertEqual(self.image.read_bytes(), b'USER-IMAGE')
        self.temp.cleanup()

    def install(self, package, old=False, ok=True):
        runner = self.path / 'runner.sh'
        runner.write_text('''set -eu
UN260_UPDATER_LIBRARY_ONLY=1
. '%s'
ROOT_PREFIX='%s'
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
STATUS_FILE=$USB_MNT/status
STATUS_TEMP=$USB_MNT/status.tmp
LOG=$USB_MNT/log
RESULT_FILE=$USB_MNT/result
LOCK_DIR='%s'
mkdir "$LOCK_DIR"
printf '%%s\\n' "$$" > "$LOCK_DIR/pid"
trap 'rm -f "$LOCK_DIR/pid"; rmdir "$LOCK_DIR"' 0
sync() { :; }
df() {
    available=2500
    [ -e "$ROOT_PREFIX/usr/local/share/ge_data/clock.bmp" ] || available=16000
    [ "$2" != "$USB_MNT" ] || available=100000
    printf 'Filesystem Blocks Used Available Use Mount\\nfixture 200000 1 %%s 1%% /\\n' "$available"
}
install_bundle
''' % (self.old if old else self.updater, self.root, self.usb, package, self.path / 'update-lock'))
        result = subprocess.run(SHELL + [str(runner)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log = (self.usb / 'log').read_text() if (self.usb / 'log').exists() else ''
        self.assertEqual(result.returncode == 0, ok, result.stdout + log)
        return log

    def test_old_installer_accepts_updater_only_then_full(self):
        self.install(self.bridge, old=True)
        self.assertEqual(self.app.read_bytes(), b'OLD-UI')
        self.assertTrue(self.demo.exists())
        self.install(self.full)
        self.assertEqual(self.app.read_bytes(), self.new_app)
        self.assertFalse(self.demo.exists())
        backup = self.usb / 'UN260_RESOURCE_BACKUP_20260924/files/usr/local/share/ge_data/clock.bmp'
        self.assertEqual(backup.read_bytes(), b'demo-data')
        self.install(self.full)
        self.assertEqual(backup.read_bytes(), b'demo-data')

    def test_full_on_old_installer_still_rejects_low_space(self):
        self.install(self.full, old=True, ok=False)
        self.assertTrue(self.demo.exists())
        self.assertEqual(self.app.read_bytes(), b'OLD-UI')

    def test_modified_demo_not_removed(self):
        self.install(self.bridge, old=True)
        self.demo.write_bytes(b'USER-CHANGED')
        self.install(self.full, ok=False)
        self.assertEqual(self.demo.read_bytes(), b'USER-CHANGED')
        self.assertEqual(self.app.read_bytes(), b'OLD-UI')

    def test_unmounted_usb_never_cleans(self):
        self.install(self.bridge, old=True)
        self.mounts.write_text('')
        self.install(self.full, ok=False)
        self.assertTrue(self.demo.exists())
        self.assertEqual(self.app.read_bytes(), b'OLD-UI')

    def test_checksum_failure_precedes_cleanup(self):
        self.install(self.bridge, old=True)
        archive(self.full, self.files, corrupt=True)
        self.install(self.full, ok=False)
        self.assertTrue(self.demo.exists())

    def test_core_helper_required_by_bootstrap_builder(self):
        del self.files['usr/local/bin/un260_resource_cleanup']
        archive(self.full, self.files)
        before = self.bridge.read_bytes()
        with self.assertRaises(ValueError): bootstrap.build(self.full, self.bridge)
        self.assertEqual(before, self.bridge.read_bytes())

    def test_bridge_blocks_old_unguarded_cleanup_and_installs_guard_first(self):
        with tarfile.open(self.bridge, 'r:gz') as package:
            plan = package.extractfile('install.tsv').read().decode().splitlines()
            self.assertEqual(plan[0], 'file|0755|usr/local/bin/un260_storage_sync')
            self.assertIn('tree|0755|usr/local/share/ge_data', plan)
            self.assertTrue(package.getmember('payload/usr/local/share/ge_data').isdir())
            self.assertFalse(any(m.isfile() and m.name.startswith('payload/usr/local/share/ge_data/') for m in package.getmembers()))

    def test_package_owned_sdk_files_skip_maintenance(self):
        self.install(self.bridge, old=True)
        self.files['usr/local/share/ge_data/clock.bmp'] = (b'demo-data', 0o644)
        archive(self.full, self.files)
        self.assertIn('skipped; package references SDK demo files', self.install(self.full, ok=False))
        self.assertTrue(self.demo.exists())

    def test_invalid_install_target_precedes_cleanup(self):
        self.install(self.bridge, old=True)
        self.files['etc/ui_state/forbidden'] = (b'BAD', 0o644)
        archive(self.full, self.files)
        self.install(self.full, ok=False)
        self.assertTrue(self.demo.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
