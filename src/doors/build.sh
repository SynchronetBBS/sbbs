#!/bin/sh
# ===========================================================================
# build.sh - Build every buildable door under src/doors from one command.
#
#   Usage:  ./build.sh                  (build every door, keep going on error)
#           ./build.sh syncdoom sde     (build only the named doors)
#           ./build.sh clean            (delete the build trees, then exit)
#           ./build.sh clean all        (delete the build trees, then build)
#           ./build.sh debug            (Debug build, where the door supports it)
#           ./build.sh -l               (list the targets and exit)
#           ./build.sh -q               (quiet: log to .build-logs/, show failures)
#
# This is an umbrella over the per-door build systems -- it does NOT replace
# them, and it passes no flags a door's own script wouldn't accept. Two kinds
# of door live here:
#
#   * The modern C/C++ doors (syncdoom, syncduke, ...) each ship a build.sh
#     that drives CMake and vendors its own copy of libtermgfx, so they have
#     no build order between them.
#   * The legacy doors are plain make(1) trees that link ../odoors, so
#     libODoors is built first whenever any of them is selected. GAC's three
#     games additionally need its gamesdk art tools, so gac-sdk is ordered
#     ahead of them below.
#
# Building does NOT touch any live install -- the doors that ship a deploy.js
# still need `jsexec deploy.js` run in their own directory afterwards. Keeping
# deploy separate means a sysop can rebuild and test before pushing a new
# binary to a live BBS.
#
# Unlike a per-door build.sh, a failure here is not fatal: every remaining
# target is still attempted and a pass/fail summary is printed at the end.
# The exit status is non-zero if any target failed.
# ===========================================================================

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LOGDIR="$SRCDIR/.build-logs"
CONFIG=Release
DOCLEAN=
DOALL=
QUIET=

# --- Target table ----------------------------------------------------------
# One row per target: "<name> <kind> <path> [make-target]"
#
#   cmake  the door ships build.sh, which understands "debug"
#   sh     the door ships build.sh, which takes no config argument
#   make   a legacy make(1) tree; <path> is where make runs
#
# Order matters only for the legacy rows noted in the header comment.
targets() {
	cat <<-EOF
	syncconquer  cmake  syncconquer
	syncdoom     cmake  syncdoom
	syncduke     cmake  syncduke
	syncmoo1     cmake  syncmoo1
	syncretro    cmake  syncretro
	syncrpg      sh     syncrpg
	syncscumm    sh     syncscumm
	clans        make   clans-src      build
	dgnlance     make   dgnlance/src
	freevote     make   freevote/src
	gac-sdk      make   gac/gamesdk
	gac_bj       make   gac/gac_bj/src
	gac_fc       make   gac/gac_fc/src
	gac_wh       make   gac/gac_wh/src
	ny2008       make   ny2008/src
	oxgen        make   oxgen
	sde          make   sde
	smurfcombat  make   smurfcombat
	timeport     make   timeport
	top          make   top
	vbbs         make   vbbs/src
	EOF
}

# Left out of the default build because it does not build at all: u32rr is an
# unfinished C rewrite of the Usurper Pascal door whose only make target is a
# testmain harness, and whose link line names a FreeBSD-specific library
# output directory that has never existed here. That is a source-tree problem
# rather than a missing dependency, so naming it explicitly still runs make
# and shows the error.
excluded() {
	cat <<-EOF
	u32rr      no door target (testmain only); link names a nonexistent lib dir
	EOF
}

excluded_targets() {
	cat <<-EOF
	u32rr      make   u32rr
	EOF
}

# Look a name up in either table; echoes the row, or nothing if unknown.
lookup() {
	{ targets; excluded_targets; } | while read -r n rest; do
		[ "$n" = "$1" ] && echo "$n $rest"
	done
}

# The title + usage block of this file's header comment (lines 3-11); the
# rationale below it is for someone reading the script, not for --help.
usage() {
	sed -n '3,11p' "$0" | sed 's/^# \{0,1\}//'
}

# --- Parse arguments -------------------------------------------------------
# "clean", "all", "debug" and "release" are argument words rather than flags,
# matching what the per-door build.sh scripts and the make targets in
# ../build/rules.mk accept. Anything else is a target name.
SELECTED=
for arg in "$@"; do
	case "$arg" in
	-h | --help)    usage; exit 0 ;;
	-l | --list)    DOLIST=1 ;;
	-q | --quiet)   QUIET=1 ;;
	clean)          DOCLEAN=1 ;;
	all)            DOALL=1 ;;
	debug | Debug)     CONFIG=Debug ;;
	release | Release) CONFIG=Release ;;
	-*)
		echo "build.sh: unknown option '$arg'" >&2
		echo "build.sh: try './build.sh --help'" >&2
		exit 2
		;;
	*)
		if [ -z "$(lookup "$arg")" ]; then
			echo "build.sh: unknown target '$arg'" >&2
			echo "build.sh: try './build.sh --list'" >&2
			exit 2
		fi
		SELECTED="$SELECTED $arg"
		;;
	esac
done

if [ -n "$DOLIST" ]; then
	echo "Buildable targets:"
	targets | while read -r n k p rest; do
		printf '  %-12s %-6s %s\n' "$n" "$k" "$p"
	done
	echo
	echo "Excluded from the default build (does not compile -- name one to try anyway):"
	excluded | while read -r n reason; do
		printf '  %-12s %s\n' "$n" "$reason"
	done
	exit 0
fi

# The rows to act on: every buildable target, or just the ones named.
selected_rows() {
	if [ -z "$SELECTED" ]; then
		targets
	else
		for name in $SELECTED; do lookup "$name"; done
	fi
}

if ! command -v cmake >/dev/null 2>&1; then
	echo "[build] WARNING: cmake not found in PATH -- the modern doors will fail" >&2
fi

[ -n "$QUIET" ] && mkdir -p "$LOGDIR"

# --- Clean -----------------------------------------------------------------
# Done here rather than by forwarding "clean" to each door's build.sh: the
# per-door scripts disagree about what "clean" means (syncconquer cleans and
# exits, the others clean and then build), and the legacy makefiles are
# inconsistent about having a clean target at all.
clean_target() {
	name=$1; kind=$2; path=$3
	case "$kind" in
	cmake | sh)
		for d in "$SRCDIR/$path/build" "$SRCDIR/$path/build-msvc"; do
			if [ -d "$d" ]; then
				echo "[build] $name: removing $d"
				rm -rf "$d"
			fi
		done
		;;
	make)
		# Only some legacy makefiles have a clean target; probing with -n
		# keeps a missing one from being reported as a build failure.
		if (cd "$SRCDIR/$path" && make -n clean >/dev/null 2>&1); then
			echo "[build] $name: make clean in $path"
			(cd "$SRCDIR/$path" && make clean >/dev/null 2>&1) || true
		else
			echo "[build] $name: no clean target, leaving $path alone"
		fi
		;;
	esac
}

# "clean" alone cleans and exits; "clean all" cleans and then builds.
if [ -n "$DOCLEAN" ]; then
	selected_rows | while read -r name kind path rest; do
		clean_target "$name" "$kind" "$path"
	done
	if [ -z "$DOALL" ]; then
		echo "[build] Clean complete."
		exit 0
	fi
fi

# --- odoors prerequisite ---------------------------------------------------
# The legacy doors link ../odoors/libs-$(uname); it is not a door itself, so
# it never appears in the target table, but nothing legacy links without it.
build_odoors() {
	odoors="$SRCDIR/../odoors"
	[ -d "$odoors" ] || { echo "[build] WARNING: $odoors not found" >&2; return 1; }
	echo "[build] Building prerequisite: odoors (libODoors)"
	if [ -n "$QUIET" ]; then
		(cd "$odoors" && make) >"$LOGDIR/odoors.log" 2>&1
	else
		(cd "$odoors" && make)
	fi
}

need_odoors=
selected_rows | grep -q '  *make  *' && need_odoors=1
if [ -n "$need_odoors" ]; then
	if ! build_odoors; then
		echo "[build] ERROR: odoors failed to build; the legacy doors will fail too" >&2
		[ -n "$QUIET" ] && tail -20 "$LOGDIR/odoors.log" >&2
	fi
	echo
fi

# --- Build -----------------------------------------------------------------
# Each door's own build script already parallelizes internally (-j nproc), so
# targets are built one at a time rather than oversubscribing the machine.
build_target() {
	name=$1; kind=$2; path=$3; mktarget=$4
	dir="$SRCDIR/$path"

	if [ ! -d "$dir" ]; then
		echo "[build] $name: SKIP -- $path does not exist" >&2
		return 2
	fi

	case "$kind" in
	cmake)
		if [ "$CONFIG" = Debug ]; then
			(cd "$dir" && ./build.sh debug)
		else
			(cd "$dir" && ./build.sh)
		fi
		;;
	sh)
		# No config argument: these scripts build Release unconditionally.
		if [ "$CONFIG" = Debug ]; then
			echo "[build] $name: no Debug build available, building Release"
		fi
		(cd "$dir" && ./build.sh)
		;;
	make)
		if [ -n "$mktarget" ]; then
			(cd "$dir" && make "$mktarget")
		else
			(cd "$dir" && make)
		fi
		;;
	*)
		echo "[build] $name: unknown kind '$kind'" >&2
		return 2
		;;
	esac
}

RESULTS="$LOGDIR/.results.$$"
[ -n "$QUIET" ] || RESULTS=$(mktemp) || exit 1
[ -n "$QUIET" ] && : >"$RESULTS"
trap 'rm -f "$RESULTS" "$RESULTS.rows"' EXIT INT TERM

echo "[build] Configuration: $CONFIG"
echo

# The loop runs in this shell (not a pipeline subshell) so the tally survives;
# results are still accumulated in a file to keep that independent of it.
selected_rows > "$RESULTS.rows"
while read -r name kind path mktarget; do
	[ -n "$name" ] || continue
	echo "==> $name ($path)"
	start=$(date +%s)
	if [ -n "$QUIET" ]; then
		if build_target "$name" "$kind" "$path" "$mktarget" \
			>"$LOGDIR/$name.log" 2>&1; then rc=0; else rc=$?; fi
	else
		if build_target "$name" "$kind" "$path" "$mktarget"; then rc=0; else rc=$?; fi
	fi
	end=$(date +%s)
	if [ "$rc" -eq 0 ]; then
		echo "PASS $name $((end - start))" >>"$RESULTS"
		echo "<== $name OK ($((end - start))s)"
	else
		echo "FAIL $name $((end - start))" >>"$RESULTS"
		echo "<== $name FAILED (rc=$rc)" >&2
		if [ -n "$QUIET" ]; then
			echo "--- tail of $LOGDIR/$name.log ---" >&2
			tail -15 "$LOGDIR/$name.log" >&2
			echo "--------------------------------" >&2
		fi
	fi
	echo
done < "$RESULTS.rows"
rm -f "$RESULTS.rows"

# --- Summary ---------------------------------------------------------------
echo "=========================================================="
echo " Build summary ($CONFIG)"
echo "=========================================================="
while read -r status name secs; do
	printf ' %-6s %-12s %4ss\n' "$status" "$name" "$secs"
done < "$RESULTS"
echo "----------------------------------------------------------"
# grep -c prints 0 and exits 1 when nothing matches, so swallow the status
# rather than substituting a count of our own (which would print twice).
passed=$(grep -c '^PASS ' "$RESULTS" 2>/dev/null || true)
failed=$(grep -c '^FAIL ' "$RESULTS" 2>/dev/null || true)
echo " $passed passed, $failed failed"
[ -n "$QUIET" ] && echo " Logs: $LOGDIR"
echo

[ "$failed" -eq 0 ] || exit 1
exit 0
