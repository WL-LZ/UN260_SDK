#!/usr/bin/env python3
"""History CSV/HTML and selection regression; all output goes to a temporary directory."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("cc/gcc required")

with tempfile.TemporaryDirectory(prefix="un260-history-export-") as directory:
    binary = Path(directory) / ("history-export-test.exe" if os.name == "nt" else "history-export-test")
    includes = ["-I" + str(ROOT / "tools/tests/history_export_stubs"), "-I" + str(ROOT)]
    dependency_root = os.environ.get("UN260_TEST_DEPENDENCY_ROOT")
    if dependency_root:
        includes += ["-I" + dependency_root]
    subprocess.run([compiler, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    *includes,
                    str(ROOT / "tools/test_history_export.c"),
                    str(ROOT / "un260/history/history_record_detail.c"),
                    str(ROOT / "un260/history/history_export_sn_parser.c"),
                    str(ROOT / "un260/history/history_export_text.c"),
                    str(ROOT / "un260/protocol/protocol_frame.c"),
                    str(ROOT / "un260/counting/counting_reject_reason.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary), directory], check=True)
