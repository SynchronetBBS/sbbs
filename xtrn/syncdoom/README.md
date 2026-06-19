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
- **Windows / MSVC** — planned. The doomgeneric platform layer and Winsock
  wiring are not yet in place; build on a Unix-like host for now.

## Installing into Synchronet (SCFG)

Register the door with the bundled installer (`install-xtrn.ini`):

```
jsexec install-xtrn ../xtrn/syncdoom
```

or, from the terminal as a sysop:

```
;exec ?install-xtrn ../xtrn/syncdoom
```

It offers two programs, each prompted, so you can take either or both:

- **SyncDOOM** — the lobby (browse/create co-op & deathmatch games, single
  player, and the controls help). Recommended.
- **SyncDOOM (single-player)** — a direct single-player launch via DOOR32.SYS,
  with no lobby.

## Configuration & data

- **`syncdoom.ini`** — all settings, commented in-file: `[video]`/`[input]` are
  read by the C door, `[net]`/`[wads]`/`[wadset:*]` by the lobby.
- **Multiplayer reach** — out of the box a match server binds loopback
  (`127.0.0.1`), so play is **same-host only**. For cross-host (multi-machine
  LAN) play set `[net] advertise` to this host's LAN IP/DNS name (`bind` follows
  it); see the `[net]` comments and `src/doors/syncdoom/MULTIPLAYER.md`.
- **`wads/`** — put your IWADs and PWADs here (the directory is set by
  `[wads] dir`). Freedoom (`freedoom1.wad`, `freedoom2.wad`) is free to
  redistribute; the commercial `doom.wad`/`doom2.wad` you must supply yourself.
- Per-user save games and config are written under `data/user/<num>/doom/`,
  which Synchronet auto-cleans with the user account.
