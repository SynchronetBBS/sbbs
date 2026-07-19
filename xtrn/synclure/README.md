# SyncLure (Lure of the Temptress)

**Lure of the Temptress** — Revolution Software's 1992 fantasy point-and-click
adventure (their first game) — as a Synchronet external program (door): Diermot,
an unlikely hero, must free the village of Turnvale from the sorceress Selena
and her Skorl warriors, rendered to the terminal as sixel/JPEG-XL graphics with
sound streamed to SyncTERM.

This directory (`xtrn/synclure/`) is the installed door — the `syncscumm`
binary and the game data (`Disk*.vga`, `lure.dat`) live here. The **source**
(the ScummVM engine collection plus the Synchronet backend that drives it)
lives in `src/doors/syncscumm/` of the Synchronet source tree; that one binary
plays every SyncSCUMM title, this directory just points it at Lure.

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

`deploy.js` copies the built binary in as `xtrn/synclure/syncscumm`
(`syncscumm.exe` on Windows).

## Installing into Synchronet

SyncLure ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs**
  (the `xtrn-setup` module), find **Lure of the Temptress**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/synclure`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/synclure`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`, and
its `[exec:getdata.js]` step downloads the game data for you (prompted,
non-fatal if declined or offline). To fetch it yourself, or re-run later:

```
jsexec xtrn/synclure/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term`
afterwards.

## Game data

**Lure of the Temptress is officially freeware**, released for free
redistribution by Revolution Software, and `getdata.js` downloads it straight
from ScummVM's own hosting — nothing copyrighted-and-unlicensed is bundled in
this repository, and nothing needs to be supplied by the sysop.

It fetches `lure-1.1.zip`, verified against ScummVM's published `.sha256`, and
unpacks only the **VGA** disk resources (`Disk1.vga` … `Disk4.vga`) plus the
freeware `LICENSE.txt`. Taking VGA alone (the archive also carries an EGA build,
`Lure.exe`, and PDFs) makes ScummVM detect one game rather than a VGA/EGA pair.

The lure engine also needs ScummVM's own `lure.dat` engine-data file. That
belongs to ScummVM rather than the game, so it isn't fetched — it **ships with
this package** and the door finds it via `--path`.

## Per-user saves & config

`--savepath` and `-c` on the door's command line (`install-xtrn.ini`) give
each user their own save directory and ScummVM config file, under
`data/user/<####>/lure/` — no cross-user or cross-node collisions. ScummVM's
own F5 in-game menu handles save slots and naming, and **Ctrl-G** opens the
ScummVM Global Main Menu (volume, save/load, quit) from any terminal.

## Legal

Lure of the Temptress's game data is freeware, released for free redistribution
by Revolution Software; `getdata.js` fetches it from ScummVM's own distribution
point rather than this repository shipping it. The ScummVM engine is GPL-2.0+.
This door's own code is GPL-2.0.
