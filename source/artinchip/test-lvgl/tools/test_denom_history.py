#!/usr/bin/env python3
"""Production note/report/runtime/history/storage integration with disk reread."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
from test_i18n_support import with_i18n
from test_main_view import function

root = Path(__file__).resolve().parents[1]
out = Path("/dev/shm/un260-a-accuracy")
out.mkdir(parents=True, exist_ok=True)
temp_root = out / "temp"
temp_root.mkdir(exist_ok=True)
runtime = (root / "un260/app_service/app_counting_runtime.c").read_text()
results = []
with tempfile.TemporaryDirectory(prefix="denom-history-", dir=temp_root) as temp:
    work = Path(temp); store = work / "history"; exe = work / "test"
    (work / "history_runtime_under_test.h").write_text(function(runtime, "app_counting_runtime_reset_session"))
    names = [
        "app_counting_runtime_try_history_commit", "app_counting_runtime_handle_info",
        "app_counting_runtime_handle_denom", "app_counting_runtime_on_history_record",
    ]
    (work / "denom_history_runtime.inc").write_text("\n\n".join(function(runtime, name) for name in names))
    stub = work / "un260/lv_drivers/lv_drivers.h"; stub.parent.mkdir(parents=True)
    stub.write_text("void uart_debug_printf(const char *, ...);\n")
    parts = [
        "tools/test_denom_history.c", "un260/storage/storage_worker.c",
        "un260/counting/counting_history_service.c", "un260/counting/counting_data_store.c",
        "un260/history/history_record_snapshot.c", "un260/history/history_record_reader.c",
        "un260/history/history_read_task.c", "un260/history/history_record_detail.c",
        "un260/history/history_export_sn_parser.c", "un260/history/history_query.c",
        "un260/counting/counting_serial_text.c", "un260/counting/counting_info_reply.c",
        "un260/protocol/protocol_frame.c", "un260/history/history_export_text.c",
        "un260/lv_system/ui_report_i18n.c", "un260/counting/counting_reject_reason.c",
        "un260/counting/counting_control_reply.c", "un260/counting/counting_report_sync.c",
        "un260/counting/counting_denom_reply.c", "un260/counting/counting_denom_query_service.c",
        "un260/counting/counting_reject_sn_reply.c",
    ]
    sources = with_i18n([root / path for path in parts], root)
    command = [
        os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
        "-Wno-misleading-indentation", "-Wno-unused-variable", "-Wno-sign-compare",
        "-pthread", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
        "-I" + str(work), "-I" + str(root / "tools/tests/history_export_stubs"), "-I" + str(root),
        '-DUI_HISTORY_STORE_DIR="' + str(store) + '"', *map(str, sources),
        "-Wl,--wrap=malloc", "-o", str(exe),
    ]
    env = dict(os.environ, TMPDIR=str(temp_root), TEMP=str(temp_root), TMP=str(temp_root))
    compiled = subprocess.run(command, capture_output=True, text=True, env=env)
    (out / "denom-history-build.log").write_text(compiled.stdout + compiled.stderr)
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
        raise SystemExit(compiled.returncode)
    for mode in ("write", "read"):
        run = subprocess.run([str(exe), mode], capture_output=True, text=True, env=env)
        output = run.stdout + run.stderr
        print(output, end="" if output.endswith("\n") else "\n", flush=True)
        results.append({"phase": mode, "returncode": run.returncode, "output": output})
        if run.returncode:
            break
summary = {"passed": len(results) == 2 and all(r["returncode"] == 0 for r in results), "results": results}
(out / "denom-history.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print("SUMMARY history integration", "PASS" if summary["passed"] else "FAIL")
raise SystemExit(0 if summary["passed"] else 1)
