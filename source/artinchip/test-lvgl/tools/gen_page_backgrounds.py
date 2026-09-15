#!/usr/bin/env python3
"""Two deterministic opaque 1280x400 surfaces. Host-only, no text or controls."""
from pathlib import Path
from PIL import Image
import math, io

root=Path(__file__).resolve().parents[1]
out=root/'aic_ui/lvgl_data/backgrounds'
out.mkdir(parents=True,exist_ok=True)
palettes={
    'user': ((237,241,249),(250,250,249),(211,221,246),(241,229,221)),
    'settings': ((238,246,244),(250,251,248),(201,229,221),(220,229,241)),
}
for name,(left,right,lower,upper) in palettes.items():
    im=Image.new('RGB',(1280,400));px=im.load()
    for y in range(400):
        for x in range(1280):
            t=min(x/1279*2,1)
            c=[a+(b-a)*t for a,b in zip(left,right)]
            a=.55*max(0,1-math.hypot((x-140)/750,(y-448)/460))
            b=.55*max(0,1-math.hypot((x-1230)/740,(y+60)/450))
            c=[int(v*(1-a)+k*a) for v,k in zip(c,lower)]
            px[x,y]=tuple(int(v*(1-b)+k*b) for v,k in zip(c,upper))
    buf=io.BytesIO();im.save(buf,format='PNG');data=buf.getvalue()
    path=out/(name+'.png')
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    print(name,im.size,len(data),'bytes, RGB opaque')
