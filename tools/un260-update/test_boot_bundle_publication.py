#!/usr/bin/env python3
"""Real-file shell publication fault injection; no flash, USB or display."""
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import io
import tarfile
import struct
import shutil
from boot_bundle import validate as validate_host, asset_python
from build_delta import build as build_delta
from test_storage_sync import native_tool
import unittest

SDK = Path(__file__).resolve().parents[2]
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
TREE = 'usr/local/lib/un260-boot/versions'
ACTIVE = 'etc/un260/boot-active'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']


def digest(data):
    return hashlib.sha256(data).hexdigest()


def make_bundle(root, name, player=None):
    if player is None:
        player = b'#!/bin/sh\n# isolated check-only fixture, never a shipping ELF\nexit 0\n'
    theme = b'UNBTPK1\0' + bytes(20) + struct.pack('<I', 1) + ('fixture-theme-' + name).encode()
    ph, th = digest(player), digest(theme)
    identity = digest(('player_sha256=' + ph + '\ntheme_sha256=' + th + '\nmode=animation\n').encode())
    manifest = ('schema=un260-boot-bundle-v1\nid=' + identity + '\nprotocol=1\nplayer_sha256=' + ph +
                '\nplayer_bytes=' + str(len(player)) + '\ntheme_sha256=' + th + '\ntheme_bytes=' +
                str(len(theme)) + '\nmode=animation\n').encode()
    directory = root / TREE / identity
    directory.mkdir(parents=True)
    for filename, data, mode in [('player', player, 0o755), ('theme.pack', theme, 0o644), ('manifest', manifest, 0o644)]:
        path = directory / filename
        path.write_bytes(data)
        path.chmod(mode)
    return identity, digest(manifest)


def active_data(current, previous=('none', 'none')):
    return ('\n'.join(('UN260_BOOT_ACTIVE_V1', *current, *previous)) + '\n').encode()


class PublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-boot-publish-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.sync = native_tool()
        self.root = self.base / 'root'
        self.usb = self.base / 'usb'
        self.stage = self.usb / 'update/.un260_stage'
        self.payload = self.stage / 'payload'
        self.payload.mkdir(parents=True)
        (self.root / 'etc/un260').mkdir(parents=True)
        (self.root / 'usr/local/bin').mkdir(parents=True)
        (self.root / 'usr/local/bin/test_lvgl').write_text('old-app')
        (self.root / 'usr/local/bin/test_lvgl').chmod(0o755)
        (self.payload / 'usr/local/bin').mkdir(parents=True)
        (self.payload / 'usr/local/bin/test_lvgl').write_text('new-app')
        (self.payload / 'usr/local/bin/test_lvgl').chmod(0o755)
        self.old = make_bundle(self.root, 'old')
        self.new = make_bundle(self.payload, 'new')
        (self.root / ACTIVE).write_bytes(active_data(self.old))
        (self.root / ACTIVE).chmod(0o644)
        self.plan = 'file|0755|usr/local/bin/test_lvgl\n'
        for name, mode in [('player', '0755'), ('theme.pack', '0644'), ('manifest', '0644')]:
            self.plan += 'file|' + mode + '|' + TREE + '/' + self.new[0] + '/' + name + '\n'
        (self.stage / 'install.tsv').write_text(self.plan)
        (self.stage / 'manifest.ini').write_text('boot_bundle_id=' + self.new[0] + '\nboot_manifest_sha256=' + self.new[1] + '\n')

    @property
    def journal(self):
        return self.root / 'var/lib/un260-updater/transaction'

    def run_shell(self, body, expected=0, source=UPDATER):
        q = shlex.quote
        script = f'''set -eu
UN260_UPDATER_LIBRARY_ONLY=1
. {q(str(source))}
SYNC_TOOL={q(str(self.sync))}
ROOT_PREFIX={q(str(self.root))}
USB_MNT={q(str(self.usb))}
UPDATE_DIR=$USB_MNT/update
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
STATUS_FILE={q(str(self.base / 'status'))}
STATUS_TEMP={q(str(self.base / 'status.tmp'))}
LOG={q(str(self.base / 'log'))}
RESULT_FILE={q(str(self.base / 'result'))}
WORK_OWNED=0
package_version=boot-publication-test
sync() {{ :; }}
df() {{ printf 'Filesystem 1K-blocks Used Available Use%% Mounted\\nfixture 200000 1000 100000 1%% /\\n'; }}
: >> "$LOG"
{body}
'''
        result = subprocess.run(SHELL, input=script, text=True, capture_output=True, timeout=30)
        log = (self.base / 'log').read_text()
        self.assertEqual(result.returncode, expected, result.stderr + result.stdout + log)
        return result

    def prepare_archive(self, guard='syncfs-boot-bundle-v1', declared=True, platform=True, kind='ui'):
        if platform:
            (self.root / 'dev').mkdir(exist_ok=True)
            (self.root / 'dev/.un260-boot-platform').write_bytes(b'UN260_BOOT_PLATFORM_V1\n')
        # A fingerprint fixture isolates boot compatibility from storage I/O.
        sync = self.root / 'usr/local/bin/un260_storage_sync'
        sync.write_text('#!/bin/sh\nprintf "0123456789abcdef\\n"\n')
        sync.chmod(0o755)
        for rel, data in [('usr/bin/ui_update.sh', UPDATER.read_bytes()), ('etc/init.d/S00lvgl', b'#!/bin/sh\n# UN260_BOOT_BUNDLE_1\n')]:
            path = self.payload / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(0o755)
            if rel not in self.plan:
                self.plan += 'file|0755|' + rel + '\n'
        files = {str(path.relative_to(self.payload)): (path.read_bytes(), path.stat().st_mode & 0o777)
                 for path in self.payload.rglob('*') if path.is_file()}
        checks = ''.join(digest(data) + '  payload/' + rel + '\n' for rel, (data, mode) in sorted(files.items())).encode()
        meta = ('format=UN260_UPGRADE\nschema=1\nproduct=UN260\npackage_type=' + kind + '\nversion=boot-guard-test\npackage_id=' + digest(checks) +
                '\nstorage_guard=' + guard + '\n')
        if declared:
            meta += 'boot_bundle_id=' + self.new[0] + '\nboot_manifest_sha256=' + self.new[1] + '\n'
        entries = {'manifest.ini': (meta.encode(), 0o644), 'checksums.sha256': (checks, 0o644), 'install.tsv': (self.plan.encode(), 0o644)}
        entries.update({'payload/' + rel: value for rel, value in files.items()})
        package = self.usb / 'update/UN260_UPDATE.upk'
        with tarfile.open(package, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
            directory = tarfile.TarInfo('payload'); directory.type = tarfile.DIRTYPE; directory.mode = 0o755
            archive.addfile(directory)
            for rel, (data, mode) in entries.items():
                item = tarfile.TarInfo(rel); item.mode = mode; item.size = len(data)
                archive.addfile(item, io.BytesIO(data))
        return package

    def test_new_pid1_with_missing_active_still_rejects_legacy_package(self):
        self.prepare_archive(guard='syncfs-v1', declared=False)
        (self.root / ACTIVE).unlink()
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())

    def test_old_production_updater_rejects_new_guard_before_copy(self):
        self.prepare_archive()
        backup = SDK / 'work/boot_bundle_20261009/update_backup/updater-before.tar'
        with tarfile.open(backup) as archive:
            source = archive.extractfile('target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh').read()
        old_script = self.base / 'old-updater.sh'; old_script.write_bytes(source)
        # Load the original script before redirecting every owned path.
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; GUARDED=1; usb_barrier() { :; }; install_bundle', 1, source=old_script)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertIn('Package predates USB write protection', (self.base/'log').read_text())

    def test_new_bundle_requires_pid1_platform_before_write(self):
        self.prepare_archive(platform=False)
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())

    def test_new_bundle_with_matching_platform_passes(self):
        self.prepare_archive()
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle')
        self.assert_active(self.new, self.old)

    def test_legacy_guard_on_new_boot_device_rejected(self):
        self.prepare_archive(guard='syncfs-v1', declared=False)
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')

    def test_new_guard_without_bundle_on_ui_rejected_in_verify_only(self):
        self.prepare_archive(declared=False)
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; VERIFY_ONLY=1; install_bundle', 1)
        self.assert_active(self.old)
        self.assertFalse(self.journal.exists())

    def test_missing_theme_rejected_in_verify_only(self):
        (self.payload / TREE / self.new[0] / 'theme.pack').unlink()
        self.prepare_archive()
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; VERIFY_ONLY=1; install_bundle', 1)
        self.assert_active(self.old)
        self.assertFalse(self.journal.exists())

    def test_forged_maintenance_with_application_rejected(self):
        self.prepare_archive(declared=False, kind='ui-maintenance')
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')

    def test_invalid_or_symlink_platform_rejected(self):
        self.prepare_archive()
        marker = self.root / 'dev/.un260-boot-platform'
        marker.write_text('UN260_BOOT_PLATFORM_V1\nextra')
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.prepare_archive()
        marker.rename(self.base / 'platform')
        marker.symlink_to(self.base / 'platform')
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle', 1)
        self.assert_active(self.old)

    def test_host_validation_checks_pack_mode_matches_manifest(self):
        validate_host(self.payload, *self.new, require_elf=False)
        path = self.payload / TREE / self.new[0] / 'theme.pack'
        data = bytearray(path.read_bytes()); struct.pack_into('<I', data, 28, 2); path.write_bytes(data)
        # Keep manifest hashes/identity correct: mode mismatch itself is rejected.
        directory = path.parent
        raw = (directory / 'manifest').read_text().replace('theme_sha256=' + digest(bytes(data[:28]) + struct.pack('<I', 1) + bytes(data[32:])), 'theme_sha256=' + digest(data))
        values = dict(line.split('=', 1) for line in raw.splitlines())
        identity = digest(('player_sha256=' + values['player_sha256'] + '\ntheme_sha256=' + values['theme_sha256'] + '\nmode=animation\n').encode())
        raw = raw.replace('id=' + self.new[0], 'id=' + identity)
        (directory / 'manifest').write_text(raw)
        directory.rename(directory.parent / identity)
        with self.assertRaisesRegex(ValueError, 'mode/header'):
            validate_host(self.payload, identity, digest(raw.encode()), require_elf=False)

    def test_theme_delta_retains_old_version_and_repeat_publishes_once(self):
        self.prepare_archive()
        newfull = self.usb / 'update/UN260_UPDATE.upk'
        # Build the baseline with only old bundle, same matching scripts.
        saved_payload = self.base / 'saved-new-payload'
        self.payload.rename(saved_payload)
        self.payload.mkdir()
        shutil.copytree(self.root / TREE / self.old[0], self.payload / TREE / self.old[0])
        for rel in ('usr/local/bin/test_lvgl', 'usr/bin/ui_update.sh', 'etc/init.d/S00lvgl'):
            path = self.payload / rel; path.parent.mkdir(parents=True, exist_ok=True)
            if rel == 'usr/local/bin/test_lvgl': path.write_text('old-app'); path.chmod(0o755)
            else: shutil.copy2(saved_payload / rel, path)
        actual_new, actual_plan = self.new, self.plan
        self.new = self.old
        self.plan = actual_plan.replace(actual_new[0], self.old[0])
        shutil.copy2(newfull, self.base / 'newfull.upk')
        baseline = self.prepare_archive()
        shutil.copy2(baseline, self.base / 'base.upk')
        self.new, self.plan = actual_new, actual_plan
        shutil.rmtree(self.payload); saved_payload.rename(self.payload)
        # Full package replacement in fixture is irrelevant: use generated delta.
        build_delta(self.base / 'base.upk', self.base / 'newfull.upk', newfull)
        for rel in ('usr/bin/ui_update.sh', 'etc/init.d/S00lvgl'):
            path = self.root / rel; path.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(self.payload / rel, path)
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle')
        self.assert_active(self.new, self.old)
        self.assertTrue((self.root / TREE / self.old[0] / 'player').is_file())
        self.run_shell('BUNDLE_PATH=$UPDATE_DIR/UN260_UPDATE.upk; install_bundle')
        self.assert_active(self.new, self.old)

    def assert_active(self, current, previous=('none', 'none')):
        self.assertEqual((self.root / ACTIVE).read_bytes(), active_data(current, previous))

    def test_empty_retired_candidate_after_rollback_does_not_block_other_update(self):
        self.run_shell('''local_barrier() { if [ -f "$JOURNAL/boot-publish" ] && [ ! -f "$JOURNAL/committed" ]; then exit 88; fi; }
execute_plan''', 88)
        abandoned = self.new
        self.run_shell('recover_transaction')
        self.assertEqual(list((self.root / TREE / abandoned[0]).iterdir()), [])
        self.new = make_bundle(self.payload, 'next-version')
        self.plan = self.plan.replace(abandoned[0], self.new[0])
        (self.stage/'install.tsv').write_text(self.plan)
        (self.stage/'manifest.ini').write_text('boot_bundle_id=' + self.new[0] + '\nboot_manifest_sha256=' + self.new[1] + '\n')
        self.run_shell('execute_plan')
        self.assert_active(self.new, self.old)
        self.assertFalse((self.root / TREE / abandoned[0]).exists())
        self.assertFalse(self.journal.exists())

    def test_success_retains_old_self_contained_bundle(self):
        self.run_shell('execute_plan')
        self.assert_active(self.new, self.old)
        self.assertEqual((self.root / (ACTIVE + '.previous')).read_bytes(), active_data(self.old))
        self.assertTrue((self.root / TREE / self.old[0] / 'player').is_file())
        self.assertFalse(self.journal.exists())
        self.assertIn('success=1', (self.base / 'status').read_text())

    def test_before_commit_keeps_old_active_and_recovers_files(self):
        self.run_shell('''local_barrier() { if [ -f "$JOURNAL/boot-publish" ] && [ ! -f "$JOURNAL/committed" ]; then exit 88; fi; }
execute_plan''', 88)
        self.assert_active(self.old)
        self.assertTrue((self.journal / 'boot-publish').is_file())
        self.run_shell('recover_transaction')
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())
        # Empty candidate directories after rollback must not block a retry.
        self.run_shell('execute_plan')
        self.assert_active(self.new, self.old)

    def test_after_commit_before_active_resumes_without_usb(self):
        self.run_shell('boot_publish_committed() { exit 88; }; execute_plan', 88)
        self.assert_active(self.old)
        self.assertTrue((self.journal / 'committed').is_file())
        self.run_shell('USB_MNT=/missing-usb; recover_transaction')
        self.assert_active(self.new, self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'new-app')
        self.assertFalse(self.journal.exists())

    def test_after_active_rename_is_idempotent(self):
        self.run_shell('''mv() { command mv "$@" || return 1; case "$*" in *boot-active.tmp*) exit 88 ;; esac; }
execute_plan''', 88)
        self.assert_active(self.new, self.old)
        self.assertTrue((self.journal / 'committed').exists())
        self.run_shell('recover_transaction; recover_transaction')
        self.assert_active(self.new, self.old)

    def test_publish_failure_preserves_committed_journal_and_reports_fail(self):
        self.run_shell('''mv() { case "$*" in *boot-active.tmp*) return 1 ;; esac; command mv "$@"; }
execute_plan''', 1)
        self.assert_active(self.old)
        self.assertTrue((self.journal / 'committed').exists())
        self.assertIn('success=0', (self.base / 'status').read_text())
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'new-app')
        self.run_shell('recover_transaction')
        self.assert_active(self.new, self.old)

    def test_incomplete_theme_rejected_before_application_write(self):
        (self.payload / TREE / self.new[0] / 'theme.pack').write_bytes(b'broken')
        self.run_shell('execute_plan', 1)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())

    def test_invalid_active_preserves_application(self):
        (self.root / ACTIVE).write_text('invalid\n')
        self.run_shell('execute_plan', 1)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())

    def test_active_cannot_be_ordinary_payload(self):
        (self.payload / 'etc/un260').mkdir(parents=True)
        (self.payload / ACTIVE).write_bytes(active_data(self.new))
        (self.stage / 'install.tsv').write_text('file|0644|' + ACTIVE + '\n')
        self.run_shell('execute_plan', 1)
        self.assert_active(self.old)

    def test_symlink_current_version_is_rejected(self):
        directory = self.root / TREE / self.old[0]
        directory.rename(self.base / 'outside-bundle')
        directory.symlink_to(self.base / 'outside-bundle', target_is_directory=True)
        self.run_shell('execute_plan', 1)
        self.assert_active(self.old)
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')

    def test_unknown_or_duplicate_manifest_key_is_rejected(self):
        path = self.payload / TREE / self.new[0] / 'manifest'
        path.write_bytes(path.read_bytes() + b'mode=logo\n')
        (self.stage / 'manifest.ini').write_text('boot_bundle_id=' + self.new[0] + '\nboot_manifest_sha256=' + digest(path.read_bytes()) + '\n')
        self.run_shell('execute_plan', 1)
        self.assert_active(self.old)

    def test_repeat_noop_does_not_republish_or_lose_previous(self):
        self.run_shell('execute_plan')
        self.run_shell('''mv() { case "$*" in *boot-active.tmp*) return 1 ;; esac; command mv "$@"; }
execute_plan''')
        self.assert_active(self.new, self.old)
        self.assertFalse(self.journal.exists())

    def test_uncommitted_corrupt_pending_cannot_select_new_theme(self):
        self.journal.mkdir(parents=True)
        (self.journal / 'state').write_text('')
        (self.journal / 'boot-publish').write_text('broken\n')
        self.run_shell('recover_transaction')
        self.assert_active(self.old)
        self.assertFalse(self.journal.exists())

    def test_committed_corrupt_pending_is_retained_for_service(self):
        self.journal.mkdir(parents=True)
        (self.journal / 'state').write_text('')
        (self.journal / 'boot-publish').write_text('broken\n')
        (self.journal / 'committed').write_text('')
        self.run_shell('recover_transaction', 1)
        self.assert_active(self.old)
        self.assertTrue(self.journal.exists())

    def test_package_cannot_replace_a_different_existing_version(self):
        rel = TREE + '/' + self.old[0] + '/theme.pack'
        path = self.payload / rel
        path.parent.mkdir(parents=True)
        path.write_bytes(b'overwrite-previous')
        with (self.stage / 'install.tsv').open('a') as stream:
            stream.write('file|0644|' + rel + '\n')
        self.run_shell('execute_plan', 1)
        self.assert_active(self.old)


    def test_previous_backup_rename_before_active_is_recoverable(self):
        self.run_shell('''mv() { command mv "$@" || return 1; case "$*" in *boot-active.previous.tmp*) exit 88 ;; esac; }
execute_plan''', 88)
        self.assert_active(self.old)
        self.assertEqual((self.root / (ACTIVE + '.previous')).read_bytes(), active_data(self.old))
        self.assertTrue((self.journal / 'committed').exists())
        self.run_shell('recover_transaction')
        self.assert_active(self.new, self.old)

    def test_same_active_update_does_not_overwrite_previous_descriptor(self):
        self.run_shell('execute_plan')
        (self.payload / 'usr/local/bin/test_lvgl').write_text('newer-app-only')
        self.run_shell('execute_plan')
        self.assert_active(self.new, self.old)
        self.assertEqual((self.root / (ACTIVE + '.previous')).read_bytes(), active_data(self.old))

    def test_invalid_current_active_blocks_committed_publication(self):
        (self.root / (ACTIVE + '.previous')).write_bytes(active_data(self.old))
        self.run_shell('boot_publish_committed() { exit 88; }; execute_plan', 88)
        (self.root / ACTIVE).write_text('corrupted-active\n')
        self.run_shell('recover_transaction', 1)
        self.assertEqual((self.root / (ACTIVE + '.previous')).read_bytes(), active_data(self.old))
        self.assertTrue((self.journal / 'committed').exists())

    def test_previous_descriptor_symlink_is_not_followed(self):
        outside = self.base / 'untouched'
        outside.write_text('untouched')
        (self.root / (ACTIVE + '.previous')).symlink_to(outside)
        self.run_shell('execute_plan', 1)
        self.assertEqual(outside.read_text(), 'untouched')
        self.assert_active(self.old)
        self.assertTrue(self.journal.exists())

    def test_corrupt_pending_previous_identity_cannot_publish(self):
        self.run_shell('boot_publish_committed() { exit 88; }; execute_plan', 88)
        lines = (self.journal / 'boot-publish').read_text().splitlines()
        lines[3] = 'a' * 64
        lines[4] = 'b' * 64
        (self.journal / 'boot-publish').write_text('\n'.join(lines) + '\n')
        self.run_shell('recover_transaction', 1)
        self.assert_active(self.old)
        self.assertTrue(self.journal.exists())


    def test_collects_only_retired_complete_version(self):
        retired = make_bundle(self.root, 'retired')
        self.run_shell('execute_plan')
        self.assert_active(self.new, self.old)
        self.assertFalse((self.root / TREE / retired[0]).exists())
        self.assertTrue((self.root / TREE / self.old[0]).is_dir())

    def test_live_snapshot_is_kept_even_if_older_than_previous(self):
        live = make_bundle(self.root, 'live-snapshot')
        (self.root / 'dev').mkdir()
        (self.root / 'dev/.un260-boot-bundle').write_bytes(active_data(live))
        self.run_shell('execute_plan')
        self.assertTrue((self.root / TREE / live[0]).is_dir())
        self.assert_active(self.new, self.old)

    def test_retired_partial_unlink_resumes_from_durable_plan(self):
        retired = make_bundle(self.root, 'retired-cut')
        body = f"""rm() {{ command rm "$@" || return 1; case "$*" in *{TREE}/{retired[0]}/manifest*) exit 88 ;; esac; }}
execute_plan"""
        self.run_shell(body, 88)
        self.assert_active(self.new, self.old)
        self.assertTrue((self.journal / 'boot-gc').exists())
        self.run_shell('recover_transaction')
        self.assertFalse((self.root / TREE / retired[0]).exists())
        self.assertFalse(self.journal.exists())

    def test_unknown_garbage_entry_is_preserved_with_journal(self):
        retired = make_bundle(self.root, 'retired-extra')
        (self.root / TREE / retired[0] / 'foreign-data').write_text('preserve')
        self.run_shell('execute_plan', 1)
        self.assert_active(self.new, self.old)
        self.assertEqual((self.root / TREE / retired[0] / 'foreign-data').read_text(), 'preserve')
        self.assertTrue(self.journal.exists())



    def _check_transport(self, prelude='', expected=0, installed=False):
        marker = self.base / 'transport-private-path'
        marker.unlink(missing_ok=True)
        q = shlex.quote
        tree = self.root if installed else self.payload
        identity, manifest_hash = self.old if installed else self.new
        scope = '' if installed else ' transport'
        track = '''mktemp() {
    transport_created=$(command mktemp "$@") || return 1
    printf '%s\n' "$transport_created" > MARKER
    printf '%s\n' "$transport_created"
}
'''.replace('MARKER', q(str(marker)))
        body = track + prelude + '\nboot_validate_bundle ' + q(str(tree)) + ' ' + identity + ' ' + manifest_hash + scope
        result = self.run_shell(body, expected)
        if marker.exists():
            private = Path(marker.read_text().strip())
            self.assertEqual(private.parent, Path('/tmp'))
            self.assertTrue(private.name.startswith('un260-boot-check.'))
            self.assertFalse(private.exists(), 'private check directory leaked')
        self.assertEqual((self.root / ACTIVE).read_bytes(), active_data(self.old))
        self.assertEqual((self.root / 'usr/local/bin/test_lvgl').read_text(), 'old-app')
        self.assertFalse(self.journal.exists())
        return result, marker

    def test_native_extractor_0600_bundle_validates_via_private_copy(self):
        package = self.prepare_archive()
        unpacker = self.base / 'host-un260-unpack'
        subprocess.run(['cc', '-O2', '-Wall', '-Wextra', str(SDK / 'source/artinchip/test-lvgl/tools/un260_unpack.c'), '-lz', '-o', str(unpacker)], check=True, capture_output=True)
        shutil.rmtree(self.stage)
        self.stage.mkdir()
        subprocess.run([str(unpacker), str(package), str(self.stage)], check=True, capture_output=True)
        bundle = self.payload / TREE / self.new[0]
        before = {name: digest((bundle / name).read_bytes()) for name in ('player', 'theme.pack', 'manifest')}
        self.assertEqual({(bundle / name).stat().st_mode & 0o777 for name in before}, {0o600})
        self._check_transport()
        self.assertEqual(before, {name: digest((bundle / name).read_bytes()) for name in before})
        self.assertEqual({(bundle / name).stat().st_mode & 0o777 for name in before}, {0o600})

    def test_transport_uniform_modes_do_not_become_install_permissions(self):
        bundle = self.payload / TREE / self.new[0]
        for mode in (0o600, 0o644, 0o755, 0o777):
            with self.subTest(mode=oct(mode)):
                for name in ('player', 'theme.pack', 'manifest'):
                    (bundle / name).chmod(mode)
                self._check_transport()
                self.assertEqual({(bundle / name).stat().st_mode & 0o777 for name in ('player', 'theme.pack', 'manifest')}, {mode})

    def test_installed_modes_remain_strict(self):
        bundle = self.root / TREE / self.old[0]
        for name, wrong in (('player', 0o600), ('theme.pack', 0o755), ('manifest', 0o755)):
            with self.subTest(name=name):
                original = (bundle / name).stat().st_mode & 0o777
                (bundle / name).chmod(wrong)
                self._check_transport(expected=1, installed=True)
                (bundle / name).chmod(original)

    def test_transport_requires_unique_explicit_install_modes(self):
        original = self.plan
        base = TREE + '/' + self.new[0]
        variants = (
            original.replace('file|0755|' + base + '/player', 'file|0644|' + base + '/player'),
            original.replace('file|0644|' + base + '/theme.pack', 'file|0755|' + base + '/theme.pack'),
            original.replace('file|0644|' + base + '/manifest\n', ''),
            original + 'file|0755|' + base + '/player\n',
            original.replace('file|0755|' + base + '/player', 'delete|0755|' + base + '/player'),
        )
        for index, plan in enumerate(variants):
            with self.subTest(index=index):
                (self.stage / 'install.tsv').write_text(plan)
                _, marker = self._check_transport(expected=1)
                self.assertFalse(marker.exists(), 'invalid contract created verification copy')
        (self.stage / 'install.tsv').write_text(original)

    def test_transport_bad_hash_rejected_before_copy(self):
        path = self.payload / TREE / self.new[0] / 'theme.pack'
        path.write_bytes(path.read_bytes() + b'corrupt')
        _, marker = self._check_transport(expected=1)
        self.assertFalse(marker.exists())

    def test_transport_symlink_rejected_before_copy(self):
        path = self.payload / TREE / self.new[0] / 'theme.pack'
        saved = self.base / 'transport-theme-real'
        path.rename(saved)
        path.symlink_to(saved)
        _, marker = self._check_transport(expected=1)
        self.assertFalse(marker.exists())

    def test_transport_private_copy_failure_cleans_up(self):
        self._check_transport(prelude='dd() { return 1; }', expected=1)

    def test_transport_private_copy_corruption_is_rechecked(self):
        body = '''dd() {
    transport_output=""
    for transport_arg in "$@"; do
        case "$transport_arg" in of=*) transport_output=${transport_arg#of=} ;; esac
    done
    command dd "$@" || return 1
    case "$transport_output" in */theme.pack) printf corrupt >> "$transport_output" ;; esac
}
'''
        self._check_transport(prelude=body, expected=1)

    def test_transport_private_chmod_failure_cleans_up(self):
        body = '''chmod() {
    case "$*" in *'/player'*) return 1 ;; esac
    command chmod "$@"
}
'''
        self._check_transport(prelude=body, expected=1)

    def test_transport_mktemp_failure_does_not_modify_installed_bundle(self):
        _, marker = self._check_transport(prelude='mktemp() { return 1; }', expected=1)
        self.assertFalse(marker.exists())

    def test_transport_cleanup_failure_prevents_success(self):
        body = '''rm() {
    case "$*" in *'/tmp/un260-boot-check.'*) command rm "$@" || return 1; return 1 ;; esac
    command rm "$@"
}
'''
        self._check_transport(prelude=body, expected=1)

    def test_transport_signals_clean_private_copy(self):
        for signal in ('HUP', 'INT', 'TERM'):
            with self.subTest(signal=signal):
                body = 'dd() { /bin/sh -c \'kill -' + signal + ' "$PPID"\'; return 1; }'
                self._check_transport(prelude=body, expected=1)

    def test_transport_copy_remains_physically_bounded_when_source_grows(self):
        count = self.base / 'transport-copy-byte-count'
        body = '''dd() {
    transport_input=""
    transport_output=""
    for transport_arg in "$@"; do
        case "$transport_arg" in if=*) transport_input=${transport_arg#if=} ;; of=*) transport_output=${transport_arg#of=} ;; esac
    done
    case "$transport_input" in */player) command dd if=/dev/zero bs=1048576 count=6 >> "$transport_input" 2>/dev/null || return 1 ;; esac
    command dd "$@" || return 1
    case "$transport_output" in */player) wc -c < "$transport_output" > COUNT ;; esac
}
'''.replace('COUNT', shlex.quote(str(count)))
        self._check_transport(prelude=body, expected=1)
        self.assertTrue(count.is_file())
        self.assertEqual(int(count.read_text().strip()), 2097152)

    def test_transport_player_check_failure_cleans_up(self):
        previous = self.new[0]
        shutil.rmtree(self.payload / TREE / previous)
        self.new = make_bundle(self.payload, 'check-failed', player=b'#!/bin/sh\nexit 23\n')
        self.plan = self.plan.replace(previous, self.new[0])
        (self.stage / 'install.tsv').write_text(self.plan)
        self._check_transport(expected=23)


class AssetPythonTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-asset-python-')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.target = self.base / 'target'
        self.target.mkdir()
        self.cache = self.base / 'build/test-lvgl/CMakeCache.txt'
        self.cache.parent.mkdir(parents=True)
        self.cache.write_text('UN260_ASSET_PYTHON:FILEPATH=/usr/bin/python3\n')

    def test_cached_asset_python_ignores_sdk_path_shadow(self):
        shadow = self.base / 'sdk/bin'; shadow.mkdir(parents=True)
        marker = self.base / 'wrong-interpreter-used'
        wrong = shadow / 'python3'
        wrong.write_text('#!/bin/sh\nprintf wrong > ' + shlex.quote(str(marker)) + '\nexit 92\n')
        wrong.chmod(0o755)
        previous = os.environ.get('PATH', '')
        try:
            os.environ['PATH'] = str(shadow) + ':' + previous
            selected = asset_python(self.target)
            self.assertEqual(selected, '/usr/bin/python3')
            subprocess.run([selected, str(SDK/'source/artinchip/test-lvgl/tools/build_boot_bundle.py'), '--validate-config'], check=True)
        finally:
            os.environ['PATH'] = previous
        self.assertFalse(marker.exists())

    def test_missing_cache_uses_same_fixed_search_as_cmake(self):
        self.cache.unlink()
        self.assertEqual(asset_python(self.target), '/usr/bin/python3')

    def test_invalid_relative_or_duplicate_cache_is_rejected(self):
        for value in ('python3', '/usr/bin/../bin/python3', '/missing/python3'):
            self.cache.write_text('UN260_ASSET_PYTHON:FILEPATH=' + value + '\n')
            with self.assertRaises(ValueError):
                asset_python(self.target)
        self.cache.write_text('UN260_ASSET_PYTHON:FILEPATH=/usr/bin/python3\n' * 2)
        with self.assertRaises(ValueError):
            asset_python(self.target)

    def test_symlink_cache_is_rejected(self):
        self.cache.rename(self.base / 'outside-cache')
        self.cache.symlink_to(self.base / 'outside-cache')
        with self.assertRaises(ValueError):
            asset_python(self.target)

    def test_configured_sdk_python_without_pillow_is_not_silently_replaced(self):
        interpreter = SDK / 'output/d211_d213_devkitf/host/bin/python3'
        if not interpreter.is_file():
            self.skipTest('SDK host Python absent')
        probe = subprocess.run([str(interpreter), '-c', 'from PIL import Image'], capture_output=True)
        if probe.returncode == 0:
            self.skipTest('SDK host Python has Pillow')
        self.cache.write_text('UN260_ASSET_PYTHON:FILEPATH=' + str(interpreter) + '\n')
        with self.assertRaises(subprocess.CalledProcessError):
            asset_python(self.target)


if __name__ == '__main__':
    unittest.main()
