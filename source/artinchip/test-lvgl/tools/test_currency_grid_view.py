"""Render the production VIEW builder at 1280x400; not a board/protocol test."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import os,re,subprocess,sys,tempfile
from PIL import Image
from test_currency_modes import function
from test_i18n_support import with_i18n, lvgl_source
root=Path(__file__).resolve().parents[1]
lvgl=lvgl_source(root)
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable, str(root/'tools/gen_currency_view_atlas.py'), '--check'],check=True)
with tempfile.TemporaryDirectory(prefix='un260-currency-grid-') as temp:
    work=Path(temp);(work/'lvgl').mkdir();(work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    (work/'lv_drv_conf.h').write_text('/* Host rendering only. */\n')
    font_parts=['un260/font/ui_message_font.c','un260/font/ui_locale_font_bank.c']
    font_parts += ['un260/font/lv_font_message_cjk_'+str(n)+'.c' for n in [12,14,16,18,20,22,24,28]]
    fonts=sorted(set(re.findall(r'\blv_font_(?:instrument_sans|manrope)_[a-zA-Z0-9_]+',
                       ''.join((root/p).read_text() for p in font_parts))))
    conf=work/'lv_conf.h';conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (16U*1024U*1024U)\n#define LV_IMG_CACHE_DEF_SIZE 48\n#define LV_USE_THEME_DEFAULT 0\n#define LV_USE_BIDI 1\n#define LV_USE_ARABIC_PERSIAN_CHARS 1\n#define LV_USE_FS_STDIO 1\n#define LV_FS_STDIO_LETTER 83\n#define LV_FS_STDIO_PATH ""\n#define LV_FONT_CUSTOM_DECLARE '+' '.join(f'LV_FONT_DECLARE({s});' for s in fonts)+'\n#endif\n')
    page=(root/'un260/lv_core/page_07_curr.c').read_text();port=(root/'lv_port_indev.c').read_text()
    image=(root/'un260/lv_resources/lv_img_init.c').read_text()
    currency=(root/'un260/currency/currency_state.c').read_text()
    parts=[function(currency,'currency_state_display_code'),function(image,'get_currency_img'),function(port,'evdev_feedback'),function(port,'lv_port_indev_set_drag_obj')]
    parts += [function(page,n) for n in ['curr_fav_press_feedback_cb','curr_build_grid_layer','curr_apply_grid_selected_style','curr_set_mode_visible']]
    (work/'grid_under_test.h').write_text('\n'.join(parts))
    entries=[]
    for path in [*(root/'aic_ui/lvgl_data').glob('CURR_*.png'),root/'aic_ui/lvgl_data/main_icons/multi_card.png',root/'aic_ui/lvgl_data/currency_view/flags.png',root/'aic_ui/lvgl_data/main_icons/currencies_32.png']:
        bitmap=Image.open(path).convert('RGBA');target=work/(path.stem+'.bgra');target.write_bytes(bitmap.tobytes('raw','BGRA'))
        entries.append(f'{{"{path.name}","{target}",{bitmap.width},{bitmap.height}}}')
    (work/'grid_assets.h').write_text('typedef struct {const char *name,*path;unsigned w,h;} grid_asset_t;\nstatic grid_asset_t grid_assets[]={'+','.join(entries)+'};\n')
    sources=[root/'tools/test_currency_grid_view.c',root/'un260/lv_core/page_07_curr/page_07_curr_view.c',root/'un260/lv_core/page_07_curr/page_07_curr_overview.c',root/'un260/lv_components/lv_damped_button.c',root/'un260/lv_components/ui_scrollbar.c',*[root/'un260/font'/f'{n}.c' for n in fonts],*[root/p for p in font_parts],*lvgl.joinpath('src').rglob('*.c')]
    sources=with_i18n(sources,root)
    common=['gcc','-std=gnu11','-O1','-g','-Wall','-Wextra','-fsanitize=address,undefined','-fno-sanitize-recover=all','-no-pie','-DUI_STATE_DIR="/tmp/un260-i18n-tests"','-DUN260_LOCALE_FONT_DIR="S:'+str(root/'aic_ui/lvgl_data/locale_fonts')+'/"',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'view';subprocess.run([*common,*map(str,objects),'-lm','-lz','-o',str(exe)],check=True)
    result=subprocess.run([str(exe)],env={**os.environ,'OUT':str(out)})
for path in out.glob('*.bgra'):Image.frombytes('RGBA',(1280,400),path.read_bytes(),'raw','BGRA').convert('RGB').save(path.with_suffix('.png'))
result.check_returncode()
