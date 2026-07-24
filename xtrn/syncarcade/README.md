# SyncArcade

**Arcade cabinets** as a Synchronet external program (door): coin-op classics
played over the BBS, rendered to the terminal as sixel graphics with the game's
own audio streamed to SyncTERM.

This directory (`xtrn/syncarcade/`) is the installed door — the `syncretro`
binary, the MAME 2003-Plus core, the lobby, `syncretro.ini`, `games.ini`, and
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
it is the same one. That holds **per platform**, though: an install shared by a
Windows host and a \*nix one needs each host's own binary *and* its own core
(`.dll` at the door root, `.so` under `linux-x64/`). Run `build.sh`/`build.bat`,
`deploy.js` and `getcore.js` once on each.

Full build instructions — prerequisites, configure options, the unit tests and
the `probe_core` diagnostic — are in `src/doors/syncretro/COMPILING.md`.

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
  Readable names for the picker come from `games.ini` instead — see
  [Per-game facts](#per-game-facts).
- **Some games need their parent romset present.** In a split set a clone holds
  only what differs from its parent, and loads the rest — graphics ROMs, PROMs —
  out of the parent's zip. That parent is just another zip in `roms/`, not a
  BIOS you have to hunt down. See the warning under
  [What to hide](#what-to-hide) before you tidy any of them away.
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

**Twin-stick cabinets** (Battlezone's two treads, Robotron's move-and-aim) get
a second stick on `I` `K` (up/down). MAME 2003-Plus reads a machine's second
stick off the RetroPad's right analog stick and nowhere else — no button, no
d-pad, no core option reaches it — so these two keys are the only path in. A
game with one stick simply ignores them; its driver never reads that axis.

A cabinet's own control labels — which button fires, whether it has a second
stick — are not something MAME 2003-Plus tells the door: it reports button
*counts* per driver, never what a button *does*. `games.ini` supplies that,
measured per romset rather than guessed (see
[Per-game facts](#per-game-facts)). The in-game help (`?`) renders one line per
labelled key on a cabinet with an entry, and falls back to the generic
"buttons 1 and 2" grouping on one without; it shows the `I`/`K` row only for a
cabinet whose `games.ini` section sets `stick2` — most romsets have no second
stick, and the row would be noise on every one of them. Battlezone is the
labelled example: its help reads `C  Fire` and `I K  Right tread` in place of
"buttons 3 and 4" and a row every other cabinet would also show.

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

## Per-game facts

Because a romset cannot be renamed, `games.ini` maps romset names to readable
titles for the picker — `[puckman] name = Pac-Man (Japan)`. A romset with no
section is listed under its raw name, which is ugly but harmless. **Extend that
file rather than renaming a zip.**

It also carries what MAME will not tell the door: `button.<id>` names what a
RetroPad button actually is on that cabinet, and `stick2` labels a twin-stick
cabinet's second stick — the one reached only via `I`/`K` (see
[Playing](#playing)) — so the in-game help can say `C -- Fire` and
`I K -- Right tread` instead of numbering six buttons the driver may not even
have and showing a second-stick row on cabinets that have no second stick.
Those values are measured with `probe_core -hold`, never guessed, and a
cabinet is labelled completely or not at all. See
[GAMES_INI.md](../../src/doors/syncretro/GAMES_INI.md).

**`names.json` is no longer read.** An install that had one keeps its file, and
the picker ignores it; re-enter any hand-added titles as `games.ini` sections.

## Configuration

`syncretro.ini` (seeded from `syncretro.example.ini`) holds the sysop's knobs.
Two are worth knowing about here:

- **`[options]`** — libretro core options, as `key = value`. This console pins
  `mame2003-plus_skip_disclaimer` and `mame2003-plus_skip_warnings` so a player
  is not made to sit through MAME's legal screens, and — measured with
  `probe_core` — `mame2003-plus_vector_resolution = original` and
  `mame2003-plus_vector_antialias = disabled`, which draw Battlezone at its own
  400×300 rather than the core's supersampled 1024×768 default and keep the
  sixel quantizer's colour count exact. Anything not listed keeps the core's
  own default. These live in the ini rather than on the door's command line
  because the BBS builds that command line in a fixed 260-character buffer and
  truncates it silently.
- **`[roms] exclude`** — a MAME collection is mostly things you do *not* want in
  a BBS picker. Every game has several clones and bootlegs (the parent is the
  one to show), and **Tempest** wants a spinner this door has no input path
  for. Battlezone is not in the same boat: measured with `probe_core`, its
  native vector resolution is 400×300 (120k pixels a frame, under Pac-Man's own
  200k/frame budget), and its controls are two digital joysticks, which this
  console's own key bindings already reach (see [Playing](#playing)). Hide
  unwanted romsets by name.

### Hide unwanted romsets — do not move or delete them

**Use `[roms] exclude`. Do not tidy romsets out of `roms/`.**

A clone cannot load without its parent, and *the parent is often the entry that
looks most redundant*. Moving `puckman.zip` into a `pruned/` sub-directory —
which looks like housekeeping, since "Pac-Man (Japan)" and "Pac-Man" are plainly
the same game — silently breaks `pacman.zip`, because the Midway clone loads its
graphics ROMs and PROMs out of the Japanese parent's zip. MAME does **not**
search sub-directories of the ROM directory, so one level down is as fatal as
deleting it.

Nothing warns you. The parent/clone relationship lives inside MAME's driver
tables, so neither the lobby nor the door can see it: the game stays in the
picker and the player is told only that the content is wrong for this core.

`exclude` drops the menu entry while leaving the file where MAME can still reach
it, which is what you want:

```ini
[roms]
; hide the duplicate entry, keep the file pacman.zip depends on
exclude = puckman
```

`[audio]` and `[video]` behave as they do for the other consoles; see
`syncretro.example.ini`, which documents every key.

### Bandwidth: only the changed cells are sent

`[video] dirty_rect` (on by default) repaints just the parts of the picture that
changed rather than re-sending the whole frame — the terminal is already holding
the last one, and a sixel can be drawn at any character cell. How much that wins
depends entirely on how much of the screen moves, and the `dr N%` field in the
`Ctrl-S` overlay reports it live. Measured in active play over SyncTERM:

| game | frames patched rather than repainted |
|---|---|
| Pac-Man, Frogger | **80–90%** |
| Galaga | **20–30%** |

The split is the games, not a defect. Pac-Man and Frogger are a static maze or
a static road with a few sprites crossing it. Galaga moves an entire formation
at once, so most of its frames change too much of the screen to be worth
patching and correctly fall back to a whole frame.

Requires SyncTERM (it is the terminal that persists sixel colour registers, so
the palette can be omitted from each patch). Every other terminal, and any frame
where the palette changed or a door screen painted over the game, sends a whole
frame — which is always correct, and is what the door did before this existed.

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
