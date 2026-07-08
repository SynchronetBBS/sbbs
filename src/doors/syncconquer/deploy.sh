#!/bin/sh
# ===========================================================================
# deploy.sh - Install the already-built syncalert binary into the door's
#             xtrn dir(s).  Run this AFTER ./build.sh, when you actually want
#             the running door updated -- building does not deploy
#             automatically, so a sysop can rebuild and test before pushing a
#             new binary live.  Mirrors src/doors/syncduke/deploy.sh.
#
#   Usage:  ./deploy.sh
#
#   Env:    SYNCALERT_DEST=<dir>    also deploy into this live xtrn/syncalert
#                                   dir (overrides the $SBBSCTRL auto-detect)
#
# Deploys ./build/syncalert to two destinations, each a no-op when it already
# holds the build output:
#   1. THIS tree's ../../../xtrn/syncalert/ -- the in-tree door bundle, next
#      to install-xtrn.ini.  On the recommended SYMLINK=1 *nix install that
#      dir IS the live xtrn (xtrn -> repo/xtrn), so this updates the running
#      door in place.
#   2. The live install located via $SBBSCTRL (root = $SBBSCTRL/..) or
#      $SYNCALERT_DEST -- for COPY-style installs where the in-tree dir is
#      not the live xtrn.
# The binary is a git-ignored build artifact, so unlike the tracked door
# files it is not carried to a live install by `git pull` / `update.js` --
# this script is what puts it where the running door reads it.
#
# Deployed BESIDE the binary is xtrn/syncalert/, one level ABOVE
# xtrn/syncalert/assets/ (the REDALERT.MIX/MAIN.MIX directory,
# fetch-assets.sh's output) -- door_io.c's default asset-dir resolution
# (door_resolve_assets_dir(), no -assets override needed) is
# "<the real binary's own directory>/assets", so this layout is what makes a
# stock install-xtrn.ini launch (no -assets arg) find the assets with zero
# extra sysop configuration. Symlink deploys resolve through to their real
# target (realpath(/proc/self/exe)), so a SYMLINK=1 install works the same
# way as a COPY install here.
#
# For a first-time install, run xtrn/syncalert/fetch-assets.sh (or drop
# REDALERT.MIX/MAIN.MIX into xtrn/syncalert/assets/ by hand) and register the
# door with:
#     jsexec install-xtrn ../xtrn/syncalert
#
# Safe against the SMB-symlink trap: on a SYMLINK=1 install the live
# xtrn/syncalert/syncalert can be a server-side symlink back to $EXE, exposed
# over an SMB mount as a plain file on a different device.  `-ef` (and cp's
# own same-file guard) compare st_dev/st_ino, which differ across that
# mount, so a naive `cp -f "$EXE" dest` would open dest O_TRUNC -- and since
# dest IS $EXE, zero the build output to 0 bytes before cp reads a byte.
# Two guards prevent that: (1) skip when dest already has identical CONTENT
# (catches the cross-mount self-reference that -ef misses, and preserves the
# symlink), and (2) copy via a temp file + atomic rename, so the source is
# never the victim of an O_TRUNC even if some self-reference slips past both
# checks.
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
EXE="$SRCDIR/build/syncalert"
DESTDIR="$SRCDIR/../../../xtrn/syncalert"

if [ ! -x "$EXE" ]; then
	echo "[deploy] ERROR: $EXE not found -- run ./build.sh first" >&2
	exit 1
fi

deploy() {   # $1 = destination xtrn/syncalert directory
	mkdir -p "$1"
	if [ "$EXE" -ef "$1/syncalert" ] || cmp -s "$EXE" "$1/syncalert"; then
		echo "[deploy] $1/syncalert already is the build output -- nothing to copy"
	else
		tmp="$1/.syncalert.tmp.$$"
		cp -f "$EXE" "$tmp" && mv -f "$tmp" "$1/syncalert"
		echo "[deploy] Deployed: $(CDPATH= cd -- "$1" && pwd)/syncalert"
	fi
}

deploy "$DESTDIR"      # in-tree bundle (== the live xtrn on a SYMLINK=1 install)

# Copy-style installs: the live xtrn is a separate dir the in-tree copy misses.
# Locate it via an explicit override, else $SBBSCTRL (install root = its parent).
LIVE="$SYNCALERT_DEST"
[ -z "$LIVE" ] && [ -n "$SBBSCTRL" ] && LIVE="$SBBSCTRL/../xtrn/syncalert"
if [ -n "$LIVE" ]; then
	if [ "$LIVE" -ef "$DESTDIR" ]; then
		:   # same directory as the in-tree bundle (symlinked / build-in-place) -- already done
	elif [ -n "$SYNCALERT_DEST" ] || [ -d "$LIVE" ]; then
		deploy "$LIVE"      # explicit override (create if needed), or an existing live dir
	else
		echo "[deploy] Live install dir not found at $LIVE -- skipping (run install-xtrn there for a first install)"
	fi
fi
