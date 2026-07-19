# SyncQueen (Flight of the Amazon Queen)

**Flight of the Amazon Queen** — the 1995 point-and-click adventure by John
Passfield & Steve Stamatiadis (Krome Studios) — as a Synchronet external
program (door): pilot Joe King, flying movie star Faye Russell into the
Amazon jungle, rendered to the terminal as sixel/JPEG-XL graphics with sound
streamed to SyncTERM.

This directory (`xtrn/syncqueen/`) is the installed door — the `syncscumm`
binary and the game data (`talkie/`, `floppy/`) live here. The **source**
(the ScummVM engine collection plus the Synchronet backend that drives it)
lives in `src/doors/syncscumm/` of the Synchronet source tree; that one
binary plays every SyncSCUMM title, this directory just points it at Queen.

## Building the door binary

The `syncscumm` binary is not produced by the normal Synchronet build; build
it separately, then deploy it here.

- A checkout of the **Synchronet source tree** — the build compiles against
  the in-tree `xpdev` and `termgfx` libraries and the vendored ScummVM tree,
  so a binary-only install is not enough.
- **CMake**/ScummVM's own `configure` + a C++ compiler (**GCC**/**Clang**,
  or **MSVC** on Windows).

```
cd src/doors/syncscumm
./build.sh                 # Windows: build.bat
```

Copy (or symlink) the resulting binary in as `xtrn/syncqueen/syncscumm`
(`syncscumm.exe` on Windows).

## Installing into Synchronet

SyncQueen ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs**
  (the `xtrn-setup` module), find **Flight of the Amazon Queen**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncqueen`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncqueen`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`,
and its `[exec:getdata.js]` step downloads the game data for you (prompted,
non-fatal if declined or offline). To fetch it yourself, or re-run later:

```
jsexec xtrn/syncqueen/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term`
afterwards.

## Game data

**Unlike Master of Orion, Flight of the Amazon Queen is officially
freeware.** Passfield & Stamatiadis released it for free redistribution, and
`getdata.js` downloads it straight from ScummVM's own hosting — nothing
copyrighted-and-unlicensed is bundled in this repository, and nothing needs
to be supplied by the sysop.

It fetches **both** English builds, each verified against ScummVM's
published `.sha256` before unpacking:

- `talkie/` — the Talkie (speech) build.
- `floppy/` — the Floppy (text-only) build.

The door itself, not this package, picks between them per session: a player
whose terminal can actually play audio gets the Talkie build, one who can't
gets the Floppy build (so the story is never silently unheard) — see
`src/doors/syncscumm`'s `sst_select_datadir()`. Neither build needs any
further engine-data file; both are self-contained.

## Per-user saves & config

`--savepath` and `-c` on the door's command line (`install-xtrn.ini`) give
each user their own save directory and ScummVM config file, under
`data/user/<####>/queen/` — no cross-user or cross-node collisions.
ScummVM's own F5 in-game menu handles save slots and naming.

## Legal

Flight of the Amazon Queen's game data is freeware, released for free
redistribution by its copyright holders; `getdata.js` fetches it from
ScummVM's own distribution point rather than this repository shipping it.
The ScummVM engine is GPL-2.0+. This door's own code is GPL-2.0.
