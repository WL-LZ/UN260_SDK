#!/usr/bin/env python3
"""Exercise the real Main detail projection, recycled lists and LVGL 8 renderer.

This uses a 4 MiB LVGL pool (24 MiB with locale fonts) and coordinate16,
not the board's DMA/touch devices.
The counting store and localization are genuine; only navigation is captured.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from test_i18n_support import with_i18n

ROOT = Path(__file__).resolve().parents[1]
FONTS = sorted(set([f"instrument_sans_medium_{size}" for size in (12, 14, 16, 18, 20)] +
    re.findall(r"\blv_font_((?:instrument_sans|message_cjk)_[A-Za-z0-9_]+)",
               (ROOT / "un260/font/ui_message_font.c").read_text())))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--locale-fonts", action="store_true",
                        help="Load the production locale font bank for all-language raster checks")
    parser.add_argument("--sanitize", default="none" if os.name == "nt" else "undefined",
                        choices=("none", "undefined", "address,undefined"))
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").is_file():
        raise SystemExit("Requires host GCC and full LVGL 8 source")
    with tempfile.TemporaryDirectory(prefix="un260-main-detail-") as tmp:
        work = Path(tmp)
        (work / "lvgl").mkdir()
        (work / "lvgl/lvgl.h").write_text(f'#include "{(lvgl / "lvgl.h").as_posix()}"\n')
        conf = work / "lv_conf.h"
        conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LVGL_DIR "L:/usr/local/share/lvgl_data/"
#define LV_MEM_SIZE (""" + ("24U" if args.locale_fonts else "4U") + """ * 1024U * 1024U)
#define LV_USE_FS_STDIO """ + ("1" if args.locale_fonts else "0") + """
#define LV_FS_STDIO_LETTER 83
#define LV_FS_STDIO_PATH ""
#define LV_USE_BIDI 1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_LOG 0
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE(lv_font_{font});" for font in FONTS) + "\n#endif\n")
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        match = re.search(r"void lv_port_indev_set_drag_obj\(.*?\n\}", port, re.S)
        if not match:
            raise AssertionError("Production drag-ownership registration missing")
        helper = work / "port_drag.c"
        helper.write_text('#include "lvgl/lvgl.h"\n' + match.group() + '\n')
        sources = [ROOT / "tools/test_main_detail_view.c", helper]
        sources += [ROOT / path for path in (
            "un260/lv_components/lv_recycled_list.c",
            "un260/lv_components/ui_scrollbar.c",
            "un260/lv_components/ui_list_window.c",
            "un260/lv_system/ui_update_batch.c",
            "un260/lv_system/ui_text_page.c",
            "un260/lv_system/ui_text_widget.c",
            "un260/lv_system/ui_lang.c",
            "un260/counting/counting_data_store.c",
            "un260/font/ui_message_font.c")]
        sources += [ROOT / f"un260/font/lv_font_{font}.c" for font in FONTS]
        if args.locale_fonts:
            sources.append(ROOT / "un260/font/ui_locale_font_bank.c")
        sources = with_i18n(sources, ROOT)
        sources += sorted((lvgl / "src").rglob("*.c"))
        executable = work / ("test-main-detail.exe" if os.name == "nt" else "test-main-detail")
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-DLV_DRV_CONF_H", "-D_POSIX_C_SOURCE=200809L", "-D_DEFAULT_SOURCE",
                   f'-DUI_STATE_DIR="{work}/state"',
                   "-Wl,--wrap=lv_mem_alloc", "-Wl,--wrap=lv_timer_create",
                   f"-I{work}", f"-I{ROOT}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}"]
        if args.sanitize != "none":
            command += [f"-fsanitize={args.sanitize}", "-fno-sanitize-recover=all"]
        if args.locale_fonts:
            command += ["-DMAIN_DETAIL_REAL_FONTS",
                        '-DUN260_LOCALE_FONT_DIR="S:' + str(ROOT / "aic_ui/lvgl_data/locale_fonts") + '/"']
        # A response file avoids Windows' 32K command-line limit with SDK sources.
        response = work / "compile.rsp"
        response.write_text("\n".join('"' + str(arg).replace("\\", "/").replace('"', '\\"') + '"'
                                       for arg in [*command[1:], *map(str, sources), "-lm",
                                                   *(["-lz"] if args.locale_fonts else []),
                                                   "-o", str(executable)]))
        subprocess.run([compiler, f"@{response}"], check=True)
        environment = os.environ.copy()
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["MAIN_DETAIL_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True, timeout=60)


if __name__ == "__main__":
    main()
