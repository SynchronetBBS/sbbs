# SyncSCUMM — ScummVM point-and-click adventures as a Synchronet door

Status: design approved, pre-implementation
Date: 2026-07-14

## Goal

A libtermgfx game door (joining SyncDOOM, SyncDuke, SyncConquer, SyncMOO1,
SyncRetro) that plays classic 2D point-and-click adventures over a terminal:
ScummVM's engine collection driven through a native Synchronet backend, with
graphics, mouse, and sound. v1 decisions made at design time:

- **Multi-title from day one**: one door, a game picker, a curated catalog.
  The officially-freeware adventures are the bundled flagships — Beneath a
  Steel Sky, Flight of the Amazon Queen, Lure of the Temptress, Drascula —
  with commercial titles (the LucasArts SCUMM classics) playable when a
  sysop supplies the data files.
- **Graphics required, mouse optional**: no text tier (these games are
  meaningless without pixels); mouse when the terminal reports it, a
  keyboard-driven cursor always.
- **Curated engine set**, not all of ScummVM: `scumm`, `sky`, `queen`,
  `lure`, `drascula` at v1. Growing the set is a build flag plus testing,
  not architecture.
- **Single-player** with a `node.exb` presence line. Spectating is a listed
  later milestone; the frame path must stay teeable (one encoder, N sinks
  later) but v1 ships one sink.

## Engine substrate: ScummVM

ScummVM (GPLv2+) is the maintained interpreter collection for SCUMM and
~80 sibling adventure engines. Unlike every engine vendored so far (Duke's
SDL-ectomy, Conquer's null-backend selection), ScummVM requires **no seam
carving**: its `OSystem` class is the project's published porting API —
every port (SDL, libretro, consoles) is an `OSystem` implementation. We
implement a Synchronet `OSystem` (sources in our `door/` directory, a thin
registration shim in-tree — see Layout) and the engines never know they are
in a terminal.

Vendoring follows the house pattern: pin the current stable release
(exact tag + SHA recorded in PROVENANCE.md at vendor time), source under
`src/doors/syncscumm/scummvm/`, our backend and door glue beside it.
ScummVM's own `configure` drives the build:
`--disable-all-engines --enable-engine=scumm,sky,queen,lure,drascula`
plus the new backend. All five v1 titles (and the SCUMM classics) render
320×200 8-bit paletted — the exact shape the SyncConquer dirty-rect work
was proven on.

## Layout & naming

```
src/doors/syncscumm/           door + backend + vendored engine
  scummvm/                     vendored ScummVM (pinned; PROVENANCE.md)
  door/                        ALL C/C++ we author (syncconquer convention):
                               the OSystem implementation (video, events,
                               mixer, saves, timers) + door glue (main,
                               config, node.exb). Registered into ScummVM's
                               build via a thin in-tree shim under
                               scummvm/backends/platform/synchronet/ that
                               only points at door/ sources — the one local
                               vendor-tree touch, documented in PROVENANCE.md
  build.sh, deploy.js          house pattern; live binary is a symlink
  DESIGN.md, PROVENANCE.md
xtrn/syncscumm/                install side
  install-xtrn.ini             registers door; runs fetch-games.js
  fetch-games.js               freeware provisioner (HTTPRequest.Download +
                               Archive; idempotent, non-fatal, .js per the
                               xtrn [exec:] rule — never a shell script)
  lobby JS + curated catalog   picker; per-title door-tested status
  games/<id>/                  game data (freeware fetched; commercial
                               sysop-dropped; validated by ScummVM's detector)
```

## Video path

`OSystem` graphics: the engine gives us an 8-bit paletted 320×200 surface +
palette updates; the GUI overlay (F5 save/options dialogs) renders 16-bit
RGB and is quantized through the SyncRetro-style quantizer into the same
indexed pipeline. The mouse cursor is composited server-side into the frame
before diffing (cursor motion = small dirty rects). Frames diff → coalesce →
sixel or JXL per the probed tier (termgfx), integer-scaled to the terminal
viewport (2× on 80-col SyncTERM → 640×400). Adventures are low-motion:
dirty-rect coalescing + the existing AIMD pacing carry the remote case.

## Input path

termgfx keymode negotiation (kitty / SyncTERM-evdev) as in the siblings;
ScummVM keys (F5 menu, Esc skip) pass through; family meta-keys (Ctrl-T /
Ctrl-S stats, Ctrl-P) stay consistent. Mouse is a three-rung ladder, probed
in-door (there is no mouse ARS — the user MOUSE flag only governs text
hotspots):

1. **DECSET 1016** (SGR pixel) — CTerm >= 1330 (gate via CTDA; the one-cell-
   off bug is fixed upstream, SF #267) and modern xterm family. Exact
   coordinates. This work finally builds the shared `termgfx_mouse` module.
2. **DECSET 1006** (SGR cell) — near-universal fallback. A cell click maps
   to the cell's center pixel, un-scaled into game space (4×8 game pixels at
   80×25 over 2×-scaled 320×200 — inside typical hotspot sizes). A 1006
   click also places the keyboard cursor: "click roughly, nudge precisely."
3. **No mouse reporting** — keyboard cursor only.

Keyboard cursor (always available): arrows/WASD move with acceleration,
Enter = left click, Tab = right click.

## Audio path

ScummVM's mixer hands the backend one mixed PCM stream — music (AdLib/MT-32
synthesized engine-side), speech, SFX — so iMUSE and talkie versions come
for free. New work is **continuous streaming** over the audio APCs (the
siblings do one-shots and looped tracks): chunk mixer output (~1-2 s),
encode OGG/Opus with termgfx's encoder, C;S + Load + Queue each chunk onto
one channel — the FIFO plays chunks gapless — pacing uploads against
playback with a small prefetch depth so audio never starves video. Probe
once; no libsndfile → silent tier, like the siblings.

## Saves & per-user state

`OSystem`'s SavefileManager routes every engine's saves to
**`data/user/<####>/scumm/<gameid>/`** (zero-padded user number via the
`SBBSDATA` env var; created on first use) — per-user, per-game, engine-
agnostic. ScummVM's own F5 dialog handles slots/naming (it is the overlay
GUI we already render). Disconnect mid-game: no engine-agnostic checkpoint
exists (unlike the Z-machine door), so v1 documents "save often"; a later
milestone investigates wiring ScummVM's autosave to the hangup path.

## Presence

`node.exb` free-text who's-online line via the existing sbbs_node
integration ("playing Beneath a Steel Sky"). Spectate: deferred, design
constraint recorded above.

## Milestones

- **M1 vendor + boot**: vendored tree builds with curated engines; backend
  skeleton; headless boot of Beneath a Steel Sky to first frame, PPM dumps.
- **M2 video**: dirty-rects, palette, cursor compositing, overlay
  quantization; live SyncTERM validation.
- **M3 input**: keymode, the 1016/1006/keyboard mouse ladder,
  `termgfx_mouse`; playable start-to-hotspot.
- **M4 audio**: chunked mixer streaming; music + speech validated.
- **M5 ship**: SavefileManager, picker/lobby, fetch-games.js, catalog,
  install-xtrn.ini, node.exb, SYSOP docs.
- **M6 polish**: Windows build, autosave-on-hangup investigation, engine
  additions, spectate groundwork.

Each milestone gets a headless test rig (the house pattern: PPM/wire dumps
asserted without a terminal) before live SyncTERM validation.

## Explicitly out of scope for v1

Text tier; spectate (deferred); engines beyond the curated five; 640×480+
titles; shared-control co-op; autosave-on-hangup (investigated in M6).
