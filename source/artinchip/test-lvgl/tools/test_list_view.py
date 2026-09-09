#!/usr/bin/env python3
"""Run the actual List, viewport, buttons and LVGL 8 software renderer.

Requires --lvgl-dir pointing to the SDK's LVGL 8 source. No board or firmware
state is used. Raster output is optional. This does not validate GE/touch HW.
"""
import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FONTS = ["instrument_sans_medium_10", "instrument_sans_medium_12",
         "instrument_sans_medium_14", "instrument_sans_medium_16",
         "instrument_sans_medium_18", "instrument_sans_medium_20",
         "instrument_sans_medium_32", "instrument_sans_semibold_12",
         "instrument_sans_semibold_14", "instrument_sans_semibold_20",
         "instrument_sans_semibold_22"]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").exists():
        raise SystemExit("Requires host GCC and full LVGL source")
    with tempfile.TemporaryDirectory(prefix="un260-list-view-") as tmp:
        work = Path(tmp)
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
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE(lv_font_{font});" for font in FONTS) + "\n#endif\n")
        # Compile the actual tiny pointer-ownership registration without the
        # evdev device loop; the rest of the driver needs board devices.
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        match = re.search(r"void lv_port_indev_set_drag_obj\(.*?\n\}", port, re.S)
        if not match:
            raise AssertionError("Production drag-ownership registration missing")
        port_helper = work / "port_drag.c"
        port_helper.write_text('#include "lvgl/lvgl.h"\n' + match.group() + '\n')
        sources = [ROOT / "tools/test_list_view.c", port_helper]
        for path in ["un260/lv_components/lv_recycled_list.c",
                     "un260/lv_components/ui_list_window.c",
                     "un260/lv_components/lv_card_surface.c",
                     "un260/lv_components/lv_damped_button.c",
                     "un260/lv_core/page_02_list_data.c",
                     "un260/lv_core/ui_frame_commit.c",
                     "un260/lv_system/ui_update_batch.c",
                     "un260/lv_system/ui_text_page.c",
                     "un260/lv_system/ui_text_widget.c",
                     "un260/lv_system/ui_lang.c",
                     "un260/counting/counting_data_store.c"]:
            sources.append(ROOT / path)
        sources += [ROOT / f"un260/font/lv_font_{f}.c" for f in FONTS]
        sources += sorted((lvgl / "src").rglob("*.c"))
        executable = work / "test-list"
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-DLV_DRV_CONF_H",  # Hardware driver config is not a host raster configuration.
                   "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   "-Wl,--wrap=lv_timer_create",  # Test-only motion timer allocation failure.
                   f"-I{work}", f"-I{ROOT}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}",
                   *map(str, sources), "-lm", "-o", str(executable)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["LIST_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True, timeout=60)

if __name__ == "__main__":
    main()
