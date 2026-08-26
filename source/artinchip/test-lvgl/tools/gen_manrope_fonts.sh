#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   FONT_DIR=/abs/path/to/manrope ./tools/gen_manrope_fonts.sh
#
# Required TTF files in FONT_DIR:
#   - InstrumentSans-Bold.ttf
#   - InstrumentSans-Medium.ttf
#   - InstrumentSans-SemiBold.ttf
#   - Manrope-Bold.ttf
#   - Manrope-ExtraBold.ttf

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/un260/font"
FONT_DIR="${FONT_DIR:-$ROOT_DIR/aic_ui/font}"
LV_FONT_CONV="${LV_FONT_CONV:-npx -y lv_font_conv}"

SIZES=(10 12 14 16 18 20 22 24 28 30 32 40 42 44 48)
WEIGHTS=(
  "bold:Bold"
  "extrabold:ExtraBold"
)

INSTRUMENT_WEIGHTS=(
  "bold:Bold"
  "medium:Medium"
  "semibold:SemiBold"
)

need_file() {
    local f="$1"
    if [ ! -f "$f" ]; then
        echo "Missing font file: $f" >&2
        exit 1
    fi
}

for weight in "${WEIGHTS[@]}"; do
    ttf_name="${weight#*:}"
    need_file "$FONT_DIR/Manrope-$ttf_name.ttf"
done

for weight in "${INSTRUMENT_WEIGHTS[@]}"; do
    ttf_name="${weight#*:}"
    need_file "$FONT_DIR/InstrumentSans-$ttf_name.ttf"
done

mkdir -p "$OUT_DIR"

for weight in "${WEIGHTS[@]}"; do
    key="${weight%%:*}"
    ttf_name="${weight#*:}"
    ttf="$FONT_DIR/Manrope-$ttf_name.ttf"

    for size in "${SIZES[@]}"; do
        font_name="lv_font_manrope_${key}_${size}"
        out_file="$OUT_DIR/${font_name}.c"
        echo "Generate ${font_name}.c"
        eval "$LV_FONT_CONV \
          --no-compress --no-prefilter --bpp 4 --size $size \
          --font \"$ttf\" \
          -r 0x20-0x7E,0xB0,0x2022 \
          --format lvgl \
          --lv-font-name \"$font_name\" \
          -o \"$out_file\""
        sed -i 's/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$//' "$out_file"
    done
done

for weight in "${INSTRUMENT_WEIGHTS[@]}"; do
    key="${weight%%:*}"
    ttf_name="${weight#*:}"
    ttf="$FONT_DIR/InstrumentSans-$ttf_name.ttf"

    for size in "${SIZES[@]}"; do
        font_name="lv_font_instrument_sans_${key}_${size}"
        out_file="$OUT_DIR/${font_name}.c"
        echo "Generate ${font_name}.c"
        eval "$LV_FONT_CONV \
          --no-compress --no-prefilter --bpp 4 --size $size \
          --font \"$ttf\" \
          -r 0x20-0x7E,0xB0,0x2022 \
          --format lvgl \
          --lv-font-name \"$font_name\" \
          -o \"$out_file\""
        sed -i 's/^[[:space:]]*\.static_bitmap = 0,[[:space:]]*$//' "$out_file"
    done
done

echo "Done. Generated UI fonts in: $OUT_DIR"
