#!/usr/bin/env python3
"""Render and exercise the production Menu with LVGL 8.3 software drawing.

Use an isolated full source copy, never a board deployment. DMA cache allocation
is deliberately unavailable so the production live-object fallback is rendered.
Real PNG/font files are used. Settings transport, navigation and the Main-page
refresh boundary are host stubs; this does not validate UART ACK or touch HW.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^\s*(?:static\s+)?[\w* ]+\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(f"Production function missing: {name}")
    end = source.index("{", match.start()) + 1
    depth = 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--render-only", action="store_true", help="Capture baseline without candidate assertions")
    args = parser.parse_args()
    root, lvgl = args.source_dir.resolve(), args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").is_file():
        raise SystemExit("Requires host GCC and full LVGL 8.3 source")
    paths = ["un260/lv_core/lv_page_event.c", "un260/lv_resources/lv_img_init.c",
             "un260/lv_components/lv_components.c", "un260/lv_components/lv_damped_button.c",
             "un260/lv_components/lv_nav_button.c", "un260/lv_components/lv_card_surface.c",
             "un260/lv_core/ui_frame_commit.c",
             "un260/machine_state/machine_state.c", "un260/currency/currency_state.c",
             "un260/lv_system/ui_text_page.c", "un260/lv_system/ui_text_widget.c",
             "un260/lv_system/ui_lang.c", "un260/lv_system/lv_str.c"]
    sources = [root / path for path in paths]
    menu = root / "un260/lv_core/page_03_menu.c"
    fonts = sorted(set(re.findall(r"\blv_font_\w+", "\n".join(p.read_text(encoding="utf-8") for p in [menu, *sources]))))
    custom = [font for font in fonts if (root / f"un260/font/{font}.c").is_file()]
    with tempfile.TemporaryDirectory(prefix="un260-menu-view-") as temporary:
        work = Path(temporary)
        # Preserve LVGL's public include structure, including source headers.
        (work / "lvgl").symlink_to(lvgl, target_is_directory=True)
        conf = work / "lv_conf.h"
        conf.write_text("\n".join([
            "#ifndef LV_CONF_H", "#define LV_CONF_H", "#define LV_COLOR_DEPTH 32",
            "#define LV_MEM_SIZE (16U * 1024U * 1024U)", "#define LV_USE_LOG 0",
            "#define LV_USE_GPU_AIC 0", "#define LV_USE_GPU_AIC_GE 0",
            "#define LV_USE_THEME_DEFAULT 1", "#define LV_USE_THEME_BASIC 0",
            "#define LV_USE_THEME_MONO 0", "#define LV_USE_PNG 1", "#define LV_USE_SNAPSHOT 1",
            *[f"#define {font.upper()} 1" for font in fonts if font.startswith("lv_font_montserrat_")],
            "#define LV_FONT_CUSTOM_DECLARE " + " ".join(f"LV_FONT_DECLARE({f});" for f in custom),
            "#endif", ""]), encoding="utf-8")
        # Compile these existing generic utilities unchanged, without the rest
        # of platform_app's board/protocol loop and unrelated page projections.
        platform = (root / "un260/lv_system/platform_app.c").read_text(encoding="utf-8")
        helper = work / "object_utils.c"
        helper.write_text('#include <stdarg.h>\n#include <string.h>\n#include "un260/lv_system/ui_object_utils.h"\n' +
                          "\n".join(function(platform, name) for name in
                                    ["find_obj_by_name", "label_set_text_if_changed", "update_label_by_name"]), encoding="utf-8")
        context_api = "void ui_page_03_menu_refresh_data(uint32_t topics)" in menu.read_text(encoding="utf-8")
        manager_helper = []
        if context_api:
            manager = (root / "un260/lv_core/lv_page_manager.c").read_text(encoding="utf-8")
            entry = re.search(r"\[UI_PAGE_MENU\]\s*=\s*\{(.*?)\n\s*\},", manager, re.S).group(1)
            assert ".refresh_data = ui_page_03_menu_refresh_data" in entry
            topics = re.search(r"\.data_topics\s*=\s*([^,]+),", entry).group(1)
            bridge = work / "manager_data.c"
            bridge.write_text('''#include <assert.h>
#include "un260/lv_core/lv_page_manager.h"
#include "un260/lv_core/page_03_menu.h"
#include "un260/lv_core/ui_frame_commit.h"
/* Actual manager publish/commit functions below; only the unrelated page
 * registry and navigation environment are reduced for this host boundary. */
typedef struct { ui_data_topic_t data_topics; void (*refresh_data)(uint32_t); } ui_page_registration_t;
static struct { ui_page_t current; } g_page_manager = {UI_PAGE_MENU};
static bool g_page_cache_ready[UI_PAGE_COUNT] = {[UI_PAGE_MENU] = true};
static ui_data_topic_t g_page_data_dirty[UI_PAGE_COUNT];
static unsigned host_calls;
static uint32_t host_topics;
static void host_refresh(uint32_t topics) { ++host_calls; host_topics |= topics; ui_page_03_menu_refresh_data(topics); }
static const ui_page_registration_t g_page_registry[UI_PAGE_COUNT] = {
    [UI_PAGE_MENU] = {.data_topics = ''' + topics + ''', .refresh_data = host_refresh}
};
''' + "\n".join(function(manager, name) for name in ["ui_manager_commit_visible_data", "ui_manager_publish_data_changed"]) + '''
void menu_host_manager_select(bool visible) { g_page_manager.current = visible ? UI_PAGE_MENU : UI_PAGE_MAIN; }
void menu_host_manager_reset_observation(void) { host_calls = 0; host_topics = 0; }
unsigned menu_host_manager_calls(void) { return host_calls; }
uint32_t menu_host_manager_topics(void) { return host_topics; }
uint32_t menu_host_manager_dirty(void) { return g_page_data_dirty[UI_PAGE_MENU]; }
void menu_host_manager_commit(void) { ui_manager_commit_visible_data(NULL, 0); }
''', encoding="utf-8")
            manager_helper.append(str(bridge))
        executable = work / "test-menu"
        command = [compiler, "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                   "-fsanitize=undefined", "-fno-sanitize-recover=all", "-DLV_DRV_CONF_H",
                   '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"', "-include", "stdio.h",
                   f"-I{work}", f"-I{root}", f"-I{root / 'aic_ui'}", f"-I{lvgl}",
                   f"-DLV_CONF_PATH={conf}", str(Path(__file__).with_suffix(".c")), str(helper),
                   *manager_helper, *map(str, sources), *[str(root / f"un260/font/{font}.c") for font in custom],
                   *[str(path) for path in sorted((lvgl / "src").rglob("*.c")) if path.stem not in custom],
                   "-lm", "-o", str(executable)]
        if context_api:
            command.insert(1, "-DMENU_HOST_HAS_CONTEXT=1")
        subprocess.run(command, check=True)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        environment = os.environ.copy()
        environment["MENU_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        environment["MENU_ASSET_ROOT"] = str(root / "aic_ui/lvgl_data")
        environment["MENU_RENDER_ONLY"] = "1" if args.render_only else "0"
        environment["UBSAN_OPTIONS"] = "print_stacktrace=1"
        subprocess.run([str(executable)], env=environment, check=True, timeout=60)


if __name__ == "__main__":
    main()
