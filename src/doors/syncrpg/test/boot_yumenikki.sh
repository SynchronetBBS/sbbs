#!/bin/sh
# M1+M3 acceptance: boot Yume Nikki headless with termgfx forced into sixel
# file-capture mode, then (M1) decode the first captured frame and assert it
# is a 320x240 non-blank title render (not a black screen), and (M3) assert
# the mixer produced non-silent audio on the way to that title. Exit 0 = pass.
#
# Mirrors syncscumm/test/boot_sixel.sh: SYNCSCUMM_SIXELOUT is termgfx_termio's
# internal capture env (unchanged since the shared-lib extraction) -- it
# captures any termgfx consumer's present output, including present_rgbx, which
# is the path this door drives. Decoding uses the repo's stdlib-only sixel
# decoder (syncmoo1/tools/sixdecode.py); no third-party deps.
#
# The audio side (M3 / Task 6) uses the same shared module's other diagnostic
# tap: SYNCSCUMM_AUDIODUMP (termgfx_termio.c) dumps the raw pre-encode PCM
# handed to termgfx_termio_audio_stream() to a file, unconditionally -- ahead
# of every session/availability gate, including the g_active/g_fd_in check
# that makes termgfx_termio_audio_available() answer 0 for exactly this
# headless run (see audio_term.h's file doc). That is what makes this
# assertion possible without a live SyncTERM audio session: SyncrpgAudio::tick()
# (audio_term.cpp) calls termgfx_termio_audio_stream() every poll regardless of
# session state, so the tap sees the mixer's real signal -- EasyRPG's GenericAudio
# decoding Yume Nikki's title BGM -- whether or not anyone is listening on the
# wire. This exercises the seam this door owns (GenericAudio -> audio_term ->
# termgfx_termio_audio_stream); it is NOT a live-fd capture of encoded audio
# reaching a terminal, which (like video capture) this headless harness has no
# input fd to drive -- see the seam vs. wire distinction called out in the
# Task 6 report.
#
# The game tree is not in the repo (copyright). Supply it as $1 or via
# $SYNCRPG_YN; defaults to the xtrn/yumenikki package's yumenikki/ (fetched by
# xtrn/yumenikki/getdata.js). A decoded PNG of the first frame is written to
# build/yn_title.png for eyeballing.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/syncrpg"
SIXDECODE="$DOOR/../syncmoo1/tools"
GAME=${1:-${SYNCRPG_YN:-$DOOR/../../../xtrn/yumenikki/yumenikki}}
PNG="$DOOR/build/yn_title.png"

[ -x "$BIN" ] || { echo "FAIL: $BIN not built (run ./build.sh)"; exit 1; }
[ -f "$GAME/RPG_RT.ldb" ] || {
	echo "FAIL: no game at $GAME (pass the Yume Nikki dir as \$1 or set SYNCRPG_YN)"
	exit 1
}
[ -f "$SIXDECODE/sixdecode.py" ] || { echo "FAIL: sixdecode.py not found at $SIXDECODE"; exit 1; }

CAP=$(mktemp -d)
trap 'rm -rf "$CAP"' EXIT
SYNCSCUMM_SIXELOUT="$CAP/frames.six" SYNCSCUMM_AUDIODUMP="$CAP/audio.raw" SYNCRPG_GAME="$GAME" \
	timeout 12 "$BIN" > "$CAP/boot.log" 2>&1 || true   # timeout kill is the normal end

[ -s "$CAP/frames.six" ] || { echo "FAIL: no sixel output captured"; exit 1; }
# DCS sixel introducer must be present at all.
pat=$(printf '\033P')
grep -qa "$pat" "$CAP/frames.six" || { echo "FAIL: no DCS introducer in capture"; exit 1; }

# Decode the captured frames; assert geometry is 320x240 from the very first
# frame and that a non-blank title render appears promptly (frame 0 is the
# engine's initial black clear -- the title paints from frame 1). Write the
# title frame to a PNG for eyeballing.
PYTHONPATH="$SIXDECODE" python3 - "$CAP/frames.six" "$PNG" <<'PY'
import sys
import sixdecode

data = open(sys.argv[1], "rb").read()
pngpath = sys.argv[2]


def colors(img):
    used = set()
    for row in img:
        used.update(row)
    return used


first_dims = None
first_nonblank = -1    # index of the earliest non-blank render (promptness)
last = None            # (w, h, img, pal) of the final captured frame
last_idx = -1
for i, (w, h, img, pal) in enumerate(sixdecode.decode_stream(data)):
    if i == 0:
        first_dims = (w, h)
    # Non-blank: a real render uses many colors across the frame, not a single
    # flat fill. Count distinct palette indices actually painted.
    if first_nonblank < 0 and len(colors(img)) >= 8:
        first_nonblank = i
    last = (w, h, img, pal)
    last_idx = i

if first_dims is None or last is None:
    print("FAIL: no decodable sixel frame")
    sys.exit(1)
if first_dims != (320, 240):
    print("FAIL: first frame is %dx%d, expected 320x240" % first_dims)
    sys.exit(1)
if first_nonblank < 0:
    print("FAIL: no non-blank render captured (all frames black)")
    sys.exit(1)
if first_nonblank > 30:
    print("FAIL: first non-blank render not until frame %d (slow/stuck boot)" % first_nonblank)
    sys.exit(1)

# The final captured frame is the screen the game settles on -- the title menu
# after the boot logos. Assert it is a 320x240 non-blank render and save it as
# the eyeball reference (the scout's yn_title.png shows the same title).
w, h, img, pal = last
if (w, h) != (320, 240):
    print("FAIL: final frame is %dx%d, expected 320x240" % (w, h))
    sys.exit(1)
if len(colors(img)) < 8:
    print("FAIL: final frame near-blank (%d distinct colors)" % len(colors(img)))
    sys.exit(1)

# Write a PNG for eyeballing. Prefer Pillow; fall back to the bundled PPM
# writer when Pillow is unavailable.
try:
    from PIL import Image
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y, row in enumerate(img):
        for x, idx in enumerate(row):
            px[x, y] = pal.get(idx, (255, 0, 255))
    im.save(pngpath)
    print("PNG %s" % pngpath)
except ImportError:
    ppm = pngpath[:-4] + ".ppm"
    sixdecode.write_pnm(ppm, w, h, img, pal)
    print("PNM %s (Pillow unavailable)" % ppm)

print("first non-blank frame #%d; title frame #%d: %dx%d, %d distinct colors"
      % (first_nonblank, last_idx, w, h, len(colors(img))))
PY

# M3 (Task 6): assert the audiodump tap actually saw non-silent PCM -- raw
# interleaved S16 stereo at SYNCRPG_AUDIO_RATE (audio_term.h), written by
# termgfx_termio_audio_stream() every time SyncrpgAudio::tick() (audio_term.cpp)
# calls it, which is every poll regardless of session/availability state (see
# the file header above). Non-silence is the meaningful bar, not mere
# non-emptiness: a stopped/EmptyAudio backend would still produce a stream of
# correctly-sized all-zero frames.
[ -s "$CAP/audio.raw" ] || { echo "FAIL: no audio dump captured (SYNCSCUMM_AUDIODUMP)"; exit 1; }
python3 - "$CAP/audio.raw" <<'PY'
import struct
import sys

path = sys.argv[1]
data = open(path, "rb").read()
if len(data) % 2 != 0:
    print("FAIL: audio dump size %d is not a whole number of int16 samples" % len(data))
    sys.exit(1)

nsamples = len(data) // 2
samples = struct.unpack("<%dh" % nsamples, data)
frames = nsamples // 2   # stereo

if frames == 0:
    print("FAIL: audio dump has zero frames")
    sys.exit(1)

peak = max(abs(s) for s in samples)
nonzero = sum(1 for s in samples if s != 0)

# A real BGM mix should clear a floor well above quantization noise, and do so
# over more than a token handful of samples -- both catch a stream of
# structurally-valid but silent (or near-silent) frames.
if peak < 100:
    print("FAIL: audio dump near-silent (peak amplitude %d of 32767)" % peak)
    sys.exit(1)
if nonzero < nsamples // 4:
    print("FAIL: audio dump mostly silent (%d/%d nonzero samples)" % (nonzero, nsamples))
    sys.exit(1)

print("audio dump: %d frames (%.2fs @ 24000Hz), peak %d/32767, %d/%d samples nonzero"
      % (frames, frames / 24000.0, peak, nonzero, nsamples))
PY

echo "BOOT-YUMENIKKI OK (320x240 title render, PNG at $PNG; non-silent audio dump)"
