#!/usr/bin/env python3
"""Render actual Main/detail/Smart Island with real production compiled images.

Peripheral actions are captured, DMA skin uses its supported software fallback,
and Innovation's real handle is extracted without its hardware workflow.
This host test does not validate the board or controller protocol.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from PIL import Image
from test_list_view import compiled_asset_sources

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\([^;{}]*\)\s*\{.*?^\}", source, re.M | re.S)
    if not match:
        raise AssertionError(f"Missing actual production helper {name}")
    return match.group()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--sanitize", default="undefined", choices=("none", "undefined", "address,undefined"))
    parser.add_argument("--harness", type=Path, help="Optional focused harness using the same real LVGL build")
    parser.add_argument("--extra-source", action="append", default=[], type=Path)
    parser.add_argument("--wrap", action="append", default=[], help="Linker-wrapped function for fault injection")
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").exists():
        raise SystemExit("Requires host GCC and LVGL 8 source")
    sources = [args.harness.resolve() if args.harness else ROOT / "tools/test_main_view.c",
               *[source.resolve() for source in args.extra_source], *compiled_asset_sources()]
    actual = [
        "un260/lv_components/lv_recycled_list.c", "un260/lv_components/ui_list_window.c",
        "un260/lv_components/lv_damped_button.c", "un260/lv_components/lv_loading_orbit.c",
        "un260/lv_components/lv_capsule_pagination.c", "un260/lv_components/smart_island.c",
        "un260/lv_components/smart_island/smart_island_view.c",
        "un260/lv_components/smart_island/smart_island_action.c",
        "un260/lv_components/smart_island/smart_island_warning.c",
        "un260/lv_components/smart_island/smart_island_result.c",
        "un260/lv_core/ui_frame_commit.c", "un260/lv_system/ui_update_batch.c",
        "un260/lv_system/ui_text_page.c", "un260/lv_system/ui_text_widget.c",
        "un260/lv_system/ui_lang.c", "un260/counting/counting_data_store.c",
        "un260/machine_state/machine_state.c", "un260/currency/currency_state.c",
        "un260/currency/currency_metadata.c", "un260/lv_system/app_clock.c"]
    sources += [ROOT / path for path in actual]
    font_inputs = sources + [ROOT / "un260/lv_core/page_01_main.c",
                             ROOT / "un260/lv_core/page_01_main_detail.c"]
    fonts = {"lv_font_instrument_sans_bold_10"}
    for source in font_inputs:
        fonts.update(re.findall(r"\blv_font_(?:instrument_sans|manrope|main)_[a-zA-Z0-9_]+", source.read_text(encoding="utf-8")))
    # Page-specific declarations can provide a source without repeating its name in Main.
    for name in sorted(fonts):
        path = ROOT / "un260/font" / f"{name}.c"
        if not path.is_file():
            raise AssertionError(f"Actual font source missing: {path}")
        sources.append(path)
    with tempfile.TemporaryDirectory(prefix="un260-main-view-") as tmp:
        work = Path(tmp)
        # Firmware keeps large PNGs outside the binary and decodes into the DMA
        # pool, not LVGL's 4 MiB object heap. Decode those same pixels on the host
        # into a separate malloc-backed cache; never substitute a mock picture.
        manifest = json.loads((ROOT / "aic_ui/generated_assets/manifest.json").read_text())
        external_entries = []
        for entry in manifest["external"]:
            name = entry["name"]
            if name not in ("page_02_menu_bg.png", "backgrounds/user.png") and not name.startswith("CURR_"):
                continue
            path = ROOT / "aic_ui/lvgl_data" / name
            if hashlib.sha256(path.read_bytes()).hexdigest() != entry["source_sha256"]:
                raise AssertionError(f"External PNG differs from manifest: {path}")
            with Image.open(path) as bitmap:
                width, height = bitmap.size
                pixels = bitmap.convert("RGBA").tobytes("raw", "BGRA")
            binary = work / f"external-{len(external_entries)}.bgra"
            binary.write_bytes(pixels)
            external_entries.append('{ {' + f'{json.dumps(name)},{width},{height},{width*4},1,NULL' +
                                    '}, ' + json.dumps(binary.as_posix()) + ' }')
        external = work / "actual_external_images.c"
        external.write_text('''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aic_ui/compiled_asset.h"
static struct { un260_compiled_asset_t asset; const char *file; } images[]={
''' + ',\n'.join(external_entries) + '''
};
const un260_compiled_asset_t *host_external_asset_find(const char *path) {
    const char *prefix="L:/usr/local/share/lvgl_data/";
    if(strncmp(path,prefix,strlen(prefix)))return NULL;
    path+=strlen(prefix);
    for(unsigned i=0;i<sizeof(images)/sizeof(images[0]);++i) {
        un260_compiled_asset_t *asset=&images[i].asset;
        if(strcmp(asset->name,path))continue;
        if(!asset->pixels) {
            FILE *file=fopen(images[i].file,"rb");assert(file);
            size_t size=asset->stride*asset->height;
            unsigned char *pixels=malloc(size);assert(pixels);
            assert(fread(pixels,1,size,file)==size);fclose(file);asset->pixels=pixels;
        }
        return asset;
    }
    return NULL;
}
void host_external_assets_release(void) {
    for(unsigned i=0;i<sizeof(images)/sizeof(images[0]);++i) {
        free((void *)images[i].asset.pixels);images[i].asset.pixels=NULL;
    }
}
''')
        sources.append(external)
        (work / "lvgl/src/misc").mkdir(parents=True)
        (work / "lvgl/lvgl.h").write_text(f'#include "{(lvgl / "lvgl.h").as_posix()}"\n')
        (work / "lvgl/src/misc/lv_txt.h").write_text(f'#include "{(lvgl / "src/misc/lv_txt.h").as_posix()}"\n')
        conf = work / "lv_conf.h"
        conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (4U * 1024U * 1024U)
#define LV_USE_LOG 0
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE({font});" for font in sorted(fonts)) + "\n#endif\n")
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        platform = (ROOT / "un260/lv_system/platform_app.c").read_text(encoding="utf-8")
        images = (ROOT / "un260/lv_resources/lv_img_init.c").read_text(encoding="utf-8")
        helper = work / "actual_helpers.c"
        helper.write_text('#include <stdio.h>\n#include <stdarg.h>\n#include <string.h>\n#include "lvgl/lvgl.h"\n'
                          '#include "un260/lv_resources/lv_img_init.h"\n'
                          '#include "un260/currency/currency_state.h"\n' +
                          function(port, "lv_port_indev_set_drag_obj") + '\n' +
                          function(platform, "find_obj_by_name") + '\n' +
                          function(platform, "label_set_text_if_changed") + '\n' +
                          function(platform, "update_label_by_name") + '\n' +
                          function(platform, "format_amount_with_comma") + '\n' +
                          function(images, "get_currency_img") + '\n')
        sources.append(helper)
        innovation = (ROOT / "un260/innovation/page_32_innovation.c").read_text(encoding="utf-8")
        gesture_type = re.search(r"typedef struct \{[^}]*\} innovation_handle_gesture_t;", innovation)
        if not gesture_type: raise AssertionError("Actual Innovation gesture state missing")
        arm = re.search(r"^#define INNOVATION_PREVIEW_ARM_DY\s+\d+", innovation, re.M)
        if not arm: raise AssertionError("Actual Innovation gesture threshold missing")
        handle = work / "actual_handle.c"
        handle.write_text('''#include "lvgl/lvgl.h"
#include "lv_port_indev.h"
#include <string.h>
#define INNOVATION_BLUE 0x3578F6
''' + arm.group() + '''
static lv_obj_t *g_handle_touch;
static lv_timer_t *g_preview_preload_timer;
static void (*g_handle_tap_handler)(const lv_point_t *point);
static bool g_page_transitioning;
''' + gesture_type.group() + '''
static innovation_handle_gesture_t g_handle_gesture;
/* Keep actual stationary tap routing. The separate Innovation regression
 * suite, not this Main raster harness, owns preview/navigation validation. */
static void innovation_handle_drag_cancel(void) { g_handle_gesture.pressed=false; }
static bool innovation_handle_preview_begin(void) { return false; }
static void innovation_handle_drag_update(lv_indev_t *indev) { (void)indev; }
static void innovation_handle_drag_finish(lv_indev_t *indev) { (void)indev;g_handle_gesture.pressed=false; }
static void innovation_preview_preload_timer_cb(lv_timer_t *timer) { lv_timer_pause(timer); }
void page_32_innovation_handle_detach(void) {
    if(g_preview_preload_timer) lv_timer_del(g_preview_preload_timer);
    g_preview_preload_timer=NULL;
    if(g_handle_touch) lv_obj_del(g_handle_touch);
    g_handle_touch=NULL;
    g_handle_tap_handler=NULL;
    memset(&g_handle_gesture,0,sizeof(g_handle_gesture));
}
''' + function(innovation, "innovation_handle_event_cb") + '\n' +
                          function(innovation, "page_32_innovation_handle_attach") + '\n' +
                          function(innovation, "page_32_innovation_handle_set_tap_handler") + '\n')
        sources.append(handle)
        sources += sorted((lvgl / "src").rglob("*.c"))
        executable = work / "test-main-view"
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-DLV_DRV_CONF_H", "-D_POSIX_C_SOURCE=200809L",
                   '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"', f"-I{work}", f"-I{ROOT}",
                   f"-I{ROOT / 'aic_ui'}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}"]
        if args.sanitize != "none": command += [f"-fsanitize={args.sanitize}", "-fno-sanitize-recover=all"]
        objects = [work / f"source-{index}.o" for index in range(len(sources))]
        def compile_source(item):
            source, output = item
            subprocess.run([*command, "-c", str(source), "-o", str(output)], check=True)
        with ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(compile_source, zip(sources, objects)))
        subprocess.run([*command, *map(str, objects),
                        *[f"-Wl,--wrap={symbol}" for symbol in args.wrap],
                        "-lm", "-pthread", "-o", str(executable)], check=True)
        environment = os.environ.copy()
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["MAIN_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        subprocess.run([str(executable)], env=environment, check=True, timeout=60)


if __name__ == "__main__":
    main()
