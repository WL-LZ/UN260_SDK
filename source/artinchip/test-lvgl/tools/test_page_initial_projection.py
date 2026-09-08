#!/usr/bin/env python3
"""Compile real page-manager entry paths against the real frame commit queue.

Hardware and page callbacks are a deliberately small lifecycle fixture. The
separate test_main_bottom_animation.py exercises Main's seven actual writers.
The negative control removes ONLY the new sync guards from extracted manager
functions, reproducing batch prewarm -> cancel -> unchanged-resume "Text".
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:static )?(?:const char \*|bool )" + name + r"\(", source, re.M)
    if not match:
        raise AssertionError("Missing production function: " + name)
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[match.start():end]


def main():
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host C compiler required")
    source = (ROOT / "un260/lv_core/lv_page_manager.c").read_text(encoding="utf-8")
    extracted = "\n\n".join(function(source, name) for name in (
        "ui_manager_page_is_registered", "create_new_page", "ui_manager_prewarm_page"))
    assert extracted.count("ui_frame_commit_begin_sync();") == 2
    assert extracted.count("ui_frame_commit_end_sync();") == 2
    mutant = re.sub(r"^[ \t]*ui_frame_commit_(?:begin|end)_sync\(\);\s*$", "", extracted, flags=re.M)
    with tempfile.TemporaryDirectory(prefix="un260-initial-projection-") as directory:
        work = Path(directory)
        for negative, body in ((False, extracted), (True, mutant)):
            (work / "page_manager_under_test.h").write_text(body, encoding="utf-8")
            executable = work / ("test.exe" if os.name == "nt" else "test")
            command = [compiler, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                       "-I" + str(ROOT), "-I" + str(work),
                       str(ROOT / "tools/test_page_initial_projection.c"),
                       str(ROOT / "un260/lv_core/ui_frame_commit.c"), "-o", str(executable)]
            if os.name != "nt":
                command.append("-fsanitize=undefined")
            subprocess.run(command, check=True)
            subprocess.run([str(executable)] + (["--sync-guard-mutation"] if negative else []),
                           check=True, timeout=15)


if __name__ == "__main__":
    main()
