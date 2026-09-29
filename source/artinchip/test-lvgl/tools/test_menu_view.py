from pathlib import Path
from test_i18n_support import with_i18n, lvgl_source
import re,subprocess,tempfile,os,hashlib,contextlib,concurrent.futures

def write_if_changed(path,text):
    if not path.exists() or path.read_text()!=text:path.write_text(text)
from test_list_view import compiled_asset_sources
root=Path(__file__).resolve().parents[1]
lvgl = lvgl_source(root)
parts=['tools/test_menu_view.c','un260/workspace/workspace_model.c','un260/workspace/cashbook.c','un260/counting/counting_reject_reason.c','un260/lv_components/lv_settings.c','un260/lv_components/lv_quick_controls.c','un260/lv_components/lv_damped_button.c','un260/lv_components/lv_nav_button.c','un260/lv_components/lv_modal_dialog.c','un260/lv_components/lv_popup_style.c','un260/lv_components/lv_alnum_keyboard.c','un260/lv_components/lv_qr_popup.c','un260/lv_components/qrcodegen.c','un260/lv_components/ui_scrollbar.c','un260/lv_core/settings_detail_ui.c','un260/lv_system/ui_lang.c','un260/lv_system/ui_text_widget.c','un260/lv_system/ui_text_page.c']
inspected=parts+['un260/lv_core/page_03_menu.c']+['un260/lv_core/'+p.name for p in (root/'un260/lv_core').glob('menu_*.inc')]
parts+=['un260/app_service/support_report.c','un260/app_service/app_auto_qr.c']
fonts=sorted(set(re.findall(r'lv_font_(instrument_sans_[a-z]+_\d+)','\n'.join((root/p).read_text() for p in inspected))))
out=Path(os.environ.get('MENU_OUTPUT','/tmp/un260-menu-renders'));out.mkdir(exist_ok=True)
cache=Path('/tmp/un260-menu-host-cache');cache.mkdir(exist_ok=True)
with contextlib.nullcontext(str(cache)) as directory:
    work=Path(directory);(work/'lvgl').mkdir(exist_ok=True);write_if_changed(work/'lvgl/lvgl.h','#include "'+str(lvgl/'lvgl.h')+'"\n')
    conf=work/'lv_conf.h';write_if_changed(conf,'''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (4U*1024U*1024U)
#define LV_USE_GPU_AIC 0
#define LV_USE_GPU_AIC_GE 0
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO 0
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_ASSERT_HANDLER __builtin_trap();
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_CUSTOM_DECLARE '''+' '.join('LV_FONT_DECLARE(lv_font_'+f+');' for f in fonts)+'\n#endif\n')
    port=(root/'lv_port_indev.c').read_text();helper=re.search(r'void lv_port_indev_set_drag_obj\(.*?\n\}',port,re.S);assert helper
    bridge=work/'port.c';write_if_changed(bridge,'#include "lvgl/lvgl.h"\n'+helper[0])
    sources=[root/p for p in parts]+[root/('un260/font/lv_font_'+f+'.c') for f in fonts]+compiled_asset_sources()+[bridge]+sorted(p for p in (lvgl/'src').rglob('*.c') if p.name!='qrcodegen.c')
    sources = with_i18n(sources, root)
    flags=['cc','-DUI_STATE_DIR="/tmp/un260-i18n-tests"','-std=gnu11','-g','-O1','-Wall','-Wextra','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-DLV_DRV_CONF_H','-DLVGL_DIR="L:/usr/local/share/lvgl_data/"',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    subprocess.run(flags+['-fsyntax-only',str(root/'tools/test_menu_view.c')],check=True)
    def compile_one(source):
        key=hashlib.sha256((str(source)+repr(flags)).encode()).hexdigest()
        obj=work/(key+'.o');dep=work/(key+'.d')
        deps=[source]
        if dep.exists():
            deps += [Path(p) for p in dep.read_text().replace('\\\n',' ').split(':',1)[1].split()]
        if not obj.exists() or not dep.exists() or any(not p.exists() or p.stat().st_mtime_ns>obj.stat().st_mtime_ns for p in deps):
            subprocess.run(flags+['-MMD','-MF',str(dep),'-c',str(source),'-o',str(obj)],check=True)
        return obj
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,sources))
    exe=work/'test';subprocess.run(['cc','-fsanitize=address,undefined','-no-pie',*map(str,objects),'-lm','-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],env=dict(os.environ,MENU_OUTPUT=str(out)))
    from PIL import Image
    for path in out.glob('*.bgra'):Image.frombytes('RGBA',(1280,400),path.read_bytes(),'raw','BGRA').convert('RGB').save(path.with_suffix('.png'))
    result.check_returncode()
