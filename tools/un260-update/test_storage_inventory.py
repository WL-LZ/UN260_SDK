#!/usr/bin/env python3
"""Batch inventory checks on host filesystems; not board power-cut validation."""
import functools
import hashlib
import json
import os
import shlex
from pathlib import Path
import subprocess
import tempfile
import unittest

BUILD_FLAGS = ['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror'] + shlex.split(os.environ.get('UN260_TEST_CFLAGS', '-O2'))
SDK = Path(__file__).resolve().parents[2]
SOURCE = SDK / 'source/artinchip/test-lvgl/tools/un260_storage_sync.c'
MEMBER_MAX = 24 * 1024 * 1024
PAYLOAD_MAX = 32 * 1024 * 1024
_build = tempfile.TemporaryDirectory(prefix='un260-storage-inventory-')

WRAPPER = r'''#!/usr/bin/python3
import json, os, signal, subprocess, sys
from pathlib import Path
action = os.environ.get('INV_TEST_ACTION', '')
target = Path(os.environ.get('INV_TEST_TARGET', '/no-test-target'))
args = sys.argv[1:]
fds = tuple(int(p.rsplit('/', 1)[1]) for p in args)
marker = os.environ.get('INV_TEST_MARKER')
call = 1
if marker:
    p = Path(marker)
    old = p.read_text().splitlines() if p.exists() else []
    call = len(old) + 1
    with p.open('a') as stream:
        stream.write(json.dumps({'fds': len(fds), 'paths': args}) + '\n')
if action == 'second-batch-change' and call == 2:
    target.write_bytes(b'late mutation')
elif action == 'content':
    data = target.read_bytes()
    stamp = target.stat().st_mtime_ns
    target.write_bytes(bytes([data[0] ^ 1]) + data[1:])
    os.utime(target, ns=(target.stat().st_atime_ns, stamp))
elif action == 'size':
    with target.open('ab') as stream: stream.write(b'x')
elif action == 'mode':
    target.chmod(0o600)
elif action in ['replace', 'table-replace']:
    data = target.read_bytes()
    mode = target.stat().st_mode & 0o7777
    target.rename(str(target) + '.old')
    target.write_bytes(data)
    target.chmod(mode)
elif action == 'symlink':
    target.rename(str(target) + '.old')
    target.symlink_to(str(target) + '.old')
elif action == 'directory-symlink':
    target.rename(str(target) + '.old')
    target.symlink_to(str(target) + '.old', target_is_directory=True)
elif action == 'anchor-replace':
    target.rename(str(target) + '.old')
    target.mkdir()
    for child in Path(str(target) + '.old').iterdir():
        child.rename(target / child.name)
elif action == 'table-edit':
    stamp = target.stat().st_mtime_ns
    data = bytearray(target.read_bytes())
    data[0] = ord('0') if data[0] != ord('0') else ord('1')
    target.write_bytes(data)
    os.utime(target, ns=(target.stat().st_atime_ns, stamp))
elif action == 'missing-create':
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(b'created')
elif action == 'owner-marker':
    target.touch()
elif action == 'signal':
    os.kill(os.getpid(), signal.SIGKILL)
if action == 'empty':
    sys.exit(0)
real = subprocess.run(['/usr/bin/sha256sum'] + args, pass_fds=fds, capture_output=True)
data = real.stdout
if action == 'partial':
    data = data.splitlines(keepends=True)[0]
elif action == 'bad-hash':
    data = b'g' + data[1:]
elif action == 'bad-name':
    data = data.replace(b'/proc/self/fd/', b'/proc/other/fd/', 1)
elif action == 'reorder':
    data = b''.join(reversed(data.splitlines(keepends=True)))
elif action == 'extra':
    data += b'junk\n'
elif action == 'overflow':
    data += b'x' * 10000
sys.stdout.buffer.write(data)
sys.exit(19 if action == 'nonzero' else real.returncode)
'''

@functools.lru_cache()
def native_tools():
    root = Path(_build.name)
    native = root / 'un260_storage_sync'
    subprocess.run(BUILD_FLAGS + [str(SOURCE), '-o', str(native)], check=True)
    wrapper = root / 'batch-sha256sum'
    wrapper.write_text(WRAPPER)
    wrapper.chmod(0o755)
    injected = root / 'injected.c'
    original = SOURCE.read_text()
    needle = 'execv("/usr/bin/sha256sum", arguments);'
    if original.count(needle) != 1:
        raise RuntimeError('Inventory sha256sum test hook not unique')
    injected.write_text(original.replace(needle, 'execv(' + json.dumps(str(wrapper)) + ', arguments);'))
    test_binary = root / 'injected'
    subprocess.run(BUILD_FLAGS + [str(injected), '-o', str(test_binary)], check=True)
    missing = root / 'missing.c'
    missing.write_text(original.replace(needle, 'execv("/no-such-un260-sha256sum", arguments);'))
    missing_binary = root / 'missing'
    subprocess.run(BUILD_FLAGS + [str(missing), '-o', str(missing_binary)], check=True)
    return native, test_binary, missing_binary

class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-inventory-case-')
        self.addCleanup(self.temp.cleanup)
        self.home = Path(self.temp.name)
        self.root = self.home / 'root'
        self.root.mkdir()
        self.app = self.home / 'app'
        self.app.mkdir()
        self.table = self.home / 'inventory.tsv'
        self.entries = {}

    def add(self, path, data=b'payload', mode=0o644, mapped=False):
        file = (self.app / 'test_lvgl') if mapped else (self.root / path)
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(data)
        file.chmod(mode)
        self.entries[path] = (hashlib.sha256(data).hexdigest(), mode, len(data))
        return file

    def write_table(self, entries=None):
        entries = self.entries if entries is None else entries
        text = ''.join('%s|%04o|%d|%s\n' % (*entries[path], path) for path in sorted(entries))
        self.table.write_text(text)
        return text

    def run_tool(self, option='--verify-inventory', app='-', binary=None, env=None, table=None):
        binary = native_tools()[0] if binary is None else binary
        args = [str(binary), option, str(self.root)]
        if option != '--verify-payload':
            args.append(str(self.app) if app == 'mapped' else str(app))
        args.append(str(self.table if table is None else table))
        return subprocess.run(args, capture_output=True, text=True,
                              env={**os.environ, **(env or {})}, timeout=20)

    def assert_failed(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout, '', 'No partial inventory is usable on failure')

    def test_probe_true_hashes_modes_empty_and_boundaries(self):
        result = subprocess.run([native_tools()[0], '--probe-inventory'], capture_output=True, text=True)
        self.assertEqual((result.returncode, result.stdout), (0, 'UN260_STORAGE_INVENTORY_1\n'))
        for i, size in enumerate([0, 1, 511, 4096, 65535, 65536, 65537, 131073]):
            self.add('files/f%03d' % i, (bytes(range(256)) * 513)[:size],
                     0o755 if i % 2 else 0o644)
        expected = self.write_table()
        inspect = self.run_tool('--inspect-inventory')
        self.assertEqual(inspect.returncode, 0, inspect.stderr)
        self.assertEqual(inspect.stdout, expected)
        for option in ['--verify-inventory', '--verify-payload']:
            result = self.run_tool(option)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout, '')

    def test_bounded_batches_and_sorted_output(self):
        for i in range(145):
            self.add('files/f%03d' % i, ('row-%d' % i).encode())
        expected = self.write_table()
        marker = self.home / 'calls.jsonl'
        result = self.run_tool('--inspect-inventory', binary=native_tools()[1],
                               env={'INV_TEST_MARKER': str(marker)})
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, expected)
        calls = [json.loads(line) for line in marker.read_text().splitlines()]
        self.assertEqual([call['fds'] for call in calls], [64, 64, 17])
        self.assertTrue(all(path.startswith('/proc/self/fd/') for call in calls for path in call['paths']))

    def test_app_mapping_is_only_exact_application_path(self):
        self.add('usr/local/bin/test_lvgl', b'app-volume', 0o755, mapped=True)
        self.add('usr/local/bin/other', b'root-other', 0o755)
        self.add('usr/local/lib/liblvgl.so', b'root-library', 0o755)
        (self.root / 'usr/local/bin/test_lvgl').write_bytes(b'wrong-root-app')
        expected = self.write_table()
        result = self.run_tool('--inspect-inventory', app='mapped')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, expected)
        self.assertEqual(self.run_tool(app='mapped').returncode, 0)
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))

    def test_missing_terminal_and_ancestor_are_inspection_only(self):
        self.add('present/member')
        self.entries['absent-parent/a'] = ('a' * 64, 0o644, 10)
        self.entries['present/missing'] = ('b' * 64, 0o755, 0)
        self.write_table()
        result = self.run_tool('--inspect-inventory')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.splitlines()[0], '-|0000|0|absent-parent/a')
        self.assertEqual(result.stdout.splitlines()[2], '-|0000|0|present/missing')
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))

    def test_unsafe_objects_and_anchors_never_fall_back_to_missing(self):
        file = self.add('files/member')
        self.write_table()
        for shape in ['terminal-link', 'parent-link', 'fifo', 'directory', 'device-link', 'parent-file']:
            with self.subTest(shape=shape):
                for child in (self.root / 'files').iterdir():
                    child.unlink()
                if shape == 'terminal-link':
                    file.symlink_to(self.home / 'missing')
                elif shape == 'parent-link':
                    (self.root / 'files').rmdir()
                    (self.root / 'files').symlink_to(self.home / 'missing', target_is_directory=True)
                elif shape == 'fifo':
                    os.mkfifo(file)
                elif shape == 'directory':
                    file.mkdir()
                elif shape == 'device-link':
                    file.symlink_to('/dev/zero')
                else:
                    (self.root / 'files').rmdir()
                    (self.root / 'files').write_bytes(b'not-directory')
                for option in ['--inspect-inventory', '--verify-inventory', '--verify-payload']:
                    self.assert_failed(self.run_tool(option))
                parent = self.root / 'files'
                if parent.is_symlink() or parent.is_file():
                    parent.unlink(); parent.mkdir()
                else:
                    if file.is_dir(): file.rmdir()
                    elif file.exists() or file.is_symlink(): file.unlink()
                file.write_bytes(b'payload'); file.chmod(0o644)
        link = self.home / 'root-link'
        link.symlink_to(self.root, target_is_directory=True)
        result = subprocess.run([native_tools()[0], '--inspect-inventory', link, '-', self.table],
                                capture_output=True, text=True, timeout=3)
        self.assert_failed(result)
        self.assert_failed(self.run_tool('--inspect-inventory', app=link))
        relative_root = subprocess.run([native_tools()[0], '--inspect-inventory', '.', '-', self.table],
                                       capture_output=True, text=True, timeout=3)
        self.assert_failed(relative_root)
        table_link = self.home / 'table-link'; table_link.symlink_to(self.table)
        self.assert_failed(self.run_tool('--inspect-inventory', table=table_link))
        link_parent = self.home / 'table-parent-link'
        link_parent.symlink_to(self.home, target_is_directory=True)
        self.assert_failed(self.run_tool('--inspect-inventory', table=link_parent / self.table.name))
        self.assert_failed(self.run_tool('--inspect-inventory', app=self.home / 'absent-app'))
        table_fifo = self.home / 'table-fifo'; os.mkfifo(table_fifo)
        self.assert_failed(self.run_tool('--inspect-inventory', table=table_fifo))

    def test_strict_table_syntax_and_path_limits(self):
        self.add('files/a')
        self.add('files/b')
        valid = self.write_table()
        malformed = ['', valid.rstrip('\n'), valid + '\n', valid.replace('|0644|', '|644|'),
                     valid.replace('|0644|', '|0600|'), valid.replace('|7|', '|07|'),
                     valid.replace('|7|', '|-7|'), valid.replace('|7|', '|7x|'),
                     valid.replace('|7|', '|99999999999999999999999999|'),
                     valid.replace('|files/a', '|files/a|extra'),
                     valid.replace('|files/a', '|\x00files/a'),
                     valid.replace(valid[:64], valid[:64].upper(), 1),
                     valid.replace(valid[:64], 'z' * 64, 1),
                     valid.splitlines(keepends=True)[0] * 2,
                     ''.join(reversed(valid.splitlines(keepends=True)))]
        for path in ['', '/absolute', '../outside', 'a/../b', 'a..b', 'a//b',
                     'a/./b', './b', 'a/', 'a\\b', 'a b', 'a\tb', 'a\rb',
                     'a|b', 'a@b', 'a/.un260-new', 'a/x.un260-old', 'x' * 256,
                     '/'.join(['x' * 255] * 17)]:
            malformed.append('%s|0644|0|%s\n' % ('a' * 64, path))
        for text in malformed:
            with self.subTest(table=repr(text[:120])):
                self.table.write_bytes(text.encode())
                for option in ['--inspect-inventory', '--verify-inventory', '--verify-payload']:
                    self.assert_failed(self.run_tool(option))

    def test_canonical_path_255_byte_boundary(self):
        self.entries['x' * 255] = ('a' * 64, 0o644, 0)
        self.write_table()
        result = self.run_tool('--inspect-inventory')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, '-|0000|0|' + 'x' * 255 + '\n')
        self.entries.clear()
        self.entries['x/' + 'y' * 254] = ('a' * 64, 0o644, 0)
        self.write_table()
        self.assert_failed(self.run_tool('--inspect-inventory'))

    def test_table_row_and_byte_limits(self):
        entries = {'missing/f%05d' % i: ('a' * 64, 0o644, 0) for i in range(8192)}
        self.write_table(entries)
        result = self.run_tool('--inspect-inventory')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(result.stdout.splitlines()), 8192)
        entries['missing/f08192'] = ('a' * 64, 0o644, 0)
        self.write_table(entries)
        self.assert_failed(self.run_tool('--inspect-inventory'))
        self.table.write_bytes(b'x' * (3 * 1024 * 1024 + 1))
        self.assert_failed(self.run_tool('--inspect-inventory'))

    def test_expected_budget_limits_and_larger_old_files_are_inspectable(self):
        big = self.root / 'large'; big.touch()
        with big.open('wb') as stream: stream.truncate(MEMBER_MAX)
        big.chmod(0o644)
        self.entries['large'] = (hashlib.sha256(big.read_bytes()).hexdigest(), 0o644, MEMBER_MAX)
        rest = self.root / 'rest'; rest.touch()
        with rest.open('wb') as stream: stream.truncate(PAYLOAD_MAX - MEMBER_MAX)
        rest.chmod(0o644)
        self.entries['rest'] = (hashlib.sha256(rest.read_bytes()).hexdigest(), 0o644, rest.stat().st_size)
        self.write_table()
        self.assertEqual(self.run_tool('--verify-payload').returncode, 0)
        with big.open('ab') as stream: stream.write(b'x')
        inspect = self.run_tool('--inspect-inventory')
        self.assertEqual(inspect.returncode, 0, inspect.stderr)
        self.assertIn('|%d|large\n' % (MEMBER_MAX + 1), inspect.stdout)
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))
        with big.open('r+b') as stream: stream.truncate(MEMBER_MAX)
        with rest.open('ab') as stream: stream.write(b'x')
        inspect = self.run_tool('--inspect-inventory')
        self.assertEqual(inspect.returncode, 0, inspect.stderr)
        self.assertIn('|%d|rest\n' % (PAYLOAD_MAX - MEMBER_MAX + 1), inspect.stdout)
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))
        self.entries['large'] = ('a' * 64, 0o644, MEMBER_MAX + 1)
        self.write_table()
        self.assert_failed(self.run_tool('--inspect-inventory'))
        self.entries['large'] = ('a' * 64, 0o644, MEMBER_MAX)
        self.entries['rest'] = ('b' * 64, 0o644, PAYLOAD_MAX - MEMBER_MAX + 1)
        self.write_table()
        self.assert_failed(self.run_tool('--inspect-inventory'))

    def test_content_size_and_mode_mismatches_and_transport_mode_ignored(self):
        file = self.add('files/member', b'payload')
        self.write_table()
        file.write_bytes(b'payloaX')
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))
        file.write_bytes(b'payload-more')
        self.assert_failed(self.run_tool())
        file.write_bytes(b'payload')
        file.chmod(0o600)
        self.assert_failed(self.run_tool())
        result = self.run_tool('--verify-payload')
        self.assertEqual(result.returncode, 0, result.stderr)
        inspect = self.run_tool('--inspect-inventory')
        self.assertEqual(inspect.returncode, 0, inspect.stderr)
        self.assertIn('|0600|7|files/member\n', inspect.stdout)

    def test_stable_transport_or_installed_owner_and_owner_mutation(self):
        self.add('files/member')
        self.write_table()
        preload = self.home / 'owner.c'
        preload.write_text(r'''
#define _GNU_SOURCE
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int alter_owner(int fd,struct stat *st,int rc) {
    char proc[64], path[4096];
    snprintf(proc,sizeof(proc),"/proc/self/fd/%d",fd);
    ssize_t n=readlink(proc,path,sizeof(path)-1);
    if(!rc && n>0) {path[n]=0; if(strstr(path,"/files/member") && (!getenv("INV_OWNER_MARKER") || access(getenv("INV_OWNER_MARKER"),F_OK)==0))st->st_uid=geteuid()+1;}
    return rc;
}
int fstat(int fd,struct stat *st) {
    int (*real_fstat)(int,struct stat*) = dlsym(RTLD_NEXT,"fstat");
    return alter_owner(fd,st,real_fstat(fd,st));
}
int __fxstat(int version,int fd,struct stat *st) {
    int (*real_fxstat)(int,int,struct stat*) = dlsym(RTLD_NEXT,"__fxstat");
    return alter_owner(fd,st,real_fxstat(version,fd,st));
}
''')
        library = self.home / 'owner.so'
        subprocess.run(['gcc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                        str(preload), '-ldl', '-o', str(library)], check=True)
        env = {'LD_PRELOAD': str(library)}
        self.assertEqual(self.run_tool(env=env).returncode, 0)
        self.assertEqual(self.run_tool('--verify-payload', env=env).returncode, 0)
        self.assertEqual(self.run_tool('--inspect-inventory', env=env).returncode, 0)
        for option in ['--inspect-inventory', '--verify-inventory', '--verify-payload']:
            marker = self.home / ('owner-change-' + option[2:])
            environment = {**env, 'INV_OWNER_MARKER': str(marker),
                           'INV_TEST_ACTION': 'owner-marker', 'INV_TEST_TARGET': str(marker)}
            self.assert_failed(self.run_tool(option, binary=native_tools()[1], env=environment))

    def test_inspect_reads_large_legacy_member_beyond_old_single_file_limit(self):
        file = self.root / 'legacy'; file.touch(); file.chmod(0o644)
        size = 65 * 1024 * 1024 + 13
        with file.open('wb') as stream: stream.truncate(size)
        digest = hashlib.sha256()
        with file.open('rb') as stream:
            for chunk in iter(lambda: stream.read(65536), b''):
                digest.update(chunk)
        self.entries['legacy'] = ('a' * 64, 0o644, 1)
        self.write_table()
        inspect = self.run_tool('--inspect-inventory')
        self.assertEqual(inspect.returncode, 0, inspect.stderr)
        self.assertEqual(inspect.stdout, '%s|0644|%d|legacy\n' % (digest.hexdigest(), size))
        self.assert_failed(self.run_tool())
        self.assert_failed(self.run_tool('--verify-payload'))


    def test_mutations_during_hashing_are_rejected_in_every_mode(self):
        for action in ['content', 'size', 'mode', 'replace', 'symlink', 'directory-symlink',
                       'table-edit', 'table-replace', 'anchor-replace']:
            for option in ['--inspect-inventory', '--verify-inventory', '--verify-payload']:
                with self.subTest(action=action, option=option):
                    with tempfile.TemporaryDirectory(prefix='un260-inventory-race-') as temp:
                        root = Path(temp) / 'root'; root.mkdir()
                        file = root / 'files/member'; file.parent.mkdir()
                        file.write_bytes(b'payload'); file.chmod(0o644)
                        table = Path(temp) / 'inventory.tsv'
                        table.write_text('%s|0644|7|files/member\n' % hashlib.sha256(b'payload').hexdigest())
                        target = table if action in ['table-edit', 'table-replace'] else (
                            root if action == 'anchor-replace' else (
                                file.parent if action == 'directory-symlink' else file))
                        args = [native_tools()[1], option, root]
                        if option != '--verify-payload': args.append('-')
                        args.append(table)
                        result = subprocess.run(args, capture_output=True, text=True, timeout=10,
                            env={**os.environ, 'INV_TEST_ACTION': action, 'INV_TEST_TARGET': str(target)})
                        self.assert_failed(result)

    def test_late_batch_change_missing_creation_and_app_anchor_replacement(self):
        for i in range(65):
            self.add('files/f%03d' % i, b'payload')
        self.write_table()
        result = self.run_tool('--inspect-inventory', binary=native_tools()[1],
            env={'INV_TEST_ACTION': 'second-batch-change',
                 'INV_TEST_TARGET': str(self.root / 'files/f000'),
                 'INV_TEST_MARKER': str(self.home / 'calls')})
        self.assert_failed(result)
        self.entries.clear()
        self.add('z/member')
        self.entries['absent/member'] = ('a' * 64, 0o644, 0)
        self.write_table()
        result = self.run_tool('--inspect-inventory', binary=native_tools()[1],
            env={'INV_TEST_ACTION': 'missing-create', 'INV_TEST_TARGET': str(self.root / 'absent/member')})
        self.assert_failed(result)
        self.entries.clear()
        self.add('usr/local/bin/test_lvgl', b'app-volume', 0o755, mapped=True)
        self.write_table()
        result = self.run_tool('--inspect-inventory', app='mapped', binary=native_tools()[1],
            env={'INV_TEST_ACTION': 'anchor-replace', 'INV_TEST_TARGET': str(self.app)})
        self.assert_failed(result)

    def test_sha_process_failures_output_shape_and_order_are_rejected(self):
        self.add('files/a')
        self.add('files/b')
        self.write_table()
        for action in ['empty', 'partial', 'bad-hash', 'bad-name', 'reorder',
                       'extra', 'overflow', 'nonzero', 'signal']:
            for option in ['--inspect-inventory', '--verify-inventory', '--verify-payload']:
                with self.subTest(action=action, option=option):
                    self.assert_failed(self.run_tool(option, binary=native_tools()[1],
                                                     env={'INV_TEST_ACTION': action}))
        self.assert_failed(self.run_tool('--inspect-inventory', binary=native_tools()[2]))

if __name__ == '__main__':
    unittest.main(verbosity=2)
