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
assert 'if (!startup_started && (first_frame_presented || native_waiting))' in main
assert 'if (!native_waiting) ui_page_00_boot_anim_create(lv_layer_top());' in main
assert 'bool display_ready = !native_waiting && lv_port_disp_poll();' in main
assert 'startup_ready && native_surface_prepared, startup_error_reported' in main
assert 'first_frame_presented && !native_waiting && current_page == UI_PAGE_BOOT' in main
assert main.index('app_startup_runtime_poll(') < main.index('app_command_runtime_process_frames_budget(')
assert 'if (!app_startup_runtime_can_process()) {' in main
assert 'uint32_t delay_ms = lvgl_delay_ms > 5U ? 5U : lvgl_delay_ms;' in main
assert 'ui_history_data_init_async();' in runtime
assert 'ui_history_data_init_poll();' in runtime
assert 'user_cfg_startup_apply(&g_snapshot)' in runtime
assert 'startup_frame_trace_begin(!ui_page_00_boot_anim_is_active());' in main
assert 'lv_port_disp_set_present_observer(startup_frame_present);' in main
assert 'if (!startup_frame_trace.selfcheck && !ui_page_00_boot_anim_is_active())' in main
trace_poll=main[main.index('static void startup_frame_trace_poll('):main.index('//--------------------')]
assert trace_poll.index('lv_port_disp_set_present_observer(NULL);') < trace_poll.index('app_startup_frames_finish();')
assert 'startup_visual_note_ui_presented' not in (root / 'lv_port_disp.c').read_text()
assert 'if (startup_ready && !native_waiting &&\n            !startup_error_reported &&\n            fbdev_present_sequence() != startup_health_sequence)' in main
assert main.index('startup_health_sequence = fbdev_present_sequence();', main.index('if (status == APP_STARTUP_READY)')) < main.index('startup_ready = true;')
assert 'if (startup_input_ready && !native_surface_prepared && !startup_error_reported)' in main
assert 'ui_page_08_curr_prepare_failed()' in main
assert 'app_startup_trace_mark("selftest_prepare_failed")' in main
assert main.index('ui_page_08_curr_set_covered(false);') < main.index('startup_frame_trace_begin(true);')
assert main.index('startup_frame_trace_begin(true);') < main.index('app_startup_trace_mark("root_visual_handoff");')
assert 'now_us - startup_frame_trace.first_us >= 5000000U' in main
main_helpers = main[main.index('/* Presentation tracing is attached'):main.index('//--------------------')]
fallback_block_start = main.index('    if (!native_waiting && !ui_page_00_boot_anim_is_active()) {')
fallback_block_end = main.index('    if (native_waiting) ui_page_08_curr_set_covered(true);', fallback_block_start)
fallback_block = main[fallback_block_start:fallback_block_end]
assert 'return 1;' in fallback_block and 'ui_page_08_curr_prepare_failed()' in fallback_block
assert 'if (!native_waiting && ui_page_08_curr_prepare_failed())' in main
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "un260/app_service/app_startup_trace.h"
static uint64_t now = 4000;
uint64_t app_clock_monotonic_ms(void) { return now; }
typedef enum {UI_PAGE_BOOT,UI_PAGE_MAIN} ui_page_t;
static bool mock_intro=true,mock_failed;
static ui_page_t mock_page=UI_PAGE_BOOT;
static unsigned prepare_calls;
static void (*observer)(uint64_t);
bool ui_page_00_boot_anim_is_active(void){return mock_intro;}
bool ui_page_08_curr_prepare_failed(void){return mock_failed;}
bool ui_page_08_curr_prepare_step(void){prepare_calls++;return !mock_failed;}
ui_page_t ui_manager_get_current_page(void){return mock_page;}
void lv_port_disp_set_present_observer(void (*cb)(uint64_t)){observer=cb;}
MAIN_HELPERS
static int failed_no_intro(void){
 bool native_waiting=false;
MAIN_FALLBACK_BLOCK
 return 0;
}
int main(int argc, char **argv) {
    if(argc>1&&!strcmp(argv[1],"no-intro-failure")){
        mock_intro=false;mock_failed=true;
        assert(failed_no_intro()==1&&prepare_calls==1);
        return 0;
    }
    if(argc>1&&!strncmp(argv[1],"observer-",9)){
        bool native=!strcmp(argv[1],"observer-native");
        bool direct=!strcmp(argv[1],"observer-no-intro");
        bool failed=!strcmp(argv[1],"observer-failed");
        bool other=!strcmp(argv[1],"observer-other");
        const char *enabled=getenv("UN260_BOOT_FRAME_TRACE");
        startup_frame_trace_begin(native||direct);
        if(!enabled||strcmp(enabled,"1")){
            assert(!observer&&!startup_frame_trace.active);
            startup_frame_present(20000000);startup_frame_trace_poll(30000000);
            return 0;
        }
        assert(observer);
        if(!native&&!direct){
            observer(4000000);observer(4013000);observer(4027000);
            startup_frame_trace_poll(50000000);assert(observer);
            mock_intro=false;mock_failed=failed;if(other)mock_page=UI_PAGE_MAIN;
        }else{
            startup_frame_trace_poll(9000000);assert(observer&&!startup_frame_trace.first_us);
        }
        observer(20000000);
        if(failed||other){assert(!observer&&!startup_frame_trace.active);return 0;}
        assert(startup_frame_trace.selfcheck&&startup_frame_trace.first_us==20000000);
        startup_frame_trace_poll(19999999);assert(observer);
        observer(20013000);observer(20027000);
        startup_frame_trace_poll(24999999);assert(observer);
        startup_frame_trace_poll(25000000);assert(!observer&&!startup_frame_trace.active);
        startup_frame_present(30000000);startup_frame_trace_poll(30000000);
        return 0;
    }

    if (argc > 1) {
        const char *value = getenv("UN260_BOOT_FRAME_TRACE");
        app_startup_frames_present(1); /* No implicit activation. */
        app_startup_frames_finish();
        bool selfcheck = strcmp(argv[1], "selfcheck") == 0;
        if (!strcmp(argv[1], "stress")) {
            assert(app_startup_frames_begin_selfcheck());
            uint64_t t = 1000000;
            app_startup_frames_present(t);
            for (unsigned i = 1; i <= 600; ++i) {
                t += i % 30 ? 10000U : 60000U;
                app_startup_frames_present(t);
            }
            app_startup_frames_finish();
            return 0;
        }
        if (!strcmp(argv[1], "empty")) {
            assert(app_startup_frames_begin_selfcheck());
            app_startup_frames_finish();
            return 0;
        }
        assert((selfcheck ? app_startup_frames_begin_selfcheck() :
            app_startup_frames_begin()) == (value && strcmp(value, "1") == 0));
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
fixture = fixture.replace('MAIN_HELPERS', main_helpers).replace('MAIN_FALLBACK_BLOCK', fallback_block)
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
    for frame_setting in (None, '0', 'false', '1'):
        env = os.environ.copy()
        env.pop('UN260_BOOT_TRACE', None)
        env.pop('UN260_BOOT_FRAME_TRACE', None)
        if frame_setting is not None:
            env['UN260_BOOT_FRAME_TRACE'] = frame_setting
        result = subprocess.run([str(exe), 'selfcheck'], env=env, capture_output=True,
                                text=True, check=True)
        if frame_setting == '1':
            lines = result.stderr.splitlines()
            assert len(lines) == 2
            assert lines[0].startswith('BOOT_FRAMES renderer=lvgl-selfcheck reason=detached frames=3 ')
            assert 'first_uptime_us=4000000 last_uptime_us=4027000' in lines[0]
            assert lines[1] == ('BOOT_FRAME_PERCENTILES renderer=lvgl-selfcheck samples=2 omitted=0 '
                'p50_us=13000 p95_us=14000 worst_us=14000 long_intervals_gt50000=0')
        else:
            assert result.stderr == ''
    env = os.environ.copy()
    env['UN260_BOOT_FRAME_TRACE'] = '1'
    for mode, expected in (
        ('empty', 'samples=0 omitted=0 p50_us=0 p95_us=0 worst_us=0 long_intervals_gt50000=0'),
        ('stress', 'samples=512 omitted=88 p50_us=10000 p95_us=10000 worst_us=60000 long_intervals_gt50000=20'),
    ):
        result = subprocess.run([str(exe), mode], env=env, capture_output=True, text=True, check=True)
        assert len(result.stderr.splitlines()) == 2
        assert result.stderr.splitlines()[1].endswith(expected)
    for setting in (None,'0','false','1'):
        for mode in ('observer-native','observer-no-intro','observer-fallback','observer-failed','observer-other'):
            env=os.environ.copy();env.pop('UN260_BOOT_TRACE',None);env.pop('UN260_BOOT_FRAME_TRACE',None)
            if setting is not None:env['UN260_BOOT_FRAME_TRACE']=setting
            result=subprocess.run([str(exe),mode],env=env,capture_output=True,text=True,check=True)
            lines=result.stderr.splitlines()
            if setting!='1':assert not lines
            elif mode in ('observer-failed','observer-other'):
                assert len(lines)==1 and 'renderer=lvgl-intro' in lines[0]
            else:
                if mode=='observer-fallback':
                    assert len(lines)==3 and 'renderer=lvgl-intro' in lines[0]
                    lines=lines[1:]
                else:assert len(lines)==2
                assert 'renderer=lvgl-selfcheck' in lines[0] and 'frames=3' in lines[0]
                assert 'first_uptime_us=20000000 last_uptime_us=20027000' in lines[0]
                assert lines[1].endswith('samples=2 omitted=0 p50_us=13000 p95_us=14000 worst_us=14000 long_intervals_gt50000=0')
    env=os.environ.copy();env.pop('UN260_BOOT_TRACE',None)
    result=subprocess.run([str(exe),'no-intro-failure'],env=env,capture_output=True,text=True,check=True)
    assert result.stderr=='Self-test display preparation failed; restart required\n'
print('PASS actual main callback: native/no-intro/legacy first selftest present included; 5s from first-present; failure/navigation detach; default-off')
print('PASS actual main no-intro preparation loop fails once and exits, no initialization spin')
print('PASS startup selfcheck trace default-off; first present included; exact percentiles/long intervals; 512 bounded samples and empty finish')
print('PASS startup trace default-off, explicit opt-in, monotonic milestones')
print('PASS startup frame trace independent default-off/opt-in, one finish summary, no post-finish samples')
print('PASS startup integration: asynchronous history before protocol processing; self-test readiness gate; confirmed present sequence')
