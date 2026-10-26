#!/usr/bin/env python3
"""Independently verify paired boot variants using C builds and real pack loading.

The temporary source views change only their private header. Production files and
CMake output are read-only. Pass the actual cross-build directory to also verify
the generated source membership and compiler resource-selection seal.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
CONFIG = Path("un260/lv_core/boot_anim/boot_theme_config.h")
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
RESOURCES = [
    "un260/lv_core/boot_anim/boot_customer_art.c",
    "un260/boot/boot_customer_catalog.c",
    "aic_ui/generated_assets/customer_boot/boot_customer_blob.c",
]
PUBLIC = [
    "ui_page_00_boot_anim_create", "ui_page_00_boot_anim_destroy",
    "ui_page_00_boot_anim_is_active", "ui_page_00_boot_anim_poll",
    "ui_page_00_boot_anim_set_startup_ready", "ui_page_00_boot_anim_adopt_elapsed",
    "ui_page_00_boot_anim_set_startup_error", "ui_page_08_curr_create",
    "ui_page_08_curr_destroy", "ui_page_08_curr_defer_next_create",
    "ui_page_08_curr_prepare_step", "ui_page_08_curr_prepare_failed",
    "ui_page_08_curr_set_covered", "ui_page_08_curr_start_handoff",
    "ui_page_08_curr_visual_is_quiet", "ui_page_08_curr_ready_hold_complete",
    "boot_progress_set", "boot_progress_reset", "boot_selftest_list_reset",
    "boot_selftest_list_sync_step", "boot_selftest_list_set_result",
    "boot_selftest_list_finish",
]


def run(command, **kwargs):
    result = subprocess.run([str(value) for value in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        print(result.stdout)
        raise subprocess.CalledProcessError(result.returncode, command, result.stdout)
    return result.stdout


def source_view(root, destination, header):
    destination.mkdir(parents=True, exist_ok=False)
    expanded = {Path("."), Path("un260"), Path("un260/lv_core"),
                Path("un260/lv_core/boot_anim")}

    def populate(relative):
        source = root / relative
        target = destination / relative
        for child in source.iterdir():
            name = relative / child.name
            output = target / child.name
            if name in expanded:
                output.mkdir()
                populate(name)
            elif name == CONFIG:
                output.write_text(header)
            elif str(name) in PAGES:
                shutil.copyfile(child, output)
            else:
                output.symlink_to(child, target_is_directory=child.is_dir())

    populate(Path("."))
    return destination


def variant_header(header, variant):
    result, count = re.subn(
        r"(#ifndef UI_BOOT_VARIANT\s+#define UI_BOOT_VARIANT\s+)"
        r"(?:UI_BOOT_VARIANT_[AB]|[12])\b",
        lambda match: match[1] + "UI_BOOT_VARIANT_" + variant.upper(), header)
    assert count == 1, "one authoritative variant default is required"
    return result


def inspect_cmake(root, build, expected):
    dependency = build / "CMakeFiles/test_lvgl.dir/DependInfo.cmake"
    flags = build / "CMakeFiles/test_lvgl.dir/flags.make"
    text = dependency.read_text()
    sources = {str(Path(path).relative_to(root)) for path in re.findall(r'"([^"]+\.c)"', text)
               if str(Path(path)).startswith(str(root) + "/")}
    assert all(name in sources for name in PAGES), "CMake omitted a public boot page"
    sealed = re.findall(r"-DUI_BOOT_BUILD_VARIANT=([12])\b", flags.read_text())
    assert sealed == [str(1 if expected == "a" else 2)], ("CMake variant seal", sealed, expected)
    for name in RESOURCES:
        assert (name in sources) == (expected == "b"), ("CMake resource selection", expected, name)
    return {"variant": expected, "resource_membership": sorted(sources.intersection(RESOURCES)),
            "dependency_sha256": hashlib.sha256(dependency.read_bytes()).hexdigest(),
            "flags_sha256": hashlib.sha256(flags.read_bytes()).hexdigest()}


LOADER = r'''
#include "un260/lv_drivers/boot_theme_pack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    assert(argc==5);
    unsigned version=(unsigned)atoi(argv[2]), minimum=(unsigned)atoi(argv[3]);
    unsigned mode=(unsigned)atoi(argv[4]);
    static const char *tags[]={"en","zh-Hans","de","fr","ar","ja","ko","pl","ru","tr","invalid"};
    for(unsigned i=0;i<sizeof(tags)/sizeof(tags[0]);i++) {
        boot_theme_pack_t pack;
        assert(boot_theme_pack_load_path(argv[1],&pack));
        assert(pack.version==version && pack.theme==4 && pack.mode==mode);
        assert(pack.min_show_ms==minimum && pack.language_count==10);
        assert(boot_theme_pack_select_language(&pack,tags[i]));
        assert(boot_theme_pack_freeze(&pack) && boot_theme_pack_freeze(&pack));
        assert(pack.data && pack.data_bytes && pack.frozen);
        assert(!boot_theme_pack_select_language(&pack,"en"));
        boot_theme_pack_free(&pack);
        assert(!pack.data && !pack.data_bytes);
    }
    printf("PASS pack v%u mode%u minimum%u ten locales/fallback/freeze/free\n",version,mode,minimum);
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--production-root", type=Path, default=ROOT)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--lvgl-dir", type=Path, required=True)
    parser.add_argument("--cmake-build-dir", type=Path)
    parser.add_argument("--expected-build-variant", choices=("a", "b"))
    args = parser.parse_args()
    assert bool(args.cmake_build_dir) == bool(args.expected_build_variant)
    root, out, lvgl = args.production_root.resolve(), args.output_dir.resolve(), args.lvgl_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    temp = out / "temp"
    temp.mkdir(exist_ok=True)
    os.environ["TMPDIR"] = str(temp)
    # Refuse to overwrite a prior scenario: evidence is retained rather than erased.
    work = out / "variant-fixtures"
    work.mkdir(exist_ok=False)
    header = (root / CONFIG).read_text()
    source_hash = hashlib.sha256(header.encode()).hexdigest()
    spec = importlib.util.spec_from_file_location("variant_bundle_builder", root / "tools/build_boot_bundle.py")
    builder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(builder)
    (work / "lvgl").mkdir()
    (work / "lvgl/lvgl.h").write_text('#include "' + str(lvgl / "lvgl.h") + '"\n')
    conf = work / "lv_conf.h"
    conf.write_text("""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (24U*1024U*1024U)
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_LOG 0
#endif
""")
    font_sources = [root / name for name in PAGES] + [root / "un260/font/boot_fonts.h"]
    fonts = sorted({name for name in re.findall(r"\blv_font_([A-Za-z0-9_]+)\b",
                    "\n".join(path.read_text() for path in font_sources))
                    if (root / "un260/font" / ("lv_font_" + name + ".c")).is_file()})
    declarations = work / "font-declarations.h"
    declarations.write_text('#include "lvgl/lvgl.h"\n' +
                            "\n".join("LV_FONT_DECLARE(lv_font_" + name + ");" for name in fonts))
    cc = os.environ.get("CC", "cc")
    loader_source = work / "pack-loader.c"
    loader_source.write_text(LOADER)
    loader = work / "pack-loader"
    run([cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
         "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-no-pie",
         "-I" + str(root), loader_source, root / "un260/lv_drivers/boot_theme_pack.c",
         root / "un260/boot/boot_customer_catalog.c", "-lz", "-o", loader])
    results = []
    first_a = None
    for index, variant in enumerate(("a", "b", "a")):
        name = str(index) + "-" + variant
        stage = work / name
        stage.mkdir()
        view = source_view(root, stage / "source", variant_header(header, variant))
        variant_id = 1 if variant == "a" else 2
        assert builder.source_variant(view) == variant
        assert builder.read_config(view)[:2] == ("d", "animation")
        common = [cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-DLV_DRV_CONF_H",
                  "-I" + str(work), "-I" + str(view), "-I" + str(view / "un260"),
                  "-I" + str(lvgl), "-DLV_CONF_PATH=" + str(conf),
                  "-include", str(declarations)]
        probe = stage / "config.c"
        probe.write_text('#include "un260/lv_core/boot_anim/boot_theme_config.h"\n'
                         '#include <stdio.h>\nint main(void){'
                         'printf("%d %d\\n",UI_BOOT_ANIM_THEME,UI_BOOT_VARIANT);return 0;}\n')
        for seal in (None, variant_id):
            exe = stage / ("config-" + str(seal))
            defines = [] if seal is None else ["-DUI_BOOT_BUILD_VARIANT=" + str(seal)]
            run(common + defines + [probe, "-o", exe])
            assert run([exe]).strip() == "4 " + str(variant_id)
        for defines in (["-DUI_BOOT_BUILD_VARIANT=" + str(3 - variant_id)],
                        ["-DUI_BOOT_BUILD_VARIANT=" + str(variant_id),
                         "-DUI_BOOT_VARIANT=" + str(3 - variant_id)],
                        ["-DUI_BOOT_VARIANT=0"], ["-DUI_BOOT_VARIANT=3"]):
            invalid = subprocess.run([str(v) for v in common + defines +
                                      [probe, "-o", stage / "invalid"]],
                                     text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            assert invalid.returncode and ("variant" in invalid.stdout or "UI_BOOT_VARIANT" in invalid.stdout)
        objects = []
        for page_index, page in enumerate(PAGES):
            obj = stage / ("page-" + str(page_index) + ".o")
            run(common + ["-DUI_BOOT_BUILD_VARIANT=" + str(variant_id),
                          "-c", view / page, "-o", obj])
            objects.append(obj)
        combined = stage / "boot-pages.o"
        run([cc, "-r", *objects, "-o", combined])
        symbols = run(["nm", "--defined-only", combined])
        for symbol in PUBLIC:
            assert len(re.findall(r"\bT\s+" + symbol + r"$", symbols, re.M)) == 1, (name, symbol)
        for object_index in (4, 7):
            uses_art = "boot_customer_art_acquire" in run(["nm", "-u", objects[object_index]])
            assert uses_art == (variant == "b"), (name, object_index, "art owner")
        # A's original handoff timer and B's no-overlay handoff are exercised by
        # their full native page suites; this test proves configuration/ABI pairing.
        payload, metadata = builder.theme_pack(view, "d")
        pack = stage / "theme.pack"
        pack.write_bytes(payload)
        version = 1 if variant == "a" else 2
        minimum = 2300 if variant == "a" else 2200
        assert payload[:8] == ("UNBTPK" + str(version)).encode() + b"\0"
        print(run([loader, pack, version, minimum, 1]), end="")
        if variant == "a":
            if first_a is None:
                first_a = payload
            else:
                assert payload == first_a, "A/B/A reused stale customer pack"
        static, unused = builder.theme_pack(view, "d", "logo")
        static_path = stage / "logo.pack"
        static_path.write_bytes(static)
        print(run([loader, static_path, 1, 0, 2]), end="")
        results.append({"stage": name, "variant_id": variant_id,
                        "pack_sha256": hashlib.sha256(payload).hexdigest(),
                        "pack_version": version, "pack_bytes": len(payload),
                        "minimum_ms": minimum, "public_symbols": len(PUBLIC),
                        "compile_rejections": 4, "builder_metadata": metadata})
        print("PASS paired C/header/pages/pack", name)
    cmake = None
    if args.cmake_build_dir:
        assert builder.source_variant(root) == args.expected_build_variant, "source header differs from configured build"
        cmake = inspect_cmake(root, args.cmake_build_dir.resolve(), args.expected_build_variant)
        print("PASS actual generated CMake source membership/seal", args.expected_build_variant)
    report = {"result": "passed", "host_test": True, "board_test": False,
              "source_header_sha256_at_start": source_hash, "scenarios": results,
              "cmake": cmake,
              "sources": {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                          for name in PAGES + RESOURCES + ["tools/build_boot_bundle.py",
                          "un260/lv_drivers/boot_theme_pack.c", str(CONFIG)]}}
    (out / "verification.json").write_text(json.dumps(report, indent=2))
    print("PASS paired variant verification:", out)


if __name__ == "__main__":
    main()
