#!/usr/bin/env python3
"""Normal make publishes one direct package and preserves operator archives."""
import json
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
        self.addCleanup(self.temp.cleanup)
        self.p = Path(self.temp.name)
        self.images = self.p / 'images'
        self.images.mkdir()
        self.internal = self.images / 'un260_internal'
        self.fixed = self.images / 'UN260_STORAGE_EXPANSION_R7_20260929.upk'
        self.fixed.write_bytes(b'PINNED R7 MUST NOT CHANGE')
        self.fixed.chmod(0o444)
        self.customer = self.images / 'CUSTOMER_ARCHIVE.upk'
        self.customer.write_bytes(b'CUSTOMER KEEP')
        tools = self.p / 'tools/un260-update'
        tools.mkdir(parents=True)
        self.fakebin = self.p / 'bin'
        self.fakebin.mkdir()
        dispatcher = '''#!%s
import json, os
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
    path.write_bytes(b'DIRECT FULL APPLICATION')
elif name == os.environ.get('FAIL_VERIFY'):
    sys.exit(1)
elif '--report' in args:
    path = Path(args[args.index('--report') + 1])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('{}\\n')
''' % sys.executable
        (self.fakebin / 'python3').write_text(dispatcher)
        (self.fakebin / 'python3').chmod(0o755)
        builder = tools / 'build_un260_update.sh'
        builder.write_text('#!%s\nimport subprocess, sys\nsubprocess.run(["python3", "application-builder"] + sys.argv[1:], check=True)\n' % sys.executable)
        builder.chmod(0o755)

    def run_hook(self, ok=True, fail='', deep_python=''):
        env = dict(os.environ, BINARIES_DIR=str(self.images), TARGET_DIR=str(self.p / 'target'),
                   TARGET_BOARD_DIR=str(self.p / 'board'), FAIL_VERIFY=fail,
                   UN260_FACTORY_DEEP_QA_PYTHON=deep_python,
                   PATH=str(self.fakebin) + os.pathsep + os.environ['PATH'])
        result = subprocess.run(['bash', str(HOOK)], cwd=self.p, env=env,
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode == 0, ok, result.stdout + result.stderr)
        preserved = self.fixed if self.fixed.exists() else self.internal / 'archived-public' / self.fixed.name
        self.assertEqual(preserved.read_bytes(), b'PINNED R7 MUST NOT CHANGE')
        self.assertEqual(preserved.stat().st_mode & 0o777, 0o444)
        self.assertEqual(self.customer.read_bytes(), b'CUSTOMER KEEP')
        return result

    def test_only_direct_is_published_no_default_migration_or_delta(self):
        readme = self.images / 'UN260_UPK_README.md'
        readme.write_text('OLD: choose two packages, R7 bridge, two continuations')
        self.run_hook()
        first_readme = readme.read_bytes()
        self.run_hook()
        self.assertEqual(readme.read_bytes(), first_readme)
        text = readme.read_text()
        for expected in ('UN260_UPDATE.upk', 'update/UN260_UPDATE.upk', 'V1', '\u4e00\u6b21\u4e8b\u52a1', '\u5065\u5eb7\u786e\u8ba4', 'un260_internal/'):
            self.assertIn(expected, text)
        self.assertNotIn('R7', text)
        self.assertEqual(readme.stat().st_mode & 0o777, 0o644)
        self.assertFalse((self.images / 'UN260_UPK_README.md.tmp').exists())
        self.assertEqual((self.images / 'UN260_UPDATE.upk').read_bytes(), b'DIRECT FULL APPLICATION')
        self.assertEqual({p.name for p in self.internal.glob('*.upk')}, {'UN260_APPLICATION.upk'})
        self.assertEqual({p.name for p in self.images.glob('*.upk')}, {'UN260_UPDATE.upk', self.customer.name})
        names = [Path(call[0]).name for call in map(json.loads, (self.p / 'calls.jsonl').read_text().splitlines())]
        self.assertNotIn('build_unified.py', names)
        self.assertNotIn('build_delta.py', names)
        self.assertNotIn('build_storage_bootstrap.py', names)
        self.assertIn('verify_direct_release.py', names)
        self.assertIn('verify_factory_release.py', names)
        for call in map(json.loads, (self.p / 'calls.jsonl').read_text().splitlines()):
            if Path(call[0]).name == 'verify_factory_release.py': self.assertNotIn('--deep', call)
        subprocess.run(['sha256sum', '-c', 'UN260_UPDATE.upk.sha256'], cwd=self.images, check=True, capture_output=True)

    def test_verification_failure_preserves_previous_public_package(self):
        public = self.images / 'UN260_UPDATE.upk'
        public.write_bytes(b'PREVIOUS VERIFIED')
        readme = self.images / 'UN260_UPK_README.md'
        readme.write_bytes(b'PREVIOUS README')
        self.run_hook(ok=False, fail='verify_direct_release.py')
        self.assertEqual(public.read_bytes(), b'PREVIOUS VERIFIED')
        self.assertEqual(readme.read_bytes(), b'PREVIOUS README')
        self.assertTrue(self.fixed.exists())

    def test_existing_archive_is_never_overwritten(self):
        archived = self.internal / 'archived-public' / self.fixed.name
        archived.parent.mkdir(parents=True)
        archived.write_bytes(b'OTHER ARCHIVE')
        self.run_hook(ok=False)
        self.assertEqual(archived.read_bytes(), b'OTHER ARCHIVE')
        self.assertTrue(self.fixed.exists())

    def test_symlinked_public_package_is_refused(self):
        outside = self.p / 'customer-file'
        outside.write_bytes(b'KEEP')
        (self.images / 'UN260_UPDATE.upk').symlink_to(outside)
        self.run_hook(ok=False)
        self.assertEqual(outside.read_bytes(), b'KEEP')

    def test_symlinked_internal_directory_is_refused(self):
        outside = self.p / 'outside'
        outside.mkdir()
        self.internal.symlink_to(outside)
        self.run_hook(ok=False)
        self.assertEqual(list(outside.iterdir()), [])

    def test_readme_and_temporary_symlinks_are_refused_before_publication(self):
        for name in ('UN260_UPK_README.md', 'UN260_UPK_README.md.tmp'):
            with self.subTest(name=name):
                outside = self.p / 'readme-customer-file'
                outside.write_bytes(b'KEEP README ARCHIVE')
                public = self.images / 'UN260_UPDATE.upk'
                public.write_bytes(b'PREVIOUS VERIFIED')
                link = self.images / name
                link.symlink_to(outside)
                self.run_hook(ok=False)
                self.assertEqual(outside.read_bytes(), b'KEEP README ARCHIVE')
                self.assertEqual(public.read_bytes(), b'PREVIOUS VERIFIED')
                link.unlink()
                archived = self.internal / 'archived-public' / self.fixed.name
                if archived.exists(): archived.replace(self.fixed)

    def test_deep_factory_qa_is_explicit_and_failure_preserves_publication(self):
        self.run_hook(deep_python=str(self.fakebin / 'python3'))
        calls = list(map(json.loads, (self.p / 'calls.jsonl').read_text().splitlines()))
        factory = [call for call in calls if Path(call[0]).name == 'verify_factory_release.py']
        self.assertEqual(len(factory), 1)
        self.assertIn('--deep', factory[0])
        public = self.images / 'UN260_UPDATE.upk'
        public.write_bytes(b'PREVIOUS VERIFIED')
        readme = self.images / 'UN260_UPK_README.md'
        readme.write_bytes(b'PREVIOUS README')
        self.run_hook(ok=False, deep_python=str(self.p / 'missing-python'))
        self.assertEqual(public.read_bytes(), b'PREVIOUS VERIFIED')
        self.assertEqual(readme.read_bytes(), b'PREVIOUS README')


if __name__ == '__main__': unittest.main()
