#!/bin/sh
# Fetch the freeware Beneath a Steel Sky CD data into test/games/bass/
# (gitignored). Idempotent.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DEST="$HERE/games/bass"
[ -f "$DEST/sky.dsk" ] && { echo "bass: already present"; exit 0; }
mkdir -p "$DEST"
curl -sfL -o "$DEST/bass.zip" \
  "https://downloads.scummvm.org/frs/extras/Beneath%20a%20Steel%20Sky/bass-cd-1.2.zip"
echo "53209b9400eab6fd7fa71518b2f357c8de75cfeaa5ba57024575ab79cc974593  $DEST/bass.zip" \
  | sha256sum -c -
unzip -o -q -j "$DEST/bass.zip" -d "$DEST"
rm "$DEST/bass.zip"
ls "$DEST"
