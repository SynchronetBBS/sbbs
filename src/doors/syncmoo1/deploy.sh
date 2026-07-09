#!/bin/sh
# ===========================================================================
# deploy.sh - Install the already-built syncmoo1 binary into the door's xtrn
#             dir(s).  Run this AFTER ./build.sh, when you actually want the
#             running door updated -- building no longer deploys automatically,
#             so a sysop can rebuild and test before pushing a new binary live.
#
#   Usage:  ./deploy.sh
#
#   Env:    SYNCMOO1_DEST=<dir>    also deploy into this live xtrn/syncmoo1 dir
#                                  (overrides the $SBBSCTRL auto-detect)
#
# Deploys ./build/syncmoo1 to two destinations, each a no-op when it already
# holds the build output:
#   1. THIS tree's ../../../xtrn/syncmoo1/ -- the in-tree door bundle.  On the
#      recommended SYMLINK=1 *nix install that dir IS the live xtrn
#      (xtrn -> repo/xtrn), so this updates the running door in place.
#   2. The live install located via $SBBSCTRL (root = $SBBSCTRL/..) or
#      $SYNCMOO1_DEST -- for COPY-style installs where the in-tree dir is not
#      the live xtrn.
# The binary is a git-ignored build artifact, so unlike the tracked door files
# it is not carried to a live install by `git pull` / `update.js` -- this
# script is what puts it where the running door reads it.
#
# The MoO1 LBX data files (not shipped -- see README.md) still need to be
# supplied separately per install, pointed at via SYNCMOO1_LBX or 1oom's own
# default data-dir search.
#
# Safe against the SMB-symlink trap: on a SYMLINK=1 install the live
# xtrn/syncmoo1/syncmoo1 can be a server-side symlink back to $EXE, exposed
# over an SMB mount as a plain file on a different device.  `-ef` (and cp's
# own same-file guard) compare st_dev/st_ino, which differ across that mount,
# so a naive `cp -f "$EXE" dest` would open dest O_TRUNC -- and since dest IS
# $EXE, zero the build output to 0 bytes before cp reads a byte.  Two guards
# prevent that: (1) skip when dest already has identical CONTENT (catches the
# cross-mount self-reference that -ef misses, and preserves the symlink), and
# (2) copy via a temp file + atomic rename, so the source is never the victim
# of an O_TRUNC even if some self-reference slips past both checks.
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
EXE="$SRCDIR/build/syncmoo1"
DESTDIR="$SRCDIR/../../../xtrn/syncmoo1"

if [ ! -x "$EXE" ]; then
	echo "[deploy] ERROR: $EXE not found -- run ./build.sh first" >&2
	exit 1
fi

deploy() {   # $1 = destination xtrn/syncmoo1 directory
	mkdir -p "$1"
	if [ "$EXE" -ef "$1/syncmoo1" ] || cmp -s "$EXE" "$1/syncmoo1"; then
		echo "[deploy] $1/syncmoo1 already is the build output -- nothing to copy"
	else
		tmp="$1/.syncmoo1.tmp.$$"
		cp -f "$EXE" "$tmp" && mv -f "$tmp" "$1/syncmoo1"
		echo "[deploy] Deployed: $(CDPATH= cd -- "$1" && pwd)/syncmoo1"
	fi
}

deploy "$DESTDIR"      # in-tree bundle (== the live xtrn on a SYMLINK=1 install)

# Copy-style installs: the live xtrn is a separate dir the in-tree copy misses.
# Locate it via an explicit override, else $SBBSCTRL (install root = its parent).
LIVE="$SYNCMOO1_DEST"
[ -z "$LIVE" ] && [ -n "$SBBSCTRL" ] && LIVE="$SBBSCTRL/../xtrn/syncmoo1"
if [ -n "$LIVE" ]; then
	if [ "$LIVE" -ef "$DESTDIR" ]; then
		:   # same directory as the in-tree bundle (symlinked / build-in-place) -- already done
	elif [ -n "$SYNCMOO1_DEST" ] || [ -d "$LIVE" ]; then
		deploy "$LIVE"      # explicit override (create if needed), or an existing live dir
	else
		echo "[deploy] Live install dir not found at $LIVE -- skipping (run install-xtrn there for a first install)"
	fi
fi
