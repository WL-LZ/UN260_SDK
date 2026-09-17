#!/usr/bin/env python3
"""Encode AUTO/MULTI PNGs without first-row Up filtering for the VE decoder.

This changes encoding only: RGBA pixels and dimensions must remain identical.
Run after updating these code-generated icon assets; --check verifies delivery.
"""
import argparse
import struct
import zlib
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
NAMES = ("CURR_AUTO.png", "main_icons/multi_card.png")

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

def encoded(image):
    w, h = image.size
    pixels = image.tobytes()
    rows = b"".join(b"\0" + pixels[y*w*4:(y+1)*w*4] for y in range(h))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    for name in NAMES:
        path = ROOT / "aic_ui/lvgl_data" / name
        with Image.open(path) as original:
            image = original.convert("RGBA")
        assert image.size == (182, 103), (name, image.size)
        data = encoded(image)
        if args.check:
            assert path.read_bytes() == data, "Run normalize_currency_png.py: " + name
        else:
            path.write_bytes(data)
        with Image.open(path) as check:
            assert check.convert("RGBA").tobytes() == image.tobytes()
        print("PASS PNG encoding, 182x103 RGBA, unchanged pixels:", name)

if __name__ == "__main__":
    main()
