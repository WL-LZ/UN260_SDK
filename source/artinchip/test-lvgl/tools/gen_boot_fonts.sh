#!/usr/bin/env bash
set -euo pipefail
# Maintenance only: generated C is checked in, never built on the device.
cd "$(dirname "$0")/.."
for size in 24 40 48; do
    node "${LV_FONT_CONV_JS:?Set path to lv_font_conv.js}" --no-compress \
        --no-prefilter --no-kerning --bpp 4 --size "$size" \
        --font aic_ui/font/OpenRunde-Medium.otf -r 0x20-0x7E \
        --format lvgl --lv-font-name "lv_font_open_runde_medium_$size" \
        -o "un260/font/lv_font_open_runde_medium_$size.c"
    sed -i '/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$/d' \
        "un260/font/lv_font_open_runde_medium_$size.c"
done
