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
CURRENCY_RANGE=$(python3 - "$ROOT_DIR" <<'PY'
from pathlib import Path
import json, re, sys
from fontTools.ttLib import TTFont
root = Path(sys.argv[1])
source = (root / 'un260/currency/currency_metadata.c').read_text(encoding='utf-8')
symbols = [json.loads(s) for s in re.findall(r'\{"[A-Z]{3}",\s*("(?:[^"\\]|\\.)*")\}', source)]
assert symbols, 'No currency metadata parsed'
assert len(symbols) == len(re.findall(r'\{"[A-Z]{3}",', source)), 'Unparsed currency metadata'
points = {ord(ch) for symbol in symbols for ch in symbol} - {0xFDFC, 0x20BC}
font = TTFont(root / 'aic_ui/font/DejaVuSans-CurrencySource.ttf')
missing = points - set(font.getBestCmap())
assert not missing, 'Currency source font lacks ' + repr(sorted(missing))
print('0x20-0x7E,0xFE80-0xFEFC,' + ','.join(hex(cp) for cp in sorted(points)))
PY
)
currency_font() {
  local size="$1"
  local name="$2"
  "${LV_FONT_CONV_CMD[@]}" --no-compress --no-prefilter --bpp 4 --size "$size" \
  --font "$ROOT_DIR/aic_ui/font/DejaVuSans-CurrencySource.ttf" \
  -r "$CURRENCY_RANGE" \
  --font /usr/share/fonts/truetype/freefont/FreeSerif.ttf -r 0xFDFC \
  --font "$NOTO_SANS_FONT" -r 0x20BC \
  --format lvgl --lv-font-name "$name" \
  -o "$ROOT_DIR/un260/font/$name.c"
}
currency_font 32 lv_font_main_currency_32
currency_font 56 lv_font_main_currency_56
python3 - "$ROOT_DIR" <<'PY'
from pathlib import Path
import sys
for size in (32, 56):
    path = Path(sys.argv[1]) / ('un260/font/lv_font_main_currency_%d.c' % size)
    path.write_text(path.read_text().rstrip() + '\n')
PY
# lv_font_conv 1.5 emits a field introduced after LVGL 8.3.
sed -i '/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$/d' \
  "$ROOT_DIR/un260/font/lv_font_main_numeric_64.c" \
  "$ROOT_DIR/un260/font/lv_font_main_currency_32.c" \
  "$ROOT_DIR/un260/font/lv_font_main_currency_56.c"
