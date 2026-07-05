# SyncDuke

**Duke Nukem 3D** as a BBS *door* (external program), rendering the game to the
user's terminal over the telnet/SSH session — no native client, no X11, no SDL.

SyncDuke is a [Chocolate Duke3D](https://github.com/fabiensanglard/chocolate_duke3D)
port whose SDL video/input/timer back end is replaced by a headless shim
(`syncduke_plat.c` + friends) that captures the Build engine's 8-bit framebuffer,
encodes it for the terminal, and writes it to the BBS-supplied TCP socket,
feeding terminal keystrokes back as Duke key events. It runs on any **DOOR32.SYS**
BBS. **SyncTERM is the recommended client** (it gets the best graphics tiers), but
the door auto-detects what the terminal supports and falls back gracefully — even
a plain block-character terminal (e.g. Windows conhost) renders the game.

The terminal-graphics engine (encoders, transport, capability probing, pacing,
who's-online/paging) lives in a shared library, **[termgfx](../termgfx/)**, that
SyncDuke and its sibling [SyncDOOM](../syncdoom/) both build on.

The core is **shareware Episode 1**, 64-bit. Beyond single-player it now has
**co-op and dukematch multiplayer** through a **JS lobby** waiting room, and
**full audio** — digital sound effects plus OPL/MIDI music — which requires
**SyncTERM v1.10 or later** (its audio channel); on any other terminal the game
simply plays silent.

---

## Graphics tiers

SyncDuke probes the terminal at connect and picks the best tier automatically;
**F4** cycles through the ones the terminal supports (a brief centered label
names the new tier):

| Tier | Needs | Notes |
|------|-------|-------|
| **JPEG XL** | SyncTERM (answers the `Q;JXL` probe) | Smallest frames; the default on a modern SyncTERM. |
| **Sixel** | any sixel-capable terminal (SyncTERM, xterm, mlterm, foot, WezTerm, recent Windows Terminal) | Full-color DECSIXEL. |
| **Text — half-block / blocks+shades** | any ANSI terminal | CP437 block-character fallback; the auto choice when the terminal has neither sixel nor JXL. Lower-res but works everywhere. |

A terminal that advertises neither sixel nor JXL (e.g. conhost) auto-selects the
text tier instead of showing a blank screen, and F4 only offers tiers that
terminal can actually render.

Frame delivery is paced to the terminal's *render* rate (not just link bandwidth)
via a DSR-ack pipeline, with an **auto depth** controller (AIMD) on by default —
it settles the pipeline at the link's sustainable depth from the measured
round-trip time.

---

## Controls

Most terminals send key-*down* only (no mouse, no key-up), so the scheme mirrors
[SyncDOOM](../syncdoom/) (see the in-game **Controls Help** screen — on the main
menu, or **F1** during play). Terminals that provide **real key-up** — **SyncTERM
v1.10+** (via its evdev key reports) and terminals speaking the **kitty keyboard
protocol** — are the exception; see
[Native key-up terminals](#native-key-up-terminals) below:

| Key | Action | | Key | Action |
|-----|--------|-|-----|--------|
| **W / S** | move forward / back | | **Space** | **fire** |
| **A / D** | strafe left / right | | **E** | open / use |
| **← / →** | turn left / right | | **Q** | jump |
| **↑ / ↓** | move forward / back | | **Z** | crouch (toggle) |
| **PgUp / PgDn** | look up / down | | **R** | toggle always-run |
| **Home / End** | center view *(menus: first / last item)* | | **1–0** | select weapon |
| **Tab** | automap | | **Enter / Esc** | menu select / back |

In menus and while typing (e.g. a save-game name) the action layer is off, so
letters and Space arrive literally and the arrows/Enter/Esc navigate.

**Optional mouse steering** — toggle with **Ctrl-O** (or in *Options → Setup
Controls*); when on, the terminal's mouse turns/aims and a click fires. Setup
Controls also tunes key tap/hold/turn timing and fast-turn — *byte-path knobs that
compensate for the missing key-up; they grey out on a kitty terminal, which doesn't
need them.*

**Door hotkeys** (intercepted by the door; never reach Duke):

| Key | Action |
|-----|--------|
| **F4** | cycle graphics tier (jxl / sixel / half-block / blocks+shades) |
| **F1** | Controls Help screen |
| **Ctrl-T** | cycle frame-pipeline depth (auto → 1…16 → auto) |
| **Ctrl-S** | toggle the live stats overlay (fps / KB-s / lag / depth) |
| **Ctrl-O** | toggle mouse steering |
| **Ctrl-U** | who's online — brief overlay of the other BBS nodes |
| **Ctrl-P** | page / message another node: shows who's online, then type a message over the running game (non-blocking, MP-safe — your Duke stands still, like `T` chat). Prefix a node # (`5 hi` / `5: hi`) to target one, or leave blank to message all. `Esc`/blank cancels. |

### Native key-up terminals

Two kinds of terminal give SyncDuke **real key-up** events, negotiated automatically
at connect (no setting, and mutually exclusive):

- **SyncTERM v1.10 or later** — via its **evdev** key reports.
- Terminals implementing the [kitty keyboard
  protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) — e.g. **Contour**.

With real key-up, SyncDuke drops the key-repeat heuristic for authentic input:
**hold-to-move / release-to-stop**, **momentary crouch** (hold **Z** to duck, release
to stand — not the sticky toggle basic terminals use), and turning uses Duke's
**native** acceleration ramp — so it feels like the original. The Ctrl-S stats strip
shows **`evdev/`** or **`kitty/`** (with a `nat`/`syn` turn-model suffix) when a native
path is active, and the tap/hold/turn/fast-turn knobs grey out (they only matter
without key-up). Plain key-down-only terminals keep the key-repeat scheme — and the
sticky-toggle crouch.

---

## Building

Plain C; links one library, **xpdev** (cross-platform sockets + `ini_file`), and
the in-tree **termgfx** library — both built as CMake sub-targets, so there is no
separate install step. The **JPEG XL** tier additionally uses system **libjxl**
(via `pkg-config`); if it isn't found the door still builds with the sixel and
text tiers (a warning is printed at configure time).

```sh
./build.sh            # release build -> build/syncduke (does NOT deploy)
./build.sh debug      # debug build
./build.sh clean      # wipe build tree first
./deploy.sh           # install build/syncduke into the door's xtrn dir(s)
```

Building and deploying are separate steps: `build.sh` only produces
`build/syncduke`, so you can rebuild and test without disturbing a running BBS.
`deploy.sh` copies that binary into the door's `xtrn/syncduke/` dir — and, on a
copy-style install, into the live install located via `$SBBSCTRL` (or
`SYNCDUKE_DEST=<dir>`). On a SYMLINK=1 install the live door already points at
`build/syncduke`, so `build.sh` alone updates it and `deploy.sh` is a no-op.

Or by hand:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

On **Windows** (Visual Studio 2022 / MSVC), use `build.bat` — it configures and
builds the Win32 (x86) Release tree in `build-msvc/`, leaving `syncduke.exe`
under `build-msvc\Release\`. Run `deploy.bat` afterwards to install it into
`..\..\..\xtrn\syncduke\` (build and deploy are separate, same as the *nix
scripts):

```bat
build.bat            :: Win32 release (JPEG-XL via vcpkg if installed)
build.bat x64        :: 64-bit release (best-effort; see the 32-bit note below)
build.bat clean      :: wipe the build tree first
deploy.bat           :: install build-msvc\Release\syncduke.exe into xtrn\syncduke\
```

The JPEG-XL tier links a static **libjxl** built for MSVC; install it with
`vcpkg install libjxl:x86-windows-static-md` (classic mode) and `build.bat`
picks up `C:\vcpkg\installed\x86-windows-static-md` automatically (see
[vcpkg.json](vcpkg.json)). Without it the door still builds with the sixel/text
tiers. The door socket from DOOR32.SYS is a Winsock `SOCKET` on Windows; the
shim's I/O selects `send`/`recv`/`ioctlsocket` vs the *nix `read`/`write`/`fcntl`
at compile time (`_WIN32`).

The vendored Build engine assumes 32-bit pointers in spots; the few that matter
at 64-bit are patched in place and documented in [SEAM.md](SEAM.md). The engine
sources keep their upstream style; only our shim files follow the project's
uncrustify config.

---

## Installing & running

SyncDuke ships **no game data.** You supply a base `DUKE3D.GRP` — the shareware
GRP (11,035,779 bytes, "SHAREWARE 1.3D") is freely redistributable; the Full
1.3D / Plutonium 1.4 / Atomic 1.5 GRPs you own also work — and one of those
full GRPs is **required** for third-party user maps and add-on GRPs (see
[Third-party user maps & add-on GRPs](#third-party-user-maps--add-on-grps)).

1. `./build.sh` then `./deploy.sh` — builds `syncduke` and installs it into
   `xtrn/syncduke/`.
2. Put `DUKE3D.GRP` either beside the binary in `xtrn/syncduke/`, or anywhere and
   set its directory in `syncduke.ini` (`[grp] dir = /path/to/grp`).
3. Register the door:
   ```sh
   jsexec install-xtrn ../xtrn/syncduke
   ```
   This adds the `[prog:SYNCDUKE]` entry to `ctrl/xtrn.ini` and seeds
   `syncduke.ini` from `syncduke.example.ini` (it prompts before overwriting an
   existing `syncduke.ini`).
4. Connect with the BBS and run the door.

The door is launched as `syncduke%. %f -home %juser/%4/duke/`:

- `%f` is the **DOOR32.SYS** drop file, from which SyncDuke reads the client
  socket and the session time limit.
- `-home …` gives **each user their own** writable directory for Duke's config
  (`duke3d.cfg`) and savegames — `data/user/<usernum>/duke/`, created on first
  run. Without it, config/saves would be shared in the GRP directory and collide
  across nodes. (The base GRP stays read-only and shared.)

### Command-line arguments

The DOOR32.SYS path (`%f`) or `-s<fd>` supplies the client socket; everything else
is optional. Run `syncduke -help` (also `--help`, `-?`, `/?`, or with no
arguments) to print this list.

**Session / config:**

| Argument | Purpose |
|----------|---------|
| `<path>/door32.sys` | DOOR32.SYS drop file (Synchronet's `%f`): client socket + session time limit + alias. |
| `-s<fd>` | Client socket file descriptor directly (the lobby's path — no drop file). |
| `-t<seconds>` | Session time limit; the door exits when it elapses (also carried by the drop file). |
| `-name <handle>` | Player name (who's-online status and multiplayer). Given as a flag+value so a `/`-leading alias isn't misread as an engine option. |
| `-home <dir>` | Per-user writable dir for `duke3d.cfg` + savegames (see above). |
| `-grpdir <dir>` | Where to find `DUKE3D.GRP` (else beside the binary; also `syncduke.ini [grp] dir`). |
| `-charset utf8\|cp437\|auto` | Client character set for the block/text tiers. `auto` (default) detects it from Synchronet's `<node>/terminal.ini`; pass `utf8` or `cp437` explicitly on **non-Synchronet BBSes** (DOOR32.SYS installs with no `terminal.ini`), where auto-detection can't work. `utf8` makes the block tiers emit native Unicode and adds the higher-res **quadrant/sextant** tiers to the F4 cycle. Overrides `syncduke.ini [video] charset`. |
| `-log <path>` | Write a door debug log to `<path>` (else `syncduke.ini [debug] log`, else off). |
| `-eventlog <path>` | Append game events (level start, deaths/frags) as JSONL to `<path>` for the lobby's activity feed. |
| `-map <file>` | Play a third-party **user map** (`.map`) instead of the stock levels (single-player and net games); **requires a non-shareware base GRP**. The engine's `/g<grp>` / `/x<con>` options load **add-on GRPs** (official expansions) on top of the base GRP. See [Third-party user maps & add-on GRPs](#third-party-user-maps--add-on-grps). |

**Multiplayer** (co-op and dukematch) — the lobby sets these itself, so you
don't normally pass them by hand:

| Argument | Purpose |
|----------|---------|
| `-netrole master\|join` | This peer's role: `master` (hosts/listens) or `join` (dials the master). |
| `-netport <n>` | The master's listen port (role `master`). |
| `-netpeer <host:port>` | The address the joiner dials (role `join`). |

**Engine** — the door strips its own options above and hands the rest to the
Duke3D engine, which takes DOS-style slash options:

| Argument | Purpose |
|----------|---------|
| `/v<n>` | Volume / episode number. |
| `/l<n>` | Level number. |
| `/c1` \| `/c2` \| `/c3` | Multiplayer mode: dukematch (spawn) \| co-op \| dukematch (no item respawn). |
| `/m` | Monsters off. |
| `/t` | Respawn items. |

### Third-party user maps & add-on GRPs

Sysops can offer third-party content — user maps (`.map` files) and **add-on
GRPs** (official expansions like *Duke It Out In D.C.*) — as choices in the
lobby's picker.

> **⚠ Base-GRP requirement:** third-party content needs a **full (registered)
> base `DUKE3D.GRP`** — Full 1.3D, Plutonium 1.4, or Atomic 1.5 — **not the
> shareware GRP**. The engine detects its edition from the base GRP's CRC at
> startup, and the shareware edition has no user-map path at all: a `-map`
> launch dies with *"Internal Map … not found in Shareware grp pack!"* and the
> user is dropped back to the BBS. (Independent of that, most user maps place
> registered-episode art/enemies/music, and the official add-on GRPs are Atomic
> expansions.) The lobby does **not** check the GRP edition, so don't configure
> `[map:*]` entries on a shareware-GRP install — they'd be offered and then
> fail to launch.

Setup:

1. Put the files somewhere the door can read — conventionally under the door's
   own directory, e.g. `xtrn/syncduke/maps/Roch.map` and
   `xtrn/syncduke/addons/dukedc.grp`. Relative paths in `syncduke.ini` resolve
   against the door's directory; absolute paths work too.
2. Add one `[map:<Name>]` section per choice to `syncduke.ini` — `<Name>` is
   the label players see in the picker:

   ```ini
   [map:Roch]
   file = maps/Roch.map        ; a user map, played via the engine's usermap slot

   [map:Duke It Out In D.C.]
   grp = addons/dukedc.grp     ; add-on GRP, overlaid on the base GRP (/g)
   con = addons/dukedc.con     ; the add-on's CON script, if it ships one (/x)
   ```

   `file`, `grp`, and `con` combine freely in one entry (e.g. an add-on that
   also ships a map). No recycle is needed — the lobby re-reads `syncduke.ini`
   each time it runs.
3. When any entries are configured, **S**olo and **C**reate first ask *what to
   play* — stock Duke is always choice #1. With none configured, no prompt
   appears. Multiplayer games carry the choice in their registry entry, so a
   joiner launches with matching content automatically (both peers read the
   same sysop-installed files; nothing is installed per user).

---

## Status & limits (v1)

- **Plays** end-to-end: boot → menu → new game → Episode 1, with rendering,
  movement / turn / strafe / fire / weapon-switch / jump / crouch / use / look,
  and **save / load**, all over the terminal.
- **Multiplayer** — co-op and dukematch through the JS lobby's waiting room
  (`-netrole`/`-netport`/`-netpeer`), alongside single-player.
- **Audio** — full sound: digital SFX and OPL/MIDI music (with 3-D positional
  panning) over SyncTERM's audio channel. **Requires SyncTERM v1.10+**; on a
  terminal without the audio channel the game is fully playable, just silent.
- Save files are **64-bit-only** (the in-engine save/relocation code was widened
  to `intptr_t`); they are not interchangeable with a 32-bit Duke build.
- **Cross-platform build** — Linux/Unix (GCC/Clang, `build.sh`) and Windows
  (MSVC, `build.bat`). The shim's terminal/door I/O picks `send`/`recv`/
  `ioctlsocket` on Windows vs `read`/`write`/`fcntl` on *nix at compile time.

See [DESIGN.md](DESIGN.md), [PLAN.md](PLAN.md), and [SEAM.md](SEAM.md) for the
spec, task plan, and the engine/platform seam (including the 64-bit patch list).

---

## License

The vendored engine is **Chocolate Duke3D** (Duke Nukem 3D source © 3D Realms +
the Build engine © Ken Silverman), GPLv2 — see
[CHOCOLATE_DUKE3D_README.md](CHOCOLATE_DUKE3D_README.md). The shared text-tier
renderer in [termgfx](../termgfx/) is adapted from
[ludocode/doom-cli](https://github.com/ludocode/doom-cli) (GPLv2). Our shim code
follows the Synchronet project license.
