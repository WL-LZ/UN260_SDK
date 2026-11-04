#!/usr/bin/env python3
"""Compile the production mode service; also test real POSIX storage on Linux."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
compiler = os.environ.get("CC", "gcc")
with tempfile.TemporaryDirectory(prefix="un260-work-mode-") as temp:
    directory = Path(temp)
    event_source = (root / "un260/lv_core/lv_page_event.c").read_text()
    start = event_source.index("    case PAGE_01_ACTION_WORK:")
    end = event_source.index("    case PAGE_01_ACTION_SORT:", start)
    (directory / "work_mode_main_action.inc").write_text(event_source[start:end])
    common = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root), "-I", str(directory)]
    sanitizer = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"] if os.name != "nt" else []
    executable = directory / ("service.exe" if os.name == "nt" else "service")
    subprocess.run(common + sanitizer + [str(root / "tools/test_work_mode_service.c"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
    if os.name != "nt":
        store = directory / "store"
        subprocess.run(common + sanitizer + [
            f'-DWORK_MODE_STORE_DIRECTORY="{directory}/config"',
            str(root / "tools/test_work_mode_store.c"),
            str(root / "un260/storage/storage_worker.c"), "-pthread", "-o", str(store)
        ], check=True)
        subprocess.run([str(store)], check=True)
    else:
        print("POSIX atomic storage suite requires Linux; service replay passed on Windows.")
