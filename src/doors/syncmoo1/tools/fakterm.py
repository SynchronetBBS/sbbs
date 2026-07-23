#!/usr/bin/env python3
"""fakterm.py -- drive the REAL syncmoo1 binary under a raw pty, acting as a
fake SyncTERM: answer its capability probes and per-frame DSR pacing acks,
then decode the sixel frames and audio APCs it sends back off the wire.

A door's output only means anything to a terminal. The interesting bugs --
a column dropped by the scale+encode path, a music track that loops when it
shouldn't -- are invisible to unit tests AND to the SYNCMOO1_SIXELOUT capture
mode (see README.md's "Why fakterm.py, not SYNCMOO1_SIXELOUT" section: capture
mode encodes the native 320x200 frame, bypassing scale+encode entirely, and
never sets O_NONBLOCK, so its input pump blocks on read()). This harness is
what actually exercises hw_term.c's terminal-facing code end to end. It found
the encode-narrower-than-native sixel bug during the sixel work, and it is
the only tool in this tree that can assert the music loop (";L") flag.

Usage:
    tools/fakterm.py <cols> <rows> [options]

Examples:
    # Sit through the intro to the main menu, decode the last sixel frame:
    tools/fakterm.py 80 25

    # Same run, but print the audio APCs instead of the frame summary:
    tools/fakterm.py 80 25 --audio

    # Click through to the in-game Sound options page (game-px coordinates
    # are converted to terminal cells for you):
    tools/fakterm.py 80 25 --click-game 160 159 --click-game 96 137

Options:
  --seconds N       total wall-clock time to drive the door (default 12.0;
                     the intro + first main-menu present take a few seconds)
  --audio           print one line per audio APC (Store/Load/Queue/Volume/
                     Flush) seen on the wire, instead of a sixel summary
  --exe PATH        built binary (default: ../build/syncmoo1 next to this
                     script)
  --lbx PATH        MoO1 LBX data directory, used as BOTH the subprocess's
                     cwd and its "-data"-equivalent launch-dir fallback (see
                     syncmoo1_config.c's "-data > SYNCMOO1_LBX > launch dir"
                     precedence comment); default: ../../../xtrn/syncmoo1,
                     the in-tree door bundle deploy.sh also targets
  --home PATH       door "-home" directory (default: a fresh temp dir,
                     removed only on request -- see --keep-home)
  --keep-home       don't delete the temp --home dir on exit (default: kept
                     only if --home was given explicitly)
  --wire PATH       also write the complete captured stream (both
                     directions are not distinguished; this is the pty, a
                     single full-duplex byte stream) to this file
  --out PATH        write the LAST decoded sixel frame here as a P6 PNM
                     (default: don't write one; --audio implies no frame
                     write unless --out is also given)
  --click COL ROW   inject one SGR mouse click at terminal CELL (col, row);
                     repeatable, clicks fire in order with a settle delay
                     between them
  --click-game GX GY inject one click at 1oom GAME-PIXEL coordinates (the
                     coordinates ui*.c's own layout tables use), converted to
                     a terminal cell via the same fit/center math
                     syncmoo1_geom.c uses; repeatable
  --new             pass -new to the engine (starts a new game immediately).
                     Do NOT combine with wanting to hear the intro/title
                     music -- see the trap note below.
  --skipintro       pass -skipintro. Also skips the title music -- do not
                     use when the point of the run is to verify music.
  --savequit        pass -savequit, so a run that has been clicked into an
                     active game exits by 1oom's own graceful save-and-quit
                     path on its next turn boundary, rather than this tool's
                     unconditional SIGKILL teardown. Has no effect before a
                     game is started (main menu / intro) -- see below.
  -- ARGS...        anything after a literal "--" is appended to the engine
                     argv verbatim

Traps encoded here, each of which cost a real debugging pass -- do not
rediscover them:

  * The pty MUST be raw (tty.setraw()). In canonical (cooked) mode the
    door's read() never returns without a newline in the input, and the
    door just hangs forever waiting for a line the fake terminal never
    sends -- this looks exactly like a deadlocked door, not a bad harness.

  * A present() lags one input event: after a page/menu change the engine
    blocks in read() before it redraws, so a navigation click needs one
    FILLER event (a harmless extra click/hold frame) queued right after it,
    or the harness will look at the stream before the new screen's sixel
    frame has actually been sent.

  * 1oom's menus are MOUSE-driven; letter hotkeys are unreliable (some
    screens don't map them at all). Inject SGR mouse clicks --
    ESC[<0;col;rowM (press) ... a few HOLD frames of button-down ...
    ESC[<0;col;rowm (release) -- or the click does not register. See
    click() below.

  * 1oom's "-new" sets game_opt_skip_intro as a side effect
    (game_opt_do_new_seed(), 1oom/src/game/game.c) -- so a run that wants
    to hear the intro's title music must NOT pass -new. This tool defaults
    to neither -new nor -skipintro for exactly that reason.

  * "-savequit" (game_opt_save_quit) is checked only inside the turn loop
    (1oom/src/game/game.c ~line 846/881) -- it has NO effect at the intro
    or main menu, where this tool's default runs stay. A run that never
    reaches an active game is torn down by an unconditional SIGKILL
    instead, which is fine: this is a disposable test harness process, not
    a session the BBS believes is still connected.

  * Seeding "[1oom] opta.music_volume = 0" via --launch-dir's syncmoo1.ini
    does NOT mute anything: syncmoo1_music.c's own applied-volume mirror
    (g_vol) is only ever updated by the hw_audio_music_volume hook, and
    1oom's engine calls that EXCLUSIVELY from the in-game Sound options
    page's volume-slider callback (main_menu_update_music_volume(),
    1oom/src/ui/classic/uimainmenu.c) -- never proactively from the loaded
    config at startup. Confirmed by reading the seeded value straight out
    of the door's own syncmoo1_base.tmp snapshot (it WAS 0) while the wire
    still showed the track ship at the compiled-in default volume. Proving
    "volume 0 -> stop, no Store" needs an interactive --click-game sequence
    into the Sound page (or the live SyncTERM gate) -- a static ini value
    alone will not do it.
"""
import argparse
import os
import pty
import re
import select
import shutil
import signal
import sys
import tempfile
import time
import tty

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sixdecode

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_EXE = os.path.join(HERE, "..", "build", "syncmoo1")
DEFAULT_LBX = os.path.join(HERE, "..", "..", "..", "xtrn", "syncmoo1")

# --- probe replies -----------------------------------------------------------
# See syncmoo1_input.c's sm_input_pump() CSI/APC switch for what each of
# these actually latches on the door side.
DA1_REPLY = "\x1b[?62;4;22c"    # DA1: VT-ish + sixel (param 4) + sixel-scroll (22)
CTDA_REPLY = "\x1b[<0;4c"       # CTDA: '<' intro marks SyncTERM; 4 = sixel cap
JXL_NO_REPLY = "\x1b[=1;0n"     # CTerm state report answering Q;JXL: no JXL tier
AUDIO_DIGITAL_REPLY = "\x1b[=7;100;1n"   # Q;libsndfile reply: digital tier (1)

JXL_QUERY = b"\x1b_SyncTERM:Q;JXL\x1b\\"
AUDIO_QUERY = b"SyncTERM:Q;libsndfile"   # substring match; it's inside an APC


class FakeTerm:
    """The pty side of the fake SyncTERM: owns the child process, answers
    probes as they arrive, and accumulates everything read for later
    decoding. Single-threaded, driven by a run() loop with select() --
    no reader thread, so there is no race between "have we answered the
    first ESC[6n yet" and "is the main loop about to send a click"."""

    def __init__(self, cols, rows):
        self.cols = cols
        self.rows = rows
        self.buf = bytearray()
        self.pid = None
        self.fd = None
        self._first_dsr = True
        self._tail = bytearray()   # last few bytes scanned, so a probe split
                                    # across two reads is still recognized

    def spawn(self, exe, argv, env, cwd):
        pid, fd = pty.fork()
        if pid == 0:
            os.chdir(cwd)
            try:
                os.execve(exe, argv, env)
            except OSError as e:
                sys.stderr.write("fakterm: exec failed: %s\n" % e)
                os._exit(127)
        tty.setraw(fd)   # REQUIRED -- see the module docstring's first trap
        self.pid, self.fd = pid, fd

    def _write(self, s):
        os.write(self.fd, s.encode("latin1") if isinstance(s, str) else s)

    def _answer_probes(self):
        """Scan self._tail (a rolling window over everything read so far)
        for probe sequences and reply to each exactly once per occurrence.
        Consumes matched bytes out of the tail so a probe is never answered
        twice."""
        t = self._tail
        while True:
            i_dsr = t.find(b"\x1b[6n")
            i_ctda = t.find(b"\x1b[<c")
            i_da1 = t.find(b"\x1b[c")
            i_jxl = t.find(JXL_QUERY)
            i_audio = t.find(AUDIO_QUERY)
            candidates = [(i, k) for i, k in
                          ((i_dsr, "dsr"), (i_ctda, "ctda"), (i_da1, "da1"),
                           (i_jxl, "jxl"), (i_audio, "audio")) if i >= 0]
            if not candidates:
                break
            i, kind = min(candidates)
            if kind == "dsr":
                if self._first_dsr:
                    self._first_dsr = False
                    self._write("\x1b[%d;%dR" % (self.rows, self.cols))   # grid probe reply
                else:
                    self._write("\x1b[1;1R")                              # pace ack
                del t[:i + 4]
            elif kind == "ctda":
                self._write(CTDA_REPLY)
                del t[:i + 4]
            elif kind == "da1":
                self._write(DA1_REPLY)
                del t[:i + 3]
            elif kind == "jxl":
                self._write(JXL_NO_REPLY)
                del t[:i + len(JXL_QUERY)]
            else:   # audio
                self._write(AUDIO_DIGITAL_REPLY)
                del t[:i + len(AUDIO_QUERY)]
        # Keep only a short trailing window: probes are at most ~30 bytes;
        # no need to rescan megabytes of old sixel frame data every read.
        if len(t) > 64:
            del t[:len(t) - 64]

    def pump(self, timeout=0.05):
        """Read whatever is available (non-blocking-ish via select), answer
        any probes it contains, and return the number of bytes read (0 if
        none / the child has exited)."""
        r, _, _ = select.select([self.fd], [], [], timeout)
        if not r:
            return 0
        try:
            b = os.read(self.fd, 65536)
        except OSError:
            return 0
        if not b:
            return 0
        self.buf.extend(b)
        self._tail.extend(b)
        self._answer_probes()
        return len(b)

    def drain_for(self, seconds):
        end = time.time() + seconds
        while time.time() < end:
            self.pump(min(0.05, max(0.0, end - time.time())))

    def send(self, raw):
        self._write(raw)

    def alive(self):
        try:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
        except ChildProcessError:
            return False
        return pid == 0

    def kill(self):
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except (OSError, ChildProcessError):
            pass


# --- mouse click helper (mouse-driven menus trap) ---------------------------

def click(term, col, row, hold_frames=4, settle=0.4):
    """SGR mouse click at terminal cell (col, row), 1-based. Press, a few
    HOLD frames of button-down (drag/motion reports at the same cell), then
    release -- a bare press+release does not reliably register on 1oom's
    menus (trap #3 in the module docstring). Drains/answers probes between
    every step so a DSR or capability query arriving mid-click doesn't sit
    unanswered."""
    def step(s, wait):
        term.send(s)
        term.drain_for(wait)

    step("\x1b[<0;%d;%dM" % (col, row), 0.15)      # press (button 0)
    for _ in range(hold_frames):
        step("\x1b[<32;%d;%dM" % (col, row), 0.15)  # motion w/ button held
    step("\x1b[<0;%d;%dm" % (col, row), settle)      # release
    # Filler event (trap #2): the redraw from this click may still be
    # pending in the engine's next read() call. One harmless extra motion
    # report (no button) nudges it without changing any game state.
    step("\x1b[<35;%d;%dM" % (col, row), 0.3)


def game_px_to_cell(gx, gy, cols, rows, cw=8, ch=16):
    """Convert 1oom game-pixel coordinates (its own ui*.c layout tables,
    native 320x200 space) to a 1-based terminal cell, using the same
    fit/center math syncmoo1_geom.c applies to build the encoded frame.
    Approximate on purpose (menus have generous click targets) -- exact
    geometry is syncmoo1_geom.c's job, not this tool's."""
    pagew, pageh = cols * cw, rows * ch
    fith = pageh - ch if pageh > ch * 4 else pageh   # reserve the bottom row
    vw, vh = pagew, fith
    sw, sh = 320, 200
    w = vw
    h = w * sh // sw
    if h > vh:
        h = vh
        w = h * sw // sh
    if vw > w and (vw - w) * 100 <= w * 8:
        w = vw
    if vh > h and (vh - h) * 100 <= h * 8:
        h = vh
    if w < 640 and vw >= 640 and h >= 200:
        w = 640   # native-width guarantee (syncmoo1_geom.c)
    dx = (pagew - w) // 2 if pagew > w else 0
    dy = (fith - h) // 2 if fith > h else 0
    px, py = dx + gx * w // 320, dy + gy * h // 200
    return px // cw + 1, py // ch + 1


def parse_args(argv):
    # Split on a literal "--" ourselves BEFORE handing anything to argparse.
    # argparse's own nargs=REMAINDER positional greedily swallows every
    # argument from the first one it reaches onward -- including our OWN
    # later flags like --lbx/--seconds -- because it doesn't know they are
    # meant for us, not the engine. Splitting first sidesteps that entirely.
    if "--" in argv:
        i = argv.index("--")
        argv, engine_args = argv[:i], argv[i + 1:]
    else:
        engine_args = []

    p = argparse.ArgumentParser(
        prog="fakterm.py", description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("cols", type=int)
    p.add_argument("rows", type=int)
    p.add_argument("--seconds", type=float, default=12.0)
    p.add_argument("--audio", action="store_true")
    p.add_argument("--exe", default=DEFAULT_EXE)
    p.add_argument("--lbx", default=DEFAULT_LBX)
    p.add_argument("--launch-dir", default=None,
                    help="engine cwd (default: same as --lbx). syncmoo1.ini is "
                         "read from HERE, before the door chdirs into --home "
                         "(syncmoo1_config.c's sm_config_apply()) -- give a "
                         "separate --launch-dir with its own syncmoo1.ini to "
                         "test a [1oom]/[audio]/[debug] setting without "
                         "touching the real LBX install dir. SYNCMOO1_LBX is "
                         "always set to --lbx too, so the two can safely differ.")
    p.add_argument("--home", default=None)
    p.add_argument("--keep-home", action="store_true")
    p.add_argument("--wire", default=None)
    p.add_argument("--out", default=None)
    p.add_argument("--click", nargs=2, type=int, action="append", default=[],
                    metavar=("COL", "ROW"))
    p.add_argument("--click-game", nargs=2, type=int, action="append", default=[],
                    metavar=("GX", "GY"), dest="click_game")
    # 1oom's -new takes a GAMESEED argument (OPT[:RACES[:BANNERS[:GSEED[:HUMANS]]]],
    # game.c:422, arity 1).  A bare "-new" makes options_parse() fail and the engine
    # abort before it ever writes a config -- which looks exactly like "the door hung".
    p.add_argument("--new", nargs="?", const="200:0:0:0:1", default=None,
                   metavar="GAMESEED")
    p.add_argument("--skipintro", action="store_true")
    p.add_argument("--savequit", action="store_true")
    ns = p.parse_args(argv)
    ns.engine_args = engine_args
    return ns


def main():
    ns = parse_args(sys.argv[1:])

    exe = os.path.abspath(ns.exe)
    lbx = os.path.abspath(ns.lbx)
    launch_dir = os.path.abspath(ns.launch_dir) if ns.launch_dir else lbx
    if not os.path.isfile(exe):
        sys.exit("fakterm: binary not found: %s (build it with ./build.sh first)" % exe)
    if not os.path.isdir(lbx):
        sys.exit("fakterm: --lbx dir not found: %s" % lbx)
    if not os.path.isdir(launch_dir):
        sys.exit("fakterm: --launch-dir not found: %s" % launch_dir)

    home_is_temp = ns.home is None
    home = os.path.abspath(ns.home) if ns.home else tempfile.mkdtemp(prefix="syncmoo1-fakterm-")
    os.makedirs(home, exist_ok=True)

    env = dict(os.environ)
    env.pop("SYNCMOO1_SIXELOUT", None)   # never both capture modes at once
    env["SBBSDATA"] = home                # door-side OGG music cache dir root
    env["SYNCMOO1_LBX"] = lbx             # explicit -- lets --launch-dir differ from --lbx

    argv = ["syncmoo1", "-home", home]
    if ns.new:
        argv += ["-new", ns.new]
    if ns.skipintro:
        argv.append("-skipintro")
    if ns.savequit:
        argv.append("-savequit")
    argv += ns.engine_args

    term = FakeTerm(ns.cols, ns.rows)
    term.spawn(exe, argv, env, cwd=launch_dir)

    term.drain_for(3.0)   # let term_enter + probes + the first present settle

    for col, row in ns.click:
        click(term, col, row)
    for gx, gy in ns.click_game:
        col, row = game_px_to_cell(gx, gy, ns.cols, ns.rows)
        click(term, col, row)

    remaining = ns.seconds - 3.0 - 0.9 * (len(ns.click) + len(ns.click_game))
    if remaining > 0:
        term.drain_for(remaining)

    still_alive = term.alive()
    term.kill()

    if home_is_temp and not ns.keep_home:
        shutil.rmtree(home, ignore_errors=True)

    data = bytes(term.buf)
    if ns.wire:
        with open(ns.wire, "wb") as f:
            f.write(data)

    print("door still running at teardown: %s" % still_alive, file=sys.stderr)
    print("bytes captured: %d" % len(data), file=sys.stderr)

    frame_count = sum(1 for _ in sixdecode.iter_sixel_frames(data))
    print("sixel frames: %d" % frame_count, file=sys.stderr)

    if ns.audio:
        n = 0
        for ev, line in sixdecode.audio_events(data):
            print(line)
            n += 1
        print("-- %d audio APC(s)" % n, file=sys.stderr)
        return

    if frame_count == 0:
        print("no sixel frames decoded -- nothing to summarize", file=sys.stderr)
        return
    # Palette carried forward across frames (decode_stream), not decoded in
    # isolation -- see its docstring for why a lone last-frame decode would
    # under-report colors the terminal already had registered.
    w, h, img, pal = list(sixdecode.decode_stream(data))[-1]
    print("last frame: %dx%d, %d palette entries" % (w, h, len(pal)))
    if ns.out:
        sixdecode.write_pnm(ns.out, w, h, img, pal)
        print("wrote %s" % ns.out)


if __name__ == "__main__":
    main()
