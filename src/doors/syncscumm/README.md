# SyncSCUMM — ScummVM point-and-click adventures as a BBS door

SyncSCUMM plays classic 2D point-and-click adventure games over a terminal, as
a **BBS door**. It embeds [ScummVM](https://www.scummvm.org/)'s engine
collection behind a native `OSystem` backend, rendering the game to the
caller's terminal with **graphics** (sixel or JPEG-XL), **mouse**,
**keyboard**, and **sound** (Opus/Ogg-streamed) — no local ScummVM install on
the client, just a capable terminal (SyncTERM and other sixel-capable
terminals).

**Any DOOR32.SYS BBS can run it.** SyncSCUMM is developed and tested against
[Synchronet](https://wiki.synchro.net/config:xtrn), but it is a standard
[DOOR32.SYS](#how-the-caller-connects-door32sys-stdio-vs-socket) door — comm
over a **socket** on any platform, or **UNIX-stdio** on \*nix — with no
Synchronet-specific runtime dependency, so it also installs under **Mystic
BBS** or any other BBS that speaks DOOR32.SYS. This README is written to be
useful to any sysop: the door contract,
terminal requirements, and game-install steps are BBS-agnostic, and the
**Synchronet-only conveniences** (its `%`-macro command line, the
`install-xtrn.ini` auto-installer, exact SCFG menu names) are labelled as such
wherever they appear. See [Installing a game](#installing-a-game-worked-example-maniac-mansion)
for a full worked example (general steps **and** Synchronet SCFG specifics).

The binary is `syncscumm` (`syncscumm.exe` on Windows). One binary plays every
title; each game lives in its own directory that launches the same binary with
a different game target (on Synchronet, an installable `xtrn/sync<game>/`
package, e.g. `xtrn/syncbass`, `xtrn/syncqueen`). See [`DESIGN.md`](DESIGN.md)
for the architecture and [`COMPILING.md`](COMPILING.md) for building.

## Supported engines and games

SyncSCUMM builds a **curated subset** of ScummVM's engines (growing the set is a
build flag plus testing, not an architecture change — see `DESIGN.md` and
`build.sh` / `build.bat`'s `--enable-engine` list). At v1:

| Engine    | Games it plays | Bundled freeware flagship |
|-----------|----------------|---------------------------|
| `scumm`   | LucasArts SCUMM classics (v0–v6): Maniac Mansion, Zak McKracken, Indiana Jones and the Last Crusade, Loom, The Secret of Monkey Island 1 & 2, Indiana Jones and the Fate of Atlantis, Day of the Tentacle, Sam & Max, Full Throttle, … | — (commercial; sysop supplies data files) |
| `sky`     | Beneath a Steel Sky | **Beneath a Steel Sky** (freeware) — `xtrn/syncbass` |
| `queen`   | Flight of the Amazon Queen | **Flight of the Amazon Queen** (freeware) — `xtrn/syncqueen` |
| `lure`    | Lure of the Temptress | **Lure of the Temptress** (freeware) |
| `drascula`| Dráscula: The Vampire Strikes Back | **Dráscula** (freeware) |
| `agi`     | Sierra AGI adventures (King's Quest I–III, Space Quest I–II, Leisure Suit Larry, Police Quest I, … + AGI fan games) | **Space Quest 0: Replicated** (freeware fan game) — `xtrn/spacequest0` |
| `sci`     | Sierra SCI / SCI32 adventures (King's Quest IV–VII, Space Quest III–VI, Leisure Suit Larry 2–7, … + SCI fan games) | **Cascade Quest** (freeware fan game) — `xtrn/cascadequest` |

The **officially-freeware** adventures are the bundled flagships and need no
purchase — their data is fetched by each package's `getdata.js`. The
**commercial** titles (the LucasArts SCUMM catalog, the Sierra AGI/SCI catalog)
play once a sysop drops the game's data files into the package directory;
ScummVM's own detector identifies them. For the exact per-game data files,
versions, and quirks, refer to upstream ScummVM:

- Supported games / compatibility: <https://www.scummvm.org/compatibility/>
- Required data files per game: <https://wiki.scummvm.org/index.php/Datafiles>
- Where to obtain the freeware games: <https://www.scummvm.org/games/>

**Theoretically supported:** any of ScummVM's ~80 engines could be added
(restore the engine's `engines/<id>/` dir from the pinned tarball, add it to the
`--enable-engine` list, and door-test it — see `PROVENANCE.md`). Only the five
above are compiled and tested today; titles that need >320×200 or hard real-time
input are out of scope for v1.

## Terminal requirements and capabilities

**A graphics-capable terminal is required for every title.** Unlike the
framebuffer *action* doors (SyncDOOM, SyncRetro), SyncSCUMM has **no text/ANSI
fallback tier** — a point-and-click adventure is meaningless rendered as block
characters, so a terminal that cannot draw pixels cannot run any game. In
practice that means **[SyncTERM](https://syncterm.bbsdev.net/)** or another
sixel-capable terminal (mlterm, xterm `+sixel`, WezTerm, foot, …).

Capabilities are probed at connect through the shared
[`../termgfx`](../termgfx) library; each rung degrades to the next:

- **Graphics — required.** The engine's 320×200 paletted frame is emitted as
  either **JPEG-XL** (over SyncTERM's cached-image APC transport — the
  high-fidelity, low-bandwidth tier, when the terminal reports JXL support) or
  **DECSIXEL** (the near-universal graphics tier). Frames are diffed to dirty
  rectangles and integer-scaled to the viewport (2× on an 80-column SyncTERM →
  640×400). A terminal with *neither* tier cannot play.
- **Mouse — recommended, optional.** The natural input for point-and-click,
  probed as a three-rung ladder but never required (a keyboard cursor is always
  available): **DECSET 1016** (SGR *pixel* mouse — exact game coordinates; CTerm
  ≥ 1330 and the modern xterm family) → **DECSET 1006** (SGR *cell* mouse —
  near-universal; a cell click maps to that cell's centre pixel and re-homes the
  keyboard cursor, "click roughly, nudge precisely") → **no mouse reporting**
  (keyboard cursor only). The keyboard cursor is always present: arrows / WASD
  move with acceleration, **Enter** = left click, **Tab** = right click.
  ScummVM's own keys pass through (**F5** save/load menu, **Esc** skip).
- **Audio — optional.** When the terminal can play digital audio (SyncTERM's
  audio APC, with libsndfile present in the build), the mixer's music + speech +
  SFX stream to it as Opus chunks. A terminal that cannot degrades cleanly to
  **silence**, and **subtitles switch on automatically** for that session — so
  the talkie titles stay playable without sound. (Sysops can force subtitles
  either way in `syncscumm.ini`.)

## How the caller connects (DOOR32.SYS, stdio vs socket)

SyncSCUMM is a [`DOOR32.SYS`](https://wiki.synchro.net/ref:dropfile#door32.sys)
door — DOOR32.SYS is a **cross-BBS drop-file standard** written by Synchronet,
Mystic, and most other modern BBS packages. The BBS writes a `DOOR32.SYS` drop
file and passes its path to the door on the command line (on Synchronet, via
the `%f` macro — see [Installing a game](#installing-a-game-worked-example-maniac-mansion)).
The door reads the drop file to find the caller's connection. Two comm types
are handled, and platform support differs:

| DOOR32.SYS comm type | Transport | *nix | Windows |
|----------------------|-----------|:----:|:-------:|
| **2** (socket)       | The BBS hands the door an open TCP **socket** handle; the door does its own non-blocking `send()`/`recv()` on it. | ✅ | ✅ |
| **0** (stdio)        | The BBS redirects the door's **stdin/stdout** to the caller. | ✅ | ❌ |

- **Socket I/O works on both Windows and \*nix** and is what every install
  should use (DOOR32.SYS comm type **2** — on Synchronet, an `XTRN_DOOR32` door
  with the I/O method set to *Socket*; on Mystic and other BBSes, whatever
  their door config calls a socket/DOOR32.SYS door). On Windows the DOOR32.SYS
  handle is a Winsock `SOCKET`, which the CRT's `read()`/`write()` cannot
  touch, so the door uses `send()`/`recv()` — hence a Windows door **requires**
  socket comm.
- **Stdio I/O is \*nix-only.** On POSIX the door's descriptor may be a plain fd
  (the socket, or fd 1 in a dev/tty run) and `read()`/`write()` cover both; on
  Windows there is no stdio-door path. (See `door/sst_plat.h`.)

If no connection resolves (no drop file, no `-s<fd>`, no `$SYNCSCUMM_SOCK`), the
door runs **headless** — useful only for the capture/test modes below.

## Command-line arguments

The BBS launches the door with a command line like this (shown here in
Synchronet's `%`-macro form, e.g. Flight of the Amazon Queen, `xtrn/syncqueen`):

```
syncscumm%. %f --savepath=%juser/%4/queen -c %juser/%4/queen/scummvm.ini queen
```

The `%`-tokens are **Synchronet command-line macros** the BBS expands at launch
(`%.` → the platform executable extension, `%f` → the DOOR32.SYS drop-file
path, `%j` → the data dir, `%4` → the zero-padded user number). **On another
BBS**, substitute your BBS's own macros — most importantly its drop-file-path
macro in place of `%f` — or hard-coded paths; the door itself only cares about
the resolved values. The minimal generic form is
`syncscumm <DOOR32.SYS-path> <game>` (e.g. `… maniac`); everything else is
optional tuning. The door's own CLI contract:

### Required

| Argument | Purpose |
|----------|---------|
| A DOOR32.SYS drop-file path (`%f`) **or** `-s<fd>` **or** `$SYNCSCUMM_SOCK` | The caller's connection (see the table above). One of these must resolve or the session is headless. `%f` is the normal door launch; `-s<fd>`/`$SYNCSCUMM_SOCK` are for dev/testing against a raw socket fd. |
| A **game target** (e.g. `sky`, `queen`, `lure`, `drascula`, or a detected SCUMM game id), as the trailing argument | Which title to launch. Omitting it drops the caller into ScummVM's launcher GUI, which a single-title door does not want — every bundled package supplies it. |

### Optional (door / ScummVM pass-through)

| Argument | Purpose |
|----------|---------|
| `--path=<dir>` | Game-data directory. Defaults to the door's own startup dir (where the package's data lives). The door rewrites it per session to the `talkie/` or `floppy/` sub-variant when a title ships both (speech vs guaranteed on-screen text), so a package may omit `--path` and still get the right build. |
| `--savepath=<dir>` | Where ScummVM writes saves. The bundled packages point it at `data/user/<####>/<game>` for **per-user, per-node-independent** saves; the door creates the directory if missing (ScummVM will not). |
| `-c <file>` | A per-user ScummVM config (`scummvm.ini`), so two nodes never race one config; the door creates its parent dir if missing. |
| Any other ScummVM option (`--subtitles`, `--music-volume=`, `--no-fullscreen`, …) | Passed straight through to `scummvm_main()`. See <https://docs.scummvm.org/en/latest/advanced_topics/command_line.html>. |

Sysop-facing door-wide settings (subtitles policy, sixel ceiling, audio tuning)
live in an optional `syncscumm.ini` the door reads from its startup dir — see
`syncscumm.example.ini`.

### Environment variables (optional)

| Variable | Purpose |
|----------|---------|
| `SBBSDATA` | Synchronet data dir; used only to locate the door's opt-in diagnostic files (`trace`/`wirecap`, and a redirected `stderr`) under `<data-dir>/syncscumm/`. Set automatically by Synchronet; **optional** — with it unset (a dev/standalone run, or another BBS) those diagnostics fall back to the door's own startup directory. |
| `SYNCSCUMM_DATA` | An extra engine-data search directory (added to ScummVM's search set). |
| `SYNCSCUMM_SOCK` | Client socket fd, as an alternative to a DOOR32.SYS drop file. |
| `SYNCSCUMM_SIXELOUT` | Capture the door's graphics output to a file instead of a live client (headless testing). |
| `SYNCSCUMM_TRACE`, `SYNCSCUMM_AUDIODUMP` | Diagnostic dumps (frame/audio), opt-in. |

## Installing a game (worked example: Maniac Mansion)

This walks through installing one title end-to-end. **Maniac Mansion** (the
LucasArts SCUMM classic) is used because it is a good example of a
**commercial** title: unlike the freeware flagships in the engine table, its
data files are not redistributable, so *you supply them from a copy you own*
(see [Legal](#legal), below). The freeware titles differ only in step 2 — their
data is fetched for you.

The steps are the same shape on every BBS; only the door-config UI differs.
First the **general (any DOOR32.SYS BBS)** procedure, then the exact
**Synchronet SCFG** field values.

### 1. Put the binary in a game directory

Create a directory for this title and place the `syncscumm` binary
(`syncscumm.exe` on Windows) in it — build it (see [Building](#building)) or
copy in a prebuilt one. This example uses a directory named `maniac`
(on Synchronet, `xtrn/maniac/`).

### 2. Supply the game data

Copy your licensed Maniac Mansion data files — the numbered `NN.LFL` files
(e.g. `00.LFL … 53.LFL`) — **into that same directory**, flat alongside the
binary. (A `floppy/` sub-directory works too; the door checks `talkie/` then
`floppy/` and otherwise uses the directory itself.) ScummVM's own detector
identifies the game from these files — no manual "which game is this" step.

The trailing **game target** on the command line is ScummVM's game id; for
Maniac Mansion that is **`maniac`**. For other titles, look up the id in
ScummVM's [compatibility list](https://www.scummvm.org/compatibility/) and its
[required-datafiles wiki](https://wiki.scummvm.org/index.php/Datafiles).

### 3. Configure the door in your BBS

Add a new external program / door pointing at the binary, with these
properties (names vary by BBS; the *meaning* is what matters):

- **Command line:** `syncscumm <DOOR32.SYS-path> --savepath=<save-dir> -c <config-file> maniac`
  — where `<DOOR32.SYS-path>` is your BBS's macro for the drop file it writes
  for each session. `--savepath`/`-c` are optional (give each user private
  saves + ScummVM config); the minimal form is just
  `syncscumm <DOOR32.SYS-path> maniac`.
- **Drop-file type:** DOOR32.SYS.
- **I/O method / comm type:** **socket** (DOOR32.SYS comm type 2). Required on
  Windows, recommended everywhere.
- **Binary / raw mode:** no CR/LF or character-set translation — the door
  emits raw sixel/JXL graphics and reads the socket itself.
- **Multiple concurrent users:** yes (one binary per caller; the per-user
  `--savepath`/`-c` above keep their saves from colliding).
- **No local display window**, if your BBS offers the option — the door draws
  to the *caller's* terminal, not the BBS console.
- **Access requirement:** a graphics-capable terminal (see
  [Terminal requirements](#terminal-requirements-and-capabilities)).
- **Working directory:** the game directory, so the binary and data resolve.

### 3a. Synchronet specifics (SCFG)

On Synchronet, do step 3 in **SCFG**. Put the binary and the `NN.LFL` data in
`xtrn/maniac/`, then:

**SCFG → External Programs → Online Programs (Doors) → Multimedia Games**, press
**INS** to add a program, and set:

| SCFG field                  | Value |
|-----------------------------|-------|
| Name                        | `Maniac Mansion` |
| Internal Code               | `MANIAC` |
| Start-up Directory          | `../xtrn/maniac/` |
| Command Line                | `syncscumm%. %f --savepath=%juser/%4/maniac -c %juser/%4/maniac/scummvm.ini maniac` |
| Execution Requirements      | `ANSI` |
| Multiple Concurrent Users   | `Yes` |
| Native Executable           | `Yes` |
| I/O Method                  | `Socket` |
| Disable Local Display       | `Yes` |
| BBS Drop File Type          | `DOOR32.SYS` |

The `%`-macros are Synchronet's (see [Command-line arguments](#command-line-arguments)):
`%.` → executable extension, `%f` → the DOOR32.SYS path, `%j` → the data dir,
`%4` → the 4-digit user number — so `--savepath`/`-c` give every user their own
saves and ScummVM config under `data/user/<####>/maniac/`. Use a section other
than *Multimedia Games* if you prefer; the section is just where it appears in
the door menu.

Save and exit SCFG — it flags the running servers to re-read their
configuration on its way out, so the new door appears without a manual recycle.

**A note on `install-xtrn.ini`:** Synchronet's bundled *freeware* titles ship
an `install-xtrn.ini`, so you can `jsexec install-xtrn ../xtrn/<pkg>` instead
of the SCFG steps — that auto-installer is documented in each freeware
package's README (e.g. `xtrn/syncqueen/README.md`). A commercial title like
Maniac Mansion has no bundled package (you provide the data), so use the SCFG
steps above.

### Legal

Maniac Mansion is commercial LucasArts software. Install its data files only
from a copy you legally own — nothing copyrighted-and-unlicensed is shipped
with this door. The ScummVM engine embedded in the door is GPL-2.0+; this
door's own code is GPL-2.0. (The freeware flagships in the
[engine table](#supported-engines-and-games) — Beneath a Steel Sky, Flight of
the Amazon Queen, Dráscula, and the fan games — need no purchase and fetch
their own data.)

## Building

- **Linux / \*nix:** `./build.sh` (ScummVM `configure` + `make`; the door's
  termgfx/xpdev libs via CMake first). Produces `build/syncscumm`.
- **Windows:** `build.bat` (MSVC via ScummVM's `create_project` + MSBuild +
  vcpkg). Produces `build-msvc\Release\syncscumm.exe`.

Full details, prerequisites, and the graphics/audio tiers are in
[`COMPILING.md`](COMPILING.md). After building, deploy the binary into each
game directory: **on Synchronet**, `jsexec deploy.js` does this for you —
copying the freshly-built binary into every installed SyncSCUMM package it
auto-discovers (`jsexec` and `deploy.js` are Synchronet-only). On another BBS,
copy the binary into each game directory yourself.

## See also

- [`DESIGN.md`](DESIGN.md) — architecture: the `OSystem` backend, video/input/
  audio paths, saves, presence, milestones.
- [`PROVENANCE.md`](PROVENANCE.md) — what is vendored from ScummVM, pruned, and
  patched.
- [`COMPILING.md`](COMPILING.md) — building on \*nix and Windows.
- ScummVM documentation: <https://docs.scummvm.org/>
