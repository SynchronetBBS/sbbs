# Space Quest 0: Replicated

**Space Quest 0: Replicated** — a 2003 fan-made prequel to Sierra's Space
Quest series by **Jeff Stewart**, built with Sierra's AGI interpreter — as a
Synchronet external program (door): Roger Wilco wakes aboard Labion Orbital
Station 10 to find the crew murdered, in an old-fashioned text-parser, 16-color
adventure, rendered to the terminal as sixel/JPEG-XL graphics via ScummVM.

This directory (`xtrn/spacequest0/`) is the installed door — the `syncscumm`
binary and the game data live here. The **source** (the ScummVM engine
collection plus the Synchronet backend that drives it) lives in
`src/doors/syncscumm/` of the Synchronet source tree; that one binary plays
every SyncSCUMM title, this directory just points it at Space Quest 0. The
**AGI engine** it needs was added to that build alongside SCI.

## Licensing — please read

Unlike Beneath a Steel Sky and Flight of the Amazon Queen (officially
relicensed freeware), **Space Quest 0: Replicated is a fan game that builds on
Sierra's copyrighted/trademarked Space Quest IP.** The author's page states
only *"Roger Wilco and related materials are © Sierra On-Line. Space Quest is a
registered trademark…"* — it grants no redistribution licence of its own.

So **this repository bundles no game data.** `getdata.js` downloads the game
from the **author's own host** (`https://www.wiw.org/~jess/download/rep_104.zip`,
sha256-pinned) at install time and unpacks it here; the bytes never live in the
Synchronet repository. Installing this door and running its `getdata.js` is the
sysop's decision to serve a fan game that uses Sierra's IP. archive.org mirrors
the same file (item `SpaceQuest0Replicated`) if the author's host is down — the
pinned sha256 lets you verify any copy is identical.

## Building the door binary

The `syncscumm` binary is not produced by the normal Synchronet build; build it
separately (it needs the AGI engine enabled — it is, in `build.sh`), then
deploy it here.

```
cd src/doors/syncscumm
./build.sh                 # Windows: build.bat
jsexec deploy.js           # copies the binary into each xtrn/ title dir
```

## Installing into Synchronet

- **From the BBS, as a sysop** — run **Auto-install New External Programs**,
  find **Space Quest 0: Replicated**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/spacequest0`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/spacequest0`.

The installer seeds a reference `scummvm.ini` from `scummvm.example.ini`, and
its `[exec:getdata.js]` step downloads the game (prompted, non-fatal if
declined). To fetch it yourself, or re-run later:

```
jsexec xtrn/spacequest0/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## Game data & play

`getdata.js` fetches the AGI game files (`LOGDIR`, `PICDIR`, `VIEWDIR`,
`SNDDIR`, `OBJECT`, `WORDS.TOK`, `VOL.*`) into this directory; ScummVM detects
them as game id **`sq0`**. The ZIP's bundled DOS/Windows interpreter
(`.exe`/`.dll`/`.nbf`) is skipped — ScummVM does not use it. AGI is
keyboard/parser-driven (type commands, arrow keys to move); no mouse or audio
is required.

## Per-user saves & config

`--savepath` and `-c` give each user their own save directory and ScummVM
config under `data/user/<####>/spacequest0/` — no cross-user or cross-node
collisions. ScummVM's F5 in-game menu handles save slots.

## Credits

Game © Jeff Stewart (2003), built on Sierra's AGI. Space Quest and Roger Wilco
are Sierra On-Line's. The ScummVM engine is GPL-2.0+. This door's own code
(the fetcher + config in this directory) is GPL-2.0; it ships no game data.
