#!/usr/bin/env python3
"""Exercise the actual storage worker with deterministic allocation/I/O gates."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--sanitize", choices=("undefined", "address,undefined"), default="undefined")
args = parser.parse_args()
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Linux cc/gcc required")

with tempfile.TemporaryDirectory(prefix="un260-storage-worker-") as directory:
    binary = Path(directory) / "storage-worker-test"
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fno-omit-frame-pointer", "-fsanitize=" + args.sanitize,
                    "-no-pie", "-pthread", "-I" + str(ROOT),
                    str(ROOT / "tools/test_storage_worker.c"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
