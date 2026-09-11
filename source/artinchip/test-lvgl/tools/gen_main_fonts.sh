#!/usr/bin/env bash
set -euo pipefail
# Optional maintenance tool; normal firmware builds use committed generated C.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LV_FONT_CONV="${LV_FONT_CONV:-lv_font_conv}"
"$LV_FONT_CONV" --no-compress --no-prefilter --bpp 4 --size 64 \
  --font "$ROOT_DIR/aic_ui/font/Manrope-ExtraBold.ttf" \
  -r 0x20,0x2C-0x39 --format lvgl --lv-font-name lv_font_main_numeric_64 \
  -o "$ROOT_DIR/un260/font/lv_font_main_numeric_64.c"
"$LV_FONT_CONV" --no-compress --no-prefilter --bpp 4 --size 56 \
  --font "$ROOT_DIR/aic_ui/font/InstrumentSans-Medium.ttf" \
  -r 0x20-0x7E,0xA3,0xA5,0x20AC \
  --font "$ROOT_DIR/aic_ui/font/DejaVuSans-CurrencySource.ttf" \
  -r 0x20A9,0x20B1,0x20B9,0x20BA \
  --format lvgl --lv-font-name lv_font_main_currency_56 \
  -o "$ROOT_DIR/un260/font/lv_font_main_currency_56.c"
# lv_font_conv 1.5 emits a field introduced after LVGL 8.3.
sed -i '/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$/d' \
  "$ROOT_DIR/un260/font/lv_font_main_numeric_64.c" \
  "$ROOT_DIR/un260/font/lv_font_main_currency_56.c"
