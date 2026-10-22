#!/usr/bin/env python3
"""Actual production settings, fonts, PNG resources and LVGL renderer; hardware edges captured."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from test_i18n_support import with_i18n, lvgl_source
import argparse,os,re,subprocess,tempfile,sys
from PIL import Image
from test_list_view import compiled_asset_sources
root=Path(__file__).resolve().parents[1]
def function(source,name):
    match=re.search(r'^(?:static )?[\w *]+\b'+name+r'\([^;]*?\)\s*\{',source,re.M)
    assert match,name
    depth=0
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*|[{}]',source[source.index('{',match.start()):],re.S):
        if token.group()=='{':depth+=1
        elif token.group()=='}':
            depth-=1
            if not depth:return source[match.start():source.index('{',match.start())+token.end()]
lvgl = lvgl_source(root)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('output_dir',type=Path)
parser.add_argument('--harness',type=Path,help='Explicit focused harness; default keeps the complete legacy suite')
parser.add_argument('--panel-fixture-in-harness',action='store_true',help='Harness supplies the Panel navigation boundary')
args=parser.parse_args()
out=args.output_dir.resolve();out.mkdir(parents=True,exist_ok=True)
for name in ['user','settings']:
    im=Image.open(root/'aic_ui/lvgl_data/backgrounds'/f'{name}.png').convert('RGBA')
    (out/f'{name}.bgra').write_bytes(im.tobytes('raw','BGRA'))
parts=['un260/lv_core/page_06_settings.c','un260/lv_core/page_11_timeset.c','un260/lv_core/page_21_set_language.c',
       'un260/lv_components/lv_settings.c','un260/lv_components/lv_nav_button.c','un260/font/ui_message_font.c','un260/lv_core/page_collection.c']
fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope|message)_[a-zA-Z0-9_]+',''.join((root/p).read_text() for p in parts))))
with tempfile.TemporaryDirectory(prefix='un260-settings-view-') as temp:
    work=Path(temp);(work/'lvgl').mkdir();(work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    (work/'lv_drv_conf.h').write_text('/* Host rendering: no physical driver. */\n')
    port=(root/'lv_port_indev.c').read_text()
    (work/'actual_settings_pointer.h').write_text(function(port,'evdev_feedback')+'\n'+function(port,'lv_port_indev_set_drag_obj'))
    theme=work/'actual_settings_theme.c'
    theme.write_text('#include "lvgl/lvgl.h"\n'+function((root/'un260/lv_core/settings_detail_ui.c').read_text(),'settings_theme_color_hex')+'\n')
    conf=work/'lv_conf.h';conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (24U*1024U*1024U)\n#define LV_USE_BIDI 1\n#define LV_USE_ARABIC_PERSIAN_CHARS 1\n#define LV_USE_FS_STDIO 1\n#define LV_FS_STDIO_LETTER 83\n#define LV_FS_STDIO_PATH ""\n#define LV_USE_THEME_DEFAULT 0\n#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({f});' for f in fonts)+'\n#endif\n')
    sources=[theme,*compiled_asset_sources(),args.harness.resolve() if args.harness else root/'tools/test_settings_view.c',root/'un260/lv_components/lv_settings.c',
      root/'un260/lv_components/lv_nav_button.c',root/'un260/lv_components/lv_damped_button.c',
      root/'un260/lv_core/settings_catalog.c',root/'un260/lv_system/machine_time.c',root/'un260/lv_system/ui_lang.c',
      root/'un260/data_collection/data_collection.c',root/'un260/device_info/device_info.c',
      root/'un260/lv_core/page_collection.c',root/'un260/app_service/app_collection.c',root/'un260/protocol/protocol_request.c',
      root/'un260/font/ui_message_font.c',root/'un260/font/ui_locale_font_bank.c',
      *[root/'un260/font'/f'{f}.c' for f in fonts],*lvgl.joinpath('src').rglob('*.c')]
    if not args.panel_fixture_in_harness:
        sources.append(root / "tools/tests/fullscreen_panel_stubs.c")
    sources = with_i18n(sources, root)
    # Exercise every language row before the production readiness gate opens.
    (work/'i18n/generated').mkdir(parents=True)
    (work/'i18n/generated/ui_locale_registry.inc').write_text((root/'i18n/generated/ui_locale_registry.inc').read_text().replace('false','true'))
    (work/'ui_lang.c').write_text((root/'un260/lv_system/ui_lang.c').read_text())
    sources=[work/'ui_lang.c' if p==root/'un260/lv_system/ui_lang.c' else p for p in sources]
    common=['gcc','-DUN260_LOCALE_FONT_DIR="S:'+str(root/'aic_ui/lvgl_data/locale_fonts')+'/"', '-I'+str(root/'un260/lv_system'),'-DLVGL_DIR="L:/usr/local/share/lvgl_data/"','-DUI_STATE_DIR="/tmp/un260-i18n-tests"','-DLV_DRV_CONF_H','-std=gnu11','-O1','-g','-Wall','-Wextra',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'view';subprocess.run([*common,*map(str,objects),'-lm','-lz','-o',str(exe)],check=True)
    subprocess.run([str(exe)],env={**os.environ,'OUT':str(out),'UN260_BACKGROUND_DIR':str(out)},check=True)
for file in out.glob('*.bgra'):
    Image.frombytes('RGBA',(1280,400),file.read_bytes(),'raw','BGRA').convert('RGB').save(file.with_suffix('.png'))
