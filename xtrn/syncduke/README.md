# SyncDuke

**Duke Nukem 3D** as a Synchronet external program (door): single player, plus
2-player LAN co-op and dukematch over the BBS, rendered to the terminal as
JPEG-XL or sixel graphics — or block characters on a terminal with no graphics
at all. The door itself is C (a vendored Chocolate Duke3D port); the lobby that
browses and creates network games is JavaScript (`lobby.js`).

This directory (`xtrn/syncduke/`) is the installed door — the `syncduke`
binary, the lobby, `syncduke.ini`, the display files, and your GRP file all
live here. The **source** lives in `src/doors/syncduke/` of the Synchronet
source tree.

## Terminal support

The door picks the best tier the caller's terminal can take, and the player can
cycle tiers in-game with **F4**:

- **JPEG-XL** — SyncTERM's best; Duke's frame is scaled up to fill the graphics
  canvas.
- **Sixel** — any sixel-capable terminal.
- **Block characters** — half, quadrant, or sextant glyphs, on any ANSI
  terminal with no pixel graphics at all. This is a real fallback, not a
  placeholder: SyncDuke stays playable here, which is why it has no
  "graphics required" gate.

Quadrant and sextant tiers need UTF-8; on CP437 they'd render as missing-glyph
boxes, so they're only offered when the client's charset supports them
(`[video] charset`). Mouse control is optional.

## Getting the door binary

The `syncduke` binary is **not shipped** with Synchronet (size, plus the GPL
source-distribution obligation), so it has to be fetched or built. The
installer runs `get-binary.js` before it registers anything, and **aborts the
install if the binary is still missing** — SyncDuke never registers without a
runnable executable.

- **Windows** — `get-binary.js` downloads the current Win32 build from
  synchro.net and extracts it here. Nothing else to do.
- **Linux / Unix-like** — build it yourself (below); `get-binary.js` only
  verifies it arrived.

### Building

Prerequisites: CMake 3.13+, C **and** C++ compilers (GCC or Clang), and
optionally `libjxl-dev` (the JPEG-XL tier) and `libsndfile1-dev` (OGG music
compression), both found via `pkg-config`. Without them the door still builds
and serves the lower tiers. On Debian/Ubuntu:

```sh
sudo apt install cmake g++ pkg-config libjxl-dev libsndfile1-dev
```

From `<sbbs>/src/doors/syncduke`:

```sh
cmake -B build
cmake --build build -j
```

That produces `build/syncduke`. Building does **not** deploy — run
`jsexec deploy.js` afterwards to install the binary into this directory, so you
can rebuild and test before putting a new binary in front of players. On
Windows, `build.bat` builds (Visual Studio 2022) and `jsexec deploy.js`
installs, same as on \*nix.

## Installing into Synchronet

SyncDuke ships an `install-xtrn.ini`, so the bundled installer registers it for
you — no manual SCFG entry needed. Launch the installer any of these ways; the
menu-driven ones discover SyncDuke automatically (you just pick it from the
list), while the command line takes the path:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), included by default in the **Operator** external-programs
  section. Find **SyncDuke** in the list and install it.
- **SBBSCTRL (Windows)** — **File → Run → Install External Programs**.
- **Command line** — `jsexec install-xtrn ../xtrn/syncduke`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncduke`.

It offers two programs, each prompted, so you can take either or both:

- **SyncDuke (Nukem 3D)** — the lobby: create or join 2-player co-op and
  dukematch games, play single player, or read the controls. **Recommended.**
- **SyncDuke (Nukem 3D) single-player** — a direct single-player launch via
  DOOR32.SYS, with no lobby.

It also seeds `syncduke.ini` from `syncduke.example.ini` (never overwriting an
existing one without asking), and offers — prompted, and safe to decline — to
download the shareware GRP so the door is playable immediately.

## Game data — the DUKE3D.GRP file

SyncDuke ships **no game data**. It needs a Duke Nukem 3D GRP file:

- The **shareware Episode 1** GRP (`DUKE3D.GRP`, 11,035,779 bytes, "SHAREWARE
  1.3D") is freely redistributable, and the installer offers to download it
  (~6 MB) from 3D Realms' original shareware package. You can also run it later:
  `jsexec ../xtrn/syncduke/download.js`.
- The **Full 1.3D / Plutonium 1.4 / Atomic 1.5** GRPs work too, and you supply
  those yourself.

Where it goes is `[grp] dir` in `syncduke.ini`. Leave it blank to just drop the
GRP beside the binary, or set an absolute path to share one copy. **That
directory must be writable** — the engine writes its own config there.

Two things the shareware GRP doesn't have: the built-in attract demos (see
`[game] attract_demos`), and enough content for most third-party user maps,
which generally want the Full/Atomic data.

## Configuration

**`syncduke.ini`** is read from this directory and is fully commented in-file;
every key is optional.

- **`[grp] dir`** — where the GRP lives (above).
- **`[lobby]`** — the lobby menu only; the door binary reads none of it.
  `live` (on here) shows a who's-online-and-recent-activity panel anchored to
  the bottom of the menu, refreshed about once a second — it repaints in place,
  so it suits ANSI terminals; set it false for a plain static menu.
  `enter_sound` plays a one-shot sound on lobby entry for SyncTERM callers,
  from a file or a wildcard you supply — **nothing ships**, so point it at
  sounds you extracted from your own GRP rather than redistributing any.
- **`[net]`** — co-op networking, read by the **lobby**, not the direct
  single-player entry. See [Multiplayer](#multiplayer).
- **`[dukematch]`** — deathmatch rules the lobby applies when a player picks
  Dukematch: `submode` (weapons respawn or grab-once), `monsters`,
  `respawn_items`. Co-op ignores them.
- **`[game]`** — `record` (the in-game demo-recording menu option, off: a demo
  a BBS user can't download is just wasted disk) and `attract_demos` (off, so
  single player drops straight to the menu instead of streaming a demo nobody
  asked for). This section also holds **`[map:<Name>]`** entries for
  third-party user maps and add-on GRPs — one per choice, named as players see
  it in the picker. Configure any, and Solo and Create start asking what to
  play; configure none and there's no prompt at all.
- **`[video]`** — `scale_max` caps the JPEG-XL image width (default 1280) so a
  maximized window can't produce an enormous per-frame payload;
  `sixel_max_width` does the same for sixel (default 1024, clamped 320–1024);
  `charset` picks the block-tier character set (`auto`, `utf8`, `cp437`).
- **`[audio] music_quality`** — Opus VBR quality for Duke's FM music, which is
  rendered, encoded, and shipped to the terminal once, then cached at both ends.
  Default 0.15 (roughly 88 kbps). Only affects newly encoded tracks.
- **`[debug]`** — `log` (on by default, see [Logging](#logging)) and the
  Windows-only `hide_console` safety net for BBSes with no `XTRN_NODISPLAY`
  equivalent.
- **`[idle]`** — end the game and hand an idle player back to the BBS. Default
  **10 minutes**; leaving it unset is *not* the same as off (set `0` for that).
  Both keystrokes and mouse movement count as being present. SCFG's **Maximum
  Inactivity** can't do this job for a graphical door — the door's frame pacing
  makes the terminal answer several times a second, so that timer never fires.

Per-user config and saved games are written under `data/user/<num>/duke/`,
which Synchronet auto-cleans with the user account.

## Multiplayer

Co-op and dukematch are **2-player, peer to peer**, arranged by the lobby. The
player who Creates a game hosts it; the other dials in.

- **Same-host play** — two nodes on this BBS — works with **no `[net]` setup at
  all**. The host listens on all interfaces and joiners use loopback.
- **Cross-host play** — a second BBS host on the same LAN, sharing the games
  registry — needs `[net] advertise` set to this host's LAN IP or DNS name.
  On a shared config seen by several hosts, give each its own
  `[net:<hostname>]` section rather than trying to make one value fit.
- `port_low`/`port_high` bound the UDP ports the host listens on — one per
  concurrent match.

**The game protocol has no authentication.** Once the door is reachable on a
routable address, anything that can reach the UDP port and speaks the right
Duke version can join. Keep it to the LAN and restrict it with a host firewall.

## Logging

The door normally runs with no local console window (the installer sets
Synchronet's **Disable Local Display**, and the lobby launches games the same
way), so diagnostics go to a file: **`data/syncduke/syncduke_n<node>.log`**,
tagged with the node number so concurrent co-op nodes can't share one file. It
records hangups, clean exits, and crashes — the door installs its own crash
handler. Logging is on by default (`[debug] log`); set that key blank to turn
it off.

## License & credits

Duke Nukem 3D is © 3D Realms; the game data is theirs and is not distributed
here. The vendored Chocolate Duke3D engine and this door's own code are GPLv2.
Full attribution ships with the source in `src/doors/syncduke/`.
