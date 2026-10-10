#!/usr/bin/env python3
"""Real production guard fork/exec/proc tests for startup health ownership."""
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time
import unittest

HERE = Path(__file__).resolve().parent
ROOT = Path(os.environ.get('UN260_REPO', HERE.parents[1]))
RUNTIME = Path(os.environ.get('UN260_RUNTIME_SOURCE', ROOT / 'package/artinchip/test-lvgl/S20test_lvgl'))
SHELL = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['sh']

APP_SOURCE = r'''
#define _GNU_SOURCE
#include "runtime_guard.h"
#include <sys/prctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static const char *health;
static int paint=1;
static volatile sig_atomic_t stopping;
static void stop(int s){(void)s;stopping=1;}
static int application(void) {
    char path[512]; snprintf(path,sizeof(path),"%s.XXXXXX",health);
    if(paint){int fd=mkstemp(path);if(fd<0)return 3;
        char record[32];int n=snprintf(record,sizeof(record),"%ld\n",(long)getpid());
        if(write(fd,record,n)!=n||close(fd)||rename(path,health))return 4;}
    while(!stopping){runtime_guard_heartbeat();usleep(10000);}return 0;
}
int main(int argc,char **argv) {
    health=getenv("TEST_HEALTH");if(!health)return 2;
    if(argc>1&&!strcmp(argv[1],"--early-exit"))return 0;
    if(argc>1&&!strcmp(argv[1],"--no-paint"))paint=0;
    if(argc>1&&!strcmp(argv[1],"--direct")){
        signal(SIGTERM,stop);return application();}
    if(argc>1&&!strcmp(argv[1],"--two-children")){
        signal(SIGTERM,stop);pid_t a=fork();if(!a){prctl(PR_SET_PDEATHSIG,SIGKILL);signal(SIGTERM,stop);return application();}
        pid_t b=fork();if(!b){prctl(PR_SET_PDEATHSIG,SIGKILL);paint=0;signal(SIGTERM,stop);return application();}
        char other[512];snprintf(other,sizeof(other),"%s.other",health);FILE*f=fopen(other,"w");if(!f)return 2;fprintf(f,"%ld\n",(long)b);fclose(f);
        while(!stopping)usleep(10000);kill(a,SIGTERM);kill(b,SIGTERM);waitpid(a,0,0);waitpid(b,0,0);return 0;}
    return runtime_guard_run(application);
}
'''


class HealthTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-health-')
        self.base = Path(self.temp.name)
        self.health = self.base / 'health'
        (self.base / 'run').mkdir()
        (self.base / 'var').mkdir()
        (self.base / 'var' / 'run').symlink_to('../run', target_is_directory=True)
        self.token = self.base / 'run' / 'app.token'
        self.pidfile = self.base / 'run' / 'app.pid'
        self.app = self.base / 'managed-app'
        self.processes = []
        wrapper = self.base / 'app.c'
        wrapper.write_text(APP_SOURCE)
        guard_dir = ROOT / 'source/artinchip/test-lvgl/un260/app_service'
        command = ['gcc', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-Werror',
                   '-Wno-misleading-indentation', '-DUN260_RUNTIME_GUARD_ENABLED=1',
                   '-DRUNTIME_GUARD_DIR="' + str(self.base / 'fault') + '"',
                   '-DRUNTIME_GUARD_LOCK="' + str(self.base / 'guard.lock') + '"',
                   '-DRUNTIME_GUARD_DISPLAY="/bin/false"', '-I', str(guard_dir),
                   str(wrapper), str(guard_dir / 'runtime_guard.c'), '-o', str(self.app)]
        subprocess.run(command, check=True, capture_output=True)
        source = RUNTIME.read_text().rsplit('\ncase "$1" in\n    --rescue-only)', 1)[0]
        for old, new in [('/run/test_lvgl.pid', str(self.pidfile)),
                         ('/run/test_lvgl.token', str(self.token)),
                         ('/tmp/un260-ui.health', str(self.health)),
                         ('/tmp/test_lvgl.startup.log', str(self.base / 'startup.log')),
                         ('/mnt/un260-app/test_lvgl', str(self.app)),
                         ('/etc/pointercal', str(self.base / 'pointercal'))]:
            source = source.replace(old, new)
        (self.base / 'pointercal').touch()
        source += '\nDAEMON=' + shlex.quote(str(self.app)) + '\n'
        self.functions = self.base / 'runtime-functions.sh'
        self.functions.write_text(source)
        self.env = dict(os.environ, TEST_HEALTH=str(self.health), UN260_EARLY_DISPLAY='0')
        self.dispatch = self.base / 'dispatch'
        self.dispatch.write_text('#!/bin/sh\ncase "$1" in --can-launch) printf "%s\\n" ' +
                                 shlex.quote(str(self.app)) + ' ;; --trial-pending) exit 1 ;; '
                                 '--confirm-trial) printf "%s\\n" "$2" > ' +
                                 shlex.quote(str(self.base / 'confirmed')) + ' ;; esac\n')
        self.dispatch.chmod(0o755)

    def tearDown(self):
        for p in self.processes:
            if p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=4)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait(timeout=4)
        self.temp.cleanup()

    def call(self, body, timeout=12):
        return subprocess.run(SHELL + ['-c', '. ' + shlex.quote(str(self.functions)) + '; ' + body],
                              env=self.env, text=True, capture_output=True, timeout=timeout)

    def start(self, mode=None, expect_health=True):
        args = [str(self.app)] + ([mode] if mode else [])
        p = subprocess.Popen(args, env=self.env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.processes.append(p)
        if expect_health:
            limit = time.monotonic() + 3
            while not self.health.exists() and time.monotonic() < limit:
                time.sleep(.01)
            self.assertTrue(self.health.exists())
        result = self.call('publish_app_identity ' + str(p.pid))
        self.assertEqual(result.returncode, 0, result.stderr)
        return p

    def launch_stubs(self):
        return ('modern_platform(){ return 0; }; prepare_devices(){ return ${PREP_FAIL:-0}; }; '
                'cpu_performance_enter(){ :; }; cpu_performance_leave(){ :; }; '
                'stop_runtime_worker(){ :; }; DEFER_PIDFILE=' + shlex.quote(str(self.base / 'defer.pid')) + '; '
                'boot_light_owner(){ return 1; }; UPDATE_DISPATCH=' + shlex.quote(str(self.dispatch)) + '; ')

    def test_real_guard_child_resolved_and_stable(self):
        p = self.start()
        child = int(self.health.read_text())
        self.assertNotEqual(child, p.pid)
        result = self.call('HEALTH_STABLE=0; wait_ui_health ' + str(p.pid) + '; printf "%s" "$UI_PID"')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, str(child))
        self.assertEqual((self.base / 'var/run/app.token').read_bytes(), self.token.read_bytes())

    def test_paused_monitor_or_ui_cannot_pass_three_second_health(self):
        for which in ['monitor', 'ui']:
            with self.subTest(which=which):
                p = self.start()
                target = p.pid if which == 'monitor' else int(self.health.read_text())
                os.kill(target, signal.SIGSTOP)
                try:
                    result = self.call('HEALTH_LIMIT=4; HEALTH_STABLE=3; wait_ui_health ' + str(p.pid))
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(self.call('stop_owned_app ' + str(p.pid)).returncode, 0)
                finally:
                    try:
                        os.kill(target, signal.SIGCONT)
                    except ProcessLookupError:
                        pass
                p.wait(timeout=4)
                self.assertFalse(self.health.exists())

    def test_guard_disabled_direct_process(self):
        p = self.start('--direct')
        result = self.call('resolve_ui_health ' + str(p.pid) + '; printf "%s" "$UI_PID"')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, str(p.pid))

    def test_confirm_uses_child_pid_not_monitor(self):
        p = self.start()
        child = int(self.health.read_text())
        result = self.call('UPDATE_DISPATCH=' + shlex.quote(str(self.dispatch)) +
                           '; PID=' + str(p.pid) + '; HEALTH_STABLE=0; trial_pending(){ return 0; }; runtime_supervise 0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.base / 'confirmed').read_text(), str(child) + '\n')

    def test_sibling_same_executable_and_other_executable_rejected(self):
        p = self.start()
        original = self.health.read_text()
        peer = subprocess.Popen([str(self.app), '--direct'], env=dict(self.env, TEST_HEALTH=str(self.base / 'peer-health')))
        self.processes.append(peer)
        for value in [str(peer.pid), str(os.getpid())]:
            self.health.write_text(value + '\n')
            self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)
        self.health.write_text(original)
        self.assertEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)

    def test_missing_corrupt_reused_or_linked_token_never_kills(self):
        p = self.start()
        original = self.token.read_bytes()
        for data in [b'', b'bad\n', (str(p.pid) + ' 1\n').encode(), original + b'\n']:
            self.token.write_bytes(data)
            self.assertNotEqual(self.call('stop_owned_app ' + str(p.pid)).returncode, 0)
            self.assertIsNone(p.poll())
        self.token.unlink()
        self.assertNotEqual(self.call('stop_owned_app ' + str(p.pid)).returncode, 0)
        saved = self.base / 'saved'
        saved.write_bytes(original)
        saved.chmod(0o600)
        self.token.symlink_to(saved)
        self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)
        self.assertIsNone(p.poll())

    def test_marker_permissions_shape_and_symlink_rejected(self):
        p = self.start()
        original = self.health.read_bytes()
        for data in [b'0\n', b'1\n', original + b'\n', b'123 extra\n', b'0' + original]:
            self.health.write_bytes(data)
            self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)
        self.health.write_bytes(original)
        self.health.chmod(0o644)
        self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)
        self.health.unlink()
        self.health.symlink_to(self.token)
        self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)

    def test_child_identity_change_restarts_stable_interval(self):
        p = self.start('--two-children')
        second = self.health.with_suffix('.other')
        limit = time.monotonic() + 3
        while not second.exists() and time.monotonic() < limit:
            time.sleep(.01)
        other = second.read_text()
        shell = subprocess.Popen(SHELL + ['-c', '. ' + shlex.quote(str(self.functions)) +
                                 '; HEALTH_STABLE=2; HEALTH_LIMIT=8; wait_ui_health ' + str(p.pid)], env=self.env)
        time.sleep(1.2)
        self.health.write_text(other)
        self.health.chmod(0o600)
        time.sleep(1.2)
        self.assertIsNone(shell.poll(), 'old UI stable count must not accept a replacement child')
        self.assertEqual(shell.wait(timeout=5), 0)

    def test_stop_after_first_paint_clears_child_marker_and_records(self):
        p = self.start()
        child = int(self.health.read_text())
        result = self.call('stop_owned_app ' + str(p.pid))
        self.assertEqual(result.returncode, 0, result.stderr)
        p.wait(timeout=4)
        self.assertFalse(self.health.exists())
        self.assertFalse(self.pidfile.exists())
        self.assertFalse(self.token.exists())
        self.assertFalse(Path('/proc/' + str(child)).exists())

    def test_fresh_stop_entry_uses_managed_exe_with_default_daemon_path(self):
        p = self.start()
        result = self.call(self.launch_stubs() + 'DAEMON=/usr/local/bin/test_lvgl; stop')
        self.assertEqual(result.returncode, 0, result.stderr)
        p.wait(timeout=4)
        self.assertFalse(self.health.exists())
        self.assertFalse(self.token.exists())

    def test_parent_sigkill_pdeath_child_marker_is_not_healthy(self):
        p = self.start()
        p.kill()
        p.wait(timeout=4)
        self.assertNotEqual(self.call('resolve_ui_health ' + str(p.pid)).returncode, 0)
        self.assertEqual(self.call('stop_owned_app ' + str(p.pid)).returncode, 0)

    def test_no_first_paint_times_out_then_rescue(self):
        p = self.start('--no-paint', expect_health=False)
        result = self.call('PID=' + str(p.pid) + '; HEALTH_LIMIT=1; try_trial_rollback(){ return 1; }; '
                           'same_boot_trial_reboot(){ return 1; }; usb_recovery_wait(){ echo RESCUE; }; runtime_supervise 0')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('RESCUE', result.stdout)
        p.wait(timeout=4)

    def test_prepare_failed_never_claims_health_or_launches(self):
        result = self.call(self.launch_stubs() + 'PREP_FAIL=1; start_ready')
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.pidfile.exists())
        self.assertFalse(self.health.exists())

    def test_immediate_exit_cleans_launch_records(self):
        result = self.call(self.launch_stubs() + 'DAEMONOPTS=--early-exit; '
                           'try_trial_rollback(){ return 1; }; same_boot_trial_reboot(){ return 1; }; '
                           'usb_recovery_wait(){ echo RESCUE; }; start_ready; runtime_supervise "$?"')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('RESCUE', result.stdout)
        self.assertFalse(self.pidfile.exists())
        self.assertFalse(self.token.exists())

    def test_token_publication_failure_stops_only_new_launch(self):
        result = self.call(self.launch_stubs() + 'APP_TOKENFILE=' + shlex.quote(str(self.base / 'missing' / 'token')) +
                           '; start_ready', timeout=12)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.pidfile.exists())
        if self.health.exists():
            child = int(self.health.read_text())
            self.assertNotEqual(self.call('managed_process ' + str(child)).returncode, 0)

    def test_partial_pid_record_cleanup_and_old_without_token_refused(self):
        p = self.start()
        self.pidfile.unlink()
        self.assertEqual(self.call('stop_owned_app ' + str(p.pid)).returncode, 0)
        p.wait(timeout=4)
        p = self.start('--direct')
        self.token.unlink()
        result = self.call(self.launch_stubs() + 'stop')
        self.assertNotEqual(result.returncode, 0)
        self.assertIsNone(p.poll())
        self.assertTrue(self.pidfile.exists())

    def test_rescue_recognizes_existing_healthy_guard_child(self):
        p = self.start()
        # Exercise the actual rescue branch without starting or stopping real services.
        pid = self.base / 'recovery.pid'
        pid.write_text('0\n')
        body = ('RECOVERY_PIDFILE=' + shlex.quote(str(pid)) + '; '
                'runtime_cleanup(){ :; }; runtime_cancel(){ exit 1; }; '
                'DAEMON=/usr/local/bin/test_lvgl; '
                'read -r self rest < /proc/self/stat; printf "%s\\n" "$self" > "$RECOVERY_PIDFILE"; '
                'runtime_worker rescue')
        result = self.call(body)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIsNone(p.poll())


if __name__ == '__main__':
    unittest.main()
