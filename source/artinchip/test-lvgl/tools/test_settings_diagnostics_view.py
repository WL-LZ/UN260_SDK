#!/usr/bin/env python3
"""Actual production settings, fonts, PNG resources and LVGL renderer; hardware edges captured."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from test_i18n_support import with_i18n, lvgl_source
import os,re,subprocess,tempfile,sys,contextlib,hashlib
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
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
color_depth=int(os.environ.get('UN260_TEST_COLOR_DEPTH','32'))
assert color_depth in (16,32),color_depth
for name in ['user','settings']:
    im=Image.open(root/'aic_ui/lvgl_data/backgrounds'/f'{name}.png').convert('RGBA')
    (out/f'{name}.bgra').write_bytes(im.tobytes('raw','BGRA'))
pages=['un260/lv_core/'+p for p in ['page_09_cis_cala.c','page_10_debug.c','page_12_sensor.c','page_17_motor_test.c','page_26_set_aging.c','page_dynamic_capture.c']]
parts=[*pages,
       'un260/lv_components/lv_settings.c','un260/lv_components/lv_nav_button.c']
dialog_source=(root/'un260/lv_core/settings_detail_ui.c').read_text()
dialog_functions=['detail_style_plain','detail_panel','settings_detail_create_label','settings_detail_create_button',
 'settings_detail_dialog_is_owned_by','settings_detail_overlay_is_open','settings_detail_dialog_hide',
 'settings_detail_dialog_close','settings_detail_dialog_confirm_cb','settings_detail_dialog_cancel_cb',
 'settings_detail_dialog_show','settings_detail_dialog_deleted','settings_detail_dialog_show_ex']
dialog_actual='\n'.join(function(dialog_source,name) for name in dialog_functions)
fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+',''.join((root/p).read_text() for p in parts)+dialog_actual)))
cache=os.environ.get('UN260_SETTINGS_VIEW_CACHE')
if cache: Path(cache).mkdir(parents=True,exist_ok=True)
def write_if_changed(path,text):
    if not path.exists() or path.read_text()!=text:path.write_text(text)
with contextlib.nullcontext(cache) if cache else tempfile.TemporaryDirectory(prefix='un260-settings-view-') as temp:
    work=Path(temp);(work/'lvgl').mkdir(exist_ok=True);write_if_changed(work/'lvgl/lvgl.h',f'#include "{lvgl}/lvgl.h"\n')
    write_if_changed(work/'lv_drv_conf.h','/* Driver configuration is not used by this renderer. */\n')
    write_if_changed(work/'actual_diagnostic_pointer.h',function((root/'lv_port_indev.c').read_text(),'lv_port_indev_set_drag_obj')+'\n'+function((root/'un260/lv_core/settings_detail_ui.c').read_text(),'settings_detail_add_run'))
    write_if_changed(work/'actual_capture_dialog.h',dialog_actual)
    conf=work/'lv_conf.h';write_if_changed(conf,f'#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH {color_depth}\n#define LV_MEM_SIZE (16U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 0\n#define LV_USE_LOG 0\n#define LV_FONT_MONTSERRAT_18 1\n#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({f});' for f in fonts)+'\n#endif\n')
    # Optional old translation units for deterministic before/after reproduction.
    page_overrides={'page_10_debug.c':os.environ.get('UN260_TEST_DEBUG_SOURCE'),

                    'page_dynamic_capture.c':os.environ.get('UN260_TEST_CAPTURE_SOURCE')}
    page_sources=[Path(override) if (override:=page_overrides.get(Path(p).name)) else root/p for p in pages]
    sources=[*compiled_asset_sources(),root/'tools/test_settings_diagnostics_view.c',*page_sources,root/'un260/lv_components/lv_settings.c',
      root/'un260/lv_components/lv_nav_button.c',root/'un260/lv_components/lv_popup_style.c',root/'un260/lv_components/lv_damped_button.c',root/'un260/lv_components/ui_scrollbar.c',root/'un260/lv_components/lv_content_pager.c',
      root/'un260/diagnostic/diagnostic.c',root/'un260/app_service/motor_test_service.c',root/'un260/protocol/protocol_request.c',root/'un260/protocol/protocol_frame.c',root/'un260/protocol/protocol_frame_parser.c',root/'un260/protocol/stream_data_reply.c',
      *[root/'un260/font'/f'{f}.c' for f in fonts],*lvgl.joinpath('src').rglob('*.c')]
    sources = with_i18n(sources, root)
    common=['gcc','-DLVGL_DIR="L:/usr/local/share/lvgl_data/"','-DLV_DRV_CONF_H','-DUI_STATE_DIR="/tmp/un260-i18n-tests"','-std=gnu11','-O1','-g','-Wall','-Wextra',
      '-fsanitize=address,undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{root}/un260/lv_core',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):
        source,obj=pair;dep=obj.with_suffix('.d');stamp=obj.with_suffix('.flags');signature=hashlib.sha256(repr(common).encode()).hexdigest()
        dependencies=[source]
        if dep.exists():dependencies += [Path(v) for v in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
        if not obj.exists() or not stamp.exists() or stamp.read_text()!=signature or any(not d.exists() or d.stat().st_mtime_ns>obj.stat().st_mtime_ns for d in dependencies):
            subprocess.run([*common,'-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)],check=True);stamp.write_text(signature)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'view';subprocess.run([*common,*map(str,objects),'-lm','-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],env={**os.environ,'OUT':str(out),'UN260_BACKGROUND_DIR':str(out)},check=False)
for file in out.glob('*.bgra'):
    Image.frombytes('RGBA',(1280,400),file.read_bytes(),'raw','BGRA').convert('RGB').save(file.with_suffix('.png'))
sys.exit(result.returncode)
