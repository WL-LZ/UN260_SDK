#!/usr/bin/env python3
"""Package routing and downgrade regression; host fixtures, never real NAND."""
import hashlib
import unittest

from build_delta import load, build as build_delta
from build_storage_bootstrap import build as bootstrap, APPROVED, POLICY_V2, POLICY_V3, POLICY_COMPONENTS
from build_unified import build, NESTED
import test_unified

MARKER = b'UN260_STORAGE_POLICY_V2'
MARKER_V3 = b'UN260_STORAGE_POLICY_V3'


class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.f = test_unified.UnifiedTests()
        self.f.setUp()
        helper = 'usr/local/bin/un260_app_storage'
        self.assertIn(MARKER, self.f.files[helper][0])
        build(self.f.full, self.f.unified, POLICY_V2)

    def tearDown(self):
        self.f.tearDown()

    def test_v2_bridge_then_boot_and_policy_readback(self):
        self.f.bridge()
        _, files = load(self.f.unified)
        nested = self.f.p / 'readback-migration.upk'
        nested.write_bytes(files[NESTED + 'migration.upk'][0])
        meta, migration = load(nested)
        self.assertEqual(meta['storage_migration'], POLICY_V2)
        for path in POLICY_COMPONENTS:
            self.assertIn(MARKER, migration[path][0])
        self.f.boot()
        self.assertTrue((self.f.root / 'var/lib/un260-updater/unified.completed').is_file())

    def test_v3_bridge_boot_and_managed_retry_use_same_public_package(self):
        build(self.f.full, self.f.unified, POLICY_V3)
        self.f.bridge()
        _, files = load(self.f.unified)
        migration_path = self.f.p / 'readback-v3-migration.upk'
        migration_path.write_bytes(files[NESTED + 'migration.upk'][0])
        meta, migration = load(migration_path)
        self.assertEqual(meta['storage_migration'], POLICY_V3)
        for path in POLICY_COMPONENTS:
            self.assertIn(MARKER_V3, migration[path][0])
        self.f.boot()
        first_calls = (self.f.p / 'migration-calls').read_text()
        self.f.run_update(package=self.f.unified)
        self.assertEqual((self.f.p / 'migration-calls').read_text(), first_calls)
        self.assertIn('success=1', (self.f.root / 'status').read_text())

    def test_v3_builder_rejects_each_old_recovery_component(self):
        original = dict(self.f.files)
        for path in POLICY_COMPONENTS:
            with self.subTest(path=path):
                self.f.files = dict(original)
                data, mode = self.f.files[path]
                self.f.files[path] = (data.replace(MARKER_V3, b'OLD_STORAGE_POLICY'), mode)
                self.f.pack()
                with self.assertRaises(ValueError):
                    build(self.f.full, self.f.unified, POLICY_V3)

    def test_v3_installed_rejects_v2_outer_full_and_delta_downgrade(self):
        build(self.f.full, self.f.unified, POLICY_V3)
        self.f.bridge()
        base = self.f.p / 'v3-base.upk'; base.write_bytes(self.f.full.read_bytes())
        for path in POLICY_COMPONENTS:
            data, mode = self.f.files[path]
            self.f.files[path] = (data.replace(MARKER_V3, b'OLD_STORAGE_POLICY'), mode)
        self.f.pack()
        self.assert_downgrade_refused(self.f.full)
        old_outer = self.f.p / 'v2-outer.upk'
        build(self.f.full, old_outer, POLICY_V2)
        self.assert_downgrade_refused(old_outer)
        delta = self.f.p / 'v2-delta.upk'
        build_delta(base, self.f.full, delta)
        self.assert_downgrade_refused(delta)

    def test_v3_migration_requires_installed_v3_helper(self):
        build(self.f.full, self.f.unified, POLICY_V3)
        self.f.bridge()
        migration = self.f.p / 'v3-migration.upk'
        bootstrap(self.f.full, migration, True, POLICY_V3)
        helper = self.f.root / 'usr/local/bin/un260_app_storage'
        helper.write_bytes(helper.read_bytes().replace(MARKER_V3, b'OLD_STORAGE_POLICY'))
        _, log = self.f.run_update(package=migration, managed=0, ok=False)
        self.assertIn('matching backed-up legacy storage bridge', log)
        self.assertFalse((self.f.p / 'migration-calls').exists())

    def test_builder_refuses_unknown_policy(self):
        with self.assertRaises(ValueError):
            build(self.f.full, self.f.unified, 'accept-anything')
        with self.assertRaises(ValueError):
            bootstrap(self.f.full, self.f.p / 'migration.upk', True, 'accept-anything')

    def test_v2_builder_rejects_each_old_recovery_component(self):
        original = dict(self.f.files)
        for path in POLICY_COMPONENTS:
            with self.subTest(path=path):
                self.f.files = dict(original)
                data, mode = self.f.files[path]
                self.f.files[path] = (data.replace(MARKER, b'OLD_STORAGE_POLICY'), mode)
                self.f.pack()
                with self.assertRaises(ValueError):
                    build(self.f.full, self.f.unified, POLICY_V2)
                # The explicit legacy policy still builds old-compatible input.
                build(self.f.full, self.f.p / 'legacy.upk', APPROVED)

    def make_old_components(self):
        for path in POLICY_COMPONENTS:
            data, mode = self.f.files[path]
            self.f.files[path] = (data.replace(MARKER, b'OLD_STORAGE_POLICY'), mode)
        self.f.pack()

    def assert_downgrade_refused(self, package):
        helper = self.f.root / 'usr/local/bin/un260_app_storage'
        before = hashlib.sha256(helper.read_bytes()).hexdigest()
        _, log = self.f.run_update(package=package, ok=False)
        self.assertIn('would downgrade storage recovery', log)
        self.assertEqual(hashlib.sha256(helper.read_bytes()).hexdigest(), before)

    def test_v2_installed_rejects_old_full_and_outer_bridge(self):
        self.f.bridge()
        self.make_old_components()
        self.assert_downgrade_refused(self.f.full)
        old_outer = self.f.p / 'old-outer.upk'
        build(self.f.full, old_outer, APPROVED)
        self.assert_downgrade_refused(old_outer)

    def test_v2_installed_rejects_old_delta_recovery_files(self):
        self.f.bridge()
        base = self.f.p / 'v2-base.upk'
        base.write_bytes(self.f.full.read_bytes())
        self.make_old_components()
        delta = self.f.p / 'old-components-delta.upk'
        build_delta(base, self.f.full, delta)
        self.assert_downgrade_refused(delta)

    def test_v2_migration_requires_installed_v2_helper(self):
        self.f.bridge()
        migration = self.f.p / 'migration.upk'
        bootstrap(self.f.full, migration, True, POLICY_V2)
        helper = self.f.root / 'usr/local/bin/un260_app_storage'
        helper.write_bytes(helper.read_bytes().replace(MARKER, b'OLD_STORAGE_POLICY'))
        _, log = self.f.run_update(package=migration, managed=0, ok=False)
        self.assertIn('matching storage compatibility bridge', log)
        self.assertFalse((self.f.p / 'migration-calls').exists())

    def test_v3_package_binds_matching_migration_and_recovery_components(self):
        build(self.f.full, self.f.unified, POLICY_V3)
        _, files = load(self.f.unified)
        nested = self.f.p / 'v3-migration.upk'
        nested.write_bytes(files[NESTED + 'migration.upk'][0])
        meta, migration = load(nested)
        self.assertEqual(meta['storage_migration'], POLICY_V3)
        for path in POLICY_COMPONENTS:
            self.assertIn(b'UN260_STORAGE_POLICY_V3', migration[path][0])

    def test_v3_builder_refuses_each_mismatched_recovery_component(self):
        original = dict(self.f.files)
        for path in POLICY_COMPONENTS:
            with self.subTest(path=path):
                self.f.files = dict(original)
                data, mode = self.f.files[path]
                self.f.files[path] = (data.replace(b'UN260_STORAGE_POLICY_V3', b'OLD_STORAGE_POLICY'), mode)
                self.f.pack()
                with self.assertRaises(ValueError):
                    build(self.f.full, self.f.p / 'invalid-v3.upk', POLICY_V3)
                with self.assertRaises(ValueError):
                    bootstrap(self.f.full, self.f.p / 'invalid-v3-migration.upk', True, POLICY_V3)

    def test_v3_installed_rejects_v2_recovery_downgrade(self):
        build(self.f.full, self.f.unified, POLICY_V3)
        self.f.bridge()
        for path in POLICY_COMPONENTS:
            data, mode = self.f.files[path]
            self.f.files[path] = (data.replace(b'UN260_STORAGE_POLICY_V3', b'OLD_STORAGE_POLICY'), mode)
        self.f.pack()
        self.assert_downgrade_refused(self.f.full)


if __name__ == '__main__':
    unittest.main()
