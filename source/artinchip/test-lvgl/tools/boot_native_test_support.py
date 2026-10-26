#!/usr/bin/env python3
"""Shared host build support for the original A and customer B Theme D tests.

Both entry points compile real production pages and LVGL. A needs the default
LVGL theme enabled to exercise the original arc knob style. Reuse one compatible
cache across scenarios; never combine objects from different lv_conf settings.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PAGES = [
    "un260/lv_core/page_00_boot_anim.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_theme_a.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_theme_c.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_theme_d.c",
    "un260/lv_core/boot_anim/page_00_boot_anim_customer.c",
    "un260/lv_core/page_08_boot.c",
    "un260/lv_core/boot_anim/page_08_boot_flow.c",
    "un260/lv_core/boot_anim/page_08_boot_customer.c",
]
PUBLIC = ["ui_page_00_boot_anim_create", "ui_page_00_boot_anim_destroy",
          "ui_page_00_boot_anim_is_active", "ui_page_08_curr_create",
          "ui_page_08_curr_destroy"]
D_PUBLIC = ["ui_page_00_boot_anim_poll", "ui_page_00_boot_anim_set_startup_ready",
            "ui_page_00_boot_anim_adopt_elapsed", "ui_page_00_boot_anim_set_startup_error",
            "ui_page_08_curr_defer_next_create", "ui_page_08_curr_prepare_step",
            "ui_page_08_curr_prepare_failed", "ui_page_08_curr_set_covered",
            "ui_page_08_curr_start_handoff", "ui_page_08_curr_visual_is_quiet",
            "ui_page_08_curr_ready_hold_complete", "boot_progress_set",
            "boot_progress_reset", "boot_selftest_list_reset",
            "boot_selftest_list_sync_step", "boot_selftest_list_set_result",
            "boot_selftest_list_finish"]


def run(command, **kwargs):
    return subprocess.run([str(value) for value in command], check=True, **kwargs)


def source_fonts(root, sources):
    names = set(re.findall(r"\blv_font_([A-Za-z0-9_]+)\b", "\n".join(
        path.read_text() for path in sources)))
    return sorted(root / "un260/font" / ("lv_font_" + name + ".c")
                  for name in names
                  if (root / "un260/font" / ("lv_font_" + name + ".c")).is_file())


def check_matrix(common, root, out):
    results = []
    cases = [("legacy-1", 1, 1), ("legacy-2", 2, 1), ("legacy-3", 3, 1),
             ("variant-a", 4, 1), ("variant-b", 4, 2), ("default-a", None, None)]
    for name, theme, variant in cases:
        defines = [] if theme is None else ["-DUI_BOOT_ANIM_THEME=" + str(theme),
                                            "-DUI_BOOT_VARIANT=" + str(variant)]
        objects = []
        for index, source in enumerate(PAGES):
            obj = out / (name + "-" + str(index) + ".o")
            run(common + defines + ["-c", root / source, "-o", obj])
            objects.append(obj)
        combined = out / (name + ".o")
        run([common[0], "-r", *objects, "-o", combined])
        symbols = subprocess.check_output(["nm", "--defined-only", str(combined)], text=True)
        for symbol in PUBLIC + (D_PUBLIC if theme in (4, None) else []):
            assert len(re.findall(r"\bT\s+" + symbol + r"$", symbols, re.M)) == 1, (name, symbol)
        # The customer object contains its shared art calls only for B. This
        # proves that default selection is A, not just that duplicate APIs vanish.
        customer = out / (name + "-4.o")
        dependencies = subprocess.check_output(["nm", "-u", str(customer)], text=True)
        assert ("boot_customer_art_acquire" in dependencies) == (theme == 4 and variant == 2), name
        results.append(name)
    invalid = out / "invalid-variant.c"
    invalid.write_text('#include "un260/lv_core/boot_anim/boot_theme_config.h"\n')
    for value in (0, 3):
        result = subprocess.run([str(v) for v in common + ["-DUI_BOOT_VARIANT=" + str(value),
                                "-c", invalid, "-o", out / "invalid.o"]],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        assert result.returncode != 0 and "UI_BOOT_VARIANT" in result.stderr
    results.append("invalid-variant-rejected")
    return results


def main(variant):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--production-root", type=Path, default=ROOT)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--blob", type=Path)
    parser.add_argument("--sanitize", choices=("none", "undefined", "address,undefined"),
                        default="address,undefined")
    parser.add_argument("--skip-theme-compile", action="store_true")
    parser.add_argument("--reduced-motion", action="store_true")
    args = parser.parse_args()
    root, lvgl, out = args.production_root.resolve(), args.lvgl_dir.resolve(), args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    (out / "temp").mkdir(exist_ok=True)
    os.environ["TMPDIR"] = str(out / "temp")
    (out / "lvgl").mkdir(exist_ok=True)
    (out / "lvgl/lvgl.h").write_text('#include "' + str(lvgl / "lvgl.h") + '"\n')
    cache = args.cache.resolve() if args.cache else out / "runtime-cache"
    cache.mkdir(parents=True, exist_ok=True)
    conf = cache / "lv_conf.h"
    if not conf.exists():
        conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (24U*1024U*1024U)
#define LV_IMG_CACHE_DEF_SIZE 4
#define LV_DISP_DEF_REFR_PERIOD 10
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_LOG 0
#endif
""")
    if variant == 1 and not re.search(r"#define\s+LV_USE_THEME_DEFAULT\s+1\b", conf.read_text()):
        raise SystemExit("Original A requires a LV_USE_THEME_DEFAULT=1 runtime cache")
    fonts = source_fonts(root, [root / name for name in PAGES] + [root / "un260/font/boot_fonts.h"])
    declarations = out / "font-declarations.h"
    declarations.write_text('#include "lvgl/lvgl.h"\n' + "\n".join(
        "LV_FONT_DECLARE(" + path.stem + ");" for path in fonts) + "\n")
    common = [os.environ.get("CC", "cc"), "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
              "-DLV_DRV_CONF_H", "-I" + str(out), "-I" + str(root), "-I" + str(root / "un260"),
              "-I" + str(lvgl), "-DLV_CONF_PATH=" + str(conf),
              '-DBOOT_WELCOME_LOCALE_PATH="/nonexistent-un260-test-locale"',
              "-include", str(declarations)]
    if args.sanitize != "none":
        common += ["-fsanitize=" + args.sanitize, "-fno-sanitize-recover=all", "-no-pie"]
    if args.reduced_motion:
        common.append("-DBOOT_WELCOME_MOTION_SCALE=0")
    matrix = [] if args.skip_theme_compile else check_matrix(common, root, out)
    objects = {}
    for dep in cache.glob("*.d"):
        tokens = dep.read_text().replace("\\\n", " ").split()
        if len(tokens) > 1:
            src, obj = Path(tokens[1]), dep.with_suffix(".o")
            if str(src).startswith(str(lvgl / "src") + "/") and obj.exists() and obj.stat().st_mtime >= conf.stat().st_mtime:
                objects[str(src)] = obj
    reused = len(objects)
    for index, src in enumerate(sorted((lvgl / "src").rglob("*.c"))):
        if str(src) not in objects:
            obj = cache / ("lvgl-" + str(index) + ".o")
            run(common + ["-MMD", "-MF", obj.with_suffix(".d"), "-c", src, "-o", obj])
            objects[str(src)] = obj
    is_customer = variant == 2
    harness_name = "test_boot_customer" if is_customer else "test_boot_theme_d"
    welcome = root / ("un260/lv_core/boot_anim/" + (
        "page_00_boot_anim_customer.c" if is_customer else "page_00_boot_anim_theme_d.c"))
    page = root / ("un260/lv_core/boot_anim/" + (
        "page_08_boot_customer.c" if is_customer else "page_08_boot_flow.c"))
    local = [root / "tools" / (harness_name + ".c"), welcome]
    if is_customer:
        local += [root / "un260/lv_core/boot_anim/boot_customer_art.c",
                  root / "un260/boot/boot_customer_catalog.c",
                  args.blob or root / "aic_ui/generated_assets/customer_boot/boot_customer_blob.c"]
    local += source_fonts(root, [local[0], welcome, page, root / "un260/font/boot_fonts.h"])
    linked = []
    for index, src in enumerate(local):
        obj = out / ("local-" + str(index) + ".o")
        run(common + ["-DUI_BOOT_ANIM_THEME=4", "-DUI_BOOT_VARIANT=" + str(variant), "-c", src, "-o", obj])
        linked.append(obj)
    wrappers = ["lv_timer_create", "lv_obj_create"]
    if is_customer:
        wrappers += ["malloc", "calloc", "realloc", "fopen", "boot_customer_art_acquire"]
    exe = out / harness_name
    run(common + linked + list(objects.values()) + ["-Wl,--wrap=" + name for name in wrappers]
        + ["-lm", "-lz", "-pthread", "-o", exe])
    frames = out / "frames"
    frames.mkdir(exist_ok=True)
    env = dict(os.environ)
    if is_customer:
        env["BOOT_CUSTOMER_RASTER_OUTPUT"] = str(frames)
    else:
        inputs = out / "inputs"
        inputs.mkdir(exist_ok=True)
        for name, relative in [("background", "boot_theme_d/background.png"),
                               ("emblem", "boot_theme_d/emblem.png"),
                               ("boot", "backgrounds/boot.png")]:
            with Image.open(root / "aic_ui/lvgl_data" / relative) as image:
                (inputs / (name + ".bgra")).write_bytes(image.convert("RGBA").tobytes("raw", "BGRA"))
        env["BOOT_THEME_C_PIXEL_INPUT"] = str(inputs)
        env["BOOT_THEME_C_RASTER_OUTPUT"] = str(frames)
    result = subprocess.run([str(exe)], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (out / "test.log").write_text(result.stdout)
    print(result.stdout, end="")
    result.check_returncode()
    for path in frames.glob("*.ppm"):
        with Image.open(path) as image:
            image.save(path.with_suffix(".png"))
    sources = local + [page, Path(__file__), root / "tools" / (harness_name + ".py"),
                       root / "un260/lv_core/boot_anim/boot_theme_config.h"]
    (out / "verification.json").write_text(json.dumps({
        "result": "passed", "variant": "B-customer" if is_customer else "A-original",
        "sanitize": args.sanitize, "runtime_config_sha256": hashlib.sha256(conf.read_bytes()).hexdigest(),
        "reused_lvgl_objects": reused, "theme_compile": matrix, "board_test": False,
        "sources": {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in sources if str(path).startswith(str(root) + "/")}}, indent=2))
    print("PASS native variant", "B" if is_customer else "A", "evidence:", out)
