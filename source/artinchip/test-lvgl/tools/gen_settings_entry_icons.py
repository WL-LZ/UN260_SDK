#!/usr/bin/env python3
"""Settings entry artwork: 24px, 1.7px rounded strokes, transparent background.
Vector primitives are the source of truth; SVG masters and antialiased PNGs
are generated together without browser/font/network dependencies.
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
ICONS = {
 'Receipt': [('r',5,2,19,22),('l',8,7,16,7),('l',8,11,16,11),('l',8,15,12,15),('l',15,18,16,18)],
 'Standby': [('r',3,3,21,18),('l',8,22,16,22),('l',12,18,12,22),('c',15,8,2),('p',5,15,9,11,13,15,17,12,19,15)],
 'Reject': [('p',3,14,6,20,18,20,21,14),('l',3,14,8,14),('l',16,14,21,14),('p',12,3,12,14,9,11),('l',12,14,15,11)],
 'DoubleNote': [('r',3,3,17,14),('p',7,17,7,21,21,21,21,7,20,7),('l',6,7,14,7),('l',6,10,11,10)],
 'Serial': [('r',2,5,22,19),('l',6,8,6,16),('l',9,8,9,16),('l',13,8,13,16),('l',16,8,16,16),('l',19,8,19,16)],
 'Calibration': [('c',12,12,6),('c',12,12,2),('l',12,2,12,6),('l',12,18,12,22),('l',2,12,6,12),('l',18,12,22,12)],
 'Cis': [('r',3,5,21,19),('l',6,9,18,9),('l',6,12,18,12),('l',6,15,18,15),('l',8,2,8,5),('l',16,19,16,22)],
 'Balance': [('c',12,12,8),('l',12,4,12,20),('l',4,12,20,12),('c',8,8,1),('c',16,16,1)],
 'Motor': [('r',4,6,18,19),('l',8,3,14,3),('l',8,3,8,6),('l',14,3,14,6),('l',18,10,22,10),('l',18,15,22,15),('c',11,12,3)],
 'Sensor': [('r',5,5,19,19),('c',12,12,3),('l',8,2,8,5),('l',16,2,16,5),('l',8,19,8,22),('l',16,19,16,22),('l',2,8,5,8),('l',19,16,22,16)],
 'Flap': [('l',3,5,21,5),('l',3,19,21,19),('p',5,12,11,12,18,8),('c',11,12,1.5)],
 'Image': [('r',3,3,21,21),('c',8,8,2),('p',4,18,10,12,14,16,18,10,21,14)],
 'Wave': [('l',3,3,3,21),('l',3,21,22,21),('p',5,13,8,13,10,6,14,18,17,10,21,10)],
 'Aging': [('c',12,13,8),('l',9,2,15,2),('p',12,8,12,13,16,15)],
 'Display': [('r',2,3,22,18),('l',7,22,17,22),('l',12,18,12,22),('l',7,7,7,14),('l',12,7,12,14),('l',17,7,17,14)],
 'Collect': [('r',3,3,21,21),('l',7,8,10,8),('l',7,12,10,12),('p',16,7,16,17,13,14),('l',16,17,19,14)],
 'Upgrade': [('p',4,15,4,21,20,21,20,15),('p',7,8,12,3,17,8),('l',12,3,12,16)],
 'Board': [('r',3,3,21,21),('r',8,8,16,16),('l',5,6,6,6),('l',18,18,19,18),('l',12,3,12,8),('l',16,12,21,12),('l',8,12,3,12),('l',12,16,12,21)],
 'ImageBoard': [('r',3,3,21,21),('r',7,7,17,17),('c',10,10,1),('p',7,16,11,12,14,15,17,11)],
 'UiUpdate': [('r',2,3,22,18),('l',7,22,17,22),('p',8,11,12,7,16,11),('l',12,7,12,15)],
 'Version': [('r',3,3,21,21),('c',8,8,1),('l',12,8,17,8),('c',8,12,1),('l',12,12,17,12),('c',8,16,1),('l',12,16,17,16)],
 'Password': [('r',4,10,20,22),('p',7,10,7,6,9,3,15,3,17,6,17,10),('c',12,15,1.5),('l',12,16,12,19)],
 'Debug': [('p',4,6,9,11,4,16),('l',12,17,20,17),('r',1,2,23,22)],
 'Reset': [('p',3,9,5,5,9,3,15,3,20,7,21,13,18,19,13,21,7,19),('p',3,3,3,9,9,9)],
}

def main():
    pngdir=ROOT/'aic_ui/lvgl_data/settings_icons'
    svgdir=ROOT/'tools/settings_entry_icons'
    pngdir.mkdir(parents=True,exist_ok=True);svgdir.mkdir(parents=True,exist_ok=True)
    scale=6;stroke=1.7;ink=(83,107,121,255)
    for name,shapes in ICONS.items():
        im=Image.new('RGBA',(24*scale,24*scale));d=ImageDraw.Draw(im);svg=[]
        for kind,*v in shapes:
            coords=[a*scale for a in v]
            if kind=='r':
                x0,y0,x1,y1=coords;r=2*scale;w=round(stroke*scale)
                d.line([(x0+r,y0),(x1-r,y0)],fill=ink,width=w)
                d.line([(x0+r,y1),(x1-r,y1)],fill=ink,width=w)
                d.line([(x0,y0+r),(x0,y1-r)],fill=ink,width=w)
                d.line([(x1,y0+r),(x1,y1-r)],fill=ink,width=w)
                for box,start in [((x0,y0,x0+2*r,y0+2*r),180),((x1-2*r,y0,x1,y0+2*r),270),((x1-2*r,y1-2*r,x1,y1),0),((x0,y1-2*r,x0+2*r,y1),90)]:
                    d.arc(box,start,start+90,fill=ink,width=w)
                x,y,x2,y2=v;svg.append(f'<rect x="{x}" y="{y}" width="{x2-x}" height="{y2-y}" rx="2"/>')
            elif kind=='c':
                x,y,r=v;d.ellipse([(x-r)*scale,(y-r)*scale,(x+r)*scale,(y+r)*scale],outline=ink,width=round(stroke*scale))
                svg.append(f'<circle cx="{x}" cy="{y}" r="{r}"/>')
            else:
                points=list(zip(coords[::2],coords[1::2]));d.line(points,fill=ink,width=round(stroke*scale),joint='curve')
                for x,y in points:
                    r=stroke*scale/2;d.ellipse((x-r,y-r,x+r,y+r),fill=ink)
                svg.append('<polyline points="'+' '.join(f'{v[i]},{v[i+1]}' for i in range(0,len(v),2))+'"/>')
        im.resize((24,24),Image.Resampling.LANCZOS if hasattr(Image,'Resampling') else Image.LANCZOS).save(pngdir/f'Entry{name}.png')
        (svgdir/f'Entry{name}.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><g fill="none" stroke="#536b79" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">'+''.join(svg)+'</g></svg>\n')
    print(f'Generated {len(ICONS)} settings entry icons (24px PNG + SVG masters).')
if __name__=='__main__':main()
