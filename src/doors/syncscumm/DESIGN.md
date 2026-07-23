# SyncSCUMM — ScummVM point-and-click adventures as a Synchronet door

Status: Shipped — one binary, one installable xtrn package per title
(SyncBASS, SyncQueen, SyncLure, SyncDrascula ship freeware-ready; commercial
titles play when the sysop supplies the data files). There is no in-door game
picker/lobby — not planned.
Date: 2026-07-15 (design); shipped 2026-07-18

## Goal

A libtermgfx game door (joining SyncDOOM, SyncDuke, SyncConquer, SyncMOO1,
SyncRetro) that plays classic 2D point-and-click adventures over a terminal:
ScummVM's engine collection driven through a native termgfx backend, with
graphics, mouse, and sound. v1 decisions made at design time:

- **Multi-title, one binary**: a single door binary plays every title, but
  each game installs as its own Synchronet xtrn package pointing at that
  shared binary (see `deploy.js`, `COMPILING.md`) — there is **no** in-door
  game picker or lobby, and none is planned. The officially-freeware
  adventures ship as ready-made packages — Beneath a Steel Sky (SyncBASS),
  Flight of the Amazon Queen (SyncQueen), Lure of the Temptress (SyncLure),
  Drascula (SyncDrascula) — with commercial titles (the LucasArts SCUMM
  classics) playable when a sysop supplies the data files.
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
implement a termgfx `OSystem` (sources in our `door/` directory, a thin
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
                               scummvm/backends/platform/termgfx/ that
                               only points at door/ sources — the one local
                               vendor-tree touch, documented in PROVENANCE.md
  build.sh, deploy.js          house pattern; live binary is a symlink
  DESIGN.md, PROVENANCE.md
xtrn/<title>/                  install side — ONE package per title
  install-xtrn.ini             registers that title's door command (the
                               shared binary + that game's id); no picker,
                               no lobby
  games/<id>/                  game data (freeware bundled/dropped in;
                               commercial sysop-supplied; validated by
                               ScummVM's detector)
  (a freeware auto-provisioner -- getgames/fetch-games.js, HTTPRequest.Download
   + Archive -- is a DEFERRED idea, not shipped; freeware titles ship as
   ready-made packages instead)
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

**Shipped in M4.** ScummVM's mixer hands the backend one mixed PCM stream —
music (AdLib/MT-32 synthesized engine-side), speech, SFX — so iMUSE and
talkie versions come for free.

The continuous-streaming machinery is **not this door's**: it is
`termgfx/audio_stream.{c,h}` + `termgfx/chunk.c`, shared with SyncRetro (which
had the same shape first — a mixed stream with no useful per-sound boundary —
and where it was originally built and then extracted). The chunk accumulator,
silence cache, cushion, backlog policy, blob path and underrun re-prime all
live there. `door/sst_io.c` supplies only the door's half: the put/flush/
backlog output seam and the session's config.

What actually ships, per session:

- 100 ms chunks of the mixer's genuine **stereo** PCM at 22050 Hz, Opus-encoded
  (VBR quality 0.15), each `C;S` + `A;Load` + `A;Queue`d onto channel 2 — the
  FIFO plays them back to back. CTerm ≥ 1.329 takes them inline via `A;LoadBlob`
  instead, with no cache-file churn at all.
- A **3-chunk (~300 ms) cushion** before playback starts. It is both the latency
  budget and the whole jitter tolerance — a FIFO fed one chunk at a time from
  empty underruns on the first stutter — so an underrun re-primes rather than
  resumes.
- **Channel volume 70, not 100**: the stream is already mixed and SyncTERM's
  mixer soft-clips, so unity distorts. A mute is never spelled `volume = 0`
  (SyncTERM clamps 0 to −60 dB and bakes it into the channel's entry volume);
  the module stops sending instead.
- The stream opens **lazily on the first PCM**, once the capability probe has
  answered — a stream created against an unknown tier would either drop the
  cushion's first chunks or emit to a terminal that cannot play them. Tier 1
  (libsndfile) streams; tone-only or absent degrades to silence, and
  `subtitles = auto` turns subtitles on for exactly those sessions.
- Sysop-tunable via `syncscumm.ini`'s `[audio]` section (`enabled`, `quality`,
  `volume`, `chunk_ms`, `prebuffer`); the module clamps every value, so a
  nonsense setting costs the default, never the session.

Full rationale — the measurements, the rejected alternatives, the transport
choice — is in
[`docs/superpowers/specs/2026-07-16-syncscumm-m4-audio-design.md`](../../../docs/superpowers/specs/2026-07-16-syncscumm-m4-audio-design.md).
**Read its "Amendment 2026-07-16" section first: on transport it supersedes the
spec's own body**, which is left intact for its reasoning rather than rewritten.

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
- **M5 ship**: SavefileManager, node.exb, per-title install-xtrn.ini
  packages, SYSOP docs. **Dropped from M5:** an in-door picker/lobby (not
  planned — each title installs as its own xtrn package instead) and
  fetch-games.js (deferred — freeware titles ship as ready-made packages).
- **M6 polish**: Windows build, autosave-on-hangup investigation, engine
  additions, spectate groundwork.

Each milestone gets a headless test rig (the house pattern: PPM/wire dumps
asserted without a terminal) before live SyncTERM validation.

## Explicitly out of scope for v1

Text tier; spectate (deferred); engines beyond the curated five; 640×480+
titles; shared-control co-op; autosave-on-hangup (investigated in M6).
