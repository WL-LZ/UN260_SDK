#!/usr/bin/env python3
"""Compile real page-manager entry paths against the real frame commit queue.

Hardware and page callbacks are a deliberately small lifecycle fixture. The
separate test_main_bottom_animation.py exercises Main's seven actual writers.
The negative control removes ONLY the sync guards from extracted manager
functions, reproducing batch prewarm -> cancel -> unchanged-resume "Text".
Main's actual create/resume call order is checked separately, so the manager
fixture cannot silently outlive the production seven-label projection path.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:const\s+char\s*\*|bool|void)\s*" +
                      re.escape(name) + r"\(", source, re.M)
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


def assert_order(body, statements, context):
    """Match executable statements, not a comment describing a removed call."""
    body = re.sub(r"/\*.*?\*/|//[^\n]*", "", body, flags=re.S)
    position = 0
    for statement in statements:
        found = re.search(statement, body[position:])
        assert found, f"{context}: missing/out-of-order {statement}"
        position += found.end()


def verify_main_projection(source):
    writers = (
        ("MODE", "page_01_mode_switch_refre", "page_01_bottom_a_refresh_mode"),
        ("ADD", "page_01_add_refre", "page_01_bottom_a_refresh_add"),
        ("WORK", "page_01_work_refre", "page_01_bottom_a_refresh_work"),
        ("BATCH", "page_01_batch_refre", "page_01_bottom_c_refresh_batch"),
        ("FO", "page_01_face_refre", "page_01_bottom_a_refresh_fo"),
        ("CFD", "page_01_cfd_refre", "page_01_bottom_c_refresh_cfd"),
        ("SPEED", "page_01_speed_refre", "page_01_bottom_c_refresh_speed"),
    )
    calls = [r"page_01_bottom_a_create\(\);", r"page_01_bottom_c_create\(\);"]
    calls.extend(re.escape(refresh) + r"\(\);" for _, refresh, _ in writers)
    calls.extend((r"ui_refresh_main_page\(\);", r"s_main_dirty\s*=\s*0;",
                  r"page_01_main_snapshot_capture\(\);"))
    assert_order(function(source, "ui_main_create"), calls,
                 "Main create must project all seven labels before the completed snapshot")
    resume = function(source, "page_01_main_resume")
    for flag, refresh, bottom in writers:
        assert_order(function(source, refresh), (
            r"page_01_main_defer_refresh\(PAGE_01_MAIN_DIRTY_" + flag + r"\)",
            re.escape(bottom) + (r"\(\);" if flag == "CFD" else r"\(false\);"),
        ), f"Main {flag} projection wrapper")
        assert_order(resume, (
            r"lv_obj_clear_flag\(main_page,\s*LV_OBJ_FLAG_HIDDEN\);",
            r"if\s*\(dirty\s*&\s*PAGE_01_MAIN_DIRTY_" + flag + r"\)\s*" +
            re.escape(refresh) + r"\(\);",
            r"page_01_main_snapshot_capture\(\);",
        ), f"Main {flag} hidden-to-visible projection")
    assert_order(function(source, "page_01_main_suspend"), (
        r"page_01_bottom_animations_stop\(\);",
        r"lv_obj_add_flag\(main_page,\s*LV_OBJ_FLAG_HIDDEN\);",
    ), "Main suspend cancels interaction effects before hiding")
    assert_order(function(source, "ui_main_destroy"), (
        r"page_01_bottom_animations_stop\(\);",
        r"page_01_bottom_a_destroy\(\);", r"page_01_bottom_c_destroy\(\);",
    ), "Main destroy cancels animations before deleting their labels")
    print("PASS: actual Main creates all seven new bottom labels before snapshot, and resumes dirty values after reveal", flush=True)


def main():
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host C compiler required")
    verify_main_projection((ROOT / "un260/lv_core/page_01_main.c").read_text(encoding="utf-8"))
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
            subprocess.run(command, check=True, timeout=60)
            subprocess.run([str(executable)] + (["--sync-guard-mutation"] if negative else []),
                           check=True, timeout=15)


if __name__ == "__main__":
    main()
