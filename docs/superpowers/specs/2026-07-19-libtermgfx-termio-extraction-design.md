# libtermgfx terminal-I/O extraction — design

**Date:** 2026-07-19
**Status:** design (approved direction; pending spec review)
**One-liner:** Lift syncscumm's `sst_io.c` terminal-I/O engine into a shared
libtermgfx module (`termgfx_termio`) with a clean door-facing API, refactor
syncscumm onto it behavior-identically, so a second door (`syncrpg`, EasyRPG)
can reuse the proven render/input/socket pipeline.

## Goal

One shared, tested terminal-I/O engine — canvas probing, aspect-preserving
scale, sixel/JXL tier selection + pacing, keyboard/mouse decode, DOOR32/socket
I/O — that any graphical libtermgfx door drives by handing it native-resolution
frames and reading back input events. syncscumm is refactored onto it with **no
user-visible change**; syncrpg (a later sub-project) is the second consumer.

This is sub-project 1 of two. Sub-project 2 (build syncrpg) has its own spec and
depends on this one landing first.

## Background / motivation

- The `syncrpg` door (EasyRPG Player, RPG Maker 2000/2003) was scouted feasible:
  clean `BaseUi` backend abstraction, builds on this host, renders a 320×240
  game headlessly. It needs exactly the render/input/scale/canvas machinery
  syncscumm already has.
- That machinery is `src/doors/syncscumm/door/sst_io.c` (~3000 lines). Carving
  it into libtermgfx has been the standing "next" item for the door library; a
  second consumer is the point at which a shared library earns its keep.
- **Already shared in libtermgfx (out of scope here):** the PCM audio stream
  (`termgfx_stream_*`, used by syncretro + syncscumm), sixel and JPEG-XL encode,
  `termgfx_sbbs_node`, the audio caching/music helpers, and the kitty/evdev
  key-mode negotiation (`termgfx/keymode.h`). This extraction does NOT touch
  those; it moves the piece that *drives* them.

## Scope

### Moves into the shared module (`termgfx_termio`)

Everything in `sst_io.c` that is game/engine-agnostic:

- **Render pipeline:** terminal-caps probe (DA1/CTDA, CPR, XTVERSION,
  XTSMGRAPHICS, JXL query), canvas-geometry resolution and the ceiling policy
  (sysop `sixel_max` > XTSMGRAPHICS > exact-canvas-unless-xterm > SAFE_MAX),
  tier selection (JXL / sixel / no-gfx), aspect-preserving scale of a
  native-resolution frame to the canvas, dirty-rect vs full-frame present,
  pacing/backpressure, the deferred-present static-screen retry, the Ctrl-S
  stats bar.
- **Input decode:** kitty CSI-u, SyncTERM evdev, and legacy byte paths →
  `SST_KEY_*` events + SGR mouse (DECSET 1016 pixel / 1006 cell) → a single
  SPSC event FIFO; the reserved door hotkeys (Ctrl-S stats, Ctrl-Q/Ctrl-C quit,
  the configurable Ctrl-`<letter>` menu key) that are consumed before the FIFO.
- **Transport:** DOOR32.SYS drop-file parse, the client socket / fd resolution,
  raw read/write, hangup detection, the trace/wirecap touch-file diagnostics,
  the stderr-capture gate.

The module keeps its current **single-instance, per-process** structure (file
static state). A door process serves one terminal session, so this is correct
and keeps the extraction a *pure move*, not a redesign. (A future opaque-context
refactor is possible but explicitly NOT required for behavior identity.)

### Stays in the door (syncscumm / syncrpg)

- Reading the door's own `syncscumm.ini` (`[audio]`, subtitles, `menu_key`) and
  calling the module's setters (`termgfx_termio_set_menu_key`, audio config).
  The *keys* differ per door; the module exposes setters, the door parses.
- Mapping the FIFO's `SST_KEY_*` to the engine's key type (ScummVM
  `Common::KEYCODE_*`; EasyRPG's `Input::Keys`).
- Being the **frame source** — the engine backend renders a native surface and
  calls `present()`.
- Feeding audio via the already-shared `termgfx_stream_*`.

### Explicitly out of scope

- Any behavior change to syncscumm (this is a move; byte-identical output).
- Building syncrpg (sub-project 2).
- An opaque multi-instance context struct.
- RTP / game-data concerns (a syncrpg-packaging concern, later).

## Shared API (`termgfx_termio.h`)

A door-facing C API; names are `termgfx_termio_*` (the current `sst_io_*` set,
moved and renamed). Semantics are today's `sst_io.c` behavior, unchanged:

- `int  termgfx_termio_init(int argc, char **argv)` — parse DOOR32/fd, resolve
  the socket, probe the terminal. Returns 0 to abort (e.g. headless with no fd,
  which the dump path handles).
- `void termgfx_termio_present(const uint8_t *rgb, int w, int h)` — hand a
  native-resolution frame; the module scales-to-canvas, tier-encodes, paces,
  and sends. (Exact pixel format — packed RGB/RGBA, stride — pinned during
  implementation to match the current internal surface contract.)
- `void termgfx_termio_tick(void)` — drive the pipeline once with no new frame
  (static-screen retry/flush); called every poll, as today.
- `int  termgfx_termio_next_event(termgfx_input_event_t *ev)` — pull one decoded
  keyboard/mouse event; 0 when the FIFO is empty.
- `int  termgfx_termio_quit_requested(void)` / `_hung_up(void)` /
  `_menu_requested(void)` — the reserved-hotkey and transport signals.
- `void termgfx_termio_set_menu_key(int letter)` — enable the GMM/menu hotkey on
  Ctrl+`<letter>` (0 = off).
- `int  termgfx_termio_audio_available(void)` — the session's audio capability
  (used by the door to decide subtitles / audio streaming), unchanged.
- `void termgfx_termio_shutdown(void)` — restore the terminal.

The `SST_KEY_*` enum and `sst_input_event_t` move to the shared header as
`termgfx_key_*` / `termgfx_input_event_t` (name TBD in implementation; a
mechanical rename).

## syncscumm refactor

`door/sst_io.c` is deleted; `door/syncscumm.cpp`'s OSystem backend calls the
`termgfx_termio_*` API instead of `sst_io_*`. The door-specific glue
(`resolveSubtitles`, `resolveVolumes`, `resolveMenuKey`, the `SST_KEY_*` →
`Common::KEYCODE` switch in `pollEvent`, the `SYNCSCUMM_DUMP` PPM path) stays in
syncscumm. `build.sh` links the shared module from libtermgfx instead of
compiling the local `sst_io.c`.

## Verification — behavior-identical (the safety net)

syncscumm is **live on the BBS** and freshly pushed; the refactor must not
change its behavior. Three gates, all required:

1. **Existing tests unchanged:** every `test/boot_*.sh` (BASS, Queen, Drascula,
   Lure, Cascade, Betrayed, SQ0) and `unit_sst_io` / the other unit binaries
   pass with no edits to the tests themselves.
2. **Wire-byte diff:** capture a recorded syncscumm session's exact outbound
   bytes (the wirecap diagnostic already exists) on a deterministic headless
   run, before and after the refactor, and **diff for byte-identity**. A pure
   move produces identical output; any diff is a regression to fix before
   proceeding.
3. **Live confirm:** the sysop plays a title on the real BBS and confirms no
   change (render, input, audio, quit, menu).

## Naming / location

- Module: `src/doors/termgfx/termgfx_termio.{c,h}` (alongside the existing
  `termgfx_stream`, sixel, jxl, `keymode`, `sbbs_node`).
- The current per-symbol `sst_io_*` names become `termgfx_termio_*`; the door's
  `SST_KEY_*` become the shared `termgfx_key_*`.

## Risks

- **Refactoring live code.** Mitigated by the wire-byte-diff gate: the change is
  provably a move, not a behavior change, or it does not land.
- **Hidden coupling** between `sst_io.c` and `syncscumm.cpp` (shared statics,
  build-flag assumptions like `WITH_JXL`/`SYNCSCUMM_NO_LAUNCHER`). Mitigated by
  drawing the API line at the already-clear seam (frame in, events out, config
  setters) and keeping engine-specific `#define`s in the door's build.
- **Scope creep into a context-struct redesign.** Explicitly disallowed for
  this sub-project; single-instance move only.

## Success criteria

- `termgfx_termio` exists in libtermgfx with the API above; `sst_io.c` is gone
  from syncscumm.
- syncscumm builds against it, all its tests pass unedited, its wire output is
  byte-identical, and the sysop confirms live parity.
- The API is sufficient for syncrpg's `BaseUi` backend to drive (frame source +
  event pull + config setters) — validated on paper here, exercised in
  sub-project 2.

## Open questions (for review)

- Exact frame pixel-format contract for `present()` (RGB vs RGBA, stride) —
  pin to the current internal surface to keep it a move.
- Final symbol names (`termgfx_termio_*` vs a shorter prefix) — cosmetic.
- Whether the trace/wirecap/stderr diagnostics move as-is or get a small config
  gate in the shared module — lean: move as-is.
