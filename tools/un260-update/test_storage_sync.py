#!/usr/bin/env python3
"""Native guard tests on host filesystem; not board USB power-cut validation."""
import functools
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

SDK = Path(__file__).resolve().parents[2]
_build = tempfile.TemporaryDirectory(prefix='un260-storage-sync-')

@functools.lru_cache()
def native_tool():
    binary = Path(_build.name) / 'un260_storage_sync'
    subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2',
        str(SDK / 'source/artinchip/test-lvgl/tools/un260_storage_sync.c'), '-o', str(binary)], check=True)
    return binary

def ui_fingerprint(data):
    """Independent reference for the deployed UI's historical FNV seed."""
    value = 1469598103934665603
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return '%016x' % value

class SyncTests(unittest.TestCase):
    def test_fnv_matches_ui_across_read_boundaries(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'package'
            for size in [0, 1, 511, 4096, 65535, 65536, 65537, 131073]:
                data = (bytes(range(256)) * 513)[:size]
                file.write_bytes(data)
                r = subprocess.run([native_tool(), '--hash-fnv64', file], capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertEqual(r.stdout.strip(), ui_fingerprint(data))

    def test_fnv_rejects_unsafe_or_oversize_files(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'package'; file.write_bytes(b'package')
            link = p / 'link'; link.symlink_to(file)
            large = p / 'large'
            with large.open('wb') as stream: stream.truncate(64 * 1024 * 1024 + 1)
            for path in [link, p / 'absent', p, large, Path('/dev/null')]:
                r = subprocess.run([native_tool(), '--hash-fnv64', path], capture_output=True)
                self.assertNotEqual(r.returncode, 0)
                self.assertEqual(r.stdout, b'')

    def test_stream_hash_exact_lengths(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'source'
            for size in [0, 1, 511, 4096, 65535, 65536, 65537, 131072]:
                data = b'x' * size; file.write_bytes(data)
                r = subprocess.run([native_tool(), '--hash-stream', file, str(size)], capture_output=True, text=True)
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertEqual(r.stdout.strip(), hashlib.sha256(data).hexdigest())

    def test_stream_rejects_wrong_sizes_and_invalid_arguments(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'source'; file.write_bytes(b'1234')
            for count in ['0', '3', '5', '-1', '67108865', 'bad', '4x', '']:
                r = subprocess.run([native_tool(), '--hash-stream', file, count], capture_output=True)
                self.assertNotEqual(r.returncode, 0); self.assertEqual(r.stdout, b'')
            # Character devices have st_size=0; exact read + EOF are still mandatory.
            for path, count in [('/dev/null', '4'), ('/dev/zero', '4')]:
                r = subprocess.run([native_tool(), '--hash-stream', path, count], capture_output=True)
                self.assertNotEqual(r.returncode, 0); self.assertEqual(r.stdout, b'')

    def test_stream_read_error_short_read_and_interruption(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'source'; file.write_bytes(b'x' * 131072)
            source = p / 'read-failure.c'; library = p / 'read-failure.so'
            source.write_text(r'''
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
ssize_t read(int fd, void *buf, size_t size) {
    ssize_t (*real_read)(int,void*,size_t) = dlsym(RTLD_NEXT, "read");
    char link[64], path[4096];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t n = readlink(link, path, sizeof(path)-1);
    if(n >= 0) {
        path[n] = 0;
        const char *wanted = getenv("UN260_TEST_READ_PATH");
        if(wanted && !strcmp(wanted,path)) {
            static int calls;
            const char *mode = getenv("UN260_TEST_READ_MODE");
            if(!strcmp(mode,"error")) { errno=EIO; return -1; }
            if(!strcmp(mode,"short")) {
                if(calls++) return 0;
                return real_read(fd,buf,size < 17 ? size : 17);
            }
            if(!strcmp(mode,"interrupt") && !calls++) { errno=EINTR; return -1; }
        }
    }
    return real_read(fd,buf,size);
}
''')
            subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-ldl', '-o', str(library)], check=True)
            for method in ['--hash-stream', '--hash-fnv64']:
                for mode in ['error', 'short', 'interrupt']:
                    args = [native_tool(), method, file] + (['131072'] if method == '--hash-stream' else [])
                    r = subprocess.run(args,
                        env={**os.environ, 'LD_PRELOAD': str(library), 'UN260_TEST_READ_PATH': str(file),
                             'UN260_TEST_READ_MODE': mode}, capture_output=True, text=True)
                    if mode == 'interrupt':
                        self.assertEqual(r.returncode, 0, r.stderr)
                        expected = (hashlib.sha256(file.read_bytes()).hexdigest() if method == '--hash-stream'
                                    else ui_fingerprint(file.read_bytes()))
                        self.assertEqual(r.stdout.strip(), expected)
                    else:
                        self.assertNotEqual(r.returncode, 0); self.assertEqual(r.stdout, '')

    def test_direct_hash_boundaries(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'backup'
            for size in [0, 1, 511, 512, 4095, 4096, 65535, 65536, 65537, 131072]:
                data = b'x' * size; file.write_bytes(data)
                result = subprocess.run([native_tool(), '--hash-direct', file], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), hashlib.sha256(data).hexdigest())

    def test_reject_symlink_and_missing(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'source'; file.write_bytes(b'x')
            link = Path(temp) / 'link'; link.symlink_to(file)
            for path in [link, Path(temp) / 'absent', Path(temp)]:
                r = subprocess.run([native_tool(), '--hash-direct', path], capture_output=True)
                self.assertNotEqual(r.returncode, 0); self.assertEqual(r.stdout, b'')

    def test_barrier_held_fd_and_replaced_path(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / 'mount'; p.mkdir(); fd = os.open(p, os.O_RDONLY)
            try:
                args = [native_tool(), '--fd', str(fd), p]
                self.assertEqual(subprocess.run(args, pass_fds=[fd]).returncode, 0)
                p.rename(Path(temp) / 'old'); p.mkdir()
                self.assertNotEqual(subprocess.run(args, pass_fds=[fd], capture_output=True).returncode, 0)
            finally: os.close(fd)

    def test_syncfs_io_error_is_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); source = p / 'failure.c'; library = p / 'failure.so'
            source.write_text('#include <errno.h>\nint syncfs(int fd){(void)fd;errno=EIO;return -1;}\n')
            subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-o', str(library)], check=True)
            fd = os.open(p, os.O_RDONLY)
            try:
                r = subprocess.run([native_tool(), '--fd', str(fd), p], pass_fds=[fd],
                    env={**os.environ, 'LD_PRELOAD': str(library)}, capture_output=True)
                self.assertNotEqual(r.returncode, 0)
                self.assertIn(b'STORAGE_IO_FAILED', r.stderr)
            finally: os.close(fd)

if __name__ == '__main__': unittest.main()
