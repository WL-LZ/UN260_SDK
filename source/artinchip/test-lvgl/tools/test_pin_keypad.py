#!/usr/bin/env python3
"""Compile/run the real PIN component and password pages with LVGL host fakes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("A host C compiler (cc/gcc or CC) is required")
with tempfile.TemporaryDirectory(prefix="un260-pin-keypad-") as directory:
    executable = Path(directory) / ("pin-test.exe" if os.name == "nt" else "pin-test")
    command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
               "-I" + str(ROOT / "tools/tests/pin_stubs"), "-I" + str(ROOT),
               str(ROOT / "tools/test_pin_keypad.c"), "-o", str(executable)]
    if os.name != "nt":
        command.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
