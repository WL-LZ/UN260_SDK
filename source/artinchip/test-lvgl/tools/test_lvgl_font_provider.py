#!/usr/bin/env python3
"""Check UN260's private LVGL font configuration and real firmware providers.

Example, from the firmware repository or application directory:
  python3 tools/test_lvgl_font_provider.py --sdk /REPO/output/d211_d213_devkitf/host --repo /REPO

--app selects the unstripped application ELF, not the source directory. The
packaged application is checked independently in --target-root. Configuration
and ABI probes compile only into a disposable temporary directory.
"""

import argparse
import hashlib
from pathlib import Path
import re
import shlex
import struct
import subprocess
import sys
import tempfile


SIZES = (10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 40, 42, 44, 48)
FONTS = {f"lv_font_montserrat_{size}" for size in SIZES}
REMOVED_FONTS = FONTS - {"lv_font_montserrat_14"}
SIYUAN = "lv_font_siyuanheiti_normal_24"
EXPECTED_MACROS = {f"LV_FONT_MONTSERRAT_{size}" for size in SIZES if size != 14}
EXPECTED_MACROS.add("LV_FONT_SIYUANHEITI_NORMAL_24")
CJK_FONTS = {f"lv_font_message_cjk_{size}" for size in (12, 14, 16, 18, 20, 22, 24, 28)}


def run(command, input_text=None):
    result = subprocess.run(command, input=input_text, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f"Command failed: {' '.join(map(str, command))}\n{result.stderr}")
    return result.stdout


def tool(sdk, name):
    for directory in (sdk / "bin", sdk):
        preferred = directory / f"riscv64-unknown-linux-gnu-{name}"
        if preferred.is_file():
            return str(preferred)
        candidates = sorted(directory.glob(f"riscv*-{name}"))
        if len(candidates) == 1:
            return str(candidates[0])
    raise RuntimeError(f"Cannot find target {name} under {sdk}")


def repo_path(explicit, sdk):
    if explicit:
        return explicit.resolve()
    for base in (Path.cwd(), sdk):
        for candidate in (base, *base.parents):
            if (candidate / "source/artinchip/test-lvgl/lv_conf.h").is_file():
                return candidate
    raise RuntimeError("Cannot infer repository; specify --repo")


def macros(text):
    result = {}
    for line in text.splitlines():
        match = re.match(r"^#define\s+(LV_[A-Za-z0-9_]+)(.*)$", line)
        if match and match[1] != "LV_CONF_PATH":
            result[match[1]] = " ".join(match[2].split())
    return result


def symbol_rows(readelf, path, dynamic=False):
    output = run([readelf, "-W", "--dyn-syms" if dynamic else "--syms", str(path)])
    rows = {}
    for line in output.splitlines():
        match = re.match(r"^\s*\d+:\s+([0-9a-fA-F]+)\s+(\d+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\w+)\s+(\S+)", line)
        if match:
            value, size, kind, binding, visibility, section, name = match.groups()
            name = name.split("@", 1)[0]
            rows[name] = dict(value=int(value, 16), size=int(size), kind=kind,
                              binding=binding, visibility=visibility, section=section)
    return rows


def has_definition(rows, name):
    return name in rows and rows[name]["section"] not in ("UND", "ABS")


def sections(readelf, path):
    result = {}
    for line in run([readelf, "-W", "--section-headers", str(path)]).splitlines():
        match = re.match(r"^\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s*(.*?)\s+\d+\s+\d+\s+\d+\s*$", line)
        if match:
            name, kind, address, offset, size, flags = match.groups()
            result[name] = dict(kind=kind, address=int(address, 16), offset=int(offset, 16),
                                size=int(size, 16), flags=flags.strip())
    return result


def loadable_digest(readelf, path):
    # Strip may remove non-allocatable symbol/debug sections. Compare the actual
    # loader-visible bytes plus BSS sizes, rather than whole-file hashes.
    # CMake installation rewrites RPATH in .dynstr (e.g. dropping its trailing
    # empty padding). Font symbols are checked separately; do not mistake this
    # legitimate string-table rewrite for changed font/code payload.
    digest = hashlib.sha256()
    raw = path.read_bytes()
    for name, section in sorted(sections(readelf, path).items()):
        if "A" not in section["flags"] or name == ".dynstr":
            continue
        digest.update(f"{name}:{section['kind']}:{section['address']}:{section['size']}\n".encode())
        if section["kind"] != "NOBITS":
            offset, size = section["offset"], section["size"]
            digest.update(raw[offset:offset + size])
    return digest.hexdigest()


def compiler_flags(path):
    text = path.read_text()
    values = {}
    for key in ("C_DEFINES", "C_INCLUDES", "C_FLAGS"):
        match = re.search(rf"^{key}\s*=\s*(.*)$", text, re.MULTILINE)
        values[key] = shlex.split(match[1]) if match else []
    return values


def elf_files(root):
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        with path.open("rb") as handle:
            if handle.read(4) == b"\x7fELF":
                yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--repo", type=Path)
    parser.add_argument("--app", type=Path, help="Unstripped application ELF; defaults to output/build/test-lvgl/test_lvgl")
    parser.add_argument("--app-build", type=Path)
    parser.add_argument("--lvgl-build", type=Path)
    parser.add_argument("--target-root", type=Path)
    parser.add_argument("--library", type=Path)
    parser.add_argument("--sysroot", type=Path)
    parser.add_argument("--config-only", action="store_true", help="Run SDK configuration/declaration/ABI probes before firmware rebuild")
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    repo = repo_path(args.repo, sdk)
    app_source = repo / "source/artinchip/test-lvgl"
    lvgl_source = repo / "source/third-party/lvgl-8.3.2"
    app_config = app_source / "lv_conf.h"
    private_config = app_source / "lv_conf_library.h"
    sysroot = args.sysroot.resolve() if args.sysroot else sdk / "riscv64-linux-gnu/sysroot"
    output = sdk.parent
    app_build = args.app_build.resolve() if args.app_build else output / "build/test-lvgl"
    lvgl_build = args.lvgl_build.resolve() if args.lvgl_build else output / "build/lvgl-8.3.2"
    root = args.target_root.resolve() if args.target_root else output / "target"
    app = args.app.resolve() if args.app else app_build / "test_lvgl"
    packaged_app = root / "usr/local/bin/test_lvgl"
    library = args.library.resolve() if args.library else root / "usr/local/lib/liblvgl.so"
    gcc, readelf = tool(sdk, "gcc"), tool(sdk, "readelf")
    failures = []

    def require(condition, message):
        if not condition:
            failures.append(message)

    with tempfile.TemporaryDirectory(prefix="un260-lvgl-font-provider-") as temporary:
        temporary = Path(temporary)
        base_flags = [gcc, f"--sysroot={sysroot}", "-DLV_CONF_INCLUDE_SIMPLE",
                      "-I", str(app_source), "-isystem", str(lvgl_source)]
        header = f'#include "{lvgl_source / "lvgl.h"}"\n'
        configurations = {}
        declarations = {}
        abi_values = {}
        for label, config in (("app", app_config), ("library", private_config)):
            flags = base_flags + [f"-DLV_CONF_PATH={config}"]
            configurations[label] = macros(run(flags + ["-dM", "-E", "-x", "c", "-"], header))
            # The real fallback source supplies the eight application CJK font
            # declarations, in addition to LV_FONT_CUSTOM_DECLARE from config.
            declaration_source = header + f'#include "{app_source / "un260/font/ui_message_font.c"}"\n'
            preprocessed = run(flags + ["-E", "-P", "-x", "c", "-"], declaration_source)
            declarations[label] = set(re.findall(r"\bextern\s+(?:const\s+)?lv_font_t\s+(\w+)\s*;", preprocessed))
            probe = temporary / f"abi_{label}.o"
            probe_source = header + '''
#include <stddef.h>
const uint32_t un260_font_provider_abi[] = {
    sizeof(lv_font_t), offsetof(lv_font_t, get_glyph_dsc), offsetof(lv_font_t, get_glyph_bitmap),
    offsetof(lv_font_t, line_height), offsetof(lv_font_t, base_line), offsetof(lv_font_t, dsc),
    offsetof(lv_font_t, fallback), sizeof(lv_font_glyph_dsc_t), sizeof(lv_font_fmt_txt_dsc_t),
    sizeof(lv_font_fmt_txt_glyph_dsc_t), sizeof(lv_font_fmt_txt_cmap_t),
    sizeof(lv_coord_t), sizeof(lv_area_t), sizeof(lv_color_t), sizeof(lv_draw_rect_dsc_t),
    sizeof(lv_draw_ctx_t), sizeof(lv_obj_t), sizeof(lv_style_t), sizeof(lv_style_value_t),
#if LV_USE_USER_DATA
    offsetof(lv_font_t, user_data),
#else
    0xffffffffU,
#endif
};
'''
            run(flags + ["-O1", "-c", "-x", "c", "-", "-o", str(probe)], probe_source)
            section = sections(readelf, probe).get(".rodata")
            if section is None:
                raise RuntimeError("ABI probe .rodata missing")
            raw = probe.read_bytes()[section["offset"]:section["offset"] + section["size"]]
            abi_values[label] = struct.unpack(f"<{len(raw) // 4}I", raw)
        keys = set(configurations["app"]) | set(configurations["library"])
        differences = {key for key in keys if configurations["app"].get(key) != configurations["library"].get(key)}
        require(differences == EXPECTED_MACROS, f"Unexpected LV_* macro differences: extra={sorted(differences - EXPECTED_MACROS)}, missing={sorted(EXPECTED_MACROS - differences)}")
        for macro in EXPECTED_MACROS:
            expected_app = None if macro == "LV_FONT_SIYUANHEITI_NORMAL_24" else "1"
            require(configurations["app"].get(macro) == expected_app, f"Application font switch changed: {macro}")
            require(configurations["library"].get(macro) == "0", f"Private library switch is not disabled: {macro}")
        require(configurations["app"].get("LV_FONT_MONTSERRAT_14") == "1", "Application default Montserrat14 disabled")
        require(configurations["library"].get("LV_FONT_MONTSERRAT_14") == "1", "Library default Montserrat14 disabled")
        require(configurations["library"].get("LV_FONT_DEFAULT") == "&lv_font_montserrat_14", "Library default font changed")
        require(abi_values["app"] == abi_values["library"], "Target font/draw/object/memory ABI values differ")
        custom_app = declarations["app"] - FONTS - {SIYUAN}
        custom_library = declarations["library"] - FONTS - {SIYUAN}
        require(custom_app == custom_library, "Custom font/CJK declarations differ under private configuration")
        require(CJK_FONTS <= custom_app, "Actual message CJK fallback declarations missing")
        require(declarations["app"] - declarations["library"] == REMOVED_FONTS, "Builtin font declaration differences are not precisely the 15 removed sizes")
        print(f"config: {len(keys)} LV_* macros checked; exactly 16 font differences; {len(custom_app)} custom font declarations")
        print(f"ABI: {len(abi_values['app'])} target sizes/offsets checked; lv_font_t={abi_values['app'][0]} bytes")

    if not args.config_only:
        require(app.is_file(), f"Unstripped application missing: {app}")
        require(library.is_file(), f"Packaged library missing: {library}")
        if app.is_file() and library.is_file():
            app_symbols = symbol_rows(readelf, app)
            library_symbols = symbol_rows(readelf, library, dynamic=True)
            font_size = abi_values["app"][0]
            for name in FONTS | CJK_FONTS:
                require(has_definition(app_symbols, name), f"Linked application provider missing: {name}")
                require(app_symbols.get(name, {}).get("size") == font_size, f"Linked application font ABI size changed: {name}")
                require(app_symbols.get(name, {}).get("binding") == "GLOBAL" and app_symbols.get(name, {}).get("kind") == "OBJECT", f"Linked font is not a public object provider: {name}")
            require(has_definition(library_symbols, "lv_font_montserrat_14"), "Packaged library default14 missing")
            require(library_symbols.get("lv_font_montserrat_14", {}).get("size") == font_size, "Packaged library default14 ABI size changed")
            for name in REMOVED_FONTS | {SIYUAN}:
                require(name not in library_symbols, f"Packaged library still contains/depends on duplicate font: {name}")
            for name in FONTS | CJK_FONTS:
                object_path = app_build / f"CMakeFiles/test_lvgl.dir/un260/font/{name}.c.o"
                require(object_path.is_file(), f"Actual application font object missing: {object_path}")
                if object_path.is_file():
                    rows = symbol_rows(readelf, object_path)
                    require(has_definition(rows, name), f"Application build font payload disabled: {name}")
                    require(rows.get(name, {}).get("size") == font_size, f"Application build font descriptor size differs: {name}")
                    require(rows.get(name, {}).get("binding") == "GLOBAL" and rows.get(name, {}).get("kind") == "OBJECT", f"Application build font is not a public object provider: {name}")
                    allocated = sum(section["size"] for section in sections(readelf, object_path).values() if "A" in section["flags"])
                    require(allocated > font_size, f"Application font object has no glyph payload: {name}")
            for name in REMOVED_FONTS | {SIYUAN}:
                object_path = lvgl_build / f"CMakeFiles/lvgl.dir/src/font/{name}.c.o"
                require(object_path.is_file(), f"Actual library font object missing: {object_path}")
                if object_path.is_file():
                    allocated = sum(section["size"] for section in sections(readelf, object_path).values() if "A" in section["flags"])
                    require(allocated == 0, f"Disabled library font still has {allocated} allocated payload bytes: {name}")
            library_flags = compiler_flags(lvgl_build / "CMakeFiles/lvgl.dir/flags.make")
            require(f"-DLV_CONF_PATH={private_config}" in library_flags["C_DEFINES"], "Actual library compile flags do not select private wrapper")
            app_flags = compiler_flags(app_build / "CMakeFiles/test_lvgl.dir/flags.make")
            require(not any("LV_CONF_PATH" in flag for flag in app_flags["C_DEFINES"]), "Private LV_CONF_PATH leaked into application compile flags")
            require(packaged_app.is_file(), f"Packaged application missing: {packaged_app}")
            if packaged_app.is_file():
                require(loadable_digest(readelf, app) == loadable_digest(readelf, packaged_app), "Packaged application loader-visible bytes differ from verified build ELF")
            print("providers: actual application 16 Montserrat + 8 CJK objects/linked fonts checked; library keeps default14 only")

            nodes = {}
            providers = {}
            consumers = []
            for path in elf_files(root):
                dynamic = run([readelf, "-W", "--dynamic", str(path)])
                needed = re.findall(r"\(NEEDED\).*\[([^]]+)\]", dynamic)
                soname = re.search(r"\(SONAME\).*\[([^]]+)\]", dynamic)
                rows = symbol_rows(readelf, path, dynamic=True)
                nodes[path] = dict(needed=needed, rows=rows)
                providers.setdefault(path.name, path)
                if soname:
                    providers[soname[1]] = path
                if "liblvgl.so" in needed:
                    consumers.append(path)
            require(consumers == [packaged_app], f"Unexpected rootfs LVGL consumers: {[str(path.relative_to(root)) for path in consumers]}")
            checked_references = 0
            for path, node in nodes.items():
                undefined_fonts = {name for name, row in node["rows"].items() if name.startswith("lv_font_") and row["section"] == "UND" and row["binding"] != "WEAK"}
                if not undefined_fonts:
                    continue
                closure, queue = set(), [path]
                while queue:
                    current = queue.pop()
                    if current in closure:
                        continue
                    closure.add(current)
                    queue.extend(providers[dependency] for dependency in nodes[current]["needed"] if dependency in providers)
                defined = {name for current in closure for name, row in nodes[current]["rows"].items() if row["section"] not in ("UND", "ABS") and row["binding"] in ("GLOBAL", "WEAK")}
                missing = undefined_fonts - defined
                require(not missing, f"Unresolved rootfs font dependencies in {path.relative_to(root)}: {sorted(missing)}")
                checked_references += len(undefined_fonts)
            print(f"rootfs: {len(nodes)} ELF files, {len(consumers)} LVGL consumer, {checked_references} font API/provider references resolved")

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1
    print("PASS: private font configuration preserves ABI and application font providers" + (" (configuration only)" if args.config_only else ""))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, ValueError, KeyError, OSError, struct.error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(2)
