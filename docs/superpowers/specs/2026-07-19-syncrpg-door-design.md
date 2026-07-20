# SyncRPG — EasyRPG (RPG Maker 2000/2003) door — design

**Date:** 2026-07-19
**Status:** design (approved direction; pending spec review)
**One-liner:** A dedicated Synchronet door that plays RPG Maker 2000/2003 games
over a terminal via a vendored EasyRPG Player, driving the shared
`termgfx_termio` module through a new truecolor present path, with the freeware
Yume Nikki (+ English fan-translation) as the v1 flagship.

This is **sub-project 2** of the SyncRPG effort. Sub-project 1 (extracting the
shared `termgfx_termio` terminal-I/O module out of syncscumm) is complete,
reviewed, live-confirmed, and shipped; this sub-project builds the door on top
of it. It depends on sub-project 1 having landed.

## Goal

Yume Nikki playable end-to-end on the BBS — movement, the effects/menu system,
and save/load — as a single proof, with the door built the syncscumm way
(vendored engine + per-game data fetcher + per-game `install-xtrn` registration,
one binary launched with a game's data path) so other RM2k/2k3 freeware games
install the same way later.

## Background / motivation

- EasyRPG Player (GPLv3, RPG Maker 2000/2003) was scouted feasible on this host:
  it builds here and renders a real RM2k3 game 320×240 headless. It exposes a
  clean `BaseUi` virtual interface (`ProcessEvents` / `UpdateDisplay` /
  `GetAudio` / `CreateUi`), so a dedicated Synchronet backend is feasible —
  directly analogous to syncscumm's `OSystem_Synchronet`, with no Duke-style
  shim. Deps: `fmt` / `opusfile` / `libxmp` (system), `inih` (built from source,
  two files — Debian doesn't package it), inline liblcf
  (`-DPLAYER_BUILD_LIBLCF=ON`); select SDL2 (`-DPLAYER_TARGET_PLATFORM=SDL2`) as
  the reference backend to mirror, though the synchronet backend replaces it.
- The per-door terminal-I/O orchestration is already consolidated in
  `termgfx_termio` (canvas probing, aspect-preserving scale, sixel/JXL tier
  selection + pacing, keyboard/mouse decode, DOOR32/socket I/O). syncrpg is the
  second consumer of that module and the forcing function for its truecolor
  present path.
- Path **B** (a dedicated door) was chosen over Path A (an EasyRPG libretro core
  under SyncRetro): the dedicated `BaseUi` backend is cleaner and gives direct
  control of input/audio/save paths.

## The one architectural decision: truecolor frames

syncscumm feeds `termgfx_termio_present(idx, pal)` **8-bit indexed** pixels plus
a 256-color palette, because ScummVM games are natively 256-color. **EasyRPG is
different: it composites to a 32-bit truecolor screen surface.** Rather than
quantize door-side and reuse the indexed API (which would force 256-color output
even on JXL-capable clients and round-trip RGB→indexed→RGB for JXL), this design
**adds a truecolor present path to the shared module** — approach chosen because
it also serves the future truecolor doors (retro/doom), which the extraction
spec explicitly anticipated.

### Frame format — 32-bit XRGB, not 24-bit RGB

EasyRPG's screen (`BaseUi::main_surface`, via `GetDisplaySurface()`) is
inherently 32-bit; the backend chooses the channel order (EasyRPG's
`format_R8G8B8A8_n` / `B8G8R8A8_n` family — the `_n` suffix means **NoAlpha**:
the fourth byte is padding, not a meaningful alpha). Alpha carries no
information at the present boundary — every sprite is already composited into the
opaque screen surface upstream. The present path therefore takes a **32-bit
packed buffer with the pad byte ignored**, not 24-bit RGB, for one reason:
zero-copy. Because syncrpg *writes* the backend, it makes EasyRPG render directly
into termgfx's expected 32-bit layout, so `UpdateDisplay` hands the buffer
straight through with no per-frame repack — and the same applies to future
libretro cores, which are also natively 32-bit XRGB8888. A 24-bit API would
force a 4→3-byte repack every frame for no benefit.

- **API:** `void termgfx_termio_present_rgbx(const uint8_t *xrgb, int w, int h)`.
  `xrgb` is `w*h` 32-bit pixels, **byte order R, G, B, X** in memory (X =
  ignored pad), stride `w*4`. termgfx reads bytes [0],[1],[2] as R,G,B at
  stride 4 with no swizzle. The exact EasyRPG `format_*` constant that yields
  this byte order is pinned in implementation; the synchronet backend sets
  `main_surface` to it so the hand-off is zero-copy.
- The existing `termgfx_termio_present(idx, pal)` stays unchanged for indexed
  sources (syncscumm, the future doom door).

### Quantizer moves into termgfx

The truecolor path needs a quantizer for the sixel tier. syncscumm's median-cut
quantizer (`door/sst_quant.{c,h}`) moves into the shared library as
`termgfx/termgfx_quant.{c,h}` (mechanical rename, same deterministic algorithm),
and syncscumm's overlay path switches to the shared copy. This is a small
extension of the sub-project-1 move.

### Tier branching inside `present_rgbx`

- **sixel** tier: `termgfx_quant` reduces XRGB→(idx, pal768) internally, then the
  existing sixel encoder runs — unchanged output path.
- **JXL** tier: the R,G,B bytes are encoded **directly** (no indexed
  round-trip) — the concrete benefit of this approach.
- **no-gfx** tier: unchanged.

### Verification of the shared-module change

syncscumm is live and must not regress. Its existing indexed path stays
byte-identical, gated by: its boot suite (9/9), its unit tests (20/20), and a
**PPM / decoded-image** comparison — **not** a raw sixel-byte diff. (Lesson from
sub-project 1: a syncscumm sixel capture is not reproducible across builds —
ScummVM bakes `__DATE__`/`__TIME__` into an on-screen version string, and the
palette-register serialization order is build-nondeterministic. The valid gates
are the PPM render, the boot/unit suites, and the decoded image; raw sixel
captures are not an invariant.) The new `present_rgbx` path gets its own unit
test: feed a known XRGB frame, assert the emitted sixel (quantized) and JXL
(direct) output.

## Component layout (mirrors syncscumm)

```
src/doors/syncrpg/
  easyrpg/               vendored EasyRPG Player + liblcf + inih (pruned)
  door/
    syncrpg.cpp          main + BaseUi_Synchronet (analogue of OSystem_Synchronet)
    video_term.{cpp,h}   GetDisplaySurface() 32-bit frame -> present_rgbx
    audio_term.{cpp,h}   EasyRPG audio (GenericAudio + decoders) -> termgfx audio stream
    input_term.{cpp,h}   termgfx key events -> EasyRPG Input::Keys
  build.sh, deploy.js, module.mk, CMakeLists.txt, msvc/
  getdata.js, install-xtrn.ini, syncrpg.example.ini, test/
```

## The EasyRPG synchronet backend

`BaseUi_Synchronet : BaseUi` implements the three hot paths:

- **`ProcessEvents()`** — drain `termgfx_termio_next_event()` and translate
  `TERMGFX_KEY_*` to EasyRPG `Input::Keys`, plus honor the reserved door hotkeys
  termgfx already consumes (Ctrl-S stats, Ctrl-Q quit, the configurable menu
  key). Also drives `termgfx_termio_quit_requested()` / `_hung_up()`.
- **`UpdateDisplay()`** — hand `GetDisplaySurface()`'s 32-bit buffer to
  `video_term`, which calls `termgfx_termio_present_rgbx()`.
- **Audio** — EasyRPG's `GenericAudio` mixes to PCM; `audio_term` feeds that PCM
  to the shared `termgfx_stream_*` module (the same audio path syncscumm uses).
  Use EasyRPG's built-in **fmmidi** synth for MIDI so the door is self-contained
  (no external soundfont); the built-in WAV/OGG/MP3 decoders cover the rest.

The backend renders straight to `main_surface` (headless by construction — no
SDL window, no `SDL_VIDEODRIVER=dummy` needed; the scouting proved the internal
bitmap is fully rendered without a display surface).

## Data & control flow

- **Game data (`getdata.js`)** — fetches the freeware Yume Nikki and applies the
  English fan-translation patch. The most legitimate/stable source is pinned in
  the implementation plan after research; sourcing follows the project rule
  (fetch from an established source, never re-host), and the fetched tree is
  git-ignored (only checksums/manifests are committed). Yume Nikki is
  self-contained (no RTP).
- **Saves & config** — per-user, at **`data/user/<####>/syncrpg/<game>/`**
  (zero-padded user number, matching the door per-user-save convention). EasyRPG's
  save path is pointed there; the user number comes from the DOOR32.SYS drop file
  that `termgfx_termio` already parses. Global per-game state (if any) lives in
  `data/syncrpg/<game>/`.
- **Launch** — `install-xtrn.ini` registers Yume Nikki as its own `xtrn` menu
  entry; the binary is launched with that game's data path (the syncscumm model).
  Directories are located via `system.*_dir`, never hardcoded.
- **Input mapping** — arrows = move; Enter / Space = action/confirm; Esc =
  cancel/menu; Shift = dash; plus the termgfx-reserved hotkeys above. Numeric
  keypad optional. Exact EasyRPG `Input::Keys` bindings pinned in the plan.

## Scope

### In scope (v1)

- The `termgfx_termio` truecolor present path (`present_rgbx`), the
  `sst_quant`→`termgfx_quant` move, and syncscumm regression verification.
- The syncrpg door: vendored/pruned EasyRPG + liblcf + inih, the synchronet
  `BaseUi` backend, video/audio/input, per-user saves, `getdata.js` +
  `install-xtrn` for Yume Nikki, `build.sh` / `deploy.js` / `test/`.
- Yume Nikki (freeware original + English fan-TL) playable end-to-end.

### Explicitly out of scope

- Any second game in v1 (the door is *structured* for more; only Yume Nikki
  ships).
- RTP-dependent games and newer RPG Makers (RMXP+/MV/MZ — those need mkxp-z /
  JS runtimes; separate, harder efforts).
- Migrating retro/doom/duke/conquer onto `present_rgbx` (the API is *designed*
  for them here; their adoption is a separate later effort).
- Mouse input (RM2k/2k3 games are keyboard-driven; termgfx mouse stays available
  but unused).
- The Steam re-release of Yume Nikki (free but Steam-gated + non-redistributable;
  does not fit the fetcher model).

## Milestones

- **M0 — termgfx truecolor path.** Add `present_rgbx` + move `sst_quant`→
  `termgfx_quant` + tier branching; unit-test the new path; verify syncscumm
  unchanged (boot 9/9, unit 20/20, PPM/decoded-image identical). Lands in the
  shared module before the door consumes it.
- **M1 — video.** syncrpg skeleton, vendored/pruned EasyRPG, minimal
  `BaseUi_Synchronet`; Yume Nikki's title/intro renders over the terminal via
  `present_rgbx`. `test/boot_yumenikki.sh` captures headless frames.
- **M2 — keyboard.** Movement + action/cancel/menu/dash; exploration is playable.
- **M3 — audio.** BGM/SFX via EasyRPG decoders + fmmidi → `termgfx_stream_*`.
- **M4 — package + live.** `getdata.js` (fetch + fan-TL patch), `install-xtrn`,
  per-user save/load, sysop live play-test (movement, effects/menu, save/load).

## Verification / testing

- **M0:** the syncscumm regression gates above + a `termgfx_quant` /
  `present_rgbx` unit test.
- **Door:** `test/boot_yumenikki.sh` (headless frame capture, mirroring
  syncscumm's boot tests), input decode unit tests where a `TERMGFX_KEY_*` →
  `Input::Keys` seam warrants it, and a save/load round-trip check.
- **Portability:** the door wiring is C++ and the vendored EasyRPG builds under
  GCC/Clang; the MSVC path (create_project) is generated but may not be verified
  on this host — flag it explicitly, as with syncscumm.

## Risks

- **EasyRPG build/vendoring complexity.** Deps (fmt/opusfile/libxmp/inih/liblcf)
  and pruning the tree. Mitigated by the successful scouting build and by
  mirroring syncscumm's vendoring layout.
- **Audio (MIDI).** RM2k/2k3 lean on MIDI; fmmidi (built-in) keeps the door
  self-contained but its timbre differs from a soundfont synth. Confirm Yume
  Nikki's actual audio formats during M3; a soundfont synth stays a later option.
- **Save-path redirection.** EasyRPG must write saves to the per-user dir, not
  the (read-only, shared) game tree. Verify the config/CLI path knob does what we
  need.
- **Shared-module regression.** `present_rgbx` + the quantizer move must not
  change syncscumm's output — gated as above.
- **Yume Nikki sourcing legitimacy** and fan-TL patch mechanics — researched and
  pinned in the plan; fetched data is never committed.

## Success criteria

- `termgfx_termio_present_rgbx` exists and is unit-tested; `termgfx_quant` is in
  the shared library; syncscumm is verified unchanged.
- syncrpg builds, boots Yume Nikki headless, and a sysop plays it end-to-end on
  the real BBS — movement, effects/menu, and save/load all work.
- Another RM2k/2k3 freeware game could be added purely by writing its
  `getdata.js` + `install-xtrn.ini`, with no door code change.
- The truecolor present path is validated as a seam the future retro/doom doors
  can adopt (on paper here; exercised when they migrate).

## Open questions (for review)

- The canonical/stable **Yume Nikki source URL** and the exact English
  fan-translation patch — researched and pinned in the plan, not here.
- The exact **32-bit channel order / EasyRPG `format_*` constant** for the
  zero-copy hand-off — pinned in implementation to match termgfx's encoders.
- Whether **fmmidi** is sufficient for Yume Nikki's audio or a soundfont synth is
  wanted — decided during M3.
