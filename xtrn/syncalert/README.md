# SyncAlert

**Command & Conquer: Red Alert** as a Synchronet external program (door):
Westwood's original, played against the computer over the BBS, rendered to the
terminal as JPEG-XL or sixel graphics with sound, music, and the game's
full-motion video cutscenes. The door is C/C++, built on a vendored
[Vanilla Conquer](https://github.com/Vanilla-Conquer/Vanilla-Conquer) engine
whose video, keyboard, and audio backends are replaced by terminal-facing
shims.

This directory (`xtrn/syncalert/`) is the installed door — the `syncalert`
binary, `syncalert.ini`, and the game data under `assets/`. The **source**
lives in `src/doors/syncconquer/` of the Synchronet source tree, where one tree
builds both C&C doors: SyncAlert (Red Alert) and its sibling SyncDawn
(Tiberian Dawn).

## Terminal requirements

SyncAlert needs a **sixel- or JPEG-XL-capable terminal**; SyncTERM is the
reference client. Block-character tiers exist, but a real-time strategy game on
block glyphs is something to glance at rather than play, so rather than dropping
such a caller into one and leaving them to work out why it's unusable, the door
tells them and returns them to the BBS. You can reword or re-skin that notice
(`[text]` in `syncalert.ini`, below).

Mouse support matters here more than in most doors — this is a
point-and-click RTS. It works on any terminal that reports mouse events.

## Building the door binary

SyncAlert ships no binary and has none to download: build it yourself, then
deploy it into this directory. The installer does **not** check for it, so
confirm it arrived before you run the installer.

Prerequisites: CMake 3.25+, a C/C++ toolchain, and optionally `libjxl`
(the JPEG-XL tier) and `libsndfile` (OGG/Opus music compression), both found
via `pkg-config`. Missing either is a warning, not an error — without libjxl
you get sixel; without libsndfile music ships as raw PCM. On Debian/Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libjxl-dev libsndfile1-dev
```

From `<sbbs>/src/doors/syncconquer`:

```sh
./build.sh              # release build -> build/syncalert + build/syncdawn
./build.sh ra           # Red Alert only -- skip Tiberian Dawn
./build.sh debug        # debug build
./build.sh clean        # remove the build tree
```

Building does **not** install anything. Run `jsexec deploy.js` afterwards to
copy the binary into this directory, so you can rebuild and test before putting
a new one in front of players.

On Windows, `build.bat` builds with Visual Studio 2022 and `jsexec deploy.js`
installs, same as on \*nix. **Win32 (x86) is the only Windows target** and
`build.bat` refuses x64 up front — the headless path this door takes carries
32-bit assumptions upstream never compiles. A Win32 door runs fine on a Win64
Synchronet host.

## Installing into Synchronet

SyncAlert ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed. Launch the installer any of these ways;
the menu-driven ones discover SyncAlert automatically (you just pick it from
the list), while the command line takes the path:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), included by default in the **Operator** external-programs
  section. Find **SyncAlert** in the list and install it.
- **SBBSCTRL (Windows)** — **File → Run → Install External Programs**.
- **Command line** — `jsexec install-xtrn ../xtrn/syncalert`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncalert`.

The installer registers one program, **SyncAlert (Red Alert)**, seeds
`syncalert.ini` from `syncalert.example.ini`, and offers — prompted, and safe
to decline — to download the game data.

## Game data — freeware, downloaded for you

SyncAlert ships **no game data**. Electronic Arts released Red Alert as
freeware on 31 August 2008, so the installer can fetch it: `fetch-assets.js`
downloads the official RA95 freeware CD image from archive.org (~556 MB),
verifies its checksum, and extracts just the two archives the engine needs —
`REDALERT.MIX` and `MAIN.MIX` — into `assets/`. The cutscenes ride inside
`MAIN.MIX`, so they come along with it.

It's idempotent (data already in place is left alone) and non-fatal, so an
install with no outbound internet still completes. Run it by hand any time:

```
jsexec ../xtrn/syncalert/fetch-assets.js
```

Or drop the two MIX files into `assets/` yourself. The download streams
straight to disk and the checksum is computed as it goes, so the CD image is
never held in memory — but do make sure there's room for it before you start.

## Playing

A lone caller's game is **skirmish against the computer**, which the engine's
own multiplayer menu offers. Campaign missions are present in the vendored
engine but aren't part of what this door promises or tests yet, so treat them
as untried rather than supported.

**Network play is not available.** The option is deliberately absent from the
menu: the engine announces games by broadcast and binds one fixed global port,
so two nodes on one BBS host collide and a node on another host is never seen.
Choosing it could only fail, so the button is gone until peer discovery is
built. Skirmish is unaffected.

## Configuration

**`syncalert.ini`** is read from this directory, beside the binary, and is
fully commented in-file; every key is optional, and an absent file just uses
the built-in defaults.

- **`[game] movies`** — the full-motion video cutscenes (intro, briefings,
  win/lose). The default is **auto**: play them only when the caller's client
  can actually play audio, because a silent FMV is a poor experience and it's
  the heaviest thing the door sends. Set `true` or `false` to force it.
- **`[game] captions`** — on-screen captions for the spoken EVA callouts
  ("construction complete", "unit ready", "our base is under attack"). The
  default is **auto**: show them only when the caller's client has no usable
  audio. Set `true` to show them always — worth considering as an
  accessibility default rather than leaving it to the audio probe.
- **`[video] dirty_rect`** — on by default: each frame re-sends only the parts
  of the screen that actually changed, rather than re-encoding the whole frame.
  It cuts bandwidth substantially and never sends more than a full frame. Set
  it false only if a particular terminal mis-composites the partial updates
  (you'd see seams or leftover pixels).
- **`[text]`** — the notice shown to a caller with no pixel graphics:
  `no_graphics` for one-line wording, `no_graphics_file` for a multi-line or
  ANSI-art file (drop in `nographics.txt` and nothing else needs setting), and
  `no_graphics_pause` for how long it's held before the BBS repaints over it.
- **`[idle]`** — end the game and hand an idle player back to the BBS. Default
  **10 minutes**; leaving it unset is *not* the same as off (set `0` for that).
  Both keystrokes and mouse movement count as being present, so a player
  commanding units with the mouse never times out. SCFG's **Maximum
  Inactivity** can't do this job for a graphical door — the door's frame pacing
  makes the terminal answer several times a second, so that timer never fires.

Per-user config and saved games are written under `data/user/<num>/redalert/`,
which Synchronet auto-cleans with the user account.

## Logging

The door draws to the caller's terminal and prints nothing locally — the
installer sets Synchronet's **Disable Local Display**, so on Windows no console
window ever appears. Diagnostics go to
**`data/syncalert/syncalert_n<node>.log`**, tagged with the node number so
concurrent nodes can't share one file.

## License & credits

Command & Conquer: Red Alert is © Westwood Studios / Electronic Arts; the game
data is theirs, released as freeware, and is not redistributed here — the
installer fetches it from archive.org. The vendored Vanilla Conquer engine is
**GPLv3 with EA's additional terms** (`vanilla/License.txt`). Full attribution
and the exact vendoring record ship with the source in
`src/doors/syncconquer/`.
