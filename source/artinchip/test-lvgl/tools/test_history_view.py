#!/usr/bin/env python3
"""Render and exercise the real History page/search with LVGL 8, real locale fonts and a 24 MiB pool.

The store and external actions are in-memory stubs. No device, real history,
USB mount or firmware write is used. This is not a board touch/GE validation.
"""
from test_i18n_support import with_i18n, lvgl_source
import contextlib, concurrent.futures, hashlib, gzip, io
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_list_view import compiled_asset_sources

def write_if_changed(path,text):
    if not path.exists() or path.read_text()!=text:path.write_text(text)
ROOT = Path(__file__).resolve().parents[1]
COMPONENTS = [
    "un260/font/ui_message_font.c", "un260/font/ui_locale_font_bank.c",
    *["un260/font/lv_font_message_cjk_%d.c" % n for n in (12,14,16,18,20,22,24,28)],
    "un260/workspace/workspace_model.c",
    "tools/test_search_modes.c",
    "un260/lv_components/ui_multi_detail.c",
    "un260/lv_core/page_02_list_search.c",
    "un260/lv_components/ui_detail_reveal.c",
    "un260/lv_components/lv_loading_orbit.c",
    "un260/counting/counting_serial_query.c",
    "un260/counting/counting_data_store.c",
    "un260/lv_components/lv_popup_style.c",
    "tools/test_popup_visibility.c",
    "tools/test_popup_views.c",
    "un260/lv_components/lv_modal_dialog.c",
    "un260/lv_components/lv_qr_popup.c",
    "un260/lv_components/qrcodegen.c",
    "un260/lv_core/settings_detail_ui.c",
    "un260/font/scaled_font.c",
    "un260/font/lv_font_main_currency_32.c",
    "un260/currency/currency_metadata.c",
    "un260/currency/currency_state.c",
    "un260/lv_components/lv_recycled_list.c",
    "un260/lv_components/ui_scrollbar.c",
    "un260/lv_components/lv_select_dropdown.c",
    "un260/lv_components/ui_list_window.c",
    "un260/lv_components/lv_card_surface.c",
    "un260/lv_components/lv_damped_button.c",
    "un260/lv_components/lv_settings.c",
    "un260/lv_components/lv_alnum_keyboard.c",
    "un260/lv_components/lv_nav_button.c",
    "un260/lv_core/page_19_history_search.c",
    "un260/lv_core/ui_frame_commit.c",
    "un260/lv_system/ui_update_batch.c",
    "un260/lv_system/ui_text_page.c",
    "un260/lv_system/ui_text_widget.c",
    "un260/lv_system/ui_lang.c",
    "un260/history/history_record_detail.c",
    "un260/history/history_record_snapshot.c",
    "un260/history/history_record_reader.c",
    "un260/history/history_read_task.c",
    "un260/storage/storage_worker.c",
    "un260/history/history_query.c",
    "un260/history/history_export_sn_parser.c",
    "un260/counting/counting_reject_reason.c",
    "un260/counting/counting_serial_text.c",
    "un260/protocol/protocol_frame.c",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lvgl-dir", required=True, type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--asan", action="store_true", help="Also check host heap/lifetime with AddressSanitizer")
    args = parser.parse_args()
    lvgl = args.lvgl_dir.resolve()
    compiler = os.environ.get("CC") or shutil.which("gcc") or shutil.which("cc")
    if not compiler or not (lvgl / "lvgl.h").is_file():
        raise SystemExit("Requires host GCC and full LVGL 8 source")
    assets = compiled_asset_sources()
    inspected = [ROOT / "un260/lv_core/page_19_history.c", *(ROOT / p for p in COMPONENTS)]
    fonts = sorted(set(re.findall(r"lv_font_(instrument_sans_[a-z]+_\d+)",
                                 "\n".join(p.read_text(encoding="utf-8") for p in inspected))))
    cache=Path(tempfile.gettempdir())/'un260-history-host-cache';cache.mkdir(exist_ok=True)
    with contextlib.nullcontext(str(cache)) as directory:
        work = Path(directory)
        (work / "lvgl").mkdir(exist_ok=True)
        write_if_changed(work / "lvgl/lvgl.h",f'#include "{(lvgl / "lvgl.h").as_posix()}"\n')
        conf = work / "lv_conf.h"
        write_if_changed(conf,"""#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (24U * 1024U * 1024U)
#define LV_USE_BIDI 1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 83
#define LV_FS_STDIO_PATH ""
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_ASSERT_HANDLER __builtin_trap();
#define LV_USE_SNAPSHOT 1
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_CUSTOM_DECLARE """ + " ".join(f"LV_FONT_DECLARE(lv_font_{f});" for f in fonts) + "\n#endif\n")
        port = (ROOT / "lv_port_indev.c").read_text(encoding="utf-8")
        helper = re.search(r"void lv_port_indev_set_drag_obj\(.*?\n\}", port, re.S)
        if not helper:
            raise AssertionError("Production drag ownership registration is missing")
        port_source = work / "port_drag.c"
        write_if_changed(port_source,'#include "lvgl/lvgl.h"\n' + helper.group() + "\n")
        sources = [ROOT / "tools/test_history_view.c", ROOT / "tools/test_history_search_view.c",
                   port_source, *assets,
                   *(ROOT / p for p in COMPONENTS if p not in {"un260/lv_core/page_19_history_search.c",
                     "un260/lv_components/ui_multi_detail.c", "un260/lv_core/page_02_list_search.c"}),
                   *(ROOT / f"un260/font/lv_font_{f}.c" for f in fonts),
                   *sorted(p for p in (lvgl / "src").rglob("*.c") if p.name != "qrcodegen.c")]
        (work / "history").mkdir(exist_ok=True)
        executable = work / "test-history-view"
        sources = with_i18n(sources, ROOT)
        command = [compiler, '-DUN260_LOCALE_FONT_DIR="S:'+str(ROOT/'aic_ui/lvgl_data/locale_fonts')+'/"', '-DUI_HISTORY_STORE_DIR="'+str(work/"history")+'"', "-D_GNU_SOURCE", '-DUI_STATE_DIR="/tmp/un260-i18n-tests"', "-pthread", "-std=c11", "-O1", "-g", "-Wall", "-Wextra",
                   "-DLV_DRV_CONF_H", '-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',
                   "-fsanitize=address,undefined" if args.asan else "-fsanitize=undefined", "-fno-sanitize-recover=all",
                   f"-I{work}", f"-I{ROOT}", f"-I{lvgl}", f"-DLV_CONF_PATH={conf}",
                   *map(str, sources), "-lm", "-lz", "-o", str(executable)]
        # Compile changed units only; generated headers and flags are dependencies.
        flags=command[:command.index(str(sources[0]))]
        def compile_one(source):
            key=hashlib.sha256((str(source)+repr(flags)).encode()).hexdigest()
            obj=work/(key+'.o');dep=work/(key+'.d');deps=[source]
            if dep.exists():deps += [Path(v) for v in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
            if not obj.exists() or not dep.exists() or any(not v.exists() or v.stat().st_mtime_ns>obj.stat().st_mtime_ns for v in deps):
                subprocess.run(flags+['-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)],check=True)
            return obj
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,sources))
        subprocess.run([compiler,'-fsanitize=address,undefined' if args.asan else '-fsanitize=undefined',*map(str,objects),'-pthread','-lm','-lz','-o',str(executable)],check=True)
        from PIL import Image
        for name in ('user', 'settings'):
            with Image.open(ROOT / 'aic_ui/lvgl_data/backgrounds' / (name+'.png')) as image:
                (work / (name+'.bgra')).write_bytes(image.convert('RGBA').tobytes('raw','BGRA'))
        environment = os.environ.copy()
        for name in ('CURR_USD.png','CURR_CNY.png'):
            with Image.open(ROOT/'aic_ui/lvgl_data'/name) as im:
                assert im.size==(182,103)
                (work/(name+'.bgra')).write_bytes(im.convert('RGBA').tobytes('raw','BGRA'))
        environment['UN260_BACKGROUND_DIR'] = str(work)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            environment["HISTORY_RASTER_OUTPUT"] = str(args.output_dir.resolve())
        try:
            subprocess.run([str(executable)], env=environment, check=True, timeout=240)
        finally:
            if args.output_dir:
                for path in args.output_dir.glob('*.bmp.gz'):
                    im=Image.open(io.BytesIO(gzip.decompress(path.read_bytes()))).convert('RGB')
                    target=path.with_suffix('').with_suffix('.png');im.save(target)
                    assert Image.open(target).tobytes()==im.tobytes();path.unlink()


if __name__ == "__main__":
    main()
