#!/usr/bin/env python3
"""Production PURE lifecycle, real locale fonts and existing PNG exit icon.

Optional rasters are host software rendering, not target-board DMA/touch evidence.
"""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse, hashlib, json, os, re, subprocess, tempfile
from PIL import Image
from test_i18n_support import lvgl_source, with_i18n
from test_list_view import compiled_asset_sources

root=Path(__file__).resolve().parents[1]
lvgl=lvgl_source(root)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output-dir',type=Path)
args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='un260-pure-lifecycle-') as temp:
    work=Path(temp)
    (work/'lvgl').mkdir()
    (work/'lvgl/lvgl.h').write_text(f'#include "{lvgl}/lvgl.h"\n')
    conf=work/'lv_conf.h'
    conf.write_text('#ifndef LV_CONF_H\n#define LV_CONF_H\n#define LV_COLOR_DEPTH 32\n#define LV_MEM_SIZE (24U*1024U*1024U)\n#define LV_USE_THEME_DEFAULT 1\n#define LV_USE_LOG 0\n#define LV_USE_SNAPSHOT 1\n#define LV_USE_BIDI 1\n#define LV_USE_ARABIC_PERSIAN_CHARS 1\n#define LV_USE_FS_STDIO 1\n#define LV_FS_STDIO_LETTER 83\n#define LV_FS_STDIO_PATH ""\n#define LV_FONT_MONTSERRAT_18 1\n#endif\n')
    sources=[root/'tools/test_pure_lifecycle.c',*compiled_asset_sources()]
    sources += [root/p for p in ['un260/font/ui_message_font.c','un260/font/ui_locale_font_bank.c','un260/lv_system/ui_text_widget.c','un260/lv_system/ui_text_page.c','un260/lv_components/lv_damped_button.c']]
    fonts={'MONTSERRAT_EXTRABOLD_35','MONTSERRAT_MEDIUM_15','lv_font_rajdhani_27','lv_font_rajdhani_142','lv_font_rajdhani_194'}
    fonts.update(re.findall(r'\blv_font_(?:instrument_sans_|message_cjk_)\w+', (root/'un260/font/ui_message_font.c').read_text()))
    conf.write_text(conf.read_text().replace('#endif', '#define LV_FONT_CUSTOM_DECLARE '+ ' '.join(f'LV_FONT_DECLARE({name});' for name in sorted(fonts))+'\n#endif'))
    sources += [root/'un260/font'/f'{name}.c' for name in sorted(fonts)]
    sources=with_i18n(sources,root)+list(lvgl.joinpath('src').rglob('*.c'))
    common=['cc','-std=gnu11','-O1','-g','-D_DEFAULT_SOURCE','-fsanitize=undefined','-fno-sanitize-recover=all',f'-I{work}',f'-I{root}',f'-I{lvgl}',f'-DLV_CONF_PATH={conf}', '-DUN260_LOCALE_FONT_DIR="S:'+str(root/'aic_ui/lvgl_data/locale_fonts')+'/"']
    objects=[work/f'{i}.o' for i in range(len(sources))]
    def build(pair):subprocess.run([*common,'-c',str(pair[0]),'-o',str(pair[1])],check=True)
    with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(build,zip(sources,objects)))
    exe=work/'test'
    subprocess.run([*common,*map(str,objects),'-lm','-lz','-o',str(exe)],check=True)
    for name in ('user','settings'):
        with Image.open(root/'aic_ui/lvgl_data/backgrounds'/(name+'.png')) as image:
            (work/(name+'.bgra')).write_bytes(image.convert('RGBA').tobytes('raw','BGRA'))
    env=os.environ.copy();env['UN260_BACKGROUND_DIR']=str(work)
    if args.output_dir:
        args.output_dir.mkdir(parents=True,exist_ok=True);env['PURE_RASTER_OUTPUT']=str(args.output_dir.resolve())
    subprocess.run([str(exe)],check=True,env=env)
    if args.output_dir:
        for ppm in args.output_dir.glob('pure-*.ppm'):
            with Image.open(ppm) as image:image.save(ppm.with_suffix('.png'))
