#!/usr/bin/env python3
"""Actual engine journal/trial tests on host files and live ELF processes.

Native syncfs validates inherited root/app descriptors. Boot publication uses
the real parser/hash/mode/GC code and a check-only fixture player; its codec,
physical NAND power loss, application scanout and GUI are separate tests.
"""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
MODULE = SDK / 'tools/un260-update/engine_trial.sh'
ACTIVE = 'etc/un260/boot-active'
TREE = 'usr/local/lib/un260-boot/versions'
APP = 'usr/local/bin/test_lvgl'
LIB = 'usr/local/lib/liblvgl.so'
RUNTIME = 'usr/local/lib/un260-startup/runtime.sh'
CORE = 'etc/un260/update-core.manifest'
RUNTIME_SHA = 'etc/un260/startup-runtime.sha256'
RESOURCE = 'usr/local/share/lvgl_data/fixture.bin'
OLD_BOOT = '01234567-1234-5678-9abc-0123456789ab'
NEW_BOOT = 'abcdef01-2345-6789-abcd-0123456789ab'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def descriptor(current, previous=('none', 'none')):
    return ('\n'.join(('UN260_BOOT_ACTIVE_V1', *current, *previous)) + '\n').encode()


def bundle(root, name):
    player = b'#!/bin/sh\n# check-only host fixture; no display/codec claim\nexit 0\n'
    theme = b'UNBTPK1\0' + bytes(20) + struct.pack('<I', 1) + name.encode()
    ph, th = sha(player), sha(theme)
    identity = sha(('player_sha256=%s\ntheme_sha256=%s\nmode=animation\n' % (ph, th)).encode())
    manifest = ('schema=un260-boot-bundle-v1\nid=%s\nprotocol=1\nplayer_sha256=%s\nplayer_bytes=%d\n'
                'theme_sha256=%s\ntheme_bytes=%d\nmode=animation\n' %
                (identity, ph, len(player), th, len(theme))).encode()
    directory = root / TREE / identity
    directory.mkdir(parents=True)
    for filename, data, mode in [('player', player, 0o755), ('theme.pack', theme, 0o644), ('manifest', manifest, 0o644)]:
        path = directory / filename
        path.write_bytes(data)
        path.chmod(mode)
    return identity, sha(manifest)


class TrialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native_temp = tempfile.TemporaryDirectory(prefix='un260-trial-native-')
        cls.native = Path(cls.native_temp.name)
        subprocess.run(['gcc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
            str(SDK / 'source/artinchip/test-lvgl/tools/un260_storage_sync.c'), '-o', str(cls.native / 'sync')],
            check=True, capture_output=True)
        cfile = cls.native / 'app.c'
        cfile.write_text('#include <unistd.h>\nint main(void) { for(;;) pause(); }\n')
        subprocess.run(['gcc', '-O2', str(cfile), '-o', str(cls.native / 'app')], check=True, capture_output=True)
        cls.shell = [shutil.which('busybox'), 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['/bin/sh']
        if not cls.shell[0]:
            raise unittest.SkipTest('BusyBox unavailable')

    @classmethod
    def tearDownClass(cls):
        cls.native_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-trial-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'root'
        self.app_volume = self.base / 'app'
        self.app_volume.mkdir()
        self.stage = self.base / 'usb/update/.un260_dispatch.1.aBcD12'
        self.stage.mkdir(parents=True)
        self.state_dir = self.root / 'var/lib/un260-updater'
        self.journal = self.state_dir / 'transaction'
        self.journal.mkdir(parents=True, mode=0o700)
        (self.root / 'etc/un260').mkdir(parents=True)
        self.boot_id = self.base / 'boot_id'
        self.boot_id.write_text(OLD_BOOT + '\n')
        self.health = self.base / 'health'
        self.old_a = bundle(self.root, 'old-A')
        self.old_b = bundle(self.root, 'old-B')
        self.new_c = bundle(self.root, 'new-C')
        self.old_active = descriptor(self.old_b, self.old_a)
        self.old_previous = descriptor(self.old_a)
        self.put(self.root / ACTIVE, self.old_active, 0o644)
        self.put(self.root / (ACTIVE + '.previous'), self.old_previous, 0o644)
        self.put(self.state_dir / 'installed.fnv64', b'0123456789abcdef\n', 0o600)
        self.put(self.state_dir / 'unified.completed', b'a' * 64 + b'\n', 0o600)
        runtime = b'#!/bin/sh\n# NEW_RUNTIME\nexit 0\n'
        core = b'UN260_UPDATE_CORE_V1\n' + b''.join(
            (role + '_sha256=' + 'b' * 64 + '\n').encode()
            for role in ['engine', 'sync', 'unpack', 'app_storage', 'resource_cleanup'])
        self.new = {
            APP: ((self.native / 'app').read_bytes(), 0o755),
            LIB: (b'new-library', 0o755), RUNTIME: (runtime, 0o755),
            CORE: (core, 0o644), RUNTIME_SHA: ((sha(runtime) + '\n').encode(), 0o644),
            RESOURCE: (b'new-resource', 0o644),
        }
        self.old = {rel: (('old:' + rel).encode(), mode) for rel, (_, mode) in self.new.items()}
        self.original_inodes = {}
        rows = []
        for rel, (data, mode) in sorted(self.new.items()):
            dest = self.destination(rel)
            self.put(dest, self.old[rel][0], mode)
            os.link(dest, str(dest) + '.un260-old')
            self.original_inodes[rel] = dest.stat().st_ino
            replacement = Path(str(dest) + '.un260-new')
            self.put(replacement, data, mode)
            replacement.replace(dest)
            rows.append('1|' + rel + '\n')
        for name in ['manifest', 'player', 'theme.pack']:
            rel = TREE + '/' + self.new_c[0] + '/' + name
            p = self.root / rel
            self.new[rel] = (p.read_bytes(), p.stat().st_mode & 0o777)
            rows.append('0|' + rel + '\n')
        self.put(self.journal / 'state', ''.join(rows).encode(), 0o600)
        self.table()
        (self.stage / 'manifest.ini').write_text('package_type=ui\n')

    @staticmethod
    def put(path, data, mode):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode)

    def destination(self, rel):
        return self.app_volume / 'test_lvgl' if rel == APP else self.root / rel

    def table(self):
        rows = ['%s|%04o|%d|%s\n' % (sha(data), mode, len(data), rel)
                for rel, (data, mode) in sorted(self.new.items())]
        self.table_data = ''.join(rows).encode()
        self.target_id = sha(self.table_data)
        (self.stage / 'target.tsv').write_bytes(self.table_data)

    def run_shell(self, body, expected=0):
        q = shlex.quote
        script = '''set -u
UN260_UPDATER_LIBRARY_ONLY=1
. UPDATER
. MODULE
ROOT_PREFIX=ROOT
APP_VOLUME=1
APP_VOLUME_DIR=APPDIR
INSTALLED_STATE_DIR=STATE
INSTALLED_HASH_PATH=$INSTALLED_STATE_DIR/installed.fnv64
JOURNAL=$INSTALLED_STATE_DIR/transaction
STAGE_DIR=STAGE
FULL_TARGET_ID=TARGET
BOOT_REQUEST_ID=NEWID
BOOT_REQUEST_HASH=NEWHASH
BOOT_OLD_ID=OLDID
BOOT_OLD_HASH=OLDHASH
BOOT_OLD_PREVIOUS=PREVID
BOOT_OLD_PREVIOUS_HASH=PREVHASH
TRIAL_EXPECTED_UID=UID
TRIAL_BOOT_ID_PATH=BOOTUUID
TRIAL_HEALTH_PATH=HEALTH
TRIAL_ENABLED=1
DISPATCH_MODE=update
SYNC_TOOL=SYNC
GUARDED=1
APP_GUARDED=1
exec 7< "$ROOT_PREFIX"
exec 9< "$APP_VOLUME_DIR"
LOG=LOGPATH
: >> "$LOG"
BODY
'''
        replacements = {
            'UPDATER': q(str(UPDATER)), 'MODULE': q(str(MODULE)), 'ROOT': q(str(self.root)),
            'APPDIR': q(str(self.app_volume)), 'STATE': q(str(self.state_dir)), 'STAGE': q(str(self.stage)),
            'TARGET': self.target_id, 'NEWID': self.new_c[0], 'NEWHASH': self.new_c[1],
            'OLDID': self.old_b[0], 'OLDHASH': self.old_b[1], 'PREVID': self.old_a[0], 'PREVHASH': self.old_a[1],
            'UID': str(os.getuid()), 'BOOTUUID': q(str(self.boot_id)), 'HEALTH': q(str(self.health)),
            'SYNC': q(str(self.native / 'sync')), 'LOGPATH': q(str(self.base / 'log')), 'BODY': body,
        }
        # Replace whole template lines to avoid substituting text inside paths.
        for key in sorted(replacements, key=len, reverse=True):
            script = script.replace('=' + key + '\n', '=' + replacements[key] + '\n')
            script = script.replace('. ' + key + '\n', '. ' + replacements[key] + '\n')
        script = script.replace('\nBODY\n', '\n' + body + '\n')
        result = subprocess.run(self.shell, input=script, text=True, capture_output=True, timeout=40)
        if expected is not None:
            self.assertEqual(result.returncode, expected, result.stdout + result.stderr +
                             (self.base / 'log').read_text())
        return result

    def prepare(self):
        self.run_shell('boot_prepare_publication && trial_prepare && : > "$JOURNAL/committed" && '
                       'local_barrier && boot_publish_committed && recover_transaction && trial_is_committed')
        self.assertTrue((self.journal / 'trial').is_file())
        for rel in self.old:
            self.assertTrue(Path(str(self.destination(rel)) + '.un260-old').is_file())
        self.boot_id.write_text(NEW_BOOT + '\n')

    def assert_old(self):
        for rel, (data, mode) in self.old.items():
            dest = self.destination(rel)
            self.assertEqual(dest.read_bytes(), data)
            self.assertEqual(dest.stat().st_mode & 0o777, mode)
            self.assertEqual(dest.stat().st_ino, self.original_inodes[rel])
            self.assertFalse(Path(str(dest) + '.un260-old').exists())
        self.assertEqual((self.root / ACTIVE).read_bytes(), self.old_active)
        self.assertEqual((self.root / (ACTIVE + '.previous')).read_bytes(), self.old_previous)
        self.assertEqual((self.state_dir / 'installed.fnv64').read_bytes(), b'0123456789abcdef\n')
        self.assertEqual((self.state_dir / 'unified.completed').read_bytes(), b'a' * 64 + b'\n')
        self.assertFalse(self.journal.exists())

    def app_process(self):
        process = subprocess.Popen([str(self.destination(APP))])
        def stop():
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)
        self.addCleanup(stop)
        self.health.write_text(str(process.pid) + '\n')
        return process

    def test_prepare_retains_whole_transaction_and_cold_gate(self):
        self.prepare()
        self.boot_id.write_text(OLD_BOOT + '\n')
        self.run_shell('trial_boot_eligible', 1)
        self.run_shell('trial_rollback', 1)
        self.boot_id.write_text(NEW_BOOT + '\n')
        self.run_shell('trial_boot_eligible && trial_runtime_valid')
        lines = (self.journal / 'trial').read_text().splitlines()
        self.assertEqual(len(lines), 11)
        self.assertEqual(lines[0], 'UN260_UPDATE_TRIAL_V1')
        self.assertEqual((self.journal / 'trial.targets').read_bytes(), self.table_data)

    def test_real_pid_confirm_and_gc(self):
        self.prepare()
        process = self.app_process()
        self.run_shell('trial_confirm %d' % process.pid)
        self.assertFalse(self.journal.exists())
        for rel, (data, _) in self.new.items():
            self.assertEqual(self.destination(rel).read_bytes(), data)
        for rel in self.old:
            self.assertFalse(Path(str(self.destination(rel)) + '.un260-old').exists())
        self.assertFalse((self.root / TREE / self.old_a[0]).exists())
        self.assertTrue((self.root / TREE / self.old_b[0]).exists())

    def test_real_pid_confirm_without_od(self):
        self.prepare()
        process = self.app_process()
        result = self.run_shell('od() { printf "UNSUPPORTED_DEVICE_COMMAND\\n" >&2; return 127; }\n'
                                'trial_confirm %d' % process.pid)
        self.assertNotIn('UNSUPPORTED_DEVICE_COMMAND', result.stderr)
        self.assertFalse(self.journal.exists())

    def test_full_rollback_old_inodes_boot_and_receipts(self):
        self.prepare()
        self.put(self.state_dir / 'installed.fnv64', b'ffffffffffffffff\n', 0o600)
        self.put(self.state_dir / 'unified.completed', b'f' * 64 + b'\n', 0o600)
        self.run_shell('trial_rollback')
        self.assert_old()

    def test_uncommitted_preparation_recovers(self):
        self.run_shell('boot_prepare_publication && trial_prepare && recover_transaction')
        self.assert_old()

    def test_confirm_rejects_marker_pid_exe_boot_and_runtime(self):
        self.prepare()
        process = self.app_process()
        good_health = self.health.read_bytes()
        for data in [b'0\n', b'999999999\n', good_health + b'\n', good_health.rstrip(), b'other\n']:
            with self.subTest(marker=data):
                self.health.write_bytes(data)
                self.run_shell('trial_confirm %d' % process.pid, 2)
                self.assertTrue((self.journal / 'trial').exists())
        self.health.write_bytes(good_health)
        self.boot_id.write_text(OLD_BOOT + '\n')
        self.run_shell('trial_confirm %d' % process.pid, 1)
        self.boot_id.write_text(NEW_BOOT + '\n')
        for rel in [APP, LIB, RUNTIME, CORE, RUNTIME_SHA]:
            with self.subTest(runtime=rel):
                path = self.destination(rel)
                original = path.read_bytes()
                # Replacing the disk inode keeps the real candidate process live.
                wrong = Path(str(path) + '.wrong')
                self.put(wrong, b'wrong', self.new[rel][1])
                wrong.replace(path)
                self.run_shell('trial_confirm %d' % process.pid, 2)
                self.put(path, original, self.new[rel][1])
        self.put(self.root / ACTIVE, descriptor(self.old_b, self.old_a), 0o644)
        self.run_shell('trial_confirm %d' % process.pid, 2)

    def test_confirm_rejects_different_live_elf(self):
        self.prepare()
        process = subprocess.Popen(['/bin/sleep', '60'])
        self.addCleanup(lambda: (process.terminate(), process.wait(timeout=10)))
        self.health.write_text(str(process.pid) + '\n')
        self.run_shell('trial_confirm %d' % process.pid, 2)
        self.assertTrue((self.journal / 'trial').exists())

    def test_strict_record_table_state_and_unknown_metadata(self):
        self.prepare()
        files = [self.journal / 'trial', self.journal / 'trial.targets', self.journal / 'state']
        original = {p: p.read_bytes() for p in files}
        cases = [
            (files[0], original[files[0]].replace(b'UN260_UPDATE_TRIAL_V1', b'UNKNOWN')),
            (files[0], original[files[0]] + b'extra=1\n'),
            (files[0], original[files[0]].replace(b'targets_count=', b'wrong_count=')),
            (files[0], original[files[0]].replace(b'boot_id=' + OLD_BOOT.encode(), b'boot_id=' + b'-' * 36)),
            (files[1], original[files[1]] + original[files[1]].splitlines(keepends=True)[0]),
            (files[1], b''), (files[2], original[files[2]] + original[files[2]].splitlines(keepends=True)[0]),
            (files[2], b'1|../../data\n'), (files[2], original[files[2]].rstrip(b'\n')),
        ]
        for path, data in cases:
            with self.subTest(path=path.name, data=data[:25]):
                path.write_bytes(data)
                self.run_shell('trial_is_committed', 2)
                path.write_bytes(original[path])
        unknown = self.journal / 'unknown'
        unknown.write_text('preserve')
        self.run_shell('recover_transaction', 1)
        self.assertEqual(unknown.read_text(), 'preserve')
        unknown.unlink()
        (self.journal / 'trial.rollback').write_bytes(original[files[0]])
        (self.journal / 'trial.rollback').chmod(0o600)
        self.run_shell('trial_is_committed', 2)

    def test_symlink_and_mode_fail_closed(self):
        self.prepare()
        for name in ['trial', 'trial.targets', 'trial.boot-active', 'state']:
            with self.subTest(name=name):
                path = self.journal / name
                original = path.read_bytes()
                path.unlink()
                path.symlink_to(self.base / 'missing')
                self.run_shell('trial_is_committed', 2)
                path.unlink()
                self.put(path, original, 0o600)
        (self.journal / 'trial').chmod(0o644)
        self.run_shell('trial_is_committed', 2)
        (self.journal / 'trial').chmod(0o600)
        self.destination(LIB).chmod(0o644)
        self.run_shell('trial_runtime_valid', 2)

    def test_confirmed_cleanup_cut_missing_snapshots_resumes(self):
        self.prepare()
        process = self.app_process()
        self.run_shell('rm() { case "$*" in *trial.targets*) return 1 ;; esac; command rm "$@"; }\n'
                       'trial_confirm %d' % process.pid, 1)
        self.assertTrue((self.journal / 'trial.confirmed').is_file())
        (self.journal / 'trial.boot-active').unlink()
        (self.journal / 'trial.targets').unlink()
        self.run_shell('recover_transaction')
        self.assertFalse(self.journal.exists())
        self.assertEqual(self.destination(APP).read_bytes(), self.new[APP][0])

    def test_rolled_back_cleanup_cut_missing_snapshots_resumes(self):
        self.prepare()
        self.run_shell('rm() { case "$*" in *trial.targets*) return 1 ;; esac; command rm "$@"; }\ntrial_rollback', 1)
        self.assertTrue((self.journal / 'trial.rollback').is_file())
        self.assertTrue((self.journal / 'rolled-back').is_file())
        (self.journal / 'trial.boot-active').unlink()
        (self.journal / 'trial.targets').unlink()
        self.run_shell('recover_transaction')
        self.assert_old()

    def test_rollback_metadata_barrier_cuts_resume_without_republish(self):
        self.prepare()
        original_trial = (self.journal / 'trial').read_bytes()
        # Cut after the first old-descriptor rename, before receipts/committed.
        self.run_shell('cut_count=0\nlocal_barrier() { "$SYNC_TOOL" --fd 7 "$ROOT_PREFIX" && '
                       '"$SYNC_TOOL" --fd 9 "$APP_VOLUME_DIR" || return 1; '
                       'cut_count=$((cut_count+1)); [ "$cut_count" != 3 ]; }\ntrial_rollback', 1)
        self.assertEqual((self.journal / 'trial.rollback').read_bytes(), original_trial)
        self.assertEqual((self.root / ACTIVE).read_bytes(), self.old_active)
        self.assertTrue((self.journal / 'committed').exists())
        self.run_shell('recover_transaction')
        self.assert_old()

    def test_absent_old_receipts_removed_on_rollback(self):
        (self.state_dir / 'installed.fnv64').unlink()
        (self.state_dir / 'unified.completed').unlink()
        self.prepare()
        self.put(self.state_dir / 'installed.fnv64', b'ffffffffffffffff\n', 0o600)
        self.put(self.state_dir / 'unified.completed', b'f' * 64 + b'\n', 0o600)
        self.run_shell('trial_rollback')
        self.assertFalse((self.state_dir / 'installed.fnv64').exists())
        self.assertFalse((self.state_dir / 'unified.completed').exists())
        self.assertFalse(self.journal.exists())

    def test_prepare_copy_failure_partial_cleanup_and_legacy_unchanged(self):
        self.run_shell('dd() { return 1; }\ntrial_prepare', 1)
        self.run_shell('recover_transaction')
        self.assert_old()

    def test_no_trial_fast_path_never_hashes_resources(self):
        shutil.rmtree(self.journal)
        self.run_shell('checked_digest() { echo unexpected >&2; return 9; }\n'
                       'trial_runtime_valid && trial_is_committed', 1)

    def test_pending_missing_snapshots_and_commit_flags_rejected(self):
        self.prepare()
        (self.journal / 'trial.targets').unlink()
        self.run_shell('trial_is_committed', 2)
        self.run_shell('recover_transaction', 1)
        self.assertTrue((self.journal / 'trial').exists())
        self.put(self.journal / 'trial.targets', self.table_data, 0o600)
        record = (self.journal / 'trial').read_bytes()
        (self.journal / 'trial').unlink()
        self.run_shell('trial_is_committed', 2)
        self.put(self.journal / 'trial', record, 0o600)
        (self.journal / 'committed').write_text('not-empty')
        self.run_shell('trial_is_committed', 2)

    def test_partial_prepare_unknown_file_preserved(self):
        self.put(self.journal / 'trial.targets.tmp', b'incomplete', 0o600)
        self.run_shell('recover_transaction')
        self.assert_old()

    def test_bound_inventory_with_matching_record_digest_rejected(self):
        self.prepare()
        record = (self.journal / 'trial').read_text()
        rows = self.table_data.decode().splitlines(keepends=True)
        table_cases = [
            list(reversed(rows)), rows + [rows[0]],
            [r for r in rows if not r.endswith('|' + APP + '\n')],
            [r.replace('|0755|', '|0777|', 1) if r.endswith('|' + APP + '\n') else r for r in rows],
            [r.replace('|' + RESOURCE + '\n', '|etc/un260/updater-platform\n') for r in rows],
            [r.replace('|' + RESOURCE + '\n', '|usr/local/share/../business\n') for r in rows],
            [r.replace('|' + RESOURCE + '\n', '|usr/local/share/bad\x00name\n') for r in rows],
            [r.replace('|%d|' % len(self.new[RESOURCE][0]), '|25165825|') if r.endswith('|' + RESOURCE + '\n') else r for r in rows],
        ]
        for changed in table_cases:
            with self.subTest(table=changed[0][:75]):
                data = ''.join(changed).encode()
                self.put(self.journal / 'trial.targets', data, 0o600)
                updated = record.replace(self.target_id, sha(data))
                updated = updated.replace('targets_count=%d\n' % len(rows), 'targets_count=%d\n' % len(changed))
                self.put(self.journal / 'trial', updated.encode(), 0o600)
                self.run_shell('trial_is_committed', 2)
        self.put(self.journal / 'trial', record.encode(), 0o600)
        self.put(self.journal / 'trial.targets', self.table_data, 0o600)
        self.run_shell('trial_is_committed')

    def test_v1_32m_total_and_24m_member_table_boundaries(self):
        # Inventory parser boundary only: no invented 32MiB installed payload.
        # The transaction/preflight and actual archive budgets have other tests.
        self.prepare()
        original_record = (self.journal / 'trial').read_text()
        base_rows = [line.split('|') for line in self.table_data.decode().splitlines()]
        fixed_bytes = sum(int(row[2]) for row in base_rows if row[3] != RESOURCE)
        cases = [('total-exact24', 24 * 1024 * 1024, False, 0),
                 ('total-exact32', 32 * 1024 * 1024, False, 0),
                 ('total-over32', 32 * 1024 * 1024 + 1, False, 2),
                 ('member-exact24', 0, True, 0),
                 ('member-over24', 1, True, 2)]
        for label, amount, member_case, expected in cases:
            with self.subTest(boundary=label):
                rows = [row[:] for row in base_rows]
                if member_case:
                    for row in rows:
                        if row[3] == RESOURCE:
                            row[2] = str(24 * 1024 * 1024 + amount)
                else:
                    for row in rows:
                        if row[3] == RESOURCE:
                            row[2] = str(16 * 1024 * 1024)
                    rows.append(['c' * 64, '0644', str(amount - fixed_bytes - 16 * 1024 * 1024),
                                 'usr/local/share/lvgl_data/second.bin'])
                rows.sort(key=lambda row: row[3])
                data = ''.join('|'.join(row) + '\n' for row in rows).encode()
                self.put(self.journal / 'trial.targets', data, 0o600)
                record = original_record.replace(self.target_id, sha(data))
                record = record.replace('targets_count=%d\n' % len(base_rows), 'targets_count=%d\n' % len(rows))
                self.put(self.journal / 'trial', record.encode(), 0o600)
                self.run_shell('trial_is_committed', expected)
                self.run_shell('trial_load_record "$JOURNAL/trial" && trial_table_format_valid "$JOURNAL/trial.targets"',
                               0 if expected == 0 else 1)

    def test_pure_table_codec_accepts_transport_before_private_snapshot(self):
        self.prepare()
        transport = self.stage / 'target.tsv'
        transport.chmod(0o755)  # FAT transport can have one synthesized mode.
        self.run_shell('trial_load_record "$JOURNAL/trial" && trial_table_format_valid "$STAGE_DIR/target.tsv"')
        self.run_shell('trial_load_record "$JOURNAL/trial" && trial_table_valid "$STAGE_DIR/target.tsv"', 1)

    def test_restore_copy_corruption_and_old_boot_damage_pause_recovery(self):
        self.prepare()
        self.run_shell('dd() { printf corrupt > "$ROOT_PREFIX/$BOOT_ACTIVE.tmp"; return 0; }\ntrial_rollback', 1)
        self.assertTrue((self.journal / 'trial.rollback').exists())
        self.assertTrue((self.journal / 'committed').exists())
        self.assertEqual((self.root / ACTIVE).read_bytes(), descriptor(self.new_c, self.old_b))
        manifest = self.root / TREE / self.old_b[0] / 'manifest'
        original = manifest.read_bytes()
        manifest.write_bytes(b'broken')
        self.run_shell('recover_transaction', 1)
        self.assertTrue((self.journal / 'committed').exists())
        manifest.write_bytes(original)
        self.run_shell('recover_transaction')
        self.assert_old()

    def test_rollback_cuts_at_each_metadata_barrier(self):
        for cut in range(1, 12):
            with self.subTest(cut=cut):
                if cut != 1:
                    self.temp.cleanup()
                    self.setUp()
                self.prepare()
                result = self.run_shell('cut_count=0\nlocal_barrier() { "$SYNC_TOOL" --fd 7 "$ROOT_PREFIX" && '
                    '"$SYNC_TOOL" --fd 9 "$APP_VOLUME_DIR" || return 1; '
                    'cut_count=$((cut_count+1)); [ "$cut_count" != %d ]; }\ntrial_rollback' % cut, None)
                self.assertNotEqual(result.returncode, 0)
                self.run_shell('recover_transaction')
                self.assert_old()

    def test_legacy_disabled_does_not_retain_trial(self):
        self.run_shell('TRIAL_ENABLED=0\nboot_prepare_publication && trial_prepare && '
            ': > "$JOURNAL/committed" && boot_publish_committed && recover_transaction')
        self.assertFalse(self.journal.exists())
        self.assertEqual(self.destination(APP).read_bytes(), self.new[APP][0])


if __name__ == '__main__':
    unittest.main(verbosity=2)
