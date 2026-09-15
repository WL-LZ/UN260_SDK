#!/usr/bin/env python3
"""Offline fourth-theme surface and native sprites; no device font parsing."""
from pathlib import Path
import re,zlib,io,struct,runpy
from PIL import Image
root=Path(__file__).resolve().parents[1]
runpy.run_path(str(root/'tools/gen_selfcheck_icons.py'))
def glyphs(filename,text,color,width,height):
    s=(root/'un260/font'/filename).read_text()
    first=int(re.search(r'\.range_start = (\d+)',s)[1])
    data=s.split('glyph_bitmap[] = {',1)[1].split('};',1)[0]
    data=re.sub(r'/\*.*?\*/','',data,flags=re.S)
    data=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',data))
    rows=re.findall(r'\{\.bitmap_index = (\d+), .adv_w = (\d+), .box_w = (\d+), .box_h = (\d+), .ofs_x = (-?\d+), .ofs_y = (-?\d+)\}',s)
    gs=[tuple(map(int,rows[ord(c)-first+1])) for c in text]
    length=sum((g[1]+8)//16 for g in gs);x=(width-length)//2
    line=int(re.search(r'\.line_height = (\d+)',s)[1]);base=int(re.search(r'\.base_line = (\d+)',s)[1])
    im=Image.new('RGBA',(width,height))
    for index,adv,w,h,ox,oy in gs:
        for y in range(h):
            for xx in range(w):
                n=y*w+xx;b=data[index+n//2];a=((b>>4) if n%2==0 else b&15)*17
                px=x+ox+xx;py=line-base-h-oy+y
                if 0<=px<width and 0<=py<height:im.putpixel((px,py),(*color,a))
        x+=(adv+8)//16
    return im
out=root/'aic_ui/lvgl_data/boot_theme_d';out.mkdir(parents=True,exist_ok=True)
# Same generated Main surface; only static brand/footer ink is composited below.
runpy.run_path(str(root/'tools/gen_page_backgrounds.py'))
bg=Image.open(root/'aic_ui/lvgl_data/backgrounds/user.png').convert('RGBA')
# Bake static brand/footer once, at the same pixels for native and LVGL.
# Reuse the approved round-capped artwork, reduced offline with a coverage filter.
# Both renderers consume these same baked pixels; no runtime scaling or drawing.
resample=getattr(Image,'Resampling',Image).LANCZOS
emblem=Image.open(root/'aic_ui/lvgl_data/boot_theme_c/emblem.png').convert('RGBA')
brand_emblem=emblem.resize((24,24),resample)
bg.alpha_composite(brand_emblem,(48,27))
brand=glyphs('lv_font_roboto_16.c','UN260',(111,116,129),64,24);bg.alpha_composite(brand,(77,29))
footer=glyphs('lv_font_instrument_sans_semibold_12.c','CURRENCY PROCESSING SYSTEM',(146,152,168),220,20);bg.alpha_composite(footer,(52,363))
def save(im,path):
    data=io.BytesIO();im.save(data,format='PNG');data=data.getvalue()
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
save(bg,out/'background.png');blank=Image.new('RGBA',(96,96));save(emblem,out/'emblem.png')
welcome=glyphs('lv_font_boot_welcome.c','\ue000',(82,109,130),500,128)
pixels=bg.tobytes('raw','BGRA')+blank.tobytes('raw','BGRA')+Image.new('RGBA',(500,128)).tobytes('raw','BGRA')+welcome.tobytes('raw','BGRA')
value=2166136261
for b in pixels:value=((value^b)*16777619)&0xffffffff
payload=struct.pack('<8sII',b'UNBOOT1\0',len(pixels),value)+pixels
binary=root/'aic_ui/generated_assets/boot_theme_d/boot-light.bin'
binary.parent.mkdir(parents=True,exist_ok=True)
if not binary.exists() or binary.read_bytes()!=payload:binary.write_bytes(payload)
packed=zlib.compress(pixels,9)
header=root/'aic_ui/generated_assets/boot_light_d_packed.h';header.parent.mkdir(parents=True,exist_ok=True)
text='/* Generated fourth-theme immutable sprites. */\nstatic const unsigned char boot_light_packed[]={\n'+ '\n'.join(','.join(str(b) for b in packed[i:i+24])+',' for i in range(0,len(packed),24))+'\n};\n'
if not header.exists() or header.read_text()!=text:header.write_text(text)
print('D assets',len(pixels),'raw bytes',len(packed),'compressed bytes')
