#!/bin/sh
# Build + run the termgfx_quant unit test. Pure C, no libs needed.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
TERMGFX=$(dirname "$HERE")
cc -o /tmp/test_termgfx_quant -I"$TERMGFX" \
   "$HERE/test_termgfx_quant.c" "$TERMGFX/termgfx_quant.c" -lm
/tmp/test_termgfx_quant
