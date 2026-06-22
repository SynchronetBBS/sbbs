# SyncDOOM

Multiplayer DOOM as a Synchronet external program (door): co-op and deathmatch
over the BBS, rendered to the terminal via SyncTERM JPEG-XL/PPM graphics, sixel,
or a text/block-character fallback. The door itself is C (a doomgeneric /
Chocolate Doom port); the lobby that browses and creates network games is
JavaScript (`lobby.js`).

This directory (`xtrn/syncdoom/`) is the installed door — the `syncdoom` binary,
the lobby, `syncdoom.ini`, the display files, and your WADs all live here. The
**source** lives in `src/doors/syncdoom/` of the Synchronet source tree.

## Building the door binary

The `syncdoom` binary is not produced by the normal Synchronet build; build it
separately as below, then place it in this directory.

### Prerequisites

- A checkout of the **Synchronet source tree** — the build compiles against the
  in-tree `xpdev` library, so a binary-only install is not enough.
- **CMake** 3.13 or newer and a C compiler (**GCC** or **Clang**).
- *Optional:* the **libjxl** development package, for the JPEG-XL graphics tier
  used by SyncTERM. Without it the door still serves the sixel and text/block
  tiers.
  - Debian/Ubuntu: `sudo apt install cmake pkg-config libjxl-dev`
  - Fedora: `sudo dnf install cmake pkgconf-pkg-config libjxl-devel`

The build compiles its own minimal copy of `xpdev` (cryptography and audio
backends disabled), so you do **not** need to build the rest of Synchronet
first — only the source tree needs to be present.

### CMake (recommended; out-of-source)

From the door's source directory (`<sbbs>/src/doors/syncdoom`):

```
cmake -B build
cmake --build build -j
```

This produces `build/syncdoom`. Copy it into the installed door directory:

```
cp build/syncdoom <sbbs>/xtrn/syncdoom/
```

To build without the JPEG-XL tier (sixel/text only):

```
cmake -B build -DWITHOUT_JPEG_XL=ON
cmake --build build -j
```

Tip: to have CMake write the binary straight into the installed door directory
and skip the copy, add `-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=<sbbs>/xtrn/syncdoom`
at configure time.

### Platform support

- **Linux / Unix-like** with GCC or Clang — supported.
- **Windows / MSVC** — supported (Visual Studio 2022). Build with the
  `build.bat` helper, or CMake directly; the JPEG-XL graphics tier uses
  libjxl via vcpkg. See `src/doors/syncdoom/COMPILING.md` for the steps.

## Installing into Synchronet

SyncDOOM ships an `install-xtrn.ini`, so the bundled installer registers it for
you — no manual SCFG entry needed. Launch the installer any of these ways; the
menu-driven ones discover SyncDOOM automatically (you just pick it from the
list), while the command line takes the path:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), included by default in the **Operator** external-programs
  section. Find **SyncDOOM** in the list and install it.
- **SBBSCTRL (Windows)** — **File → Run → Install External Programs**.
- **Command line** — `jsexec install-xtrn ../xtrn/syncdoom`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncdoom`.

However it's launched, the installer offers two programs, each prompted, so you
can take either or both:

- **SyncDOOM** — the lobby (browse/create co-op & deathmatch games, single
  player, and the controls help). Recommended.
- **SyncDOOM (single-player)** — a direct single-player launch via DOOR32.SYS,
  with no lobby.

It also offers (prompted) to **download the free Freedoom WAD set** (Phase 1 + 2
and FreeDM, ~35 MB) into the `wads/` dir so the door is playable immediately —
see `getwads.js`. It's skipped if those WADs are already present, and is
optional (decline it if you'll supply your own WADs or have no outbound
internet). You can also run it later by hand: `jsexec ../xtrn/syncdoom/getwads.js`.

## Configuration & data

- **`syncdoom.ini`** — all settings, fully commented in-file. `[video]`,
  `[input]`, and `[game]` are read by the C door; `[lobby]`, `[net]`, `[wads]`,
  and `[wadset:*]` by the lobby. A few notables added recently:
  - **Input feel** — turning defaults to **FAST TURN** on (`[input] instant_turn`),
    which suits terminal key-repeat; players toggle it (and the TAP/HOLD/TURN
    grace sliders) live in-game under **Options**. Per-user tweaks save to
    `data/user/<num>/doom/syncdoom.ini` and override the house defaults — but only
    settings a player actually changes are stored, so your house defaults keep
    reaching everyone else.
  - **Waiting-room splash** — `[game] splash`: an editable `waiting.bin` (80×25
    "binary text", e.g. PabloDraw — set 80×25, Save As Binary Text) ships beside
    the door; edit it to re-skin the multiplayer waiting room, no rebuild.
  - **Player-quit effect** — `[game] quit_effect` = `keep` / `vanish` / `fog`
    (default: a teleport-out puff) decides the fate of a marine whose player
    leaves a match.
  - **Lobby attract** — `[lobby] art_dir`: drop full-screen DOOM ANSI (`*.ans`)
    there to show a random one on lobby entry.
- **Multiplayer reach** — out of the box a match server binds loopback
  (`127.0.0.1`), so play is **same-host only**. For cross-host (multi-machine
  LAN) play set `[net] advertise` to this host's LAN IP/DNS name (`bind` follows
  it); see the `[net]` comments and `src/doors/syncdoom/MULTIPLAYER.md`.
- **`wads/`** — put your IWADs and PWADs here (the directory is set by
  `[wads] dir`). Freedoom (`freedoom1.wad`, `freedoom2.wad`) is free to
  redistribute; the commercial `doom.wad`/`doom2.wad` you must supply yourself.
- Per-user save games and config are written under `data/user/<num>/doom/`,
  which Synchronet auto-cleans with the user account.
