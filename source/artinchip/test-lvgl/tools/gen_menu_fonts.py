#!/usr/bin/env python3
"""Generate exact approved Menu sizes; pass the installed lv_font_conv JS path."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
for weight,sizes in [('menumedium',(14,)),('medium',(15,17,19)),('semibold',(17,19,21,25,26,29,37,50,52,55))]:
    face='Medium' if 'medium' in weight else 'SemiBold'
    for size in sizes:
        name=f'lv_font_instrument_sans_{weight}_{size}'
        glyphs='0x20-0x7E,0xB0,0xB7,0x2013,0x2019,0x2022'
        if weight=='semibold' and size in (37,50,52,55):glyphs='0x20,0x2C-0x2E,0x30-0x39,0x46,0x4E-0x4F,0x55'
        subprocess.run(['node',sys.argv[1],'--no-compress','--no-prefilter','--bpp','4','--size',str(size),'--font',str(root/f'aic_ui/font/InstrumentSans-{face}.ttf'),'-r',glyphs,'--format','lvgl','--lv-font-name',name,'-o',str(root/f'un260/font/{name}.c')],check=True)
