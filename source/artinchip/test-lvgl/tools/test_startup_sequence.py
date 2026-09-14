#!/usr/bin/env python3
"""Host checks for opt-in startup trace and the integration ordering contract."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'main.c').read_text()
assert 'ui_history_data_init();' not in main
runtime = (root / 'un260/app_service/app_startup_runtime.c').read_text()
assert main.index('ui_page_00_boot_anim_create(') < main.index('ui_history_data_init_async();')
assert main.index('ui_history_data_init_async();') < main.index('app_command_runtime_process_frames_budget(')
assert main.index('ui_history_data_init_poll();') < main.index('app_command_runtime_process_frames_budget(')
assert '!ui_page_00_boot_anim_is_active() &&\n                 ui_history_data_is_initialized()' in main
assert 'first_frame_presented && fbdev_present_sequence() != 0U' in main
assert 'if (!startup_started && first_frame_presented)' in main
assert main.index('app_startup_runtime_poll(') < main.index('app_command_runtime_process_frames_budget(')
assert 'if (!app_startup_runtime_can_process()) {' in main
assert 'uint32_t delay_ms = lvgl_delay_ms > 5U ? 5U : lvgl_delay_ms;' in main
assert 'ui_history_data_init_async();' in runtime
assert 'ui_history_data_init_poll();' in runtime
assert 'user_cfg_startup_apply(&g_snapshot)' in runtime
assert 'ui_page_00_boot_anim_is_active() && app_startup_frames_begin()' in main
assert 'lv_port_disp_set_present_observer(app_startup_frames_present);' in main
assert 'if (boot_frames_active && !ui_page_00_boot_anim_is_active())' in main
assert main.index('lv_port_disp_set_present_observer(NULL);') < main.index('app_startup_frames_finish();')
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "un260/app_service/app_startup_trace.h"
static uint64_t now = 4000;
uint64_t app_clock_monotonic_ms(void) { return now; }
int main(int argc, char **argv) {
    (void)argv;
    if (argc > 1) {
        const char *value = getenv("UN260_BOOT_FRAME_TRACE");
        app_startup_frames_present(1); /* No implicit activation. */
        app_startup_frames_finish();
        assert(app_startup_frames_begin() == (value && strcmp(value, "1") == 0));
        app_startup_frames_present(4000000);
        app_startup_frames_present(4013000);
        app_startup_frames_present(4027000);
        app_startup_frames_finish();
        app_startup_frames_present(9999999); /* Finished observers retain no new sample. */
        app_startup_frames_finish();
        return 0;
    }
    app_startup_trace_mark("main_enter");
    now += 23;
    app_startup_trace_mark("display_initialized");
    now += 7;
    app_startup_trace_mark("first_frame_presented");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='un260-startup-trace-') as directory:
    tmp = Path(directory)
    source = tmp / 'fixture.c'
    source.write_text(fixture)
    exe = tmp / 'trace'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-I'+str(root), str(source),
                    str(root/'un260/app_service/app_startup_trace.c'), '-o', str(exe)], check=True)
    for setting in (None, '0', 'false', '1'):
        env = os.environ.copy()
        env.pop('UN260_BOOT_TRACE', None)
        env.pop('UN260_BOOT_FRAME_TRACE', None)
        if setting is not None:
            env['UN260_BOOT_TRACE'] = setting
        result = subprocess.run([str(exe)], env=env, capture_output=True, text=True, check=True)
        assert result.stdout == ''
        if setting == '1':
            assert result.stderr.splitlines() == [
                'BOOT_TRACE app uptime_ms=4000 elapsed_ms=0 stage=main_enter',
                'BOOT_TRACE app uptime_ms=4023 elapsed_ms=23 stage=display_initialized',
                'BOOT_TRACE app uptime_ms=4030 elapsed_ms=30 stage=first_frame_presented']
        else:
            assert result.stderr == ''
    for frame_setting in (None, '0', 'false', '1'):
        for milestone_setting in (None, '1'):
            env = os.environ.copy()
            env.pop('UN260_BOOT_TRACE', None)
            env.pop('UN260_BOOT_FRAME_TRACE', None)
            if frame_setting is not None:
                env['UN260_BOOT_FRAME_TRACE'] = frame_setting
            if milestone_setting is not None:
                env['UN260_BOOT_TRACE'] = milestone_setting
            result = subprocess.run([str(exe), 'frames'], env=env, capture_output=True,
                                    text=True, check=True)
            assert result.stdout == ''
            if frame_setting == '1':
                assert result.stderr.splitlines() == [
                    'BOOT_FRAMES renderer=lvgl-intro reason=detached frames=3 '
                    'first_uptime_us=4000000 last_uptime_us=4027000 '
                    'raw_intervals=2 raw_interval_total_us=27000 raw_interval_max_us=14000 '
                    'work_total_us=0 work_max_us=0 '
                    'raw_bins_us=le16000:2,le20000:0,le33000:0,le50000:0,le100000:0,gt100000:0 '
                    'clock_resets=0 includes_static_holds=1']
            else:
                assert result.stderr == ''
print('PASS startup trace default-off, explicit opt-in, monotonic milestones')
print('PASS startup frame trace independent default-off/opt-in, one finish summary, no post-finish samples')
print('PASS startup integration: asynchronous history before protocol processing; self-test readiness gate; confirmed present sequence')
