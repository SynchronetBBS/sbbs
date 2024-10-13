#!/bin/sh

# Files that need to be updated:
# CMakeLists.txt - CMake packaging... obsolete, but present
# Info.plist - macOS information
# Manual.txt - Ugh.
# dpkg-control.in - Debian Package information
# syncterm.c - syncterm_version variable
# syncterm.rc - Windows information
# syncterm.spec - RPM package information

VERSTR=$1
NUMERIC=${VERSTR%%[a-z]*}
PATCH=${VERSTR##${NUMERIC}}
case "${PATCH}" in
a)
	PATCHSTR=alpha
	PATCHSEQ=0
	ISREL=0
	;;
b)
	PATCHSTR=beta
	PATCHSEQ=1
	ISREL=0
	;;
rc*)
	PATCHSTR=${PATCH}
	PATCHSEQ=$((1+${PATCH##rc}))
	ISREL=0
	;;
"")
	PATCHSTR=release
	PATCHSEQ=0
	ISREL=1
	;;
*)
	echo Invalid patch value
	exit
	;;
esac
MAJOR=${NUMERIC%%.*}
MINOR=${NUMERIC##*.}

echo "VERSTR:   ${VERSTR}"
echo "NUMERIC:  ${NUMERIC}"
echo "MAJOR:    ${MAJOR}"
echo "MINOR:    ${MINOR}"
echo "PATCH:    ${PATCH}"
echo "PATCHSEQ: ${PATCHSEQ}"
echo "PATCHSTR: ${PATCHSTR}"
echo "ISREL:    ${ISREL}"
echo "Do you need to bump revision in ../conio/cterm.c?"

# CMakeLists.txt
perl -pi -e "s/(CPACK_PACKAGE_VERSION_MAJOR )[0-9]+/\$1.'${MAJOR}'/ge" CMakeLists.txt
perl -pi -e "s/(CPACK_PACKAGE_VERSION_MINOR )[0-9]+/\$1.'${MINOR}'/ge" CMakeLists.txt
perl -pi -e "s/(CPACK_PACKAGE_VERSION_PATCH )[^)]+/\$1.'${PATCHSTR}'/ge" CMakeLists.txt
perl -pi -e "s/(CPACK_PACKAGE_VERSION )[^)]+/\$1.'${VERSTR}'/ge" CMakeLists.txt

# Info.plist
perl -pi -e "s|(<key>CFBundleShortVersionString</key>.+?<string>)[^<]+(?=</string>)|\$1.'${VERSTR}'|ges" Info.plist
perl -pi -e "s|(<key>CFBundleVersion</key>.+?<string>)[^<]+(?=</string>)|\$1.'${NUMERIC}'|ges" Info.plist

# Manual.txt
perl -pi -e "s/(?<=SyncTERM v)[0-9.a-z]+/${VERSTR}/g" Manual.txt

# dpkg-control.in
perl -pi -e "s/(?<=^Version: ).*$/${VERSTR}/g" dpkg-control.in

# syncterm.c
perl -pi -e "s/(?<=^const char \\*syncterm_version = \"SyncTERM )[^\"]+/${VERSTR}/g" syncterm.c

# syncterm.rc
perl -pi -e "s/^(\s+(PRODUCTVERSION|FILEVERSION)\s+).*$/\$1.'${MAJOR},${MINOR},${ISREL},${PATCHSEQ}'/e" syncterm.rc
perl -pi -e "s/^(\s+(VALUE \"(File|Product)Version\",)\s+\").*$/\$1.'${VERSTR}\\0\"'/e" syncterm.rc

# syncterm.spec
perl -pi -e "s/(?<=^Version: ).*$/${VERSTR}/g" dpkg-control.in
