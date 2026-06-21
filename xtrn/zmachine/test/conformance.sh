#!/bin/sh
# Multi-version conformance gate: run the czech (Comprehensive Z-machine Emulation
# CHecker) story through our engine for every supported version and require
# "Failed: 0" from each. Prints "CONFORMANCE OK" and exits 0 only if all pass.
#
# Usage: test/conformance.sh
# Requires: jsexec on PATH, test/stories/czech.z{3,4,5,8} present (gitignored).
HERE=$(cd "$(dirname "$0")" && pwd)
status=0
for V in 3 4 5 8; do
  if [ ! -f "$HERE/stories/czech.z$V" ]; then
    echo "czech.z$V: MISSING (skipped)"
    status=1
    continue
  fi
  line=$(sh "$HERE/czech.sh" "$V" 2>&1 | grep -iE "passed|failed" | tail -1)
  echo "czech.z$V: $line"
  case "$line" in
    *"Failed: 0"*) ;;                 # ok
    *) echo "  -> FAIL (czech.z$V did not report Failed: 0)"; status=1 ;;
  esac
done
if [ "$status" -eq 0 ]; then
  echo "CONFORMANCE OK"
else
  echo "CONFORMANCE FAILED"
fi
exit "$status"
