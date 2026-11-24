#!/usr/bin/env bash
# make_fonts.sh - builds the Inter fonts used by the UI (src/head/fonts/*.c).
# Inter (SIL Open Font License) is the typeface of the Chiku UI kit. Icons (LV_SYMBOL_*) are not in Inter:
# every font falls back to a built-in Montserrat font that has them.
# Needs Node.js. Run from anywhere:  bash Chiku/Firmware/tools/make_fonts.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$HERE/../src/head/fonts"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
curl -sL -o "$TMP/inter.zip" https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
unzip -q "$TMP/inter.zip" -d "$TMP"
TTF="$TMP/extras/ttf"
# ASCII + degree, middle dot, en/em dash, bullet, ellipsis, arrows
TEXT_RANGE="0x20-0x7E,0xB0,0xB7,0x2013,0x2014,0x2022,0x2026,0x2190-0x2195"
conv() {   # name weight size range fallback
  npx -y lv_font_conv@1.5.3 --no-compress --no-prefilter --bpp 4 --format lvgl \
    --font "$TTF/Inter-$2.ttf" --size "$3" -r "$4" --lv-font-name "$1" --lv-fallback "$5" \
    --lv-include lvgl.h -o "$OUT/$1.c"
}
mkdir -p "$OUT"
conv inter_14 Medium   14 "$TEXT_RANGE" lv_font_montserrat_14
conv inter_16 Medium   16 "$TEXT_RANGE" lv_font_montserrat_14
conv inter_20 SemiBold 20 "$TEXT_RANGE" lv_font_montserrat_20
conv inter_28 Bold     28 "$TEXT_RANGE" lv_font_montserrat_28
conv inter_44 Bold     44 "$TEXT_RANGE" lv_font_montserrat_48
conv inter_72 Bold     72 "0x20,0x25,0x2B-0x3A,0x43,0xB0" lv_font_montserrat_48   # digits for clock / timer
echo "fonts written to $OUT"
