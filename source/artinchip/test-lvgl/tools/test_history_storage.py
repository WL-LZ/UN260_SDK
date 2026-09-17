#!/usr/bin/env python3
"""Linux host test of real history storage/worker/counting, with injected I/O faults."""
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
    def compile_binary(output, overlay=None, store_path=state):
        includes = ["-I" + str(overlay)] if overlay else []
        subprocess.run([compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                        "-fno-omit-frame-pointer", "-fsanitize=" + args.sanitize,
                        "-no-pie", "-pthread", *includes, "-I" + str(ROOT), "-I" + str(work),
                        '-DUI_HISTORY_STORE_DIR="' + store_path.as_posix() + '"',
                        str(ROOT / "tools/test_history_storage.c"),
                        str(ROOT / "un260/storage/storage_worker.c"),
                        str(ROOT / "un260/counting/counting_history_service.c"),
                        str(ROOT / "un260/counting/counting_data_store.c"),
                        "-Wl,--wrap=malloc", "-o", str(output)], check=True)

    compile_binary(binary)
    multi_binary=work / "multi-history-test"
    compile_binary(multi_binary,store_path=work / "multi-history")
    subprocess.run([str(multi_binary), "multi-history"], check=True)
    subprocess.run([str(multi_binary), "multi-reload"], check=True)
    record_size, store_size, capacity = map(int, subprocess.check_output(
        [str(binary), "sizes"], text=True).split())
    assert capacity == 100
    print(f"History sizes: record={record_size}, store={store_size}, capacity={capacity}", flush=True)

    def dump():
        return tuple(map(int, subprocess.check_output([str(binary), "dump"], text=True).split()))

    authoritative = state / "index.cfg"

    def record_fields():
        return dict(line.split("=", 1) for line in authoritative.read_text().splitlines())

    # The disk format is unchanged: generate a real 20-capacity v2 index using
    # the same writer, with only its capacity header changed in this temp overlay.
    overlay = work / "legacy"
    legacy_directory = overlay / "un260/lv_system"
    legacy_directory.mkdir(parents=True)
    production_header = (ROOT / "un260/lv_system/ui_history_data.h").read_text()
    assert production_header.count("#define UI_HISTORY_MAX_RECORDS 100") == 1
    (legacy_directory / "ui_history_data.h").write_text(production_header.replace(
        "#define UI_HISTORY_MAX_RECORDS 100", "#define UI_HISTORY_MAX_RECORDS 20"))
    shutil.copyfile(ROOT / "un260/lv_system/ui_history_data_fs.c",
                    legacy_directory / "ui_history_data_fs.c")
    legacy_binary = work / "history-legacy20-test"
    compile_binary(legacy_binary, overlay)
    subprocess.run([str(legacy_binary), "fill-capacity"], check=True)
    # Single-currency fields are identical in v2; exercise the actual v2 header.
    legacy_index = authoritative.read_bytes().replace(b"version=3\n",b"version=2\n")
    authoritative.write_bytes(legacy_index)
    legacy_records = record_fields()
    assert dump() == (20, 20, 21)
    assert authoritative.read_bytes() == legacy_index, "Upgrade read must not rewrite the old index"
    subprocess.run([str(binary), "fill-capacity"], check=True)
    assert dump() == (capacity, capacity, capacity + 1)
    expanded = record_fields()
    for i in range(20):
        for key, value in legacy_records.items():
            prefix = f"record{i:02d}_"
            if key.startswith(prefix):
                assert expanded[f"record{capacity - 20 + i:02d}_" + key[len(prefix):]] == value
    assert (state / "100.rec").is_file()
    assert "slot100_slot_no=100\n" in (state / "100.rec").read_text()
    oldest_id = expanded[f"record{capacity - 1:02d}_record_no"]
    oldest_slot = expanded[f"record{capacity - 1:02d}_slot_no"]
    subprocess.run([str(binary), "append-one"], check=True)
    replaced = record_fields()
    assert dump() == (capacity, capacity + 1, capacity + 2)
    assert replaced["record00_slot_no"] == oldest_slot
    assert oldest_id not in [replaced[f"record{i:02d}_record_no"] for i in range(capacity)]
    expanded_index = authoritative.read_bytes()
    subprocess.run([str(legacy_binary), "load-corrupt"], check=True)
    assert authoritative.read_bytes() == expanded_index, "Downgrade must protect a larger index"
    print("PASS: 20-to-100 v2 upgrade preserves every record/ID/slot, slot100 reload, 101st oldest-only replacement, protected downgrade", flush=True)
    shutil.rmtree(state)  # Isolated TemporaryDirectory fixture, never device state.
    subprocess.run([str(binary), "exercise"], check=True)
    baseline = dump()
    assert baseline[:2] == (21, 23), baseline
    before = authoritative.read_bytes()
    subprocess.run([str(binary), "init-failure"], check=True)
    assert authoritative.read_bytes() == before
    assert dump() == baseline
    subprocess.run([str(binary), "total-arithmetic"], check=True)
    arithmetic = dump()
    assert arithmetic == (23, 4, baseline[2] + 2)
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
    assert after_rename == (24, baseline[1] + 1, baseline[2] + 1)
    # A process kill after rename models a complete new index (real sudden power
    # loss may retain old OR new). Neither outcome may contain a partial index.
    (state / "01.rec").write_text("corrupt mirror\n")
    (state / "meta.cfg").write_text("corrupt metadata mirror\n")
    assert dump() == after_rename, "Self-contained v2 index remains the only recovery authority"
    print("PASS: fresh-process v2 recovery, before-fsync/before-rename/after-rename interruption models, stale-mirror isolation")

    subprocess.run([str(binary), "retention"], check=True)
    retained = dump()
    assert retained == (capacity, after_rename[1] + capacity + 3, after_rename[2] + capacity + 3)

    before = record_fields()
    oldest_id = int(before[f"record{capacity - 1:02d}_record_no"])
    oldest_slot = before[f"record{capacity - 1:02d}_slot_no"]
    subprocess.run([str(binary), "append-one"], check=True)
    after = record_fields()
    assert int(after["record00_record_no"]) == retained[2]
    assert after["record00_slot_no"] == oldest_slot
    assert oldest_id not in [int(after[f"record{i:02d}_record_no"]) for i in range(capacity)]
    assert dump() == (capacity, retained[1] + 1, retained[2] + 1)

    subprocess.run([str(binary), "delete-all"], check=True)
    empty = dump()
    assert empty == (0, retained[1] + 1, retained[2] + 1)
    subprocess.run([str(binary), "append-one"], check=True)
    assert dump() == (1, empty[1] + 1, empty[2] + 1)
    assert int(record_fields()["record00_record_no"]) == empty[2]
    print("PASS: retention and delete-all survive fresh-process reload without record ID reuse")

    healthy_index = authoritative.read_bytes()
    healthy_state = dump()
    for command in ("load-eio", "load-eacces", "load-read-error"):
        subprocess.run([str(binary), command], check=True)
        assert authoritative.read_bytes() == healthy_index
        assert dump() == healthy_state
    for corrupt_index in (b"invalid index\n", healthy_index[:-1],
                          healthy_index.replace(b"version=3\n", b"version=999\n")):
        authoritative.write_bytes(corrupt_index)
        subprocess.run([str(binary), "load-corrupt"], check=True)
        assert authoritative.read_bytes() == corrupt_index
        # Repair is simulated only in this temporary fixture. Production never
        # replaces or migrates an unreadable index automatically.
        authoritative.write_bytes(healthy_index)
        assert dump() == healthy_state
    print("PASS: EIO/EACCES/read failure/corrupt index protection and recovery on restart")
    subprocess.run([str(binary), "delete-records"], check=True)
    assert dump() == (0, healthy_state[1] + capacity, healthy_state[2] + capacity)
    print("PASS: stable-ID batch deletion remains committed after fresh-process reload")
    subprocess.run([str(binary), "maximum-text"], check=True)
    maximum_index = authoritative.read_bytes()
    subprocess.run([str(binary), "reload-maximum-text"], check=True)
    assert authoritative.read_bytes() == maximum_index
    assert max(map(len, maximum_index.splitlines())) < 8191
    mirrors = sum(path.stat().st_size for path in state.glob("*.rec"))
    metadata = (state / "meta.cfg").stat().st_size
    # Every escaped character is at most two bytes. 16 KiB per record safely
    # covers all fixed text fields plus maximum-width v2 scalar keys/values.
    assert len(maximum_index) <= capacity * 16384 + 1024
    assert mirrors <= capacity * 16384 and metadata <= 1024
    print(f"Maximum-text disk fixture: index={len(maximum_index)}, mirrors={mirrors}, "
          f"meta={metadata}, persisted={len(maximum_index) + mirrors + metadata} bytes", flush=True)
