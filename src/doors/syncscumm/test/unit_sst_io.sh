#!/bin/sh
# Build + run the sst_io unit test against the stage-1 libraries.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
[ -f "$DOOR/build/libs/termgfx/libtermgfx.a" ] || { echo "run ./build.sh first"; exit 1; }
# When door/CMakeLists.txt's libjxl probe found the library, libtermgfx.a's
# jxl.o has real libjxl calls (not the stub) -- ANY final link of it needs
# -l<lib> regardless of whether sst_io.c itself uses WITH_JXL. Compiling
# sst_io.c here WITH -DWITH_JXL too (mirrors build.sh's config.mk DEFINES
# append) is what actually exercises the JXL present path in this test.
JXL_DEFINE=
JXL_LIBS=
if [ -f "$DOOR/build/libs/jxl_libs.txt" ]; then
	JXL_DEFINE="-DWITH_JXL"
	for l in $(cat "$DOOR/build/libs/jxl_libs.txt" | tr ';' ' '); do
		case "$l" in
		/*) JXL_LIBS="$JXL_LIBS $l" ;;
		*)  JXL_LIBS="$JXL_LIBS -l$l" ;;
		esac
	done
fi
# libsndfile: same handoff as JXL above (door/CMakeLists.txt writes
# sndfile_libs.txt) -- ANY final link of libtermgfx.a needs it once termgfx
# was built with libsndfile, regardless of whether sst_io.c itself uses it.
SNDFILE_LIBS=""
if [ -s "$DOOR/build/libs/sndfile_libs.txt" ]; then
	SNDFILE_LIBS=$(cat "$DOOR/build/libs/sndfile_libs.txt")
fi
# sst_io.c reads syncscumm.ini via xpdev's ini_file.h (sixel_max override --
# door/syncscumm.cpp's subtitles read does the same), so every cc invocation
# below needs xpdev's own include dir, not just its static lib.
XPDEV_INC="-I$DOOR/../../xpdev"

cc -o /tmp/test_sst_io $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io

# The SGR mouse coordinate mapper (M3) needs the SST_TEST seams compiled in
# (sst_io_test_set_geom()/sst_io_test_mouse_report()), which the shipped door
# never defines -- its own binary, and a fresh session like the others above.
cc -o /tmp/test_sst_mouse $JXL_DEFINE -DSST_TEST -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_mouse.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_mouse

# The keyboard decode (M3 Task 5) needs the same SST_TEST seam
# (sst_io_test_feed()), driving parse_bytes() directly with raw wire bytes --
# fresh session, own binary, same reason as the mouse test above.
cc -o /tmp/test_sst_input $JXL_DEFINE -DSST_TEST -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_input.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_input

# The evdev physical-key decode (M3 Task 5 review fix) needs a FRESH
# g_km/g_quit -- once test_sst_input above wins kitty on its process's
# g_km, termgfx's "evdev wins" guard would refuse to also enable evdev
# there -- so this is its own binary, same SST_TEST seam as above.
cc -o /tmp/test_sst_input_evdev $JXL_DEFINE -DSST_TEST -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_input_evdev.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_input_evdev

# The non-graphics-terminal gate needs a fresh sst_io session (file-static
# state, no reset), so it's its own binary rather than another case above.
cc -o /tmp/test_sst_io_nogfx $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_nogfx.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_nogfx

# The canvas-size startup hold (Foot geometry bug) likewise needs a fresh
# session; its own binary for the same reason.
cc -o /tmp/test_sst_io_canvas $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_canvas.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_canvas

# The strand-on-static-screen fix (F5 Sky panel / speech-toggle X stay black
# until the mouse moves): a pacing/backpressure-gated present() must retain
# its frame and sst_io_tick() must retry it with no further present() call.
# Needs the SST_TEST seams (sst_io_test_set_inflight()/
# sst_io_test_present_pending()); fresh session, own binary for the same
# reason as the others above.
cc -o /tmp/test_sst_io_present_pending $JXL_DEFINE -DSST_TEST -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_present_pending.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_present_pending

# The XTSMGRAPHICS sixel-ceiling clamp (xterm oversized-sixel discard fix)
# likewise needs a fresh session; its own binary for the same reason.
cc -o /tmp/test_sst_io_gfxmax $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_gfxmax.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_gfxmax

# XTVERSION-identified xterm: an exact canvas report with no XTSMGRAPHICS
# reply must still clamp to TERMGFX_SIXEL_SAFE_MAX once the terminal has
# positively identified itself as xterm -- fresh session, own binary.
cc -o /tmp/test_sst_io_xterm_ceiling $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_xterm_ceiling.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_xterm_ceiling

# The audio probe needs a fresh sst_io session (file-static tier/settle
# state, no reset), so it's its own binary rather than another case above.
cc -o /tmp/test_sst_io_audio $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio

# The tone-only audio tier likewise needs a fresh session (the tier latches
# once per process); its own binary for the same reason.
cc -o /tmp/test_sst_io_audio_tone $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_tone.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio_tone

# The underrun re-prime dispatch likewise needs a fresh session (the stream
# and its cushion state latch once per process); its own binary for the
# same reason.
cc -o /tmp/test_sst_io_audio_underrun $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_underrun.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio_underrun

# Audio must report ITS OWN share of the shared output FIFO, not the stage's
# depth -- a parked video frame is not audio congestion (the comic-intro
# dialogue-dropout defect) -- and the FIFO must stay strictly ordered on the
# wire. Fresh session, own binary for the same reason as the others above.
cc -o /tmp/test_sst_io_audio_backlog $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_backlog.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio_backlog

# A STILL screen must not silence the door: with no present() at all, fed PCM
# still has to reach the wire (the comic-intro dialogue-gap defect -- the
# module only flushes on PRIME release and stop, so if the door only flushes
# from present(), a still panel leaves audio staged and unwritten). Fresh
# session, own binary for the same reason as the others above.
cc -o /tmp/test_sst_io_audio_static $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_static.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
/tmp/test_sst_io_audio_static

# Sysop "sixel_max" ini override beats even a reported XTSMGRAPHICS ceiling.
# Needs a syncscumm.ini in CWD when sst_io_init() runs, so this one is
# invoked from a dedicated temp directory rather than $HERE/$DOOR.
cc -o /tmp/test_sst_io_sixelmax_override $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_sixelmax_override.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
SIXELMAX_TMPDIR=$(mktemp -d)
INI_TMPDIR=$(mktemp -d)
TUNE_TMPDIR=$(mktemp -d)
HEADROOM_TMPDIR=$(mktemp -d)
trap 'rm -rf "$SIXELMAX_TMPDIR" "$INI_TMPDIR" "$TUNE_TMPDIR" "$HEADROOM_TMPDIR"' EXIT
printf 'sixel_max = 800\n' > "$SIXELMAX_TMPDIR/syncscumm.ini"
(cd "$SIXELMAX_TMPDIR" && /tmp/test_sst_io_sixelmax_override)

# Sysop "[audio] enabled = false" must cost the uplink NOTHING on a terminal
# that could have played the stream -- including no per-PCM-block re-read of
# syncscumm.ini (a per-mixer-tick SMB open/close storm on this door's usual
# CIFS-mounted startup_dir). -Wl,--wrap=fopen redirects every fopen() call in
# sst_io.c to the test's __wrap_fopen(), which counts the syncscumm.ini opens
# and forwards to the real fopen() via __real_fopen() -- so the door's actual
# ini reads are unchanged, only counted. Same CWD-sensitive pattern as the
# sixel_max override above: its own temp dir seeded with the ini.
cc -o /tmp/test_sst_io_audio_ini_off $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   -Wl,--wrap=fopen \
   "$HERE/test_sst_io_audio_ini_off.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
printf '[audio]\nenabled = false\n' > "$INI_TMPDIR/syncscumm.ini"
(cd "$INI_TMPDIR" && /tmp/test_sst_io_audio_ini_off)

# ... and a tuning key must change the bytes, not just parse: "chunk_ms = 50"
# doubles how many chunks a 1s feed closes, which the 100ms default cannot
# reach. Fresh session, own binary + temp dir for the same reasons.
cc -o /tmp/test_sst_io_audio_ini_tune $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_ini_tune.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
printf '[audio]\nchunk_ms = 50\n' > "$TUNE_TMPDIR/syncscumm.ini"
(cd "$TUNE_TMPDIR" && /tmp/test_sst_io_audio_ini_tune)

# ... and the pre-encode headroom is the one tuning key whose whole effect is on
# the sample VALUES, so no other audio test here can notice it going missing.
# This one therefore DECODES the A;LoadBlob the door emitted and reads the peak
# back: "headroom = 50" must put a full-scale sine on the wire at half scale.
# Fresh session, own binary + temp dir for the same reasons.
cc -o /tmp/test_sst_io_audio_ini_headroom $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
   "$HERE/test_sst_io_audio_ini_headroom.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
   -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
printf '[audio]\nheadroom = 50\n' > "$HEADROOM_TMPDIR/syncscumm.ini"
(cd "$HEADROOM_TMPDIR" && /tmp/test_sst_io_audio_ini_headroom)
