#!/bin/sh
# Build + run the sst_quant unit test. Pure C, no libs needed.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
cc -o /tmp/test_sst_quant -I"$DOOR/door" \
   "$HERE/test_sst_quant.c" "$DOOR/door/sst_quant.c" -lm
/tmp/test_sst_quant
