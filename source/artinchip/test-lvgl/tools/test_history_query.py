#!/usr/bin/env python3
"""Compile and run pure, production history parsing/query code without LVGL."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("A C11 host compiler is required")

with tempfile.TemporaryDirectory(prefix="un260-history-query-") as directory:
    binary = Path(directory) / ("history-query.exe" if os.name == "nt" else "history-query")
    flags = [] if os.name == "nt" else ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    *flags, "-I" + str(ROOT), str(ROOT / "tools/test_history_query.c"),
                    str(ROOT / "un260/history/history_record_detail.c"),
                    str(ROOT / "un260/history/history_query.c"),
                    str(ROOT / "un260/counting/counting_serial_text.c"),
                    str(ROOT / "un260/history/history_export_sn_parser.c"),
                    str(ROOT / "un260/protocol/protocol_frame.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
