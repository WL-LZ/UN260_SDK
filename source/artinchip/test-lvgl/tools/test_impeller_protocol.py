#!/usr/bin/env python3
"""Independent real 0x4B owner/model/runtime checks; no hardware or LVGL claims."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from test_currency_modes import function

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--case", action="append", dest="selected")
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    temp_root = out / "temp"
    temp_root.mkdir(exist_ok=True)
    env = dict(os.environ, TMPDIR=str(temp_root), TEMP=str(temp_root), TMP=str(temp_root),
               PYTHONDONTWRITEBYTECODE="1")
    test = ROOT / "tools/test_impeller_protocol.c"
    detail = ROOT / "un260/app_service/app_setting_reply_detail.c"
    runtime = ROOT / "un260/app_service/app_setting_runtime.c"
    serial = ROOT / "un260/app_service/app_serial_runtime.c"
    parts = [function(detail.read_text(), "setting_reply_handle_impeller"),
             function(detail.read_text(), "app_setting_reply_handle_detail"),
             function(runtime.read_text(), "app_setting_runtime_handle_reply"),
             function(runtime.read_text(), "app_setting_runtime_poll"),
             function(serial.read_text(), "app_serial_runtime_observe_reply")]
    generated = "\n\n".join(parts)
    (out / "runtime-under-test.inc").write_text(generated)
    cases = re.findall(r'\{"([^"]+)",\w+\}', test.read_text())
    if args.selected:
        unknown = set(args.selected) - set(cases)
        if unknown:
            parser.error("unknown cases: " + ", ".join(sorted(unknown)))
        cases = args.selected
    sources = [ROOT / name for name in [
        "un260/app_service/setting_service.c", "un260/protocol/protocol_request.c",
        "un260/protocol/protocol_frame.c", "un260/machine_state/machine_state.c",
        "un260/lv_components/fault_guide/fault_guide_catalog.c",
    ]]
    results = []
    for optimization in ["-O0", "-O2"]:
        label = optimization[1:]
        with tempfile.TemporaryDirectory(prefix="impeller-" + label + "-", dir=temp_root) as temporary:
            work = Path(temporary)
            stub = work / "un260/lv_drivers/lv_drivers.h"
            stub.parent.mkdir(parents=True)
            stub.write_text("void uart_debug_printf(const char *, ...);\n")
            (work / "impeller_runtime_under_test.inc").write_text(generated)
            exe = work / "test_impeller_protocol"
            command = [os.environ.get("CC", "cc"), "-std=c11", optimization, "-g",
                       "-Wall", "-Wextra", "-fsanitize=address,undefined",
                       "-fno-sanitize-recover=all", "-ffunction-sections", "-fdata-sections",
                       "-no-pie", "-I" + str(work), "-I" + str(ROOT), str(test),
                       *map(str, sources), "-Wl,--gc-sections", "-o", str(exe)]
            build = subprocess.run(command, env=env, text=True, capture_output=True)
            (out / ("build-" + label + ".log")).write_text(build.stdout + build.stderr)
            if build.returncode:
                print(build.stdout + build.stderr)
                raise SystemExit(build.returncode)
            for name in cases:
                run = subprocess.run([str(exe), name], env=env, text=True, capture_output=True, timeout=30)
                log = label + "-" + name + ".log"
                (out / log).write_text(run.stdout + run.stderr)
                results.append(dict(case=name, optimization=label, returncode=run.returncode,
                                    passed=run.returncode == 0, log=log))
                print(label, (run.stdout + run.stderr).strip(), flush=True)
    checked = [test, Path(__file__).resolve(), detail, runtime, serial, *sources,
               ROOT / "un260/app_service/setting_service.h",
               ROOT / "un260/machine_state/machine_state.h"]
    summary = dict(scenarios=len(cases), runs=len(results),
                   passed=sum(r["passed"] for r in results),
                   failed=sum(not r["passed"] for r in results), results=results,
                   sources={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in checked},
                   scope="Real service/request/model/frame and extracted exact reply/runtime poll/RX observer; unrelated UI/history edges stubbed.",
                   board_test=False, native_lvgl_test=False,
                   untagged_ack_limit="An old ACK arriving after a new same-command request cannot be distinguished without a wire transaction tag; no claim of solving that ambiguity.")
    (out / "results.json").write_text(json.dumps(summary, indent=2))
    print("SUMMARY", summary["passed"], "/", summary["runs"], "runs passed")
    raise SystemExit(bool(summary["failed"]))

if __name__ == "__main__":
    main()
