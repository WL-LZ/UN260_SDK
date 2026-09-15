#!/usr/bin/env python3
"""Verify theme C using genuine LVGL pixels and compile-time A/B/C selection.

PNG inputs are decoded once by Pillow into a host-only decoder. The production
theme, timeline, object tree, LVGL renderer and timer cleanup are not mocked.
These checks do not measure the board's GE driver or physical display frame rate.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from PIL import Image

DEFAULT_ROOT = Path(__file__).resolve().parents[1]
THEMES = (
    "un260/lv_core/page_00_boot_anim.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_theme_a.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_theme_c.c",
)
PUBLIC_SYMBOLS = (
    "ui_page_00_boot_anim_create",
    "ui_page_00_boot_anim_destroy",
    "ui_page_00_boot_anim_is_active",
)


def run(command, **kwargs):
    subprocess.run([str(part) for part in command], check=True, **kwargs)


def write_assets(root, work):
    definitions = []
    metadata = {}
    for name in ("background", "emblem"):
        source = root / f"aic_ui/lvgl_data/boot_theme_c/{name}.png"
        if not source.is_file():
            raise SystemExit(f"Missing production PNG (no substitute fixture): {source}")
        with Image.open(source) as image:
            image = image.convert("RGBA")
            width, height = image.size
            if name == "background" and (width, height) != (1280, 400):
                raise AssertionError("Background must match the native 1280 x 400 canvas")
            (work / f"{name}.bgra").write_bytes(image.tobytes("raw", "BGRA"))
        definitions += [f"#define BOOT_TEST_{name.upper()}_WIDTH {width}U",
                        f"#define BOOT_TEST_{name.upper()}_HEIGHT {height}U"]
        metadata[name] = {"width": width, "height": height,
                          "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest()}
    (work / "boot_theme_c_assets.h").write_text("\n".join(definitions) + "\n")
    return metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--production-root", type=Path, default=DEFAULT_ROOT)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--sanitize", choices=("none", "undefined", "address,undefined"),
                        default="none" if os.name == "nt" else "undefined")
    parser.add_argument("--skip-theme-compile", action="store_true")
    parser.add_argument("--reduced-motion", action="store_true")
    args = parser.parse_args()
    root = args.production_root.resolve()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
    nm = shutil.which("nm")
    if not compiler or not (lvgl / "lvgl.h").is_file():
        raise SystemExit("Requires host GCC and SDK LVGL 8 source")
    theme_sources = [root / name for name in THEMES]
    for source in theme_sources:
        if not source.is_file():
            raise SystemExit(f"Missing production theme: {source}")
    fonts = sorted(set(re.findall(r"\blv_font_([a-zA-Z0-9_]+)\b",
        "\n".join(source.read_text(encoding="utf-8") for source in theme_sources))))
    fonts = [font for font in fonts if (root / f"un260/font/lv_font_{font}.c").is_file()]
    with tempfile.TemporaryDirectory(prefix="un260-theme-c-") as tmp:
        work = Path(tmp)
        metadata = write_assets(root, work)
        (work / "lvgl").mkdir()
        (work / "lvgl/lvgl.h").write_text(f'#include "{(lvgl / "lvgl.h").as_posix()}"\n')
        conf = work / "lv_conf.h"
        conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_COLOR_SCREEN_TRANSP 0
#define LV_DISP_DEF_REFR_PERIOD 10
#define LV_IMG_CACHE_DEF_SIZE 4
#define LV_MEM_SIZE (8U * 1024U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(
            f"LV_FONT_DECLARE(lv_font_{font});" for font in fonts) + "\n#endif\n")
        common = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                  "-DLV_DRV_CONF_H", f"-I{work}", f"-I{root}", f"-I{lvgl}",
                  f"-DLV_CONF_PATH={conf}"]
        if args.reduced_motion:
            common.append("-DBOOT_WELCOME_MOTION_SCALE=0")
        if not args.skip_theme_compile:
            if not nm:
                raise SystemExit("nm is required for theme-selection symbol checks")
            for theme in (1, 2, 3):
                objects = []
                for index, source in enumerate(theme_sources):
                    obj = work / f"theme-{theme}-{index}.o"
                    run([*common, f"-DUI_BOOT_ANIM_THEME={theme}", "-c", source, "-o", obj])
                    objects.append(obj)
                combined = work / f"theme-{theme}-combined.o"
                run([compiler, "-r", *objects, "-o", combined])
                symbols = subprocess.check_output([nm, "--defined-only", str(combined)], text=True)
                for name in PUBLIC_SYMBOLS:
                    assert len(re.findall(r"\bT\s+" + re.escape(name) + r"$", symbols, re.M)) == 1, (
                        f"Theme {theme}: duplicate/missing {name}")
                print(f"PASS theme {theme}: exactly one create/destroy/is_active implementation", flush=True)
        harness = Path(__file__).with_suffix(".c")
        sources = [harness, *[root / f"un260/font/lv_font_{font}.c" for font in fonts],
                   *sorted((lvgl / "src").rglob("*.c"))]
        executable = work / ("test-theme-c.exe" if os.name == "nt" else "test-theme-c")
        command = [*common, "-DUI_BOOT_ANIM_THEME=3", "-Wl,--wrap=lv_timer_create"]
        if args.sanitize != "none":
            command += [f"-fsanitize={args.sanitize}", "-fno-sanitize-recover=all"]
        response = work / "compile.rsp"
        response.write_text("\n".join('"' + str(arg).replace("\\", "/").replace('"', '\\"') + '"'
            for arg in [*command[1:], *sources, "-lm", "-o", executable]))
        run([compiler, f"@{response}"])
        environment = os.environ.copy()
        environment["BOOT_THEME_C_PIXEL_INPUT"] = str(work)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["BOOT_THEME_C_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        verification = subprocess.run([str(executable)], env=environment, timeout=90,
                                      capture_output=True, text=True)
        print(verification.stdout, end="", flush=True)
        if verification.stderr:
            print(verification.stderr, end="", flush=True)
        verification.check_returncode()
        if args.output_dir:
            (args.output_dir / "verification.log").write_text(
                verification.stdout + verification.stderr)
            for raster in args.output_dir.glob("*.bmp"):
                with Image.open(raster) as image:
                    image.convert("RGB").save(raster.with_suffix(".png"))
            (args.output_dir / "asset-verification.json").write_text(
                json.dumps(metadata, indent=2) + "\n")


if __name__ == "__main__":
    main()
