#!/bin/sh
# run_termtest.sh — Launch SyncTERM headless with the termtest program
#
# Usage: run_termtest.sh <syncterm_binary> <termtest_binary>

set -e

SYNCTERM="$1"
TERMTEST="$2"

if [ -z "$SYNCTERM" ] || [ -z "$TERMTEST" ]; then
	echo "Usage: $0 <syncterm> <termtest>" >&2
	exit 1
fi

RESULTS=$(mktemp /tmp/termtest.XXXXXX)
WRAPPER=$(mktemp /tmp/termtest_run.XXXXXX)
trap 'rm -f "$RESULTS" "$WRAPPER"' EXIT

# shell: URLs are limited to 64 chars (LIST_ADDR_MAX), so use a
# wrapper script to avoid truncation of long paths.
cat > "$WRAPPER" <<EOF
#!/bin/sh
exec "$TERMTEST" "$RESULTS"
EOF
chmod +x "$WRAPPER"

export SDL_VIDEODRIVER=offscreen
export SDL_RENDER_DRIVER=software
export SDL_VIDEO_EGL_DRIVER=none

timeout 30 "$SYNCTERM" -iS -Q "shell:$WRAPPER" 2>/dev/null || true

if [ ! -s "$RESULTS" ]; then
	echo "ERROR: no test results produced" >&2
	exit 1
fi

cat "$RESULTS"

# Check for failures
if grep -q '^FAIL ' "$RESULTS"; then
	exit 1
fi

# Check we got at least some passes
if ! grep -q '^PASS ' "$RESULTS"; then
	echo "ERROR: no tests passed" >&2
	exit 1
fi

exit 0
