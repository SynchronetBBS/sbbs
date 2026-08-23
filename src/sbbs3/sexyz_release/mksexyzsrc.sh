#!/bin/bash
# Cut the stand-alone sexyz source archives from a Synchronet checkout.
#
#   mksexyzsrc.sh <sbbs-checkout> <output-dir>
#
# The file set is derived, never hand-maintained: the sources are whatever
# GNUmakefile's SRCS names, and the headers are their #include closure.  Every
# file is copied verbatim, so a fix made in the archive applies upstream.
set -euo pipefail
R="${1:?usage: mksexyzsrc.sh <sbbs-checkout> <output-dir>}"; OUT="${2:?}"
REL="$R/src/sbbs3/sexyz_release"
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT

# first match wins; the trees are searched in this order
find_in_tree() {
	local f=$1 d
	for d in src/sbbs3 src/xpdev src/hash src/smblib; do
		if [ -f "$R/$d/$f" ]; then printf '%s\n' "$R/$d/$f"; return 0; fi
	done
	return 1
}

SRCS=$(sed -n '/^SRCS =/,/[^\\]$/p' "$REL/GNUmakefile" | sed 's/^SRCS =//; s/\\//g' | tr -s ' \n' ' ')
for f in $SRCS; do
	p=$(find_in_tree "$f") || { echo "missing source: $f" >&2; exit 1; }
	cp -p "$p" "$D/"
done
for f in $SRCS; do
	gcc -MM -I "$R/src/sbbs3" -I "$R/src/xpdev" -I "$R/src/hash" -I "$R/src/smblib" "$D/$f"
done | tr ' ' '\n' | grep -E '\.h$' | xargs -n1 basename | sort -u | while read -r h; do
	case $h in git_branch.h|git_hash.h) continue;; esac
	p=$(find_in_tree "$h") || { echo "missing header: $h" >&2; exit 1; }
	cp -p "$p" "$D/"
done

# git_*.h are generated in a full tree; pin them to the commit being cut
hash=$(git -C "$R" rev-parse --short=10 HEAD)
stamp=$(git -C "$R" show -s --format=%at HEAD)
printf '#define GIT_BRANCH "%s"\n' "$(git -C "$R" rev-parse --abbrev-ref HEAD)" > "$D/git_branch.h"
printf '#define GIT_HASH "%s"\n#define GIT_DATE "%s"\n#define GIT_TIME %s\n' \
	"$hash" "$(date -d "@$stamp" '+%b %d %Y %H:%M')" "$stamp" > "$D/git_hash.h"

cp -p "$REL"/COMPILING.md "$REL"/CHANGES.md "$REL"/FILE_ID.DIZ "$REL"/GNUmakefile "$REL"/Makefile.vc "$D/"
cp -p "$R/docs/sexyz.txt" "$D/sexyz.txt"; cp -p "$R/LICENSE" "$D/LICENSE"

( cd "$D" && make >/dev/null 2>&1 && ./sexyz v >/dev/null ) || { echo "build failed" >&2; exit 1; }
( cd "$D" && make clean >/dev/null )

# name the archive after sexyz.c's own revision, so a version bump needs no
# edit here: 3.5 -> sexyz35_src
ver=$(sed -n 's/^const char\* *revision *= *"\([0-9.]*\)".*/\1/p' "$D/sexyz.c" | head -1)
[ -n "$ver" ] || { echo "cannot read revision from sexyz.c" >&2; exit 1; }
name="sexyz$(echo "$ver" | tr -d .)_src"

mkdir -p "$OUT"; rm -f "$OUT/$name.zip" "$OUT/$name.tgz"
( cd "$D" && zip -9 -X -q "$OUT/$name.zip" FILE_ID.DIZ COMPILING.md CHANGES.md sexyz.txt LICENSE GNUmakefile Makefile.vc *.c *.h )
( cd "$D" && tar czf "$OUT/$name.tgz" --owner=0 --group=0 . )
echo "$name cut from $hash: $(ls -1 "$D"/*.c | wc -l) sources, $(ls -1 "$D"/*.h | wc -l) headers"
