# SyncDOOM

A multiplayer **DOOM** that runs as a BBS *door* (external program), rendering
the game to the user's terminal over the telnet/SSH session — no native client,
no X11. It is a [doomgeneric](https://github.com/ozkl/doomgeneric) /
[Chocolate Doom](https://www.chocolate-doom.org/) port whose video, input and
networking back end (`syncdoom.c`) talks directly to the terminal and the
BBS-supplied TCP socket. It runs on any **DOOR32.SYS**-compatible BBS, and looks
(and sounds) best on **SyncTERM**: real graphics via its APC/JXL image-cache
protocol, and **full audio** — digital sound effects plus OPL-rendered music —
via its audio channel (**SyncTERM v1.10 or later**; see [Audio](#audio)).

Co-op and deathmatch are supported, played between BBS nodes (or against a
detached dedicated server). The companion JavaScript lobby that browses and
creates network games is host-specific and lives elsewhere; this directory is
the portable C door and its two build systems.

---

## Rendering tiers

The door picks the best picture the terminal can show, probing at startup. From
richest to most compatible:

| Tier | Wire format | Terminals |
|------|-------------|-----------|
| **JXL** | JPEG-XL still, sent as a cached APC image | SyncTERM 1.4+ (smallest, fastest) |
| **PPM** | uncompressed RGB APC image | SyncTERM without JXL (opt-in; LAN/localhost only) |
| **Sixel** | DECSIXEL raster | xterm, mlterm, foot, WezTerm, recent Windows Terminal, … |
| **Text** | half/quadrant/sextant block glyphs in ANSI color | any ANSI/UTF-8 or CP437 terminal |

Auto-selection order is **JXL → Sixel → Text** (PPM is opt-in only). Any tier can
be forced from the command line (`-jxl`, `-sixel`, `-text`) or the config file.

JXL support is a compile-time option; a build without libjxl still serves the
sixel and text tiers.

---

## Audio

Full game audio is supported — **digital sound effects** and **music** (Doom's
MUS/MIDI lumps, rendered to OPL3 FM and Opus-encoded; each track ships once and
is then cached client-side) — delivered over **SyncTERM's audio channel**, the
same transport SyncDuke uses.

This **requires SyncTERM v1.10 or later**; the door probes the terminal's audio
capability at connect. On an older SyncTERM or any other terminal the game is
fully playable, just silent — every audio call is a harmless no-op.

The JavaScript **lobby** can also play a one-shot sound when a player enters it:
set `[lobby] enter_sound` to a WAV/OGG/FLAC/VOC file you supply (e.g. a shotgun or
growl extracted from your own WAD), or a wildcard like `sfx/*.wav` to pick one at
random each entry. It plays only on a SyncTERM that can decode audio files (silent
otherwise), through the shared `cterm_lib.js` audio helpers. Nothing ships — don't
redistribute copyrighted sound.

---

## Building

The door is plain C and links one library, **xpdev** (cross-platform sockets,
recursive `mkdir`, and INI parsing), which is part of the Synchronet source
tree. Building therefore requires a checkout of that source tree, not just a
binary install. The build system is **CMake** (3.13+) on every platform.

```sh
# Linux / Unix-like (GCC or Clang)
cmake -B build && cmake --build build -j          # -> build/syncdoom
```

```bat
:: Windows (Visual Studio 2022 / MSVC)
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release              :: -> build\Release\syncdoom.exe
```

Both are supported. The JPEG-XL graphics tier is optional (the door still serves
the sixel and text tiers without it); enabling it needs a system `libjxl` on
*nix or an MSVC-built `libjxl` via vcpkg on Windows.

**See [COMPILING.md](COMPILING.md) for full instructions** — prerequisites, the
JPEG-XL tier on each platform, build options, and output locations.

---

## Command-line syntax

```
syncdoom [door/session options] [WAD options] [DOOM/engine options]
```

The door consumes the options it recognizes and passes everything else through
to the underlying DOOM engine (so standard Chocolate Doom switches such as
`-warp`, `-skill`, `-nomonsters`, `-deathmatch`, `-altdeath`, `-connect`,
`-server` work as usual).

A typical door invocation (the BBS fills in the DOOR32.SYS drop-file path):

```
syncdoom <path>/door32.sys -home <per-user-dir> -iwad <wad> -name "<user>"
```

The DOOR32.SYS drop file carries the client socket and session time limit; on a
BBS that doesn't write one, pass those directly with `-s<fd>` and `-t<seconds>`.
(The door live-probes the terminal size, so no screen-rows argument is needed.)

Run `syncdoom -help` (also `--help`, `-?`, `/?`, or with no arguments) to print
the full option list below.

### Session / terminal

| Option | Meaning |
|--------|---------|
| `-s<fd>` | Client comm **socket** descriptor (the connected telnet/SSH socket the BBS hands off). Glued form `-s7` or spaced `-s 7`. |
| `-t<seconds>` | **Time limit** for the session in seconds; the door exits when it elapses. Glued or spaced. |
| `-door32 <path>` | Path to a **DOOR32.SYS** drop file (see below). A bare path whose name is `door32.sys` is also auto-detected without this flag. |
| `-term <path>` | A `terminal.ini` file (or a directory containing one) describing `cols`/`rows`/`chars`/`desc`. Sets the baseline; explicit flags override. |
| `-home <dir>` | **Per-user storage** directory. The door `chdir`s here so DOOM's config, savegames and screenshots are written per user. Created if absent. |
| `-name <handle>` | Player name shown in multiplayer (chat, scoreboard). Default `Player`. |
| `-eventlog <path>` | Append game events (level start, deaths/frags) as JSONL to `<path>`; the lobby's activity feed reads it. The directory is created if absent. |
| `-log <path>` | Write the door's diagnostics **and any fatal error** to a durable file (also `syncdoom.ini [debug] log`, or the `SYNCDOOM_LOG` env). A bare filename goes in `<data>/syncdoom/` (node-tagged, e.g. `syncdoom_n3.log`); a path with a separator is used as-is. Off by default. See [Fatal errors & diagnostics](#fatal-errors--diagnostics). |

### Video

| Option | Meaning |
|--------|---------|
| `-jxl [0\|1\|auto]` | Force the JXL tier on (`1`), force PPM (`0`), or auto. |
| `-sixel [0\|1\|auto]` | Force sixel on (`1`), off (`0`), or auto. |
| `-text` | Force the text/block-glyph tier. |
| `-mode <m>` | Text-tier glyph mode: `half` (default), `quadrant`, `sextant`, or `space`. |
| `-charset <c>` | `utf-8` or `cp437` (text tier). |
| `-colors <c>` | Color depth: `16` (default), `8`, `256`, or `true`/`24`. |
| `-scaling <n>` | Engine framebuffer scale factor (default **2**: DOOM's 320×200 → 640×400). |
| `-jxldistance <d>` | JXL lossy quality/size lever (Butteraugli distance; ~1.0 lossless, higher = smaller). JXL builds only. |

### Input (key-repeat tuning, milliseconds)

Terminals send key-*down* only, so a held key is treated as released a short
grace period after its last byte. These tune that synthesis:

| Option | Meaning |
|--------|---------|
| `-kpsmooth <ms>` | HOLD grace (held/repeating key). |
| `-kpdelay <ms>` | TAP grace (fresh press) — the main "slidy" lever. |
| `-kpturn <ms>` | TURN grace (turn-key tap). |
| `-mouse <on\|off>` | Terminal mouse steering (default on) — see *Mouse control*. |

These three set the *startup* feel (also via `syncdoom.ini [input]`; the TURN
default is low — **75 ms** — to suit FAST TURN, below). A player retunes them
**live in-game** under **Options** (TAP/HOLD/TURN sliders) alongside a **FAST
TURN** on/off toggle (`[input] instant_turn`, default **on**) that defeats Doom's
slow-start turn-acceleration ramp — the ramp keeps resetting on terminal
key-repeat gaps and makes turning feel laggy, so full speed from the first tic
turns better over a terminal. Changes save to a per-user `syncdoom.ini` in
`-home` and override the house defaults — but **only settings the player actually
changes are written**, so the sysop's house defaults keep reaching players who
haven't touched a given setting. An explicit `-kp*` flag still wins.

**Native key-up terminals** are the exception: **SyncTERM v1.10 or later** (via
its **evdev** key reports) and terminals speaking the [kitty keyboard
protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) (e.g. **Contour**).
SyncDOOM negotiates either automatically at connect and gets **real key-up**
events — movement becomes **hold-to-move / release-to-stop** and turning uses
Doom's native acceleration ramp, with none of the grace synthesis above
involved. On such a terminal the TAP/HOLD/TURN/FAST-TURN controls show as
**NATIVE** (and do nothing), and the Ctrl-S stats strip shows **`evdev/`** or
**`kitty/`**. Other terminals keep the key-repeat scheme described here.

### WAD selection (passed through to the engine)

| Option | Meaning |
|--------|---------|
| `-iwad <file>` | Base IWAD (e.g. `doom2.wad`, `freedoom1.wad`). |
| `-file <wad…>` | One or more PWADs (appended). |
| `-merge <wad…>` | One or more PWADs merged into the IWAD namespace (NWT-style). |
| `-deh <patch…>` | One or more DeHackEd / BEX (`.deh`/`.bex`) patches (see below). |
| `-wadname <name>` | Friendly WAD-set name for the who's-online status (defaults to the IWAD's base name). |

WAD paths are resolved to absolute *before* the door `chdir`s into `-home`, so
relative WAD paths work regardless of the storage directory, and the lookup is
**case-insensitive** — a configured `doom2.wad` finds a real `DOOM2.WAD` (and
vice-versa) on case-sensitive filesystems, since DOS/Windows WADs are commonly
upper-case. If `-iwad` names a bare file, the engine's standard IWAD search also
applies (e.g. the `DOOMWADDIR` environment variable).

### Multiplayer — client

The lobby's waiting room sets these when it releases players into a match; give
them by hand only for a direct connect or an engine test:

| Option | Meaning |
|--------|---------|
| `-connect <host:port>` | Join a netgame at that address (standard Chocolate Doom, passed through). |
| `-players <n>` | Expected player count for the match. |
| `-skill <n>` | Skill level 1–5. |
| `-warp <n>` \| `<e> <m>` | Start directly on the given map (Doom II) or episode+map (Doom). |
| `-deathmatch` | Deathmatch (else co-op). Set on the *creating* client; joiners inherit it over the net. |
| `-altdeath` | Deathmatch 2.0 (weapons **and** items respawn). |
| `-mustered` | Skip the C waiting room: the JS lobby already gathered every player (deferred connect), so the client drops straight into the game with no splash/beep. |

### Multiplayer — dedicated server

Spawned automatically by the lobby; run by hand only to test.

| Option | Meaning |
|--------|---------|
| `-dedicated` | Run as a headless dedicated server (the match's tic relay; no terminal). Reads `-port`. |
| `-spawnserver [-port <n>] [meta…]` | Daemonize a detached `-dedicated` server and exit, printing `<pid> <port>` on stdout. With no `-port`, a free port is allocated. Extra args are forwarded to the server. |
| `-port <n>` | UDP port for the dedicated/spawned server. |
| `-maxplayers <n>` | Match size the server waits for before starting (2–4). |
| `-bindaddr <addr>` | Local address the server's UDP socket listens on. Default **`127.0.0.1`** (loopback only — same-host clients). Use `0.0.0.0` for all interfaces, or a specific local IP/hostname, for cross-host play. (The lobby derives this from `[net] bind`, defaulting to `[net] advertise`.) |
| `-advertise <addr>` | Address cross-host joiners dial, recorded in the browse registry (the lobby's `[net] advertise`). |
| `-gamesdir <dir>` | Directory where the server writes its registry entry so the lobby's Browse can discover it. (Omitted on the muster path, where the JS lobby owns discovery instead.) |
| `-host <alias>` | Host name recorded in the registry entry (shown in the lobby game list). |
| `-wadset <id>` | WAD-set id recorded in the registry entry. |
| `-gamemode <mode>` | Game mode recorded in the registry entry. |

Clients join with `-connect <host:port>`; the creating client's `-skill` /
`-deathmatch` / `-altdeath` define the match.

---

## DeHackEd patches & WAD merging (mods)

The binary includes Chocolate Doom's **DeHackEd** engine and **WAD merging**, so
DeHackEd/BEX-patched mods and total conversions run — not just plain PWADs:

- **`-deh <patch…>`** loads one or more `.deh`/`.bex` patches (the Thing, Frame,
  Weapon, Ammo, Sound, Cheat, Pointer, and `Text` sections, plus BEX). Multiple
  patches apply in order; a later patch overrides an earlier one.
- **IWAD patches load automatically.** The Freedoom/FreeDM IWADs ship a `DEHACKED`
  lump (their level names, etc.); the door loads it *before* any `-deh`, so a
  command-line patch still overrides it. (HACX and Chex Quest IWADs are handled
  too.)
- **`-merge <wad…>`** merges a PWAD's lumps into the IWAD namespace (the NWT-style
  merge some mods expect), as opposed to `-file`, which appends.

**BEX `[STRINGS]` caveat.** Chocolate Doom only parses the BEX `[STRINGS]` section
(mnemonic string replacement, e.g. `GOTARMOR = …`) when the patch contains the
magic comment line `# *allow-extended-strings*`. The classic `Text` replacement
and every other section need no such comment. Without it, `[STRINGS]` is silently
ignored (vanilla strictness) — a common reason a string change "doesn't apply."

Everything here is **vanilla / limit-removing** only: this is the Chocolate Doom
engine, so Boom/MBF21/ZDoom-only patches and maps won't run.

---

## DOOR32.SYS drop file

DOOR32.SYS is the portable door-info interface written by most BBS software. The
door reads three fields:

| Line (1-based) | Field | Used for |
|----------------|-------|----------|
| 1 | Comm type | `2` = telnet socket (otherwise the socket is not taken from the drop file) |
| 2 | Comm/socket handle | The client socket descriptor (same as `-s`) |
| 9 | Time left, **minutes** | Session time limit (same as `-t`, but in minutes) |

DOOR32.SYS does **not** carry the screen row count. The door auto-detects it (a
live size probe, then terminal.ini if present, else a 25-row default). An
explicit `-s` / `-t` on the command line overrides the drop-file values.

A drop-file path is recognized automatically when its filename is `door32.sys`
(case-insensitive), so a BBS that passes the drop-file path as a bare argument
needs no `-door32` flag.

---

## Terminal sizing & geometry

For the graphics tiers the door needs the terminal's pixel dimensions. It probes
at startup with `ESC[14t` / `ESC[16t` (text-area and cell pixels) and
XTSMGRAPHICS (`ESC[?2;1S`), falling back to a `cols × rows` estimate when the
terminal answers none of them (e.g. xterm with `allowWindowOps` off).

The character grid itself (`rows × cols`) is measured live at startup — the door
parks the cursor in the far corner and reads back a cursor-position report — and
that overrides a stale terminal.ini (whose values a mid-session client resize
never updates). This is a one-time startup measurement; a resize *during* play is
not currently detected.

When the real pixel size is unknown the **sixel** image is capped at DOOM's
native 640×400 rather than upscaled from the estimate — an upscaled guess can
exceed what the terminal will render and produce a blank or partial image. A
front end that knows the real cell size can override the estimate (see the
`cell_width` / `cell_height` settings in the config file) to fill a larger
window.

---

## Per-user storage (`-home`)

With `-home <dir>` the door `chdir`s into that directory before the engine
starts, so all of DOOM's `cwd`-relative files — `default.cfg` (keybindings),
savegames, screenshots — are written there. Point it at a per-user, per-session
path to keep saves and key bindings across sessions without node collisions. The
directory is created if it does not exist, and used verbatim (nothing is
appended). Without `-home`, the current working directory is used as-is.

---

## Fatal errors & diagnostics

**Fatal errors don't pop a dialog.** A fatal engine error (a bad WAD, out of
memory, …) is recorded and the door exits cleanly — it never shows a modal error
box. A door has no reachable desktop: upstream Chocolate Doom's popup would block
the process (and under a Windows service land on a window station nobody can
dismiss), hanging the node while the remote user sees a frozen screen. The error
goes to **stderr**, to the **`-log` file** if configured, and (on Windows) to
`OutputDebugString` (visible in DebugView).

On **\*nix** the BBS captures a door's stderr into its node/system log, so the
error is already there. On **Windows** a native socket door's stderr is *not*
captured by the BBS, so set `syncdoom.ini [debug] log` (or pass `-log`) to get a
durable record there — recommended for any Windows install.

The door also writes two diagnostic lines to **stderr** (and the `-log` file):

**Startup banner** — the running build, detected terminal, and resolved
geometry:

```
build Jun 17 2026 01:40:44 | term=…/terminal.ini (read) | cols=182 rows=53
desc="xterm" cterm=0 | win=0x0 cell=0x0 | image 640x400 @408,0 scale=fit mode=sixel
```

**At-exit telemetry** — what was actually sent and how fast:

```
telemetry: sixel 640x400 | 1240 frames in 95.3s = 13.0 fps | 41203112 bytes
(33228/frame avg) | 422.1 KB/s avg
```

Fields: render tier and emitted image size, frame count / elapsed / frames per
second, total wire bytes, average bytes per frame, and average throughput.

---

## In-game controls

Because a terminal can't send Ctrl/Alt/Shift on their own, the modifier-based
DOOM defaults are remapped to a WASD scheme. A controls reference is available
in-game with **F1**. Keys:

| Key | Action |
|-----|--------|
| Arrow keys | Move (up/down) and **turn** (left/right). |
| Mouse | **Turn** (SyncTERM / xterm-mouse clients). Left button fires. See *Mouse control* below. |
| `W` `S` | Move forward / back. |
| `A` `D` | Strafe left / right. |
| `Space` | Fire. |
| `E` | Use / open door. |
| `R` | Toggle always-run (on by default). |
| `1`–`7` | Select weapon. |
| `T` | Talk / chat (multiplayer); `g`/`i`/`b`/`r` whisper one player by color in 3+ player games. |
| `Tab` | Automap. |
| `F4` | Cycle the render tier / glyph mode. |
| `Ctrl-T` | Cycle the frame pipeline depth (`1 → 2 → … → 8 → auto`), flashing the depth + measured round-trip. Higher depths lift the frame rate on a high-latency (far-away) link toward Doom's 35 fps sim rate (frame rate ≈ depth ÷ round-trip), at the cost of some added input lag; `auto` (the default) adapts to the link. Mainly an A/B tuning aid for remote play. Saved per-user. (Also `[video] frames_in_flight` in `syncdoom.ini`.) |
| `Ctrl-S` | Toggle a live stats overlay (top row): render tier, frame rate, round-trip (current / baseline), and pipeline depth — handy for watching `auto` adapt over a remote link. Session-only. |
| `Ctrl-O` | Toggle mouse steering on/off, flashing the new state. Saved per-user. (Also `[input] mouse` in `syncdoom.ini`.) |
| `Ctrl-U` | Who's online — a brief top-strip overlay of the other BBS nodes and what they're doing. |
| `Ctrl-P` | Page / message another node: shows who's online, then you type a message over the running game (non-blocking — your player just stands still, like `T` chat, so a co-op game never stalls). Prefix a node number (`5 hi` / `5: hi`) to target one node, or leave it blank to message everyone online. `Esc` or a blank message cancels. |
| `F2` / `F3` | Save / load game (written under `-home`); `F6` / `F9` quicksave / quickload. |
| `Esc` | Menu. **Options → Input Feel** tunes the movement graces (TAP/HOLD/TURN) live if it feels too slidy or twitchy; saved per-user. |

The movement/action letters shadow their lowercase keys, so **cheat codes and
save-game names are typed in UPPERCASE** (the shifted form bypasses the binding
and is folded back to lowercase for matching).

### Mouse control

On SyncTERM (and other xterm-mouse-capable clients) the door can turn with the
mouse, which sidesteps the key-repeat lag of the arrow keys entirely — turning is
smoother and more precise. Terminals report the pointer's **absolute** screen
position (no relative deltas, and the pointer can't be re-centered by the host),
so steering works like a **joystick**: the pointer's horizontal offset from
screen-center sets a turn **rate** — hold it left of center to keep turning left,
return it to center to stop. (Leaving the pointer parked off-center relaxes to
neutral after a short idle, so an abandoned pointer doesn't spin forever.)

Buttons map to DOOM's defaults: **left = fire**, right = strafe-modifier (hold and
the horizontal mouse strafes instead of turning), middle = forward. Vertical mouse
and the wheel are unused; forward/back stays on the keyboard.

Enable/disable with `[input] mouse = on|off` (default **on**) or `-mouse on|off`,
or toggle it live in-game with **Ctrl-O**. Mouse turning is suppressed in menus and
while typing chat, and the setting saves per-user. On a terminal without mouse
reporting it's simply inert.

---

## Configuration file

An optional `syncdoom.ini`, read from the directory of the executable, provides
defaults for the video and input options above plus a `[game]` section, and the
networking and WAD-set definitions used by the JavaScript lobby. All keys are
optional; an absent key keeps the auto-detected value or built-in default. The
documented template ships with the lobby as `xtrn/syncdoom/syncdoom.example.ini`
and is copied to `syncdoom.ini` on install; see it for the full key list. Notable
`[game]` keys:

- `quit_effect` = `keep` / `vanish` / `fog` (default **fog**) — what happens to a
  player's marine when they leave a match in progress: stay (vanilla), vanish, or
  a teleport-out puff. Changes lockstep game state, so it must match across players
  in a match (a house setting, not per-user).
- `attract_demos` = `true` / `false` (default **false**) — play the title-screen
  attract demos (the IWAD's `demo1`–`demo4`) when the title sits idle. Off, the
  title page holds instead, so an idle single-player door doesn't stream a demo's
  worth of frames the remote user didn't ask for. Set `true` to restore the
  classic attract loop.
- `splash` — the waiting-room backdrop as an editable external 80×25 "binary text"
  file (default `waiting.bin`, beside the door; edit in PabloDraw/Moebius). Missing
  or wrong-size → the baked-in default.

A player's in-game tweaks — the **Options** sliders, the **FAST TURN** toggle, and
the **Ctrl-T** frame-pacing depth — are written to a **per-user `syncdoom.ini`** in
their `-home` directory, overlaying the matching `[input]`/`[video]` keys from the
house template. **Only settings the player actually changes are written**, so the
sysop's house defaults keep reaching untouched keys. Precedence: built-in → house
`syncdoom.ini` → per-user → CLI flag.

---

## License & credits

DOOM is © id Software; the engine is derived from the GPLv2 DOOM source release
via Chocolate Doom and doomgeneric. This door and its terminal/network back end
are distributed under the same **GNU GPL v2**. See `CREDITS` for attribution.
Freedoom (a free, compatible IWAD) is a convenient royalty-free content set; the
commercial `doom.wad` / `doom2.wad` must be supplied by the user.
