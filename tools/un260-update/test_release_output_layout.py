#!/usr/bin/env python3
"""Host packaging-path contract; real build separately verifies archive bytes."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
HOOK = SDK / 'target/d211/d213_devkitf/post-image-extra.sh'

class OutputLayoutTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-output-layout-')
        self.p = Path(self.temp.name)
        self.images = self.p / 'images'; self.images.mkdir()
        self.internal = self.images / 'un260_internal'
        self.fixed = self.images / 'UN260_STORAGE_EXPANSION_R7_20260929.upk'
        self.fixed.write_bytes(b'PINNED R7 MUST NOT CHANGE')
        self.fixed.chmod(0o444)
        tools = self.p / 'tools/un260-update'; tools.mkdir(parents=True)
        self.fakebin = self.p / 'bin'; self.fakebin.mkdir()
        dispatcher = '''#!%s
import json
from pathlib import Path
import sys
args = sys.argv[1:]
with Path('calls.jsonl').open('a') as log: log.write(json.dumps(args) + '\\n')
name = Path(args[0]).name
if name == 'release_version.py':
    print('1.0.0' if '--base' in args else 'layout-test')
elif '--output' in args:
    path = Path(args[args.index('--output') + 1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(name.encode())
''' % sys.executable
        (self.fakebin / 'python3').write_text(dispatcher)
        (self.fakebin / 'python3').chmod(0o755)
        builder = tools / 'build_un260_update.sh'
        builder.write_text('#!%s\nimport subprocess, sys\nsubprocess.run(["python3", "application-builder"] + sys.argv[1:], check=True)\n' % sys.executable)
        builder.chmod(0o755)

    def tearDown(self): self.temp.cleanup()

    def run_hook(self, baseline=None, ok=True):
        env = dict(os.environ, BINARIES_DIR=str(self.images), TARGET_DIR=str(self.p / 'target'),
                   TARGET_BOARD_DIR=str(self.p / 'board'),
                   PATH=str(self.fakebin) + os.pathsep + os.environ['PATH'])
        env.pop('UN260_UPDATE_BASELINE', None)
        if baseline is not None: env['UN260_UPDATE_BASELINE'] = str(baseline)
        result = subprocess.run(['bash', str(HOOK)], cwd=self.p, env=env,
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
        self.assertEqual(self.fixed.read_bytes(), b'PINNED R7 MUST NOT CHANGE')
        return result

    def test_two_public_packages_and_repeat_build_does_not_touch_fixed_r7(self):
        self.run_hook(); self.run_hook()
        self.assertEqual({p.name for p in self.images.glob('*.upk')},
                         {self.fixed.name, 'UN260_UPDATE.upk'})
        self.assertEqual({p.name for p in self.internal.glob('*.upk')},
                         {'UN260_APPLICATION.upk', 'UN260_STORAGE_BOOTSTRAP.upk',
                          'UN260_UPDATE_BASE.upk', 'UN260_UPDATE_DELTA.upk', 'UN260_FIRST_MIGRATION.upk'})
        self.assertEqual((self.images / 'UN260_UPDATE.upk').read_bytes(), b'application-builder')
        self.assertEqual((self.internal / 'UN260_FIRST_MIGRATION.upk').read_bytes(), b'build_unified.py')
        subprocess.run(['sha256sum', '-c', 'UN260_UPDATE.upk.sha256'], cwd=self.images, check=True, capture_output=True)
        self.assertFalse((self.images / 'UN260_UNIFIED_UPDATE.upk').exists())

    def test_legacy_delta_baseline_is_preserved_not_replaced_with_current_app(self):
        legacy = self.images / 'UN260_UPDATE_BASE.upk'; legacy.write_bytes(b'ORIGINAL BASELINE')
        self.run_hook(); self.run_hook()
        self.assertEqual(legacy.read_bytes(), b'ORIGINAL BASELINE')
        self.assertEqual((self.internal / legacy.name).read_bytes(), b'ORIGINAL BASELINE')

    def test_conflicting_baselines_stop_before_build(self):
        self.internal.mkdir()
        (self.images / 'UN260_UPDATE_BASE.upk').write_bytes(b'ONE')
        (self.internal / 'UN260_UPDATE_BASE.upk').write_bytes(b'TWO')
        self.assertIn('Conflicting pinned baselines', self.run_hook(ok=False).stderr)
        self.assertFalse((self.internal / 'UN260_APPLICATION.upk').exists())

    def test_explicit_baseline_remains_read_only_input(self):
        reference = self.p / 'released-baseline.upk'; reference.write_bytes(b'RELEASED')
        self.run_hook(baseline=reference)
        self.assertEqual(reference.read_bytes(), b'RELEASED')
        self.assertFalse((self.internal / 'UN260_UPDATE_BASE.upk').exists())

    def test_output_cannot_be_its_own_baseline(self):
        for path in [self.images / 'UN260_UPDATE.upk', self.internal / 'UN260_APPLICATION.upk',
                     self.internal / 'UN260_UPDATE_DELTA.upk', self.internal / 'UN260_STORAGE_BOOTSTRAP.upk']:
            with self.subTest(path=path):
                result = self.run_hook(baseline=path, ok=False)
                self.assertIn('not a build output', result.stderr)

    def test_migration_output_cannot_be_baseline(self):
        self.assertIn('not a build output', self.run_hook(baseline=self.internal/'UN260_FIRST_MIGRATION.upk',ok=False).stderr)

    def test_symlinked_public_package_is_refused(self):
        outside=self.p/'customer-file';outside.write_bytes(b'KEEP')
        (self.images/'UN260_UPDATE.upk').symlink_to(outside)
        self.run_hook(ok=False)
        self.assertEqual(outside.read_bytes(),b'KEEP')

    def test_symlinked_internal_directory_is_refused(self):
        outside = self.p / 'outside'; outside.mkdir()
        self.internal.symlink_to(outside, target_is_directory=True)
        self.run_hook(ok=False)
        self.assertEqual(list(outside.iterdir()), [])

if __name__ == '__main__': unittest.main()
