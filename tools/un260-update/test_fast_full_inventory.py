#!/usr/bin/env python3
"""Production shell + real inventory helper; host fixtures do not prove flash durability."""
import functools
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

import test_full_inventory as baseline

SDK = baseline.SDK
_build = tempfile.TemporaryDirectory(prefix='un260-fast-inventory-native.')


@functools.lru_cache()
def native_tool():
    supplied = os.environ.get('UN260_TEST_SYNC_BINARY')
    if supplied:
        binary = Path(supplied)
        if not binary.is_file():
            raise RuntimeError('Missing explicit inventory helper: ' + supplied)
        return binary
    binary = Path(_build.name) / 'un260_storage_sync'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2',
                    str(SDK / 'source/artinchip/test-lvgl/tools/un260_storage_sync.c'),
                    '-o', str(binary)], check=True)
    return binary


class FastInventoryTests(unittest.TestCase):
    def setUp(self):
        self.fx = baseline.InventoryTests()
        self.fx.setUp()
        self.calls = self.fx.base / 'native-calls'
        self.wrapper = self.fx.base / 'inventory-sync'
        self.helper('new')

    def tearDown(self):
        self.fx.tearDown()

    def helper(self, mode, failure=''):
        binary = native_tool()
        body = [
            '#!/bin/sh',
            'printf "%s\\n" "$1" >> ' + shlex.quote(str(self.calls)),
        ]
        if mode == 'old':
            body.append('case "$1" in --probe-inventory) exit 2 ;; esac')
        elif mode == 'wrong-probe':
            body.append('case "$1" in --probe-inventory) printf "OTHER_INVENTORY_1\\n"; exit 0 ;; esac')
        elif mode == 'fail':
            body.append('case "$1" in ' + failure + ') exit 23 ;; esac')
        elif mode == 'partial-inspect':
            body.extend([
                'case "$1" in --inspect-inventory)',
                '  ' + shlex.quote(str(binary)) + ' "$@" > ' + shlex.quote(str(self.fx.base / 'actual-inspection')) + ' || exit $?',
                '  IFS= read -r row < ' + shlex.quote(str(self.fx.base / 'actual-inspection')) + ' || exit 24',
                '  printf "%s\\n" "$row"; exit 0 ;; esac',
            ])
        body.append('exec ' + shlex.quote(str(binary)) + ' "$@"')
        self.wrapper.write_text('\n'.join(body) + '\n')
        self.wrapper.chmod(0o755)
        self.calls.write_text('')

    def call(self, action='', *, before='', full=True, planning=True):
        setup = 'SYNC_TOOL=' + shlex.quote(str(self.wrapper)) + '\n'
        if full:
            setup += ('FULL_TARGET_ID=' + self.fx.target_id + '\n'
                      'trial_targets_count=' + str(len(self.fx.files)) + '\n'
                      'trial_app_sha=' + baseline.digest(self.fx.files[baseline.APP][0]) + '\n')
        return self.fx.call(action, before=setup + before, full=full, planning=planning)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def native_calls(self):
        return self.calls.read_text().splitlines()

    def install_plan(self):
        # Actual journal/backup/inode installation, with physical barriers
        # replaced deliberately: directory tests cannot model NAND durability.
        return '''
BACKUP_DIR=$3/backup
JOURNAL=$3/journal
mkdir -p "$BACKUP_DIR" "$JOURNAL" || exit 31
: > "$JOURNAL/state"
local_barrier() { return 0; }
usb_barrier() { return 0; }
while IFS='|' read -r kind mode rel hash oldhash bytes; do
  install_file_entry "$mode" "$rel" "$hash" "$oldhash" "$kind"
done < "$PLAN_FILE"
'''

    def plan_rows(self):
        return [row.split('|') for row in (self.fx.base / 'plan').read_text().splitlines()]

    def install_all_expected(self):
        for path, (data, mode) in self.fx.files.items():
            self.fx.put(self.fx.root / path, data, mode)
        (self.fx.root / baseline.STATIC / 'retired.png').unlink(missing_ok=True)

    def test_real_native_mixed_plan_full_verify_and_retired_membership(self):
        result = self.call(self.install_plan() + 'verify_installed_targets\n')
        self.assert_ok(result)
        self.assertEqual(len(self.plan_rows()), 4)
        for command in ['--verify-payload', '--inspect-inventory', '--verify-inventory']:
            self.assertIn(command, self.native_calls())
        for path, (data, mode) in self.fx.files.items():
            installed = self.fx.root / path
            self.assertEqual(installed.read_bytes(), data)
            self.assertEqual(installed.stat().st_mode & 0o7777, int(mode, 8))
        self.assertFalse((self.fx.root / baseline.STATIC / 'retired.png').exists())
        self.assertEqual(len((self.fx.base / 'journal/state').read_text().splitlines()), 4)

    def test_only_app_changed_large_full_package_hashes_every_target_in_batch(self):
        self.install_all_expected()
        for n in range(300):
            path = baseline.STATIC + '/stable-%03d.png' % n
            self.fx.files[path] = (b'asset' + bytes([n % 256]), '0644')
            self.fx.put(self.fx.payload / path, self.fx.files[path][0], '0600')
            self.fx.put(self.fx.root / path, self.fx.files[path][0], '0644')
        self.fx.refresh()
        self.fx.put(self.fx.root / baseline.APP, b'app-old', '0755')
        # Avoid per-file shell SHA/mode calls while still reading actual bytes
        # of every installed target through the native final batch.
        before = '''
checked_digest() {
  case "$1" in "$ROOT_PREFIX"/*) printf 'per-target shell hash forbidden\\n' >&2; return 90 ;; esac
  value=$(sha256sum "$1") || return 1
  printf '%s\\n' "${value%% *}"
}
file_permissions() { printf 'per-target shell mode forbidden\\n' >&2; return 91; }
'''
        result = self.call(self.install_plan() + 'verify_installed_targets\nprintf "%s %s\\n" "$INSTALL_ENTRY_COUNT" "$SKIPPED_COUNT"\n', before=before)
        self.assert_ok(result)
        self.assertEqual(result.stdout.splitlines()[-1], '1 304')
        self.assertEqual([row[2] for row in self.plan_rows()], [baseline.APP])
        commands = self.native_calls()
        for command in ['--inspect-inventory', '--verify-payload', '--verify-inventory']:
            self.assertEqual(commands.count(command), 1)

    def test_same_hash_wrong_mode_is_replaced_and_verified(self):
        self.install_all_expected()
        path = self.fx.root / 'usr/local/lib/liblvgl.so'
        path.chmod(0o644)
        self.assert_ok(self.call(self.install_plan() + 'verify_installed_targets\n'))
        self.assertEqual([row[2] for row in self.plan_rows()], ['usr/local/lib/liblvgl.so'])
        self.assertEqual(path.stat().st_mode & 0o7777, 0o755)

    def test_missing_file_is_planned(self):
        for relative in [baseline.APP, baseline.STATIC + '/a.png']:
            (self.fx.root / relative).unlink()
        self.assert_ok(self.call(self.install_plan() + 'verify_installed_targets\n'))
        planned = {row[2]: row for row in self.plan_rows()}
        self.assertEqual(planned[baseline.APP][4], '-')
        self.assertEqual(planned[baseline.STATIC + '/a.png'][4], '-')
        self.assertEqual((self.fx.root / baseline.APP).read_bytes(), b'app-new')

    def test_payload_transport_modes_are_ignored(self):
        for path in self.fx.payload.rglob('*'):
            if path.is_file():
                path.chmod(0o755)
        self.assert_ok(self.call(self.install_plan() + 'verify_installed_targets\n'))
        self.assertIn('--verify-payload', self.native_calls())

    def test_corrupt_payload_rejected_before_any_plan(self):
        (self.fx.payload / baseline.APP).write_bytes(b'bad-new')
        result = self.call(self.install_plan() + 'verify_installed_targets\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn('--inspect-inventory', self.native_calls())
        self.assertFalse((self.fx.base / 'journal').exists())

    def test_changed_source_after_payload_validation_cannot_be_committed(self):
        for content in ['bad-new', 'altered-too-long']:
            with self.subTest(content=content):
                (self.fx.payload / baseline.APP).write_bytes(b'app-new')
                before = '''
request_install_file() {
  FULL_PLAN_REQUEST_COUNT=$((FULL_PLAN_REQUEST_COUNT+1))
  printf '%s|%s\\n' "$2" "$1" >> "$PLAN_FILE.requests"
  if [ "$1" = usr/local/bin/test_lvgl ]; then printf ''' + shlex.quote(content) + ''' > "$STAGE_DIR/payload/$1"; fi
}
'''
                result = self.call(self.install_plan() + 'verify_installed_targets\n', before=before)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.fx.base / 'journal/committed').exists())
                for suffix in ['old', 'new']:
                    for path in self.fx.root.rglob('*.un260-' + suffix):
                        path.unlink()
                self.fx.put(self.fx.root / baseline.APP, b'old-app-new', '0755')

    def test_unchanged_target_mutated_during_install_is_still_rejected(self):
        for mutation in ['printf corrupt > "$ROOT_PREFIX/usr/local/lib/liblvgl.so"',
                         'chmod 0644 "$ROOT_PREFIX/usr/local/lib/liblvgl.so"',
                         'rm "$ROOT_PREFIX/usr/local/lib/liblvgl.so"']:
            with self.subTest(mutation=mutation):
                result = self.call(self.install_plan() + mutation + '\nverify_installed_targets\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('--verify-inventory', self.native_calls())
                self.install_all_expected()
                for suffix in ['old', 'new']:
                    for path in self.fx.root.rglob('*.un260-' + suffix):
                        path.unlink()

    def test_unsafe_destination_and_payload_links_do_not_publish(self):
        for path in [self.fx.payload / baseline.APP, self.fx.root / baseline.APP,
                     self.fx.root / 'usr/local/lib']:
            with self.subTest(path=path):
                saved = path.with_name(path.name + '.saved')
                path.rename(saved)
                path.symlink_to(saved)
                result = self.call(self.install_plan() + 'verify_installed_targets\n')
                self.assertNotEqual(result.returncode, 0)
                path.unlink()
                saved.rename(path)

    def test_app_volume_mapping_is_used_for_planning_and_final_verify(self):
        volume = self.fx.base / 'app-volume'
        self.fx.put(volume / 'test_lvgl', b'app-old', '0755')
        (self.fx.root / baseline.APP).unlink()
        before = 'APP_VOLUME=1\nAPP_VOLUME_DIR=' + shlex.quote(str(volume)) + '\n'
        result = self.call(self.install_plan() + 'verify_installed_targets\nprintf "%s\\n" "$APP_PEAK_REQUIRED_KB"\n', before=before)
        self.assert_ok(result)
        self.assertGreater(int(result.stdout.splitlines()[-1]), 0)
        self.assertEqual((volume / 'test_lvgl').read_bytes(), b'app-new')
        self.assertFalse((self.fx.root / baseline.APP).exists())

    def test_duplicate_missing_or_overlap_install_coverage_rejected(self):
        original = self.fx.manifest[:]
        for manifest in [original + [original[0]], original[:-1],
                         original + ['file|0644|' + baseline.STATIC + '/a.png']]:
            with self.subTest(manifest=manifest):
                self.fx.manifest = manifest
                self.fx.refresh()
                result = self.call(self.install_plan() + 'verify_installed_targets\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual((self.fx.base / 'plan').read_bytes(), b'')
                self.assertFalse((self.fx.base / 'journal').exists())

    def test_target_or_checksum_changed_after_validation_rejected(self):
        for filename in ['target.tsv', 'checksums.sha256']:
            with self.subTest(filename=filename):
                before = '''
request_install_file() {
  FULL_PLAN_REQUEST_COUNT=$((FULL_PLAN_REQUEST_COUNT+1))
  printf '%s|%s\\n' "$2" "$1" >> "$PLAN_FILE.requests"
  if [ "$1" = usr/local/bin/test_lvgl ]; then printf fake >> "$STAGE_DIR/''' + filename + '''"; fi
}
'''
                result = self.call(self.install_plan() + 'verify_installed_targets\n', before=before)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse((self.fx.base / 'journal').exists())
                self.fx.refresh()

    def test_capability_absent_or_wrong_probe_uses_compatible_old_path(self):
        for mode in ['old', 'wrong-probe']:
            with self.subTest(mode=mode):
                self.helper(mode)
                self.assert_ok(self.call(self.fx.install_fixture() + 'verify_installed_targets\n'))
                commands = self.native_calls()
                self.assertIn('--probe-inventory', commands)
                for operation in ['--verify-payload', '--inspect-inventory', '--verify-inventory']:
                    self.assertNotIn(operation, commands)

    def test_capable_helper_failure_never_downgrades_to_shell_verification(self):
        for operation in ['--verify-payload', '--inspect-inventory', '--verify-inventory']:
            with self.subTest(operation=operation):
                self.helper('fail', operation)
                result = self.call(self.fx.install_fixture() + 'verify_installed_targets\n')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(operation, self.native_calls())

    def test_partial_success_inspection_output_cannot_skip_targets(self):
        self.helper('partial-inspect')
        result = self.call(self.install_plan() + 'verify_installed_targets\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--inspect-inventory', self.native_calls())
        self.assertFalse((self.fx.base / 'journal').exists())

    def test_legacy_without_target_uses_existing_plan_and_verification(self):
        self.assert_ok(self.call(
            'while read -r hash source; do rel=${source#payload/}; cp "$STAGE_DIR/$source" "$ROOT_PREFIX/$rel"; '
            'case "$rel" in usr/local/bin/*|usr/local/lib/liblvgl.so) chmod 0755 "$ROOT_PREFIX/$rel" ;; '
            '*) chmod 0644 "$ROOT_PREFIX/$rel" ;; esac; done < "$STAGE_DIR/checksums.sha256"\n'
            'rm -f "$ROOT_PREFIX/' + baseline.STATIC + '/retired.png"\n'
            'verify_installed_targets\n', full=False))
        self.assertFalse((self.fx.base / 'plan.inventory').exists())
        for operation in ['--inspect-inventory', '--verify-inventory']:
            self.assertNotIn(operation, self.native_calls())

    def test_scan_field_mutation_during_consumption_is_rejected(self):
        for edit in ['s/0755/0644/', 's/|7|/|8|/', 's/' + baseline.digest(b'app-new') + '/' + '0' * 64 + '/', 's/usr.local.bin.test_lvgl/usr\\/local\\/bin\\/alien/']:
            with self.subTest(edit=edit):
                before = '''
scan_reads=0
read() {
  case "$*" in *inventory_old_hash*)
    scan_reads=$((scan_reads+1))
    if [ "$scan_reads" = 1 ]; then
      sed ''' + shlex.quote(edit) + ''' "$PLAN_FILE.scan" > "$PLAN_FILE.scan.changed" || return 2
      mv "$PLAN_FILE.scan.changed" "$PLAN_FILE.scan" || return 2
    fi ;;
  esac
  command read "$@"
}
'''
                self.assertNotEqual(self.call(self.install_plan() + 'verify_installed_targets\n', before=before).returncode, 0)
                self.assertFalse((self.fx.base / 'journal').exists())

    def test_interrupted_or_unterminated_scan_cannot_skip_targets(self):
        for stop in [1, 3, 5]:
            with self.subTest(stop=stop):
                before = '''
scan_reads=0
read() {
  case "$*" in *inventory_old_hash*)
    scan_reads=$((scan_reads+1))
    [ "$scan_reads" != ''' + str(stop) + ''' ] || return 1 ;;
  esac
  command read "$@"
}
'''
                self.assertNotEqual(self.call(self.install_plan() + 'verify_installed_targets\n', before=before).returncode, 0)
                self.assertFalse((self.fx.base / 'journal').exists())
        before = '''
scan_reads=0
read() {
  case "$*" in *inventory_old_hash*)
    scan_reads=$((scan_reads+1))
    if [ "$scan_reads" = 1 ]; then printf 'broken' >> "$PLAN_FILE.scan"; fi ;;
  esac
  command read "$@"
}
'''
        self.assertNotEqual(self.call(self.install_plan() + 'verify_installed_targets\n', before=before).returncode, 0)
        self.assertFalse((self.fx.base / 'journal').exists())

    def test_noop_still_checks_all_targets_and_unrecovered_artifacts(self):
        self.install_all_expected()
        self.assert_ok(self.call('verify_installed_targets\nprintf "%s\\n" "$INSTALL_ENTRY_COUNT"\n'))
        self.assertEqual((self.fx.base / 'plan').read_bytes(), b'')
        self.assertIn('--verify-inventory', self.native_calls())
        library = self.fx.root / 'usr/local/lib/liblvgl.so'
        os.link(library, library.with_name(library.name + '.un260-old'))
        result = self.call(self.install_plan() + 'verify_installed_targets\n')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Unrecovered install artifact', result.stderr)
        self.assertFalse((self.fx.base / 'journal').exists())

    def test_missing_safe_parent_is_recreated_for_a_complete_package(self):
        library = self.fx.root / 'usr/local/lib/liblvgl.so'
        library.unlink()
        library.parent.rmdir()
        self.assert_ok(self.call(self.install_plan() + 'verify_installed_targets\n'))
        planned = {row[2]: row for row in self.plan_rows()}
        self.assertEqual(planned['usr/local/lib/liblvgl.so'][4], '-')
        self.assertEqual(library.read_bytes(), b'lib-same')

    def test_mixed_old_large_target_can_upgrade_to_small_new_payload(self):
        # The new table qualifies only new bytes. Old installed bytes may be
        # larger; their actual size must still charge the rollback backup.
        library = self.fx.root / 'usr/local/lib/liblvgl.so'
        library.write_bytes(b'L' * (25 * 1024 * 1024))
        library.chmod(0o755)
        result = self.call(self.install_plan() +
                           'verify_installed_targets\nprintf "%s\\n" "$BACKUP_REQUIRED_KB"\n')
        self.assert_ok(result)
        self.assertGreater(int(result.stdout.splitlines()[-1]), 25 * 1024)
        self.assertEqual(library.read_bytes(), b'lib-same')
        backup = self.fx.base / 'backup/rootfs/usr/local/lib/liblvgl.so'
        self.assertEqual(backup.stat().st_size, 25 * 1024 * 1024)


if __name__ == '__main__':
    unittest.main(verbosity=2)
