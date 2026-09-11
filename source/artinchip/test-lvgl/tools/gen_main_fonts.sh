#!/usr/bin/env bash
set -euo pipefail
# Optional maintenance tool; normal firmware builds use committed generated C.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -n "${LV_FONT_CONV:-}" ]]; then
  read -r -a LV_FONT_CONV_CMD <<<"$LV_FONT_CONV"
elif command -v lv_font_conv >/dev/null 2>&1; then
  LV_FONT_CONV_CMD=(lv_font_conv)
else
  LV_FONT_CONV_CMD=(npx --yes lv_font_conv)
fi
NOTO_SANS_URL="https://github.com/notofonts/noto-fonts/raw/main/hinted/ttf/NotoSans/NotoSans-Regular.ttf"
NOTO_SANS_FONT="${UN260_NOTO_SANS_FONT:-$(mktemp)}"
if [[ -z "${UN260_NOTO_SANS_FONT:-}" ]]; then
  trap 'rm -f "$NOTO_SANS_FONT"' EXIT
  curl -fsSL "$NOTO_SANS_URL" -o "$NOTO_SANS_FONT"
fi
"${LV_FONT_CONV_CMD[@]}" --no-compress --no-prefilter --bpp 4 --size 64 \
  --font "$ROOT_DIR/aic_ui/font/Manrope-ExtraBold.ttf" \
  -r 0x20,0x2C-0x39 --format lvgl --lv-font-name lv_font_main_numeric_64 \
  -o "$ROOT_DIR/un260/font/lv_font_main_numeric_64.c"
currency_font() {
  local size="$1"
  local name="$2"
  "${LV_FONT_CONV_CMD[@]}" --no-compress --no-prefilter --bpp 4 --size "$size" \
  --font "$ROOT_DIR/aic_ui/font/DejaVuSans-CurrencySource.ttf" \
  -r 0x20-0x7E,0xA3,0xA5,0x405,0x41C,0x564,0x580,0x20A8-0x20AA,0x20AC,0x20B1,0x20B4,0x20B9-0x20BA,0x20BD,0xE3F,0x627,0x62C,0x62F,0x631,0x633,0x639,0x642-0x645 \
  --font /usr/share/fonts/truetype/freefont/FreeSerif.ttf -r 0xFDFC \
  --font "$NOTO_SANS_FONT" -r 0x20BC \
  --format lvgl --lv-font-name "$name" \
  -o "$ROOT_DIR/un260/font/$name.c"
}
currency_font 32 lv_font_main_currency_32
currency_font 56 lv_font_main_currency_56
# lv_font_conv 1.5 emits a field introduced after LVGL 8.3.
sed -i '/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$/d' \
  "$ROOT_DIR/un260/font/lv_font_main_numeric_64.c" \
  "$ROOT_DIR/un260/font/lv_font_main_currency_32.c" \
  "$ROOT_DIR/un260/font/lv_font_main_currency_56.c"
