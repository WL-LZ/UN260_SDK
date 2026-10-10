#!/usr/bin/env python3
"""Real dispatcher exec/FD tests with native codecs and isolated host volumes.

Mount identity, root UID and fixed paths are fixture seams. No board, NAND or
physical USB power-cut claim; the production shell and native tools are used.
"""
import hashlib
import io
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile
import tempfile
import time
import unittest

SDK = Path(__file__).resolve().parents[2]
SOURCE = SDK / 'tools/un260-update/un260_update_dispatch.sh'
CORES = {
    'engine.sh': 'usr/bin/ui_update.sh',
    'sync': 'usr/local/bin/un260_storage_sync',
    'unpack': 'usr/local/bin/un260_unpack',
    'app_storage': 'usr/local/bin/un260_app_storage',
    'resource_cleanup': 'usr/local/bin/un260_resource_cleanup',
}
HASH_KEYS = ['engine', 'sync', 'unpack', 'app_storage', 'resource_cleanup']
CONTEXT_KEYS = ['pid', 'usb_dev', 'stage', 'bundle_path', 'bundle_sha256',
    'bundle_fnv64', 'engine_sha256', 'sync_sha256', 'unpack_sha256',
    'app_storage_sha256', 'resource_cleanup_sha256', 'start_time', 'phase_time',
    'last_progress', 'last_stage', 'guarded', 'usb_guarded', 'app_guarded',
    'mode', 'action_arg']


def sha(data):
    return hashlib.sha256(data).hexdigest()


def engine_script(directory, label, wait=False, result=0):
    # This engine only observes ownership, paths and descriptors, then cleans.
    # Application/storage/boot policies are the production engine's tests.
    return (r'''#!/bin/sh
# UN260_PACKAGE_ENGINE_V1
[ "$1" = --dispatch-context ] || exit 90
ctx=$2
cp "$ctx/context" OBSERVED
printf '%s\n' "$$" > PIDFILE
[ "$(cat LOCKDIR/pid)" = "$$" ] || exit 91
root=ROOTDIR
usb=USBDIR
stage=$(sed -n 's/^stage=//p' "$ctx/context")
usb_held=$(sed -n 's/^usb_guarded=//p' "$ctx/context")
cleanup_engine() {
    rc=$?
    trap - 0
    rm -rf "$ctx"
    [ "$stage" = none ] || rm -rf "$stage"
    rm -f LOCKDIR/pid
    rmdir LOCKDIR
    exit "$rc"
}
trap cleanup_engine 0
"$ctx/sync" --fd 7 "$root" || exit 92
"$ctx/sync" --lock-held FLOCKFILE || exit 96
[ "$usb_held" = 0 ] || "$ctx/sync" --fd 8 "$usb" || exit 93
printf 'LABEL\n' > LABELFILE
for f in engine.sh sync unpack app_storage resource_cleanup; do
    ls -ln "$ctx/$f" >> MODESFILE
done
ls -ldn "$ctx" >> MODESFILE
if [ "$stage" != none ]; then
    [ -f "$stage/payload/usr/local/bin/test_lvgl" ] || exit 94
    ls -ln "$stage/payload/usr/bin/ui_update.sh" >> MODESFILE
    [ -f USBDIR/update/.un260_backup/sentinel ] || exit 95
fi
WAIT_CODE
exit RESULT
'''.replace('OBSERVED', shlex.quote(str(directory / 'observed')))
        .replace('PIDFILE', shlex.quote(str(directory / 'pid')))
        .replace('LABELFILE', shlex.quote(str(directory / 'label')))
        .replace('MODESFILE', shlex.quote(str(directory / 'modes')))
        .replace('LOCKDIR', shlex.quote(str(directory / 'lock')))
        .replace('FLOCKFILE', shlex.quote(str(directory / 'gate.flock')))
        .replace('ROOTDIR', shlex.quote(str(directory / 'root')))
        .replace('USBDIR', shlex.quote(str(directory / 'usb')))
        .replace('LABEL', label)
        .replace('RESULT', str(result))
        .replace('WAIT_CODE', ('touch %s\nread -r released < %s' %
            (shlex.quote(str(directory / 'ready')), shlex.quote(str(directory / 'release')))) if wait else '')).encode()


class DispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native_temp = tempfile.TemporaryDirectory(prefix='un260-dispatch-native-')
        cls.native = Path(cls.native_temp.name)
        for role, filename in [('sync', 'un260_storage_sync.c'), ('unpack', 'un260_unpack.c')]:
            source = SDK / 'source/artinchip/test-lvgl/tools' / filename
            if role == 'sync' and os.environ.get('TEST_BUSYBOX') == '1':
                # Host /bin/sh is dash. Change only native exec's interpreter
                # path so the same FD6 logic actually reexecs BusyBox ash.
                text = source.read_text()
                if text.count('execv("/bin/sh",shell);') != 1:
                    raise AssertionError('Native shell interpreter seam changed')
                text = text.replace('execv("/bin/sh",shell);',
                    'execv("%s",shell);' % shutil.which('busybox'))
                source = cls.native / 'sync-busybox.c'
                source.write_text(text)
            command = ['gcc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-O2',
                str(source), '-o', str(cls.native / role)]
            if role == 'unpack':
                command.append('-lz')
            subprocess.run(command, check=True, capture_output=True)
        cls.shell = [shutil.which('busybox'), 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['/bin/sh']
        if not cls.shell[0]:
            raise unittest.SkipTest('BusyBox executable unavailable')

    @classmethod
    def tearDownClass(cls):
        cls.native_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='un260-dispatch-test-')
        self.p = Path(self.temp.name)
        self.root, self.usb, self.ram = [self.p / name for name in ('root', 'usb', 'ram')]
        self.rescue = self.root / 'usr/local/lib/un260-updater/rescue'
        self.rescue.mkdir(parents=True)
        self.ram.mkdir()
        (self.usb / 'update/.un260_backup').mkdir(parents=True)
        (self.usb / 'update/.un260_backup/sentinel').write_bytes(b'EXISTING RECOVERY BACKUP')
        self.platform = self.root / 'etc/un260/updater-platform'
        self.platform.parent.mkdir(parents=True)
        self.platform.write_text('UN260_UPDATER_PLATFORM_V1\n')
        self.device = self.p / 'device'
        self.device.write_bytes(b'HOST MOUNT IDENTITY SEAM')
        self.mounts = self.p / 'mounts'
        self.mounts.write_text('%s %s vfat rw 0 0\n' % (self.device, self.usb))
        self.lock = self.p / 'lock'
        self.flock = self.p / 'gate.flock'
        self.status, self.log = self.p / 'status', self.p / 'log'
        self.core = {'engine.sh': engine_script(self.p, 'candidate'),
            'sync': (self.native / 'sync').read_bytes(),
            'unpack': (self.native / 'unpack').read_bytes(),
            'app_storage': b'#!/bin/sh\nexit 0\n',
            'resource_cleanup': b'#!/bin/sh\nexit 0\n'}
        self.rescue_core = dict(self.core)
        self.rescue_core['engine.sh'] = engine_script(self.p, 'rescue')
        self.write_rescue()
        text = SOURCE.read_text()
        replacements = {'ROOT_DIR=/': self.root, 'USB_MNT=/mnt/usb': self.usb,
            'MOUNTS=/proc/mounts': self.mounts, 'PLATFORM=/etc/un260/updater-platform': self.platform,
            'RESCUE=/usr/local/lib/un260-updater/rescue': self.rescue,
            'LOCK_DIR=/tmp/un260-updater.lock': self.lock, 'LOCK_FILE=/tmp/un260-updater.flock': self.flock,
            'RAM_BASE=/tmp': self.ram,
            'STATUS_FILE=/tmp/ui_update.status': self.status, 'LOG=/tmp/ui_update.log': self.log}
        for old, new in replacements.items():
            self.assertEqual(text.count(old + '\n'), 1)
            text = text.replace(old + '\n', old.split('=')[0] + '=' + shlex.quote(str(new)) + '\n')
        text = text.replace('EXPECTED_UID=0\n', 'EXPECTED_UID=%d\n' % os.getuid())
        text = text.replace('usb_device() { [ -b "$1" ]; }', 'usb_device() { [ -f "$1" ]; }')
        if os.environ.get('TEST_BUSYBOX') == '1':
            text = text.replace('/bin/sh ', shlex.quote(self.shell[0]) + ' sh ')
        self.runner = self.p / 'dispatcher'
        self.runner.write_text(text)
        self.bundle = self.usb / 'update/UN260_UPDATE.upk'
        self.pack()

    def tearDown(self):
        self.temp.cleanup()

    def write_rescue(self):
        records = ['UN260_UPDATE_CORE_V1']
        for role, key in zip(CORES, HASH_KEYS):
            destination = self.rescue / role
            destination.write_bytes(self.rescue_core[role])
            destination.chmod(0o600 if role == 'engine.sh' else 0o755)
            records.append(key + '_sha256=' + sha(self.rescue_core[role]))
        (self.rescue / 'manifest').write_text('\n'.join(records) + '\n')
        (self.rescue / 'manifest').chmod(0o644)

    def write_installed(self, values=None):
        values = values or self.core
        records = ['UN260_UPDATE_CORE_V1']
        for role, key in zip(CORES, HASH_KEYS):
            destination = self.root / CORES[role]
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(values[role]); destination.chmod(0o755)
            records.append(key + '_sha256=' + sha(values[role]))
        manifest = self.root / 'etc/un260/update-core.manifest'
        manifest.write_text('\n'.join(records) + '\n'); manifest.chmod(0o644)

    def pack(self, metadata='', omit=None, extra=None, corrupt=None):
        payload = {'payload/' + rel: value for role, rel in CORES.items()
                   for value in [self.core[role]] if role != omit}
        payload['payload/usr/local/bin/test_lvgl'] = b'NEW APP REMAINS STAGED'
        checks = ''.join(sha(data) + '  ' + path + '\n' for path, data in sorted(payload.items())).encode()
        manifest = ('format=UN260_UPGRADE\nproduct=UN260\nschema=1\npackage_type=ui\n'
            'updater_envelope=UN260_UPDATE_ENVELOPE_V1\nupdater_platform=UN260_UPDATER_PLATFORM_V1\n'
            'updater_engine_abi=1\npackage_id=' + sha(checks) + '\n' + metadata).encode()
        if corrupt:
            payload['payload/' + CORES[corrupt]] += b'BROKEN'
        entries = {'manifest.ini': manifest, 'checksums.sha256': checks, 'install.tsv': b'future policy\n'}
        entries.update(payload)
        if extra:
            entries.update(extra)
        with tarfile.open(self.bundle, 'w:gz', format=tarfile.USTAR_FORMAT) as archive:
            for name, data in entries.items():
                info = tarfile.TarInfo(name); info.size = len(data); info.mode = 0o755
                archive.addfile(info, io.BytesIO(data))

    def run_dispatch(self, args=(), ok=True, env=None):
        runner = self.runner
        if env and env['PATH'].split(os.pathsep)[0] == str(self.p / 'commands'):
            # BusyBox can prefer compiled applets over PATH. Explicit shell
            # functions make the same fault seam effective on both shells.
            functions = []
            for name in ('dd', 'mktemp'):
                command = self.p / 'commands' / name
                if command.exists():
                    functions.append(name + '() { ' + shlex.quote(str(command)) + ' "$@"; }\n')
            text = self.runner.read_text()
            runner = self.p / 'dispatcher-injected'
            runner.write_text(text.replace('#!/bin/sh\n', '#!/bin/sh\n' + ''.join(functions), 1))
        process = subprocess.Popen(self.shell + [str(runner)] + list(args), env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        out, err = process.communicate(timeout=20)
        self.last_rc = process.returncode
        self.assertEqual(process.returncode == 0, ok, out + err + (self.log.read_text() if self.log.exists() else ''))
        return process.pid, err

    def clean(self):
        self.assertFalse(self.lock.exists())
        self.assertEqual(list(self.ram.glob('un260-update-engine.*')), [])
        self.assertEqual(list((self.usb / 'update').glob('.un260_dispatch.*')), [])
        self.assertEqual((self.usb / 'update/.un260_backup/sentinel').read_bytes(), b'EXISTING RECOVERY BACKUP')

    def context(self):
        lines = (self.p / 'observed').read_text().splitlines()
        self.assertEqual(lines[0], 'UN260_UPDATE_CONTEXT_V1')
        self.assertEqual([line.split('=', 1)[0] for line in lines[1:]], CONTEXT_KEYS)
        return dict(line.split('=', 1) for line in lines[1:])

    def test_selected_candidate_exec_same_pid_fd_native_0600_and_cleanup(self):
        # Poison the installed algorithm; selection must precede its execution.
        installed = self.root / 'usr/bin/ui_update.sh'
        installed.parent.mkdir(parents=True)
        installed.write_text('exit 97\n')
        pid, _ = self.run_dispatch()
        self.assertEqual(int((self.p / 'pid').read_text()), pid)
        context = self.context()
        self.assertEqual(context['pid'], str(pid))
        self.assertEqual(context['engine_sha256'], sha(self.core['engine.sh']))
        self.assertEqual(context['bundle_sha256'], sha(self.bundle.read_bytes()))
        self.assertEqual(context['usb_guarded'], '1')
        self.assertEqual(context['app_guarded'], '0')
        self.assertEqual((self.p / 'label').read_text(), 'candidate\n')
        modes = (self.p / 'modes').read_text().splitlines()
        self.assertTrue(modes[0].startswith('-rw-------'))
        self.assertTrue(all(line.startswith('-rwxr-xr-x') for line in modes[1:5]))
        self.assertTrue(modes[-1].startswith('-rw-------'))  # Actual native unpacked stage.
        self.assertFalse(self.status.exists())  # Dispatcher never claims success.
        self.clean()

    def test_all_no_usb_modes_snapshot_rescue_and_forward_action_only(self):
        self.bundle.unlink()
        self.mounts.write_text('')
        modes = [(['--recover'], 'recover', 'none'), (['--unified-pending'], 'pending', 'none'),
            (['--trial-pending'], 'trial-pending', 'none'), (['--confirm-trial', '123'], 'trial-confirm', '123'),
            (['--trial-installed'], 'trial-installed', 'none'),
            (['--rollback-trial'], 'trial-rollback', 'none'), (['--can-launch'], 'app-check', 'none')]
        for arguments, mode, arg in modes:
            with self.subTest(mode=mode):
                pid, _ = self.run_dispatch(arguments)
                context = self.context()
                self.assertEqual(int((self.p / 'pid').read_text()), pid)
                for key in ('stage', 'bundle_path', 'bundle_sha256', 'bundle_fnv64', 'usb_dev'):
                    self.assertEqual(context[key], 'none')
                self.assertEqual(context['mode'], mode)
                self.assertEqual(context['action_arg'], arg)
                self.assertEqual(context['usb_guarded'], '0')
                self.assertEqual((self.p / 'label').read_text(), 'rescue\n')
                self.clean()

    def test_candidate_exit_status_and_single_writer_lifetime(self):
        os.mkfifo(self.p / 'release')
        self.core['engine.sh'] = engine_script(self.p, 'candidate', wait=True, result=42)
        self.pack()
        process = subprocess.Popen(self.shell + [str(self.runner)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while not (self.p / 'ready').exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertTrue((self.p / 'ready').exists())
            self.assertEqual(int((self.lock / 'pid').read_text()), process.pid)
            self.assertEqual(int((self.p / 'pid').read_text()), process.pid)
            self.run_dispatch(ok=False)
            self.assertEqual(self.last_rc, 75)
            with (self.p / 'release').open('w') as release:
                release.write('release\n')
            process.communicate(timeout=10)
            self.assertEqual(process.returncode, 42)
            self.assertFalse(self.status.exists())
            self.clean()
        finally:
            if process.poll() is None:
                process.kill(); process.communicate()

    def test_no_usb_complete_installed_core_preferred_partial_or_bad_falls_back(self):
        self.write_installed()
        self.run_dispatch(['--recover'])
        self.assertEqual((self.p / 'label').read_text(), 'candidate\n')
        self.assertEqual(self.context()['engine_sha256'], sha(self.core['engine.sh']))
        self.clean()
        for role in CORES:
            with self.subTest(corrupt_role=role):
                self.write_installed()
                (self.root / CORES[role]).write_bytes(b'PARTIAL OR MIXED CORE')
                self.run_dispatch(['--can-launch'])
                self.assertEqual((self.p / 'label').read_text(), 'rescue\n')
                self.clean()
        for fault in ('mode', 'symlink', 'marker', 'syntax'):
            with self.subTest(fault=fault):
                values = dict(self.core)
                if fault == 'marker': values['engine.sh'] = b'#!/bin/sh\nexit 0\n'
                if fault == 'syntax': values['engine.sh'] = b'#!/bin/sh\n# UN260_PACKAGE_ENGINE_V1\nif\n'
                self.write_installed(values)
                path = self.root / CORES['engine.sh']
                if fault == 'mode': path.chmod(0o600)
                if fault == 'symlink':
                    path.unlink(); path.symlink_to(self.rescue / 'engine.sh')
                self.run_dispatch(['--trial-installed'])
                self.assertEqual((self.p / 'label').read_text(), 'rescue\n')
                self.clean()
                if path.is_symlink(): path.unlink()

    def test_bad_envelopes_missing_bad_core_and_archive_fail_closed(self):
        scenarios = [dict(metadata='schema=2\n'), dict(metadata='product=OTHER\n'),
            dict(metadata='updater_engine_abi=2\n'), dict(metadata='updater_envelope=OTHER\n'),
            dict(omit='sync'), dict(corrupt='engine.sh'), dict(extra={'../outside': b'BAD'})]
        for case in scenarios:
            with self.subTest(case=case):
                self.pack(**case)
                self.run_dispatch(ok=False)
                self.assertFalse((self.p / 'pid').exists())
                self.clean()
        self.pack()
        data = bytearray(self.bundle.read_bytes()); data[-8] ^= 1
        self.bundle.write_bytes(data)
        self.run_dispatch(ok=False)
        self.clean()

    def test_platform_rescue_hash_engine_marker_and_syntax(self):
        self.platform.write_text('UN260_UPDATER_PLATFORM_V1\nEXTRA\n')
        self.run_dispatch(ok=False); self.clean()
        self.platform.write_text('UN260_UPDATER_PLATFORM_V1\n')
        (self.rescue / 'sync').write_bytes(b'bad rescue')
        self.run_dispatch(ok=False); self.clean()
        self.write_rescue()
        for source in (b'#!/bin/sh\nexit 0\n', b'#!/bin/sh\n# UN260_PACKAGE_ENGINE_V1\nif\n'):
            self.core['engine.sh'] = source
            self.pack()
            self.run_dispatch(ok=False); self.clean()

    def test_rerun_uses_latest_archive_and_rejects_gui_stale_fingerprint(self):
        self.run_dispatch()
        self.clean()
        self.core['engine.sh'] = engine_script(self.p, 'next-candidate')
        self.pack()
        self.run_dispatch()
        self.assertEqual((self.p / 'label').read_text(), 'next-candidate\n')
        self.clean()
        self.run_dispatch(['--bundle-fnv', '0000000000000000'], ok=False)
        self.clean()

    def test_symlink_and_oversize_core_are_not_executed(self):
        real_bundle = self.p / 'original.upk'
        self.bundle.rename(real_bundle)
        self.bundle.symlink_to(real_bundle)
        self.run_dispatch(ok=False); self.clean()
        self.bundle.unlink()
        self.core['engine.sh'] += b'#' * 262145
        self.pack()
        self.run_dispatch(ok=False); self.clean()
        self.assertFalse((self.p / 'pid').exists())

    def test_copy_rehash_and_physical_growth_cap(self):
        command_dir = self.p / 'commands'; command_dir.mkdir()
        dd = command_dir / 'dd'
        actual_dd = shlex.quote(shutil.which('dd'))
        for operation in ('corrupt', 'grow'):
            with self.subTest(operation=operation):
                code = '''#!/bin/sh
src=; dest=
for a in "$@"; do case "$a" in if=*) src=${a#if=} ;; of=*) dest=${a#of=} ;; esac; done
case "$dest" in */engine.sh)
ACTION
;; esac
REAL_DD "$@" || exit
case "$dest" in */engine.sh) wc -c < "$dest" > SIZEFILE ;; esac
'''.replace('REAL_DD', actual_dd).replace('SIZEFILE', shlex.quote(str(self.p / 'copied-size')))
                action = ('printf corrupt > "$dest"\nexit 0' if operation == 'corrupt' else
                          actual_dd + ' if=/dev/zero bs=1024 count=512 >> "$src" 2>/dev/null')
                dd.write_text(code.replace('ACTION', action)); dd.chmod(0o755)
                self.pack()
                environment = dict(os.environ, PATH=str(command_dir) + os.pathsep + os.environ['PATH'])
                self.run_dispatch(ok=False, env=environment)
                if operation == 'grow':
                    self.assertEqual(int((self.p / 'copied-size').read_text()), 262144)
                self.assertFalse((self.p / 'pid').exists())
                self.clean()

    def test_argument_validation_no_mutation_and_readonly_busy_is_pending(self):
        for arguments in (['--confirm-trial'], ['--confirm-trial', '0'], ['--confirm-trial', '01'],
                          ['--confirm-trial', '1;touch'], ['--confirm-trial', '1000000000'], ['--recover', 'extra']):
            with self.subTest(arguments=arguments):
                self.run_dispatch(arguments, ok=False)
                self.clean()
        self.lock.mkdir(); (self.lock / 'pid').write_text('OTHER')
        self.run_dispatch(['--unified-pending'])
        self.run_dispatch(['--trial-pending'], ok=False)
        self.assertEqual((self.lock / 'pid').read_text(), 'OTHER')
        self.assertFalse(self.status.exists())

    def test_internal_failures_preserve_public_gui_status_and_update_failure_has_owner(self):
        previous = 'progress=77\nstage=install\nsuccess=0\nwriter_pid=OTHER\n'
        self.status.write_text(previous)
        self.platform.write_text('BAD\n')
        for argument in ('--recover', '--trial-pending', '--trial-installed', '--can-launch'):
            self.run_dispatch([argument], ok=False)
            self.assertEqual(self.status.read_text(), previous)
            self.clean()
        pid, _ = self.run_dispatch(ok=False)
        self.assertEqual(self.status.read_text(), previous)  # Rejected before owning a lock.
        self.platform.write_text('UN260_UPDATER_PLATFORM_V1\n')
        self.bundle.unlink()
        pid, _ = self.run_dispatch(ok=False)
        self.assertIn('writer_pid=%s\n' % pid, self.status.read_text())
        self.assertIn('stage=fail\n', self.status.read_text())
        self.assertNotIn('bundle_fnv64=', self.status.read_text())
        self.clean()

    def test_sigkill_releases_kernel_gate_and_reclaims_only_dead_pid(self):
        os.mkfifo(self.p / 'release')
        self.core['engine.sh'] = engine_script(self.p, 'candidate', wait=True)
        self.pack()
        process = subprocess.Popen(self.shell + [str(self.runner)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while not (self.p / 'ready').exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.01)
            self.assertTrue((self.p / 'ready').exists())
            self.run_dispatch(['--trial-pending'], ok=False)
            self.assertEqual(self.last_rc, 75)
            self.assertEqual((self.lock / 'pid').read_text(), str(process.pid) + '\n')
            process.kill(); process.communicate(timeout=10)
            orphan_ram = list(self.ram.glob('un260-update-engine.*'))
            orphan_stage = list((self.usb / 'update').glob('.un260_dispatch.*'))
            self.assertTrue(orphan_ram and orphan_stage)
            self.run_dispatch(['--recover'])
            self.assertEqual((self.p / 'label').read_text(), 'rescue\n')
            self.assertFalse(self.lock.exists())
            # Reclaim only the known pid record. Unknown old stage/context data
            # remain available; this test does not claim SIGKILL trap cleanup.
            self.assertEqual(list(self.ram.glob('un260-update-engine.*')), orphan_ram)
            self.assertEqual(list((self.usb / 'update').glob('.un260_dispatch.*')), orphan_stage)
        finally:
            if process.poll() is None:
                process.kill(); process.communicate()

    def test_dead_empty_and_unsafe_ram_lock_records(self):
        for empty in (False, True):
            with self.subTest(empty=empty):
                self.lock.mkdir(mode=0o700)
                if not empty:
                    (self.lock / 'pid').write_text('999999999\n')
                    (self.lock / 'pid').chmod(0o600)
                self.run_dispatch(['--recover'])
                self.clean()
        for fault in ('live', 'unknown', 'pid-mode', 'dir-mode', 'pid-link', 'malformed'):
            with self.subTest(fault=fault):
                self.lock.mkdir(mode=0o700)
                pid = self.lock / 'pid'
                pid.write_text(str(os.getpid()) + '\n' if fault == 'live' else '999999999\n')
                pid.chmod(0o600)
                if fault == 'unknown': (self.lock / 'extra').write_text('preserve')
                if fault == 'pid-mode': pid.chmod(0o644)
                if fault == 'dir-mode': self.lock.chmod(0o755)
                if fault == 'pid-link': pid.unlink(); pid.symlink_to(self.platform)
                if fault == 'malformed': pid.write_text('0\n')
                self.run_dispatch(['--recover'], ok=False)
                self.assertTrue(self.lock.exists())
                shutil.rmtree(self.lock)

    def test_exec_failure_runs_exit_cleanup_with_same_pid(self):
        text = self.runner.read_text()
        last = text.rfind('exec ')
        self.assertGreater(last, 0)
        self.runner.write_text(text[:last] + 'exec /__un260_absent_interpreter__ "$CTX/engine.sh" --dispatch-context "$CTX"\n')
        self.run_dispatch(ok=False)
        self.assertFalse((self.p / 'pid').exists())
        self.clean()

    def test_nested_mktemp_result_is_rejected_without_deleting_unknown_path(self):
        commands = self.p / 'commands'; commands.mkdir()
        command = commands / 'mktemp'
        command.write_text('''#!/bin/sh
for template in "$@"; do :; done
result=${template%XXXXXX}ABC123/nested
mkdir -p "$result"
printf preserved > "$result/sentinel"
printf '%s\\n' "$result"
'''); command.chmod(0o755)
        environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ['PATH'])
        self.run_dispatch(ok=False, env=environment)
        paths = list((self.usb / 'update').glob('.un260_dispatch.*/nested/sentinel'))
        self.assertEqual(len(paths), 1)
        self.assertEqual(paths[0].read_text(), 'preserved')
        self.assertFalse(self.lock.exists())
        self.assertFalse((self.p / 'pid').exists())

    def test_ram_copy_signal_and_write_failures_cleanup_before_engine(self):
        commands = self.p / 'commands'; commands.mkdir()
        command = commands / 'dd'
        environment = dict(os.environ, PATH=str(commands) + os.pathsep + os.environ['PATH'])
        for operation in ('exit 1', 'kill -TERM "$(cat %s)"\nexit 0' % shlex.quote(str(self.lock / 'pid'))):
            with self.subTest(operation=operation):
                command.write_text('#!/bin/sh\n' + operation + '\n'); command.chmod(0o755)
                self.run_dispatch(ok=False, env=environment)
                self.assertFalse((self.p / 'pid').exists())
                self.clean()


if __name__ == '__main__':
    unittest.main(verbosity=2)
