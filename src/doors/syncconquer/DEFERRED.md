# SyncConquer — deferred features

Features consciously skipped for v1. Each entry records why it was
deferred and what should trigger reevaluation. Review this list at every
milestone (first playable, v1 ship, each subsequent release).

## Campaign mode (missions, savegames, progression)

- **Why deferred**: v1 scope decision — skirmish vs computer opponents +
  intra-BBS multiplayer covers the lone caller and the headline feature;
  campaign
  drags in savegame handling, mission progression, and FMV.
- **Note**: RA has a no-movies path, so campaign may come nearly free once
  the engine is up. If it "just works" during development, promote it —
  it simply must not be on the v1 critical path.
- **Reevaluate**: at first-playable milestone (measure how broken/working
  campaign already is).

## VQA full-motion video (cutscenes, briefings)

- **Why deferred**: heaviest orthogonal subsystem; 320×200 15fps paletted
  video is streamable through the JXL/sixel path but is its own project.
- **Reevaluate**: after campaign mode lands.

## Inter-BBS federation (tracker service + relay)

- **Why deferred**: v1 strictly intra-BBS (user decision 2026-07-06).
  Engine is well suited (command-pipelined lockstep, order events only,
  modem-era latency tolerance — unlike the render-coupled FPS doors).
- **Design already accommodates it**: lobby registry sits behind an
  interface; the federation backend is a rendezvous/tracker service (e.g.
  a Synchronet services.ini JSON service on a well-known host) handing
  out host:port; transport stays Vanilla Conquer UDP between BBS hosts.
  A CnCNet-style relay for un-forwardable hosts is a later refinement.
- **Prerequisites**: same-version + same-asset join handshake (in v1),
  cross-host determinism validation, drop/timeout policy hardening.
- **Reevaluate**: after v1 multiplayer has real usage.

## Tiberian Dawn title (`syncdawn`)

- **Why deferred**: RA first (8-player MP, built-in skirmish computer
  opponents, Aftermath). TD is the same shared codebase — mostly
  asset/content work,
  plus classic TD has no skirmish mode (campaign-only for solo callers).
- **Reevaluate**: after v1 ships.

## Tiberian Sun / Red Alert 2 — recorded as NOT FEASIBLE

- Source never GPL'd; only open implementations are reimplementations
  (OpenRA, C#/.NET, gameplay-reimagined). Not an extension of this door;
  would be a separate project. Recorded here so it isn't re-litigated.

## SGR-pixels mouse (mode 1016)

- **Why deferred**: SGR 1006 cell resolution (8×16px) + game-side
  snap-to-target is expected to suffice (tile/object targeting, large
  sidebar buttons). 1016 is new work on both door and SyncTERM sides.
- **Reevaluate**: if playtesting shows targeting/selection misses that
  snapping can't fix (measure, don't assume).

## Contextual mouse cursor feedback (attack/sell/move shapes)

- **Why deferred**: v1 uses the player's OS pointer as the cursor
  (zero-latency motion, no dirty rects); engine cursor sprite suppressed,
  so contextual shapes are lost.
- **Options when revisited**: render engine cursor only when non-default
  (small dirty rect, lags pointer by RTT — double-cursor feel);
  cell-granular overlay hint near the pointer; SyncTERM extension to set
  the OS pointer shape.
- **Reevaluate**: first playtest feedback.

## Client-side sprite compositing (rejected approach B elements)

- **Why deferred**: full sprite-command rendering rejected (battle scenes
  → thousands of draw commands, SyncTERM-only, engine-invasive). But
  static-UI compositing from cached sheets (sidebar, cameos via
  C;S/C;L + DrawJXL, the SyncChess pattern) may beat dirty-rects for
  those regions.
- **SyncTERM now has the blit primitives (CTerm rev 1.332,
  `TERMGFX_CTERM_VER_ZOOM`).** Deuce's "scaled graphics APC blits" commit
  added, to DrawPPM / DrawJXL / P;Paste: **ZX/ZY** integer zoom (ship
  native-res, terminal replicates pixels), **FX/FY** mirroring (store one
  facing, mirror the other), signed **DX/DY** with off-screen **clipping**,
  and **mask-aware** zoom/flip — the mask buffer (since rev 1.316, fed by
  `C;LoadPBM`) gives non-rectangular **transparency**. Together that's a real
  sprite blitter: transparent + mirrored + integer-zoomed + edge-clipped
  cached blits. This is the missing piece that made cached-sprite-sheet
  compositing (units, cameos, sidebar) impractical before.
- **Still deferred — SyncTERM-only.** These are APC verbs no other terminal
  (sixel xterm/mlterm/wezterm, JXL-less builds) has, so they can't be the
  cross-terminal rendering path; the framebuffer dirty-rect differ (DESIGN.md
  Phase 2) stays the portable plan. And full *unit*-sprite compositing still
  needs engine-invasive draw-call hooking, which these primitives don't
  remove. What they newly enable cheaply is (a) **static-UI cached blits** on
  SyncTERM and (b) a **native-res + client ZX/ZY zoom** transport (smaller
  uploads at large canvases) — both SyncTERM-tier add-ons layered on the
  portable path, not replacements for it.
- **Reevaluate**: after phase-2 dirty-rect + image-cache metrics exist; the
  ZX/ZY native-res-zoom transport can be evaluated independently (it's a
  full-frame/geometry win, not sprite-specific).

## Cross-architecture determinism validation

- **Why deferred**: v1 multiplayer is same-host/LAN with identical
  Linux/x86-64 builds — trivially safe. Classic C&C is integer/fixed-point
  math so cross-arch is likely fine, but unverified.
- **Reevaluate**: prerequisite for federation; test before promising
  mixed-platform games.

## Text-tier RTS usability

- **Why deferred**: text tiers (cell-diff half/quadrant/sextant blocks)
  function as a fallback, but a 640×400 RTS at text resolution is of
  unknown playability. v1 ships it working, not polished.
- **Reevaluate**: if non-sixel-terminal demand materializes.

## SyncTERM partial-band sixel fix (SourceForge #258)

- **Constraint, not feature**: phase-2 partial sixel rects stay 6-pixel
  band-aligned until the cterm_dec.c band-buffer fix (already root-caused)
  ships in SyncTERM releases. Unaligned rects would leak previous band
  content on affected clients.
- **Reevaluate**: when the fix is merged/released; then drop alignment
  restriction guardedly (old clients persist).

## termgfx_present() unification

- **Why deferred**: termgfx README already flags unifying the per-door
  present/pacing/tier-dispatch loops; SyncConquer clones syncduke's loop
  like the other doors rather than blocking on the refactor.
- **Reevaluate**: once three doors share near-identical loops, the
  extraction is well-motivated — good post-v1 cleanup.

## Windows x64 build of the door

- **Why deferred**: the Win32 (x86) build is complete and is the target
  the rest of the Synchronet Windows build uses, so x64 buys nothing
  today. `build.bat` refuses `x64` up front rather than failing deep
  into the build.
- **What blocks it**: the vendored engine's *Windows-without-SDL* path —
  the one the headless door selects — has Win16-era 32-bit assumptions
  upstream never compiles (its Windows build always defines `SDL_BUILD`
  and skips them). Known offenders: `common/wwmouse.cpp` passes a
  `DWORD`-argument callback to `timeSetEvent()` where `LPTIMECALLBACK`
  wants `DWORD_PTR`; `redalert/winstub.cpp` assigns a
  `long(HWND,UINT,UINT,LONG)` to a `WNDPROC`. Both are hard MSVC errors
  at x64 and harmless at x86, where `DWORD`/`LONG` are pointer-width.
  The list is not known to be exhaustive — fixing the first two only
  uncovered the next.
- **Note**: these are upstream bugs, best fixed upstream (Vanilla
  Conquer) rather than accumulated as local patches in PROVENANCE.md.
- **Reevaluate**: if a 64-bit-only Windows dependency appears, or if
  upstream ports its non-SDL Windows path.
