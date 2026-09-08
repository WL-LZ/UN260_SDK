#!/usr/bin/env python3
"""Compile real language tables, picker and reject mapping for host validation."""
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

with tempfile.TemporaryDirectory(prefix="un260-list-i18n-") as directory:
    executable = Path(directory) / ("test.exe" if os.name == "nt" else "test")
    command = shlex.split(compiler) if os.environ.get("CC") else [compiler]
    sources = ["tools/test_list_i18n.c", "un260/lv_system/ui_lang.c",
               "un260/lv_system/ui_text_page.c", "un260/lv_system/ui_text_widget.c",
               "un260/counting/counting_reject_reason.c"]
    command += ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT), *[str(ROOT / source) for source in sources],
                "-o", str(executable)]
    if os.name != "nt":
        command += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    subprocess.run(command, check=True)
    subprocess.run([str(executable)], check=True)
