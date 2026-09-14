#!/usr/bin/env python3
"""Run the real startup shell functions against an isolated fake device tree.

No real service, module, calibration, updater, sync, governor or device is used.
The top-level service dispatcher is deliberately not sourced.
"""
import argparse
import pathlib
import subprocess
import tempfile


def run_case(source, name, commands, *, trace=True, touch=True, calibration=True,
             recovery=False, usleep=True, fractional=True, trace_flag=False,
             early=False, early_disabled=False):
    with tempfile.TemporaryDirectory(prefix="un260-startup-test-") as tmp:
        root = pathlib.Path(tmp)
        for sub in ("dev/input", "sys/class/input/event7/device", "etc", "run", "bin", "tmp"):
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
        (root / "uptime").write_text("3.70 0.00\n")
        executables = {
            "daemon": "printf 'DAEMON_OUTPUT\\n'\n",
            "updater": 'echo recover >> "$CASE_ROOT/events"\n[ "${RECOVER_FAIL:-0}" != 1 ]\n',
            "calibrate": ('echo calibrate >> "$CASE_ROOT/events"\n'
                          '[ "${CALIBRATE_FAIL:-0}" != 1 ] || exit 1\n'
                          '[ "${CALIBRATE_NO_FILE:-0}" = 1 ] || : > "$CASE_ROOT/etc/pointercal"\n'),
        }
        for filename, body in executables.items():
            path = root / "bin" / filename
            path.write_text("#!/bin/sh\n" + body)
            path.chmod(0o755)

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
        }
        for node in ('fb0', 'ge', 'ttyS4', 'ttyS5', 'ttyS6'):
            replacements['/dev/' + node] = str(root / 'dev' / node)
        harness = source[:source.rindex('\ncase "$1" in')]
        for old, new in replacements.items():
            harness = harness.replace(old, new)
        # Fake event nodes are ordinary files; production still requires -c.
        harness = harness.replace('[ -c "$candidate" ]', '[ -e "$candidate" ]')
        harness += r'''
record() { printf '%s\n' "$*" >> "$CASE_ROOT/events"; }
pause_mock() {
    record "pause $1"
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
    if [ "$1" != -0 ]; then record "kill $*"; return 0; fi
    case "$2" in
        4242) [ -f "$CASE_ROOT/mdev-ready" ] ;;
        4343) [ -f "$CASE_ROOT/app-ready" ] ;;
        4444) [ -f "$CASE_ROOT/defer-ready" ] ;;
        *) [ "${HEALTH_FAIL:-0}" != 1 ] ;;
    esac
}
expect() { "$@" || { echo "assertion failed: $*" >&2; exit 97; }; }
count_event() { grep -c "^$1$" "$CASE_ROOT/events" 2>/dev/null || true; }
'''
        harness += commands + '\n'
        path = root / 'harness.sh'
        path.write_text(harness)
        env = {
            'PATH': '/usr/bin:/bin', 'CASE_ROOT': str(root),
            'USLEEP_OK': str(int(usleep)), 'FRACTIONAL_OK': str(int(fractional)),
        }
        if trace is not None:
            env['UN260_BOOT_TRACE'] = str(int(trace))
        if early is not None:
            env['UN260_EARLY_DISPLAY'] = str(int(early))
        result = subprocess.run(['/bin/sh', str(path)], env=env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
        if result.returncode:
            events = (root / 'events').read_text() if (root / 'events').exists() else ''
            raise AssertionError(f'{name}: exit={result.returncode}\n{result.stdout}\n{events}')
        print(f'PASS {name}')


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
    ]
    for name, commands, options in cases:
        run_case(source, name, commands, **options)
    print(f'{len(cases)} startup mock cases passed; no real device/service actions')


if __name__ == '__main__':
    main()
