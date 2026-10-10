#!/usr/bin/env python3
"""Run the real startup shell functions against an isolated fake device tree.

No real service, module, calibration, updater, sync, governor or device is used.
The top-level service dispatcher is deliberately not sourced.
"""
import argparse
import re
import os
import hashlib
import signal
import pathlib
import subprocess
import tempfile


def run_case(source, name, commands, *, trace=True, touch=True, calibration=True,
             recovery=False, usleep=True, fractional=True, trace_flag=False,
             early=False, early_disabled=False, launcher=False,
             ready_marker=None, owner_comm='un260-boot', owner_alive=True,
             boot_disabled=False, unified=False, unified_completed=False):
    with tempfile.TemporaryDirectory(prefix="un260-startup-test-") as tmp:
        root = pathlib.Path(tmp)
        for sub in ("dev/input", "sys/class/input/event7/device", "etc", "run", "bin", "tmp", "proc/5555"):
            (root / sub).mkdir(parents=True)
        for node in ("fb0", "ge", "ttyS4", "ttyS5", "ttyS6"):
            (root / "dev" / node).touch()
        (root / "sys/class/input/event7/device/name").write_text("goodix-ts\n")
        if touch:
            (root / "dev/input/event7").touch()
        if calibration:
            (root / "etc/pointercal").touch()
        if recovery:
            (root / "transaction").mkdir()
        if trace_flag:
            (root / 'etc/boot-trace.enabled').touch()
        if early_disabled:
            (root / 'etc/early-display.disabled').touch()
        if boot_disabled:
            (root / 'etc/boot-light.disabled').touch()
        if ready_marker is not None:
            (root / 'visual-ready').write_text(ready_marker)
        (root / 'proc/5555/comm').write_text(owner_comm+'\n')
        if owner_alive:
            (root / 'visual-alive').touch()
        (root / 'proc/modules').touch()
        if unified:
            (root / 'etc/unified-request').write_text('a'*64+'\n'+'b'*64+'\n')
        (root / "uptime").write_text("3.70 0.00\n")
        executables = {
            "daemon": ('printf \'DAEMON_OUTPUT\\n\'\n'
                       'printf \'%s\\n\' "${UN260_BOOT_LIGHT_ACTIVE-unset}" > "$CASE_ROOT/daemon-visual-env"\n'),
            "updater": ('# UN260_UNIFIED_1\n'
                        'case "$1" in\n'
                        '--unified-pending) [ "${UNIFIED_COMPLETED:-0}" != 1 ] ;;\n'
                        '--continue-unified) echo continue_unified >> "$CASE_ROOT/events"; [ "${CONTINUE_FAIL:-0}" != 1 ] ;;\n'
                        '*) echo recover >> "$CASE_ROOT/events"; [ "${RECOVER_FAIL:-0}" != 1 ] ;;\n'
                        'esac\n'),
            "calibrate": ('echo calibrate >> "$CASE_ROOT/events"\n'
                          '[ "${CALIBRATE_FAIL:-0}" != 1 ] || exit 1\n'
                          '[ "${CALIBRATE_NO_FILE:-0}" = 1 ] || : > "$CASE_ROOT/etc/pointercal"\n'),
            "boot_launch": ('echo root_launch >> "$CASE_ROOT/events"\n'
                            '[ "${LAUNCH_FAIL:-0}" != 1 ] || exit 1\n'
                            '[ "${LAUNCH_NO_FRAME:-0}" != 1 ] || exit 0\n'
                            'printf \'5555\\n\' > "$CASE_ROOT/visual-ready"\n'
                            'printf \'un260-boot\\n\' > "$CASE_ROOT/proc/5555/comm"\n'
                            ': > "$CASE_ROOT/visual-alive"\n'),
            "storage": ('case "$1" in\n'
                        '--prepare) echo storage_prepare >> "$CASE_ROOT/events" ;;\n'
                        '--can-launch-current) echo can_launch_current >> "$CASE_ROOT/events"; [ "${CURRENT_FAIL:-0}" != 1 ] ;;\n'
                        '*) exit 1 ;;\nesac\n'),
            "upgrade_display": r'''
printf '%s\n' "$$" > "$CASE_ROOT/maintenance-pid"
mkdir -p "$CASE_ROOT/proc/$$"
printf 'un260-boot\n' > "$CASE_ROOT/proc/$$/comm"
printf '%s\n' "$$" > "$CASE_ROOT/visual-ready"
echo maintenance_started >> "$CASE_ROOT/events"
cleanup() {
    echo maintenance_stopped >> "$CASE_ROOT/events"
    if [ "${MAINT_SWAP_OWNER:-0}" = 1 ]; then
        printf '5555\n' > "$CASE_ROOT/visual-ready"
    elif [ "$(cat "$CASE_ROOT/visual-ready" 2>/dev/null)" = "$$" ]; then
        rm -f "$CASE_ROOT/visual-ready"
    fi
    exit 0
}
if [ "${MAINT_IGNORE_TERM:-0}" = 1 ]; then trap '' TERM; else trap cleanup TERM; fi
# Even the stubborn fixture has a bounded natural lifetime; test cleanup
# never needs to kill an untrusted PID or wait for a real service.
for tick in $(seq 1 250); do /bin/sleep 0.01; done
cleanup
''',
        }
        for filename, body in executables.items():
            path = root / "bin" / filename
            path.write_text("#!/bin/sh\n" + body)
            path.chmod(0o755)
        if not launcher:
            (root / 'bin/boot_launch').unlink()
        if not unified:
            (root / 'bin/storage').unlink()
            (root / 'bin/upgrade_display').unlink()

        # Redirect every side-effecting resource; keep /dev/null unchanged.
        replacements = {
            '/usr/local/bin/test_lvgl': str(root / 'bin/daemon'),
            '/var/run/': str(root / 'run') + '/',
            '/tmp/test_lvgl.startup.log': str(root / 'tmp/startup.log'),
            '/etc/pointercal': str(root / 'etc/pointercal'),
            '/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor': str(root / 'governor-absent'),
            '/sys/class/input/event*': str(root / 'sys/class/input/event*'),
            '/dev/input/': str(root / 'dev/input') + '/',
            '/var/lib/un260-updater/transaction': str(root / 'transaction'),
            '/usr/bin/ui_update.sh': str(root / 'bin/updater'),
            '/usr/bin/ts_calibrate': str(root / 'bin/calibrate'),
            '/proc/uptime': str(root / 'uptime'),
            '/etc/un260/boot-trace.enabled': str(root / 'etc/boot-trace.enabled'),
            '/etc/un260/early-display.disabled': str(root / 'etc/early-display.disabled'),
            '/etc/un260/boot-light.disabled': str(root / 'etc/boot-light.disabled'),
            '/etc/un260/boot-frames.enabled': str(root / 'etc/boot-frames.enabled'),
            '/etc/un260/unified-request': str(root / 'etc/unified-request'),
            '/usr/local/bin/un260_app_storage': str(root / 'bin/storage'),
            '/usr/local/bin/un260_upgrade_display': str(root / 'bin/upgrade_display'),
            '/usr/local/bin/un260_boot_launch': str(root / 'bin/boot_launch'),
            '/dev/.un260-boot-light.ready': str(root / 'visual-ready'),
            '/dev/.un260-early.log': str(root / 'early-log-absent'),
            '/proc/$candidate/comm': str(root / 'proc/$candidate/comm'),
            '/proc/modules': str(root / 'proc/modules'),
        }
        for node in ('fb0', 'ge', 'ttyS4', 'ttyS5', 'ttyS6'):
            replacements['/dev/' + node] = str(root / 'dev' / node)
        harness = source[:source.rindex('\ncase "$1" in')]
        for old, new in replacements.items():
            harness = harness.replace(old, new)
        harness = re.sub(r"(?<![A-Za-z0-9_./-])/run/", str(root / "run") + "/", harness)
        # Fake event nodes are ordinary files; production still requires -c.
        harness = harness.replace('[ -c "$candidate" ]', '[ -e "$candidate" ]')
        harness += r'''
record() { printf '%s\n' "$*" >> "$CASE_ROOT/events"; }
pause_mock() {
    record "pause $1"
    # Give only our maintenance fixture child a scheduling opportunity.
    [ ! -f "$CASE_ROOT/etc/unified-request" ] || /bin/sleep 0.005
    count=0
    [ ! -f "$CASE_ROOT/pause-count" ] || read -r count < "$CASE_ROOT/pause-count"
    count=$((count + 1))
    printf '%s\n' "$count" > "$CASE_ROOT/pause-count"
    if [ "$count" = "${MDEV_READY_AFTER:-none}" ]; then
        : > "$CASE_ROOT/mdev-ready"
    fi
    if [ "$count" = "${TOUCH_READY_AFTER:-none}" ]; then
        : > "$CASE_ROOT/dev/input/event7"
    fi
}
usleep() {
    [ "$USLEEP_OK" = 1 ] || return 1
    [ "$1" != 0 ] || return 0
    pause_mock "$1"
}
sleep() {
    case "$1" in
        0.0) [ "$FRACTIONAL_OK" = 1 ] ;;
        0.1) [ "$FRACTIONAL_OK" = 1 ] && pause_mock 100000 ;;
        1) pause_mock 1000000 ;;
        *) echo unexpected_sleep >&2; return 99 ;;
    esac
}
modprobe() { record modprobe; return 0; }
insmod() { record insmod; return 0; }
mdev() {
    record mdev_scan
    if [ "${MDEV_FIX_TOUCH:-0}" = 1 ]; then
        : > "$CASE_ROOT/dev/input/event7"
    fi
}
sync() { record sync; }
kill() {
    owned=""
    [ ! -r "$CASE_ROOT/maintenance-pid" ] || read -r owned < "$CASE_ROOT/maintenance-pid"
    if [ -n "$owned" ] && [ "$2" = "$owned" ]; then
        if [ "$1" != -0 ]; then record "kill $*"; fi
        command kill "$@"
        return $?
    fi
    if [ "$1" != -0 ]; then record "kill $*"; return 0; fi
    case "$2" in
        4242) [ -f "$CASE_ROOT/mdev-ready" ] ;;
        4343) [ -f "$CASE_ROOT/app-ready" ] ;;
        4444) [ -f "$CASE_ROOT/defer-ready" ] ;;
        5555) [ -f "$CASE_ROOT/visual-alive" ] ;;
        *) [ "${HEALTH_FAIL:-0}" != 1 ] ;;
    esac
}
expect() { "$@" || { echo "assertion failed: $*" >&2; exit 97; }; }
expect_not() { if "$@"; then echo "unexpected success: $*" >&2; exit 97; fi; }
count_event() { grep -c "^$1$" "$CASE_ROOT/events" 2>/dev/null || true; }
'''
        # Fake daemon children must finish their fixture writes before Python
        # removes the temporary tree; they never own a real service/device.
        harness += commands + '\nwait 2>/dev/null || true\n'
        path = root / 'harness.sh'
        path.write_text(harness)
        env = {
            'PATH': '/usr/bin:/bin', 'CASE_ROOT': str(root),
            'USLEEP_OK': str(int(usleep)), 'FRACTIONAL_OK': str(int(fractional)),
            'UNIFIED_COMPLETED': str(int(unified_completed)),
        }
        if trace is not None:
            env['UN260_BOOT_TRACE'] = str(int(trace))
        if early is not None:
            env['UN260_EARLY_DISPLAY'] = str(int(early))
        result = subprocess.run((['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['/bin/sh']) + [str(path)], env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
        if result.returncode:
            events = (root / 'events').read_text() if (root / 'events').exists() else ''
            raise AssertionError(f'{name}: exit={result.returncode}\n{result.stdout}\n{events}')
        print(f'PASS {name}')


def run_modern_case(source, name, commands, flags=None, trial=False, installed=False, usb=False):
    with tempfile.TemporaryDirectory(prefix="un260-runtime-test-") as tmp:
        root = pathlib.Path(tmp)
        for directory in ('bin', 'run', 'tmp', 'dev', 'etc', 'proc'):
            (root / directory).mkdir()
        for node in ('fb0', 'ge'): (root / 'dev' / node).touch()
        (root / 'etc/pointercal').touch()
        (root / 'etc/platform').write_text('UN260_UPDATER_PLATFORM_V1\n')
        (root / 'uptime').write_text('7.31 0.00\n')
        (root / 'governor').write_text('ondemand\n')
        if trial: (root / 'trial').touch()
        if installed: (root / 'installed').touch()
        if usb: (root / 'package').write_bytes(b'fixture USB package')
        executables = {
            'daemon': r"""
echo daemon >> "$CASE_ROOT/events"
printf '%s\n' "$$" > "$CASE_ROOT/actual-app"
[ "${APP_DIE:-0}" != 1 ] || exit 8
if [ "${APP_NO_HEALTH:-0}" != 1 ] || [ -e "$CASE_ROOT/rolled" ]; then
    printf '%s\n' "$$" > "$CASE_ROOT/health.new"; chmod 0600 "$CASE_ROOT/health.new"
    mv "$CASE_ROOT/health.new" "$CASE_ROOT/health"
fi
trap 'exit 0' TERM HUP INT
while :; do /bin/sleep 0.01; done
""",
            'dispatch': r"""
case "${1:-update}" in
 --recover) echo recover >> "$CASE_ROOT/events"; [ "${RECOVER_FAIL:-0}" != 1 ] ;;
 --can-launch)
    echo can_launch >> "$CASE_ROOT/events"
    if [ "${ASSERT_PERFORMANCE:-0}" = 1 ]; then
        [ "$(cat "$CASE_ROOT/governor")" = performance ] || exit 12
    fi
    # Real app-check completes recovery before its launch check.
    [ "${RECOVER_FAIL:-0}" != 1 ] || exit 1
    [ "${CURRENT_FAIL:-0}" != 1 ] || exit 1
    [ "${NEEDS_ROLLBACK:-0}" != 1 ] || [ -e "$CASE_ROOT/rolled" ] || exit 1
    [ "${APP_DIE:-0}" != 1 ] || [ ! -e "$CASE_ROOT/rolled" ] || exit 1
    printf '%s\n' "$CASE_ROOT/bin/daemon" ;;
 --trial-pending)
    echo pending >> "$CASE_ROOT/events"
    [ "${TRIAL_UNKNOWN:-0}" != 1 ] || exit 2
    [ -e "$CASE_ROOT/trial" ] && [ ! -e "$CASE_ROOT/rolled" ] ;;
 --trial-installed)
    echo installed >> "$CASE_ROOT/events"
    [ "${TRIAL_UNKNOWN:-0}" != 1 ] || exit 2
    [ -e "$CASE_ROOT/installed" ] && [ ! -e "$CASE_ROOT/rolled" ] ;;
 --confirm-trial)
    echo "confirm $2" >> "$CASE_ROOT/events"
    [ "$(cat "$CASE_ROOT/health")" = "$2" ] || exit 1
    [ "${CONFIRM_FAIL:-0}" != 1 ] ;;
 --rollback-trial)
    echo rollback >> "$CASE_ROOT/events"
    [ "${ROLLBACK_FAIL:-0}" != 1 ] || exit 1
    if [ -r "$CASE_ROOT/actual-app" ]; then
        read -r app < "$CASE_ROOT/actual-app"
        kill -0 "$app" 2>/dev/null && { echo unsafe_rollback_live >> "$CASE_ROOT/events"; exit 9; }
    fi
    : > "$CASE_ROOT/rolled" ;;
 update) echo update >> "$CASE_ROOT/events"; [ "${UPDATE_FAIL:-0}" != 1 ] ;;
 *) exit 2 ;;
esac
""",
            'display': r"""
echo display >> "$CASE_ROOT/events"
mkdir -p "$CASE_ROOT/proc/$$"
printf 'un260-boot\n' > "$CASE_ROOT/proc/$$/comm"
trap 'echo display_stop >> "$CASE_ROOT/events"; exit 0' TERM HUP INT
while :; do /bin/sleep 0.01; done
""",
            'reboot': 'echo reboot >> "$CASE_ROOT/events"\n',
        }
        for filename, body in executables.items():
            path = root / 'bin' / filename
            path.write_text('#!/bin/sh\n' + body); path.chmod(0o755)
        (root / 'bin/display.sha256').write_text(hashlib.sha256((root / 'bin/display').read_bytes()).hexdigest() + '\n')
        replacements = {
            '/usr/local/bin/test_lvgl': str(root / 'bin/legacy-unused'),
            '/mnt/un260-app/test_lvgl': str(root / 'bin/daemon'),
            '/usr/local/bin/un260_update_dispatch': str(root / 'bin/dispatch'),
            '/usr/local/lib/un260-updater/rescue/display': str(root / 'bin/display'),
            '/etc/un260/updater-platform': str(root / 'etc/platform'),
            '/tmp/un260-ui.health': str(root / 'health'),
            '/tmp/un260-recovery.heartbeat': str(root / 'heartbeat'),
            '/tmp/ui_update.status': str(root / 'status'),
            '/tmp/un260-updater.lock': str(root / 'backend-lock'),
            '/mnt/usb/update/UN260_UPDATE.upk': str(root / 'package'),
            '/var/run/': str(root / 'run') + '/',
            '/tmp/test_lvgl.startup.log': str(root / 'tmp/startup.log'),
            '/etc/pointercal': str(root / 'etc/pointercal'),
            '/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor': str(root / 'governor'),
            '/proc/uptime': str(root / 'uptime'),
            '/dev/fb0': str(root / 'dev/fb0'), '/dev/ge': str(root / 'dev/ge'),
            '/dev/.un260-boot-light.ready': str(root / 'no-owner'),
            '/dev/.un260-early.log': str(root / 'no-early-log'),
            '/usr/local/bin/un260_boot_launch': str(root / 'no-launcher'),
            '/proc/$rescue_display_pid/comm': str(root / 'proc/$rescue_display_pid/comm'),
            '/sbin/reboot': str(root / 'bin/reboot'),
        }
        harness = source[:source.rindex('\ncase "$1" in')]
        for old, new in replacements.items(): harness = harness.replace(old, new)
        harness = re.sub(r"(?<![A-Za-z0-9_./-])/run/", str(root / "run") + "/", harness)
        # This I/O suite uses a shell daemon; actual ELF/parent ownership is
        # tested with production runtime_guard.c by test_startup_health.py.
        harness += "\nmanaged_process() { process_identity \"$1\"; }\n"
        harness += r"""
expect() { "$@" || { echo "assertion failed: $*" >&2; exit 97; }; }
count_event() { grep -c "^$1$" "$CASE_ROOT/events" 2>/dev/null || true; }
usleep() { [ "$1" = 0 ] || /bin/sleep 0.002; }
sleep() {
    case "$1" in
      0.0) return 0 ;;
      0.1) /bin/sleep 0.002 ;;
      1) /bin/sleep 0.01 ;;
      2)
        tick=0
        [ ! -r "$CASE_ROOT/tick" ] || read -r tick < "$CASE_ROOT/tick"
        tick=$((tick + 1)); echo "$tick" > "$CASE_ROOT/tick"
        if [ "$tick" = "${RELEASE_LOCK_AFTER:-none}" ]; then
            rm -f "$CASE_ROOT/backend-lock/pid"; rmdir "$CASE_ROOT/backend-lock"
        fi
        if [ "${USB_REINSERT:-0}" = 1 ]; then
            [ "$tick" != 1 ] || rm -f "$CASE_ROOT/package"
            [ "$tick" != 2 ] || printf 'replacement\n' > "$CASE_ROOT/package"
        fi
        if [ "$tick" -ge "${MAX_RECOVERY_TICKS:-3}" ]; then exit 0; fi
        /bin/sleep 0.01 ;;
      *) echo unexpected_sleep >&2; return 99 ;;
    esac
}
UN260_MDEV_READY=1
UN260_EARLY_DISPLAY=1
"""
        harness += commands
        harness += r"""
stop
[ ! -r "$CASE_ROOT/actual-app" ] || {
    read -r remaining < "$CASE_ROOT/actual-app"
    command kill -TERM "$remaining" 2>/dev/null || true
    wait "$remaining" 2>/dev/null || true
}
wait 2>/dev/null || true
"""
        path = root / 'harness'; path.write_text(harness)
        env = dict(os.environ)
        env.update({'CASE_ROOT': str(root), 'UN260_BOOT_TRACE': '1', 'UN260_BOOT_FRAME_TRACE': '0'})
        env.update(flags or {})
        shell = ['busybox', 'sh'] if os.environ.get('TEST_BUSYBOX') == '1' else ['/bin/sh']
        process = subprocess.Popen(shell + [str(path)], env=env, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            output, _ = process.communicate(timeout=20)
        finally:
            # The process group was created solely for these fixture children.
            try: os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            if process.poll() is None:
                process.wait(timeout=3)
        events = (root / 'events').read_text() if (root / 'events').exists() else ''
        if process.returncode:
            logs = (root / 'tmp/startup.log').read_text() if (root / 'tmp/startup.log').exists() else ''
            raise AssertionError(f'{name}: exit={process.returncode}\n{output}\n{events}\n{logs}')
        print('PASS ' + name)


def run_modern_cases(source):
    common = 'start\nwait "$recovery_pid"\n'
    cases = [
        ('newbase_first_paint_confirm', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event recover)" = 0 ]
expect [ "$(count_event can_launch)" = 1 ]
expect [ "$(count_event rollback)" = 0 ]
expect grep -q '^confirm ' "$CASE_ROOT/events"
expect [ "$(count_event update)" = 0 ]
expect [ "$(count_event display)" = 0 ]
expect [ ! -e "$RECOVERY_PIDFILE" ]
""", {}, True, False, False),
        ('newbase_ordinary_health_no_trial', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event rollback)" = 0 ]
expect [ "$(count_event update)" = 0 ]
expect_not grep -q '^confirm ' "$CASE_ROOT/events"
""", {}, False, False, False),
        ('newbase_health_timeout_rolls_back_stopped_candidate', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event rollback)" = 1 ]
expect [ "$(count_event daemon)" = 2 ]
expect [ "$(count_event unsafe_rollback_live)" = 0 ]
expect [ "$(count_event update)" = 0 ]
""", {'APP_NO_HEALTH': '1'}, True, False, False),
        ('newbase_missing_app_rolls_back_before_launch', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event rollback)" = 1 ]
expect [ "$(count_event daemon)" = 1 ]
""", {'NEEDS_ROLLBACK': '1'}, True, False, False),
        ('newbase_no_app_USB_then_reboot', common + """
expect [ "$?" != 0 ]
expect [ "$(count_event daemon)" = 0 ]
expect [ "$(count_event update)" = 1 ]
expect [ "$(count_event reboot)" = 1 ]
expect [ ! -e "$RECOVERY_HEARTBEAT" ]
""", {'CURRENT_FAIL': '1'}, False, False, True),
        ('newbase_same_boot_trial_reboots_once', common + """
expect [ "$?" != 0 ]
expect [ "$(count_event rollback)" = 0 ]
expect [ "$(count_event reboot)" = 1 ]
expect [ "$(count_event update)" = 0 ]
""", {'CURRENT_FAIL': '1'}, False, True, True),
        ('newbase_unknown_trial_no_rollback_reboot', common + """
expect [ "$(count_event rollback)" = 0 ]
expect [ "$(count_event reboot)" = 0 ]
expect [ "$(count_event update)" = 1 ]
expect [ ! -e "$RECOVERY_PIDFILE" ]
""", {'CURRENT_FAIL': '1', 'TRIAL_UNKNOWN': '1', 'UPDATE_FAIL': '1'}, False, False, True),
        ('newbase_bad_USB_attempt_once', common + """
expect [ "$(count_event update)" = 1 ]
expect [ "$(count_event reboot)" = 0 ]
expect [ "$(count_event daemon)" = 0 ]
""", {'CURRENT_FAIL': '1', 'UPDATE_FAIL': '1'}, False, False, True),
        ('newbase_USB_reinsert_retries', common + 'expect [ "$(count_event update)" = 2 ]\n',
         {'CURRENT_FAIL': '1', 'UPDATE_FAIL': '1', 'USB_REINSERT': '1', 'MAX_RECOVERY_TICKS': '4'}, False, False, True),
        ('newbase_rescue_only_rollback', """
select_poll_method
start_runtime_worker rescue
wait "$recovery_pid"
expect [ "$?" = 0 ]
expect [ "$(count_event rollback)" = 1 ]
expect [ "$(count_event daemon)" = 1 ]
""", {}, True, False, False),
        ('newbase_rescue_only_no_blind_restart', """
select_poll_method
start_runtime_worker rescue
wait "$recovery_pid"
expect [ "$(count_event daemon)" = 0 ]
expect [ "$(count_event rollback)" = 0 ]
expect [ "$(count_event display)" = 1 ]
""", {'UPDATE_FAIL': '1'}, False, False, True),
        ('newbase_invalid_display_not_exec', """
printf '%065d' 0 > "$RESCUE_DISPLAY.sha256"
""" + common + """
expect [ "$(count_event display)" = 0 ]
expect [ "$(count_event update)" = 1 ]
""", {'CURRENT_FAIL': '1', 'UPDATE_FAIL': '1'}, False, False, True),
        ('newbase_marker_shape_and_extra_bytes', r"""
printf '4343\n' > "$UI_HEALTH"; chmod 0600 "$UI_HEALTH"
expect health_marker_matches 4343
printf '4343' > "$UI_HEALTH"; expect_not health_marker_matches 4343
printf '4343\nx' > "$UI_HEALTH"; expect_not health_marker_matches 4343
printf '4343\r\n' > "$UI_HEALTH"; expect_not health_marker_matches 4343
printf '4343\n' > "$UI_HEALTH"; chmod 0666 "$UI_HEALTH"; expect_not health_marker_matches 4343
chmod 0600 "$UI_HEALTH"; mv "$UI_HEALTH" "$UI_HEALTH.old"
ln -s "$UI_HEALTH.old" "$UI_HEALTH"; expect_not health_marker_matches 4343
rm "$UI_HEALTH"
printf '5555\n' > "$UI_HEALTH"; chmod 0600 "$UI_HEALTH"
clear_owned_health 4343
expect [ -f "$UI_HEALTH" ]
""", {}, False, False, False),
    ]
    cases.extend([
        ('newbase_single_worker_stop_cleans_own_processes', """
start
first_worker=$recovery_pid
/bin/sleep 0.02
expect owned_runtime_worker
start
expect [ "$recovery_pid" = "$first_worker" ]
stop
expect_not kill -0 "$first_worker"
expect [ ! -e "$RECOVERY_PIDFILE" ]
expect [ ! -e "$RECOVERY_TOKENFILE" ]
expect [ ! -e "$RECOVERY_HEARTBEAT" ]
""", {'APP_NO_HEALTH': '1'}, False, False, False),
        ('newbase_stale_worker_PID_does_not_kill_other_process', """
/bin/sleep 5 &
stranger=$!
printf '%s\\n' "$stranger" > "$RECOVERY_PIDFILE"
printf '%s 0\\n' "$stranger" > "$RECOVERY_TOKENFILE"
stop_runtime_worker
expect kill -0 "$stranger"
command kill -TERM "$stranger"
wait "$stranger" 2>/dev/null || true
""", {}, False, False, False),
        ('newbase_failed_rollback_waits_USB_without_launch', """
start
wait "$recovery_pid"
expect [ "$(count_event rollback)" = 1 ]
expect [ "$(count_event daemon)" = 0 ]
expect [ "$(count_event reboot)" = 0 ]
expect [ "$(count_event update)" = 1 ]
""", {'CURRENT_FAIL': '1', 'ROLLBACK_FAIL': '1', 'UPDATE_FAIL': '1'}, True, True, True),
    ])
    cases.extend([
        ('newbase_wait_status_replaces_previous_success', """
mv() {
    if [ "$3" = "$RECOVERY_STATUS" ]; then
        expect [ "$(ls -ld "$UPDATE_LOCK/pid" | awk '{print $1}')" = -rw------- ]
        printf 'private_publisher_lock\\n' >> "$CASE_ROOT/events"
    fi
    command mv "$@"
}
printf 'progress=100\\nsuccess=1\\nstage=success\\n' > "$RECOVERY_STATUS"
start
saved_worker=$recovery_pid
wait "$recovery_pid"
expect grep -q '^stage=prepare$' "$RECOVERY_STATUS"
expect grep -q '^message=Insert USB drive with update/UN260_UPDATE.upk$' "$RECOVERY_STATUS"
expect grep -q "^writer_pid=$saved_worker$" "$RECOVERY_STATUS"
expect_not grep -q '^success=1$' "$RECOVERY_STATUS"
expect [ ! -e "$UPDATE_LOCK" ]
expect [ "$(count_event private_publisher_lock)" = 1 ]
""", {'CURRENT_FAIL': '1', 'UPDATE_FAIL': '1'}, False, False, True),
        ('newbase_active_updater_lock_is_preserved', """
mkdir -m 0700 "$UPDATE_LOCK"
printf '%s\\n' "$$" > "$UPDATE_LOCK/pid"; chmod 0600 "$UPDATE_LOCK/pid"
printf 'existing-owner-status\\n' > "$RECOVERY_STATUS"
start
wait "$recovery_pid"
expect [ "$(count_event update)" = 0 ]
expect [ "$(cat "$RECOVERY_STATUS")" = existing-owner-status ]
expect [ "$(cat "$UPDATE_LOCK/pid")" = "$$" ]
""", {'CURRENT_FAIL': '1'}, False, False, True),
        ('newbase_USB_retry_after_active_lock_released', """
mkdir -m 0700 "$UPDATE_LOCK"
printf '%s\\n' "$$" > "$UPDATE_LOCK/pid"; chmod 0600 "$UPDATE_LOCK/pid"
start
wait "$recovery_pid"
expect [ "$(count_event update)" = 1 ]
expect [ "$(count_event reboot)" = 1 ]
""", {'CURRENT_FAIL': '1', 'RELEASE_LOCK_AFTER': '1'}, False, False, True),
    ])
    cases.extend([
        ('newbase_known_dead_lock_delegates_to_dispatcher', """
mkdir -m 0700 "$UPDATE_LOCK"
printf '999999999\\n' > "$UPDATE_LOCK/pid"; chmod 0600 "$UPDATE_LOCK/pid"
start
wait "$recovery_pid"
expect [ "$(count_event update)" = 1 ]
expect [ "$(cat "$UPDATE_LOCK/pid")" = 999999999 ]
""", {'CURRENT_FAIL': '1', 'UPDATE_FAIL': '1', 'MAX_RECOVERY_TICKS': '1'}, False, False, True),
        ('newbase_unknown_lock_shape_blocks_dispatcher', """
mkdir -m 0755 "$UPDATE_LOCK"
printf '999999999\\n' > "$UPDATE_LOCK/pid"; chmod 0600 "$UPDATE_LOCK/pid"
start
wait "$recovery_pid"
expect [ "$(count_event update)" = 0 ]
expect [ "$(cat "$UPDATE_LOCK/pid")" = 999999999 ]
""", {'CURRENT_FAIL': '1'}, False, False, True),
    ])
    cases.extend([
        ('newbase_one_launch_check_at_performance', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event recover)" = 0 ]
expect [ "$(count_event can_launch)" = 1 ]
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = performance ]
expect [ "$(cat "$CPUFREQ_SAVED")" = ondemand ]
expect grep -q 'stage=app_launch_check_begin$' "$STARTUP_LOG"
expect grep -q 'stage=app_launch_check_complete$' "$STARTUP_LOG"
stop
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = ondemand ]
expect [ ! -e "$CPUFREQ_SAVED" ]
""", {'ASSERT_PERFORMANCE': '1'}, False, False, False),
        ('newbase_recovery_inside_check_failure_restores_governor', """
start_ready
expect [ "$?" != 0 ]
expect [ "$(count_event recover)" = 0 ]
expect [ "$(count_event can_launch)" = 1 ]
expect [ "$(count_event daemon)" = 0 ]
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = ondemand ]
expect [ ! -e "$CPUFREQ_SAVED" ]
""", {'RECOVER_FAIL': '1', 'ASSERT_PERFORMANCE': '1'}, False, False, False),
        ('newbase_display_failure_restores_governor', """
rm "$CASE_ROOT/dev/fb0"
start_ready
expect [ "$?" != 0 ]
expect [ "$(count_event daemon)" = 0 ]
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = ondemand ]
expect [ ! -e "$CPUFREQ_SAVED" ]
""", {'ASSERT_PERFORMANCE': '1'}, False, False, False),
        ('newbase_cancel_restores_governor', """
start
/bin/sleep 0.02
stop
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = ondemand ]
expect [ ! -e "$CPUFREQ_SAVED" ]
""", {'APP_NO_HEALTH': '1'}, False, False, False),
        ('newbase_health_failure_USB_wait_restores_governor', common + """
expect [ "$(cat "$CPUFREQ_GOVERNOR")" = ondemand ]
expect [ ! -e "$CPUFREQ_SAVED" ]
expect [ "$(count_event display)" = 1 ]
""", {'APP_NO_HEALTH': '1'}, False, False, False),
        ('newbase_stage_trace_disabled_without_boot_delay', common + """
expect [ "$?" = 0 ]
expect [ "$(count_event can_launch)" = 1 ]
expect_not grep -q 'BOOT_TIME' "$STARTUP_LOG"
""", {'UN260_BOOT_TRACE': '0'}, False, False, False),
    ])
    for name, commands, flags, trial, installed, usb in cases:
        run_modern_case(source, name, commands, flags, trial, installed, usb)
    print(f'{len(cases)} modern runtime isolated cases passed; no real device/service actions')


def run_health_writer():
    sdk = pathlib.Path(__file__).resolve().parents[2]
    driver_source = (sdk / 'source/artinchip/test-lvgl/lv_port_disp.c').read_text()
    notify = driver_source.split('static void notify_present(void)', 1)[1].split('\n}', 1)[0]
    assert '++g_present_sequence' in notify
    assert 'startup_visual_note_ui_presented' not in notify
    main_source = (sdk / 'source/artinchip/test-lvgl/main.c').read_text()
    assert 'if (startup_ready && !native_waiting &&' in main_source
    assert 'fbdev_present_sequence() != startup_health_sequence)' in main_source
    assert 'startup_health_sequence = fbdev_present_sequence();' in main_source
    assert 'if (first_frame_presented && ui_history_data_is_initialized())' in main_source
    adopt = driver_source.split('bool lv_port_disp_adopt_scanout(void)', 1)[1].split('\n}', 1)[0]
    assert 'startup_visual_note_ui_presented' not in adopt
    with tempfile.TemporaryDirectory(prefix='un260-health-C-') as tmp:
        root = pathlib.Path(tmp); marker = root / 'marker'; harness = root / 'health.c'
        harness.write_text(r"""
#define _GNU_SOURCE
#include "un260/lv_drivers/startup_visual.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int fds(void) {
    DIR *d=opendir("/proc/self/fd"); assert(d); int n=0;
    while(readdir(d))++n;
    closedir(d); return n;
}
int main(int argc,char **argv) {
    const char *path=argv[1]; struct stat st;
    int before=fds(); char expected[32],record[32]={0};
    snprintf(expected,sizeof(expected),"%ld\n",(long)getpid());
    if(argc==2) {
        assert(access(path,F_OK)<0);
        startup_visual_note_ui_presented();
        assert(stat(path,&st)==0 && (st.st_mode&0777)==0600);
        FILE *f=fopen(path,"r"); assert(f);
        assert(fread(record,1,sizeof(record),f)==strlen(expected));
        fclose(f); assert(!strcmp(record,expected));
        ino_t inode=st.st_ino;
        startup_visual_note_ui_presented();
        assert(stat(path,&st)==0 && st.st_ino==inode);
    } else {
        assert(mkdir(path,0700)==0);
        startup_visual_note_ui_presented();
        assert(stat(path,&st)==0 && S_ISDIR(st.st_mode));
        startup_visual_note_ui_presented();
    }
    assert(fds()==before);
    return 0;
}
""")
        binary = root / 'health'
        compile_command = [
            'gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-ffunction-sections', '-fdata-sections', '-fsanitize=undefined',
            '-DSTARTUP_VISUAL_UI_HEALTH="' + str(marker) + '"',
            '-I' + str(sdk / 'source/artinchip/test-lvgl'),
            str(sdk / 'source/artinchip/test-lvgl/un260/lv_drivers/startup_visual.c'),
            str(harness), '-Wl,--gc-sections', '-o', str(binary)]
        compiled = subprocess.run(compile_command, capture_output=True, text=True)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        subprocess.run([str(binary), str(marker)], check=True)
        marker.unlink()
        subprocess.run([str(binary), str(marker), 'rename-failure'], check=True)
        assert list(root.glob('marker.*')) == []
        print('PASS health_actual_source_UBSan_atomic_PID_once_FD_cleanup_adopt_no_notify')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parents[2] / 'package/artinchip/test-lvgl/S20test_lvgl')
    args = parser.parse_args()
    source = args.source.read_text()
    subprocess.run(['/bin/sh', '-n', str(args.source)], check=True)
    assert source.count(': > "$STARTUP_LOG"') == 1
    assert 'if [ -c "$candidate" ]; then' in source
    assert 'sleep 1\n      if ! kill -0 "$PID"' in source
    cases = [
        ('display_ready_before_mdev', r'''
start
wait "$defer_pid"
expect [ "$?" = 0 ]
expect grep -q stage=display_before_mdev "$STARTUP_LOG"
expect grep -q stage=daemon_launch "$STARTUP_LOG"
expect [ "$(count_event modprobe)" = 0 ]
''', {'early': True}),
        ('early_default_enabled', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_STARTUP_PREPARE" = 1 ]
''', {'early': None}),
        ('early_persistent_legacy_fallback', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_STARTUP_PREPARE" = 0 ]
expect [ "$LVGL_EVDEV_DEVICE" = "$CASE_ROOT/dev/input/event7" ]
''', {'early': None, 'early_disabled': True}),
        ('early_display_does_not_wait_for_touch_or_serial', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
rm "$CASE_ROOT/dev/ttyS4"
start
expect [ "$?" = 0 ]
expect [ "$UN260_STARTUP_PREPARE" = 1 ]
expect [ "${LVGL_EVDEV_DEVICE-unset}" = unset ]
expect [ "$(count_event modprobe)" = 0 ]
expect [ "$(count_event mdev_scan)" = 0 ]
expect grep -q stage=display_devices_ready "$STARTUP_LOG"
''', {'touch': False}),
        ('early_missing_display_fails', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
rm "$CASE_ROOT/dev/ge"
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
''', {}),
        ('early_first_calibration_keeps_legacy_sequence', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_STARTUP_PREPARE" = 0 ]
expect [ "$(count_event calibrate)" = 1 ]
expect [ "$(count_event sync)" = 1 ]
expect [ "$LVGL_EVDEV_DEVICE" = "$CASE_ROOT/dev/input/event7" ]
''', {'calibration': False}),
        ('worker_prepare_does_not_launch_daemon', r'''
prepare_devices
expect [ "$?" = 0 ]
expect [ ! -e "$PIDFILE" ]
expect grep -q stage=devices_ready "$STARTUP_LOG"
''', {}),
        ('ready', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$(count_event mdev_scan)" = 0 ]
expect [ "$(count_event sync)" = 0 ]
expect [ "$TOUCH_DEVICE" = "$CASE_ROOT/dev/input/event7" ]
expect grep -q stage=start_requested "$STARTUP_LOG"
expect grep -q stage=devices_ready "$STARTUP_LOG"
expect grep -q stage=daemon_healthy "$STARTUP_LOG"
''', {}),
        ('trace_disabled', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect test "$(grep -c BOOT_TIME "$STARTUP_LOG")" = 0
''', {'trace': False}),
        ('trace_default_off', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_BOOT_TRACE" = 0 ]
expect test "$(grep -c BOOT_TIME "$STARTUP_LOG")" = 0
''', {'trace': None}),
        ('trace_flag_enables_cold_boot', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_BOOT_TRACE" = 1 ]
expect grep -q stage=start_requested "$STARTUP_LOG"
expect /bin/sh -c '[ "$UN260_BOOT_TRACE" = 1 ]'
''', {'trace': None, 'trace_flag': True}),
        ('trace_explicit_off_overrides_flag', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$UN260_BOOT_TRACE" = 0 ]
expect test "$(grep -c BOOT_TIME "$STARTUP_LOG")" = 0
''', {'trace': False, 'trace_flag': True}),
        ('deferred_log_preserved', r'''
echo 4242 > "$MDEV_PIDFILE"
MDEV_READY_AFTER=2
start
wait "$defer_pid"
expect [ "$?" = 0 ]
expect [ ! -e "$DEFER_PIDFILE" ]
expect [ "$(count_event 'pause 100000')" = 2 ]
expect grep -q stage=start_requested "$STARTUP_LOG"
expect grep -q stage=mdev_wait_begin "$STARTUP_LOG"
expect grep -q stage=mdev_ready "$STARTUP_LOG"
expect grep -q stage=daemon_launch "$STARTUP_LOG"
''', {}),
        ('mdev_timeout', r'''
start
wait "$defer_pid"
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
expect [ ! -e "$DEFER_PIDFILE" ]
expect [ "$(count_event 'pause 100000')" = 100 ]
expect grep -q stage=mdev_timeout "$STARTUP_LOG"
''', {}),
        ('missing_touch_rescan', r'''
UN260_MDEV_READY=1
MDEV_FIX_TOUCH=1
start
expect [ "$?" = 0 ]
expect [ "$(count_event mdev_scan)" = 1 ]
expect [ "$TOUCH_DEVICE" = "$CASE_ROOT/dev/input/event7" ]
''', {'touch': False}),
        ('missing_touch_delayed', r'''
UN260_MDEV_READY=1
TOUCH_READY_AFTER=3
start
expect [ "$?" = 0 ]
expect [ "$(count_event mdev_scan)" = 1 ]
expect [ "$(count_event 'pause 100000')" = 3 ]
''', {'touch': False}),
        ('missing_touch_timeout', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$(count_event mdev_scan)" = 1 ]
expect [ "$(count_event 'pause 100000')" = 100 ]
''', {'touch': False}),
        ('missing_ge_still_blocks_launch', r'''
UN260_MDEV_READY=1
rm -f "$CASE_ROOT/dev/ge"
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$(count_event mdev_scan)" = 1 ]
expect [ "$(count_event 'pause 100000')" = 100 ]
''', {}),
        ('fractional_fallback', r'''
select_poll_method
expect [ "$POLL_METHOD" = fractional ]
wait_required_devices
expect [ "$?" = 1 ]
expect [ "$(count_event 'pause 100000')" = 100 ]
''', {'touch': False, 'usleep': False}),
        ('integer_sleep_fallback', r'''
select_poll_method
expect [ "$POLL_METHOD" = seconds ]
wait_required_devices
expect [ "$?" = 1 ]
expect [ "$(count_event 'pause 1000000')" = 10 ]
''', {'touch': False, 'usleep': False, 'fractional': False}),
        ('recovery_failure_blocks_launch', r'''
UN260_MDEV_READY=1
export RECOVER_FAIL=1
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$(count_event recover)" = 1 ]
expect [ "$(count_event modprobe)" = 0 ]
''', {'recovery': True}),
        ('recovery_success_before_devices', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$(head -n 1 "$CASE_ROOT/events")" = recover ]
expect [ "$(count_event recover)" = 1 ]
expect [ "$(count_event sync)" = 0 ]
''', {'recovery': True}),
        ('first_calibration_sync', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
expect [ "$(count_event calibrate)" = 1 ]
expect [ "$(count_event sync)" = 1 ]
''', {'calibration': False}),
        ('calibration_failure_blocks_launch', r'''
UN260_MDEV_READY=1
export CALIBRATE_FAIL=1
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$(count_event sync)" = 0 ]
''', {'calibration': False}),
        ('calibration_no_output_blocks_launch', r'''
UN260_MDEV_READY=1
export CALIBRATE_NO_FILE=1
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
''', {'calibration': False}),
        ('already_running_preserves_log', r'''
echo 4343 > "$PIDFILE"
: > "$CASE_ROOT/app-ready"
echo original > "$STARTUP_LOG"
start
expect [ "$?" = 0 ]
expect [ "$(cat "$STARTUP_LOG")" = original ]
''', {}),
        ('already_deferred_preserves_log', r'''
echo 4444 > "$DEFER_PIDFILE"
: > "$CASE_ROOT/defer-ready"
echo original > "$STARTUP_LOG"
start
expect [ "$?" = 0 ]
expect [ "$(cat "$STARTUP_LOG")" = original ]
''', {}),
        ('health_failure_removes_pid', r'''
UN260_MDEV_READY=1
HEALTH_FAIL=1
start
expect [ "$?" = 1 ]
expect [ ! -e "$PIDFILE" ]
''', {}),
        ('stop_pending_worker', r'''
# Signals are confined to the worker spawned by this test, not a real service.
kill() { command kill "$@"; }
pause_mock() { record "pause $1"; /bin/sleep 0.01; }
start
owned_worker=$defer_pid
stop
wait "$owned_worker" 2>/dev/null || true
expect [ ! -e "$DEFER_PIDFILE" ]
expect [ ! -e "$PIDFILE" ]
expect test "$(grep -c stage=daemon_launch "$STARTUP_LOG")" = 0
''', {}),
        ('root_launch_first_frame_success', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 1 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
expect [ "$(cat "$CASE_ROOT/daemon-visual-env")" = 1 ]
expect grep -q stage=root_visual_found "$STARTUP_LOG"
''', {'launcher': True}),
        ('root_launch_exec_failure_falls_back', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export LAUNCH_FAIL=1
export UN260_BOOT_LIGHT_ACTIVE=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 1 ]
expect [ "${UN260_BOOT_LIGHT_ACTIVE-unset}" = unset ]
expect [ "$(cat "$CASE_ROOT/daemon-visual-env")" = unset ]
expect grep -q stage=root_visual_fallback "$STARTUP_LOG"
''', {'launcher': True}),
        ('root_launch_success_without_owner_is_fallback', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export LAUNCH_NO_FRAME=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "${UN260_BOOT_LIGHT_ACTIVE-unset}" = unset ]
expect grep -q stage=root_visual_fallback "$STARTUP_LOG"
''', {'launcher': True}),
        ('root_launch_reuses_existing_owner', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 0 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
expect [ "$light_pid" = 5555 ]
expect grep -q stage=initramfs_visual_found "$STARTUP_LOG"
''', {'launcher': True, 'ready_marker': '5555\n'}),
        ('root_launch_persistent_disabled', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 0 ]
expect [ "${UN260_BOOT_LIGHT_ACTIVE-unset}" = unset ]
''', {'launcher': True, 'boot_disabled': True}),
        ('root_launch_early_display_disabled', r'''
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 0 ]
expect [ "${UN260_BOOT_LIGHT_ACTIVE-unset}" = unset ]
''', {'launcher': True, 'early': None, 'early_disabled': True}),
        ('root_launch_missing_binary_fallback', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export UN260_BOOT_LIGHT_ACTIVE=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 0 ]
expect [ "${UN260_BOOT_LIGHT_ACTIVE-unset}" = unset ]
''', {}),
        ('unified_success_releases_maintenance_before_brand', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event continue_unified)" = 1 ]
expect [ "$(count_event maintenance_stopped)" = 1 ]
expect [ "$(count_event root_launch)" = 1 ]
expect [ -z "$upgrade_visual" ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
expect awk '/maintenance_stopped/{stopped=NR}/root_launch/{launched=NR}END{exit !(stopped && launched>stopped)}' "$CASE_ROOT/events"
expect grep -q stage=upgrade_visual_released "$STARTUP_LOG"
''', {'unified': True, 'launcher': True}),
        ('unified_verified_current_fallback_releases_maintenance', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export CONTINUE_FAIL=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event can_launch_current)" = 1 ]
expect [ "$(count_event maintenance_stopped)" = 1 ]
expect [ "$(count_event root_launch)" = 1 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
expect grep -q stage=unified_current_fallback "$STARTUP_LOG"
''', {'unified': True, 'launcher': True}),
        ('unified_unlaunchable_current_keeps_maintenance', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export CONTINUE_FAIL=1 CURRENT_FAIL=1
start
expect [ "$?" = 1 ]
expect [ "$(count_event root_launch)" = 0 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$(count_event "kill -TERM $upgrade_visual")" = 0 ]
# Explicitly finish the owned fixture after checking production's fail path.
command kill -TERM "$upgrade_visual"
wait "$upgrade_visual" 2>/dev/null || true
''', {'unified': True, 'launcher': True}),
        ('unified_owned_child_timeout_blocks_second_writer', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export MAINT_IGNORE_TERM=1
start
expect [ "$?" = 1 ]
expect [ "$(count_event root_launch)" = 0 ]
expect [ ! -e "$PIDFILE" ]
expect [ "$visual_wait" = "$POLL_LIMIT" ]
''', {'unified': True, 'launcher': True}),
        ('unified_changed_marker_never_kills_new_owner', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
export MAINT_SWAP_OWNER=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event "kill -TERM 5555")" = 0 ]
expect [ "$(count_event maintenance_stopped)" = 1 ]
expect [ "$(count_event root_launch)" = 0 ]
expect [ "$light_pid" = 5555 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
''', {'unified': True, 'launcher': True}),
        ('unified_completed_request_reuses_modern_owner', r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event maintenance_started)" = 0 ]
expect [ "$(count_event continue_unified)" = 0 ]
expect [ "$(count_event root_launch)" = 0 ]
expect [ "$light_pid" = 5555 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
''', {'unified': True, 'unified_completed': True, 'launcher': True, 'ready_marker': '5555\n'}),
    ]
    for name, marker, comm, alive in (
            ('dead_pid', '5555\n', 'un260-boot', False),
            ('reused_pid_wrong_comm', '5555\n', 'other-process', True),
            ('pid_junk', '5555x\n', 'un260-boot', True),
            ('pid_negative', '-5555\n', 'un260-boot', True),
            ('pid_one', '1\n', 'un260-boot', True),
            ('pid_zero', '0\n', 'un260-boot', True),
            ('pid_empty', '\n', 'un260-boot', True),
            ('pid_overflow', '999999999999999999999999\n', 'un260-boot', True)):
        cases.append(('root_launch_stale_'+name, r'''
UN260_EARLY_DISPLAY=1
UN260_MDEV_READY=1
start
expect [ "$?" = 0 ]
wait "$PID"
expect [ "$(count_event root_launch)" = 1 ]
expect [ "$UN260_BOOT_LIGHT_ACTIVE" = 1 ]
expect grep -q stage=root_visual_found "$STARTUP_LOG"
''', dict(launcher=True, ready_marker=marker, owner_comm=comm, owner_alive=alive)))
    for name, commands, options in cases:
        run_case(source, name, commands, **options)
    print(f'{len(cases)} startup mock cases passed; no real device/service actions')
    run_modern_cases(source)
    run_health_writer()


if __name__ == '__main__':
    main()
