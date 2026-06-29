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

v1 is **single-player, shareware Episode 1**, 64-bit. No multiplayer / no JS
lobby / no audio yet.

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
menu, or **F1** during play). Terminals that speak the **kitty keyboard protocol**
are the exception — see [Kitty keyboard terminals](#kitty-keyboard-terminals) below:

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

### Kitty keyboard terminals

On a terminal that implements the [kitty keyboard
protocol](https://sw.kovidgoyal.net/kitty/keyboard-protocol/) — e.g. **Contour** —
SyncDuke negotiates it automatically at connect (no setting) and gets **real
key-up** events. That means **hold-to-move / release-to-stop** instead of the
key-repeat heuristic, and turning uses Duke's **native** acceleration ramp, so it
feels like the original. The Ctrl-S stats strip shows **`kbd:kitty`** when it's
active, and the tap/hold/turn/fast-turn knobs grey out (they only matter without
key-up). SyncTERM and other terminals are unaffected — they keep the key-repeat
scheme.

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
1.3D / Plutonium 1.4 / Atomic 1.5 GRPs you own also work.

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

---

## Status & limits (v1)

- **Plays** end-to-end: boot → menu → new game → Episode 1, with rendering,
  movement / turn / strafe / fire / weapon-switch / jump / crouch / use / look,
  and **save / load**, all over the terminal.
- **Single-player only** — no multiplayer, lobby, or audio.
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
