#!/usr/bin/env python3
"""Compile real controller/0B/runtime modules and run independent direct-display cases.

Private customer input remains external (--capture); the formal test embeds
only three readable screenshot rows. RX trailing bytes are retained, never
claimed to be verified as a checksum. Temporary output is outside the source.
"""
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


def start_preparation(source):
    """Extract the real accepted-start preparation block, not a copied model."""
    anchor = source.index("counting_report_begin(")
    begin = source.rfind("\n    if (", 0, anchor) + 1
    brace = source.index("{", begin)
    depth = 1
    cursor = brace + 1
    while depth:
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
        cursor += 1
    block = source[begin:cursor]
    assert "cmd == 0x0A" in block and "counting_report_begin(" in block
    return ("static void command_start_prepare(uint8_t cmd,const uint8_t *buf,uint8_t len)\n"
            "{\n" + block + "\n}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--production-root", type=Path, default=ROOT)
    parser.add_argument("--capture", type=Path)
    parser.add_argument("--case", action="append", dest="selected")
    args = parser.parse_args()
    root, out = args.production_root.resolve(), args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    temp_root = out / "temp"
    temp_root.mkdir(exist_ok=True)
    environment = dict(os.environ, TMPDIR=str(temp_root), TEMP=str(temp_root),
                       TMP=str(temp_root), PYTHONDONTWRITEBYTECODE="1")
    c_source = root / "tools/test_controller_denom_live.c"
    runtime = root / "un260/app_service/app_counting_runtime.c"
    command_runtime = root / "un260/app_service/app_command_runtime.c"
    names = re.findall(r'\{"([^"]+)",\s*\w+\}', c_source.read_text())
    if args.capture:
        names.append("capture_one_note")
    if args.selected:
        unknown = set(args.selected) - set(names)
        if unknown:
            parser.error("unknown case or missing --capture: " + ", ".join(sorted(unknown)))
        names = args.selected
    counting_sources = [
        "counting_denom_reply.c", "counting_denom_query_service.c",
        "counting_control_reply.c", "counting_info_reply.c",
        "counting_report_sync.c", "counting_reject_sn_reply.c",
        "counting_data_store.c", "counting_multi.c",
    ]
    sources = [root / "un260/counting" / name for name in counting_sources]
    sources += [root / "un260/protocol/protocol_frame.c"]
    results = []
    with tempfile.TemporaryDirectory(prefix="controller-denom-", dir=temp_root) as temporary:
        work = Path(temporary)
        stub = work / "un260/lv_drivers/lv_drivers.h"
        stub.parent.mkdir(parents=True)
        stub.write_text("void uart_debug_printf(const char *, ...);\n")
        text = runtime.read_text()
        command_text = command_runtime.read_text()
        generated = start_preparation(command_text) + "\n"
        generated += function(command_text, "app_command_runtime_count_start_busy") + "\n"
        generated += function(command_text, "app_command_runtime_poll") + "\n"
        for name in ["app_counting_runtime_handle_info",
                     "app_counting_runtime_handle_denom"]:
            generated += function(text, name) + "\n\n"
        (work / "controller_denom_runtime.inc").write_text(generated)
        # Preserve the exact extracted entry points for later source review.
        (out / "runtime-under-test.inc").write_text(generated)
        exe = work / "test_controller_denom_live"
        command = [
            os.environ.get("CC", "cc"), "-std=c11", "-g", "-O1", "-Wall", "-Wextra",
            "-Wno-sign-compare", "-fsanitize=address,undefined",
            "-fno-sanitize-recover=all", "-ffunction-sections", "-fdata-sections",
            "-no-pie", "-I" + str(work), "-I" + str(root),
            str(c_source), *map(str, sources), "-Wl,--gc-sections", "-o", str(exe),
        ]
        built = subprocess.run(command, env=environment, text=True, capture_output=True)
        (out / "build.log").write_text(built.stdout + built.stderr)
        if built.returncode:
            print(built.stdout + built.stderr)
            raise SystemExit(built.returncode)
        for name in names:
            case_command = [str(exe), name]
            if name == "capture_one_note":
                case_command.append(str(args.capture.resolve()))
            tested = subprocess.run(case_command, env=environment, text=True,
                                    capture_output=True, timeout=30)
            output = tested.stdout + tested.stderr
            (out / (name + ".log")).write_text(output)
            results.append({"case": name, "passed": tested.returncode == 0,
                            "returncode": tested.returncode, "log": name + ".log"})
            print(output, end="" if output.endswith("\n") else "\n", flush=True)
    hashes = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [c_source, Path(__file__), runtime, command_runtime, *sources]
              if str(path).startswith(str(root) + "/")}
    summary = {
        "scope": "Real accepted-0A preflight/control, runtime 0E and 0B, parser/query/report/49; UI and persistent storage effects stubbed.",
        "add_scenarios": "ADD uses the same direct-display 0B stream; no local A accumulation",
        "cases": len(results), "passed": sum(item["passed"] for item in results),
        "failed": sum(not item["passed"] for item in results),
        "results": results, "sources": hashes, "board_test": False,
    }
    if args.capture:
        summary["capture_sha256"] = hashlib.sha256(args.capture.read_bytes()).hexdigest()
    (out / "results.json").write_text(json.dumps(summary, indent=2))
    print("SUMMARY", summary["passed"], "/", summary["cases"], "passed")
    raise SystemExit(bool(summary["failed"]))


if __name__ == "__main__":
    main()
