#!/usr/bin/env python3
"""Pixel/ownership tests using current production present functions, not copies."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("A host C compiler is required")


def function(source, signature):
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


source = (ROOT / "lv_port_disp.c").read_text(encoding="utf-8")
template = (ROOT / "tools/test_present_pipeline.c").read_text(encoding="utf-8")
signatures = (
    "static bool sync_disp_buf(",
    "static size_t collect_damage(",
    "static void force_full_damage(",
    "static void fbdev_render_start(",
    "static bool present_submit(",
    "static void present_complete(",
    "bool lv_port_disp_poll(",
    "static void fbdev_flush(",
)
functions = "\n\n".join(function(source, signature) for signature in signatures)
globals_start = source.index("static struct fb_var_screeninfo g_pan_var;")
globals_end = source.index("#ifdef USE_DRAW_BUF", globals_start)
code = template.replace("/* ACTUAL_PRESENT_GLOBALS */", source[globals_start:globals_end])
code = code.replace("/* ACTUAL_PRESENT_FUNCTIONS */", functions)

with tempfile.TemporaryDirectory(prefix="un260-present-pipeline-") as directory:
    work = Path(directory)
    generated = work / "present_test.c"
    generated.write_text(code, encoding="utf-8")
    executable = work / ("present_test.exe" if os.name == "nt" else "present_test")
    flags = [compiler, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-parameter", "-I" + str(ROOT)]
    if os.name != "nt":
        flags.append("-fsanitize=undefined")
    subprocess.run(flags + [str(generated), str(ROOT / "aic_ui/present_damage.c"),
                            "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
