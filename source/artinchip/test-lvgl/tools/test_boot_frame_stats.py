#!/usr/bin/env python3
"""Bounded presentation statistics: no platform clocks or display required."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
fixture = r'''
#include "un260/lv_drivers/boot_frame_stats.h"
#include <assert.h>
#include <stdbool.h>
#include <string.h>
int main(void) {
    boot_frame_stats_t stats = {0};
    bool enabled = false;
    if (enabled) boot_frame_stats_record(&stats, 100, 20);
    assert(stats.frames == 0 && stats.work_total_us == 0);
    boot_frame_stats_record(&stats, 0, 7);
    assert(stats.frames == 1 && stats.first_us == 0 && stats.intervals == 0);
    const uint64_t gaps[] = {0,16000,16001,20000,20001,33000,33001,50000,50001,100000,100001};
    uint64_t now = 0;
    for (unsigned i = 0; i < sizeof(gaps)/sizeof(gaps[0]); ++i) {
        now += gaps[i];
        boot_frame_stats_record(&stats, now, i + 1U);
    }
    assert(stats.frames == 12 && stats.intervals == 11);
    assert(stats.interval_total_us == now && stats.interval_max_us == 100001);
    assert(stats.work_total_us == 73 && stats.work_max_us == 11);
    for (unsigned i = 0; i < 5; ++i) assert(stats.interval_bins[i] == 2);
    assert(stats.interval_bins[5] == 1);
    boot_frame_stats_record(&stats, now - 1U, 0);
    assert(stats.clock_resets == 1 && stats.intervals == 11);
    boot_frame_stats_record(&stats, now + 15999U, UINT32_MAX);
    assert(stats.intervals == 12 && stats.interval_bins[0] == 3);
    assert(stats.work_max_us == UINT32_MAX);
    boot_frame_stats_print(stdout, &stats, "host", "complete");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="un260-frame-stats-") as directory:
    tmp = Path(directory)
    source = tmp / "stats.c"
    binary = tmp / "stats"
    source.write_text(fixture)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-fno-sanitize-recover=all",
                    "-I" + str(root), str(source), "-o", str(binary)], check=True)
    result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
    assert result.stdout.count("BOOT_FRAMES ") == 1
    assert "includes_static_holds=1" in result.stdout
    assert "clock_resets=1" in result.stdout
    assert "drop" not in result.stdout.lower() and "fps" not in result.stdout.lower()
print("PASS frame statistics: disabled call site, zero timestamp, interval bins, backward clock, work range and single raw summary")
