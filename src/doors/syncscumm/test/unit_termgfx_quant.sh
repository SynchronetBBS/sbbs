#!/bin/sh
# Build + run the termgfx_quant unit test. Pure C, no libs needed.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
cc -o /tmp/test_termgfx_quant -I"$DOOR/door" -I"$DOOR/../termgfx" \
   "$HERE/test_termgfx_quant.c" "$DOOR/../termgfx/termgfx_quant.c" -lm
/tmp/test_termgfx_quant
