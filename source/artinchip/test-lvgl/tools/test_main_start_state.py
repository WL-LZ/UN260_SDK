#!/usr/bin/env python3
"""Regression for CLEAR projecting START after the request has settled.

Extracts the actual runtime clear/busy functions and action-service clear/send
functions. Protocol request primitives and UI projection are observable stubs;
this is not a hardware or rendering test.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:static\s+)?(?:void|bool)\s+" + re.escape(name) +
                      r"\([^;]*?\)\s*\{", source, re.M)
    assert match, "Missing production function: " + name
    opening = source.index("{", match.start())
    depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for token in tokens.finditer(source, opening):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():token.end()]
    raise AssertionError("Unterminated production function: " + name)


def main():
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    if not compiler:
        raise SystemExit("Host C compiler required")
    runtime = (ROOT / "un260/app_service/app_command_runtime.c").read_text(encoding="utf-8")
    action = (ROOT / "un260/counting/counting_action_service.c").read_text(encoding="utf-8")
    constants = "\n".join(re.findall(r"^#define COUNTING_(?:CLEAR_CMD|ACTION_REQUEST)\s+.*$", action, re.M))
    assert len(constants.splitlines()) == 2
    production = constants + "\n\n" + "\n\n".join(function(action, name) for name in (
        "counting_action_send", "counting_action_request_clear", "counting_action_start_pending"))
    production += "\n\n" + "\n\n".join(function(runtime, name) for name in (
        "app_command_runtime_count_start_busy", "app_command_runtime_clear_counting_data"))
    with tempfile.TemporaryDirectory(prefix="un260-main-start-state-") as directory:
        work = Path(directory)
        (work / "main_start_state_under_test.h").write_text(production, encoding="utf-8")
        executable = work / ("test.exe" if os.name == "nt" else "test")
        command = [compiler, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror",
                   "-I" + str(work), str(ROOT / "tools/test_main_start_state.c"),
                   "-o", str(executable)]
        if os.name != "nt":
            command.extend(("-fsanitize=undefined", "-fno-sanitize-recover=undefined"))
        subprocess.run(command, check=True, timeout=60)
        subprocess.run([str(executable)], check=True, timeout=15)


if __name__ == "__main__":
    main()
