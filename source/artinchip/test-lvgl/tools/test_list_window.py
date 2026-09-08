#!/usr/bin/env python3
"""Compile the real LVGL-free list model and deterministic host scenarios."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Host C compiler required")

with tempfile.TemporaryDirectory(prefix="un260-list-window-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    command = shlex.split(compiler) if os.environ.get("CC") else [compiler]
    command += ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT), str(ROOT / "tools/test_list_window.c"),
                str(ROOT / "un260/lv_components/ui_list_window.c"), "-lm",
                "-o", str(executable)]
    if os.name != "nt":
        command += ["-fsanitize=undefined,float-cast-overflow", "-fno-sanitize-recover=all"]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
