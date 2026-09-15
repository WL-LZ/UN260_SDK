#!/usr/bin/env python3
"""Offline, solid startup surface: no gradient or runtime background shader."""
from pathlib import Path
from PIL import Image
root = Path(__file__).resolve().parents[1]
Image.new('RGB', (1280, 400), '#101418').save(
    root / 'aic_ui/lvgl_data/boot_theme_c/background.png')
