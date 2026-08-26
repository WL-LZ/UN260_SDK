#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   FONT_DIR=/abs/path/to/ttf ./tools/gen_pure_fonts.sh
#
# Required TTF files in FONT_DIR:
#   - Rajdhani-Bold.ttf
#   - Montserrat-ExtraBold.ttf
#   - Montserrat-Medium.ttf

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/un260/font"
if [ -n "${FONT_DIR:-}" ]; then
    FONT_DIR="$FONT_DIR"
elif [ -d "$ROOT_DIR/fonts" ]; then
    FONT_DIR="$ROOT_DIR/fonts"
else
    FONT_DIR="$ROOT_DIR/un260/font"
fi
LV_FONT_CONV="${LV_FONT_CONV:-npx -y lv_font_conv}"

need_file() {
    local f="$1"
    if [ ! -f "$f" ]; then
        echo "Missing font file: $f" >&2
        exit 1
    fi
}

need_file "$FONT_DIR/Rajdhani-Bold.ttf"
need_file "$FONT_DIR/Montserrat-ExtraBold.ttf"
need_file "$FONT_DIR/Montserrat-Medium.ttf"

mkdir -p "$OUT_DIR"

echo "[1/5] Generate MONTSERRAT_EXTRABOLD_35.c"
eval "$LV_FONT_CONV \
  --no-compress --no-prefilter --bpp 4 --size 25 \
  --font \"$FONT_DIR/Montserrat-ExtraBold.ttf\" \
  -r 0x20-0x7E \
  --format lvgl \
  --lv-font-name MONTSERRAT_EXTRABOLD_35 \
  -o \"$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c\""

echo "[2/5] Generate MONTSERRAT_MEDIUM_15.c"
eval "$LV_FONT_CONV \
  --no-compress --no-prefilter --bpp 4 --size 11 \
  --font \"$FONT_DIR/Montserrat-Medium.ttf\" \
  -r 0x20-0x7E \
  --format lvgl \
  --lv-font-name MONTSERRAT_Medium_15 \
  -o \"$OUT_DIR/MONTSERRAT_MEDIUM_15.c\""

echo "[3/5] Generate lv_font_rajdhani_27.c"
eval "$LV_FONT_CONV \
  --no-compress --no-prefilter --bpp 4 --size 17 \
  --font \"$FONT_DIR/Rajdhani-Bold.ttf\" \
  -r 0x20-0x7E \
  --format lvgl \
  --lv-font-name lv_font_rajdhani_27 \
  -o \"$OUT_DIR/lv_font_rajdhani_27.c\""

echo "[4/5] Generate lv_font_rajdhani_142.c"
eval "$LV_FONT_CONV \
  --no-compress --no-prefilter --bpp 4 --size 140 \
  --font \"$FONT_DIR/Rajdhani-Bold.ttf\" \
  --symbols 0123456789, \
  --format lvgl \
  --lv-font-name lv_font_rajdhani_142 \
  -o \"$OUT_DIR/lv_font_rajdhani_142.c\""

echo "[5/5] Generate lv_font_rajdhani_194.c"
eval "$LV_FONT_CONV \
  --no-compress --no-prefilter --bpp 4 --size 193 \
  --font \"$FONT_DIR/Rajdhani-Bold.ttf\" \
  --symbols 0123456789, \
  --format lvgl \
  --lv-font-name lv_font_rajdhani_194 \
  -o \"$OUT_DIR/lv_font_rajdhani_194.c\""

# Compatibility fixes for current project LVGL config/version.
for f in \
  "$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c" \
  "$OUT_DIR/MONTSERRAT_MEDIUM_15.c" \
  "$OUT_DIR/lv_font_rajdhani_27.c" \
  "$OUT_DIR/lv_font_rajdhani_142.c" \
  "$OUT_DIR/lv_font_rajdhani_194.c"
do
    sed -i 's/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$//' "$f"
done

# Avoid macro/identifier collision for MONTSERRAT_EXTRABOLD_35.
sed -i 's/#ifndef MONTSERRAT_EXTRABOLD_35/#ifndef LV_FONT_MONTSERRAT_EXTRABOLD_35/' \
  "$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c"
sed -i 's/#define MONTSERRAT_EXTRABOLD_35 1/#define LV_FONT_MONTSERRAT_EXTRABOLD_35 1/' \
  "$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c"
sed -i 's/#if MONTSERRAT_EXTRABOLD_35/#if LV_FONT_MONTSERRAT_EXTRABOLD_35/' \
  "$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c"
sed -i 's/#endif \/\*#if MONTSERRAT_EXTRABOLD_35\*\//#endif \/\*#if LV_FONT_MONTSERRAT_EXTRABOLD_35\*\//' \
  "$OUT_DIR/MONTSERRAT_EXTRABOLD_35.c"

echo "Done. Generated fonts in: $OUT_DIR"
