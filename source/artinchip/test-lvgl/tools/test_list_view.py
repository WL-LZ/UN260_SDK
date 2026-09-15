#!/usr/bin/env python3
"""Run the actual List, viewport, buttons and LVGL 8 software renderer.

Requires --lvgl-dir pointing to the SDK's LVGL 8 source. No board or firmware
state is used. Raster output is optional. This does not validate GE/touch HW.
"""
import argparse
import hashlib
import json
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
         "instrument_sans_medium_22",
         "instrument_sans_medium_32", "instrument_sans_semibold_12",
         "instrument_sans_semibold_14", "instrument_sans_semibold_20",
         "instrument_sans_semibold_22", "instrument_sans_bold_14"]

LIST_IMAGES = {
    "list_icons/receipt_24.png": 24,
    "list_icons/barcode_24.png": 24,
    "list_icons/warning_circle_24.png": 24,
    "list_icons/barcode_36.png": 36,
    "list_icons/warning_circle_36.png": 36,
}

def compiled_asset_sources():
    """Require the production registry and its actual, up-to-date C/PNG inputs."""
    generated = ROOT / "aic_ui/generated_assets"
    manifest_path = generated / "manifest.json"
    if not manifest_path.is_file():
        raise SystemExit(f"Missing production asset manifest: {manifest_path}")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest["entries"]
    by_name = {entry["name"]: entry for entry in entries}
    if len(by_name) != len(entries):
        raise SystemExit("Duplicate production asset names in manifest")
    sources = []
    for entry in entries:
        path = (generated / entry["c_file"]).resolve()
        if generated.resolve() not in path.parents or not path.is_file():
            raise SystemExit(f"Missing or invalid compiled asset source: {path}")
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["c_sha256"]:
            raise SystemExit(f"Compiled asset differs from manifest: {path}")
        sources.append(path)
    for name, side in LIST_IMAGES.items():
        entry = by_name.get(name)
        if not entry:
            raise SystemExit(f"List image is not in the compiled asset registry: {name}")
        path = ROOT / "aic_ui/lvgl_data" / name
        if not path.is_file():
            raise SystemExit(f"Missing actual List PNG source (no mock fallback): {path}")
        raw = path.read_bytes()
        if (raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR" or
                int.from_bytes(raw[16:20], "big") != side or
                int.from_bytes(raw[20:24], "big") != side or
                hashlib.sha256(raw).hexdigest() != entry["source_sha256"]):
            raise SystemExit(f"List PNG dimensions/hash differ from manifest: {path}")
        if (entry["width"] != side or entry["height"] != side or
                entry["stride"] != side * 4 or entry["bytes"] != side * side * 4 or
                entry["alpha"] is not True or entry["format"] != "ARGB8888_LE_BGRA"):
            raise SystemExit(f"List icon must be straight-alpha BGRA at native size: {name}")
    registry = generated / "asset_registry.c"
    if not registry.is_file():
        raise SystemExit(f"Missing production asset registry: {registry}")
    return sources + [registry]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").exists():
        raise SystemExit("Requires host GCC and full LVGL source")
    asset_sources = compiled_asset_sources()
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
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE(lv_font_{font});" for font in FONTS) + "\n#endif\n")
        # Compile the actual tiny pointer-ownership registration without the
        # evdev device loop; the rest of the driver needs board devices.
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        match = re.search(r"void lv_port_indev_set_drag_obj\(.*?\n\}", port, re.S)
        if not match:
            raise AssertionError("Production drag-ownership registration missing")
        port_helper = work / "port_drag.c"
        port_helper.write_text('#include "lvgl/lvgl.h"\n' + match.group() + '\n')
        sources = [ROOT / "tools/test_list_view.c", port_helper, *asset_sources]
        for path in ["un260/lv_components/lv_recycled_list.c",
                     "un260/lv_components/ui_list_window.c",
                     "un260/lv_components/lv_card_surface.c",
                     "un260/lv_components/lv_damped_button.c",
                     "un260/lv_components/lv_alnum_keyboard.c",
                     "un260/lv_components/lv_nav_button.c",
                     "un260/lv_core/page_02_list_data.c",
                     "un260/lv_core/ui_frame_commit.c",
                     "un260/lv_system/ui_update_batch.c",
                     "un260/lv_system/ui_text_page.c",
                     "un260/lv_system/ui_text_widget.c",
                     "un260/lv_system/ui_lang.c",
                     "un260/counting/counting_data_store.c",
                     "un260/currency/currency_state.c",
                     "un260/counting/counting_serial_query.c",
                     "un260/counting/counting_serial_text.c"]:
            sources.append(ROOT / path)
        sources += [ROOT / f"un260/font/lv_font_{f}.c" for f in FONTS]
        sources += sorted((lvgl / "src").rglob("*.c"))
        executable = work / "test-list"
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-DLV_DRV_CONF_H",  # Hardware driver config is not a host raster configuration.
                   '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',
                   "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   "-Wl,--wrap=lv_timer_create",  # Test-only one-shot motion timer allocation failure.
                   "-Wl,--wrap=lv_mem_alloc",  # Test-only one-shot search owner allocation failure.
                   f"-I{work}", f"-I{ROOT}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}",
                   *map(str, sources), "-lm", "-o", str(executable)]
        subprocess.run(command, check=True)
        from PIL import Image
        for name in ('user', 'settings'):
            with Image.open(ROOT / 'aic_ui/lvgl_data/backgrounds' / (name+'.png')) as image:
                (work / (name+'.bgra')).write_bytes(image.convert('RGBA').tobytes('raw','BGRA'))
        environment = os.environ.copy()
        environment['UN260_BACKGROUND_DIR'] = str(work)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["LIST_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True, timeout=60)

if __name__ == "__main__":
    main()
