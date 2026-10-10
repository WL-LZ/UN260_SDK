#!/usr/bin/env python3
"""Fail-closed target-compatible scans and journal initialization recovery."""
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
UPDATER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/bin/ui_update.sh'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']

class SafetyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='un260-safety.')
        self.root = Path(self.tmp.name)
        self.tree = self.root / 'tree'
        self.tree.mkdir()
        self.output = self.root / 'files'
        self.bin = self.root / 'bin'
        self.bin.mkdir()

    def tearDown(self):
        self.tmp.cleanup()

    def call(self, script, *args):
        env = dict(os.environ, PATH=str(self.bin) + ':' + os.environ['PATH'])
        wrappers = ' '.join(name + '() { "' + str(path) + '" "$@"; };' for name in ('find', 'sort', 'tr') if (path := self.bin / name).exists())
        return subprocess.run(SHELL + ['-c', 'UN260_UPDATER_LIBRARY_ONLY=1; . "$1"; ' + wrappers + script,
                                     'safety-test', str(UPDATER), *map(str, args)],
                              text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)

    def scan(self):
        return self.call('scan_regular_tree "$2" "$3"', self.tree, self.output)

    def shim(self, name, body):
        path = self.bin / name
        path.write_text('#!/bin/sh\n' + body + '\n')
        path.chmod(0o755)

    def test_sorted_regular_files_and_empty_directories(self):
        (self.tree / 'empty').mkdir()
        (self.tree / 'z').write_text('z')
        (self.tree / '.hidden').write_text('h')
        result = self.scan()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.output.read_text().splitlines(), [str(self.tree / '.hidden'), str(self.tree / 'z')])

    def test_empty_tree_is_valid(self):
        self.assertEqual(self.scan().returncode, 0)
        self.assertEqual(self.output.read_bytes(), b'')

    def test_all_symlinks_rejected(self):
        (self.tree / 'file').write_text('x')
        (self.tree / 'directory').mkdir()
        for target in ['file', 'directory', 'missing']:
            with self.subTest(target=target):
                link = self.tree / 'link'
                link.symlink_to(target)
                self.assertNotEqual(self.scan().returncode, 0)
                link.unlink()

    def test_fifo_and_socket_rejected(self):
        special = self.tree / 'special'
        os.mkfifo(special)
        self.assertNotEqual(self.scan().returncode, 0)
        special.unlink()
        with socket.socket(socket.AF_UNIX) as sock:
            sock.bind(str(special))
            self.assertNotEqual(self.scan().returncode, 0)

    def test_invalid_filename_bytes_rejected_before_delimiter_conversion(self):
        for name in ['a b', 'a\nb', 'a\rb', '中文', 'a\\b', 'a..b']:
            with self.subTest(name=name):
                path = self.tree / name
                path.write_text('x')
                self.assertNotEqual(self.scan().returncode, 0)
                path.unlink()

    def test_find_partial_failure_is_not_masked(self):
        self.shim('find', 'printf "%s\\000" "$1"; exit 23')
        self.assertNotEqual(self.scan().returncode, 0)
        self.assertFalse(self.output.exists())

    def test_find_success_without_root_is_rejected(self):
        self.shim('find', 'exit 0')
        self.assertNotEqual(self.scan().returncode, 0)
        (self.tree / 'file').write_text('x')
        self.shim('find', 'printf "%s\\000" "$1/file"')
        self.assertNotEqual(self.scan().returncode, 0)

    def test_unterminated_nul_record_is_rejected(self):
        self.shim('find', 'printf "%s" "$1"')
        self.assertNotEqual(self.scan().returncode, 0)

    def test_sort_and_tr_failures_preserve_previous_output(self):
        self.output.write_text('prior')
        for command in ['sort', 'tr']:
            with self.subTest(command=command):
                self.shim(command, 'printf partial; exit 23')
                self.assertNotEqual(self.scan().returncode, 0)
                self.assertEqual(self.output.read_text(), 'prior')
                (self.bin / command).unlink()

    def test_output_symlink_is_rejected(self):
        target = self.root / 'prior'
        target.write_text('prior')
        self.output.symlink_to(target)
        self.assertNotEqual(self.scan().returncode, 0)
        self.assertEqual(target.read_text(), 'prior')

    def test_empty_journal_is_recoverable_without_touching_other_files(self):
        journal = self.root / 'journal'
        journal.mkdir()
        result = self.call('JOURNAL=$2; local_barrier() { return 0; }; recover_transaction', journal)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(journal.exists())

    def test_nonempty_uninitialized_journal_is_preserved(self):
        journal = self.root / 'journal'
        journal.mkdir()
        (journal / 'unknown').write_text('preserve')
        result = self.call('JOURNAL=$2; local_barrier() { return 0; }; recover_transaction', journal)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual((journal / 'unknown').read_text(), 'preserve')

    def test_journal_symlinks_and_special_state_rejected(self):
        journal = self.root / 'journal'
        journal.symlink_to(self.tree)
        self.assertNotEqual(self.call('JOURNAL=$2; recover_transaction', journal).returncode, 0)
        journal.unlink()
        journal.mkdir()
        os.mkfifo(journal / 'state')
        self.assertNotEqual(self.call('JOURNAL=$2; recover_transaction', journal).returncode, 0)

    def test_factory_recovery_files_cannot_be_package_targets(self):
        for path in ['usr/local/bin/un260_update_dispatch', 'usr/local/lib/un260-updater/rescue/engine.sh',
                     'etc/un260/updater-platform']:
            with self.subTest(path=path):
                self.assertNotEqual(self.call('is_allowed_target "$2"', path).returncode, 0)
        self.assertEqual(self.call('is_allowed_target usr/local/lib/un260-startup/runtime.sh').returncode, 0)

if __name__ == '__main__':
    unittest.main(verbosity=2)
