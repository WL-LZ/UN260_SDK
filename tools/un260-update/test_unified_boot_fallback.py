#!/usr/bin/env python3
"""Run the real boot start_ready and storage query in isolated host fixtures.

Only hardware/maintenance commands are fixtures; the fallback branch and its
read-only storage gate are production code. No board, NAND or power-cut claims.
The native display fixture retains ownership so premature termination is visible;
test_upgrade_handoff.py separately covers the actual framebuffer/lease protocol.
"""
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import tempfile
import time
import unittest

SDK = Path(__file__).resolve().parents[2]
BOOT = SDK / 'package/artinchip/test-lvgl/S20test_lvgl'
HELPER = SDK / 'target/d211/d213_devkitf/rootfs_overlay/usr/local/bin/un260_app_storage'
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']
NATIVE_FIXTURE = r'''
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
static const char *ready_path;
static void stop_display(int sig) { (void)sig; if(ready_path) unlink(ready_path); _exit(0); }
int main(int argc, char **argv) {
    int display = strstr(argv[0], "un260_upgrade_display") != NULL;
    if (display) {
        ready_path=getenv("TEST_READY");signal(SIGTERM,stop_display);
        if (prctl(PR_SET_NAME, "un260-boot", 0, 0, 0)) return 2;
        FILE *f = fopen(getenv("TEST_READY"), "w");
        if (!f) return 3;
        fprintf(f, "%ld\n", (long)getpid());
        if (fclose(f)) return 4;
    } else {
        FILE *f = fopen(getenv("TEST_LAUNCH"), "a");
        if (!f) return 5;
        const char *owner = getenv("UN260_BOOT_LIGHT_ACTIVE");
        fprintf(f, "pid=%ld arg=%s owner=%s\n", (long)getpid(),
                argc > 1 ? argv[1] : "", owner ? owner : "");
        if (fclose(f)) return 6;
    }
    for (;;) pause();
}
'''


class UnifiedBootFallbackTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='un260-boot-native-')
        source = Path(cls.build.name) / 'fixture.c'
        source.write_text(NATIVE_FIXTURE)
        cls.native = Path(cls.build.name) / 'fixture'
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                        str(source), '-o', str(cls.native)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-boot-fallback-')
        self.p = Path(self.temp.name)
        self.root = self.p / 'root'
        for rel in ['usr/bin', 'usr/local/bin', 'etc/un260/app-storage',
                    'run', 'var/run', 'var/lib/un260-updater', 'dev', 'tmp', 'sys']:
            (self.root / rel).mkdir(parents=True, exist_ok=True)
        self.request = self.root / 'etc/un260/unified-request'
        self.request.write_text('a' * 64 + '\n' + 'b' * 64 + '\n')
        self.request_before = self.request.read_bytes()
        self.state = self.root / 'etc/un260/app-storage'
        self.events = self.p / 'events'
        self.launch = self.p / 'launch'
        self.ready = self.root / 'dev/.un260-boot-light.ready'
        self.app = self.root / 'usr/local/bin/test_lvgl'
        self.display = self.root / 'usr/local/bin/un260_upgrade_display'
        for dest in [self.app, self.display]:
            dest.write_bytes(self.native.read_bytes())
            dest.chmod(0o755)
        for rel in ['etc/pointercal', 'dev/fb0', 'dev/ge']:
            (self.root / rel).touch()
        self.business = self.root / 'etc/ui_state/count_history/record'
        self.business.parent.mkdir(parents=True)
        self.business.write_bytes(b'BUSINESS-UNCHANGED')
        self.nand = self.root / 'dev/mtd11'
        self.nand.write_bytes(b'NAND-MUST-NOT-BE-ACCESSED')
        self.status = self.root / 'tmp/ui_update.status'
        self.status.write_text('stage=fail\nsuccess=0\nmessage=Storage migration paused\n')
        self.result = self.root / 'tmp/UN260_UPDATE_RESULT.txt'
        self.result.write_text('result=fail\nversion=\n')
        self.query = self.p / 'storage-query.sh'
        self.query.write_text(self.redirect_paths(HELPER.read_text()))
        self.query.chmod(0o755)
        # Keep real boot function bodies, redirect their absolute destinations,
        # and call start_ready rather than the asynchronous mdev dispatcher.
        boot = BOOT.read_text().rsplit('case "$1" in\n    --rescue-only)', 1)
        self.assertEqual(len(boot), 2)
        self.boot = self.p / 'boot-functions.sh'
        self.boot.write_text(self.redirect_paths(boot[0]))
        self.helper = self.root / 'usr/local/bin/un260_app_storage'
        self.write_helper()
        self.write_updater()
        self.runner = self.p / 'run.sh'
        self.runner.write_text('. %s\nstart_ready\n' % shlex.quote(str(self.boot)))
        self.env = dict(os.environ, UN260_BOOT_TRACE='0', UN260_EARLY_DISPLAY='1',
                        TEST_READY=str(self.ready), TEST_LAUNCH=str(self.launch))

    def redirect_paths(self, source):
        return re.sub(r'(?<![A-Za-z0-9_])/(?:usr|etc|var|run|tmp|dev|sys)(?=/|[\s"\'])',
                      lambda m: str(self.root) + m.group(0), source)

    def write_executable(self, path, text):
        path.write_text('#!/bin/sh\n' + text)
        path.chmod(0o755)

    def write_helper(self, old=False, prepare_failure=False):
        query_shell = ' '.join(shlex.quote(arg) for arg in SHELL)
        self.write_executable(self.helper, '''
printf 'helper:%%s\n' "$1" >> %s
case "$1" in
  --prepare) exit %s ;;
  --can-launch-current) %s ;;
  *) exit 2 ;;
esac
''' % (shlex.quote(str(self.events)), 7 if prepare_failure else 0,
       'exit 2' if old else 'exec %s %s "$@"' % (query_shell, shlex.quote(str(self.query)))))

    def write_updater(self, mutation='', continued=9, recovery=0):
        # This command boundary models updater exit/fault states without
        # reimplementing its package parser or the storage eligibility gate.
        self.write_executable(self.root / 'usr/bin/ui_update.sh', '''
# UN260_UNIFIED_1
printf 'update:%%s\n' "$1" >> %s
case "$1" in
  --unified-pending) [ -f %s ] ;;
  --recover) exit %s ;;
  --continue-unified)
    %s
    exit %s ;;
  *) exit 2 ;;
esac
''' % (shlex.quote(str(self.events)), shlex.quote(str(self.request)), recovery,
       mutation or ':', continued))

    def stop_owned_processes(self):
        paths = [self.ready, self.root / 'run/test_lvgl.pid']
        for path in paths:
            if not path.exists():
                continue
            pid = int(path.read_text().strip())
            try:
                # Never signal a process whose fixture identity is unproven.
                actual = Path('/proc/%d/exe' % pid).resolve(strict=True)
                self.assertIn(actual, [self.app, self.display])
                os.kill(pid, signal.SIGTERM)
            except (ProcessLookupError, FileNotFoundError):
                pass
            path.unlink(missing_ok=True)
        time.sleep(.02)

    def tearDown(self):
        self.stop_owned_processes()
        self.assertEqual(self.request.read_bytes(), self.request_before)
        self.assertEqual(self.business.read_bytes(), b'BUSINESS-UNCHANGED')
        self.assertEqual(self.nand.read_bytes(), b'NAND-MUST-NOT-BE-ACCESSED')
        self.assertEqual(self.status.read_text(),
                         'stage=fail\nsuccess=0\nmessage=Storage migration paused\n')
        self.assertEqual(self.result.read_text(), 'result=fail\nversion=\n')
        self.assertFalse((self.root / 'var/lib/un260-updater/unified.completed').exists())
        self.temp.cleanup()

    def run_boot(self, launched=True):
        before = self.launch.read_text().count('pid=') if self.launch.exists() else 0
        result = subprocess.run(SHELL + [str(self.runner)], env=self.env,
                                capture_output=True, text=True, timeout=8)
        log_path = self.root / 'tmp/test_lvgl.startup.log'
        log = log_path.read_text() if log_path.exists() else ''
        after = self.launch.read_text().count('pid=') if self.launch.exists() else 0
        self.assertEqual(result.returncode == 0, launched, result.stdout + result.stderr + log)
        self.assertEqual(after - before, int(launched), result.stdout + log)
        if launched:
            # Verified continuation releases maintenance before selecting boot art.
            self.assertFalse(self.ready.exists())
        elif self.ready.exists():
            # An unresolved recovery retains its maintenance display.
            os.kill(int(self.ready.read_text()), 0)
        return result, log

    def test_unmigrated_failure_launches_original_and_retries_same_request(self):
        original = self.app.read_bytes()
        for attempt in range(2):
            result, _ = self.run_boot()
            self.assertIn('Unified upgrade incomplete; returning to installed UI', result.stdout)
            self.assertIn('arg=-qws owner=\n', self.launch.read_text())
            self.assertEqual(self.events.read_text().count('update:--continue-unified'), attempt + 1)
            self.assertEqual(self.app.read_bytes(), original)
            self.stop_owned_processes()

    def test_storage_state_after_failure_never_launches(self):
        entries = [self.state / name for name in ('pending', 'pending.tmp', 'active', 'active.tmp')]
        entries += [self.root / 'var/lib/un260-updater/transaction',
                    self.root / 'tmp/un260-updater.lock']
        entries += [Path(str(self.app) + suffix) for suffix in
                    ('.un260-new', '.un260-old', '.un260-restore', '.un260-volume-old')]
        for entry in entries:
            with self.subTest(artifact=entry.name):
                self.write_updater('printf unsafe > %s' % shlex.quote(str(entry)))
                self.run_boot(launched=False)
                self.assertTrue(entry.exists())
                self.stop_owned_processes()
                entry.unlink()

    def test_incomplete_writeback_recovery_never_launches(self):
        transaction = self.root / 'var/lib/un260-updater/transaction'
        self.write_updater('mkdir %s; printf committed > %s' %
                           (shlex.quote(str(transaction)), shlex.quote(str(transaction / 'committed'))))
        self.run_boot(launched=False)
        self.assertTrue((transaction / 'committed').is_file())

    def test_old_or_missing_helper_fails_closed(self):
        self.write_helper(old=True)
        self.run_boot(launched=False)
        self.stop_owned_processes()
        self.helper.unlink()
        self.run_boot(launched=False)

    def test_invalid_root_app_never_launches(self):
        self.app.write_text('#!/bin/sh\nexit 0\n')
        self.run_boot(launched=False)

    def test_failed_existing_transaction_does_not_query_or_launch(self):
        (self.root / 'var/lib/un260-updater/transaction').mkdir()
        self.write_updater(recovery=8)
        self.run_boot(launched=False)
        self.assertNotIn('--continue-unified', self.events.read_text())
        self.assertNotIn('--can-launch-current', self.events.read_text())

    def test_failed_prepare_does_not_query_or_launch(self):
        self.write_helper(prepare_failure=True)
        self.run_boot(launched=False)
        self.assertNotIn('update:', self.events.read_text())
        self.assertNotIn('--can-launch-current', self.events.read_text())

    def test_successful_continuation_uses_existing_launch_without_query(self):
        self.write_updater(continued=0)
        self.run_boot()
        self.assertNotIn('--can-launch-current', self.events.read_text())

    def test_ordinary_boot_does_not_run_updater_or_query(self):
        self.request.unlink()
        try:
            self.run_boot()
            self.assertNotIn('update:', self.events.read_text())
            self.assertNotIn('--can-launch-current', self.events.read_text())
        finally:
            self.request.write_bytes(self.request_before)


if __name__ == '__main__':
    unittest.main(verbosity=2)
