#!/usr/bin/env python3
"""Real history parser + storage thread: asynchronous startup and failure ownership."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("Linux cc/gcc required")


def index_text(count):
    lines = ["magic=1212765012", "version=2", f"total_notes_counted={400 + count}",
             f"next_record_no={count + 1}", "next_slot_no=1", f"record_count={count}"]
    for i in range(count):
        fields = {"valid": 1, "selected": 0, "slot_no": count - i,
                  "record_no": count - i, "pcs": 2, "amount": 20, "currency": "USD",
                  "time": "2026-09-14 08:33:26", "denom": "10 x 2", "sn": f"SN{i:04d}",
                  "sn_detail": f"01\\t10\\tSN{i:04d}", "err": "", "start": "start", "end": "end",
                  "log": "session"}
        lines.extend(f"record{i:02d}_{key}={value}" for key, value in fields.items())
    return "\n".join(lines) + "\n"


with tempfile.TemporaryDirectory(prefix="un260-history-async-") as directory:
    work = Path(directory)
    state = work / "history"
    state.mkdir()
    binary = work / "history-async-test"
    subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                    "-fno-omit-frame-pointer", "-fsanitize=undefined", "-no-pie", "-pthread",
                    "-I" + str(ROOT), '-DUI_HISTORY_STORE_DIR="' + state.as_posix() + '"',
                    str(ROOT / "tools/test_history_async.c"),
                    str(ROOT / "un260/storage/storage_worker.c"),
                    str(ROOT / "un260/counting/counting_history_service.c"),
                    str(ROOT / "un260/counting/counting_data_store.c"),
                    "-Wl,--wrap=malloc", "-o", str(binary)], check=True)
    index = state / "index.cfg"

    def run(scenario, count=0, content=None, writes=False):
        if content is not None:
            index.write_text(content)
        elif scenario == "missing":
            index.unlink(missing_ok=True)
        else:
            index.write_text(index_text(count))
        before = index.read_bytes() if index.exists() else None
        subprocess.run([str(binary), scenario, str(count)], check=True, timeout=15)
        if not writes:
            assert (index.read_bytes() if index.exists() else None) == before

    run("missing")
    for count in (0, 1, 100):
        run("async", count)
        run("sync", count)
    run("queued", 100)
    run("queue-full", 100)
    for scenario in ("worker-failure", "submit-failure", "result-failure", "read-error"):
        run(scenario)
    for corrupt in ("bad index\n", index_text(100)[:-1], index_text(100).replace("version=2", "version=999")):
        run("bad", content=corrupt)
    run("early-record", 100, writes=True)
    print("PASS: async startup (0/1/100/missing), worker/queue/OOM/read/corrupt failures, pending getters/mutators, "
          "slow load and FIFO, publish boundary, idempotent init, early count rebased on loaded lifetime")
