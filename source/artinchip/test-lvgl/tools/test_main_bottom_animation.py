#!/usr/bin/env python3
"""Exercise real Main projection/animation functions with the real commit queue."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Host C compiler required")


def function(source, name):
    match = re.search(r"^(?:static )?(?:void|bool) " + name + r"\(", source, re.M)
    if not match:
        raise AssertionError("Missing production function: " + name)
    brace = source.index("{", match.start())
    depth, end = 1, brace + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[match.start():end]


source = (ROOT / "un260/lv_core/page_01_main.c").read_text(encoding="utf-8")
header = (ROOT / "un260/lv_core/page_01_main.h").read_text(encoding="utf-8")
dirty_enum = re.search(r"typedef enum \{\s*PAGE_01_MAIN_DIRTY_MODE.*?\} page_01_main_dirty_flag_t;",
                       header, re.S).group()
anim_enum = re.search(r"typedef enum \{\s*PAGE_01_BOTTOM_TEXT_ANIM_NONE.*?\} page_01_bottom_text_anim_t;",
                      source, re.S).group()
intent_start = source.index("#define PAGE_01_MAIN_BOTTOM_ANIM_SHIFT")
intent_end = source.index("static uint32_t s_main_commit_animation_flags;", intent_start)
intent = source[intent_start:intent_end] + "static uint32_t s_main_commit_animation_flags;\n"
functions = ["page_01_main_commit", "page_01_main_mark_dirty", "page_01_main_defer_refresh",
             "page_01_main_defer_bottom_refresh", "page_01_mode_switch_refre", "page_01_add_refre",
             "page_01_work_refre", "page_01_face_refre", "page_01_speed_refre",
             "page_01_batch_refre", "page_01_cfd_refre",
             "page_01_bottom_text_anim_opa_cb", "page_01_bottom_text_anim_x_cb",
             "page_01_bottom_text_anim_zoom_cb", "page_01_bottom_label_anim_stop",
             "page_01_bottom_animations_stop", "page_01_bottom_label_anim_run",
             "page_01_bottom_a_refresh_mode", "page_01_bottom_a_refresh_add",
             "page_01_bottom_a_refresh_work", "page_01_bottom_a_refresh_fo",
             "page_01_bottom_c_refresh_speed", "page_01_bottom_c_refresh_batch",
             "page_01_bottom_c_refresh_cfd"]
assert "page_01_bottom_animations_stop();" in function(source, "page_01_main_suspend")
assert "page_01_bottom_animations_stop();" in function(source, "ui_main_destroy")
with tempfile.TemporaryDirectory(prefix="un260-main-animation-") as directory:
    work = Path(directory)
    production = work / "main_animation_under_test.h"
    production.write_text(dirty_enum + "\n" + anim_enum + "\n" + intent + "\n" +
                          "\n\n".join(function(source, name) for name in functions), encoding="utf-8")
    executable = work / ("test.exe" if os.name == "nt" else "test")
    command = [compiler, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
               "-I" + str(ROOT), "-I" + str(work),
               str(ROOT / "tools/test_main_bottom_animation.c"),
               str(ROOT / "un260/lv_core/ui_frame_commit.c"), "-o", str(executable)]
    if os.name != "nt":
        command.append("-fsanitize=undefined")
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
