# SyncDOOM

A multiplayer **DOOM** that runs as a BBS *door* (external program), rendering
the game to the user's terminal over the telnet/SSH session — no native client,
no X11. It is a [doomgeneric](https://github.com/ozkl/doomgeneric) /
[Chocolate Doom](https://www.chocolate-doom.org/) port whose video, input and
networking back end (`syncdoom.c`) talks directly to the terminal and the
BBS-supplied TCP socket. It runs on any **DOOR32.SYS**-compatible BBS, and looks best
on **SyncTERM** (real graphics via its APC/JXL image-cache protocol).

Co-op and deathmatch are supported, played between BBS nodes (or against a
detached dedicated server). The companion JavaScript lobby that browses and
creates network games is host-specific and lives elsewhere; this directory is
the portable C door and its two build systems.

---

## Rendering tiers

The door picks the best picture the terminal can show, probing at startup. From
richest to most compatible:

| Tier | Wire format | Terminals |
|------|-------------|-----------|
| **JXL** | JPEG-XL still, sent as a cached APC image | SyncTERM 1.2+ (smallest, fastest) |
| **PPM** | uncompressed RGB APC image | SyncTERM without JXL (opt-in; LAN/localhost only) |
| **Sixel** | DECSIXEL raster | xterm, mlterm, foot, WezTerm, recent Windows Terminal, … |
| **Text** | half/quadrant/sextant block glyphs in ANSI color | any ANSI/UTF-8 or CP437 terminal |

Auto-selection order is **JXL → Sixel → Text** (PPM is opt-in only). Any tier can
be forced from the command line (`-jxl`, `-sixel`, `-text`) or the config file.

JXL support is a compile-time option; a build without libjxl still serves the
sixel and text tiers.

---

## Building

The door is plain C and links one library, **xpdev** (cross-platform sockets,
recursive `mkdir`, and INI parsing), which is part of the Synchronet source
tree. Building therefore requires a checkout of that source tree, not just a
binary install.

### CMake (recommended; cross-platform)

```
cmake -B build
cmake --build build -j
```

Produces `build/syncdoom`. The CMake build compiles its own minimal copy of
xpdev (cryptography and audio back ends disabled), so the rest of the tree need
not be built first.

Options:

- `-DWITHOUT_JPEG_XL=ON` — build without the JPEG-XL tier (sixel/text only).
- `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<dir>` — write the binary straight to `<dir>`
  (e.g. your door's install directory) instead of `build/`.

Prerequisites: CMake 3.13+, a C compiler (GCC or Clang), and — for the JXL tier
— the libjxl development package (`libjxl-dev` / `libjxl-devel`) plus
`pkg-config`.

### Platform support

- **Linux / Unix-like** (GCC or Clang) — supported.
- **Windows / MSVC** — planned. The doomgeneric platform layer and Winsock
  wiring are not yet in place; build on a Unix-like host for now.

---

## Command-line syntax

```
syncdoom [door/session options] [WAD options] [DOOM/engine options]
```

The door consumes the options it recognizes and passes everything else through
to the underlying DOOM engine (so standard Chocolate Doom switches such as
`-warp`, `-skill`, `-nomonsters`, `-deathmatch`, `-altdeath`, `-connect`,
`-server` work as usual).

A typical door invocation:

```
syncdoom %f -l%R -t%T -home <per-user-dir> -iwad <wad> -name "<user>"
```

where `%f`, `%R`, `%T` are the BBS's substitutions for the drop-file path, screen
rows, and time left.

### Session / terminal

| Option | Meaning |
|--------|---------|
| `-s<fd>` | Client comm **socket** descriptor (the connected telnet/SSH socket the BBS hands off). Glued form `-s7` or spaced `-s 7`. |
| `-l<rows>` | **Fallback** terminal row count. The door auto-detects the live screen size at startup; `-l` (and terminal.ini) are consulted only when the terminal doesn't answer the size probe. Glued or spaced. Default 25. |
| `-t<seconds>` | **Time limit** for the session in seconds; the door exits when it elapses. Glued or spaced. |
| `-door32 <path>` | Path to a **DOOR32.SYS** drop file (see below). A bare path whose name is `door32.sys` is also auto-detected without this flag. |
| `-term <path>` | A `terminal.ini` file (or a directory containing one) describing `cols`/`rows`/`chars`/`desc`. Sets the baseline; explicit flags override. |
| `-home <dir>` | **Per-user storage** directory. The door `chdir`s here so DOOM's config, savegames and screenshots are written per user. Created if absent. |
| `-name <handle>` | Player name shown in multiplayer (chat, scoreboard). Default `Player`. |

### Video

| Option | Meaning |
|--------|---------|
| `-jxl [0\|1\|auto]` | Force the JXL tier on (`1`), force PPM (`0`), or auto. |
| `-sixel [0\|1\|auto]` | Force sixel on (`1`), off (`0`), or auto. |
| `-text` | Force the text/block-glyph tier. |
| `-mode <m>` | Text-tier glyph mode: `half` (default), `quadrant`, `sextant`, or `space`. |
| `-charset <c>` | `utf-8` or `cp437` (text tier). |
| `-colors <c>` | Color depth: `16` (default), `8`, `256`, or `true`/`24`. |
| `-jxldistance <d>` | JXL lossy quality/size lever (Butteraugli distance; ~1.0 lossless, higher = smaller). JXL builds only. |

### Input (key-repeat tuning, milliseconds)

Terminals send key-*down* only, so a held key is treated as released a short
grace period after its last byte. These tune that synthesis:

| Option | Meaning |
|--------|---------|
| `-kpsmooth <ms>` | Repeat grace (held key). |
| `-kpdelay <ms>` | Initial-tap grace. |
| `-kpturn <ms>` | Turn-key tap grace. |

### WAD selection (passed through to the engine)

| Option | Meaning |
|--------|---------|
| `-iwad <file>` | Base IWAD (e.g. `doom2.wad`, `freedoom1.wad`). |
| `-file <wad…>` | One or more PWADs. |
| `-merge <wad…>` | WADs merged into the IWAD namespace. |

WAD paths are resolved to absolute *before* the door `chdir`s into `-home`, so
relative WAD paths work regardless of the storage directory. If `-iwad` names a
bare file, the engine's standard IWAD search applies (e.g. the `DOOMWADDIR`
environment variable).

### Multiplayer server

| Option | Meaning |
|--------|---------|
| `-dedicated` | Run as a headless dedicated server (the match's tic relay; no terminal). Reads `-port`. |
| `-spawnserver [-port <n>] [meta…]` | Daemonize a detached `-dedicated` server and exit, printing `<pid> <port>` on stdout. With no `-port`, a free port is allocated. Extra args are forwarded to the server. |
| `-port <n>` | UDP port for the dedicated/spawned server. |

Clients join a server with the standard Chocolate Doom `-connect <host>` (passed
through). `-deathmatch` / `-altdeath` on the *creating* client select the game
type for the match.

---

## DOOR32.SYS drop file

DOOR32.SYS is the portable door-info interface written by most BBS software. The
door reads three fields:

| Line (1-based) | Field | Used for |
|----------------|-------|----------|
| 1 | Comm type | `2` = telnet socket (otherwise the socket is not taken from the drop file) |
| 2 | Comm/socket handle | The client socket descriptor (same as `-s`) |
| 9 | Time left, **minutes** | Session time limit (same as `-t`, but in minutes) |

DOOR32.SYS does **not** carry the screen row count. The door auto-detects it (a
live size probe, then terminal.ini if present); pass `-l<rows>` as a fallback for
terminals that don't answer the probe on hosts without a terminal.ini. An
explicit `-s` / `-t` on the command line overrides the drop-file values.

A drop-file path is recognized automatically when its filename is `door32.sys`
(case-insensitive), so a BBS that passes the drop-file path as a bare argument
needs no `-door32` flag.

---

## Terminal sizing & geometry

For the graphics tiers the door needs the terminal's pixel dimensions. It probes
at startup with `ESC[14t` / `ESC[16t` (text-area and cell pixels) and
XTSMGRAPHICS (`ESC[?2;1S`), falling back to a `cols × rows` estimate when the
terminal answers none of them (e.g. xterm with `allowWindowOps` off).

The character grid itself (`rows × cols`) is measured live at startup — the door
parks the cursor in the far corner and reads back a cursor-position report — and
that overrides a stale `-l`/`%R` or terminal.ini (whose values a mid-session
client resize never updates). This is a one-time startup measurement; a resize
*during* play is not currently detected.

When the real pixel size is unknown the **sixel** image is capped at DOOM's
native 640×400 rather than upscaled from the estimate — an upscaled guess can
exceed what the terminal will render and produce a blank or partial image. A
front end that knows the real cell size can override the estimate (see the
`cell_width` / `cell_height` settings in the config file) to fill a larger
window.

---

## Per-user storage (`-home`)

With `-home <dir>` the door `chdir`s into that directory before the engine
starts, so all of DOOM's `cwd`-relative files — `default.cfg` (keybindings),
savegames, screenshots — are written there. Point it at a per-user, per-session
path to keep saves and key bindings across sessions without node collisions. The
directory is created if it does not exist, and used verbatim (nothing is
appended). Without `-home`, the current working directory is used as-is.

---

## Statistics & diagnostics

The door writes two diagnostic lines to **stderr** (which a BBS typically
captures into its node/system log):

**Startup banner** — the running build, detected terminal, and resolved
geometry:

```
build Jun 17 2026 01:40:44 | term=…/terminal.ini (read) | cols=182 rows=53
desc="xterm" cterm=0 | win=0x0 cell=0x0 | image 640x400 @408,0 scale=fit mode=sixel
```

**At-exit telemetry** — what was actually sent and how fast:

```
telemetry: sixel 640x400 | 1240 frames in 95.3s = 13.0 fps | 41203112 bytes
(33228/frame avg) | 422.1 KB/s avg
```

Fields: render tier and emitted image size, frame count / elapsed / frames per
second, total wire bytes, average bytes per frame, and average throughput.

---

## In-game controls

Standard DOOM controls (arrows move/turn, Ctrl fires, Space opens, etc.). A
controls reference is available in-game with **F1**. Door-specific keys:

| Key | Action |
|-----|--------|
| `F4` | Cycle the render tier / glyph mode. |
| `\` | Toggle always-run. |
| `F2` / `F3` | Save / load game (written under `-home`). |

Cheat codes work — type them in **UPPERCASE**.

---

## Configuration file

An optional `syncdoom.ini`, read from the directory of the executable, provides
defaults for the video and input options above (and host-specific networking and
WAD-set definitions used by the JavaScript lobby). All keys are optional; an
absent key keeps the auto-detected value or built-in default. See the bundled
`syncdoom.ini` for the documented key list.

---

## License & credits

DOOM is © id Software; the engine is derived from the GPLv2 DOOM source release
via Chocolate Doom and doomgeneric. This door and its terminal/network back end
are distributed under the same **GNU GPL v2**. See `CREDITS` for attribution.
Freedoom (a free, compatible IWAD) is a convenient royalty-free content set; the
commercial `doom.wad` / `doom2.wad` must be supplied by the user.
