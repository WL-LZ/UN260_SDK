#!/usr/bin/env python3
"""Execute retained Currency entry against real selection/model/scroll physics.

LVGL object/timer and persistence edges are simulated; production resume,
suspend, focus restoration and model functions are compiled unchanged.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

from test_currency_modes import function, without_includes

ROOT = Path(__file__).resolve().parents[1]


def main():
    page = (ROOT / "un260/lv_core/page_07_curr.c").read_text(encoding="utf-8")
    model = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_model.c").read_text(encoding="utf-8")
    internal = (ROOT / "un260/lv_core/page_07_curr/page_07_curr_internal.h").read_text(encoding="utf-8")
    enum = re.search(r"typedef enum \{[^{}]*\} page07_curr_view_mode_t;", internal).group()
    state = re.search(r"typedef struct \{[^{}]*\} page07_curr_model_state_t;", internal).group()
    create = function(page, "ui_page_07_curr_create")
    assert create.index("page_07_curr_img_refre();") < create.index("curr_focus_confirmed_selection_on_entry();")
    parts = [without_includes(model)]
    for name in ("curr_cached_list_matches", "curr_cached_selection_matches",
                 "curr_refresh_cached_selection", "curr_focus_confirmed_selection_on_entry",
                 "ui_page_07_curr_resume", "ui_page_07_curr_suspend"):
        parts.append(function(page, name))
    with tempfile.TemporaryDirectory(prefix="un260-currency-resume-") as temp:
        work = Path(temp)
        (work / "currency_resume_types.inc").write_text(enum + "\n" + state, encoding="utf-8")
        (work / "currency_resume_under_test.inc").write_text("\n".join(parts), encoding="utf-8")
        binary = work / ("currency-resume.exe" if os.name == "nt" else "currency-resume")
        sources = ["tools/test_currency_resume.c", "un260/currency/currency_state.c",
                   "un260/lv_components/ui_scroll_physics.c"]
        for optimization in ("-O0", "-O2"):
            command = [os.environ.get("CC", "cc"), "-std=c11", optimization,
                       "-Wall", "-Wextra", "-Werror", f"-I{work}", f"-I{ROOT}"]
            if os.name != "nt":
                command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
            subprocess.run(command + [str(ROOT / path) for path in sources] +
                           ["-o", str(binary), "-lm"], check=True)
            subprocess.run([str(binary)], check=True, timeout=30)


if __name__ == "__main__":
    main()
