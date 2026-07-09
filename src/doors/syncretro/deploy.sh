#!/bin/sh
# ===========================================================================
# deploy.sh - Install the already-built syncretro binary into the door's xtrn
#             dir(s).  Run this AFTER ./build.sh, when you want the running door
#             updated.  (Building no longer deploys automatically, so a sysop
#             can rebuild and test before pushing a new binary live.)
#
#   Usage:  ./deploy.sh
#   Env:    SYNCRETRO_DEST=<dir>   also deploy into this live xtrn/syncretro dir
#                                  (overrides the $SBBSCTRL auto-detect)
#
# Deploys ./build/syncretro to the in-tree ../../../xtrn/syncretro/ (which IS
# the live xtrn on a SYMLINK install) and, for copy-style installs, to the live
# tree located via $SBBSCTRL or $SYNCRETRO_DEST. The same SMB-symlink self-copy
# guards as ../syncmoo1/deploy.sh apply (skip when content-identical; copy via
# temp + atomic rename).
#
# NOTE: the libretro CORE .so and the (sysop-supplied, legally-owned) console
# BIOS/ROM files are separate content -- see README.md / DESIGN.md sec 9. This
# script deploys only the frontend binary.
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
EXE="$SRCDIR/build/syncretro"
DESTDIR="$SRCDIR/../../../xtrn/syncretro"

if [ ! -x "$EXE" ]; then
	echo "[deploy] ERROR: $EXE not found -- run ./build.sh first" >&2
	exit 1
fi

deploy() {   # $1 = destination xtrn/syncretro directory
	mkdir -p "$1"
	if [ "$EXE" -ef "$1/syncretro" ] || cmp -s "$EXE" "$1/syncretro"; then
		echo "[deploy] $1/syncretro already is the build output -- nothing to copy"
	else
		tmp="$1/.syncretro.tmp.$$"
		cp -f "$EXE" "$tmp" && mv -f "$tmp" "$1/syncretro"
		echo "[deploy] Deployed: $(CDPATH= cd -- "$1" && pwd)/syncretro"
	fi
}

deploy "$DESTDIR"

LIVE="$SYNCRETRO_DEST"
[ -z "$LIVE" ] && [ -n "$SBBSCTRL" ] && LIVE="$SBBSCTRL/../xtrn/syncretro"
if [ -n "$LIVE" ]; then
	if [ "$LIVE" -ef "$DESTDIR" ]; then
		:   # same dir as the in-tree bundle (symlinked / build-in-place)
	elif [ -n "$SYNCRETRO_DEST" ] || [ -d "$LIVE" ]; then
		deploy "$LIVE"
	else
		echo "[deploy] Live install dir not found at $LIVE -- skipping (run install-xtrn there for a first install)"
	fi
fi
