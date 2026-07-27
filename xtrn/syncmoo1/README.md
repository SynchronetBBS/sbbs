# SyncMOO1

**Master of Orion 1** — the 1993 4X space-empire classic — as a Synchronet
external program (door), rendered to the terminal as sixel graphics with sound
and music streamed to SyncTERM. The door is C, built on the vendored
[1oom](https://sourcecraft.dev/fork1oom/1oom) engine. Unlike SyncDOOM or
SyncDuke it is **single-player, turn-based, and mouse-driven** — there is no
network play and no lobby.

This directory (`xtrn/syncmoo1/`) is the installed door — the `syncmoo1`
binary, `syncmoo1.ini`, the data-install helper, and the Master of Orion data
files all live here. The **source** lives in `src/doors/syncmoo1/` of the
Synchronet source tree.

Two things you must deal with before your first player: the door needs a
**sixel-capable terminal**, and it needs **Master of Orion data files that you
supply**. Both are covered below.

## Terminal requirements

Master of Orion is a picture — there is no text mode to fall back to — so
SyncMOO1 renders **sixel only**. SyncTERM is the reference client. A caller
whose terminal can't draw sixel is shown a short notice and returned to the
BBS rather than left staring at nothing; you can reword or re-skin that notice
(`[text]` in `syncmoo1.ini`, below).

Mouse support is used but not required — every menu item also has a keyboard
hotkey. See [Playing](#playing).

## Building the door binary

The `syncmoo1` binary is not produced by the normal Synchronet build; build it
separately as below, then install it into this directory.

### Prerequisites

- A checkout of the **Synchronet source tree** — the build compiles against the
  in-tree `xpdev` library, so a binary-only install is not enough.
- **CMake** 3.13 or newer and a C compiler (**GCC** or **Clang**).

You do *not* need libjxl: SyncMOO1 serves the sixel tier only.

### Building

From the door's source directory (`<sbbs>/src/doors/syncmoo1`):

```
./build.sh                 # Release build -> build/syncmoo1
./build.sh debug           # Debug build
./build.sh clean           # wipe ./build/ first
```

Equivalently by hand: `cmake -B build && cmake --build build -j`.

Building never touches a live install. Install the freshly-built binary into
this directory as a separate, explicit step, so you can rebuild and test before
putting a new binary in front of players:

```
jsexec deploy.js
```

### Platform support

- **Linux / Unix-like** with GCC or Clang — supported.
- **Windows / MSVC** — supported (Visual Studio 2022); build with the
  `build.bat` helper, then `jsexec deploy.js`. **Win32 (x86)** is the supported
  Windows target, and a Win32 door runs on a Win64 Synchronet host too, so the
  one binary covers every Windows BBS.

## Installing into Synchronet

SyncMOO1 ships an `install-xtrn.ini`, so the bundled installer registers it for
you — no manual SCFG entry needed. Launch the installer any of these ways; the
menu-driven ones discover SyncMOO1 automatically (you just pick it from the
list), while the command line takes the path:

- **From the BBS, as a sysop** — run **Auto-install New External Programs** (the
  `xtrn-setup` module), included by default in the **Operator** external-programs
  section. Find **Master of Orion** in the list and install it.
- **SBBSCTRL (Windows)** — **File → Run → Install External Programs**.
- **Command line** — `jsexec install-xtrn ../xtrn/syncmoo1`.
- **Terminal sysop command** — `;exec ?install-xtrn ../xtrn/syncmoo1`.

The installer registers the door, seeds `syncmoo1.ini` from
`syncmoo1.example.ini` (it never overwrites an existing one without asking),
and then offers — prompted, and safe to decline — to install the Master of
Orion data files from a copy you've placed in this directory. Declining leaves
the door installed; you can supply the data at any time afterwards.

## Master of Orion data files — you must supply these

1oom is an **engine**, not game content. It needs the original **Master of
Orion 1 version 1.3** `.LBX` data files, which are commercial content
© Simtex / MicroProse and are **not shipped with this door**. Supplying a
legally-owned copy is the sysop's responsibility. The game is still sold,
cheaply:

- **[Master of Orion 1+2 on GOG](https://www.gog.com/en/game/master_of_orion_1_2)**
  — recommended, usually around US$6 and **DRM-free**, so the installer unpacks
  straight to the original DOS files.
- **Steam** — the classics ship only with the pricier *Master of Orion*
  Collector's Edition, and are DRM-wrapped, so GOG is the simpler source.

Any legally-owned copy of the DOS v1.3 data works — a boxed original, an old CD.
Version 1.3 is the only version 1oom can run.

**Put your copy in this directory**, then let `getdata.js` sort it out. It
accepts any of three shapes:

- the loose `*.lbx` files dropped straight in;
- a `.zip` or other archive containing them; or
- an extracted GOG/DOS game folder (a subdirectory holding the `*.lbx` files).

It extracts or copies the data files into place, lower-casing the names, and
**downloads nothing**. It's idempotent — files already present are left alone —
so re-running only fills in what's still missing:

```
jsexec ../xtrn/syncmoo1/getdata.js
```

**The `v11.lbx` trap.** `v11.lbx` is the one data file with a lower-case name;
all the others are upper-case. A copy made with a `*.LBX` pattern silently
skips it — and 1oom checks that file as its version 1.3 marker, so the door
won't start without it. If `getdata.js` reports it missing, the file is very
likely still sitting in the copy you took the data from.

## Configuration

**`syncmoo1.ini`** is read from this directory at launch and is fully commented
in-file; every key is optional. Read it before your first player does: without
it the door runs on 1oom's stock defaults, which means **Master of Orion's copy
protection is armed** — after year 40, on a random turn, it demands a ship name
printed in the boxed manual and ends the game after three wrong answers. BBS
players have no manual. The shipped template disables that, and enables 1oom's
quality-of-life preset besides.

- **`[1oom]`** — defaults for the engine's own settings, keyed by 1oom's own
  names. These are **defaults, not overrides**: a player who changes one in the
  in-game options menu keeps their choice, while a player who never touches it
  follows whatever you set, even if you change your mind later. The template
  ships 1oom's own preset plus the copy-protection fix.
- **`[video] hand_cursor`** — draw Master of Orion's hand-shaped mouse cursor.
  Off by default, because the player's terminal already shows a pointer and the
  two don't line up.
- **`[audio] music_quality`** — Ogg/Vorbis quality for uploaded music tracks;
  lower means a smaller upload and softer sound.
- **`[text]`** — the notice shown to a caller with no sixel: `no_graphics` for
  one-line wording, `no_graphics_file` for a multi-line or ANSI-art file (drop
  in `nographics.txt` and nothing else needs setting), and `no_graphics_pause`
  for how long it's held before the BBS repaints over it.
- **`[idle]`** — end the game and hand an idle player back to the BBS. Default
  **10 minutes**; leaving it unset is *not* the same as off (set `0` for that).
  Both keystrokes and mouse movement count as being present. SCFG's **Maximum
  Inactivity** can't do this job for a graphical door — the door's frame pacing
  makes the terminal answer several times a second, so that timer never fires.
- **`[debug]`** — `wire` records the whole terminal conversation for
  troubleshooting (multi-MB per session; leave it off), and `hide_console` is a
  Windows-only safety net for BBSes with no `XTRN_NODISPLAY` equivalent.

Per-user settings and saved games are written under `data/user/<num>/moo1/`,
which Synchronet auto-cleans with the user account. A player's config records
only the settings they actually changed, so your house defaults keep reaching
everyone else.

## Playing

Menus are driven by the **mouse** and by **per-item keyboard hotkeys**; `+`,
`-`, `=` and `Shift`+key work as in the original. **Arrow keys do not navigate
the menus** — use the hotkeys or the mouse.

Master of Orion's sound effects and its 40-track soundtrack both play over
SyncTERM's audio channel. A terminal without it hears silence and is otherwise
unaffected. Volume and on/off are the game's own settings, under
**Options → Sound**, and you can seed them for new players from `[1oom]`.
Rendered music is cached under `data/syncmoo1/audio/`, so a track rendered for
one player ships straight from disk for the next.

## Logging & diagnostics

The door normally runs with no local console window (the installer sets
Synchronet's **Disable Local Display** for it), so diagnostics go to files:

- **`data/user/<num>/moo1/1oom_log.txt`** — the 1oom engine's own log: the data
  file search, config load, version banner, and engine errors. **Start here when
  the door won't start**; a data-file problem is named in this file.
- **The door's own diagnostics** — hangup reason, per-user directory and session
  setup failures, the idle/time-limit exit. On Windows these are captured to
  `data/syncmoo1/syncmoo1_n<node>.log`, truncated per session and tagged with
  the node number. On Linux/Unix they go to the Synchronet terminal server's own
  log, along with everything else the door prints.

## License & credits

Master of Orion is © Simtex / MicroProse. The 1oom engine is vendored under its
own **GPLv2** license; this door's own code is likewise GPLv2. Full attribution
ships with the source in `src/doors/syncmoo1/`.
