#!/bin/sh
# Fetch the Red Alert freeware assets for the SyncAlert door.
#
# Downloads the RA95 freeware CD image (released as freeware by EA,
# 2008-08-31) from archive.org, verifies its checksum, and extracts the
# two MIX archives the engine needs. Nothing is redistributed with
# Synchronet; the sysop fetches EA's freeware release directly.
#
# Usage: ./fetch-assets.sh [dest-dir]   (default: ./assets)
set -e
cd "$(dirname "$0")"

DEST="${1:-./assets}"
ISO_URL="https://archive.org/download/CCRedAlert/CD1.iso"
ISO_SHA256="9b30966b323643d9d544b7af21caacd923a46c2f7688639591e5b59b831da4d7"
ISO_FILE="ra95-cd1.iso"

if [ -f "$DEST/REDALERT.MIX" ] && [ -f "$DEST/MAIN.MIX" ]; then
	# Repair permissions from an earlier fetch (see chmod comment below).
	chmod 444 "$DEST/REDALERT.MIX" "$DEST/MAIN.MIX" 2>/dev/null || true
	echo "Assets already present in $DEST -- nothing to do."
	exit 0
fi

command -v bsdtar >/dev/null || {
	echo "ERROR: bsdtar required (Debian: apt install libarchive-tools)" >&2
	exit 1
}

if [ ! -f "$ISO_FILE" ]; then
	echo "Downloading RA95 freeware CD1 (~556MB) from archive.org..."
	curl -L -o "$ISO_FILE.part" "$ISO_URL"
	mv "$ISO_FILE.part" "$ISO_FILE"
fi

echo "Verifying checksum..."
echo "$ISO_SHA256  $ISO_FILE" | sha256sum -c - || {
	echo "ERROR: checksum mismatch -- refusing to use $ISO_FILE" >&2
	exit 1
}

echo "Extracting MIX archives..."
mkdir -p "$DEST"
bsdtar -xf "$ISO_FILE" -C "$DEST" --strip-components=1 INSTALL/REDALERT.MIX
bsdtar -xf "$ISO_FILE" -C "$DEST" MAIN.MIX

# The ISO carries the MIX files as mode 400 (owner-read-only) and bsdtar
# preserves that, leaving them unreadable when the BBS runs the door as a
# different user (the engine then can't open them). Keep them read-only
# but readable by everyone.
chmod 444 "$DEST/REDALERT.MIX" "$DEST/MAIN.MIX"

ls -la "$DEST"
echo "Done. The ISO ($ISO_FILE) may be deleted to reclaim ~556MB."
