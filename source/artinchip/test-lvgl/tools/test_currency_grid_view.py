"""Render the production Currency VIEW with real LVGL; not a board/protocol test."""
from argparse import ArgumentParser
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import os
import re
import subprocess
import sys

from PIL import Image
from test_currency_modes import function
from test_i18n_support import with_i18n, lvgl_source


def write_changed(path, content):
    if not path.exists() or path.read_bytes() != content:
        path.write_bytes(content)


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cache-dir", type=Path,
                        help="Reuse one native dependency build across View scenarios")
    parser.add_argument("--prepare-only", action="store_true",
                        help="Compile LVGL, fonts and shared dependencies without the page harness")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    lvgl = lvgl_source(root)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = (args.cache_dir or out / "native-cache").resolve()
    work.mkdir(parents=True, exist_ok=True)
    (work / "lvgl").mkdir(exist_ok=True)
    (work / "state").mkdir(exist_ok=True)
    write_changed(work / "lvgl/lvgl.h", f'#include "{lvgl}/lvgl.h"\n'.encode())
    write_changed(work / "lv_drv_conf.h", b'/* Host rendering only. */\n')
    subprocess.run([sys.executable, str(root / "tools/gen_currency_view_atlas.py"), "--check"], check=True)

    font_parts = ["un260/font/ui_message_font.c", "un260/font/ui_locale_font_bank.c"]
    font_parts += [f"un260/font/lv_font_message_cjk_{n}.c" for n in [12, 14, 16, 18, 20, 22, 24, 28]]
    fonts = sorted(set(re.findall(r"\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+",
                                 "".join((root / p).read_text() for p in font_parts))))
    conf = work / "lv_conf.h"
    write_changed(conf, ("#ifndef LV_CONF_H\n#define LV_CONF_H\n"
                        "#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (16U*1024U*1024U)\n"
                        "#define LV_IMG_CACHE_DEF_SIZE 48\n#define LV_USE_THEME_DEFAULT 0\n"
                        "#define LV_USE_BIDI 1\n#define LV_USE_ARABIC_PERSIAN_CHARS 1\n"
                        "#define LV_USE_FS_STDIO 1\n#define LV_FS_STDIO_LETTER 83\n"
                        '#define LV_FS_STDIO_PATH ""\n#define LV_FONT_CUSTOM_DECLARE ' +
                        " ".join(f"LV_FONT_DECLARE({s});" for s in fonts) + "\n#endif\n").encode())

    shared = [root / "un260/gesture/touch_sound.c",
              root / "un260/lv_components/lv_damped_button.c",
              root / "un260/lv_components/lv_nav_button.c",
              root / "un260/lv_components/ui_scrollbar.c",
              *[root / "un260/font" / f"{n}.c" for n in fonts],
              *[root / p for p in font_parts], *sorted(lvgl.joinpath("src").rglob("*.c"))]
    shared = with_i18n(shared, root)
    page_sources = [root / "tools/test_currency_grid_view.c",
                    root / "un260/lv_core/page_07_curr/page_07_curr_view.c",
                    root / "un260/lv_core/page_07_curr/page_07_curr_overview.c"]
    if not args.prepare_only:
        page = (root / "un260/lv_core/page_07_curr.c").read_text()
        port = (root / "lv_port_indev.c").read_text()
        image = (root / "un260/lv_resources/lv_img_init.c").read_text()
        currency = (root / "un260/currency/currency_state.c").read_text()
        parts = [function(currency, "currency_state_display_code"), function(image, "get_currency_img"),
                 function(port, "evdev_feedback"), function(port, "lv_port_indev_set_drag_obj")]
        parts += [function(page, name) for name in ["curr_fav_press_feedback_cb", "curr_build_grid_layer",
                                                   "curr_apply_grid_selected_style", "curr_set_mode_visible", "curr_style_view_button", "curr_update_grid_fav_ui", "curr_switch_cached_view"]]
        button_start = page.index("    g_page07_curr.objects.btn_view = lv_btn_create(")
        button_end = page.index("    g_page07_curr.objects.btn_favorite = lv_btn_create(", button_start)
        parts.append("static void host_create_card_view_button(void) {\n" + page[button_start:button_end] + "}\n")
        write_changed(work / "grid_under_test.h", "\n".join(parts).encode())
        entries = []
        assets = [*sorted((root / "aic_ui/lvgl_data").glob("CURR_*.png")),
                  root / "aic_ui/lvgl_data/main_icons/multi_card.png",
                  root / "aic_ui/lvgl_data/currency_view/flags.png",
                  root / "aic_ui/lvgl_data/main_icons/currencies_32.png"]
        for path in assets:
            with Image.open(path) as bitmap:
                bitmap = bitmap.convert("RGBA")
                target = work / (path.stem + ".bgra")
                write_changed(target, bitmap.tobytes("raw", "BGRA"))
                entries.append(f'{{"{path.name}","{target}",{bitmap.width},{bitmap.height}}}')
        write_changed(work / "grid_assets.h", (
            "typedef struct {const char *name,*path;unsigned w,h;} grid_asset_t;\n"
            "static grid_asset_t grid_assets[]={" + ",".join(entries) + "};\n").encode())

    common = [os.environ.get("CC", "gcc"), "-std=gnu11", "-O1", "-g", "-Wall", "-Wextra",
              "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie", "-DLV_DRV_CONF_H",
              f'-DUI_STATE_DIR="{work / "state"}"',
              f'-DUN260_LOCALE_FONT_DIR="S:{root / "aic_ui/lvgl_data/locale_fonts"}/"',
              f"-I{work}", f"-I{root}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}"]

    def compile_source(source):
        flags = common + (['-DLVGL_DIR="L:/usr/local/share/lvgl_data/"'] if source.name == "lv_nav_button.c" else [])
        key = hashlib.sha256((str(source) + repr(flags)).encode()).hexdigest()
        obj, dep = work / (key + ".o"), work / (key + ".d")
        dependencies = [source]
        if dep.exists():
            dependencies += [Path(p) for p in dep.read_text().replace("\\\n", " ").split(":", 1)[1].split()]
        changed = not obj.exists() or not dep.exists() or any(
            not p.exists() or p.stat().st_mtime_ns > obj.stat().st_mtime_ns for p in dependencies)
        if changed:
            subprocess.run([*flags, "-MMD", "-MF", str(dep), "-c", str(source), "-o", str(obj)], check=True)
        return obj, changed

    sources = shared if args.prepare_only else [*page_sources, *shared]
    with ThreadPoolExecutor(max_workers=4) as pool:
        compiled = list(pool.map(compile_source, sources))
    print(f"Currency VIEW native cache: {sum(changed for _, changed in compiled)} compiled, "
          f"{sum(not changed for _, changed in compiled)} reused", flush=True)
    if args.prepare_only:
        return
    exe = work / "currency-view"
    subprocess.run([*common, *[str(obj) for obj, _ in compiled], "-lm", "-lz", "-o", str(exe)], check=True)
    result = subprocess.run([str(exe)], env={**os.environ, "OUT": str(out)}, timeout=120)
    for path in out.glob("*.bgra"):
        Image.frombytes("RGBA", (1280, 400), path.read_bytes(), "raw", "BGRA").convert("RGB").save(path.with_suffix(".png"))
    result.check_returncode()


if __name__ == "__main__":
    main()