#!/usr/bin/env python3
"""Native guard tests on host filesystem; not board USB power-cut validation."""
import functools
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile
import time
import unittest

SDK = Path(__file__).resolve().parents[2]
_build = tempfile.TemporaryDirectory(prefix='un260-storage-sync-')

@functools.lru_cache()
def native_tool():
    supplied = os.environ.get('UN260_TEST_SYNC_BINARY')
    if supplied:
        binary = Path(supplied)
        if not binary.is_file(): raise RuntimeError('Missing explicit native test binary: ' + supplied)
        return binary
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
    def test_regular_hash_boundaries_and_probe_compatibility(self):
        with tempfile.TemporaryDirectory() as temp:
            file = Path(temp) / 'local-script'
            for size in [0, 1, 511, 4096, 65535, 65536, 65537, 131073]:
                data = (bytes(range(256)) * 513)[:size]
                file.write_bytes(data)
                result = subprocess.run([native_tool(), '--hash-file', file],
                                        capture_output=True, text=True, timeout=3)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, hashlib.sha256(data).hexdigest() + '\n')
            for option, expected in [('--probe', 'UN260_STORAGE_SYNC_1'),
                                     ('--probe-stream', 'UN260_STORAGE_STREAM_1'),
                                     ('--probe-file', 'UN260_STORAGE_FILE_1')]:
                result = subprocess.run([native_tool(), option], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0)
                self.assertEqual(result.stdout, expected + '\n')

    def test_regular_hash_size_limit_and_unsafe_shapes_without_blocking(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'file'; file.write_bytes(b'valid')
            link = p / 'link'; link.symlink_to(file)
            fifo = p / 'fifo'; os.mkfifo(fifo)
            large = p / 'large'
            with large.open('wb') as stream: stream.truncate(64 * 1024 * 1024 + 1)
            for path in [link, fifo, p, p / 'missing', large, Path('/dev/null'), Path('/dev/zero')]:
                with self.subTest(path=path):
                    result = subprocess.run([native_tool(), '--hash-file', path],
                                            capture_output=True, timeout=3)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(result.stdout, b'')
            with large.open('wb') as stream: stream.truncate(64 * 1024 * 1024)
            result = subprocess.run([native_tool(), '--hash-file', large],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), hashlib.sha256(large.read_bytes()).hexdigest())
            for arguments in [['--hash-file'], ['--hash-file', file, 'extra']]:
                self.assertEqual(subprocess.run([native_tool()] + arguments,
                                               capture_output=True).returncode, 2)

    def test_regular_hash_works_when_direct_io_is_refused_without_usb_fallback(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'runtime'; file.write_bytes(b'#!/bin/sh\nexit 0\n')
            source = p / 'direct-refused.c'; library = p / 'direct-refused.so'
            source.write_text(r'''
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <dlfcn.h>
int open(const char *path, int flags, ...) {
    int (*real_open)(const char*,int,...) = dlsym(RTLD_NEXT,"open");
    if(flags & O_DIRECT) { errno=EINVAL; return -1; }
    if(flags & O_CREAT) {
        va_list args; va_start(args,flags); mode_t mode=va_arg(args,int); va_end(args);
        return real_open(path,flags,mode);
    }
    return real_open(path,flags);
}
int __open_2(const char *path, int flags) {
    int (*real_open_2)(const char*,int) = dlsym(RTLD_NEXT,"__open_2");
    if(flags & O_DIRECT) { errno=EINVAL; return -1; }
    return real_open_2(path,flags);
}
''')
            subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-ldl', '-o', str(library)], check=True)
            environment = {**os.environ, 'LD_PRELOAD': str(library)}
            direct = subprocess.run([native_tool(), '--hash-direct', file], env=environment,
                                    capture_output=True, text=True)
            self.assertNotEqual(direct.returncode, 0)
            self.assertEqual(direct.stdout, '')
            self.assertIn('Invalid argument', direct.stderr)
            regular = subprocess.run([native_tool(), '--hash-file', file], env=environment,
                                     capture_output=True, text=True)
            self.assertEqual(regular.returncode, 0, regular.stderr)
            self.assertEqual(regular.stdout.strip(), hashlib.sha256(file.read_bytes()).hexdigest())

    def test_regular_hash_rejects_path_replacement_size_change_and_false_eof(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'runtime'
            source = p / 'mutation.c'; library = p / 'mutation.so'
            source.write_text(r'''
#define _GNU_SOURCE
#include <unistd.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
ssize_t read(int fd, void *buf, size_t size) {
    ssize_t (*real_read)(int,void*,size_t) = dlsym(RTLD_NEXT,"read");
    static int watched=-1, changed;
    const char *wanted=getenv("UN260_TEST_READ_PATH"), *mode=getenv("UN260_TEST_READ_MODE");
    if(watched<0 && wanted) {
        char link[64],path[4096]; snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
        ssize_t n=readlink(link,path,sizeof(path)-1);
        if(n>=0) { path[n]=0; if(!strcmp(path,wanted))watched=fd; }
    }
    if(fd==watched && !changed) {
        if(!strcmp(mode,"extra") && size==1) { changed=1; *(char*)buf='x'; return 1; }
        if(!strcmp(mode,"replace")) {
            char saved[4096]; snprintf(saved,sizeof(saved),"%s.old",wanted); rename(wanted,saved);
            int out=open(wanted,O_WRONLY|O_CREAT|O_EXCL,0600); if(out>=0)close(out); changed=1;
        } else if(!strcmp(mode,"truncate")) { truncate(wanted,1); changed=1; }
        else if(!strcmp(mode,"append")) {
            int out=open(wanted,O_WRONLY|O_APPEND); if(out>=0) { write(out,"x",1); close(out); } changed=1;
        }
    }
    return real_read(fd,buf,size);
}
''')
            subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-ldl', '-o', str(library)], check=True)
            for mode in ['replace', 'truncate', 'append', 'extra']:
                file.write_bytes(b'x' * 131072)
                result = subprocess.run([native_tool(), '--hash-file', file],
                    env={**os.environ, 'LD_PRELOAD': str(library), 'UN260_TEST_READ_PATH': str(file),
                         'UN260_TEST_READ_MODE': mode}, capture_output=True, timeout=3)
                self.assertNotEqual(result.returncode, 0, mode)
                self.assertEqual(result.stdout, b'', mode)

    def test_regular_hash_missing_sha_helper_never_publishes_digest(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp); file = p / 'runtime'; file.write_bytes(b'x' * 131072)
            source = p / 'missing-helper.c'; library = p / 'missing-helper.so'
            source.write_text('#include <errno.h>\nint execl(const char*p,const char*a,...){(void)p;(void)a;errno=ENOENT;return -1;}\n')
            subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-o', str(library)], check=True)
            result = subprocess.run([native_tool(), '--hash-file', file],
                                    env={**os.environ, 'LD_PRELOAD': str(library)},
                                    capture_output=True, timeout=3)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b'')

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
            for method in ['--hash-stream', '--hash-file', '--hash-fnv64']:
                for mode in ['error', 'short', 'interrupt']:
                    args = [native_tool(), method, file] + (['131072'] if method == '--hash-stream' else [])
                    r = subprocess.run(args,
                        env={**os.environ, 'LD_PRELOAD': str(library), 'UN260_TEST_READ_PATH': str(file),
                             'UN260_TEST_READ_MODE': mode}, capture_output=True, text=True)
                    if mode == 'interrupt':
                        self.assertEqual(r.returncode, 0, r.stderr)
                        expected = (hashlib.sha256(file.read_bytes()).hexdigest() if method != '--hash-fnv64'
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


class LockTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-native-lock-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.lock = self.root / 'gate'
        self.script = self.root / 'script'
        self.marker = self.root / 'pid'
        self.release = self.root / 'release'
        self.children = []
        self.addCleanup(self.stop_children)

    def stop_children(self):
        for process in self.children:
            if process.poll() is None: process.kill()
            process.wait(timeout=3)
            if process.stdout: process.stdout.close()
            if process.stderr: process.stderr.close()
        time.sleep(0.03)

    def start_holder(self):
        self.script.write_text("""#!/bin/sh
printf '%s\\n' "$$" > "$1"
"$2" --lock-held "$3" || exit 12
printf '%s\\n' "$4" "$5" > "$1.args"
while [ ! -e "$6" ]; do /bin/sleep 0.02; done
""")
        process = subprocess.Popen([native_tool(), '--lock-run', self.lock, self.script,
                                    self.marker, native_tool(), self.lock,
                                    'a value with spaces', '$(never_execute)', self.release],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.children.append(process)
        deadline = time.monotonic() + 3
        while not Path(str(self.marker) + '.args').exists() and time.monotonic() < deadline:
            if process.poll() is not None: self.fail(process.communicate())
            time.sleep(0.005)
        self.assertTrue(Path(str(self.marker) + '.args').exists())
        return process

    def test_lock_run_same_pid_inherited_fd6_argv_and_probe(self):
        process = self.start_holder()
        self.assertEqual(self.marker.read_text(), str(process.pid) + '\n')
        self.assertEqual(Path(str(self.marker) + '.args').read_text(),
                         'a value with spaces\n$(never_execute)\n')
        self.assertEqual(self.lock.stat().st_mode & 0o7777, 0o600)
        self.release.touch()
        self.assertEqual(process.wait(timeout=3), 0)
        probe = subprocess.run([native_tool(), '--probe'], capture_output=True, text=True)
        self.assertEqual(probe.stdout, 'UN260_STORAGE_SYNC_1\n')
        self.assertEqual(probe.returncode, 0)

    def test_real_concurrent_busy75_and_SIGKILL_kernel_release(self):
        process = self.start_holder()
        other = subprocess.run([native_tool(), '--lock-run', self.lock, self.script],
                               capture_output=True, timeout=3)
        self.assertEqual(other.returncode, 75)
        process.kill(); process.wait(timeout=3)
        check = self.root / 'after'; check.write_text('printf "released\\n"\n')
        deadline = time.monotonic() + 3
        while True:
            result = subprocess.run([native_tool(), '--lock-run', self.lock, check],
                                    capture_output=True, text=True, timeout=3)
            if result.returncode != 75 or time.monotonic() >= deadline: break
            time.sleep(0.01)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, 'released\n')
        self.assertTrue(self.lock.exists())

    def test_lock_held_rejects_absent_and_unlocked_fresh_fd6(self):
        self.lock.touch(mode=0o600)
        no_fd = subprocess.run([native_tool(), '--lock-held', self.lock], capture_output=True)
        self.assertNotEqual(no_fd.returncode, 0)
        result = subprocess.run(['/bin/sh', '-c', 'exec 6<>"$1"; exec "$2" --lock-held "$1"',
                                 'fixture', self.lock, native_tool()], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.script.write_text('exit 0\n')
        self.assertEqual(subprocess.run([native_tool(), '--lock-run', self.lock, self.script],
                                       capture_output=True).returncode, 0)

    def test_foreign_holder_cannot_be_bypassed_by_new_fd6(self):
        self.start_holder()
        result = subprocess.run(['/bin/sh', '-c', 'exec 6<>"$1"; exec "$2" --lock-held "$1"',
                                 'fixture', self.lock, native_tool()], capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_lock_held_different_inode_and_replaced_path(self):
        other = self.root / 'other'; other.touch(mode=0o600)
        process = self.start_holder()
        result = subprocess.run(['/bin/sh', '-c', 'exec 6<>"$1"; exec "$2" --lock-held "$3"',
                                 'fixture', other, native_tool(), self.lock], capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.release.touch(); self.assertEqual(process.wait(timeout=3), 0)
        self.script.write_text("""mv "$1" "$1.old"
: > "$1"; chmod 0600 "$1"
if "$2" --lock-held "$1"; then exit 9; fi
exit 0
""")
        result = subprocess.run([native_tool(), '--lock-run', self.lock, self.script,
                                 self.lock, native_tool()], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_symlink_parent_shape_modes_and_hardlink_reject_without_exec(self):
        self.script.write_text('touch "$1"\n')
        valid = self.root / 'valid'; valid.touch(mode=0o600)
        link = self.root / 'link'; link.symlink_to(valid)
        directory = self.root / 'directory'; directory.mkdir()
        fifo = self.root / 'fifo'; os.mkfifo(fifo, 0o600)
        badmode = self.root / 'badmode'; badmode.touch(mode=0o644)
        special = self.root / 'special'; special.touch(mode=0o600); special.chmod(0o4600)
        alias = self.root / 'alias'; os.link(valid, alias)
        parent = self.root / 'parent'; parent.symlink_to(self.root, target_is_directory=True)
        for path in [link, directory, fifo, badmode, special, alias, parent / 'gate', Path('/dev/null')]:
            with self.subTest(path=path):
                output = self.root / 'unexpected'
                result = subprocess.run([native_tool(), '--lock-run', path, self.script, output],
                                        capture_output=True, timeout=3)
                self.assertNotEqual(result.returncode, 0)
                self.assertFalse(output.exists())

    def test_invalid_arguments_and_unsafe_components(self):
        self.script.write_text('exit 0\n')
        for arguments in [
            ['--lock-run'], ['--lock-held'], ['--lock-held', self.lock, 'extra'],
            ['--lock-run', 'relative', self.script], ['--lock-run', self.lock, '-c', 'exit 0'],
            ['--lock-run', str(self.root) + '/./gate', self.script],
            ['--lock-run', str(self.root) + '/../gate', self.script],
            ['--lock-run', str(self.root) + '//gate', self.script],
            ['--lock-run', str(self.root) + '/', self.script],
        ]:
            with self.subTest(arguments=arguments):
                result = subprocess.run([native_tool()] + arguments, capture_output=True, timeout=3)
                self.assertNotEqual(result.returncode, 0)

    def test_missing_script_failure_drops_gate(self):
        result = subprocess.run([native_tool(), '--lock-run', self.lock, self.root / 'absent'],
                                capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.script.write_text('exit 0\n')
        self.assertEqual(subprocess.run([native_tool(), '--lock-run', self.lock, self.script],
                                       capture_output=True).returncode, 0)

if __name__ == '__main__': unittest.main()
