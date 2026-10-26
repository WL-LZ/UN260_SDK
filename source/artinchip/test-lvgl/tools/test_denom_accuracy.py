#!/usr/bin/env python3
"""Run independent production A-area accuracy cases and report every failure.

No failed case hides later scenarios. Temporary output defaults outside the
repository. These tests validate absolute controller 0x0B rows and independent 0x0E
totals. Every START clears counts while preserving denominations, for ADD ON
and OFF. The controller supplies all cumulative quantities.
"""
from pathlib import Path
import argparse
import json
import os
import re
import subprocess
import tempfile
from test_main_view import function

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, default=Path("/dev/shm/un260-controller-denom-live/accuracy-review/denom-accuracy.json"))
parser.add_argument("--case", action="append", dest="selected")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output.resolve()
out.parent.mkdir(parents=True, exist_ok=True)
temp_root = out.parent / "temp"
temp_root.mkdir(exist_ok=True)
source = (root / "un260/app_service/app_counting_runtime.c").read_text()
cases = re.findall(r'\{"([^"]+)",\s*\w+\}', (root / "tools/test_denom_accuracy.c").read_text())
if args.selected:
    unknown = set(args.selected) - set(cases)
    if unknown:
        parser.error("unknown case: " + ", ".join(sorted(unknown)))
    cases = args.selected
results = []
with tempfile.TemporaryDirectory(prefix="denom-accuracy-", dir=temp_root) as temp:
    work = Path(temp)
    stub = work / "un260/lv_drivers/lv_drivers.h"
    stub.parent.mkdir(parents=True)
    stub.write_text("void uart_debug_printf(const char *, ...);\n")
    (work / "accuracy_runtime_info.inc").write_text(function(source, "app_counting_runtime_format_amount") + "\n\n" + function(source, "app_counting_runtime_handle_info"))
    exe = work / "test"
    modules = [
        "counting_denom_reply.c", "counting_denom_query_service.c",
        "counting_info_reply.c", "counting_report_sync.c",
        "counting_reject_sn_reply.c", "counting_data_store.c", "counting_multi.c",
    ]
    command = [
        "cc", "-std=c11", "-Wall", "-Wextra", "-Wno-sign-compare",
        "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
        "-ffunction-sections", "-fdata-sections", "-no-pie",
        "-I" + str(work), "-I" + str(root),
        str(root / "tools/test_denom_accuracy.c"),
        *[str(root / "un260/counting" / name) for name in modules],
        "-Wl,--gc-sections", "-o", str(exe),
    ]
    env = dict(os.environ, TMPDIR=str(temp_root), TEMP=str(temp_root), TMP=str(temp_root))
    built = subprocess.run(command, text=True, capture_output=True, env=env)
    (out.parent / "denom-accuracy-build.log").write_text(built.stdout + built.stderr)
    if built.returncode:
        print(built.stdout + built.stderr)
        raise SystemExit(built.returncode)
    for name in cases:
        run = subprocess.run([str(exe), name], text=True, capture_output=True, env=env)
        output = run.stdout + run.stderr
        results.append({"case": name, "passed": run.returncode == 0, "returncode": run.returncode, "output": output})
        print(output, end="" if output.endswith("\n") else "\n", flush=True)
        if run.returncode:
            print(f"CASE FAILED: {name} (exit {run.returncode})", flush=True)
summary = {
    "cases": len(results), "passed": sum(r["passed"] for r in results),
    "failed": sum(not r["passed"] for r in results), "results": results,
    "scope": "Production runtime INFO handler and note/0B dispatch; UI and storage side effects stubbed.",
}
out.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
print(f"SUMMARY {summary['passed']}/{summary['cases']} passed; failures={summary['failed']}; {out}")
raise SystemExit(1 if summary["failed"] else 0)
