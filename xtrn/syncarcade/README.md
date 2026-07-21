# SyncArcade

**Arcade cabinets** as a Synchronet external program (door): coin-op classics
played over the BBS, rendered to the terminal as sixel graphics with the game's
own audio streamed to SyncTERM.

This directory (`xtrn/syncarcade/`) is the installed door — the `syncretro`
binary, the MAME 2003-Plus core, the lobby, `syncretro.ini`, `names.json`, and
your romsets. The **source** lives in `src/doors/syncretro/` of the Synchronet
source tree.

**One door, many consoles.** SyncArcade is not its own program: it is the
SyncRetro door pointed at a different [libretro](https://www.libretro.com/)
core. The same binary runs `xtrn/syncivision` (Intellivision) and `xtrn/syncnes`
(NES) from the same source. What makes *this* one an arcade is `lobby.js` — it
names the machine, its core, its key bindings and what a romset looks like.

An arcade cabinet differs from a cartridge console in two ways this door had to
learn: the ROM's **filename is data** (see [Romsets](#romsets)), and the
**high-score table belongs to the machine**, not to each player (see
[High scores](#high-scores-are-shared)).

## Building the door binary

The `syncretro` binary is not produced by the normal Synchronet build; build it
separately, then deploy it here.

- A checkout of the **Synchronet source tree** — the build compiles against the
  in-tree `xpdev` and `termgfx` libraries, so a binary-only install is not enough.
- **CMake** 3.13+ and a C compiler (**GCC**/**Clang**, or **MSVC** on Windows).

```
cd src/doors/syncretro
./build.sh                 # Windows: build.bat
jsexec deploy.js           # installs the binary into EVERY console install dir
```

If you already installed SyncNES or SyncIvision, you have the binary already —
it is the same one.

## Installing into Synchronet

SyncArcade ships an `install-xtrn.ini`, so the bundled installer registers it
for you — no manual SCFG entry needed:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), find **Arcade**, install it.
- **Command line** — `jsexec install-xtrn ../xtrn/syncarcade`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncarcade`.

The installer seeds `syncretro.ini` from `syncretro.example.ini` and offers
(prompted) to download the **MAME 2003-Plus** core from the libretro buildbot.
Note that core's licence differs from the sibling consoles' — see
[Legal](#legal). You can also run it later:
`jsexec ../xtrn/syncarcade/getcore.js`.

`xtrn.ini` is not re-read automatically — `touch ctrl/recycle.term` afterwards.

## Romsets

**SyncArcade ships no romsets.** They are copyrighted content you supply. Drop
them into `roms/` and they appear in the lobby on the next entry.

- **Extension**: `.zip`. Unlike the cartridge consoles, the archive is correct —
  MAME opens the zip itself and expects it.
- **Never rename a romset.** MAME identifies the game *by the zip's basename*:
  `puckman.zip` is the puckman driver. Rename it and the game stops existing.
  Readable names for the picker come from `names.json` instead — see
  [Display names](#display-names).
- **Some games need their parent romset present.** `mspacman.zip` resolves
  `puckman.zip` in a split set. That is just another zip in `roms/`, not a BIOS
  you have to hunt down.
- **No BIOS is needed** for the classics. (A few later machines have their own
  boot roms, supplied the same way as any other romset.)

### Romsets are version-specific — the one thing that surprises people

MAME 2003-Plus is built on **MAME 0.78**, so it wants romsets of that vintage. A
newer collection *mostly* works — 216 of 272 zips from a 2011-era (0.14x) set
loaded in testing — and the failures split roughly evenly between two causes:

- **Files renamed inside the zip** between MAME versions. The game's driver is
  present and the ROM data is usually identical; only the internal filenames
  changed. A romset manager (clrmamepro and friends) can rebuild a 0.78-
  compatible set from files you already have.
- **Games or clones added to MAME after 0.78.** No romset version helps — the
  core simply does not know the game. The *parent* game usually loads even when
  a clone does not.

When a romset is refused, the door tells the player it is the **content** that is
wrong for this core, rather than blaming the core. The reason is in the door log
(`data/syncretro/syncretro_n<node>.log`), which names the files MAME wanted.

## Playing

An arcade control panel is a joystick, a coin slot and a start button — which is
a plain RetroPad, so the standard `pad` bindings apply:

| Player 1 | Action |
|---|---|
| `W A S D` / arrows | joystick |
| `Z` `X` `C` `V` | buttons 1–4 |
| `Backspace` | **insert coin** |
| `Enter` | **1-player start** |

**Insert a coin first, then press start** — exactly like the cabinet. Nothing
happens until you do.

Door keys: `Space` pause · `?` help · `Ctrl-S` stats overlay · `F4` cycle the
graphics tier · `+`/`-` volume · `Ctrl-R` reset · `Ctrl-Q` quit.

> **Known rough edge.** The in-game help (`?`) still shows the generic RetroPad
> names it uses for the NES — "Select", "d-pad", "B/A" — rather than "insert
> coin", "joystick", "button 1". The *bindings* are correct; only the labels
> read as console rather than cabinet.

## High scores are shared

Unlike the cartridge consoles, **every player shares one save directory**
(`data/syncretro/arcade/shared`), so the score you set is the score the next
caller has to beat. That is the point of a cabinet, and it is why this console
sets `shared_saves: true` in its `lobby.js` instead of taking the per-user
sandbox a cartridge console wants. MAME 2003-Plus writes its hiscore files (and
its input config) under the save directory the frontend hands it.

**Known limitation:** two nodes playing the *same* game simultaneously each hold
that game's scores in memory and write them at exit, so the second one out wins
and the first one's scores are lost. It is a lost update, not a corrupt file,
and it needs a busy BBS and one popular game to happen at all.

## Display names

Because a romset cannot be renamed, `names.json` maps romset names to readable
titles for the picker — `puckman` → *Pac-Man (Japan)*, `mspacman` → *Ms.
Pac-Man*. A romset with no entry is listed under its raw name, which is ugly but
harmless. **Extend that file rather than renaming a zip.** Keys beginning with
`_` are ignored, which is how the shipped file carries its own comment.

## Configuration

`syncretro.ini` (seeded from `syncretro.example.ini`) holds the sysop's knobs.
Two are worth knowing about here:

- **`[options]`** — libretro core options, as `key = value`. This console pins
  `mame2003-plus_skip_disclaimer` and `mame2003-plus_skip_warnings` so a player
  is not made to sit through MAME's legal screens. Anything not listed keeps the
  core's own default. These live in the ini rather than on the door's command
  line because the BBS builds that command line in a fixed 260-character buffer
  and truncates it silently.
- **`[roms] exclude`** — a MAME collection is mostly things you do *not* want in
  a BBS picker. Every game has several clones and bootlegs (the parent is the one
  to show), and **vector games** (Battlezone, Tempest) render at 1024×768 —
  twelve times Pac-Man's pixel count — and want a spinner or a dual stick
  besides. Hide them by name.

`[audio]` and `[video]` behave as they do for the other consoles; see
`syncretro.example.ini`, which documents every key.

## Why MAME 2003-Plus, and not a newer MAME

Measured with `probe_core` (`src/doors/syncretro/probe_core.c`) against MAME
2003-Plus (MAME 0.78 + backported drivers) and MAME 2010 (MAME 0.139):

| | 2003-Plus | 2010 |
|---|---|---|
| vertical games (Pac-Man, Galaga, Dig Dug, Donkey Kong) | delivered **upright** | delivered **sideways** (`SET_ROTATION 3`) |
| crashes across 270 romsets loaded | **0** | reproducible access violation |
| Windows path handling | normalizes `/` and `\` | `/` fails: *"Game not found"* |
| compiled Win32 size | 26 MB | 44 MB |
| high scores | **native save** | external `hiscore.dat` only |
| 4-way stick simulation | **yes** | no |

MAME 0.139 has more drivers, but what it adds over 0.78 is overwhelmingly 1990s
titles — and those are exactly what a terminal cannot serve. Of 216 romsets that
loaded here, **208 are under 200k pixels a frame** and half are Pac-Man sized;
the only ones above that are vector games.

## Legal

The libretro API is permissive, but **MAME 2003-Plus is under the MAME licence,
not the GPL**, and that licence restricts *commercial* redistribution. Nothing
here redistributes it: `getcore.js` downloads it, at your prompting, from the
libretro buildbot — the same thing you would do by hand. A BBS that charges for
access should read that licence first.

**Arcade romsets are commercial software and none are shipped.** Sourcing
legally-owned copies is the sysop's responsibility. This door's own code is
GPL-2.0.
