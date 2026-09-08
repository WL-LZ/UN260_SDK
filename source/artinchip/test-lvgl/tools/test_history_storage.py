#!/usr/bin/env python3
"""Linux host test of real history storage/worker/counting, with injected I/O faults."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Linux cc/gcc required")


def extract_function(source, signature):
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

with tempfile.TemporaryDirectory(prefix="un260-history-worker-") as directory:
    work = Path(directory)
    state = work / "history"
    binary = work / "history-test"
    runtime = (ROOT / "un260/app_service/app_counting_runtime.c").read_text(encoding="utf-8")
    (work / "history_runtime_under_test.h").write_text(extract_function(
        runtime, "bool app_counting_runtime_reset_session("), encoding="utf-8")
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=undefined", "-pthread", "-I" + str(ROOT), "-I" + str(work),
                    '-DUI_HISTORY_STORE_DIR="' + state.as_posix() + '"',
                    str(ROOT / "tools/test_history_storage.c"),
                    str(ROOT / "un260/storage/storage_worker.c"),
                    str(ROOT / "un260/counting/counting_history_service.c"),
                    "-o", str(binary)], check=True)
    subprocess.run([str(binary), "exercise"], check=True)

    def dump():
        return tuple(map(int, subprocess.check_output([str(binary), "dump"], text=True).split()))

    baseline = dump()
    assert baseline[:2] == (20, 23), baseline
    authoritative = state / "index.cfg"
    before = authoritative.read_bytes()
    subprocess.run([str(binary), "init-failure"], check=True)
    assert authoritative.read_bytes() == before
    assert dump() == baseline
    subprocess.run([str(binary), "total-arithmetic"], check=True)
    arithmetic = dump()
    assert arithmetic == (20, 4, baseline[2] + 2)
    baseline = arithmetic
    for stage in (1, 2):
        before = authoritative.read_bytes()
        result = subprocess.run([str(binary), "crash", str(stage)])
        assert result.returncode == 50 + stage
        assert authoritative.read_bytes() == before
        assert dump() == baseline, "Partial temporary index must never replace committed data"
    result = subprocess.run([str(binary), "crash", "3"])
    assert result.returncode == 53
    after_rename = dump()
    assert after_rename == (20, baseline[1] + 1, baseline[2] + 1)
    # A process kill after rename models a complete new index (real sudden power
    # loss may retain old OR new). Neither outcome may contain a partial index.
    (state / "01.rec").write_text("corrupt mirror\n")
    (state / "meta.cfg").write_text("corrupt metadata mirror\n")
    assert dump() == after_rename, "Self-contained v2 index remains the only recovery authority"
    print("PASS: fresh-process v2 recovery, before-fsync/before-rename/after-rename interruption models, stale-mirror isolation")
