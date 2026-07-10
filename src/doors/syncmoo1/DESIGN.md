# syncmoo1 — Master of Orion 1 as a Synchronet / SyncTERM door

**Status:** design approved 2026-07-08, pre-implementation.
**Binary:** `syncmoo1`. **Engine:** [1oom](https://sourcecraft.dev/fork1oom/1oom)
(the `fork1oom/1oom` line; GitHub is a mirror). **Canvas:** native 320×200,
8-bit palettized.

syncmoo1 renders 1oom's Master of Orion 1 to a BBS user's terminal via the
shared `termgfx` library (`../termgfx`), exactly as syncdoom / syncduke /
syncconquer render Doom / Duke3D / Red Alert. It is a single-player,
turn-based, **mouse-driven point-and-click** door — the same interaction model
as syncconquer (an RTS), which is the primary pattern source here.

## 1. Goals / non-goals

**Goals**
- Play MoO1 over SyncTERM (and degrade to any sixel/ANSI terminal).
- Reuse `termgfx` unchanged for encoding, transport, capability probing,
  pacing, geometry, audio APC, and `sbbs_node`.
- Keep the vendored 1oom tree frozen and unedited; all door logic lives in our
  own glue files plus a new 1oom `hw` backend.

**Non-goals**
- No multiplayer (MoO1 is single-player; skip the `mp_*`/`net_*` machinery the
  FPS doors carry).
- No changes to `termgfx` unless a real shared need arises (check the other
  consumers first, per `termgfx/CLAUDE.md`).
- No modification of upstream 1oom sources (absorb legacy-code warnings in the
  build, never patch the engine — same policy as the reference doors).

## 2. Approach

**Chosen: a new 1oom `hw` backend (`hw/sbbs`) whose implementation *is* our
door glue, compiled together with 1oom's engine sources and `termgfx` into one
`syncmoo1` binary by a door-owned CMakeLists.**

1oom already separates `game/` (logic), `os/`, `ui/`, and `hw/` (pluggable
backends: `sdl`, `alleg`, `nop`). We add a peer backend that implements the
`hw_video_*`, `hw_event_*`, `hw_mouse_*`, `hw_audio_*`, and timing hooks by
delegating to `termgfx` and our modules. This is a *legitimate backend*, not a
platform-layer amputation — cleaner than Doom's `DG_*` hooks or Duke's
`display.c` replacement, because 1oom gives us the seam natively.

**Rejected — keep 1oom's autotools, link termgfx as an external lib.**
Diverges from every reference door (all CMake), creates autotools↔CMake build
friction, and produces a poor Windows/MSVC story. The only cost of the CMake
route is enumerating 1oom's source list in CMake (as syncduke does for Duke)
and reproducing 1oom's autotools-generated `config.h` as a static header —
both mechanical.

**Rejected — CMake that shells out to 1oom's autotools to build a lib.**
Fragile cross-build-system coupling, worst Windows story.

## 3. Key simplification vs. the FPS doors

**1oom is event-driven, not busy-spin.** Doom and Duke spin a real-time render
loop and must throttle with a frame-rate ceiling (`CAP_FPS`) to avoid flooding
the wire. 1oom renders on change and blocks on input when idle, so syncmoo1
**drops the FPS ceiling entirely** and keeps only:

- **memcmp de-dupe** of the native framebuffer + palette + geometry + tier
  signature — identical frames skip encoding, so static menus cost ~zero
  bandwidth; and
- **DSR-ACK backpressure** (`ESC[6n` after each frame, `ESC[r;cR` acks one
  in-flight frame) via the shared AIMD controller in `termgfx/pace.c`.

## 4. Architecture & file layout

```
src/doors/syncmoo1/
  1oom/                 vendored frozen fork (game/ os/ ui/ hw/ …) — never edited
  hw_sbbs.c             new 1oom hw backend: hw_video_*, hw_event_handle,
                        hw_mouse_set_xy, hw_audio_*, hw_get_time_us; thin —
                        delegates to the modules below
  syncmoo1_io.c/.h      present path, tier selection, sixel/jxl/text emit,
                        DSR pacing, out-buffer, term enter/probe/leave
  syncmoo1_input.c/.h   socket read loop, ESC/CSI/APC state machine,
                        key + MOUSE decode, capability-probe reply parsing
  syncmoo1_door.c/.h    DOOR32.SYS / -s<fd> dropfile, splash, arg-sanitize,
                        hangup
  syncmoo1_config.c/.h  .ini parse, -home chdir, charset, MoO1 LBX data-file
                        path resolution
  syncmoo1_node.c/.h    sbbs_node who's-online / paging overlay (deferred > M1)
  syncmoo1_plat.c/.h    THE platform seam: monotonic clock, sleep, non-blocking
                        descriptor I/O, Winsock bring-up, console-less stderr
                        capture -- all over xpdev. The only TU that includes
                        <winsock2.h>/<windows.h>
  syncmoo1_os_win32.c   1oom `os` backend for Windows (ours, not 1oom's --
                        see PROVENANCE.md "Deliberate non-patches")
  compat/               MSVC-only shims (strings.h, unistd.h, msvc_compat.h)
                        that let the vendored 1oom tree compile unedited
  syncmoo1.h            single cross-module contract; each function carries a
                        provenance comment (which .c provides it, why)
  CMakeLists.txt  build.sh  deploy.sh  build.bat  deploy.bat
  .gitignore  CLAUDE.md  README.md  CREDITS
  tests/                pure-function unit tests (keymap, mouse mapping)
```

**Per-file responsibility split** follows syncduke (cleaner than syncdoom's
single 211 KB file). Engine-header includes are quarantined to the smallest
possible surface so the rest of the glue stays engine-agnostic and unit-
testable.

## 5. Data flow

- **Render:** 1oom draws into `hw_video_get_buf()` (indexed 320×200) with the
  palette set via `hw_video_set_palette` (RGB888, re-emitted only on real
  change). `hw_video_draw_buf()` → `syncmoo1_present(idx, pal)` → de-dupe →
  `sixel_encode_aspect(..., pan=2, pad=2, ...)` (terminal upscales 320×200 →
  640×400, half the bytes) → staged `out_put` → non-blocking flush → `ESC[6n`
  pace.
- **Input:** `syncmoo1_input_pump` reads the socket → byte state machine →
  keys into 1oom's `kbd` event path via `hw_event_handle`; mouse via the
  cell-center mapping (§6) → `hw_mouse_set_xy`.
- **Audio (deferred to M2):** 1oom `hw_audio_*` → `termgfx` `audio_mgr` →
  audio APC (256 patch slots, 14 mix channels; sampled SFX/music).

## 6. Mouse & cursor

Copy syncconquer's `mouse_event` (`door/door_input.c:285-385`) as the template:

1. **SGR decode** — `ESC[<b;col;row {M,m}` (xterm mode 1006). Auto-detect
   pixel mode: any coord exceeding the text grid latches SGR-Pixels.
2. **Reported coord → canvas pixel** — cell mode:
   `px = (col-1)*cw + cw/2`, `py = (row-1)*ch + ch/2`, using the *real probed*
   cell size (not an assumed 8×16). Mapping to the **cell center** is the
   answer to cell-coarseness. Pixel mode (DEC 1016): `px = col-1` directly.
3. **Clamp to the shared aspect-fit image rect, then rescale to game coords**
   (`0..319 × 0..199`), reusing the *same* geometry the present path used to
   draw the frame so the cursor can never drift from the pixels.

Probe `ESC[?1016h` + DECRQM `ESC[?1016$p` at startup so the filed **SGR-Pixels
feature request** (mode 1016) lights up per-pixel precision when the terminal
supports it; SyncTERM falls back to cell-center today.

**Cursor rendering:** prefer letting 1oom blit its own native MoO1 cursor into
the framebuffer (game-native look, precise visual; position steps by a cell
until 1016 lands) rather than syncconquer's terminal-drawn pointer. To be
confirmed during planning by checking 1oom's `hw` cursor expectations. Note:
1oom drawing the cursor means mouse motion dirties the framebuffer (expected —
motion should repaint; idle still de-dupes to zero).

## 7. Build integration (CMake-wrap)

- `project(syncmoo1 C CXX)` — CXX because `termgfx` pulls in C++ libADLMIDI;
  set `LINKER_LANGUAGE CXX` so libstdc++ links last.
- `add_executable(syncmoo1 …)` listing 1oom's `game/`+`os/`+`ui/` sources +
  our `hw_sbbs.c` + `syncmoo1_*.c`. Exclude 1oom's `hw/sdl`, `hw/alleg`,
  `hw/nop`.
- Reproduce 1oom's autotools-generated `config.h` as a checked-in static
  header (feature defines the engine expects).
- `add_subdirectory(../termgfx termgfx)` + `target_link_libraries(syncmoo1
  termgfx)`; termgfx's PUBLIC include dir exposes `sixel.h`/`jxl.h`/`apc.h`/
  `caps.h`/`term.h`/`pace.h`/`sbbs_node.h`.
- **libjxl** (`WITH_JXL`, optional): pkg-config → `find_library` fallback (vcpkg
  on MSVC); define on both `syncmoo1` and `termgfx`, else `termgfx/jxl.c` stubs
  to return 0 and we degrade to sixel.
- **libsndfile / libADLMIDI**: reached transitively via termgfx.
- **xpdev**: linked for `sockwrap` / `ini_file` (crypto + audio backends pinned
  off), guarded so it isn't re-added inside a larger tree.
- Git build info via the shared `synchronet_gitinfo()` macro → `git_hash.h`.
- No CMake `install()`; deployment is a separate `deploy.sh` that copies the
  built binary into the live `xtrn` dir. Build never touches the live install.

## 8. BBS integration

- **Dropfile:** `read_door32()` parses DOOR32.SYS — comm type, socket handle,
  alias, minutes-left. A bare `*door32.sys` arg self-triggers; `-s<fd>` /
  `-t<sec>` / `-name` override. A pre-`main()` constructor grabs argv, resolves
  the socket, paints an instant splash, and `sanitize_cmdline` strips door args
  before 1oom's own parser sees them (hard-won lesson from syncduke). Whether
  the constructor dance is needed depends on how reachable/patchable 1oom's
  `main()` is — resolve in planning; the arg-sanitize lesson applies regardless.
- **Socket:** non-blocking, `TCP_NODELAY`, `SIGPIPE` ignored; plain fd on *nix,
  Winsock `SOCKET` on Windows — the split is confined to `syncmoo1_plat.c`,
  which builds on xpdev's `sockwrap.h` (it normalizes `WSAGetLastError()` onto
  the POSIX `errno` values, so one test classifies a failed transfer on both).
  Same descriptor for output and input. Off a BBS it falls back to stdout on
  *nix; on Windows there is no such fallback (fd 1 is not a `SOCKET`), so a dev
  run there uses `SYNCMOO1_SIXELOUT` capture mode.
- **sbbs_node** (deferred past M1): who's-online (Ctrl-U), NODE_EXT status,
  inter-node paging (Ctrl-P), rendered as a cross-tier overlay from the
  main-loop tick — never from the input path.
- **Per-user sandbox:** `-home <dir>` → `chdir` so config/saves land per player;
  MoO1 data-file (LBX) path is a shared per-install location resolved by
  `syncmoo1_config`.

## 9. Terminal setup / probing / teardown

- **Enter** (once, on first present): `termgfx_term_enter` (clear/home, hide
  cursor, no-autowrap, DECSDM `?80l`), optional DECSSDT status-line hide to
  reclaim the bottom row for a true 640×400, then the probe burst:
  `termgfx_term_probe` (`ESC[14t`/`[16t` canvas + cursor-extreme/DSR fallback),
  `ESC[c` + `ESC[<c` (DA1 + SyncTERM CTDA → sixel/SyncTERM/evdev detect),
  `termgfx_query_jxl`, kitty `ESC[?u`, mouse `?1003h`/`?1006h`/`?1016h`+DECRQM,
  `termgfx_audio_query`.
- **Probe grace** (~500 ms) holds frame 1 until the DA reply lands so the
  auto-selected tier is right immediately (never blast sixel at a text-only
  client).
- **Teardown** via `atexit`: stop audio, drain the staged flush, disable
  mouse/kitty/evdev/sixel-scroll modes, `termgfx_term_leave` (restore
  `?80h`/`?7h`/`?25h`). A hangup uses `_exit()` (dead socket, skip atexit); a
  clean quit runs the restore with blocking writes so it isn't dropped.

## 10. Tiers

`termgfx` gives all tiers from the same present path. Ladder:
**JXL → sixel → text/block (half/quadrant/sextant)**, auto-selected from probe
replies, F-key to cycle. M1 targets **sixel-first**; text fallback comes along
for free; JXL is polish (M2+).

## 11. Milestones

**M1 — playable vertical slice (the approved first deliverable).**
Builds as `syncmoo1`; launches via DOOR32.SYS on SyncTERM; renders 1oom's main
menu / first screen as **sixel**; **navigable with keyboard + cell-mouse**;
correct term enter/probe/leave and BBS-restoring teardown.

**Deferred (M2+):** audio; JXL + text-tier polish; `sbbs_node` overlay
(Ctrl-U/Ctrl-P); per-user save sandbox hardening;
full-game playtest and balance of mouse vs. keyboard across all MoO1 screens
(esp. the dense galaxy map, where cell-resolution clicking is tightest until
mode 1016).

## 12. Testing

- **Unit:** pure-function tests for the keymap and the mouse cell→game mapping
  (standalone, no engine), mirroring syncduke's `tests/`.
- **End-to-end:** manual SyncTERM session against the live install — render the
  menu, move/click the mouse, verify cursor tracks the pixels and selections
  fire; confirm clean teardown leaves the BBS prompt intact.

## 13. Provenance / vendoring policy

- 1oom vendored as a **copied, frozen snapshot** (not a submodule) of
  `fork1oom/1oom`; record upstream ref + date + license (GPLv2+) and any local
  build-shim additions in a `PROVENANCE.md` (as syncconquer does). Never edit
  or reformat the vendored tree; house style (`../../uncrustify.cfg`) applies to
  **our** files only.
- `CLAUDE.md` in the door dir documents the ours-vs-vendored contract and the
  "never patch the engine, absorb sins in the build" rule.

## 14. Open questions to resolve in planning

1. Exact 1oom source-file list for CMake, and the minimal `config.h` the engine
   needs (derive from `configure.ac`/`Makefile.am`).
2. Does 1oom's `hw` layer draw its own cursor, or expect the backend to? (§6)
3. Is 1oom's `main()` reachable without the pre-`main()` constructor dance, or
   do we intercept argv the way syncduke does? (§8)
4. Which 1oom `hw_*` hooks are mandatory vs. optional (model on `hw/nop`), and
   how input events are injected into 1oom's queue from `hw_event_handle`.

## 15. Asset caveat

1oom requires the original MoO1 data files (LBX) to run — same licensing
footnote noted from the outset. `syncmoo1_config` resolves a shared per-install
path; sourcing the files is out of scope for the code.
