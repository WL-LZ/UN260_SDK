#!/usr/bin/env python3
"""Two deterministic opaque 1280x400 surfaces. Host-only, no text or controls."""
from pathlib import Path
from PIL import Image
import math, io

root=Path(__file__).resolve().parents[1]
out=root/'aic_ui/lvgl_data/backgrounds'
out.mkdir(parents=True,exist_ok=True)
# Flatten the approved sea-mist HTML's two CSS overlays at build time.
# Keep the source image separate so repeated builds never compound whitening.
# The reference contains fine bitmap grain. Low-pass it before scaling to the
# panel; preserve the gradient while keeping early-boot and update payloads small.
im=Image.open(root/'tools/assets/sea_mist_source.png').convert('RGB').resize((160,50), Image.BOX).resize((1280,400), Image.BILINEAR)
px=im.load()
for y in range(400):
    for x in range(1280):
        t=(x+.5)/1280
        white=.42+(.40-.42)*t/.48 if t<.48 else .40+(.30-.40)*min((t-.48)/.32,1)
        c=[v*(1-white)+255*white for v in px[x,y]]
        radius=math.hypot((x+.5-1280*.94)/(1280*.94*math.sqrt(2)),(y+.5-400*.08)/(400*.92*math.sqrt(2)))
        alpha=.28*max(0,1-radius/.62)
        px[x,y]=tuple(round(v*(1-alpha)+w*alpha) for v,w in zip(c,(225,233,241)))
for name in ('boot',):
    buf=io.BytesIO();im.save(buf,format='PNG');data=buf.getvalue()
    path=out/(name+'.png')
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    print(name,im.size,len(data),'bytes, RGB opaque')

# Original user-interface surfaces, kept independent from the boot palette.
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

# Independent fourth built-in standby photo; do not alter its palette.
palettes={'mist': ((222,229,238),(231,227,235),(236,228,224),.76)}
for name,(left,middle,right,stop) in palettes.items():
    im=Image.new('RGB',(1280,400));px=im.load()
    for y in range(400):
        for x in range(1280):
            dx=math.sin(math.radians(115));dy=-math.cos(math.radians(115))
            t=.5+((x+.5-640)*dx+(y+.5-200)*dy)/(1280*dx+400*dy)
            a,b,f=(left,middle,t/stop) if t<=stop else (middle,right,(t-stop)/(1-stop))
            c=[v+(w-v)*f for v,w in zip(a,b)]
            if name=='mist':
                radius=math.hypot((x+.5)/(1280*math.sqrt(2)),(y+.5-400)/(400*math.sqrt(2)))
                alpha=max(0,1-radius/.78)
                c=[v*(1-alpha)+w*alpha for v,w in zip(c,(205,217,231))]
            px[x,y]=tuple(round(v) for v in c)
    buf=io.BytesIO();im.save(buf,format='PNG');data=buf.getvalue()
    path=(root/'aic_ui/lvgl_data/standby/mist.png') if name=='mist' else out/(name+'.png')
    path.parent.mkdir(parents=True,exist_ok=True)
    if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    print(name,im.size,len(data),'bytes, RGB opaque')
