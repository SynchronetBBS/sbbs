# syncmoo1

**Master of Orion 1**, rendered as a Synchronet / SyncTERM BBS *door* (an
external program launched from a BBS session) via the vendored
[1oom](https://sourcecraft.dev/fork1oom/1oom) engine and the shared
`../termgfx` rendering library -- the same architecture as `../syncdoom`,
`../syncduke`, and `../syncconquer`. Unlike those, MoO1 is **single-player,
turn-based, mouse-driven point-and-click** (no multiplayer/net code); the
primary interaction-model reference is `../syncconquer` (an RTS).

See [DESIGN.md](DESIGN.md) for the full design (architecture, data flow,
mouse mapping, milestones) and [PROVENANCE.md](PROVENANCE.md) for exactly
what was vendored and how. See [CLAUDE.md](CLAUDE.md) for the ours-vs-vendored
coding contract before touching any source in this directory.

---

## Status: M1 -- playable vertical slice

This is the first milestone, **not** the finished game. M1 delivers:

- Builds as `syncmoo1` and launches via **DOOR32.SYS** on SyncTERM.
- Renders 1oom's main menu / first screen as **sixel**.
- **Navigable with keyboard + mouse** (cell-resolution cursor mapping).
- Correct terminal enter/probe/leave and a BBS-restoring teardown on exit or
  hangup.

**Deferred to M2+** (per [DESIGN.md §11](DESIGN.md)): audio; the JXL and
polished text-tier render paths; the `sbbs_node` who's-online/paging overlay
(Ctrl-U/Ctrl-P); an MSVC/Windows build + vcpkg; per-user save-sandbox
hardening; and a full-game playtest/balance pass across every MoO1 screen
(especially the dense galaxy map, where cell-resolution clicking is tightest
until SGR-Pixels mode 1016 lands on more terminals).

Don't expect a fully polished play experience yet -- expect a working slice
that proves the rendering, input, and BBS-integration plumbing end to end.

---

## Rendering tiers

`termgfx` gives every tier from the same present path; the ladder is
**JXL -> sixel -> text/block** (half/quadrant/sextant glyphs), auto-selected
from the terminal's startup capability probe.

**M1 targets sixel only.** The text-tier fallback comes along for free from
`termgfx` but isn't a tuned/verified M1 deliverable; JXL support (a
compile-time option requiring `libjxl`) is polish work deferred past M1. A
build without `libjxl` still produces a working `syncmoo1` -- it just serves
sixel/text, same as the other doors in this tree.

---

## Master of Orion 1 data files (LBX) -- you must supply these

1oom is an **engine**, not game content: it requires the original **Master of
Orion 1 v1.3** `.LBX` data files to run, and this door does not (and cannot)
ship them. Master of Orion is © Simtex / MicroProse (see [CREDITS](CREDITS));
sourcing a legally-owned copy of the data is the sysop's / player's
responsibility, out of scope for this code (DESIGN.md §15).

Point the door at wherever those LBX files live on the install with the
`SYNCMOO1_LBX` environment variable:

```sh
SYNCMOO1_LBX=/path/to/moo1/data ./syncmoo1 ...
```

`syncmoo1_config.c`'s `sm_config_apply()` resolves this once at startup
(absolutized via `realpath()` before any per-user `chdir`, so a relative path
resolves against the door's actual launch directory). If `SYNCMOO1_LBX` is
unset, empty, or doesn't resolve, the door falls straight through to 1oom's
own built-in data-directory search (cwd, XDG dirs, `/usr/share/1oom`, etc.) --
this is intentionally a thin pointer, not a reimplementation of that search.
1oom's own `-data <dir>` command-line option works too, if you'd rather pass
it directly.

---

## Building

Out-of-source **CMake** build, same shape as `../syncdoom` / `../syncduke`:

```sh
./build.sh            # Release build -> build/syncmoo1
./build.sh debug       # Debug build
./build.sh clean       # wipe ./build/ first
./build.sh debug clean # combine
```

Equivalently, by hand:

```sh
cmake -B build && cmake --build build -j
```

Building never touches any live install -- it only writes under `./build/`.
Run `./deploy.sh` afterwards to install the freshly-built binary into the
door's live `xtrn` directory (see that script's own header for exactly where
and how; it's a separate, explicit step on purpose, so a sysop can rebuild and
test before pushing a new binary live).

The `CMakeLists.txt` enumerates 1oom's `game/`+`os/`+`ui/` sources (derived
from the vendored `Makefile.am`) plus `hw_sbbs.c` and the `syncmoo1_*.c`
glue, links the shared `../termgfx` library, and absorbs the vendored
engine's legacy-code compiler warnings (`-w -fcommon`) rather than patching
upstream -- see [CLAUDE.md](CLAUDE.md) for that policy. `libjxl` is an
optional dependency (pkg-config / `find_library`, vcpkg on MSVC); its absence
degrades to the sixel/text tiers, it does not fail the build.

---

## Launching / command-line syntax

```
syncmoo1 [door opts] [1oom engine opts]
```

The door consumes its own options and leaves everything else for 1oom's
engine argument parser, so any of 1oom's native switches (e.g. `-data <dir>`)
pass straight through.

A typical BBS invocation (the BBS fills in the DOOR32.SYS drop-file path):

```
syncmoo1 <path>/door32.sys -name "<user>"
```

| Option | Meaning |
|--------|---------|
| `<path>/door32.sys` | A bare path whose filename is `door32.sys` (case-insensitive) is auto-detected as a **DOOR32.SYS** drop file: line 1 comm type (`2`=telnet/socket), line 2 socket handle, line 7 user alias, line 9 minutes left. |
| `-s<fd>` | Client socket descriptor directly (glued form, e.g. `-s7`), when not launching from a drop file. Digit-suffix-only, so it never collides with 1oom's own `-s...` word options (`-sfx`, `-skipintro`, `-savequit`, ...). |
| `-t<seconds>` | Session time limit in seconds; the door exits cleanly (restoring the terminal) when it elapses. Same digit-suffix-only rule as `-s<fd>`. |
| `-name <handle>` | Player name. |
| `-home <dir>` | Per-user sandbox directory: the door `mkpath`s and `chdir`s into it and points 1oom's config/save path at it (`os_set_path_user()`), so saves and settings don't collide across BBS nodes/users. Created if it doesn't exist. |
| `-help`, `--help`, `-?` | Print the door's own option summary and exit (checked before any session resolution, so it wins even given a well-formed drop file). |

DOOR32.SYS does not carry the screen row/pixel geometry; the door probes the
real terminal size and cell dimensions at connect (DESIGN.md §9).

---

## Development environment variables

Useful when developing/testing off a live BBS session:

| Variable | Meaning |
|----------|---------|
| `SYNCMOO1_SOCK` | Dev fallback client socket descriptor, used only when nothing on the command line resolves one (e.g. point it at one end of a `socketpair()`). Without a socket, output falls back to fd 1 (stdout) for a plain dev/tty run. |
| `SYNCMOO1_SIXELOUT=<path>` | Offline sixel-capture mode: every `sm_io_present()` call **overwrites** `<path>` with one self-contained sixel frame (palette always included), instead of writing to the live socket/stdout. Lets a captured frame be decoded independently (e.g. with ImageMagick) with no live terminal or socket involved -- useful for verifying the render path without a real SyncTERM session. |
| `SYNCMOO1_LBX=<dir>` | Shared, read-only MoO1 LBX data directory -- see [above](#master-of-orion-1-data-files-lbx----you-must-supply-these). |

---

## License & credits

Master of Orion is © Simtex / MicroProse. The 1oom engine is vendored under
its own **GPLv2** license; this door's own glue code is likewise GPLv2. See
[CREDITS](CREDITS) for full attribution and [PROVENANCE.md](PROVENANCE.md) for
the vendored engine's exact commit/date/license verification.
