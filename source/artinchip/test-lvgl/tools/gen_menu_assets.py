#!/usr/bin/env python3
"""Render private Menu SVG masters using system librsvg, not a browser."""
from pathlib import Path
import ctypes as C
import ctypes.util
import re
from PIL import Image
root=Path(__file__).resolve().parents[1]
r=C.CDLL(ctypes.util.find_library('rsvg-2'))
c=C.CDLL(ctypes.util.find_library('cairo'))
g=C.CDLL(ctypes.util.find_library('gobject-2.0'))
r.rsvg_handle_new_from_data.argtypes=[C.c_char_p,C.c_size_t,C.c_void_p];r.rsvg_handle_new_from_data.restype=C.c_void_p
r.rsvg_handle_render_cairo.argtypes=[C.c_void_p,C.c_void_p];r.rsvg_handle_render_cairo.restype=C.c_int
c.cairo_image_surface_create.argtypes=[C.c_int,C.c_int,C.c_int];c.cairo_image_surface_create.restype=C.c_void_p
c.cairo_create.argtypes=[C.c_void_p];c.cairo_create.restype=C.c_void_p
c.cairo_scale.argtypes=[C.c_void_p,C.c_double,C.c_double]
c.cairo_surface_write_to_png.argtypes=[C.c_void_p,C.c_char_p];c.cairo_surface_write_to_png.restype=C.c_int
c.cairo_destroy.argtypes=[C.c_void_p];c.cairo_surface_destroy.argtypes=[C.c_void_p];g.g_object_unref.argtypes=[C.c_void_p]
out=root/'aic_ui/lvgl_data/menu_icons';out.mkdir(parents=True,exist_ok=True)
usage=set()
for code in (root/'un260/lv_core').glob('*'):
    if code.name=='page_03_menu.c' or code.name.startswith('menu_'):
        usage.update((name,int(size)) for name,size in re.findall(r'MICON\("([^"]+)",(\d+)\)',code.read_text()))
usage.update((name,20) for name in ('layers','profiles','options','history','repeat','search','shield','receipt','print','qr','user','hand','sun','reject','brush','screen'))
for source in sorted((root/'tools/menu_icons').glob('*.svg')):
    data=source.read_bytes();handle=r.rsvg_handle_new_from_data(data,len(data),None);assert handle,source
    for size in sorted(size for name,size in usage if name==source.stem):
        surface=c.cairo_image_surface_create(0,size,size);context=c.cairo_create(surface)
        c.cairo_scale(context,size/24,size/24);assert r.rsvg_handle_render_cairo(handle,context)
        target=out/f'{source.stem}_{size}.png';assert c.cairo_surface_write_to_png(surface,str(target).encode())==0
        c.cairo_destroy(context);c.cairo_surface_destroy(surface)
    g.g_object_unref(handle)
print('Menu SVG artwork generated at native optical sizes.')
for code in ('CNY','USD','EUR'):
    im=Image.open(root/f'aic_ui/lvgl_data/CURR_{code}.png').convert('RGBA')
    im.resize((30,round(im.height*30/im.width)),Image.LANCZOS).save(out/f'flag_{code}.png')
# Remove only this generator's unused variants, never other resource families.
for source in (root/'tools/menu_icons').glob('*.svg'):
    for size in (16,18,19,20,22,25,38):
        target=out/f'{source.stem}_{size}.png'
        if (source.stem,size) not in usage and target.exists():
            assert target.resolve().parent==out.resolve()
            target.unlink()
