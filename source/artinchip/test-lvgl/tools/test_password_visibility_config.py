#!/usr/bin/env python3
"""Compile actual Linux user_cfg.c and test isolated PIN-display persistence."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    raise SystemExit("A Linux host C compiler (cc/gcc or CC) is required")


def snapshot(path):
    stat = path.stat()
    return path.read_bytes(), stat.st_mtime_ns, stat.st_ino


with tempfile.TemporaryDirectory(prefix="un260-password-visibility-") as directory:
    work = Path(directory)
    state = work / "state"
    config = state / "password_visibility.cfg"
    temporary = state / "password_visibility.cfg.tmp"
    executable = work / "password-visibility-test"
    subprocess.run([
        compiler, "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
        "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined",
        '-DUI_STATE_DIR="' + state.as_posix() + '"',
        "-I" + str(ROOT / "tools/tests/password_visibility_stubs"),
        "-I" + str(ROOT),
        str(ROOT / "tools/tests/test_password_visibility_config.c"),
        "-o", str(executable),
    ], check=True)

    def run(*arguments):
        # Every invocation starts with fresh production globals: a process restart.
        subprocess.run([str(executable), *map(str, arguments)], check=True)

    run("get", 0)
    assert not state.exists(), "Reading a missing setting must not write anything"
    run("save", 0, 0)
    assert config.read_bytes() == b"0\n"

    sentinels = {
        "password.cfg": b"8372\n",
        "screenshot.cfg": b"1\n",
        "screen_recording.cfg": b"0\n",
        "performance_monitor.cfg": b"1\n",
        "performance_profile.cfg": b"0\n",
        "gestures.cfg": b"1\n",
        "touch_feedback.cfg": b"1\n",
        "unrelated-sentinel.cfg": b"Do not change this file.\x00\n",
    }
    for name, contents in sentinels.items():
        (state / name).write_bytes(contents)
    original_sentinels = {name: snapshot(state / name) for name in sentinels}

    def assert_isolated():
        assert {name: snapshot(state / name) for name in sentinels} == original_sentinels
        assert set(path.name for path in state.iterdir()) <= set(sentinels) | {config.name}
        assert not temporary.exists(), "Atomic-save temporary file must be cleaned up"

    for value in (0, 1):
        for payload in (str(value).encode(), f"{value}\n".encode(), f"{value}\r\n".encode()):
            config.write_bytes(payload)
            run("get", value)
            run("load", value, 1)
            before = snapshot(config)
            run("same", value)
            assert snapshot(config) == before, "Unchanged valid setting was rewritten"
            assert_isolated()

    for payload in (b"", b"2\n", b"-1\n", b"true\n", b"01\n", b"1oops\n",
                    b"1\n0\n", b"1\x00\n", b" 1\n", b"1 \n"):
        config.write_bytes(payload)
        before = snapshot(config)
        run("get", 0)
        run("load", 0, 0)
        assert snapshot(config) == before, "Invalid values must not be repaired by reads"
        assert_isolated()

    # Missing/invalid false defaults are not considered already persisted.
    config.unlink()
    run("load", 0, 0)
    run("save", 0, 0)
    assert config.read_bytes() == b"0\n"
    config.write_bytes(b"invalid")
    run("save", 0, 0)
    assert config.read_bytes() == b"0\n"

    for previous, requested in ((0, 1), (1, 0), (0, 1)):
        run("save", previous, requested)
        assert config.read_bytes() == f"{requested}\n".encode()
        run("get", requested)
        assert_isolated()

    # Deterministic failures exercise the real file helper, including failures
    # after a temporary file was written. Neither cached nor old disk state moves.
    for previous in (0, 1):
        config.write_bytes(f"{previous}\n".encode())
        for stage in ("mkdir", "open", "flush", "sync", "close", "rename"):
            before = snapshot(config)
            run("failure", previous, 1 - previous, stage)
            assert snapshot(config) == before, f"{stage} failure changed old config"
            run("get", previous)
            assert_isolated()

    print("PASS: strict/default/lazy load, restart persistence, unchanged-write skip, "
          "atomic-save failures, and password/other-config isolation")
