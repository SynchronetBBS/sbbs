# Cascade Quest

**Cascade Quest** — an original freeware adventure game by **Phil Fortier**
("Troflip", author of the SCI Companion development tool), built on Sierra's
SCI interpreter — as a Synchronet external program (door): an EGA, parser-
driven quest rendered to the terminal as sixel/JPEG-XL graphics via ScummVM.

This directory (`xtrn/cascadequest/`) is the installed door — the `syncscumm`
binary and the game data live here. The **source** (the ScummVM engine
collection plus the Synchronet backend that drives it) lives in
`src/doors/syncscumm/` of the Synchronet source tree; that one binary plays
every SyncSCUMM title, this directory just points it at Cascade Quest. The
**SCI engine** it needs was added to that build alongside AGI.

## Licensing — please read

Unlike Space Quest 0, Cascade Quest is **not** built on Sierra's IP — it's
Phil Fortier's own original story and art, using only the (freely
reimplemented) SCI engine. He released it as freeware on the SCI community
site; he has since built a **commercial reboot, *Cascadia Quest*** (Steam), so
this free original is his older release. It carries no explicit redistribution
licence of its own.

So **this repository bundles no game data.** `getdata.js` downloads the game
from the author-community host (`sciprogramming.com`, sha256-pinned) at install
time and unpacks it here; the bytes never live in the Synchronet repository.
archive.org mirrors the same file (`sci_Cascade_Quest_Demo`) if the host is
down — the pinned sha256 lets you verify any copy is identical. Installing this
door and running its `getdata.js` is the sysop's decision to serve it.

## Building the door binary

```
cd src/doors/syncscumm
./build.sh                 # Windows: build.bat  (needs the SCI engine -- it's in build.sh)
jsexec deploy.js           # copies the binary into each xtrn/ title dir
```

## Installing into Synchronet

- **From the BBS, as a sysop** — run **Auto-install New External Programs**,
  find **Cascade Quest**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/cascadequest`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/cascadequest`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`, and
its `[exec:getdata.js]` step downloads the game (prompted, non-fatal if
declined). To fetch it yourself, or re-run later:

```
jsexec xtrn/cascadequest/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## Game data & play

`getdata.js` fetches the SCI resource files (`resource.map`, `resource.001`,
`resource.cfg`) into this directory; ScummVM detects them as game id
**`sci-fanmade`** and identifies Cascade Quest from the resource hashes. The
ZIP's bundled DOS interpreter (`SCIV.EXE`, `*.DRV`) is skipped — ScummVM does
not use it. Play is keyboard-first: a text parser plus arrow/keypad movement,
with SCI's clickable menu bar for the rest.

## Per-user saves & config

`--savepath` and `-c` give each user their own save directory and ScummVM
config under `data/user/<####>/cascadequest/` — no cross-user or cross-node
collisions. ScummVM's F5 in-game menu handles save slots.

## Credits

Cascade Quest © Phil Fortier (Troflip). The ScummVM engine is GPL-2.0+. This
door's own code (the fetcher + config in this directory) is GPL-2.0; it ships
no game data.
