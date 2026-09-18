#!/usr/bin/env python3
"""Theme D through the genuine LVGL renderer, plus all four macro link checks."""
from pathlib import Path
s=Path(__file__).with_name('test_boot_theme_c.py').read_text()
s=s.replace('"un260/lv_core/boot_anim/page_00_boot_anim_theme_c.c",','"un260/lv_core/boot_anim/page_00_boot_anim_theme_c.c",\n    "un260/lv_core/boot_anim/page_00_boot_anim_theme_d.c",\n    "un260/lv_core/boot_anim/page_08_boot_flow.c",')
s=s.replace('boot_theme_c/{name}.png','boot_theme_d/{name}.png').replace('for theme in (1, 2, 3):','for theme in (1, 2, 3, 4):')
s=s.replace('sources = [harness,','sources = [harness, root / "un260/lv_core/boot_anim/page_00_boot_anim_theme_d.c",')
s=s.replace('metadata = write_assets(root, work)', '''metadata = write_assets(root, work)
        with Image.open(root / "aic_ui/lvgl_data/backgrounds/boot.png") as ready:
            (work / "boot.bgra").write_bytes(ready.convert("RGBA").tobytes("raw", "BGRA"))''')
s=s.replace('"-DUI_BOOT_ANIM_THEME=3"','"-DUI_BOOT_ANIM_THEME=4"')
exec(compile(s,str(Path(__file__)), 'exec'))
