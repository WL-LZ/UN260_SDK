#!/usr/bin/env python3
"""Exercise actual invisible-strip events and Main tap routing without LVGL I/O.

The event state machine, thresholds, attach/detach/setter and Main hit routing
are extracted verbatim. Rendering, timers and settlement completion are small
fixtures; test_pulldown_capture.py and test_innovation_transition.py retain the
full raw-capture and navigation/preview lifetime regression coverage.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    signature = re.search(r"^(?:static )?[\w *]+\b" + re.escape(name) +
                          r"\([^;]*?\)\s*\{", source, re.M)
    assert signature, "Missing production function: " + name
    opening = source.index("{", signature.start())
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[signature.start():token.end()]
    raise AssertionError("Unterminated function: " + name)


def main():
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host C compiler required")
    page = (ROOT / "un260/innovation/page_32_innovation.c").read_text(encoding="utf-8")
    main_page = (ROOT / "un260/lv_core/page_01_main.c").read_text(encoding="utf-8")
    state = re.search(r"typedef struct \{\s*bool pressed;.*?\} innovation_handle_gesture_t;",
                      page, re.S)
    assert state, "Production handle state missing"
    constants = "\n".join(re.findall(
        r"^#define INNOVATION_(?:PREVIEW_ARM_DY|TRANSITION_SETTLE_MS|TRANSITION_CANCEL_MS)\s+.*$",
        page, re.M))
    assert len(constants.splitlines()) == 3
    create = function(main_page, "ui_main_create")
    attach_position = create.index("page_32_innovation_handle_attach(main_page);")
    assert create.index("page_32_innovation_handle_set_tap_handler(page_01_top_strip_tap);") > attach_position
    assert "page_32_innovation_handle_detach();" in function(main_page, "ui_main_destroy")
    production = "\n\n".join(function(page, name) for name in (
        "innovation_handle_drag_update", "innovation_handle_drag_finish",
        "innovation_handle_drag_cancel", "innovation_handle_event_cb",
        "page_32_innovation_handle_attach", "page_32_innovation_handle_set_tap_handler",
        "page_32_innovation_handle_detach"))
    production += "\n\n" + function(main_page, "page_01_top_strip_tap")
    with tempfile.TemporaryDirectory(prefix="un260-main-top-gesture-") as directory:
        work = Path(directory)
        (work / "main_top_gesture_state.h").write_text(constants + "\n" + state.group(), encoding="utf-8")
        (work / "main_top_gesture_under_test.h").write_text(production, encoding="utf-8")
        executable = work / ("test.exe" if os.name == "nt" else "test")
        command = [compiler, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(work), str(ROOT / "tools/test_main_top_gesture.c"),
                   "-o", str(executable)]
        if os.name != "nt":
            command.extend(("-fsanitize=undefined", "-fno-sanitize-recover=undefined"))
        subprocess.run(command, check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=15)


if __name__ == "__main__":
    main()
