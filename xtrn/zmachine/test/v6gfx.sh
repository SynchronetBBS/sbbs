#!/bin/sh
# v6 graphics pre-bake end-to-end: tools/blorb2gfx.js on the (copyrighted) Arthur Blorb.
# Verifies the tool produces a manifest + at least one PPM, and that picture 1's recorded
# dimensions match the PPM header convert actually wrote. Skips cleanly if the Blorb is absent.
HERE=$(cd "$(dirname "$0")" && pwd)
BLORB="${ARTHUR_BLB:-/tmp/Arthur.blb}"
[ -f "$BLORB" ] || { echo "V6 GFX SKIPPED (Blorb not found: $BLORB)"; exit 0; }
command -v convert >/dev/null 2>&1 || { echo "V6 GFX SKIPPED (ImageMagick convert not found)"; exit 0; }

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
jsexec "$HERE/../tools/blorb2gfx.js" "$BLORB" "$OUT/arthur.gfx" >/dev/null 2>&1

[ -f "$OUT/arthur.gfx/manifest" ] || { echo "V6 GFX FAIL: no manifest written"; exit 1; }
# At least one bitmap PPM exists.
ls "$OUT/arthur.gfx"/*.ppm >/dev/null 2>&1 || { echo "V6 GFX FAIL: no PPM produced"; exit 1; }

# Pick the first bitmap line from the manifest; compare its w/h to the PPM header.
LINE=$(grep -E '^[0-9]+  bitmap' "$OUT/arthur.gfx/manifest" | head -1)
[ -n "$LINE" ] || { echo "V6 GFX FAIL: no bitmap entry in manifest"; exit 1; }
NUM=$(echo "$LINE" | awk '{print $1}')
MW=$(echo "$LINE" | awk '{print $3}')
MH=$(echo "$LINE" | awk '{print $4}')
# PPM header dims (P6 then "<w> <h>"); strip comments, take the 2nd and 3rd whitespace tokens.
DIMS=$(head -c 128 "$OUT/arthur.gfx/$NUM.ppm" | sed 's/#[^\n]*//g' | tr '\n\r\t' '   ' | awk '{print $2, $3}')
PW=$(echo "$DIMS" | awk '{print $1}')
PH=$(echo "$DIMS" | awk '{print $2}')
if [ "$MW" = "$PW" ] && [ "$MH" = "$PH" ]; then
  echo "V6 GFX OK (picture $NUM: ${MW}x${MH})"
else
  echo "V6 GFX FAIL: manifest ${MW}x${MH} != PPM header ${PW}x${PH} for picture $NUM"
  exit 1
fi
# Transparency mask: an adaptive/alpha picture (frame pic 54) must get a .pbm and a mask column.
if grep -qE '^54  bitmap  [0-9]+  [0-9]+  54\.ppm  54\.pbm' "$OUT/arthur.gfx/manifest"; then
  [ -f "$OUT/arthur.gfx/54.pbm" ] || { echo "V6 GFX FAIL: 54.pbm mask not produced"; exit 1; }
  # Mask width must be a multiple of 8 (padded), to avoid the SyncTERM masked-paste shear.
  MPW=$(head -c 32 "$OUT/arthur.gfx/54.pbm" | sed 's/#[^\n]*//g' | tr '\n\r\t' '   ' | awk '{print $2}')
  if [ -n "$MPW" ] && [ $((MPW % 8)) -eq 0 ]; then
    echo "V6 GFX OK (mask 54.pbm present, width ${MPW} multiple of 8)"
  else
    echo "V6 GFX FAIL: 54.pbm width ${MPW} is not a multiple of 8 (shear workaround)"; exit 1
  fi
else
  echo "V6 GFX FAIL: no mask column for picture 54"; exit 1
fi
# Adaptive remap: BPal(scene 4, adaptive 54) -> variant 1005.
if grep -qE '^@ 4 54 1005$' "$OUT/arthur.gfx/manifest"; then
  echo "V6 GFX OK (remap @ 4 54 1005 present)"
else
  echo "V6 GFX FAIL: remap line '@ 4 54 1005' missing"; exit 1
fi
