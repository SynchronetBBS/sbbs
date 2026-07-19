# SyncDrascula (Drascula: The Vampire Strikes Back)

**Drascula: The Vampire Strikes Back** — the 1996 comedy-horror point-and-click
adventure by Alcachofa Soft (Emilio de Paz) — as a Synchronet external program
(door): John Hacker, a hapless British antique dealer, is dragged into
Transylvania to thwart Count Drascula's plans, rendered to the terminal as
sixel/JPEG-XL graphics with sound streamed to SyncTERM.

This directory (`xtrn/syncdrascula/`) is the installed door — the `syncscumm`
binary and the game data (`Packet.001`, `audio/`) live here. The **source**
(the ScummVM engine collection plus the Synchronet backend that drives it)
lives in `src/doors/syncscumm/` of the Synchronet source tree; that one binary
plays every SyncSCUMM title, this directory just points it at Drascula.

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

`deploy.js` copies the built binary in as `xtrn/syncdrascula/syncscumm`
(`syncscumm.exe` on Windows).

## Installing into Synchronet

SyncDrascula ships an `install-xtrn.ini`, so the bundled installer registers
it for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs**
  (the `xtrn-setup` module), find **Drascula: The Vampire Strikes Back**,
  install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncdrascula`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncdrascula`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`, and
its `[exec:getdata.js]` step downloads the game data for you (prompted,
non-fatal if declined or offline). To fetch it yourself, or re-run later:

```
jsexec xtrn/syncdrascula/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term`
afterwards.

## Game data

**Drascula is officially freeware**, released for free redistribution by its
author, and `getdata.js` downloads it straight from ScummVM's own hosting —
nothing copyrighted-and-unlicensed is bundled in this repository, and nothing
needs to be supplied by the sysop.

It fetches, verified against ScummVM's published `.sha256` before unpacking:

- **`drascula-1.0.zip`** (required) — the game itself (`Packet.001`). Like
  Beneath a Steel Sky (`xtrn/syncbass`) and unlike Flight of the Amazon Queen
  (`xtrn/syncqueen`, two distinct builds), Drascula is a single flat build:
  one data set serves everyone, with the door playing speech/music to a
  terminal that has audio and showing subtitles to one that does not.
- **`drascula-audio-mp3-2.0.zip`** (optional) — the CD soundtrack as MP3, kept
  in an `audio/` subdirectory. The MP3 pack (over the OGG/FLAC ones) is chosen
  because the SyncSCUMM build force-enables libmad, so MP3 always decodes.
  Without it, Drascula still plays with the engine's built-in Adlib music.

The drascula engine also needs ScummVM's own `drascula.dat` engine-data file.
That belongs to ScummVM rather than the game, so it isn't fetched — it **ships
with this package** and the door finds it via `--path`.

## Per-user saves & config

`--savepath` and `-c` on the door's command line (`install-xtrn.ini`) give
each user their own save directory and ScummVM config file, under
`data/user/<####>/drascula/` — no cross-user or cross-node collisions.
ScummVM's own F5 in-game menu handles save slots and naming.

## Legal

Drascula: The Vampire Strikes Back's game data is freeware, released for free
redistribution by its author; `getdata.js` fetches it from ScummVM's own
distribution point rather than this repository shipping it. The ScummVM engine
is GPL-2.0+. This door's own code is GPL-2.0.
