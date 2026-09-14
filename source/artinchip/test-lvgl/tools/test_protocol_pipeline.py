#!/usr/bin/env python3
"""Run real queue/RX/wakeup code and isolate the real frame-budget function."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not COMPILER:
    raise SystemExit("A C compiler with pthread support is required")


def extract_function(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


source = (ROOT / "un260/app_service/app_command_runtime.c").read_text(encoding="utf-8")
budget_function = extract_function(
    source, "uint32_t app_command_runtime_process_frames_budget(uint32_t budget_us)")
pending_function = extract_function(source, "bool app_command_runtime_frames_pending(void)")
max_frames = re.search(r"#define APP_COMMAND_MAX_FRAMES_PER_TICK\s+\d+", source).group()
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "un260/protocol/protocol_frame_queue.h"
static uint64_t now_us;
static uint32_t frame_cost_us, next_frame, end_frame, dispatched;
static uint32_t debug_logs;
static bool debug_active, block_dispatch;
static bool history_initialized, history_available, history_capacity;
static unsigned backpressure_warnings;
static char warning_text[100], warning_log[160];
static unsigned warning_logs;
static protocol_frame_t g_deferred_frame;
static bool g_deferred_frame_valid, g_deferred_frame_blocked;
static bool g_deferred_frame_warning_reported;
#define SMART_ISLAND_WARNING_LEVEL_ERROR 2
static void smart_island_notify_warning_level(const char *text, int level) {
    assert(level == SMART_ISLAND_WARNING_LEVEL_ERROR);
    snprintf(warning_text, sizeof(warning_text), "%s", text); backpressure_warnings++;
}
static bool ui_history_data_is_initialized(void) { return history_initialized; }
static bool ui_history_data_is_available(void) { return history_available; }
static void page_01_main_refresh_start_state(void) {}
static uint64_t app_clock_monotonic_us(void) { return now_us; }
static uint32_t app_clock_elapsed_us32(uint64_t start, uint64_t end) {
    return (uint32_t)(end - start);
}
bool protocol_frame_queue_pop(protocol_frame_t *frame) {
    if (next_frame == end_frame) return false;
    frame->len = 7;
    frame->data[3] = 0x49;
    frame->data[4] = (uint8_t)next_frame;
    frame->data[5] = (uint8_t)(next_frame >> 8);
    next_frame++;
    return true;
}
bool protocol_frame_queue_has_pending(void) { return next_frame != end_frame; }
static bool debug_page_rx_log_is_active(void) { return debug_active; }
static void debug_append_rx_log(const char *text) { (void)text; debug_logs++; }
size_t protocol_frame_format_hex(const uint8_t *data, size_t len,
                                 char *out, size_t cap) {
    (void)data; (void)len; assert(cap > 0); out[0] = 0; return 0;
}
static void uart_debug_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(warning_log, sizeof(warning_log), format, args);
    va_end(args);
    warning_logs++;
}
static bool app_command_runtime_dispatch(uint8_t cmd, uint8_t *data, uint8_t len) {
    assert(cmd == 0x49 && len == 7);
    assert(data[4] == (uint8_t)dispatched && data[5] == (uint8_t)(dispatched >> 8));
    if (block_dispatch || !history_initialized || !history_available || !history_capacity) return false;
    dispatched++;
    now_us += frame_cost_us;
    return true;
}
static void reset(uint32_t count, uint32_t cost) {
    now_us = 0; frame_cost_us = cost; next_frame = 0; end_frame = count;
    dispatched = 0; debug_logs = 0;
    g_deferred_frame_valid = false; g_deferred_frame_blocked = false;
    g_deferred_frame_warning_reported = false;
    history_initialized = history_available = history_capacity = true;
    block_dispatch = false; backpressure_warnings = 0;
    warning_logs = 0; warning_text[0] = warning_log[0] = '\0';
}
'''
code += max_frames + "\n" + pending_function + "\n" + budget_function + r'''
int main(void) {
    reset(0, 100); assert(app_command_runtime_process_frames_budget(2000) == 0);
    reset(5, 100); assert(app_command_runtime_process_frames_budget(0) == 1);
    reset(100, 800); assert(app_command_runtime_process_frames_budget(2000) == 3);
    assert(next_frame == 3 && dispatched == 3); /* no half-frame consumption */
    assert(app_command_runtime_process_frames_budget(2000) == 3);
    assert(next_frame == 6 && dispatched == 6); /* exact ordered continuation */
    reset(5, 3000); assert(app_command_runtime_process_frames_budget(2000) == 1);
    reset(100, 0); assert(app_command_runtime_process_frames_budget(2000) == 64);
    assert(app_command_runtime_process_frames_budget(2000) == 36);
    debug_active = true;
    reset(3, 10); assert(app_command_runtime_process_frames_budget(2000) == 3);
    assert(debug_logs == 3); /* budgeting does not disable diagnostic logging */
    reset(5, 10); block_dispatch = true;
    assert(app_command_runtime_process_frames_budget(2000) == 0);
    assert(next_frame == 1 && dispatched == 0 && debug_logs == 1);
    assert(g_deferred_frame_valid && backpressure_warnings == 1);
    assert(!app_command_runtime_frames_pending()); /* allow bounded wait, not a busy spin */
    assert(app_command_runtime_process_frames_budget(2000) == 0);
    assert(next_frame == 1 && dispatched == 0 && debug_logs == 1);
    assert(backpressure_warnings == 1); /* no duplicate side effects on retry */
    block_dispatch = false;
    assert(app_command_runtime_process_frames_budget(2000) == 5);
    assert(next_frame == 5 && dispatched == 5 && debug_logs == 5);
    assert(!g_deferred_frame_valid && !app_command_runtime_frames_pending());
    /* A start arriving during a healthy asynchronous read is held silently.
     * Loading completion must neither lose it nor invent a disk-full warning. */
    reset(5, 10); history_initialized = false;
    for (unsigned retry = 0; retry < 20; retry++) {
        assert(app_command_runtime_process_frames_budget(2000) == 0);
        assert(next_frame == 1 && dispatched == 0 && debug_logs == 1);
        assert(g_deferred_frame_valid && g_deferred_frame_blocked);
        assert(!g_deferred_frame_warning_reported && backpressure_warnings == 0);
        assert(warning_logs == 0 && warning_text[0] == '\0');
        assert(!app_command_runtime_frames_pending());
    }
    history_initialized = true;
    assert(app_command_runtime_process_frames_budget(2000) == 5);
    assert(next_frame == 5 && dispatched == 5 && debug_logs == 5);
    assert(backpressure_warnings == 0 && !g_deferred_frame_warning_reported);
    assert(warning_logs == 0);
    /* Both a completed failed load and a completed load with no queue capacity
     * still reject preflight. The deferred frame emits one real error then. */
    for (unsigned blocked_reason = 0; blocked_reason < 2; blocked_reason++) {
        reset(3, 10); history_initialized = false;
        assert(app_command_runtime_process_frames_budget(2000) == 0);
        assert(backpressure_warnings == 0 && !g_deferred_frame_warning_reported);
        assert(warning_logs == 0);
        history_initialized = true;
        if (blocked_reason == 0) history_available = false;
        else history_capacity = false;
        assert(app_command_runtime_process_frames_budget(2000) == 0);
        assert(backpressure_warnings == 1 && g_deferred_frame_warning_reported);
        assert(warning_logs == 1);
        assert(strcmp(warning_text, blocked_reason == 0 ? "History unavailable: receiving paused" :
                      "History full: receiving paused") == 0);
        assert(strcmp(warning_log, blocked_reason == 0 ?
                      "RX transition paused: history unavailable; retaining frame and session\n" :
                      "RX transition paused: history full; retaining frame and session\n") == 0);
        assert(next_frame == 1 && dispatched == 0 && debug_logs == 1);
        assert(!app_command_runtime_frames_pending());
        assert(app_command_runtime_process_frames_budget(2000) == 0);
        assert(backpressure_warnings == 1);
        assert(warning_logs == 1);
        history_available = history_capacity = true;
        assert(app_command_runtime_process_frames_budget(2000) == 3);
        assert(dispatched == 3 && debug_logs == 3 && !g_deferred_frame_warning_reported);
        /* A new blocked frame owns a new warning, after the previous frame was
         * consumed; resetting the flag must not suppress future real errors. */
        end_frame++;
        history_capacity = false;
        assert(app_command_runtime_process_frames_budget(2000) == 0);
        assert(backpressure_warnings == 2 && warning_logs == 2);
        assert(strcmp(warning_text, "History full: receiving paused") == 0);
    }
    puts("PASS: pending history is silent and retains frames; loaded success resumes, terminal failure/full reports once");
    puts("frame budget tests passed");
    return 0;
}
'''

# Compile the actual final-loop deadline calculation so display recovery cannot
# accidentally turn an overdue-but-uncommittable visual projection into a spin.
main_source = (ROOT / "main.c").read_text(encoding="utf-8")
wait_start = main_source.index("        uint32_t elapsed_ms = ")
wait_call = main_source.index("app_runtime_wakeup_wait_since(wake_sequence, wait_ms);", wait_start)
wait_end = wait_call + len("app_runtime_wakeup_wait_since(wake_sequence, wait_ms);")
wait_block = main_source[wait_start:wait_end]
lv_conf = (ROOT / "lv_conf.h").read_text(encoding="utf-8")
refresh_period = re.search(r"#define LV_DISP_DEF_REFR_PERIOD\s+\d+", lv_conf).group()
wait_code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "un260/app_service/app_runtime_wakeup.h"
static uint64_t clock_us;
static bool visual_pending, command_pending;
static unsigned waits;
static uint32_t last_wait;
static uint64_t app_clock_monotonic_us(void) { return clock_us; }
static uint32_t app_clock_uptime_ms(void) { return (uint32_t)(clock_us / 1000U); }
static bool ui_frame_commit_pending(void) { return visual_pending; }
static bool app_command_runtime_frames_pending(void) { return command_pending; }
void app_runtime_wakeup_wait_since(uint64_t sequence, uint32_t max_wait_ms) {
    assert(sequence == 712U);
    assert(max_wait_ms <= APP_RUNTIME_MAX_WAIT_MS);
    waits++;
    last_wait = max_wait_ms;
}
'''
wait_code += refresh_period + r'''
static uint32_t calculate(bool display_ready, bool pending_visual, bool pending_command,
                          uint32_t processed_frames, uint32_t lvgl_delay_ms,
                          uint32_t elapsed_ms_input, uint32_t now_ms,
                          uint32_t visual_commit_tick) {
    const uint64_t wake_sequence = 712;
    (void)display_ready; /* The old unfixed block must compile, then fail assertions. */
    clock_us = (uint64_t)now_ms * 1000U;
    uint64_t lvgl_end_us = clock_us - (uint64_t)elapsed_ms_input * 1000U;
    visual_pending = pending_visual;
    command_pending = pending_command;
    waits = 0;
    last_wait = UINT32_MAX;
'''
wait_code += wait_block + r'''
    assert(waits <= 1);
    return last_wait;
}
int main(void) {
    /* Held flush cannot commit visual work. Preserve an event-wakeable delay. */
    assert(calculate(false, true, false, 0, 10, 0, 100, 0) == 10);
    assert(calculate(false, true, false, 0, 10, 2, 100, 0) == 8);
    /* Recovery permits the overdue projection immediately. */
    assert(calculate(true, true, false, 0, 10, 0, 100, 0) == 0);
    assert(calculate(true, true, false, 0, 10, 0, 100, 97) == 7);
    assert(calculate(true, true, false, 0, 3, 0, 100, 97) == 3);
    /* No visual work: honor remaining LVGL deadline, elapsed work and ceiling. */
    assert(calculate(true, false, false, 0, 6, 2, 100, 0) == 4);
    assert(calculate(false, false, false, 0, 6, 2, 100, 0) == 4);
    assert(calculate(true, false, false, 0, 6, 7, 100, 0) == 0);
    assert(calculate(true, false, false, 0, UINT32_MAX, 0, 100, 0) == 10);
    /* Newly applied or queued protocol work must reach the next LVGL pass. */
    assert(calculate(true, false, false, 1, 10, 0, 100, 0) == UINT32_MAX);
    assert(calculate(true, false, true, 0, 10, 0, 100, 0) == UINT32_MAX);
    /* Unsigned visual age remains correct across uptime rollover. */
    assert(calculate(true, true, false, 0, 10, 0, 3, UINT32_MAX - 2U) == 4);
    puts("main-loop wait deadline tests passed");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="un260-protocol-pipeline-") as directory:
    work = Path(directory)
    suffix = ".exe" if os.name == "nt" else ""
    flags = [COMPILER, "-std=c11", "-D_GNU_SOURCE", "-Wall", "-Wextra", "-Werror",
             "-pthread", "-I" + str(ROOT)]
    if os.name != "nt":
        flags.append("-fsanitize=undefined")
    pipeline = work / ("pipeline" + suffix)
    sources = ["tools/test_protocol_pipeline.c", "un260/app_service/app_runtime_wakeup.c",
               "un260/lv_system/app_clock.c", "un260/protocol/protocol_frame.c",
               "un260/protocol/protocol_frame_parser.c", "un260/protocol/protocol_frame_queue.c",
               "un260/protocol/protocol_rx_service.c"]
    subprocess.run(flags + [str(ROOT / item) for item in sources] + ["-o", str(pipeline)], check=True)
    subprocess.run([str(pipeline)], check=True)
    budget_source = work / "budget.c"
    budget_source.write_text(code, encoding="utf-8")
    budget = work / ("budget" + suffix)
    subprocess.run(flags + [str(budget_source), "-o", str(budget)], check=True)
    subprocess.run([str(budget)], check=True)
    wait_source = work / "wait.c"
    wait_source.write_text(wait_code, encoding="utf-8")
    wait_test = work / ("wait" + suffix)
    subprocess.run(flags + [str(wait_source), "-o", str(wait_test)], check=True)
    subprocess.run([str(wait_test)], check=True)
