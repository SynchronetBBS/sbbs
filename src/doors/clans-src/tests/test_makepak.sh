#!/bin/sh
#
# Integration tests for makepak.
# Input:  listing file (.lst) referencing source files relative to CWD
# Output: .pak archive
#
# makepak resolves source filenames relative to CWD, so we cd to the
# fixture directory before running the tool.

cd "$(dirname "$0")"
TOOL="${BINDIR}makepak${EXEFILE}"
if [ ! -x "$TOOL" ]; then
	printf "SKIP  tool not built: %s\n" "$TOOL"
	exit 0
fi
. ./test_harness.sh

F="${FIXTURE_DIR}/makepak"
T="${CLANS_TMPDIR}/makepak"
mkdir -p "$T"

# --- happy path ---
# Run from the fixture directory so relative filenames in listing.lst resolve.
( cd "$F" && "$TOOL" "$T/out.pak" listing.lst ) >/dev/null 2>&1
# Re-run through the harness for the exit-code assertion.
assert_succeeds  "valid listing exits zero" \
	sh -c "cd \"$F\" && \"$TOOL\" \"$T/out2.pak\" listing.lst"
assert_nonempty  "valid listing produces .pak file" \
	"$T/out2.pak"

# --- multi-file listing: 3 source files packed into one archive ---
assert_succeeds  "multi-file listing exits zero" \
	sh -c "cd \"$F\" && \"$TOOL\" \"$T/multi.pak\" listing_multi.lst"
assert_nonempty  "multi-file listing produces .pak file" \
	"$T/multi.pak"
assert_file_larger  "multi-file pak larger than single-file pak" \
	"$T/multi.pak" "$T/out2.pak"

# --- listing with comment lines and blank lines ---
assert_succeeds  "listing with comments exits zero" \
	sh -c "cd \"$F\" && \"$TOOL\" \"$T/comments.pak\" listing_comments.lst"
assert_nonempty  "listing with comments produces .pak file" \
	"$T/comments.pak"

# --- error: missing listing file ---
assert_fails     "missing listing exits nonzero" \
	sh -c "cd \"$F\" && \"$TOOL\" \"$T/err.pak\" nonexistent.lst"

finish
