#!/usr/bin/env python3
"""Render registered locale font candidates with the project's actual LVGL 8.3.

This tests glyph loading/shaping and sample bounds, not full page acceptance.
"""
import argparse
import hashlib
import json
import os
import subprocess
import shutil
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from PIL import Image
from test_i18n_support import lvgl_source
import gen_i18n

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('fonts', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.fonts = args.fonts.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    data = json.loads((args.fonts / 'manifest.json').read_text())
    registry, catalogues = gen_i18n.catalogues()
    languages = [item for item in registry if item['id'] != 0]
    canvas_height = ((len(languages) + 3) // 4) * 400
    lvgl = lvgl_source(ROOT)
    work = args.output / 'build'
    work.mkdir(exist_ok=True)
    conf = work / 'lv_conf.h'
    conf.write_text('''#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (24U*1024U*1024U)
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_LOG 0
#define LV_USE_BIDI 1
#define LV_USE_ARABIC_PERSIAN_CHARS 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_USE_FS_STDIO 1
#define LV_FS_STDIO_LETTER 'S'
#define LV_FS_STDIO_PATH ""
#endif
''')
    literal = lambda text: json.dumps(str(text), ensure_ascii=False)
    checks, panels, bounds = [], [], []
    # Explicit measurements from production controls, including 16 px side
    # clearance. These are text-fit checks, not a replacement for page renders.
    controls = (
        ('Back', 16, 60, 'lv_nav_back_text_button_create'),
        ('Cancel', 16, 84, 'language footer'),
        ('Save', 16, 98, 'language footer'),
        ('Import count', 15, 112, 'cash entries toolbar'),
        ('Import count', 15, 108, 'exchange import action'),
        ('Receipt', 15, 92, 'cash balance actions'),
        ('Payment', 15, 92, 'cash balance actions'),
        ('Exchange rate', 15, 112, 'calculator action'),
        ('Record cash', 15, 112, 'calculator action'),
        ('Record original cash', 15, 168, 'exchange action'),
        ('Actual exchange', 15, 178, 'exchange action'),
        ('Calculator', 24, 260, 'cash page heading'),
        ('Cash journal', 24, 260, 'cash page heading'),
        ('Operators', 24, 360, 'operator page heading'),
        ('OFF', 37, 107, 'batch preset card'),
    )
    keys = ('Language', 'Count mode', 'Reject pocket', 'Amount', 'Date & time', 'Confirm')
    for n, locale in enumerate(languages):
        tag = locale['tag']
        item = data['locales'][tag]
        points = ','.join(str(point) for point in item['codepoints'])
        for font in item['files']:
            font_points = font.get('codepoints', item['codepoints'])
            points = ','.join(str(point) for point in font_points)
            path = 'S:' + str(args.fonts / tag / font['file'])
            checks.append('check_font(%s, (uint32_t[]){%s}, %d);' % (literal(path), points, len(font_points)))
        path = 'S:' + str(args.fonts / tag / 'font-20.bin')
        panels.append('f[%d]=lv_font_load(%s);assert(f[%d]);f[%d]->fallback=&lv_font_montserrat_20;' % (n, literal(path), n, n))
        for row, value in enumerate([locale['name'], *(catalogues[tag][key] for key in keys), '1,680.00 USD']):
            panels.append('sample(%d,%d,%s,f[%d],%s);' % (n, row, literal(value), n, 'LV_BASE_DIR_RTL' if locale['direction'] == 'rtl' and row < 7 else 'LV_BASE_DIR_LTR'))
        for key, size, width, owner in controls:
            value = catalogues[tag].get(key)
            if not isinstance(value, str):
                raise ValueError('Missing measured control translation: %s %s' % (tag, key))
            path = 'S:' + str(args.fonts / tag / ('font-%d.bin' % (16 if size == 15 else size)))
            face = 'lv_font_instrument_sans_%s_%d' % ('semibold' if size in (24,37) else 'medium', size)
            bounds.append('check_control(%s,%s,%s,%s,%d,&%s);' % (
                literal(path), literal(tag), literal(owner + ': ' + key), literal(value), width, face))
    source = work / 'probe.c'
    source.write_text('''#include "lvgl.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "un260/font/ui_font_fallback.h"
LV_FONT_DECLARE(lv_font_instrument_sans_medium_15);
LV_FONT_DECLARE(lv_font_instrument_sans_medium_16);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_24);
LV_FONT_DECLARE(lv_font_instrument_sans_semibold_37);
static lv_color_t pixels[1280*CANVAS_HEIGHT], buffer[1280*40];
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *p){
 for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)pixels[y*1280+x]=*p++;
 lv_disp_flush_ready(d);
}
static void check_font(const char *path,uint32_t *points,unsigned count){
 lv_font_t *font=lv_font_load(path);assert(font);
 for(unsigned i=0;i<count;i++){lv_font_glyph_dsc_t glyph;
  if(!lv_font_get_glyph_dsc(font,&glyph,points[i],0)){fprintf(stderr,"Missing %s U+%04X\\n",path,points[i]);abort();}}
 lv_font_free(font);
}
static unsigned overflows;
static void check_control(const char *path,const char *tag,const char *owner,const char *text,int width,const lv_font_t *latin){
 lv_font_t *font=lv_font_load(path);assert(font);
 lv_font_t wrapped;ui_font_compose_fallback(&wrapped,latin,font);
 lv_obj_t *label=lv_label_create(lv_scr_act());
 lv_obj_set_style_text_font(label,&wrapped,0);lv_label_set_text(label,text);lv_obj_update_layout(label);
 int actual=lv_obj_get_width(label);
 if(actual>width){fprintf(stderr,"Control overflow: %s | %s | %s | %d > %d\\n",tag,owner,text,actual,width);overflows++;}
 const char *shaped=lv_label_get_text(label);uint32_t offset=0;
 while(shaped[offset]){uint32_t cp=_lv_txt_encoded_next(shaped,&offset);lv_font_glyph_dsc_t glyph;
  if(cp<=32)continue;
  if(!lv_font_get_glyph_dsc(&wrapped,&glyph,cp,0)){fprintf(stderr,"Control missing glyph: %s U+%04X\\n",tag,cp);overflows++;continue;}
  int top=wrapped.line_height-wrapped.base_line-glyph.box_h-glyph.ofs_y;
  if(top<0||top+glyph.box_h>wrapped.line_height){fprintf(stderr,"Control vertical overflow: %s | %s | U+%04X | %d..%d / %d\\n",tag,owner,cp,top,top+glyph.box_h,wrapped.line_height);overflows++;}
 }
 lv_obj_del(label);lv_font_free(font);
}
static void sample(int column,int row,const char *text,lv_font_t *font,lv_base_dir_t direction){
 lv_obj_t *label=lv_label_create(lv_scr_act());
 lv_obj_set_style_text_font(label,font,0);lv_obj_set_style_text_color(label,lv_color_hex(0x233641),0);
 lv_obj_set_style_base_dir(label,direction,0);lv_label_set_text(label,text);
 lv_obj_set_pos(label,24+(column%4)*316,20+row*46+(column/4)*400);lv_obj_update_layout(label);
 if(lv_obj_get_width(label)>284||lv_obj_get_height(label)>42){fprintf(stderr,"Sample bounds: %s %dx%d\\n",text,lv_obj_get_width(label),lv_obj_get_height(label));abort();}
 /* Check the shaped label text rather than only the source Unicode text. */
 const char *shaped=lv_label_get_text(label);uint32_t offset=0;
 while(shaped[offset]){uint32_t cp=_lv_txt_encoded_next(shaped,&offset);lv_font_glyph_dsc_t glyph;
  if(cp>32&&!lv_font_get_glyph_dsc(font,&glyph,cp,0)){fprintf(stderr,"Missing shaped glyph U+%04X\\n",cp);abort();}}
}
int main(int argc,char **argv){assert(argc==2);lv_init();
 lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buffer,NULL,1280*40);
 lv_disp_drv_t driver;lv_disp_drv_init(&driver);driver.hor_res=1280;driver.ver_res=CANVAS_HEIGHT;driver.draw_buf=&db;driver.flush_cb=flush;lv_disp_drv_register(&driver);
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0xf2f5f8),0);
''' + '\n'.join(checks) + '\n' + '\n'.join(bounds) + '\nlv_font_t *f[LOCALE_COUNT];\n' + '\n'.join(panels) + '''
 lv_refr_now(NULL);FILE *out=fopen(argv[1],"wb");assert(out);assert(fwrite(pixels,sizeof(pixels),1,out)==1);fclose(out);
 lv_obj_clean(lv_scr_act());for(int i=0;i<LOCALE_COUNT;i++)lv_font_free(f[i]);
 if(overflows){fprintf(stderr,"%u control text-fit failures\\n",overflows);return 1;}
 puts("PASS all candidate glyphs, Arabic shaped samples, sample bounds and measured control text fit");return 0;}
''', encoding='utf-8')
    sources = [source, *(ROOT / 'un260/font' / (name + '.c') for name in (
        'lv_font_instrument_sans_medium_15', 'lv_font_instrument_sans_medium_16',
        'lv_font_instrument_sans_semibold_24', 'lv_font_instrument_sans_semibold_37')), *sorted((lvgl / 'src').rglob('*.c'))]
    flags = ['gcc', '-std=gnu11', '-O1', '-g', '-fsanitize=address,undefined',
             '-fno-sanitize-recover=all', '-DCANVAS_HEIGHT='+str(canvas_height), '-DLOCALE_COUNT='+str(len(languages)), '-DLV_LVGL_H_INCLUDE_SIMPLE', '-I'+str(ROOT), '-I'+str(lvgl), '-DLV_CONF_PATH='+str(conf.resolve())]
    def build(pair):
        number, file = pair
        obj = work / ('%d.o' % number)
        digest = hashlib.sha256(file.read_bytes()+conf.read_bytes()+str(flags).encode()).hexdigest()
        stamp = obj.with_suffix('.sha')
        if not obj.exists() or not stamp.exists() or stamp.read_text() != digest:
            subprocess.run([*flags, '-c', str(file), '-o', str(obj)], check=True)
            stamp.write_text(digest)
        return obj
    with ThreadPoolExecutor(max_workers=4) as pool:
        objects = list(pool.map(build, enumerate(sources)))
    exe = work / 'probe'
    subprocess.run([*flags, *map(str, objects), '-lm', '-o', str(exe)], check=True)
    # Exercise the production bank with copies, including damaged bytes and
    # an injected allocation/load failure. Never mutate staged runtime assets.
    bank_assets = args.output / 'bank-assets'
    shutil.copytree(ROOT / 'aic_ui/lvgl_data/locale_fonts', bank_assets, dirs_exist_ok=True)
    bank_flags = [*flags, '-DUN260_LOCALE_FONT_DIR="S:'+str(bank_assets)+'/"']
    bank_objects = []
    for name in ('un260/font/ui_locale_font_bank.c', 'tools/test_locale_font_bank.c'):
        obj = work / (Path(name).stem + '.o')
        subprocess.run([*bank_flags, '-c', str(ROOT / name), '-o', str(obj)], check=True)
        bank_objects.append(obj)
    bank_exe = work / 'bank'
    subprocess.run([*bank_flags, *map(str, [*objects[1:], *bank_objects]),
                    '-Wl,--wrap=lv_font_load', '-Wl,--wrap=lv_font_free',
                    '-lm', '-lz', '-o', str(bank_exe)], check=True)
    subprocess.run([str(bank_exe), str(bank_assets)], check=True)
    # Save and restore in distinct processes using the actual storage and font
    # preflight implementations. Registry fixture does not release the product.
    (work / 'i18n/generated').mkdir(parents=True, exist_ok=True)
    (work / 'i18n/generated/ui_locale_registry.inc').write_text(
        (ROOT / 'i18n/generated/ui_locale_registry.inc').read_text().replace('false','true'))
    restart_lang=work / 'ui_lang.c'
    restart_lang.write_text((ROOT / 'un260/lv_system/ui_lang.c').read_text())
    restart_flags=[*bank_flags, '-D_GNU_SOURCE', '-I'+str(ROOT / 'un260/lv_system'),
                   '-DUI_STATE_DIR="'+str((args.output / 'restart-state').resolve())+'"']
    restart_objects=[]
    for source_file in (restart_lang, ROOT / 'tools/test_locale_restart.c',
                        ROOT / 'un260/storage/ui_locale_store.c', ROOT / 'i18n/generated/lv_i18n.c'):
        obj=work / ('restart-'+source_file.stem+'.o')
        subprocess.run([*restart_flags, '-c', str(source_file), '-o', str(obj)], check=True)
        restart_objects.append(obj)
    restart_exe=work / 'restart'
    subprocess.run([*restart_flags, *map(str,[*objects[1:],bank_objects[0],*restart_objects]),
                    '-lm','-lz','-o',str(restart_exe)],check=True)
    for locale in registry:
        subprocess.run([str(restart_exe),'save',locale['tag']],check=True)
        subprocess.run([str(restart_exe),'restore',locale['tag']],check=True)
    raw = args.output / 'four-language-fonts.bgra'
    subprocess.run([str(exe), str(raw)], check=True)
    Image.frombytes('RGBA', (1280,canvas_height), raw.read_bytes(), 'raw', 'BGRA').convert('RGB').save(args.output / 'four-language-fonts.png')
    (args.output / 'font-check.json').write_text(json.dumps(dict(
        locales=[item['tag'] for item in languages], glyphs='passed',
        shaping_samples='passed', measured_controls='passed',
        full_page_acceptance=False), indent=2)+'\n')


if __name__ == '__main__':
    main()
