#!/usr/bin/env python3
"""Exercise actual counting reset/info functions and the real LVGL island.

History backpressure and a single animation allocation failure are injected.
No transport, history-store or animation lifecycle implementation is copied.
"""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

from test_main_view import ROOT, function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--sanitize", default="undefined", choices=("none", "undefined", "address,undefined"))
    args = parser.parse_args()
    runtime = (ROOT / "un260/app_service/app_counting_runtime.c").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="un260-island-end-") as temp:
        projection = Path(temp) / "actual_counting_reset.c"
        projection.write_text('''#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include "lvgl/lvgl.h"
#include "un260/counting/counting_session_state.h"
#include "un260/counting/counting_data_store.h"
#include "un260/counting/counting_history_service.h"
#include "un260/lv_system/counting_ui_runtime.h"
#include "un260/lv_components/smart_island.h"
extern bool reset_allowed;
bool counting_history_prepare_reset(counting_session_state_t *session,
    const counting_sim_t *data, uint32_t tick) {
    (void)session; (void)data; (void)tick; return reset_allowed;
}
void uart_debug_printf(const char *format, ...) { (void)format; }
''' + function(runtime, "app_counting_runtime_reset_session") + "\n", encoding="utf-8")
        subprocess.run([sys.executable, str(ROOT / "tools/test_main_view.py"),
                        "--lvgl-dir", str(args.lvgl_dir.resolve()),
                        "--sanitize", args.sanitize,
                        "--harness", str(ROOT / "tools/test_smart_island_count_end.c"),
                        "--extra-source", str(projection),
                        "--extra-source", str(ROOT / "un260/counting/counting_info_reply.c"),
                        "--wrap", "lv_anim_start"], check=True)


if __name__ == "__main__":
    main()
