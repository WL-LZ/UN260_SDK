#!/usr/bin/env python3
"""Validate the production inventory/descriptor contract before any writes."""
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
SOURCE = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
ROLES = {
    'engine': 'usr/bin/ui_update.sh',
    'sync': 'usr/local/bin/un260_storage_sync',
    'unpack': 'usr/local/bin/un260_unpack',
    'app_storage': 'usr/local/bin/un260_app_storage',
    'resource_cleanup': 'usr/local/bin/un260_resource_cleanup',
}


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='un260-engine-contract.')
        self.base = Path(self.temporary.name)
        self.root = self.base / 'root'
        self.stage = self.base / 'stage'
        self.payload = self.stage / 'payload'
        self.platform = self.root / 'etc/un260/updater-platform'
        self.platform.parent.mkdir(parents=True)
        self.platform.write_text('UN260_UPDATER_PLATFORM_V1\n')
        self.modes = {}
        self.put('usr/local/bin/test_lvgl', b'app0', '0755')
        self.put('usr/local/lib/liblvgl.so', b'library', '0755')
        for role, path in ROLES.items():
            self.put(path, ('#!/bin/sh\n# core-' + role + '\nexit 0\n').encode(), '0755')
        self.runtime = 'usr/local/lib/un260-startup/runtime.sh'
        self.put(self.runtime, b'#!/bin/sh\nexit 0\n', '0755')
        self.bind_descriptors()
        self.refresh()

    def tearDown(self):
        self.temporary.cleanup()

    def put(self, relative, data, mode):
        path = self.payload / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        # Native unpack may write 0600, FAT may expose 0755. Those are transport
        # modes; the authenticated target table specifies installation modes.
        path.chmod(0o600)
        self.modes[relative] = mode

    def bind_descriptors(self):
        lines = ['UN260_UPDATE_CORE_V1']
        lines.extend(role + '_sha256=' + digest((self.payload / path).read_bytes()) for role, path in ROLES.items())
        self.put('etc/un260/update-core.manifest', ('\n'.join(lines) + '\n').encode(), '0644')
        self.put('etc/un260/startup-runtime.sha256', (digest((self.payload / self.runtime).read_bytes()) + '\n').encode(), '0644')

    def refresh(self):
        paths = sorted(path for path in self.payload.rglob('*') if path.is_file())
        checks = []
        table = []
        for path in paths:
            relative = str(path.relative_to(self.payload))
            data = path.read_bytes()
            checks.append(digest(data) + '  payload/' + relative + '\n')
            table.append(digest(data) + '|' + self.modes[relative] + '|' + str(len(data)) + '|' + relative + '\n')
        (self.stage / 'checksums.sha256').write_text(''.join(checks))
        inventory = ''.join(table).encode()
        (self.stage / 'target.tsv').write_bytes(inventory)
        (self.stage / 'manifest.ini').write_text(
            'updater_envelope=UN260_UPDATE_ENVELOPE_V1\nupdater_platform=UN260_UPDATER_PLATFORM_V1\n'
            'updater_engine_abi=1\npackage_type=ui\ntarget_id=' + digest(inventory) + '\n')
        self.count = len(paths)

    def validate(self):
        q = shlex.quote
        script = ('UN260_UPDATER_LIBRARY_ONLY=1; . ' + q(str(SOURCE)) + '; . ' + q(str(SDK / 'tools/un260-update/engine_trial.sh')) + '; '
                  'ROOT_PREFIX=' + q(str(self.root)) + '; STAGE_DIR=' + q(str(self.stage)) + '; '
                  'SEEN_TARGETS=' + q(str(self.base / 'seen')) + '; LOG=' + q(str(self.base / 'log')) + '; '
                  'package_schema=1; APP_VOLUME=1; checksum_count=' + str(self.count) + '; '
                  'fail_update() { printf "%s\\n" "$1" >&2; exit 17; }; validate_updater_contract')
        return subprocess.run(SHELL + ['-c', script], capture_output=True, text=True, timeout=10)

    def rejects(self):
        result = self.validate()
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)

    def rewrite_table(self, rows):
        inventory = ''.join('|'.join(row) + '\n' for row in rows).encode()
        (self.stage / 'target.tsv').write_bytes(inventory)
        manifest = self.stage / 'manifest.ini'
        lines = [line for line in manifest.read_text().splitlines() if not line.startswith('target_id=')]
        manifest.write_text('\n'.join(lines) + '\ntarget_id=' + digest(inventory) + '\n')

    def test_v1_full_table_shape_and_budget_rejected_before_planning(self):
        original = [line.split('|') for line in (self.stage / 'target.tsv').read_text().splitlines()]
        variants = []
        rows = [row[:] for row in original]; rows.reverse(); variants.append(rows)
        rows = [row[:] for row in original]; rows[0][2] = '01'; variants.append(rows)
        rows = [row[:] for row in original]; rows[0][2] = str(24 * 1024 * 1024 + 1); variants.append(rows)
        rows = [row[:] for row in original]
        for i in range(2): rows[i][2] = str(17 * 1024 * 1024)
        variants.append(rows)
        rows = [row[:] for row in original]
        for row in rows:
            if row[3] == 'usr/local/bin/test_lvgl': row[2] = '3'
        variants.append(rows)
        for rows in variants:
            with self.subTest(rows=rows):
                self.rewrite_table(rows); self.rejects()
                self.assertFalse((self.root / 'var/lib/un260-updater/transaction').exists())
        self.rewrite_table(original)
        self.assertEqual(self.validate().returncode, 0)

    def test_v1_full_table_physical_limit_rejects_before_parser(self):
        (self.stage / 'target.tsv').write_bytes(b'X' * (3 * 1024 * 1024 + 1))
        data = (self.stage / 'target.tsv').read_bytes()
        manifest = self.stage / 'manifest.ini'
        lines = [line for line in manifest.read_text().splitlines() if not line.startswith('target_id=')]
        manifest.write_text('\n'.join(lines) + '\ntarget_id=' + digest(data) + '\n')
        self.rejects()

    def test_complete_descriptors_and_transport_modes_pass(self):
        result = self.validate()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for path in self.payload.rglob('*'):
            if path.is_file():
                path.chmod(0o755)
        self.assertEqual(self.validate().returncode, 0)

    def test_descriptor_wrong_header_order_digest_or_extra_line_rejects(self):
        path = self.payload / 'etc/un260/update-core.manifest'
        original = path.read_bytes()
        lines = original.decode().splitlines()
        for data in [original.replace(b'UN260_UPDATE_CORE_V1', b'UN260_UPDATE_CORE_V2'),
                     ('\n'.join([lines[0], lines[2], lines[1], *lines[3:]]) + '\n').encode(),
                     original.replace(lines[1].encode(), ('engine_sha256=' + '0' * 64).encode()),
                     original + b'\n']:
            with self.subTest(data=data):
                path.write_bytes(data)
                self.refresh()
                self.rejects()
        path.write_bytes(original)

    def test_runtime_descriptor_or_syntax_rejects_even_matching_inventory(self):
        path = self.payload / 'etc/un260/startup-runtime.sha256'
        for data in [b'0' * 64 + b'\n', path.read_bytes() + b'\n']:
            path.write_bytes(data)
            self.refresh()
            self.rejects()
        (self.payload / self.runtime).write_bytes(b'#!/bin/sh\nif then\n')
        self.bind_descriptors()
        self.refresh()
        self.rejects()

    def test_missing_required_component_rejects(self):
        for relative in ['etc/un260/update-core.manifest', 'etc/un260/startup-runtime.sha256', self.runtime]:
            path = self.payload / relative
            original = path.read_bytes()
            path.unlink()
            self.refresh()
            self.rejects()
            path.write_bytes(original)

    def test_inventory_hash_checksum_or_reserved_target_rejects(self):
        (self.stage / 'target.tsv').write_text('damaged\n')
        self.rejects()
        self.refresh()
        path = self.stage / 'checksums.sha256'
        path.write_text(path.read_text().replace('payload/usr/bin/ui_update.sh', 'payload/usr/bin/wrong.sh'))
        self.rejects()
        self.put('usr/local/lib/un260-updater/rescue/engine.sh', b'overwrite-factory', '0755')
        self.refresh()
        self.rejects()

    def test_missing_wrong_or_symlinked_platform_rejects(self):
        self.platform.write_text('UN260_UPDATER_PLATFORM_V0\n')
        self.rejects()
        self.platform.unlink()
        self.rejects()
        self.platform.symlink_to(self.stage / 'manifest.ini')
        self.rejects()


if __name__ == '__main__':
    unittest.main(verbosity=2)
