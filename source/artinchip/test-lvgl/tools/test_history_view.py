#!/usr/bin/env python3
"""Render and exercise the real History page/search with LVGL 8 and a 4 MiB pool.

The store and external actions are in-memory stubs. No device, real history,
USB mount or firmware write is used. This is not a board touch/GE validation.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_list_view import compiled_asset_sources

ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = [
    "un260/lv_components/lv_recycled_list.c",
    "un260/lv_components/ui_list_window.c",
    "un260/lv_components/lv_card_surface.c",
    "un260/lv_components/lv_damped_button.c",
    "un260/lv_components/lv_alnum_keyboard.c",
    "un260/lv_components/lv_nav_button.c",
    "un260/lv_core/page_19_history_search.c",
    "un260/lv_core/ui_frame_commit.c",
    "un260/lv_system/ui_update_batch.c",
    "un260/lv_system/ui_text_page.c",
    "un260/lv_system/ui_text_widget.c",
    "un260/lv_system/ui_lang.c",
    "un260/history/history_record_detail.c",
    "un260/history/history_query.c",
    "un260/history/history_export_sn_parser.c",
    "un260/counting/counting_reject_reason.c",
    "un260/counting/counting_serial_text.c",
    "un260/protocol/protocol_frame.c",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--asan", action="store_true", help="Also check host heap/lifetime with AddressSanitizer")
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").is_file():
        raise SystemExit("Requires host GCC and full LVGL 8 source")
    assets = compiled_asset_sources()
    inspected = [ROOT / "un260/lv_core/page_19_history.c", *(ROOT / p for p in COMPONENTS)]
    fonts = sorted(set(re.findall(r"lv_font_(instrument_sans_[a-z]+_\d+)",
                                 "\n".join(p.read_text(encoding="utf-8") for p in inspected))))
    with tempfile.TemporaryDirectory(prefix="un260-history-view-") as directory:
        work = Path(directory)
        (work / "lvgl").mkdir()
        (work / "lvgl/lvgl.h").write_text(f'#include "{(lvgl / "lvgl.h").as_posix()}"\n')
        conf = work / "lv_conf.h"
        conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (4U * 1024U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_SNAPSHOT 1
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE(lv_font_{f});" for f in fonts) + "\n#endif\n")
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        helper = re.search(r"void lv_port_indev_set_drag_obj\(.*?\n\}", port, re.S)
        if not helper:
            raise AssertionError("Production drag ownership registration is missing")
        port_source = work / "port_drag.c"
        port_source.write_text('#include "lvgl/lvgl.h"\n' + helper.group() + "\n")
        sources = [ROOT / "tools/test_history_view.c", ROOT / "tools/test_history_search_view.c",
                   port_source, *assets,
                   *(ROOT / p for p in COMPONENTS if p != "un260/lv_core/page_19_history_search.c"),
                   *(ROOT / f"un260/font/lv_font_{f}.c" for f in fonts),
                   *sorted((lvgl / "src").rglob("*.c"))]
        executable = work / "test-history-view"
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-DLV_DRV_CONF_H", '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',
                   "-fsanitize=address,undefined" if args.asan else "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   f"-I{work}", f"-I{ROOT}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}",
                   *map(str, sources), "-lm", "-o", str(executable)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["HISTORY_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True, timeout=90)


if __name__ == "__main__":
    main()
