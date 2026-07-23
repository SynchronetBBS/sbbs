# SyncRetro

Play **legacy game consoles** (Intellivision, NES, ColecoVision, ...) as a
Synchronet / SyncTERM **door**, by hosting a [libretro](https://www.libretro.com/)
core and rendering it to the terminal through the shared
[`../termgfx`](../termgfx) library -- the same graphics/audio/pacing stack as
`../syncdoom`, `../syncduke`, `../syncmoo1`.

**One door, many consoles:** SyncRetro is a minimal libretro *frontend*. It
vendors no emulator -- you point it at a libretro core `.so` and it drives the
core's run loop, converting each video frame to sixel/JXL and mapping BBS input
to a RetroPad. Swap the core, get a different console.

> **Status: video, input, multi-core, audio, and the Windows build are all
> complete.** The frontend loads a core, loads a ROM, renders frames as sixel,
> maps terminal keys onto a RetroPad -- including all twelve Intellivision keypad
> keys -- and streams the core's audio to SyncTERM as Opus chunks at whatever rate
> the core mixes at. Enter/probe/leave and teardown are correct on every exit path.
>
> **Two consoles ship:** Intellivision (`xtrn/syncivision`, FreeIntv) and NES
> (`xtrn/syncnes`, FCEUmm). They are the SAME door binary against a different
> core, and adding a third that is also a plain-gamepad console (SMS,
> ColecoVision, PC Engine, Genesis) needs no C at all -- just an install
> directory. See the multi-core notes (`M3_MULTICORE.md`) and DESIGN.md sec 15
> for what remains: core options and save states.
>
> To actually *play* Intellivision you still need the BIOS (below). Without it
> FreeIntv loads the cartridge, halts its CPU, and draws its own "PUT GROM/EXEC IN
> SYSTEM DIRECTORY" screen -- which the door renders correctly, and which is a
> useful way to confirm your install before you have the BIOS.

## What you supply

1. **`libretro.h`** -- the API header, vendored into this dir before building
   (see [PROVENANCE.md](PROVENANCE.md)).
2. **A libretro core `.so`** -- e.g. `FreeIntv_libretro.so` (Intellivision). GPL /
   freely redistributable.
3. **Console BIOS + game ROMs** -- **commercial, copyrighted content SyncRetro
   does not ship.** A legally-owned copy is yours to supply (same as MoO1's data
   in `../syncmoo1`).

## Running under another BBS

The door takes its client connection one of two ways, and a BBS that offers
either can run it:

- **A socket** — a `DOOR32.SYS` drop file (line 2 is the handle) or `-s<fd>`.
  This is what Synchronet gives it. Note that Synchronet does NOT hand a door the
  raw client socket: it interposes a loopback socketpair and pumps the data
  through its own threads, which is what does the telnet negotiation and the SSH
  crypto. The door therefore never sees a telnet IAC byte, and works over SSH
  without knowing SSH exists.
- **`-stdio`** — the BBS redirected our stdin and stdout instead (Mystic does
  this on \*nix, forking the door through `/bin/sh` with pipes). It does the
  telnet and SSH itself, so the stream arrives just as clean. **POSIX only**: on
  Windows a door is given a Winsock socket, and this door's I/O seam cannot read
  a CRT pipe.

The drop file is parsed by [`../termgfx/door32.c`](../termgfx/door32.h), shared by
every door; a drop file it cannot use (serial, an unknown comm type, a telnet line
with a dead handle, a truncated file) is reported rather than left to fail
silently. And because the BBS `dup2()`s a stdio door's stderr onto the player's
stream, diagnostics are captured to `data/syncretro/syncretro_n<node>.log` instead
of being painted over the game.

What the door does NOT do is speak telnet itself. A BBS that hands over the
**raw client socket** would need that: the door would have to negotiate options,
unescape `IAC IAC`, and survive a window-resize (NAWS) whose payload bytes arrive
looking like keystrokes — one of which, `0x11`, is the quit key. Use the stdio
mode with such a BBS, or add a telnet layer first.

## Building

In brief below; [COMPILING.md](COMPILING.md) has the prerequisites, the
configure options, the tests and the per-platform core install.

POSIX (Linux/*BSD/macOS):

```sh
./build.sh            # Release -> build/syncretro
./build.sh debug
./build.sh clean
```

Windows (Visual Studio 2022, Win32):

```bat
build.bat             :: Win32/Release -> build-msvc\Release\syncretro.exe
build.bat clean
```

Then install the built binary with **`deploy.js`** (both platforms):

```
jsexec src/doors/syncretro/deploy.js          # -> the in-tree bundle
jsexec src/doors/syncretro/deploy.js <dir>    # also -> a live bundle dir
```

`deploy.js` is a jsexec script rather than a shell/batch pair on purpose: it
computes the per-target sub-directory with the SAME `sv_target()` the lobby uses,
so the two can never disagree (see Installing).

Links `../termgfx` + `xpdev`; loads the core at runtime (`dlopen` on *nix,
`LoadLibrary` on Windows -- the core is a `.so` or a `.dll` to match). `libjxl`
is optional (absence degrades to sixel/text); on Windows it comes from a
classic-mode vcpkg prefix (`vcpkg install libjxl:x86-windows-static-md`).

**Win32 is the one supported Windows target**, deliberately: a Win32 door runs
on both a Win32 and a future Win64 Synchronet host (the DOOR32.SYS comm handle
is 32-bit-significant and crosses the process-bitness boundary), and it must
load a Win32 core `.dll`.

On Windows a BBS launches the door with the local console hidden
(`XTRN_NODISPLAY`), so the door's own diagnostics -- the hangup reason, `-home`
/ session errors -- are captured to `data/syncretro/syncretro_n<node>.log`
instead of a dead console. (On POSIX they stay on the inherited stderr, which
the terminal server already logs.)

## Installing

The door's own directory is the install root. Where the native artifacts -- the
door binary and the libretro core -- go depends on the platform, so one shared
install can serve several hosts at once (a BBS spanning a Linux box and a Windows
host, or two *nixes of different architecture) without their binaries colliding:

- **Windows is flat** -- `syncretro.exe` and `freeintv_libretro.dll` sit at the
  door root. Their `.exe`/`.dll` extensions already distinguish them from a *nix
  host's `syncretro`/`.so`, and there is only one Win32 target, so no sub-dir is
  needed.
- **A *nix host uses an `<os>-<arch>` sub-directory** (`linux-x64`, `linux-arm64`,
  `freebsd-x64`, `darwin-arm64`, ...) -- because every *nix build is named the
  same (`syncretro` / `freeintv_libretro.so`), so two of them WOULD collide at
  the root. The lobby **prefers that sub-directory but falls back to the flat
  door dir** when it isn't populated -- so a single-host install, or the legacy
  symlink-deploy layout (the live bundle symlinks `syncretro` straight to the
  build output), keeps working with the binary/core loose at the top level.

`deploy.js` installs the binary and `getcore.js` the core, both deriving the
location from the same `sv_target()` the lobby uses, so they always agree. The
BIOS and cartridges are platform-independent and stay at the root:

```
xtrn/syncivision/
    linux-x64/                    a *nix host uses an <os>-<arch> sub-dir, since
        syncretro                 its "syncretro"/".so" would collide flat with
        freeintv_libretro.so      another *nix host's
    syncretro.exe                 Windows is FLAT: .exe/.dll never collide with a
    freeintv_libretro.dll         *nix host's names, so no sub-dir is needed
    exec.bin  grom.bin            the BIOS you supply (case-sensitive names)
    roms/                         cartridges
        4-tris.rom
```

The BIOS is found in the door's own directory (its start-up directory in SCFG),
and cartridges are looked up in `roms/` -- so a bare ROM name is enough. Override
the BIOS location with `$SYNCRETRO_SYSTEM` if you keep it elsewhere.

## Launching

```
syncretro <path>/door32.sys -name "<user>" -home <dir> -core freeintv_libretro.so 4-tris.rom
```

DOOR32.SYS supplies the socket + time left; the door probes terminal geometry at
connect. `-home` is the per-user sandbox that SRAM and save states land in. Run
`syncretro -help` for the full option list.

### Controls

See [Keys](#keys) below for the full key list.

How well *holding* a direction works depends on your terminal, because a plain
terminal never reports a key being released:

| Terminal | Key-up | Feel |
|---|---|---|
| SyncTERM | physical key reports (CTDA cap 8) | exact; layout-independent (WASD works on AZERTY) |
| kitty, foot, ... | kitty keyboard protocol | exact |
| anything else | none -- a short auto-release timer | a held direction stutters once before it flows |

The door negotiates the best available path at connect and restores the
terminal's key mode on exit.

## Keys

Both Intellivision hand controllers are driven at once: use whichever one a cart
reads, or play two-up on one keyboard.

| Player 1 | Player 2 | Action |
|---|---|---|
| `W A S D` | arrow keys | disc (16-way) |
| `Z X C` | `, . /` | action buttons |
| `1 - 9, 0` | numeric keypad | keypad digits |
| `Backspace` | numpad `Del` | keypad Clear |
| `Enter` | numpad `Enter` | keypad Enter |

Door keys (either player): `Tab` swap the two controllers · `Space` pause · `?`
help · `Ctrl-S` stats overlay · `Ctrl-R` reset · `Ctrl-Q` quit.

Player 2's arrows and numpad have no ASCII form, so they need SyncTERM or a
kitty-keyboard terminal; on a plain client player 2's *keypad* falls back to
player 1's (disc + buttons still work). Press `?` in-game for this same list.

## Audio

Requires a SyncTERM build with **libsndfile**; without it the door is silent by
design and sends no audio APCs at all. It does still *ask* -- the capability
query is how the door learns whether the terminal can decode -- so a non-SyncTERM
client sees one harmless APC at connect and nothing further. The query itself is
suppressed only when the door was built without libsndfile, or by
`[audio] enabled = false`, which turns the whole module off. On a capable
terminal the
core's audio streams as 100 ms mono Opus chunks (roughly 1.2-2.0 KB each, about
20 KB/s sustained) over one of SyncTERM's audio-mixer channels, with a short
prebuffer cushion against link jitter and silence replayed from a single
cached sample at essentially no bandwidth cost. A game that is briefly silent
on its own -- Astrosmash and BurgerTime both do this on their title screens --
sends no audio traffic until it actually makes a sound; that is expected, not
a stuck stream. On exit the door logs a summary line:

```
syncretro: audio N chunks, N underrun(s), N drop(s)
```

Tune it in `syncretro.ini`'s `[audio]` section (see
[M4_AUDIO.md](M4_AUDIO.md) sec 5.1 for the reasoning behind each default):

| Key | Default | Clamp | Meaning |
|---|---|---|---|
| `enabled` | `true` | -- | `false` emits no audio APCs at all (not the same as `volume = 0`) |
| `quality` | `0.15` | 0.01 .. 1.0, else default | Opus VBR quality; out-of-range (including NaN) falls back to the default rather than clamping |
| `volume` | `100` | 0 .. 100 | channel base volume |
| `chunk_ms` | `100` | 50 .. 250 | chunk size; below 50 the Ogg headers dominate the stream |
| `prebuffer` | `3` | 2 .. 8 | chunks held before playback starts, i.e. 200..800 ms of cushion |

## Constraints

Software-rendered cores only (no GL/Vulkan -- correct for a terminal). Legacy
2D consoles are the sweet spot -- their tiny, low-frame-rate output suits
terminal rendering well.

Frames are truecolor, and sixel is a 256-color format, so each frame is quantized
([`syncretro_quant.c`](syncretro_quant.c)). Legacy consoles use far fewer than
256 colors, so the reduction is normally **exact** and the palette is stable.
On SyncTERM, sixel color registers persist between images, so that stable
palette is defined once and reused. Other terminals (foot, xterm, Windows
Terminal, etc.) reset their color registers on every image, so there each
image -- including each dirty-rect patch -- carries only the small subset of
colors it actually uses rather than a full palette. That per-image subset is
also what lets dirty-rect patching work on those terminals now, not just on
SyncTERM: menu/static screens and stable-palette games benefit most, while
content that changes its color set every frame (NTSC/dither-filtered cores,
for example) still falls back to full frames.

## Diagnostics

`[debug] dirty_log = true` in `syncretro.ini` turns on a per-frame trace of
the dirty-rect decision -- why a given frame took the patch path or the full
repaint path. Unlike this file's other diagnostic lines, it does NOT go to
stderr: a native door is launched `EX_BIN`, and the BBS eats its stderr
outright, so an `fprintf(stderr, ...)` here would be invisible under a real
session. Instead it is its own `fopen(..., "a")`'d file (mirrors
`../syncduke/syncduke_log.c`), fflush'd after every line so a killed/hung
session still leaves a readable tail, at:

- `$SBBSDATA/syncretro/syncretro_dirty_n<node>.log` when the BBS sets
  `$SBBSDATA` (every native door launch on a real session) -- a different
  basename than the existing `syncretro_n<node>.log` capture file, so this
  opt-in trace can't collide with it.
- otherwise (a dev/direct run with no BBS session) `syncretro_dirty.log`
  beside `syncretro.ini`, in whichever directory the door was launched from.

Off by default -- it is one line per non-deduped frame, meant for a short
session, not to run continuously.

One line is logged once, the first time geometry settles, with the numbers
that govern band alignment (see `syncretro_dirty.c`'s band_align comment):

```
syncretro: dirty-log geometry cell=8x16 image=352x208 vstep=48 hcell=208 syncterm=0
```

Then one line per non-deduped frame:

```
syncretro: dirty syncterm=0 band_align=1 cfg=1 force=0 palchg=0 haveprev=1 cell=8x16 geom=1 path=dirty nrect=2
syncretro: dirty syncterm=0 band_align=1 cfg=1 force=0 palchg=0 haveprev=1 cell=8x16 geom=0 ew=352 eh=208 last_ew=344 last_eh=208 icol=3 irow=2 last_icol=3 last_irow=2 path=full gate=geom
syncretro: dirty syncterm=0 band_align=1 cfg=1 force=0 palchg=0 haveprev=1 cell=8x16 geom=1 path=full reason=coverage pct=52%
```

`path=full gate=<name>` means the gate in `sr_io_present()` itself blocked
the dirty-rect attempt before `sr_dirty_find()` was ever called (`cfg` =
`[video] dirty_rect` is off, `force`/`palchg` = something else forced a
repaint, `haveprev` = no previous scaled frame yet, `cell` = no usable cell
grid, `geom` = the image moved/resized since the last frame the client has --
the `ew`/`eh`/`icol`/`irow` fields only appear when `geom=0`, so you can see
what changed). `path=full reason=<name> pct=N%` means the gate passed and
`sr_dirty_find()` itself returned "send a full frame", with `reason` one of
`nothing`, `coverage`, `components`, `fragmented`, `bandfit`, `grid`, `args`
(see `syncretro_dirty.h`) and `pct` the dirty-cell percentage it measured
against the `SR_DIRTY_FULL_PCT` (40%) cutoff.

The Ctrl-S overlay's `dr N%` reading is a **rolling ~2s window** (the same
window the fps/KB-per-second readout uses), not a lifetime average -- it
reflects what the current screen is doing, not the whole session.

## License

Frontend glue: GPL-2.0 (matching the door family). libretro API: permissive.
Cores: their own licenses (FreeIntv GPLv2+). BIOS/ROMs: their rights holders'.
