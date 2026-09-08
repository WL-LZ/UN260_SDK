#!/usr/bin/env python3
"""Compile the real, LVGL-free physics implementation and deterministic scenarios."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Host C compiler required")

with tempfile.TemporaryDirectory(prefix="un260-scroll-physics-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
               "-I" + str(ROOT), str(ROOT / "tools/test_scroll_physics.c"),
               str(ROOT / "un260/lv_components/ui_scroll_physics.c"), "-lm",
               "-o", str(executable)]
    if os.name != "nt":
        command.extend(["-fsanitize=undefined,float-cast-overflow", "-fno-sanitize-recover=all"])
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
