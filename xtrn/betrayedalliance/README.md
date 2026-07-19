# Betrayed Alliance Book 1

**Betrayed Alliance Book 1** — an original freeware adventure by **Ryan
Slattery** (Slattstudio), built on Sierra's SCI interpreter — as a Synchronet
external program (door): a classic EGA, type-parser quest (the first of a
planned trilogy) rendered to the terminal as sixel/JPEG-XL graphics via
ScummVM.

This directory (`xtrn/betrayedalliance/`) is the installed door — the
`syncscumm` binary and the game data live here. The **source** (the ScummVM
engine collection plus the Synchronet backend that drives it) lives in
`src/doors/syncscumm/` of the Synchronet source tree; that one binary plays
every SyncSCUMM title, this directory just points it at Betrayed Alliance. The
**SCI engine** it needs was added to that build alongside AGI.

## Licensing — the cleanest of the fetch-only titles

Betrayed Alliance is **MIT-licensed**: the game ships a `LICENSE.TXT` that
expressly permits use, copying, publishing, and redistribution. That makes it
the one title here we could legally *bundle* — unlike Space Quest 0 (Sierra
IP) or Cascade Quest (no explicit grant). This package still **fetches** it
rather than bundling: `getdata.js` downloads the pinned `release-v1.3.3.1`
from the **author's own GitHub release** (sha256-verified) and unpacks it
here, keeping the MIT `LICENSE.TXT` with the game data to honor the notice
clause. Lighter repo, always the pinned release, and consistent with the other
SyncSCUMM titles.

## Building the door binary

```
cd src/doors/syncscumm
./build.sh                 # Windows: build.bat  (needs the SCI engine -- it's in build.sh)
jsexec deploy.js           # copies the binary into each xtrn/ title dir
```

## Installing into Synchronet

- **From the BBS, as a sysop** — run **Auto-install New External Programs**,
  find **Betrayed Alliance Book 1**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/betrayedalliance`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/betrayedalliance`.

The installer seeds a reference `scummvm.ini` and the door's own
`syncscumm.ini` from their templates, and its `[exec:getdata.js]` step
downloads the game (prompted, non-fatal if declined). To fetch it yourself,
or re-run later:

```
jsexec xtrn/betrayedalliance/getdata.js
```

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## Game data & play

`getdata.js` fetches the SCI resource files (`resource.map`, `resource.001`,
`RESOURCE.CFG`) plus the game's `LICENSE.TXT` into this directory; ScummVM
detects them as game id **`sci-fanmade`** and identifies **Betrayed Alliance
1.3.3.1** by resource hash (the newest version its detection tables know). The
release zip also bundles a DOSBox package for playing on modern Windows —
skipped, since ScummVM does not use it. Play is keyboard-first: a text parser
plus arrow/keypad movement, with SCI's clickable menu bar for the rest.

## Per-user saves & config

`--savepath` and `-c` give each user their own save directory and ScummVM
config under `data/user/<####>/betrayed/` — no cross-user or cross-node
collisions. ScummVM's F5 in-game menu handles save slots.

## Credits

Betrayed Alliance Book 1 © Ryan Slattery / Slattstudio, MIT-licensed
(<https://slattstudio.com/>). The ScummVM engine is GPL-2.0+. This door's own
code (the fetcher + config in this directory) is GPL-2.0; it ships no game
data.
