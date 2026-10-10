#!/usr/bin/env python3
"""Fixed entry selection and process/uptime liveness, using real files."""
import hashlib
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']


class BoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='un260-startup-boundary.')
        self.base = Path(self.temporary.name)
        self.rescue = self.base / 'rescue'
        self.rescue.mkdir()
        self.runtime = self.base / 'runtime.sh'
        self.descriptor = self.base / 'runtime.sha256'
        self.health = self.base / 'health'
        self.heartbeat = self.base / 'heartbeat'
        self.backend = self.base / 'backend'
        self.managed = self.base / 'managed-app'
        self.process = None
        # Selection-only fixture implements buffered file SHA, never USB direct.
        # A separate real-native EINVAL test verifies the filesystem/API boundary.
        sync = self.rescue / 'sync'
        sync.write_text('#!/bin/sh\n[ "$1" = --hash-file ] || exit 2\nsha256sum "$2" | cut -d" " -f1\n')
        sync.chmod(0o755)
        source = (HERE / 'startup_entry.sh').read_text()
        self.entry = self.base / 'entry-functions.sh'
        self.entry_script = self.base / 'entry.sh'
        self.calls = self.base / 'runtime-calls'
        self.watch_calls = self.base / 'watch-calls'
        self.stop_flag = self.base / 'watch-stop'
        source = source.replace('RESCUE=/usr/local/lib/un260-updater/rescue', 'RESCUE=' + str(self.rescue))
        source = source.replace('RUNTIME=/usr/local/lib/un260-startup/runtime.sh', 'RUNTIME=' + str(self.runtime))
        source = source.replace('/etc/un260/startup-runtime.sha256', str(self.descriptor))
        source = source.replace('WATCH_PID=/tmp/un260-startup-watch.pid',
                                'WATCH_PID=' + str(self.base / 'watch-pid'))
        source = source.replace('WATCH_STOP=/tmp/un260-startup-watch.stop',
                                'WATCH_STOP=' + str(self.stop_flag))
        # On the board /bin/sh is BusyBox; run nested shells with that same
        # provider in BusyBox mode rather than silently testing host dash.
        source = source.replace('/bin/sh ', shlex.join(SHELL) + ' ')
        source = source.replace('/tmp/un260-startup-watch.log',
                                str(self.base / 'watch.log'))
        source = source.replace('/tmp/un260-startup-runtime.log',
                                str(self.base / 'runtime.log'))
        self.entry_script.write_text(source)
        self.entry.write_text(source.split('case "${1:-}" in', 1)[0])
        watch = (HERE / 'startup_watch.sh').read_text()
        watch = watch.replace('RESCUE=/usr/local/lib/un260-updater/rescue', 'RESCUE=' + str(self.rescue))
        watch = watch.replace('HEALTH=/tmp/un260-ui.health', 'HEALTH=' + str(self.health))
        watch = watch.replace('HEARTBEAT=/tmp/un260-recovery.heartbeat', 'HEARTBEAT=' + str(self.heartbeat))
        watch = watch.replace('BACKEND_LOCK=/tmp/un260-updater.lock', 'BACKEND_LOCK=' + str(self.backend))
        watch = watch.replace('= drwx------:0 ]', '= drwx------:' + str(os.getuid()) + ' ]')
        watch = watch.replace('= -rw-------:0 ]', '= -rw-------:' + str(os.getuid()) + ' ]')
        watch = watch.replace('/mnt/un260-app/test_lvgl', str(self.managed))
        self.watch = self.base / 'watch-functions.sh'
        self.watch.write_text(watch)

    def tearDown(self):
        if self.process is not None:
            self.process.terminate()
            self.process.wait(timeout=5)
        self.temporary.cleanup()

    def call(self, source, body):
        return subprocess.run(SHELL + ['-c', 'UN260_STARTUP_WATCH_LIBRARY_ONLY=1; . ' +
                                      shlex.quote(str(source)) + '; ' + body],
                              capture_output=True, text=True, timeout=5)

    def pair(self, script, descriptor, data=b'#!/bin/sh\nexit 0\n'):
        script.write_bytes(data)
        descriptor.write_text(hashlib.sha256(data).hexdigest() + '\n')

    def rescue_pair(self):
        self.pair(self.rescue / 'startup.sh', self.rescue / 'startup.sha256')

    def invoke_entry(self, *arguments, environment=None):
        command = SHELL + [str(self.entry_script), *arguments]
        environment = dict(os.environ, UN260_BOOT_TRACE='0', **(environment or {}))
        process = subprocess.Popen(command, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True,
                                   env=environment)
        try:
            stdout, stderr = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate()
            raise
        result = subprocess.CompletedProcess(command, process.returncode,
                                             stdout, stderr)
        result.launched_pid = process.pid
        return result

    def command_runtime(self, marker, status=0):
        return ("#!/bin/sh\nprintf '%s|%s|%s|%s\\n' " + shlex.quote(marker) +
                ' "$#" "$1" "$$" > ' + shlex.quote(str(self.calls)) +
                '\nexit ' + str(status) + '\n').encode()

    def actual_runtime_fixture(self, failed=False):
        # Use the canonical runtime command handler and prepare_devices body.
        # Only hardware boundary functions are mocked; no real driver/mdev runs.
        runtime = (HERE.parents[1] /
                   'package/artinchip/test-lvgl/S20test_lvgl').read_text()
        self.assertEqual(runtime.count('\ncase "$1" in\n'), 1)
        functions, commands = runtime.split('\ncase "$1" in\n', 1)
        self.assertIn('prepare-devices)', commands)
        record = shlex.quote(str(self.calls))
        mocks = ("\nload_touch_driver() { printf 'load\\n' >> " + record +
                 "; return 0; }\nrequired_devices_ready() { printf 'ready\\n' >> " +
                 record + "; return 0; }\nwait_required_devices() { printf 'wait\\n' >> " +
                 record + "; return " + ('1' if failed else '0') + "; }\n")
        return (functions + mocks + '\ncase "$1" in\n' + commands).encode()

    def test_prepare_devices_current_is_synchronous_and_returns_runtime_status(self):
        self.pair(self.rescue / 'startup.sh', self.rescue / 'startup.sha256',
                  self.command_runtime('fallback'))
        self.pair(self.rescue / 'watch.sh', self.rescue / 'watch.sha256',
                  ('#!/bin/sh\n: > ' + shlex.quote(str(self.watch_calls)) + '\n').encode())
        for status in (0, 7):
            with self.subTest(status=status):
                self.pair(self.runtime, self.descriptor,
                          self.command_runtime('current', status))
                self.stop_flag.write_text('preserve')
                result = self.invoke_entry('prepare-devices')
                self.assertEqual(result.returncode, status, result.stderr)
                self.assertEqual(self.calls.read_text(),
                                 f'current|1|prepare-devices|{result.launched_pid}\n')
                self.assertFalse(self.watch_calls.exists())
                self.assertEqual(self.stop_flag.read_text(), 'preserve')

    def test_prepare_devices_rejected_current_uses_verified_fallback(self):
        self.pair(self.rescue / 'startup.sh', self.rescue / 'startup.sha256',
                  self.command_runtime('fallback', 6))
        for corruption in ('missing', 'hash', 'syntax', 'descriptor', 'symlink'):
            with self.subTest(corruption=corruption):
                for path in (self.runtime, self.descriptor, self.calls):
                    if path.exists() or path.is_symlink():
                        path.unlink()
                if corruption != 'missing':
                    self.pair(self.runtime, self.descriptor, self.command_runtime('current'))
                if corruption == 'hash':
                    self.runtime.write_bytes(self.command_runtime('changed'))
                elif corruption == 'syntax':
                    self.pair(self.runtime, self.descriptor, b'#!/bin/sh\nif then\n')
                elif corruption == 'descriptor':
                    self.descriptor.write_text('f' * 64 + '\n\n')
                elif corruption == 'symlink':
                    saved = self.base / 'linked-runtime'
                    saved.write_bytes(self.runtime.read_bytes())
                    self.runtime.unlink()
                    self.runtime.symlink_to(saved)
                result = self.invoke_entry('prepare-devices')
                self.assertEqual(result.returncode, 6, result.stderr)
                self.assertEqual(self.calls.read_text(),
                                 f'fallback|1|prepare-devices|{result.launched_pid}\n')

    def test_prepare_devices_no_valid_runtime_and_extra_arguments_fail_closed(self):
        self.assertNotEqual(self.invoke_entry('prepare-devices').returncode, 0)
        self.assertFalse(self.calls.exists())
        self.pair(self.rescue / 'startup.sh', self.rescue / 'startup.sha256',
                  self.command_runtime('untrusted'))
        (self.rescue / 'startup.sha256').write_text('0' * 64 + '\n')
        self.assertNotEqual(self.invoke_entry('prepare-devices').returncode, 0)
        self.assertFalse(self.calls.exists())
        self.pair(self.runtime, self.descriptor, self.command_runtime('current'))
        self.assertEqual(self.invoke_entry('prepare-devices', 'unexpected').returncode, 2)
        self.assertFalse(self.calls.exists())

    def test_prepare_devices_real_current_and_frozen_runtime_handlers(self):
        for route in ('current', 'frozen'):
            for failed in (False, True):
                with self.subTest(route=route, failed=failed):
                    if self.calls.exists():
                        self.calls.unlink()
                    if self.runtime.exists():
                        self.runtime.unlink()
                    data = self.actual_runtime_fixture(failed)
                    if route == 'current':
                        self.pair(self.runtime, self.descriptor, data)
                    else:
                        self.pair(self.rescue / 'startup.sh',
                                  self.rescue / 'startup.sha256', data)
                    result = self.invoke_entry('prepare-devices')
                    self.assertEqual(result.returncode, 1 if failed else 0, result.stderr)
                    self.assertEqual(self.calls.read_text(), 'load\nready\nwait\n')

    def test_start_without_valid_runtime_reports_failure_instead_of_silent_success(self):
        result = self.invoke_entry('start')
        self.assertEqual(result.returncode, 1)
        self.assertIn('factory UI watcher validation failed', result.stderr)
        self.assertIn('current and factory startup runtimes are invalid', result.stderr)
        self.assertFalse(self.calls.exists())
        self.assertFalse(self.watch_calls.exists())

    def test_real_native_buffered_entry_when_filesystem_rejects_direct_io(self):
        native = self.base / 'native-sync'
        source = HERE.parents[1] / 'source/artinchip/test-lvgl/tools/un260_storage_sync.c'
        subprocess.run(['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                        '-O2', str(source), '-o', str(native)], check=True)
        shim = self.base / 'direct-refused.c'
        library = self.base / 'direct-refused.so'
        # Fortified glibc open() may use __open_2; both enforce the same
        # O_DIRECT-only EINVAL. Buffered reads remain the real OS operation.
        shim.write_text(r"""
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
""")
        subprocess.run(['gcc', '-shared', '-fPIC', str(shim), '-ldl',
                        '-o', str(library)], check=True)
        environment = dict(os.environ, LD_PRELOAD=str(library))
        self.pair(self.runtime, self.descriptor, self.command_runtime('current'))
        data = self.runtime.read_bytes()
        direct = subprocess.run([str(native), '--hash-direct', str(self.runtime)],
                                capture_output=True, text=True, env=environment, timeout=5)
        self.assertNotEqual(direct.returncode, 0)
        self.assertEqual(direct.stdout, '')
        self.assertIn('Invalid argument', direct.stderr)
        buffered = subprocess.run([str(native), '--hash-file', str(self.runtime)],
                                  capture_output=True, text=True, env=environment, timeout=5)
        self.assertEqual(buffered.returncode, 0, buffered.stderr)
        self.assertEqual(buffered.stdout, hashlib.sha256(data).hexdigest() + '\n')
        shutil.copyfile(native, self.rescue / 'sync')
        (self.rescue / 'sync').chmod(0o755)
        for route in ('current', 'frozen'):
            with self.subTest(route=route):
                if self.calls.exists():
                    self.calls.unlink()
                if route == 'frozen':
                    self.runtime.unlink()
                    self.pair(self.rescue / 'startup.sh', self.rescue / 'startup.sha256',
                              self.command_runtime('frozen'))
                result = self.invoke_entry('prepare-devices',
                                           environment={'LD_PRELOAD': str(library)})
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(self.calls.read_text(),
                                 f'{route}|1|prepare-devices|{result.launched_pid}\n')

    def test_matching_current_runtime_selected(self):
        self.pair(self.runtime, self.descriptor)
        self.rescue_pair()
        result = self.call(self.entry, 'select_runtime; printf "%s" "$SELECTED_RUNTIME"')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, str(self.runtime))

    def test_missing_corrupt_or_bad_syntax_current_falls_back(self):
        self.rescue_pair()
        for data in [None, b'corrupt', b'#!/bin/sh\nif then\n']:
            with self.subTest(data=data):
                if data is not None:
                    self.pair(self.runtime, self.descriptor, data)
                    if data == b'corrupt':
                        self.runtime.write_bytes(b'changed')
                result = self.call(self.entry, 'select_runtime; printf "%s" "$SELECTED_RUNTIME"')
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout, str(self.rescue / 'startup.sh'))

    def test_no_valid_runtime_fails(self):
        self.assertNotEqual(self.call(self.entry, 'select_runtime').returncode, 0)
        self.rescue_pair()
        (self.rescue / 'startup.sha256').write_text('a' * 64 + '\n\n')
        self.assertNotEqual(self.call(self.entry, 'select_runtime').returncode, 0)

    def test_symlink_file_descriptor_or_ancestor_refused(self):
        self.pair(self.runtime, self.descriptor)
        for path in [self.runtime, self.descriptor]:
            saved = path.with_suffix('.saved')
            path.rename(saved)
            path.symlink_to(saved)
            self.assertNotEqual(self.call(self.entry, 'runtime_valid "$RUNTIME" ' + shlex.quote(str(self.descriptor))).returncode, 0)
            path.unlink()
            saved.rename(path)
        self.rescue_pair()
        self.rescue.rename(self.base / 'real-rescue')
        self.rescue.symlink_to(self.base / 'real-rescue', target_is_directory=True)
        self.assertNotEqual(self.call(self.watch, 'rescue_runtime_valid').returncode, 0)

    def test_health_requires_actual_managed_process_and_handles_replaced_inode(self):
        shutil.copyfile('/bin/sleep', self.managed)
        self.managed.chmod(0o755)
        self.process = subprocess.Popen([str(self.managed), '30'])
        self.health.write_text(str(self.process.pid) + '\n')
        self.assertEqual(self.call(self.watch, 'health_alive').returncode, 0)
        self.managed.unlink()
        self.assertEqual(self.call(self.watch, 'health_alive').returncode, 0)
        self.process.terminate()
        self.process.wait(timeout=5)
        self.process = None
        self.assertNotEqual(self.call(self.watch, 'health_alive').returncode, 0)

    def test_health_rejects_other_process_symlink_and_malformed_pid(self):
        self.health.write_text(str(os.getpid()) + '\n')
        self.assertNotEqual(self.call(self.watch, 'health_alive').returncode, 0)
        for value in ['0', '0123', 'x', '1' * 17, '1\n2']:
            self.health.write_text(value)
            self.assertNotEqual(self.call(self.watch, 'health_alive').returncode, 0)
        self.health.unlink()
        self.health.symlink_to(self.descriptor)
        self.assertNotEqual(self.call(self.watch, 'health_alive').returncode, 0)

    def test_recovery_heartbeat_uses_uptime_and_expires(self):
        for tick, expected in [('100', 0), ('94', 0), ('93', 1), ('101', 1), ('x', 1), ('1' * 17, 1)]:
            self.heartbeat.write_text(tick + '\n')
            result = self.call(self.watch, 'NOW=100; recovery_alive')
            self.assertEqual(result.returncode == 0, expected == 0, (tick, result.stderr))

    def test_active_backend_is_not_the_same_as_dead_owned_ram_lock(self):
        self.assertNotEqual(self.call(self.watch, 'updater_active').returncode, 0)
        self.backend.mkdir(mode=0o700)
        self.assertNotEqual(self.call(self.watch, 'updater_active').returncode, 0)
        pid = self.backend / 'pid'
        pid.write_text(str(os.getpid()) + '\n')
        pid.chmod(0o600)
        self.assertEqual(self.call(self.watch, 'updater_active').returncode, 0)
        pid.write_text('999999999\n')
        self.assertNotEqual(self.call(self.watch, 'updater_active').returncode, 0)
        for data in ['', 'bad', '0', '1\n2']:
            pid.write_text(data)
            self.assertEqual(self.call(self.watch, 'updater_active').returncode, 0)
        pid.write_text('999999999\n')
        (self.backend / 'unknown').write_text('preserve')
        self.assertEqual(self.call(self.watch, 'updater_active').returncode, 0)

    def test_backend_unknown_permissions_and_symlinks_are_conservative(self):
        self.backend.mkdir(mode=0o755)
        self.assertEqual(self.call(self.watch, 'updater_active').returncode, 0)
        self.backend.chmod(0o700)
        pid = self.backend / 'pid'
        pid.symlink_to(self.descriptor)
        self.assertEqual(self.call(self.watch, 'updater_active').returncode, 0)


if __name__ == '__main__':
    unittest.main(verbosity=2)
