#!/usr/bin/env python3
"""Rasterize only two startup labels from the existing LVGL 4bpp glyphs."""
from pathlib import Path
import re,struct,zlib
from PIL import Image
root=Path(__file__).resolve().parents[1]
def label(filename,text,spacing):
    source=(root/'un260/font'/filename).read_text()
    assert '.kern_dsc = NULL' in source and '.bpp = 4' in source and '.bitmap_format = 0' in source
    assert '.range_start = 32, .range_length = 95, .glyph_id_start = 1' in source
    bitmap=source.split('glyph_bitmap[] = {',1)[1].split('};',1)[0]
    bitmap=re.sub(r'/\*.*?\*/','',bitmap,flags=re.S)
    data=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',bitmap))
    rows=re.findall(r'\{\.bitmap_index = (\d+), .adv_w = (\d+), .box_w = (\d+), .box_h = (\d+), .ofs_x = (-?\d+), .ofs_y = (-?\d+)\}',source)
    glyphs=[tuple(map(int,rows[ord(c)-31])) for c in text]
    width=sum((g[1]+8)//16 for g in glyphs)+spacing*(len(text)-1)
    height=int(re.search(r'\.line_height = (\d+)',source)[1])
    baseline=int(re.search(r'\.base_line = (\d+)',source)[1])
    image=Image.new('RGBA',(500,64));x=(500-width)//2
    for index,advance,w,h,ox,oy in glyphs:
        for y in range(h):
            for xx in range(w):
                n=y*w+xx;v=data[index+n//2]
                alpha=((v>>4) if n%2==0 else (v&15))*17
                image.putpixel((x+ox+xx,height-baseline-h-oy+y),(0x74,0x81,0x8a,alpha))
        x+=(advance+8)//16+spacing
    return image
base=root/'aic_ui/lvgl_data/boot_theme_c'
images=[Image.open(base/'background.png').convert('RGBA'),Image.open(base/'emblem.png').convert('RGBA'),
        label('lv_font_instrument_sans_semibold_48.c','UN260',1),
        label('lv_font_instrument_sans_medium_40.c','WELCOME',5)]
assert [im.size for im in images]==[(1280,400),(96,96),(500,64),(500,64)]
pixels=b''.join(im.tobytes('raw','BGRA') for im in images)
value=2166136261
for b in pixels:value=((value^b)*16777619)&0xffffffff
payload=struct.pack('<8sII',b'UNBOOT1\0',len(pixels),value)+pixels
path=base/'boot-light.bin'
if not path.exists() or path.read_bytes()!=payload:path.write_bytes(payload)
print('boot-light assets:',len(payload),'bytes; original LVGL glyphs, no font substitution')
packed=zlib.compress(pixels,9)
header=root/'aic_ui/generated_assets/boot_light_packed.h'
text='/* Generated from original artwork; zlib includes integrity checking. */\nstatic const unsigned char boot_light_packed[] = {\n'
text+='\n'.join(','.join(str(b) for b in packed[i:i+24])+',' for i in range(0,len(packed),24))+'\n};\n'
header.parent.mkdir(parents=True,exist_ok=True)
if not header.exists() or header.read_text()!=text:header.write_text(text)
print('boot-light embedded compressed bytes:',len(packed))
