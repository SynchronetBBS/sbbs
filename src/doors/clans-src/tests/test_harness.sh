#!/bin/sh
#
# Minimal shell test harness for The Clans devkit integration tests.
# Source this file in each test script, then call finish at the end.
#
# Variables set by GNUmakefile (all must be present):
#   BINDIR       -- path to compiled tool binaries (with trailing /)
#   EXEFILE      -- platform/flavour suffix (e.g. .freebsd.amd64.opt)
#   FIXTURE_DIR  -- absolute path to tests/fixtures/
#   CLANS_TMPDIR -- absolute path to tests/tmp/

g_pass=0
g_fail=0

pass_test() { g_pass=$((g_pass + 1)); printf "  PASS  %s\n" "$1"; }
fail_test() { g_fail=$((g_fail + 1)); printf "  FAIL  %s\n" "$1"; }

# assert_succeeds "test name" cmd [args...]
# Passes if the command exits zero.
assert_succeeds() {
	_name="$1"; shift
	if "$@" >/dev/null 2>&1; then
		pass_test "$_name"
	else
		fail_test "$_name  (nonzero exit: $*)"
	fi
}

# assert_fails "test name" cmd [args...]
# Passes if the command exits nonzero.
assert_fails() {
	_name="$1"; shift
	if "$@" >/dev/null 2>&1; then
		fail_test "$_name  (zero exit, expected failure: $*)"
	else
		pass_test "$_name"
	fi
}

# assert_nonempty "test name" file
# Passes if the file exists and has non-zero size.
assert_nonempty() {
	_name="$1"; _file="$2"
	if [ -s "$_file" ]; then
		pass_test "$_name"
	else
		fail_test "$_name  (absent or empty: $_file)"
	fi
}

# assert_contains "test name" file fixed-string
# Passes if the file contains the literal string.
assert_contains() {
	_name="$1"; _file="$2"; _pat="$3"
	if grep -qF "$_pat" "$_file" 2>/dev/null; then
		pass_test "$_name"
	else
		fail_test "$_name  (pattern '$_pat' not found in $_file)"
	fi
}

# assert_not_contains "test name" file fixed-string
# Passes if the file does NOT contain the literal string.
assert_not_contains() {
	_name="$1"; _file="$2"; _pat="$3"
	if grep -qF "$_pat" "$_file" 2>/dev/null; then
		fail_test "$_name  (unexpected pattern '$_pat' found in $_file)"
	else
		pass_test "$_name"
	fi
}

# assert_file_larger "test name" bigger-file smaller-file
# Passes if bigger-file has strictly more bytes than smaller-file.
assert_file_larger() {
	_name="$1"; _big="$2"; _small="$3"
	_bsz=$(wc -c < "$_big"   2>/dev/null | tr -d ' ')
	_ssz=$(wc -c < "$_small" 2>/dev/null | tr -d ' ')
	if [ "${_bsz:-0}" -gt "${_ssz:-0}" ]; then
		pass_test "$_name"
	else
		fail_test "$_name  (expected $_big > $_small in size, got ${_bsz:-0} vs ${_ssz:-0} bytes)"
	fi
}

# finish
# Print summary and exit with 0 if all tests passed, 1 otherwise.
finish() {
	printf "\n%d/%d passed\n" "$g_pass" "$((g_pass + g_fail))"
	[ "$g_fail" -eq 0 ]
}
