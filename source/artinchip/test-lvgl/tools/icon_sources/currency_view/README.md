# Currency view icon sources

These four transparent 66x66 PNG files are the assets used by the reviewed
Currency HTML prototype. Their geometry comes from Lucide 1.8.0:

| Source PNG | Lucide icon | Output | Color |
| --- | --- | --- | --- |
| `mode-grid.png` | `LayoutGrid` | 16x16 | `#516879` |
| `mode-card.png` | `GalleryHorizontal` | 18x18 | `#516879` |
| `star-off.png` | `Star`, outline | 16x16 | `#98a8b4` |
| `star-on.png` | `Star`, filled | 16x16 | `#185bc2` |

The prototype's `mode-icons.cjs` rasterized Lucide's 24x24 viewBox with a 1.6
stroke width and round line caps/joins at 66x66. These saved PNGs preserve that
reviewed geometry. Lucide project: https://lucide.dev/ and
https://github.com/lucide-icons/lucide. The complete bundled copyright and license
notices are retained in `LICENSE.txt`.

Regenerate offline from the `test-lvgl` root with Python 3 and Pillow:

```sh
python3 tools/gen_curr_view_icons.py
```

The tool uses Lanczos coverage downsampling and a BGRA palette with 256 alpha
levels, matching the existing `page_07_curr_star.h` indexed-8 format. The four
descriptors live in `page_07_curr_view_icons.h`. The existing 24px Card stars
remain separate. The build consumes the generated header and does not require
Pillow or an SVG/Node toolchain.

Source SHA-256:

```text
3caed260230601fb211bb1b618ea9cedfd7d3fc39d13817f315e98ced05b85ea  mode-grid.png
d8337f9bdb74e32e3f4492b8fe941560d43cc5522e0efe4eb2395a9e29077cae  mode-card.png
c9e3d734172875f9b53e167b34f2d0fe7ab3729aaf495d6375c20efd7721be67  star-off.png
44caa5ed513dced304f50df9d53df99e9fa392ab8320ef83086723e3998760d1  star-on.png
```
