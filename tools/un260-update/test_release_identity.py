#!/usr/bin/env python3
"""Content identity, stable real builder, and source-input release versions."""
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

from build_delta import load
from release_manifest import ENVELOPE, PLATFORM, CORE, CORE_DESCRIPTOR, RUNTIME_DESCRIPTOR, RESCUE, RESCUE_NAMES, table, verify, sha, factory_contract, validate_table_limits, MAX_TABLE_ROWS, MAX_TABLE_BYTES, MAX_PATH_BYTES
from release_version import input_identity, version, image_version
from test_legacy_test_release import fixture_target

SDK = Path(__file__).resolve().parents[2]


class SourceIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-source-id-')
        self.addCleanup(self.temp.cleanup)
        self.sdk = Path(self.temp.name)
        environment = {key: value for key, value in os.environ.items()
                       if key not in ('BR2_CONFIG', 'TARGET_DIR', 'OUTPUT_DIR', 'BINARIES_DIR')}
        patch = mock.patch.dict(os.environ, environment, clear=True)
        patch.start()
        self.addCleanup(patch.stop)
        config = self.sdk / 'target/d211/d213_devkitf/image_cfg.json'
        config.parent.mkdir(parents=True)
        config.write_text('{"version":"1.2.3"}')
        self.source = self.sdk / 'source/artinchip/test-lvgl/main.c'
        self.source.parent.mkdir(parents=True)
        self.source.write_text('int main(void){return 0;}\n')

    def test_exported_input_change_changes_valid_version(self):
        first = version(self.sdk)
        self.source.write_text('int main(void){return 1;}\n')
        self.assertNotEqual(version(self.sdk), first)
        self.assertLessEqual(len(version(self.sdk)), 48)

    def test_timestamp_cache_and_work_do_not_change_identity(self):
        before = input_identity(self.sdk)
        os.utime(self.source, (1, 1))
        for name in ('tools/__pycache__/x.pyc', 'work/log.txt'):
            path = self.source.parent / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('CACHE')
        self.assertEqual(input_identity(self.sdk), before)

    def test_same_dirty_head_changes_version_with_content(self):
        for args in (['init', '-q'], ['config', 'user.email', 'fixture@example.invalid'],
                     ['config', 'user.name', 'Host fixture'], ['add', '.'], ['commit', '-qm', 'fixture']):
            subprocess.run(['git', '-C', str(self.sdk)] + args, check=True, capture_output=True)
        self.source.write_text('DIRTY_A')
        first = version(self.sdk)
        self.source.write_text('DIRTY_B')
        self.assertNotEqual(version(self.sdk), first)
        self.assertIn('-dirty-', version(self.sdk))

    def test_long_base_and_explicit_version_bounds(self):
        (self.sdk / 'target/d211/d213_devkitf/image_cfg.json').write_text('{"version":"' + 'x' * 48 + '"}')
        self.assertLessEqual(len(version(self.sdk)), 48)
        self.assertEqual(version(self.sdk, 'release-42'), 'release-42')


    def test_actual_output_config_changes_version_without_output_noise(self):
        output = self.sdk / 'output/d211_d213_devkitf'
        output.mkdir(parents=True)
        config = output / '.config'
        config.write_text('BR2_ARCH="riscv64"\n')
        before = version(self.sdk)
        config.write_text('BR2_ARCH="riscv64"\nBR2_FEATURE=y\n')
        after = version(self.sdk)
        self.assertNotEqual(before, after)
        self.assertEqual(image_version(self.sdk), '1.2.3')
        self.assertEqual(version(self.sdk), after)
        (output / 'build.log').write_text('NO IDENTITY CHANGE')
        self.assertEqual(version(self.sdk), after)

    def test_selected_target_profile_and_explicit_br2_config(self):
        selected = self.sdk / 'output/selected'
        selected.mkdir(parents=True)
        config = selected / '.config'
        config.write_text('CURRENT PROFILE A\n')
        default = self.sdk / 'output/d211_d213_devkitf'
        default.mkdir(parents=True)
        (default / '.config').write_text('OTHER PROFILE\n')
        with mock.patch.dict(os.environ, TARGET_DIR=str(selected / 'target')):
            before = input_identity(self.sdk)
            config.write_text('CURRENT PROFILE B\n')
            self.assertNotEqual(before, input_identity(self.sdk))
            identity = input_identity(self.sdk)
            relocated = self.sdk / 'outside-config'
            relocated.write_bytes(config.read_bytes())
            with mock.patch.dict(os.environ, BR2_CONFIG=str(relocated)):
                self.assertEqual(input_identity(self.sdk), identity)
                config.write_text('IGNORED BECAUSE BR2_CONFIG OVERRIDES\n')
                self.assertEqual(input_identity(self.sdk), identity)
        with mock.patch.dict(os.environ, OUTPUT_DIR=str(selected)):
            self.assertNotEqual(input_identity(self.sdk), identity)


class TargetTableLimitTests(unittest.TestCase):
    def test_row_limit_inclusive_and_one_over_rejected(self):
        self.assertEqual(MAX_TABLE_ROWS, 8192)
        files = {'data/file%04d' % index: (b'', 0o644) for index in range(MAX_TABLE_ROWS)}
        self.assertEqual(table(files).count(b'\n'), MAX_TABLE_ROWS)
        files['data/one_more'] = (b'', 0o644)
        with self.assertRaisesRegex(ValueError, 'row limit'): table(files)

    def test_ascii_relative_path_limit_inclusive(self):
        self.assertEqual(MAX_PATH_BYTES, 255)
        path = 'data/' + 'a' * (MAX_PATH_BYTES - 5)
        self.assertTrue(table({path: (b'content', 0o644)}).endswith((path + '\n').encode('ascii')))
        for bad in (path + 'a', 'data/\u4e2d\u6587', '/data/absolute', 'data/../escape'):
            with self.assertRaises(ValueError): table({bad: (b'content', 0o644)})

    def test_table_byte_limit_inclusive_and_one_over_rejected(self):
        self.assertEqual(MAX_TABLE_BYTES, 3 * 1024 * 1024)
        validate_table_limits(b'x' * (MAX_TABLE_BYTES - 1) + b'\n')
        with self.assertRaisesRegex(ValueError, 'byte limit'):
            validate_table_limits(b'x' * MAX_TABLE_BYTES + b'\n')
        generated = table({'data/file': (b'', 0o644)})
        with mock.patch('release_manifest.MAX_TABLE_BYTES', len(generated)):
            self.assertEqual(table({'data/file': (b'', 0o644)}), generated)
        with mock.patch('release_manifest.MAX_TABLE_BYTES', len(generated) - 1):
            with self.assertRaisesRegex(ValueError, 'byte limit'): table({'data/file': (b'', 0o644)})


class RealBuilderIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-real-release-builder-')
        self.addCleanup(self.temp.cleanup)
        self.p = Path(self.temp.name)
        self.sdk = self.p / 'sdk'
        tools = self.sdk / 'tools/un260-update'
        tools.mkdir(parents=True)
        for name in ('build_un260_update.sh', 'release_manifest.py', 'release_profile.py',
                     'boot_bundle.py', 'build_delta.py', 'check_storage_budget.py', 'prepare_update_platform.py',
                     'factory_layout.py', 'release_version.py'):
            shutil.copy2(SDK / 'tools/un260-update' / name, tools / name)
        self.target = self.p / 'target'
        fixture_target(self.target)
        # Structural ELF/theme fixtures qualify host packaging only; never run
        # as a real target player or claim display/device validation.
        (self.target / 'usr/local/bin/un260_unpack').write_bytes(b'UNPACKER FIXTURE')
        (self.target / 'usr/local/bin/un260_unpack').chmod(0o755)
        runtime = self.target / 'usr/local/lib/un260-startup/runtime.sh'
        runtime.parent.mkdir(parents=True, exist_ok=True)
        runtime.write_bytes(b'#!/bin/sh\n# dynamic runtime fixture\n')
        runtime.chmod(0o755)
        overlay = self.sdk / 'target/d211/d213_devkitf/rootfs_overlay'
        for name in ('usr/bin/ui_update.sh', 'usr/local/bin/un260_app_storage'):
            path = overlay / name
            path.parent.mkdir(parents=True, exist_ok=True)
            data = (self.target / name).read_bytes()
            if name == 'usr/bin/ui_update.sh':
                data += b'\n# UN260_PACKAGE_ENGINE_V1\n'
                (self.target / name).write_bytes(data)
            path.write_bytes(data)
        startup = self.sdk / 'package/artinchip/test-lvgl/S20test_lvgl'
        startup.parent.mkdir(parents=True)
        shutil.copy2(SDK / 'package/artinchip/test-lvgl/S20test_lvgl', startup)
        self.out = self.p / 'application.upk'
        contract = {'usr/local/bin/un260_update_dispatch': (b'FIXED DISPATCH', 0o755),
                    'etc/un260/updater-platform': ((PLATFORM + '\n').encode(), 0o644)}
        keys = ('engine_sha256=', 'sync_sha256=', 'unpack_sha256=', 'app_storage_sha256=', 'resource_cleanup_sha256=')
        rows = ['UN260_UPDATE_CORE_V1']
        for name, original, key in zip(RESCUE_NAMES, CORE, keys):
            data = (self.target / original).read_bytes()
            contract[RESCUE + '/' + name] = (data, 0o600 if name == 'engine.sh' else 0o755)
            rows.append(key + sha(data))
        contract[RESCUE + '/manifest'] = (('\n'.join(rows) + '\n').encode(), 0o644)
        contract[CORE_DESCRIPTOR] = (('\n'.join(rows) + '\n').encode(), 0o644)
        contract[RUNTIME_DESCRIPTOR] = ((sha(runtime.read_bytes()) + '\n').encode(), 0o644)
        contract[RESCUE + '/startup-entry.sha256'] = ((sha((self.target / 'etc/init.d/S00lvgl').read_bytes()) + '\n').encode(), 0o644)
        for name, (data, mode) in contract.items():
            path = self.target / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            path.chmod(mode)

    def build(self, extra=None, ok=True, umask=None, legacy=False):
        args = ['bash', str(self.sdk / 'tools/un260-update/build_un260_update.sh'),
                '--version', 'content-test', '--output', str(self.out)]
        if extra is not None: args += ['--extra-root', str(extra)]
        if legacy: args += ['--legacy-test']
        env = dict(os.environ, TARGET_DIR=str(self.target), PYTHONDONTWRITEBYTECODE='1')
        env.pop('UN260_UPDATE_BASELINE', None)
        before_exec = (lambda: os.umask(umask)) if umask is not None else None
        result = subprocess.run(args, env=env, capture_output=True, text=True, timeout=30, preexec_fn=before_exec)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)

    def manifest(self):
        metadata, files = load(self.out)
        with tarfile.open(self.out) as archive:
            target = archive.extractfile('target.tsv').read()
        verify(metadata, files, target)
        factory_contract(self.target)
        return metadata, files, target

    def test_repeat_build_is_byte_identical_despite_source_mtimes(self):
        self.build(umask=0o022)
        before = self.out.read_bytes()
        metadata, files, target = self.manifest()
        self.assertEqual(metadata['updater_envelope'], ENVELOPE)
        self.assertEqual(metadata['updater_platform'], PLATFORM)
        self.assertEqual(metadata['qualification_route'], 'factory-app-volume')
        self.assertEqual(set(CORE) - set(files), set())
        for path in self.target.rglob('*'):
            if path.is_file(): os.utime(path, (1, 1))
        self.build(umask=0o002)
        self.assertEqual(self.out.read_bytes(), before)
        self.assertEqual(self.manifest()[2], target)
        self.assertEqual(before[4:8], bytes(4))  # gzip timestamp disabled
        with tarfile.open(self.out) as archive:
            for item in archive.getmembers():
                self.assertEqual((item.mtime, item.uid, item.gid), (0, 0, 0))
                if item.isdir(): self.assertEqual(item.mode, 0o755)

    def test_content_and_mode_changes_are_distinct(self):
        self.build()
        metadata, files, target = self.manifest()
        changed = dict(files)
        name = 'usr/local/lib/liblvgl.so'
        changed[name] = (files[name][0] + b'CHANGED', files[name][1])
        self.assertNotEqual(table(changed), target)
        with self.assertRaises(ValueError): verify(metadata, changed, target)
        changed[name] = (files[name][0], 0o644)
        self.assertNotEqual(table(changed), target)
        with self.assertRaises(ValueError): verify(metadata, changed, target)

    def test_factory_reserved_extra_never_replaces_previous_output(self):
        self.build()
        before = self.out.read_bytes()
        extra = self.p / 'extra'
        path = extra / 'usr/local/bin/un260_update_dispatch'
        path.parent.mkdir(parents=True)
        path.write_bytes(b'REJECTED')
        self.build(extra, ok=False)
        self.assertEqual(self.out.read_bytes(), before)

    def test_missing_runtime_and_corrupt_frozen_contract_fail(self):
        runtime = self.target / 'usr/local/lib/un260-startup/runtime.sh'
        runtime.unlink()
        self.build(ok=False)
        self.assertFalse(self.out.exists())
        (self.target / RESCUE / 'startup-entry.sha256').write_text('0' * 64 + '\n')
        with self.assertRaises(ValueError): factory_contract(self.target)

    def test_normal_package_uses_postbuild_assembled_engine_and_helpers(self):
        for name in (CORE[0], CORE[3]):
            path = self.target / name
            path.write_bytes(path.read_bytes() + b'\n# assembled TARGET differs from source fixture\n')
        from release_manifest import CORE_KEYS
        manifest = ('UN260_UPDATE_CORE_V1\n' + ''.join(key + sha((self.target / name).read_bytes()) + '\n'
                    for key, name in zip(CORE_KEYS, CORE))).encode('ascii')
        (self.target / CORE_DESCRIPTOR).write_bytes(manifest)
        self.build()
        _, files, _ = self.manifest()
        for name in (CORE[0], CORE[3]):
            self.assertEqual(files[name][0], (self.target / name).read_bytes())
            self.assertNotEqual(files[name][0], (self.sdk / 'target/d211/d213_devkitf/rootfs_overlay' / name).read_bytes())

    def test_frozen_startup_watch_display_require_exact_hash_and_mode(self):
        for name in ('startup.sh', 'watch.sh', 'display'):
            path = self.target / RESCUE / name
            data, mode = path.read_bytes(), path.stat().st_mode & 0o777
            path.write_bytes(data + b'CHANGED')
            with self.assertRaisesRegex(ValueError, 'Frozen recovery component differs'): factory_contract(self.target)
            path.write_bytes(data)
            path.chmod(0o644)
            with self.assertRaises(ValueError): factory_contract(self.target)
            path.chmod(mode)
            factory_contract(self.target)

    def test_frozen_display_must_be_complete_static_riscv_elf(self):
        import struct
        path = self.target / RESCUE / 'display'
        descriptor = self.target / RESCUE / 'display.sha256'
        original = path.read_bytes()
        wrong_machine = bytearray(original)
        struct.pack_into('<H', wrong_machine, 18, 62)
        dynamic = bytearray(original)
        struct.pack_into('<I', dynamic, 64, 3)
        for bad in (b'\x7fELF' + bytes(120), bytes(wrong_machine), bytes(dynamic), original[:-1]):
            path.write_bytes(bad)
            descriptor.write_text(sha(bad) + '\n')
            with self.assertRaisesRegex(ValueError, 'Invalid frozen recovery display'): factory_contract(self.target)

    def test_v1_real_builder_over_24mib_and_explicit_legacy_rejected(self):
        resources = self.target / 'usr/local/share/lvgl_data'
        resources.mkdir(parents=True, exist_ok=True)
        for index in range(2):
            (resources / ('resource%d.bin' % index)).write_bytes(bytes(13 * 1024 * 1024))
        self.build()
        _, files, _ = self.manifest()
        self.assertGreater(sum(len(data) for data, _ in files.values()), 24 * 1024 * 1024)
        before = self.out.read_bytes()
        self.build(ok=False, legacy=True)
        self.assertEqual(self.out.read_bytes(), before)

    def test_app_boundary_shared_with_factory_rejected_before_publication(self):
        from factory_layout import APP, APP_RELEASE_MAX_BYTES
        app = self.target / APP
        app.write_bytes(b'\x7fELF' + bytes(APP_RELEASE_MAX_BYTES - 4))
        self.build()
        before = self.out.read_bytes()
        _, files, _ = self.manifest()
        self.assertEqual(len(files[APP][0]), APP_RELEASE_MAX_BYTES)
        app.write_bytes(app.read_bytes() + b'X')
        self.build(ok=False)
        self.assertEqual(self.out.read_bytes(), before)
        self.build(legacy=True)
        metadata, payload = load(self.out)
        self.assertEqual(metadata['release_profile'], 'legacy-test')
        self.assertEqual(len(payload[APP][0]), APP_RELEASE_MAX_BYTES + 1)


if __name__ == '__main__': unittest.main()
