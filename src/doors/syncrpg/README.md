# SyncRPG — RPG Maker 2000/2003 games as a BBS door

SyncRPG plays [RPG Maker 2000/2003](https://easyrpg.org/) games over a
terminal, as a **BBS door**. It embeds the [EasyRPG Player](https://easyrpg.org/)
engine behind a native backend, rendering the game to the caller's terminal
with **graphics** (sixel or JPEG-XL), **keyboard** input, and **sound**
(Opus/Ogg-streamed) — no local EasyRPG/RPG Maker install on the client, just a
capable terminal (SyncTERM and other sixel-capable terminals).

**Any DOOR32.SYS BBS can run it.** SyncRPG is developed and tested against
[Synchronet](https://wiki.synchro.net/config:xtrn), but it is a standard
[DOOR32.SYS](#how-the-caller-connects-door32sys-stdio-vs-socket) door — comm
over a **socket** on any platform, or **UNIX-stdio** on \*nix — with no
Synchronet-specific runtime dependency, so it also installs under **Mystic
BBS** or any other BBS that speaks DOOR32.SYS. This README is written to be
useful to any sysop: the door contract, terminal requirements, and game-install
steps are BBS-agnostic, and the **Synchronet-only conveniences** (its
`%`-macro command line, the `install-xtrn.ini` auto-installer, exact SCFG menu
names) are labelled as such wherever they appear.

The binary is `syncrpg` (`syncrpg.exe` on Windows). SyncRPG v1 ships a single
flagship title, **Yume Nikki** (see below); the door is structured so more
RPG Maker 2000/2003 freeware games can be added later purely by writing their
own `getdata.js` + `install-xtrn.ini` — no door code change (see
`docs/superpowers/specs/2026-07-19-syncrpg-door-design.md` for the
architecture; `PROVENANCE.md` for what is vendored from EasyRPG and pruned).

## Yume Nikki

[Yume Nikki](https://en.wikipedia.org/wiki/Yume_Nikki) is a freeware surreal
exploration RPG by Kikiyama, built with RPG Maker 2003. It is a single
self-contained project (no external RTP required). The bundled package
(`xtrn/yumenikki/`, on Synchronet) fetches the **official English localization**
(the Playism/Steam 0.10a release, hosted on
[rmarchiv.de](https://rmarchiv.de/games/1089) — the EasyRPG project's own
recommended archive) via `getdata.js`.

## Terminal requirements and capabilities

**A graphics-capable terminal is required.** SyncRPG has no text/ANSI
fallback tier — RPG Maker's tile-based scenes are meaningless rendered as
block characters, so a terminal that cannot draw pixels cannot run the game.
In practice that means **[SyncTERM](https://syncterm.bbsdev.net/)** or another
sixel-capable terminal (mlterm, xterm `+sixel`, WezTerm, foot, …).

Capabilities are probed at connect through the shared
[`../termgfx`](../termgfx) library (the same module SyncSCUMM and every other
graphical door in this tree use), each rung degrading to the next:

- **Graphics — required.** EasyRPG's 32-bit truecolor screen surface is
  emitted as either **JPEG-XL** (over SyncTERM's cached-image APC transport —
  the high-fidelity, low-bandwidth tier, when the terminal reports JXL
  support) or **DECSIXEL** (the near-universal graphics tier, via a
  median-cut quantizer). Frames are integer-scaled to the viewport. A
  terminal with *neither* tier cannot play.
- **Mouse — not used.** RPG Maker 2000/2003 games are keyboard-driven;
  termgfx's mouse decoding stays available to the shared library but this
  door does not wire it up.
- **Audio — optional.** When the terminal can play digital audio (SyncTERM's
  audio APC, with libsndfile present in the build), EasyRPG's own mixer
  (`GenericAudio`, built-in **fmmidi** MIDI synth, WAV/OGG/MP3/MOD decoders)
  streams to it as Opus chunks. A terminal that cannot degrades cleanly to
  **silence**.
- **Keyboard** — the standard RPG Maker 2000/2003 key set (arrows to move,
  Enter/Z/Space to confirm, Esc for the menu, the numpad effect/action keys
  Yume Nikki uses) plus the termgfx-reserved hotkeys (Ctrl-S stats, Ctrl-Q
  quit).

## How the caller connects (DOOR32.SYS, stdio vs socket)

SyncRPG is a [`DOOR32.SYS`](https://wiki.synchro.net/ref:dropfile#door32.sys)
door — DOOR32.SYS is a **cross-BBS drop-file standard** written by Synchronet,
Mystic, and most other modern BBS packages. The BBS writes a `DOOR32.SYS` drop
file and passes its path to the door on the command line (on Synchronet, via
the `%f` macro — see [Installing the game](#installing-the-game) below). The
door reads the drop file to find the caller's connection. Two comm types are
handled, and platform support differs:

| DOOR32.SYS comm type | Transport | *nix | Windows |
|----------------------|-----------|:----:|:-------:|
| **2** (socket)       | The BBS hands the door an open TCP **socket** handle; the door does its own non-blocking `send()`/`recv()` on it. | ✅ | ✅ |
| **0** (stdio)        | The BBS redirects the door's **stdin/stdout** to the caller. | ✅ | ❌ |

- **Socket I/O works on both Windows and \*nix** and is what every install
  should use (DOOR32.SYS comm type **2** — on Synchronet, an `XTRN_DOOR32`
  door with the I/O method set to *Socket*). On Windows the DOOR32.SYS handle
  is a Winsock `SOCKET`, which the CRT's `read()`/`write()` cannot touch, so
  the door uses `send()`/`recv()` — hence a Windows door **requires** socket
  comm.
- **Stdio I/O is \*nix-only.** On POSIX the door's descriptor may be a plain
  fd and `read()`/`write()` cover both; on Windows there is no stdio-door
  path.

If no connection resolves (no drop file, no `-s<fd>`), the door runs
**headless** — useful only for the capture/test modes under `test/`.

## Command-line arguments

The BBS launches the door with a command line like this (shown here in
Synchronet's `%`-macro form):

```
syncrpg%. %f --save-path=%juser/%4/yumenikki yumenikki
```

The `%`-tokens are **Synchronet command-line macros** the BBS expands at
launch (`%.` → the platform executable extension, `%f` → the DOOR32.SYS
drop-file path, `%j` → the data dir, `%4` → the zero-padded user number).
**On another BBS**, substitute your BBS's own macros — most importantly its
drop-file-path macro in place of `%f` — or hard-coded paths; the door itself
only cares about the resolved values. The door's own CLI contract:

### Required

| Argument | Purpose |
|----------|---------|
| A DOOR32.SYS drop-file path (`%f`) **or** `-s<fd>` | The caller's connection (see the table above). One of these must resolve or the session is headless. |
| A **game/project-path** (e.g. `yumenikki`), as the trailing argument, **or** the `SYNCRPG_GAME` environment variable | The RPG Maker project directory to run — passed to EasyRPG as `--project-path`. Resolved relative to the door's startup/working directory. |

### Optional

| Argument | Purpose |
|----------|---------|
| `--save-path=<dir>` | Where EasyRPG writes save files (its own `--save-path` option — see `--help` in `easyrpg/src/player.cpp`). The bundled package points it at `data/user/<####>/yumenikki` for **per-user, per-node-independent** saves; the door creates the directory if missing (EasyRPG will not — its `FileFinder` only resolves an *existing* directory). |

Unlike the SyncSCUMM family, SyncRPG does not currently forward arbitrary
EasyRPG options through to the engine — only `--save-path` is translated; the
rest of the command line is the game/project path and the connection.

### Environment variables (optional)

| Variable | Purpose |
|----------|---------|
| `SYNCRPG_GAME` | The RPG Maker project directory, as an alternative to the trailing positional argument. |
| `SYNCRPG_FMT_PROBE` | Diagnostic: dumps the candidate 32-bit pixel-format byte layouts to stderr at startup (used to confirm the zero-copy `present_rgbx` channel order — see `door/ui_term.cpp`). |
| `SYNCRPG_FRAMELOG` | Diagnostic: logs every 60th `UpdateDisplay()` call to stderr (used to check the engine keeps redrawing a still screen). |

## Installing the game

### 1. Put the binary in the game directory

The bundled Yume Nikki package (Synchronet: `xtrn/yumenikki/`) holds the
`syncrpg` binary (`syncrpg.exe` on Windows) — build it (see
[Building](#building)) and deploy it there, flat, alongside the game data.

### 2. Supply the game data

`xtrn/yumenikki/getdata.js` fetches Yume Nikki's official English localization
(freeware) from rmarchiv.de and sha256-verifies it, extracting into a
`yumenikki/` sub-directory (the archive's own top-level folder — an RM2003
project: `RPG_RT.ldb`/`.lmt`/`.ini` + assets, no RTP dependency).

### 3. Configure the door in your BBS

Add a new external program / door pointing at the binary, with these
properties (names vary by BBS; the *meaning* is what matters):

- **Command line:** `syncrpg <DOOR32.SYS-path> --save-path=<save-dir> yumenikki`
  — where `<DOOR32.SYS-path>` is your BBS's macro for the drop file it writes
  for each session. `--save-path` is optional (gives each user private
  saves); the minimal form is just `syncrpg <DOOR32.SYS-path> yumenikki`.
- **Drop-file type:** DOOR32.SYS.
- **I/O method / comm type:** **socket** (DOOR32.SYS comm type 2). Required
  on Windows, recommended everywhere.
- **Binary / raw mode:** no CR/LF or character-set translation — the door
  emits raw sixel/JXL graphics and reads the socket itself.
- **Multiple concurrent users:** yes (one binary per caller; the per-user
  `--save-path` above keeps saves from colliding).
- **No local display window**, if your BBS offers the option — the door
  draws to the *caller's* terminal, not the BBS console.
- **Access requirement:** a graphics-capable terminal (see
  [Terminal requirements](#terminal-requirements-and-capabilities)).
- **Working directory:** the game package directory (`xtrn/yumenikki/`), so the
  binary and `yumenikki/` data resolve.

### Synchronet specifics (SCFG / install-xtrn)

On Synchronet, the bundled package ships an `install-xtrn.ini`, so instead of
manual SCFG steps you can run:

```
jsexec install-xtrn ../xtrn/yumenikki
```

(or `;exec ?install-xtrn ../xtrn/yumenikki` from the terminal). It registers
Yume Nikki as its own external-program entry (Command Line
`syncrpg%. %f --save-path=%juser/%4/yumenikki yumenikki`, DOOR32.SYS/Socket
I/O, Multiple Concurrent Users, Native Executable, Disable Local Display) and
offers to fetch the game data via `getdata.js`. See
`xtrn/yumenikki/install-xtrn.ini` for the full annotated installer data, and
`SCFG → External Programs → Online Programs (Doors)` if you prefer to review
or adjust the installed entry by hand afterward.

Save and exit SCFG (if you touch it) — it flags the running servers to
re-read their configuration on its way out, so the door appears without a
manual recycle. `install-xtrn` itself requests the same recycle.

### Legal

Yume Nikki is freeware; `getdata.js` fetches the official English localization
from rmarchiv.de (the EasyRPG project's own recommended archive) and verifies
it by sha256 — nothing is re-hosted in this repository. The EasyRPG Player
engine embedded in the door is GPL-3.0+; this door's own code is GPL-2.0.

## Building

- **Linux / \*nix:** `./build.sh` (builds the shared `termgfx`/`xpdev`
  libraries first via CMake, then configures + builds the vendored EasyRPG
  Player + this door's own sources). Produces `build/syncrpg`.
- **Windows:** `build.bat` (`build.bat clean` to start over) — the same two
  stages, driven by CMake + Ninja under MSVC with the dependencies coming
  from vcpkg. Win32/x86 Release; produces `build-msvc\Release\syncrpg.exe`.

See [COMPILING.md](COMPILING.md) for prerequisites, the vcpkg dependency
list, and what each stage does.

After building, deploy the binary into the game directory: **on Synchronet**,
`jsexec deploy.js` copies the freshly-built binary into the live
`xtrn/yumenikki/` package (falling back to the in-tree bundle when no live
install exists). On another BBS, copy the binary into the game directory
yourself.

## See also

- `docs/superpowers/specs/2026-07-19-syncrpg-door-design.md` — architecture
  design (the `BaseUi_termgfx` backend, the truecolor `present_rgbx` path
  shared with future doors, data/control flow).
- `PROVENANCE.md` — what is vendored from EasyRPG Player/liblcf, pruned, and
  patched.
- `test/boot_yumenikki.sh` — headless boot acceptance test (video + audio).
- EasyRPG Player documentation: <https://easyrpg.org/>
