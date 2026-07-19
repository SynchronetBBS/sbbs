# SyncBASS (Beneath a Steel Sky)

**Beneath a Steel Sky** — the 1994 cyberpunk point-and-click adventure by
Revolution Software (with comic-book artist Dave Gibbons) — as a Synchronet
external program (door): Robert Foster, dragged back to the dystopian Union
City he escaped as a child, rendered to the terminal as sixel/JPEG-XL
graphics with sound streamed to SyncTERM.

This directory (`xtrn/syncbass/`) is the installed door — the `syncscumm`
binary and the game data (`sky.*`) live here. The **source** (the ScummVM
engine collection plus the Synchronet backend that drives it) lives in
`src/doors/syncscumm/` of the Synchronet source tree; that one binary plays
every SyncSCUMM title, this directory just points it at Beneath a Steel Sky.

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
jsexec deploy.js           # copies the binary into each xtrn/sync*/
```

`deploy.js` copies (or symlinks) the built binary in as
`xtrn/syncbass/syncscumm` (`syncscumm.exe` on Windows).

## Installing into Synchronet

SyncBASS ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs**
  (the `xtrn-setup` module), find **Beneath a Steel Sky**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncbass`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncbass`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`,
and its `[exec:getdata.js]` step downloads the game data for you (prompted,
non-fatal if declined or offline). To fetch it yourself, or re-run later:

```
jsexec xtrn/syncbass/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term`
afterwards.

## Game data

**Beneath a Steel Sky is officially freeware.** Revolution Software released
it for free redistribution in 2003, and `getdata.js` downloads it straight
from ScummVM's own hosting — nothing copyrighted-and-unlicensed is bundled in
this repository, and nothing needs to be supplied by the sysop.

It fetches the **CD build** into this directory, verified against ScummVM's
published `.sha256` before unpacking. Unlike Flight of the Amazon Queen
(`xtrn/syncqueen`, two distinct builds), BASS needs only this one: the CD
build renders **both speech and on-screen subtitles** from a single data set,
so the door plays its speech to a terminal that has audio and shows its
subtitles to one that does not — the same data either way. The CD build is
self-contained; it carries `sky.cpt` (the sky engine's required data file),
so no further engine-data is needed.

## Per-user saves & config

`--savepath` and `-c` on the door's command line (`install-xtrn.ini`) give
each user their own save directory and ScummVM config file, under
`data/user/<####>/syncbass/` — no cross-user or cross-node collisions. ScummVM's
own F5 in-game menu handles save slots and naming.

## Legal

Beneath a Steel Sky's game data is freeware, released for free
redistribution by Revolution Software; `getdata.js` fetches it from ScummVM's
own distribution point rather than this repository shipping it. The ScummVM
engine is GPL-2.0+. This door's own code is GPL-2.0.
