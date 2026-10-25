#!/usr/bin/env python3
"""Verify late authoritative 0B through production runtime and real LVGL island.

Only transport/history side effects are stubbed. Reuses the existing Main
render harness and LVGL sources; temporary files remain outside the repository.
"""
import argparse
from pathlib import Path
import os
import subprocess
import sys
import tempfile
from test_main_view import ROOT, function

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    runtime = (ROOT / "un260/app_service/app_counting_runtime.c").read_text()
    temp_root = Path(os.environ.get("TMPDIR", "/dev/shm/un260-a-accuracy/temp"))
    temp_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="denom-result-view-", dir=temp_root) as temp:
        projection = Path(temp) / "actual_denom_runtime.c"
        prefix = """#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_denom_reply.h"
#include "un260/counting/counting_info_reply.h"
#include "un260/counting/counting_report_sync.h"
#include "un260/counting/counting_multi.h"
#include "un260/lv_core/page_01_main.h"
#include "un260/lv_core/page_01_main_detail.h"
#include "un260/lv_core/page_02_list.h"
#include "un260/lv_core/ui_frame_commit.h"
#include "un260/lv_components/smart_island.h"
#include "un260/lv_system/app_clock.h"
#include "un260/currency/currency_state.h"
#include "un260/machine_state/machine_state.h"
void uart_debug_printf(const char *format, ...) {(void)format;}
void counting_history_restart_group(void) {}
void counting_history_append_frame(const char *tag,const uint8_t *buf,uint8_t len)
{(void)tag;(void)buf;(void)len;}
void page_02_list_section_mark_dirty(page_02_section_id_t id) {(void)id;}
"""
        parts = [function(runtime, name) for name in (
            "app_counting_runtime_format_amount", "app_counting_visual_commit",
            "app_counting_runtime_refresh_compact", "app_counting_runtime_on_main_data_changed",
            "app_counting_runtime_on_live_denom_changed")]
        hook = """
static const counting_denom_reply_hooks_t g_counting_denom_hooks = {
 .on_history_frame=counting_history_append_frame,
 .on_main_data_changed=app_counting_runtime_on_main_data_changed,
};
"""
        projection.write_text(prefix + "\n".join(parts) + hook +
            function(runtime, "app_counting_runtime_handle_denom") + "\n")
        command = [sys.executable, str(ROOT / "tools/test_main_view.py"),
            "--lvgl-dir", str(args.lvgl_dir.resolve()), "--sanitize", "undefined",
            "--harness", str(ROOT / "tools/test_denom_result_view.c"),
            "--extra-source", str(projection), "--wrap", "lv_anim_start",
            "--wrap", "smart_island_notify_count_end"]
        for name in ("counting_denom_reply.c", "counting_denom_query_service.c",
                     "counting_info_reply.c", "counting_report_sync.c"):
            command += ["--extra-source", str(ROOT / "un260/counting" / name)]
        if args.output_dir:
            command += ["--output-dir", str(args.output_dir.resolve())]
        subprocess.run(command, check=True)

if __name__ == "__main__":
    main()
