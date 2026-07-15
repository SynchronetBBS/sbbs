#!/bin/sh
# SyncSCUMM build: out-of-tree ScummVM build with the curated engine set.
# Usage: ./build.sh [null|synchronet]   (backend; default: synchronet)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
BACKEND="${1:-synchronet}"
mkdir -p "$HERE/build"
cd "$HERE/build"
"$HERE/scummvm/configure" --backend="$BACKEND" --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full
# VER_REV= : the vendored tree lives inside the sbbs repo; without this,
# ScummVM's version probe appends the HOST repo's git state ("dirty").
make -j"$(nproc)" VER_REV=
ls -la "$HERE/build/scummvm"
