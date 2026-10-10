#!/usr/bin/env python3
"""V1 app-volume total budget, legacy limits, and actual native extractor caps."""
import gzip
import hashlib
import io
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest

from build_delta import load
from release_manifest import ENVELOPE, PLATFORM, ENGINE_ABI, LEGACY_PAYLOAD_BYTES, PLATFORM_PAYLOAD_BYTES, MAX_MEMBER_BYTES, MAX_ARCHIVE_BYTES, payload_budget, validate_payload_budget
from factory_layout import APP, APP_RELEASE_MAX_BYTES, LEGACY_TEST_APP_MAX_BYTES

SDK = Path(__file__).resolve().parents[2]
MIB = 1024 * 1024
V1 = dict(format='UN260_UPGRADE', schema='1', product='UN260', package_type='ui',
          storage_layout='app-volume-v1', updater_envelope=ENVELOPE,
          updater_platform=PLATFORM, updater_engine_abi=ENGINE_ABI)


class Sized:
    def __init__(self, size): self.size = size
    def __len__(self): return self.size


def files_for_size(size):
    first = min(size, MAX_MEMBER_BYTES)
    return {'data/first': (Sized(first), 0o644), 'data/second': (Sized(size - first), 0o644)}


def raw_archive(path, sizes):
    # Exact ustar records with just the two required terminal zero blocks;
    # tests exercise the production native byte counter without tar extra padding.
    block = bytes(65536)
    with gzip.GzipFile(filename=str(path), mode='wb', mtime=0) as compressed:
        for name, size in sizes:
            info = tarfile.TarInfo(name)
            info.size, info.mode = size, 0o644
            compressed.write(info.tobuf(format=tarfile.USTAR_FORMAT))
            remaining = size
            while remaining:
                part = min(remaining, len(block))
                compressed.write(block[:part])
                remaining -= part
            compressed.write(bytes((-size) % 512))
        compressed.write(bytes(1024))


class PackageBudgetTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-package-budget-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def package(self, size, metadata=None):
        metadata = dict(V1 if metadata is None else metadata)
        sizes = [min(size, 16 * MIB), max(0, size - 16 * MIB)]
        payload = {'payload/data/file%d' % index: bytes(count) for index, count in enumerate(sizes)}
        checks = ''.join(hashlib.sha256(data).hexdigest() + '  ' + name + '\n'
                         for name, data in sorted(payload.items())).encode()
        metadata['package_id'] = hashlib.sha256(checks).hexdigest()
        entries = {'manifest.ini': ''.join(key + '=' + value + '\n' for key, value in metadata.items()).encode(),
                   'checksums.sha256': checks}
        entries.update(payload)
        path = self.root / 'full.upk'
        with tarfile.open(path, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
            for name, data in entries.items():
                info = tarfile.TarInfo(name)
                info.size, info.mode = len(data), 0o644
                archive.addfile(info, io.BytesIO(data))
        return path

    def test_v1_24_to_32_inclusive_and_one_over(self):
        self.assertEqual(payload_budget(V1), 32 * MIB)
        for size in (24 * MIB, 24 * MIB + 1, 32 * MIB):
            validate_payload_budget(V1, files_for_size(size))
        with self.assertRaisesRegex(ValueError, 'Payload budget exceeded'):
            validate_payload_budget(V1, files_for_size(32 * MIB + 1))

    def test_legacy_and_incomplete_platform_contract_remain_24(self):
        for key in ('updater_envelope', 'updater_platform', 'updater_engine_abi', 'storage_layout'):
            metadata = dict(V1)
            metadata.pop(key)
            self.assertEqual(payload_budget(metadata), 24 * MIB)
            validate_payload_budget(metadata, files_for_size(24 * MIB))
            with self.assertRaises(ValueError): validate_payload_budget(metadata, files_for_size(24 * MIB + 1))
        for metadata in (dict(V1, release_profile='legacy-test'), dict(V1, updater_engine_abi='2')):
            self.assertEqual(payload_budget(metadata), LEGACY_PAYLOAD_BYTES)
            with self.assertRaises(ValueError): validate_payload_budget(metadata, files_for_size(24 * MIB + 1))

    def test_actual_loader_v1_boundary_and_old_profile_rejection(self):
        for size in (24 * MIB + 1, PLATFORM_PAYLOAD_BYTES):
            _, payload = load(self.package(size))
            self.assertEqual(sum(len(data) for data, _ in payload.values()), size)
        with self.assertRaisesRegex(ValueError, 'Payload budget exceeded'):
            load(self.package(PLATFORM_PAYLOAD_BYTES + 1))
        for metadata in (dict(V1, release_profile='legacy-test'), {key: value for key, value in V1.items() if not key.startswith('updater_')}):
            with self.assertRaisesRegex(ValueError, 'Payload budget exceeded'):
                load(self.package(LEGACY_PAYLOAD_BYTES + 1, metadata))

    def test_actual_loader_keeps_member_and_archive_caps(self):
        self.assertEqual((MAX_MEMBER_BYTES, MAX_ARCHIVE_BYTES), (24 * MIB, 48 * MIB))
        path = self.root / 'limits.upk'
        raw_archive(path, [('payload/member', MAX_MEMBER_BYTES + 1)])
        with self.assertRaisesRegex(ValueError, 'Oversized member'): load(path)
        raw_archive(path, [('target.tsv', MAX_MEMBER_BYTES), ('baseline.tsv', MAX_MEMBER_BYTES), ('manifest.ini', 1)])
        with self.assertRaisesRegex(ValueError, 'Archive budget exceeded'): load(path)

    def test_same_application_headroom_bound_in_package_validation(self):
        validate_payload_budget(V1, {APP: (Sized(APP_RELEASE_MAX_BYTES), 0o755)})
        with self.assertRaisesRegex(ValueError, 'headroom'):
            validate_payload_budget(V1, {APP: (Sized(APP_RELEASE_MAX_BYTES + 1), 0o755)})
        validate_payload_budget(dict(V1, release_profile='legacy-test'),
                                {APP: (Sized(LEGACY_TEST_APP_MAX_BYTES), 0o755)})


class NativeExtractorBudgetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='un260-native-budget-')
        cls.root = Path(cls.temp.name)
        cls.executable = cls.root / 'unpack'
        subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                        str(SDK / 'source/artinchip/test-lvgl/tools/un260_unpack.c'), '-lz', '-o', str(cls.executable)],
                       check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def extract(self, sizes, ok):
        with tempfile.TemporaryDirectory(prefix='case-', dir=str(self.root)) as directory:
            root = Path(directory)
            archive, stage = root / 'native.upk', root / 'stage'
            stage.mkdir()
            raw_archive(archive, [('payload/file%d' % index, size) for index, size in enumerate(sizes)])
            result = subprocess.run([str(self.executable), str(archive), str(stage)], capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
            return result

    def test_total_32_and_48_accepted_but_48_plus_one_rejected(self):
        self.extract([16 * MIB, 16 * MIB], True)
        self.extract([24 * MIB, 24 * MIB], True)
        self.extract([24 * MIB, 24 * MIB, 1], False)

    def test_member_24_accepted_but_one_over_rejected(self):
        self.extract([24 * MIB], True)
        self.extract([24 * MIB + 1], False)


if __name__ == '__main__': unittest.main()
