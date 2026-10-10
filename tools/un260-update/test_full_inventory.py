#!/usr/bin/env python3
"""Real shell/file validation; directory fixtures do not prove flash durability."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
SOURCE = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
TRIAL = SDK / 'tools/un260-update/engine_trial.sh'
SHELL = [shutil.which('busybox'), 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['/bin/sh']
APP = 'usr/local/bin/test_lvgl'
STATIC = 'usr/local/share/lvgl_data'


def digest(data):
    return hashlib.sha256(data).hexdigest()


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='un260-full-inventory.')
        self.base = Path(self.temporary.name)
        self.root = self.base / 'root'
        self.stage = self.base / 'stage'
        self.payload = self.stage / 'payload'
        self.bin = self.base / 'bin'
        self.bin.mkdir()
        # These are the actual device applets used by this engine, not the host's
        # broader applet set. In particular od/head/tail/stat/cmp are absent.
        bb = shutil.which('busybox')
        for command in ['awk', 'cat', 'chmod', 'cp', 'date', 'dd', 'dirname', 'find',
                        'grep', 'id', 'ln', 'ls', 'mkdir', 'mktemp', 'mv', 'readlink',
                        'rm', 'rmdir', 'sed', 'sh', 'sort', 'sync', 'tr', 'wc']:
            (self.bin / command).symlink_to(bb)
        (self.bin / 'sha256sum').symlink_to(shutil.which('sha256sum'))
        self.files = {
            APP: (b'app-new', '0755'),
            'usr/local/lib/liblvgl.so': (b'lib-same', '0755'),
            STATIC + '/a.png': (b'image-same', '0644'),
            STATIC + '/b.png': (b'image-new', '0644'),
            'etc/un260/package-version': (b'v-new', '0644'),
        }
        for path, (data, mode) in self.files.items():
            self.put(self.payload / path, data, '0600')
            old = data if path.endswith(('a.png', 'liblvgl.so')) else b'old-' + data
            self.put(self.root / path, old, mode)
        self.put(self.root / STATIC / 'retired.png', b'retired', '0644')
        self.manifest = ['file|0755|' + APP, 'tree|0755|' + STATIC,
                         'file|0755|usr/local/lib/liblvgl.so',
                         'file|0644|etc/un260/package-version']
        self.refresh()

    def tearDown(self):
        self.temporary.cleanup()

    def put(self, path, data, mode):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(int(mode, 8))

    def refresh(self):
        checks = []
        rows = []
        for path, (_, mode) in sorted(self.files.items()):
            data = (self.payload / path).read_bytes()
            sha = digest(data)
            checks.append(sha + '  payload/' + path + '\n')
            rows.append(sha + '|' + mode + '|' + str(len(data)) + '|' + path + '\n')
        (self.stage / 'checksums.sha256').write_text(''.join(checks))
        (self.stage / 'target.tsv').write_text(''.join(rows))
        (self.stage / 'install.tsv').write_text('\n'.join(self.manifest) + '\n')
        (self.stage / 'manifest.ini').write_text('package_type=ui\n')
        self.target_id = digest(''.join(rows).encode())

    def call(self, action, *, before='', full=True, planning=True):
        script = '''UN260_UPDATER_LIBRARY_ONLY=1
. "$1"
. "$2"
ROOT_PREFIX=$3/root
STAGE_DIR=$3/stage
PLAN_FILE=$3/plan
SEEN_TARGETS=$3/seen
CHECKSUM_TARGETS=$3/checks
PAYLOAD_FILES=$3/payload-files
LOG=$3/log
package_schema=1
WORK_OWNED=0
fail_update() { printf '%s\\n' "$1" >&2; exit 17; }
''' + before + '\nvalidate_payload_checksums\n'
        if full:
            script += ('FULL_TARGET_ID=' + self.target_id + '''
trial_targets_count=$checksum_count
trial_app_sha=$(awk '$2=="payload/usr/local/bin/test_lvgl" {print $1}' "$STAGE_DIR/checksums.sha256")
trial_table_format_valid "$STAGE_DIR/target.tsv" || exit 18
''')
        if planning:
            script += 'validate_install_manifest\n'
        script += action
        env = dict(os.environ, PATH=str(self.bin))
        return subprocess.run(SHELL + ['-c', script, 'inventory-test', str(SOURCE), str(TRIAL), str(self.base)],
                              env=env, text=True, capture_output=True, timeout=30)

    def assert_ok(self, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def install_fixture(self):
        return ('while IFS="|" read -r mode rel hash bytes; do cp "$STAGE_DIR/payload/$rel" "$ROOT_PREFIX/$rel"; '
                'chmod "$mode" "$ROOT_PREFIX/$rel"; done < "$PLAN_FILE.inventory"\n'
                'rm -f "$ROOT_PREFIX/' + STATIC + '/retired.png"\n')

    def test_full_order_unique_coverage_and_changed_bytes(self):
        self.assert_ok(self.call('printf "%s %s\\n" "$INSTALL_ENTRY_COUNT" "$SKIPPED_COUNT"'))
        records = [line.split('|') for line in (self.base / 'plan.inventory').read_text().splitlines()]
        self.assertEqual([row[1] for row in records], [APP, STATIC + '/a.png', STATIC + '/b.png',
                                                    'usr/local/lib/liblvgl.so', 'etc/un260/package-version'])
        plan = (self.base / 'plan').read_text().splitlines()
        self.assertEqual(len(plan), 4)  # Three replacements plus one retired file.
        self.assertEqual(sum(int(row.split('|')[5]) for row in plan),
                         sum(len(self.files[path][0]) for path in [APP, STATIC + '/b.png', 'etc/un260/package-version']))

    def test_duplicate_overlap_or_missing_manifest_rejected_before_plan(self):
        original = self.manifest[:]
        for manifest in [original + [original[0]], original[:-1], original + ['file|0644|' + STATIC + '/a.png']]:
            with self.subTest(manifest=manifest):
                self.manifest = manifest
                self.refresh()
                self.assertNotEqual(self.call('').returncode, 0)
                self.assertEqual((self.base / 'plan').read_bytes(), b'')

    def test_authenticated_wrong_mode_or_size_rejected(self):
        original = (self.stage / 'target.tsv').read_text()
        for text in [original.replace('|0755|7|' + APP, '|0644|7|' + APP),
                     original.replace('|0755|7|' + APP, '|0755|8|' + APP),
                     original.replace(digest(b'app-new') + '|0755|7|' + APP, '0' * 64 + '|0755|7|' + APP)]:
            with self.subTest(text=text):
                (self.stage / 'target.tsv').write_text(text)
                self.target_id = digest(text.encode())
                self.assertNotEqual(self.call('').returncode, 0)

    def test_target_malformed_hash_path_sort_or_noncanonical_size(self):
        original = (self.stage / 'target.tsv').read_text()
        texts = [original.replace('|7|' + APP, '|07|' + APP),
                 original.replace(APP, 'usr/local/bin/../test_lvgl'),
                 'g' + original[1:], ''.join(reversed(original.splitlines(keepends=True)))]
        for text in texts:
            with self.subTest(text=text):
                (self.stage / 'target.tsv').write_text(text)
                self.target_id = digest(text.encode())
                self.assertNotEqual(self.call('').returncode, 0)

    def test_duplicate_unchecked_and_changed_payload_rejected(self):
        checks = (self.stage / 'checksums.sha256').read_text()
        (self.stage / 'checksums.sha256').write_text(checks + checks.splitlines(keepends=True)[0])
        self.assertNotEqual(self.call('').returncode, 0)
        (self.stage / 'checksums.sha256').write_text(checks)
        self.put(self.payload / STATIC / 'unchecked.png', b'unknown', '0600')
        self.assertNotEqual(self.call('').returncode, 0)
        (self.payload / STATIC / 'unchecked.png').unlink()
        (self.payload / APP).write_bytes(b'corrupt')
        self.assertNotEqual(self.call('').returncode, 0)

    def test_payload_and_destination_links_rejected(self):
        for path in [self.payload / APP, self.root / APP, self.root / 'usr/local/lib']:
            with self.subTest(path=path):
                saved = path.with_name(path.name + '.saved')
                path.rename(saved)
                path.symlink_to(saved)
                self.assertNotEqual(self.call('').returncode, 0)
                path.unlink()
                saved.rename(path)

    def test_destination_mutation_is_not_hidden_by_preflight_hash(self):
        for mutation in ['printf corrupt > "$ROOT_PREFIX/usr/local/lib/liblvgl.so"',
                         'chmod 0644 "$ROOT_PREFIX/usr/local/lib/liblvgl.so"']:
            with self.subTest(mutation=mutation):
                self.assertNotEqual(self.call(self.install_fixture() + mutation + '\nverify_installed_targets').returncode, 0)
                self.put(self.root / 'usr/local/lib/liblvgl.so', b'lib-same', '0755')

    def test_installed_full_verification_and_extra_static_rejection(self):
        install = self.install_fixture()
        self.assert_ok(self.call(install + 'verify_installed_targets'))
        self.put(self.root / STATIC / 'extra.png', b'extra', '0644')
        self.assertNotEqual(self.call(install + 'verify_installed_targets').returncode, 0)

    def test_inventory_changed_before_and_during_consumption_rejected(self):
        self.assertNotEqual(self.call('printf fake >> "$PLAN_FILE.inventory"; verify_installed_targets').returncode, 0)
        before = '''checked_digest() {
  digest_output=$(sha256sum "$1") || return 1
  digest_value=${digest_output%% *}
  boot_hex64 "$digest_value" || return 1
  case "$1" in */root/usr/local/lib/liblvgl.so) printf fake >> "$PLAN_FILE.inventory" ;; esac
  printf '%s\\n' "$digest_value"
}
'''
        self.assertNotEqual(self.call(self.install_fixture() + 'verify_installed_targets', before=before).returncode, 0)

    def test_inventory_symlink_rejected(self):
        self.assertNotEqual(self.call('mv "$PLAN_FILE.inventory" "$PLAN_FILE.saved"; ln -s "$PLAN_FILE.saved" "$PLAN_FILE.inventory"; verify_installed_targets').returncode, 0)

    def test_inventory_wrong_fields_rejected_even_with_new_local_digest(self):
        for edit in ['s/0755/0644/', 's/|7$/|8/', 's/usr.local.bin.test_lvgl/usr\/local\/bin\/alien/']:
            with self.subTest(edit=edit):
                action = ("sed '" + edit + "' \"$PLAN_FILE.inventory\" > \"$PLAN_FILE.edited\"; "
                          'mv "$PLAN_FILE.edited" "$PLAN_FILE.inventory"; '
                          'FULL_INVENTORY_HASH=$(checked_digest "$PLAN_FILE.inventory"); full_inventory_valid')
                self.assertNotEqual(self.call(action).returncode, 0)

    def test_partial_inventory_read_cannot_skip_actual_target_verification(self):
        for stop in [1, 3, 5]:
            with self.subTest(stop=stop):
                action = self.install_fixture() + '''printf broken > "$ROOT_PREFIX/usr/local/lib/liblvgl.so"
read_count=0
read() {
  case "$*" in *verified_mode*) read_count=$((read_count+1)); [ "$read_count" != ''' + str(stop) + ''' ] || return 1 ;; esac
  command read "$@"
}
verify_installed_targets
'''
                self.assertNotEqual(self.call(action).returncode, 0)

    def test_partial_planning_read_and_transient_inventory_open_failure(self):
        before = '''read() { case "$*" in *inventory_mode*) return 1 ;; esac; command read "$@"; }
'''
        self.assertNotEqual(self.call('', before=before).returncode, 0)
        action = self.install_fixture() + '''inventory_checks=0
full_inventory_valid() {
  inventory_checks=$((inventory_checks+1))
  if [ "$inventory_checks" = 1 ]; then mv "$PLAN_FILE.inventory" "$PLAN_FILE.saved";
  else mv "$PLAN_FILE.saved" "$PLAN_FILE.inventory"; fi
}
verify_installed_targets
'''
        self.assertNotEqual(self.call(action).returncode, 0)

    def test_unterminated_inventory_tail_rejected(self):
        action = self.install_fixture() + '''inventory_size=$(wc -c < "$PLAN_FILE.inventory")
dd if="$PLAN_FILE.inventory" of="$PLAN_FILE.cut" bs=1 count=$((inventory_size-1)) 2>/dev/null
mv "$PLAN_FILE.cut" "$PLAN_FILE.inventory"
FULL_INVENTORY_HASH=$(checked_digest "$PLAN_FILE.inventory")
verify_installed_targets
'''
        self.assertNotEqual(self.call(action).returncode, 0)

    def test_source_changes_after_payload_check_rejected(self):
        before = '''request_install_file() {
  printf '%s|%s\\n' "$2" "$1" >> "$PLAN_FILE.requests"
  if [ "$1" = usr/local/bin/test_lvgl ]; then printf altered-too-long > "$STAGE_DIR/payload/$1"; fi
}
'''
        self.assertNotEqual(self.call('', before=before).returncode, 0)

    def test_checksum_or_target_changes_after_validation_rejected(self):
        for filename in ['checksums.sha256', 'target.tsv']:
            with self.subTest(filename=filename):
                before = '''request_install_file() {
  printf '%s|%s\\n' "$2" "$1" >> "$PLAN_FILE.requests"
  if [ "$1" = usr/local/bin/test_lvgl ]; then printf fake >> "$STAGE_DIR/''' + filename + '''"; fi
}
'''
                self.assertNotEqual(self.call('', before=before).returncode, 0)
                self.refresh()

    def test_legacy_plan_branch_and_retired_membership_preserved(self):
        result = self.call('printf "%s %s\\n" "$INSTALL_ENTRY_COUNT" "$SKIPPED_COUNT"', full=False)
        self.assert_ok(result)
        self.assertEqual(result.stdout.splitlines()[-1], '4 2')
        self.assertFalse((self.base / 'plan.inventory').exists())

    def test_no_external_dirname_for_safe_ancestor_walk(self):
        before = 'dirname() { printf forbidden >&2; return 99; };\n'
        self.assert_ok(self.call('', before=before))
        for relative in ['usr/local/bin/test_lvgl', 'usr/local/share/lvgl_data/a.png', 'etc/un260/package-version']:
            self.assert_ok(self.call('safe_destination "' + relative + '"', before=before))

    def test_table_lookup_count_does_not_scale_per_file(self):
        counts = []
        for n in [8, 48]:
            for i in range(n):
                path = STATIC + '/batch-' + str(i) + '.png'
                self.files[path] = (b'asset', '0644')
                self.put(self.payload / path, b'asset', '0600')
                self.put(self.root / path, b'asset', '0644')
            self.refresh()
            before = 'awk() { printf "a\\n" >> "$3_unused"; command awk "$@"; };\n'.replace('$3_unused', str(self.base / 'awk-calls'))
            (self.base / 'awk-calls').write_text('')
            self.assert_ok(self.call('', before=before))
            counts.append(len((self.base / 'awk-calls').read_text().splitlines()))
        # Actual SHA, size and mode still read every file. Metadata joins do
        # not spawn a separate awk lookup for each additional target.
        self.assertEqual(counts[1], counts[0])


if __name__ == '__main__':
    unittest.main(verbosity=2)
