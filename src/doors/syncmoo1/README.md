# syncmoo1

**Master of Orion 1**, rendered as a Synchronet / SyncTERM BBS *door* (an
external program launched from a BBS session) via the vendored
[1oom](https://sourcecraft.dev/fork1oom/1oom) engine and the shared
`../termgfx` rendering library -- the same architecture as `../syncdoom`,
`../syncduke`, and `../syncconquer`. Unlike those, MoO1 is **single-player,
turn-based, mouse-driven point-and-click** (no multiplayer/net code); the
primary interaction-model reference is `../syncconquer` (an RTS).

See [DESIGN.md](DESIGN.md) for the full design (architecture, data flow,
mouse mapping, milestones) and [PROVENANCE.md](PROVENANCE.md) for exactly
what was vendored and how. See [CLAUDE.md](CLAUDE.md) for the ours-vs-vendored
coding contract before touching any source in this directory.

---

## Status: M1 + audio -- playable, not yet polished

Delivered:

- Builds as `syncmoo1` and launches via **DOOR32.SYS** on SyncTERM.
- Renders the whole game as **sixel**, losslessly (the encode never resamples
  below the engine's native 320x200, so MoO1's 1-pixel font strokes survive).
- **Navigable with mouse and per-item keyboard hotkeys.**
- **Audio**: MoO1's sound effects and its 40-track soundtrack, over SyncTERM's
  audio APC. See [Audio](#audio).
- **Per-user storage**: config and savegames land in `-home` (never `$HOME`),
  and a player's config records only the settings they actually changed, so a
  sysop's defaults keep reaching everyone else. See [`syncmoo1.ini`](#syncmoo1ini).
- Correct terminal enter/probe/leave and a BBS-restoring teardown on exit or
  hangup.

**Deferred** (per [DESIGN.md §11](DESIGN.md)): the JXL render path (the door
probes for it and links `libjxl`, but never encodes -- measured on real frames,
lossless JXL is ~5x *larger* than sixel for MoO1's indexed pixel art, and only
lossy JXL wins, at the cost of ringing the UI text); a text/block tier for
terminals without sixel; the `sbbs_node` who's-online/paging overlay
(Ctrl-U/Ctrl-P); and a full-game playtest pass
across every MoO1 screen (especially the dense galaxy map, where
cell-resolution clicking is tightest until SGR-Pixels mode 1016 lands on more
terminals).

**Arrow keys do not navigate the menus** -- only the per-item hotkeys (and the
mouse) do. 1oom drives keyboard navigation by warping the mouse cursor, and the
directional search does not fire here; the hotkeys, `+`/`-`/`=`, and
`Shift`+key all work.

---

## Rendering tiers

`termgfx` gives every tier from the same present path; the ladder is
**JXL -> sixel -> text/block** (half/quadrant/sextant glyphs), auto-selected
from the terminal's startup capability probe.

**This door renders sixel only.** Neither the JXL nor the text/block tier has a
present path here yet, so a terminal without sixel gets no picture. The door
does probe for JXL at connect and links `libjxl` when it is available, but
nothing consumes the reply; a build without `libjxl` behaves identically.

The sixel encode is **lossless in both axes**: it never encodes smaller than
the engine's native 320x200, because the nearest-neighbour resample would
*delete* whole rows or columns rather than blur them, taking MoO1's 1-pixel
font strokes with them. Where the fitted image cannot afford the terminal's
2x pixel-aspect on an axis, that axis drops to 1x and upsamples instead.

---

## Master of Orion 1 data files (LBX) -- you must supply these

1oom is an **engine**, not game content: it requires the original **Master of
Orion 1 v1.3** `.LBX` data files to run, and this door does not (and cannot)
ship them. Master of Orion is © Simtex / MicroProse (see [CREDITS](CREDITS)), so
**you must purchase a legal copy** and supply its data -- doing so is the
sysop's / player's responsibility, out of scope for this code (DESIGN.md §15).
The game is still sold, cheaply:

- **[Master of Orion 1+2 on GOG](https://www.gog.com/en/game/master_of_orion_1_2)**
  -- **recommended** (usually around US$6). It's **DRM-free**, so the installer
  it downloads unpacks straight to the original DOS files -- point `getdata.js`
  (below) at the installed/extracted folder and it lifts the `.lbx` files out.
- **Steam** -- the classic games are not in the base *Master of Orion* (2016);
  they ship only with its pricier
  **[Collector's Edition](https://store.steampowered.com/app/298050/Master_of_Orion/)**,
  and are DRM-wrapped, so GOG is the simpler source for the raw LBX data.

Any legally-owned copy of the DOS **v1.3** data works -- a boxed original, an
old CD, etc.; the storefronts above are just the easiest way to get it today.

**Recommended: put your copy in the door directory.** The door searches its own
launch directory -- the `xtrn/syncmoo1/` install dir -- for the LBX files, so
the simplest install is to drop them there. The bundled installer automates
both registering the door and installing the data:

```sh
jsexec install-xtrn ../xtrn/syncmoo1      # register the door in SCFG + install data
```

Its data step, `getdata.js`, looks in the door directory for a copy **you** have
placed there -- the loose `*.lbx` files, a `.zip`/archive containing them, or an
extracted GOG/DOS game folder -- and extracts/copies the LBX files into place
(lower-casing the names). It downloads nothing. Run it standalone any time after
dropping a copy in:

```sh
jsexec ../xtrn/syncmoo1/getdata.js
```

**Alternatives.** 1oom's own `-data <dir>` option points the engine at any
directory -- add it to the door's command line. For a dev/manual run where you
control the shell, the `SYNCMOO1_LBX` environment variable does the same
(`sm_config_apply()` resolves it once at startup, absolutized via xpdev's
`FULLPATH()` before the per-user `chdir`). If none of these is set and the door
dir has no
LBX, 1oom falls through to its own built-in search (cwd, XDG dirs,
`/usr/share/1oom`, ...). Note `SYNCMOO1_LBX` is only convenient when you own the
environment: Synchronet has no per-door environment setting in `xtrn.ini`, so on
a live BBS prefer the door-directory drop-in (above) or `-data`.

---

## How the door reaches the player

Two ways, and the BBS chooses:

- **A socket** — `DOOR32.SYS` line 2 (comm type **2**), or `-s<fd>`. This is what
  Synchronet gives it. Note that Synchronet does **not** hand a door the *raw
  client socket*: it opens a loopback socketpair, gives the door one end, and
  pumps between that and its own ring buffers — so the BBS does the telnet
  negotiation and the SSH crypto, and the door never sees a telnet IAC byte. That
  is why it works over SSH without knowing SSH exists.

- **The BBS's stdin/stdout** — `DOOR32.SYS` comm type **0** ("local"), which means
  *there is no socket, because I redirected your stdio*. Synchronet writes exactly
  that when an external program is registered `XTRN_STDIO`; Mystic does the same
  when it forks a door on \*nix. The door then reads fd 0 and writes fd 1, and the
  BBS does the telnet/SSH work as before. **\*nix only**: on Windows a door is
  handed a Winsock `SOCKET`, and the I/O seam uses `send()`/`recv()`, which cannot
  touch a CRT pipe.

The drop file is parsed by [`../termgfx/door32.c`](../termgfx/door32.h), shared by
every door — including the case each of the five copies used to miss, comm type 0.
A drop file the door cannot use (serial, an unknown comm type, a telnet line with
a dead handle, a truncated file) is reported rather than left to fail silently.

**On a stdio door the BBS `dup2()`s stderr onto the player's stream**, so anything
written there is painted over the game. Diagnostics are therefore captured to
`data/<door>/<door>_n<node>.log` — look there when a stdio door misbehaves.

The door does **not** speak telnet itself. A BBS that hands over the *raw client
socket* would need it to negotiate options, unescape `IAC IAC`, and survive a
window-resize (NAWS) whose payload bytes arrive looking like keystrokes — one of
which, `0x11`, is the quit key. Use the stdio mode with such a BBS.

`-stdio` on the command line forces the stdio path for a BBS that
writes no drop file at all.

## Building

Out-of-source **CMake** build, same shape as `../syncdoom` / `../syncduke`:

```sh
./build.sh            # Release build -> build/syncmoo1
./build.sh debug       # Debug build
./build.sh clean       # wipe ./build/ first
./build.sh debug clean # combine
```

Equivalently, by hand:

```sh
cmake -B build && cmake --build build -j
```

Building never touches any live install -- it only writes under `./build/`.
Run `./deploy.sh` afterwards to install the freshly-built binary into the
door's live `xtrn` directory (see that script's own header for exactly where
and how; it's a separate, explicit step on purpose, so a sysop can rebuild and
test before pushing a new binary live).

### Windows (MSVC)

```
build.bat            :: Win32 release -> build-msvc\Release\syncmoo1.exe
build.bat clean      :: wipe the build tree first, then build
deploy.bat           :: install the built exe into xtrn\syncmoo1\
```

Needs Visual Studio 2022 (`build.bat` finds the bundled CMake if none is on
`PATH`). The JPEG-XL tier is picked up from a classic-mode vcpkg prefix at
`C:\vcpkg\installed\x86-windows-static-md` when present -- install it with
`vcpkg install libjxl:x86-windows-static-md`. Absent, the door still builds and
serves the sixel tier. As on *nix, building does not deploy.

**Win32 (x86) is the one supported Windows target.** A Win32 door runs on both
a Win32 and a future Win64 Synchronet host -- the DOOR32.SYS comm handle is
32-bit-significant and crosses the process-bitness boundary fine -- so one Win32
binary covers every Windows BBS, present and future, and MoO1 through 1oom gains
nothing from x64 anyway. The code is 64-bit-clean (1oom is; our glue is), so
`cmake -A x64` still compiles, but no x64 binary is shipped or tested.

### Notes

The `CMakeLists.txt` enumerates 1oom's `game/`+`os/`+`ui/` sources (derived
from the vendored `Makefile.am`) plus `hw_sbbs.c` and the `syncmoo1_*.c`
glue, links the shared `../termgfx` library and `xpdev`, and absorbs the
vendored engine's legacy-code compiler warnings (`-w -fcommon`, `/w` on MSVC)
rather than patching upstream -- see [CLAUDE.md](CLAUDE.md) for that policy,
and [PROVENANCE.md](PROVENANCE.md) for the three Windows-only workarounds that
keep `1oom/` unedited. `libjxl` is an optional dependency (pkg-config /
`find_library`, vcpkg on MSVC); its absence degrades to the sixel/text tiers,
it does not fail the build.

All of the door's Windows-vs-POSIX difference lives in `syncmoo1_plat.c`
(clock, sleep, non-blocking socket I/O, Winsock bring-up), implemented over
xpdev, plus `syncmoo1_os_win32.c` (1oom's `os` backend for Windows).

---

## Launching / command-line syntax

```
syncmoo1 [door opts] [1oom engine opts]
```

The door consumes its own options and leaves everything else for 1oom's
engine argument parser, so any of 1oom's native switches (e.g. `-data <dir>`)
pass straight through.

A typical BBS invocation (the BBS fills in the DOOR32.SYS drop-file path and
the per-user storage directory):

```
syncmoo1 <path>/door32.sys -home <data>/user/<num>/moo1/
```

No `-name` is needed: DOOR32.SYS line 7 already carries the user's alias.

| Option | Meaning |
|--------|---------|
| `<path>/door32.sys` | A bare path whose filename is `door32.sys` (case-insensitive) is auto-detected as a **DOOR32.SYS** drop file: line 1 comm type (`2`=telnet/socket), line 2 socket handle, line 7 user alias, line 9 minutes left. |
| `-s<fd>` | Client socket descriptor directly (glued form, e.g. `-s7`), when not launching from a drop file. Digit-suffix-only, so it never collides with 1oom's own `-s...` word options (`-sfx`, `-skipintro`, `-savequit`, ...). |
| `-t<seconds>` | Session time limit in seconds; the door exits cleanly (restoring the terminal) when it elapses. Same digit-suffix-only rule as `-s<fd>`. |
| `-name <handle>` | The player's alias, for the **drop-file-less** dev path (`-s<fd>`/`SYNCMOO1_SOCK`). A DOOR32.SYS drop file always wins: its line 7 is the BBS's own statement of who the user is, and `-name` cannot override it whatever the argument order. The alias pre-fills the new-game emperor name (truncated to the 11 characters that screen lets the player edit); with no alias, 1oom invents one as usual. |
| `-home <dir>` | Per-user sandbox directory: the door `mkpath`s and `chdir`s into it and points 1oom's config/save path at it (`os_set_path_user()`), so saves and settings don't collide across BBS nodes/users. Created if it doesn't exist. |
| `-help`, `--help`, `-?` | Print the door's own option summary and exit (checked before any session resolution, so it wins even given a well-formed drop file). |

DOOR32.SYS does not carry the screen row/pixel geometry; the door probes the
real terminal size and cell dimensions at connect (DESIGN.md §9).

---

## Audio

Both of MoO1's audio channels play over SyncTERM's audio APC, on any terminal
that answers the capability probe. A terminal without it hears silence and is
otherwise unaffected -- nothing is uploaded, nothing is rendered.

**Sound effects** are the LBX-wrapped Creative VOC samples, shipped to the
client once each and thereafter played by name.

**Music** is MoO1's 40-track soundtrack. Each track arrives as LBX-wrapped XMI;
the door converts it to MIDI, and `termgfx` renders it through ADLMIDI, encodes
OGG, and uploads it -- all on a worker thread, so a first play never stalls the
frame loop. Tracks whose XMI carries no loop-start controller (the intro
fanfare, the ending theme) play **once** and stop, rather than looping.

Both kinds are **content-addressed**: the cache name is a hash of the bytes
that determine the sound, so identical audio is stored once, and changing the
game data -- or the door's own converter -- renames the entry and invalidates
every stale copy automatically.

Rendered music is also cached **door-side** under `data/syncmoo1/audio/`, so a
track rendered for one player ships straight from disk for the next.

Volumes and on/off are 1oom's own settings (`opta.music`, `opta.music_volume`,
`opta.sfx`, `opta.sfx_volume`), changeable in-game under Options -> Sound and
seedable by the sysop -- see [`syncmoo1.ini`](#syncmoo1ini). Setting a music
volume of 0 stops the channel rather than uploading a track nobody will hear.

---

## `syncmoo1.ini`

Door configuration file, read from the **launch directory** (the door's
`xtrn/syncmoo1/` install dir) at startup.

Synchronet sysops get it for free: `jsexec install-xtrn ../xtrn/syncmoo1`
seeds `syncmoo1.ini` from `xtrn/syncmoo1/syncmoo1.example.ini` as part of the
install, and never overwrites one that already exists. (Outside Synchronet, or
for an install that predates this step, copy the example by hand.)

**Read it before your first player does.** With no `syncmoo1.ini` the door runs
on 1oom's stock defaults, and MoO1's copy protection is armed: after year 40,
on a random turn, it demands a ship name printed in the boxed manual and ends
the game after three wrong answers. The shipped example disables it, and
enables 1oom's quality-of-life preset besides. Every key is optional; a missing
file or key falls back to its default.

| Section | Key | Meaning |
|---------|-----|---------|
| `[audio]` | `music_quality` | Ogg/Vorbis VBR quality (`0.0`..`1.0`) for encoded music tracks. Lower = smaller upload, softer sound. Default: the termgfx-wide default. |
| `[debug]` | `wire` | Record both directions of the terminal conversation to `data/syncmoo1/syncmoo1_n<node>.wire` -- multi-MB per session, per node. Decode with `tools/wiredump.py`. Default `false`; only turn this on while actively debugging the door. |
| `[1oom]` | `<module>.<item>` | Defaults for the engine's own options, keyed by 1oom's cfg names (modules: `opt`, `opta`, `game`, `hw`, `hwx`, `ui`). Applied through 1oom's own parser, so its per-item range checks validate them. These are **defaults, not overrides**: a player who changes one in the in-game options menu keeps their choice; a player who never touches it follows whatever you set, even if you change it later. |

`SYNCMOO1_WIREDUMP=<path>` forces the wire dump on (writing to `<path>`
instead of the usual `data/syncmoo1/...` location) for a one-off debug run
without editing the ini; `SYNCMOO1_WIREDUMP=-` forces it off even if the ini
enables it.

---

## Logging & diagnostics

The door records diagnostics to two files, so nothing is lost even with the
local console hidden (`XTRN_NODISPLAY`, which `install-xtrn.ini` sets -- see
its comments):

- **`<-home>/1oom_log.txt`** -- the **1oom engine's** own log (LBX search,
  config load, the version banner, engine errors). Opened unconditionally by
  the engine, in the per-user `-home` directory.
- **`data/syncmoo1/syncmoo1_n<node>.log`** -- the **door's** own diagnostics
  (client hangup reason, `-home`/session-setup failures, the time-limit exit).
  Written only on Windows under a live BBS session, where a console-less
  process would otherwise have nowhere to print them; on *nix these go to the
  inherited stderr (the Synchronet terminal-server log) as usual. Truncated
  per session, node-tagged so concurrent nodes don't collide.

---

## Development environment variables

Useful when developing/testing off a live BBS session:

| Variable | Meaning |
|----------|---------|
| `SYNCMOO1_SOCK` | Dev fallback client socket descriptor, used only when nothing on the command line resolves one (e.g. point it at one end of a `socketpair()`). Without a socket, output falls back to fd 1 (stdout) for a plain dev/tty run. |
| `SYNCMOO1_SIXELOUT=<path>` | Offline sixel-capture mode: every `sm_io_present()` call **overwrites** `<path>` with one self-contained sixel frame (palette always included), instead of writing to the live socket/stdout. Lets a captured frame be decoded independently (e.g. with ImageMagick) with no live terminal or socket involved -- useful for verifying the render path without a real SyncTERM session. |
| `SYNCMOO1_LBX=<dir>` | Shared, read-only MoO1 LBX data directory -- see [above](#master-of-orion-1-data-files-lbx----you-must-supply-these). |

---

## Testing against a fake terminal (`tools/fakterm.py`)

A door's output only means anything to a terminal: sixel bytes and audio
APCs are just bytes on a wire until something on the other end decodes and
plays them. The bugs that matter most -- a column the scale+encode path
drops, a music track that loops when it shouldn't -- are invisible to the
unit tests in `tests/`, because those never run the terminal-facing code in
`hw_sbbs.c`/`syncmoo1_io.c` at all.

`tools/fakterm.py` closes that gap: it runs the **real, built** `syncmoo1`
binary under a raw pty, answers its capability probes and per-frame DSR
pacing acks the way a real SyncTERM session would, and decodes the sixel
frames and `SyncTERM:*` audio APCs it sends back off the wire
(`tools/sixdecode.py` holds the shared decoder, also used by
`tools/wiredump.py`). It is the only tool in this tree that can assert
whether a music track shipped with the loop flag (`;L`) set.

```
cd src/doors/syncmoo1
./build.sh                                          # build/syncmoo1 must exist
python3 tools/fakterm.py 80 25 --lbx /path/to/MoO1/LBX/files
python3 tools/fakterm.py 80 25 --lbx /path/to/LBX --seconds 120 --audio
```

The first form drives the door for a few seconds and reports the last
decoded sixel frame's size and palette. `--audio` reports one line per
audio APC instead (Store/Load/Queue/Volume/Flush), naming the cache file,
channel, slot, volume, and whether `;L` (loop) was set -- MoO1's intro
fanfare Queues without it, its looping menu theme Queues with it, and both
show up in one long-enough run. Reaching the menu theme needs a run of
roughly two minutes (`--seconds 120`+): the intro cinematic runs at its own
real-time pace regardless of terminal I/O speed, and `--seconds` is
wall-clock, not a frame count. `--click`/`--click-game` inject SGR mouse
clicks (1oom's menus are mouse-driven; letter hotkeys are unreliable), and
`--launch-dir` points the engine's cwd (where `syncmoo1.ini` is read from)
somewhere other than `--lbx`, so a `[1oom]` setting can be exercised without
touching the real LBX install dir. Run `python3 tools/fakterm.py --help`
for the full option list and see the traps documented in the module's own
docstring (raw pty mode, the one-event present() lag, and the
`-new`-skips-the-intro side effect).

**A gotcha found while building this tool, worth recording:** seeding
`[1oom] opta.music_volume = 0` via `syncmoo1.ini` does NOT mute the music
that ships on the wire, even though the door's own base-config snapshot
confirms the seed was applied to 1oom's `opt_music_volume`. The applied
volume our door tracks (`syncmoo1_music.c`'s `g_vol`) is only ever updated
by the `hw_audio_music_volume` hook, which 1oom's engine calls **exclusively
from the in-game Sound options page's volume-slider callback**
(`main_menu_update_music_volume()`, `ui/classic/uimainmenu.c`) -- never
proactively at startup from the loaded config. So a config-seeded volume of
0 is real (it's what a later slider read would show), but it never reaches
playback until the player -- or a harness driving a real slider click --
actually touches the slider. Proving "volume 0 -> stop, no Store" therefore
needs an interactive `--click-game` sequence into the Sound page, or the
live SyncTERM gate itself; a static ini value alone will not do it.

### Why `SYNCMOO1_SIXELOUT` capture mode cannot substitute for this

`SYNCMOO1_SIXELOUT` (see the environment-variable table above) looks like a
shortcut to the same thing -- it dumps a sixel frame to a file with no
terminal involved -- but it structurally cannot exercise the same code, for
two reasons:

1. **It bypasses scale+encode entirely.** Capture mode
   (`syncmoo1_io.c`'s `g_file_mode`) encodes the **native 320x200**
   framebuffer directly at its native size, skipping the fit/scale/encode
   path (`sm_io_scale_indices()` + the real page-geometry math) that a live
   terminal session always goes through. A bug in scale+encode -- the kind
   `fakterm.py` actually found -- literally cannot show up in a capture-mode
   frame, because that code path never runs.
2. **It never sets `O_NONBLOCK`.** `sm_io_init()` only makes the socket
   non-blocking in the `else` branch, i.e. when `SYNCMOO1_SIXELOUT` is
   *not* set; capture mode leaves the fd blocking. Its input pump then
   blocks on `read()` the moment there's nothing buffered, which is fine
   for a single non-interactive snapshot but means capture mode cannot
   drive any real back-and-forth (probe replies, mouse clicks, pacing
   acks) the way `fakterm.py` does.

Capture mode is still useful for a quick "does *something* render"
sanity check with zero moving parts; it is not a substitute for a real
wire-level test.

---

## License & credits

Master of Orion is © Simtex / MicroProse. The 1oom engine is vendored under
its own **GPLv2** license; this door's own glue code is likewise GPLv2. See
[CREDITS](CREDITS) for full attribution and [PROVENANCE.md](PROVENANCE.md) for
the vendored engine's exact commit/date/license verification.
