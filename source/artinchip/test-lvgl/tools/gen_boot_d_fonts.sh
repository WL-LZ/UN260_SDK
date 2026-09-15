#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
for size in 12 16 18 36; do
 node "${LV_FONT_CONV_JS:?}" --no-compress --no-prefilter --no-kerning --bpp 4 --size "$size" --font aic_ui/font/Roboto-Regular.ttf -r 0x20-0x7E --format lvgl --lv-font-name "lv_font_roboto_$size" -o "un260/font/lv_font_roboto_$size.c"
 sed -i '/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$/d' "un260/font/lv_font_roboto_$size.c"
done
