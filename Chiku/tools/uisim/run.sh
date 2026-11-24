#!/usr/bin/env bash
# run.sh - build and run the Chiku UI simulator on a PC (Linux, macOS, WSL; needs gcc/g++ or clang).
# Uses the LVGL copy that PlatformIO downloads: run "pio run -d Chiku -e head" once before.
# Output: Chiku/tools/uisim/out/*.png (every mood, animation frame and screen) + rotation self-test.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CHIKU="$HERE/../.."
LVGL="$CHIKU/.pio/libdeps/head/lvgl"
[ -d "$LVGL/src" ] || { echo "LVGL not found: run 'pio run -d Chiku -e head' once"; exit 1; }
BUILD="$HERE/build"
mkdir -p "$BUILD/lvgl"
# rebuild everything when the LVGL configuration changed
if [ "$CHIKU/include/lv_conf.h" -nt "$BUILD/stamp" ]; then rm -f "$BUILD"/lvgl/*.o "$BUILD"/*.o; touch "$BUILD/stamp"; fi
INC="-I$HERE/shim -I$HERE -I$CHIKU/include -I$CHIKU/src/head -I$LVGL -DLV_CONF_INCLUDE_SIMPLE"
CC=${CC:-cc}; CXX=${CXX:-c++}
# LVGL (compiled once, cached in build/lvgl)
find "$LVGL/src" -name '*.c' | while read -r f; do
  o="$BUILD/lvgl/$(echo "${f#$LVGL/src/}" | tr '/' '_').o"
  [ "$o" -nt "$f" ] || echo "$f $o"
done | xargs -r -n 2 -P "$(nproc 2>/dev/null || echo 4)" sh -c "$CC -O2 -w $INC -c \"\$0\" -o \"\$1\""
HEAD="$CHIKU/src/head"
SRCS="$HEAD/face.cpp $HEAD/face_draw.cpp $HEAD/ui.cpp $(ls $HEAD/ui_*.cpp) $HERE/sim.cpp $HERE/stubs.cpp"
for f in $HEAD/fonts/*.c; do o="$BUILD/$(basename "$f").o"; [ "$o" -nt "$f" ] || $CC -O2 -w $INC -c "$f" -o "$o"; done
$CXX -std=gnu++17 -O1 -Wall -Wno-unused-function -Wno-unused-variable $INC $SRCS "$BUILD"/lvgl/*.o "$BUILD"/*.c.o -lm -o "$BUILD/uisim"
cd "$HERE" && "$BUILD/uisim" "$HERE/out"
