#!/usr/bin/env python3
"""Offline factory-layout contract. No MTD, mounts, or NAND writes."""
from pathlib import Path
import tempfile
import unittest
from factory_layout import enroll, APP, STATE, OWNER, LAUNCHER, validate_app_size, APP_RELEASE_MAX_BYTES, APP_QUALIFIED_VOLUME_BYTES, APP_REPLACEMENT_RESERVE_BYTES, APP_RETAINED_GENERATIONS, LEGACY_TEST_APP_MAX_BYTES
from release_version import version

SDK = Path(__file__).resolve().parents[2]

class FactoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-factory-test-')
        self.p = Path(self.temp.name)
        self.base = self.p / 'base'; self.root = self.p / 'root'; self.vol = self.p / 'app'
        self.elf = b'\x7fELF-application'
        for tree in [self.base, self.root]:
            (tree / APP).parent.mkdir(parents=True)
            (tree / APP).write_bytes(self.elf); (tree / APP).chmod(0o755)

    def tearDown(self): self.temp.cleanup()

    def enroll(self): return enroll(self.root, self.base, self.vol, 'factory-test')

    def test_matching_factory_and_upk_input_without_runtime_state(self):
        report = self.enroll()
        self.assertEqual((self.base / APP).read_bytes(), self.elf)
        self.assertEqual((self.root / APP).read_bytes(), LAUNCHER)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), self.elf)
        self.assertEqual((self.vol / '.un260-owner').read_bytes(), OWNER)
        self.assertEqual((self.root / STATE / 'active').read_bytes(), OWNER)
        self.assertEqual((self.root / 'etc/un260/package-version').read_text(), 'factory-test\n')
        self.assertFalse(report['requires_usb']); self.assertFalse(report['first_boot_formats'])
        self.assertEqual((self.root / APP).stat().st_mode & 0o777, 0o755)
        self.assertEqual((self.vol / 'test_lvgl').stat().st_mode & 0o777, 0o755)

    def test_customer_state_rejected_before_any_enrollment(self):
        for name in [STATE, 'etc/un260/unified-request', 'var/lib/un260-updater', 'etc/ui_state']:
            with self.subTest(name=name):
                forbidden = self.root / name
                forbidden.parent.mkdir(parents=True, exist_ok=True)
                forbidden.write_bytes(b'KEEP')
                with self.assertRaises(ValueError): self.enroll()
                self.assertEqual(forbidden.read_bytes(), b'KEEP')
                self.assertEqual((self.root / APP).read_bytes(), self.elf)
                self.assertFalse(self.vol.exists())
                forbidden.unlink()

    def test_mismatching_or_non_elf_app_rejected(self):
        (self.root / APP).write_bytes(b'\x7fELF-other-build')
        with self.assertRaises(ValueError): self.enroll()
        (self.root / APP).write_bytes(b'launcher'); (self.base / APP).write_bytes(b'launcher')
        with self.assertRaises(ValueError): self.enroll()
        self.assertFalse(self.vol.exists())

    def test_symlink_app_rejected(self):
        (self.root / APP).unlink(); (self.root / APP).symlink_to(self.base / APP)
        with self.assertRaises(ValueError): self.enroll()
        self.assertEqual((self.base / APP).read_bytes(), self.elf)

    def test_preexisting_volume_is_not_overwritten(self):
        self.vol.mkdir(); (self.vol / 'customer').write_bytes(b'KEEP')
        with self.assertRaises(ValueError): self.enroll()
        self.assertEqual((self.vol / 'customer').read_bytes(), b'KEEP')
        self.assertEqual((self.root / APP).read_bytes(), self.elf)

    def test_uncompressed_update_headroom_is_required(self):
        data = b'\x7fELF' + bytes(14 * 1024 * 1024)
        (self.base / APP).write_bytes(data); (self.root / APP).write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'headroom'): self.enroll()
        self.assertFalse(self.vol.exists())

    def test_shared_app_budget_exact_boundary_and_legacy_isolation(self):
        self.assertEqual(APP_RELEASE_MAX_BYTES, 14155776)  # 13.5 MiB
        self.assertEqual(APP_RELEASE_MAX_BYTES * APP_RETAINED_GENERATIONS + APP_REPLACEMENT_RESERVE_BYTES,
                         APP_QUALIFIED_VOLUME_BYTES)
        self.assertEqual(validate_app_size(APP_RELEASE_MAX_BYTES), APP_RELEASE_MAX_BYTES)
        with self.assertRaisesRegex(ValueError, 'headroom'): validate_app_size(APP_RELEASE_MAX_BYTES + 1)
        self.assertEqual(validate_app_size(LEGACY_TEST_APP_MAX_BYTES, legacy_test=True), 16 * 1024 * 1024)
        with self.assertRaises(ValueError): validate_app_size(LEGACY_TEST_APP_MAX_BYTES + 1, legacy_test=True)

    def test_exact_13_5mib_factory_enrollment_keeps_geometry_and_modes(self):
        data = b'\x7fELF' + bytes(APP_RELEASE_MAX_BYTES - 4)
        (self.base / APP).write_bytes(data); (self.root / APP).write_bytes(data)
        report = self.enroll()
        self.assertEqual(report['app_bytes'], APP_RELEASE_MAX_BYTES)
        self.assertEqual(report['geometry']['root_bytes'], 67108864)
        self.assertEqual(report['geometry']['app_partition_bytes'], 39845888)
        self.assertEqual((self.vol / 'test_lvgl').read_bytes(), data)
        self.assertEqual((self.vol / 'test_lvgl').stat().st_mode & 0o777, 0o755)
        self.assertEqual((self.root / APP).stat().st_mode & 0o777, 0o755)

    def test_one_byte_over_factory_budget_rejected_before_mutation(self):
        data = b'\x7fELF' + bytes(APP_RELEASE_MAX_BYTES - 3)
        (self.base / APP).write_bytes(data); (self.root / APP).write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'headroom'): self.enroll()
        self.assertEqual((self.root / APP).read_bytes(), data)
        self.assertFalse(self.vol.exists())

    def test_launcher_exactly_matches_runtime_helper(self):
        helper = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage'
        import subprocess
        result = subprocess.check_output(['sh', '-c',
            'UN260_STORAGE_LIBRARY_ONLY=1; . "$1"; launcher', 'test', str(helper)])
        self.assertEqual(result, LAUNCHER)

    def test_version_uses_one_valid_explicit_release(self):
        self.assertEqual(version(SDK, '1.0.0-fleet-r7'), '1.0.0-fleet-r7')
        for invalid in ['a/b', 'line\nbreak', ' ' , 'a' * 49]:
            with self.assertRaises(ValueError): version(SDK, invalid)

    def test_exported_sdk_without_git_still_has_consistent_identity(self):
        sdk = self.p / 'export'
        config = sdk / 'target/d211/d213_devkitf/image_cfg.json'
        config.parent.mkdir(parents=True)
        config.write_text('{"version": "1.2.3"}')
        self.assertRegex(version(sdk), r'^1\.2\.3-local-[0-9a-f]{12}$')

if __name__ == '__main__': unittest.main()
