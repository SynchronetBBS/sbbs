# SyncConquer — classic Command & Conquer engine as a Synchronet door

Status: design approved, pre-implementation
Date: 2026-07-06
First title: Red Alert (`syncalert`); second title: Tiberian Dawn (`syncdawn`, deferred)

## Goal

A third libtermgfx game door (after SyncDOOM and SyncDuke): the classic
Westwood RTS engine, playable over a terminal with mouse, graphics, and
sound. Priorities set at design time:

- Multiplayer is a headline feature (v1: strictly intra-BBS).
- User responsiveness: exploit client-side caching (images and audio) and
  minimize retransmission; an RTS frame is mostly static content.
- A lone caller must get a complete game immediately (skirmish vs
  computer opponents — RA's built-in scripted game AI, not an LLM).

## Engine substrate: Vanilla Conquer

EA released the C&C game logic under GPLv3 (with section-7 additional terms
covering trademarks/assets) twice: the 2020 CnC_Remastered_Collection repo
(complete, compilable TD+RA logic) and the February 2025 preservation repos
(original-era TD/RA/Renegade/Generals — NOT compilable: missing Greenleaf,
HMI, Watcom-era pieces). The usable base is **Vanilla Conquer**
(github.com/TheAssemblyArmada/Vanilla-Conquer): a portable CMake/C++11
cleanup of the 2020 code producing standalone `vanillatd`/`vanillara`
binaries on Linux/gcc. ~500 KLOC (redalert ~223K, tiberiandawn ~183K,
common ~65K).

Why it fits:

- **Pre-cut headless seam.** Platform backends are compile-time-selected
  translation units with existing *null* implementations
  (`common/video_null.cpp`, `soundio_null.cpp` — how the Remaster DLL target
  builds, no SDL anywhere). Our backends implement `common/video.h`,
  `common/wwkeyboard.h`, `common/sound.h`; the game loop never knows it's in
  a terminal.
- **Software 8-bit paletted rendering at 640×400** into a plain memory
  buffer — a native match for the indexed sixel encoder (palette maps 1:1
  to color registers).
- **Command-pipelined deterministic lockstep multiplayer** (see below).
- Runs the original legacy assets (freeware CD releases), not the
  Remaster's.

Vendoring model follows SyncDuke's Chocolate Duke pattern: the Vanilla
Conquer source (redalert/ + common/; tiberiandawn/ excluded from the v1
build) lives under this directory with our shims beside it.

## Layout & naming

- `src/doors/syncconquer/` — door shell, termgfx shims, vendored engine,
  build.sh/deploy.sh, this doc, DEFERRED.md.
- `xtrn/syncalert/` — install-xtrn.ini, lobby.js, fetch-assets helper, docs.
  (`xtrn/syncdawn/` later.)
- Family name **SyncConquer** — "Conquer" has upstream precedent as the
  non-trademark shorthand (Vanilla Conquer, CONQUER.MIX) and stays clear
  of the "Command & Conquer" mark.
- Per-title binaries `syncalert`, `syncdawn` (user decision 2026-07-06).
  "Alert" alone evokes the title without embedding the "Red Alert" mark,
  keeping binary/door names clear of EA trademarks (whose use the GPL
  release's additional terms restrict).

## Architecture

Three engine shims at the Vanilla Conquer backend seam (modeled on its null
backends):

- `video_termgfx.cpp` — Set_Video_Mode / palette / blit; hands the
  640×400×8 framebuffer + 256-color palette to the door present loop.
- `wwkeyboard_termgfx.cpp` — parsed terminal input into the WWKeyboard
  queue plus mouse position/button state.
- `soundio_termgfx.cpp` — engine-decoded PCM (its existing AUD/ADPCM path)
  into the termgfx audio manager.

Door shell (per-door today, like syncdoom/syncduke): socket I/O from the
DOOR32.SYS/`-s` handoff, input parser (cloned from syncduke, minus the
FPS steering model, plus RTS pointer mapping), present/pacing/tier loop,
sbbs_node integration (who's-online, node.exb status, paging), logging.

## Rendering & responsiveness (approach C: hybrid, staged)

**Phase 1 — proven full-frame pipeline.** Sixel/JXL/text tiers (plus
syncdoom's full-frame PPM APC tier — near-free code parity, useful on
JXL-less SyncTERM builds; not a default anywhere), capability probe
(DA1/CTDA, Q;JXL), geometry letterbox, whole-frame dedupe, DSR-ACK +
AIMD pipeline pacing, F4 tier cycling. Fastest path to playable; all
terminal classes covered.

**Phase 2 — responsiveness layer, added to libtermgfx engine-agnostically**
(all doors benefit):

1. **Dirty-rectangle differ**: compare consecutive framebuffers, transmit
   only changed rectangles as partial draws. Rect transport on SyncTERM:
   APC **DrawPPM for small rects** (effectively a memcpy — no encoder
   latency/dependency; a 24×24 unit ≈ 2.3KB) and **DrawJXL for large
   ones**, both pixel-positioned via DX/DY with no band-alignment
   constraint (sidesteps SF #258 entirely). Non-SyncTERM sixel terminals
   get band-aligned partial sixel. Map scroll ships the leading edge;
   unit movement ships small rects. The whole-frame path remains the
   fallback (tier switches, palette changes, full-screen effects).
2. **Image cache manager**: content-hashed C;S store + C;L client-cache
   skip, mirroring `audio_mgr`'s pattern, for static art — sidebar frame,
   cameo icons, shell screens. Upload once per client, ever. Proven
   pattern: minesweeper.js and synchess.js already do C;L/C;S + cached
   draws client-side.

Constraint: partial-screen sixel is in the blast radius of SyncTERM's
partial-band bug (SourceForge #258, root-caused in cterm_dec.c). Rects
stay 6-pixel band-aligned until the client fix ships.

**Geometry.** RA's internal render resolution is fixed at 640×400
(GBUFF_INIT_* in redalert/externs.h; Settings.Video.Width/Height only
scale the SDL window, not the game buffer). SyncTERM's standard 80×25
with the 8×16 font is a 640×400 pixel canvas — an exact 1:1 match, no
scaling. Other canvas sizes are scaled server-side, **never cropped** —
RA's UI (full-height sidebar, tab strip) occupies the entire frame, so
there is no expendable margin. Two user-cyclable display-fit modes
(sticky per-user via flag file, Duke full-res-preference pattern; own
key alongside the F4 tier cycle):

- **Aspect** (default): true aspect ratio, letterbox bars as needed
  (80×24 = 640×384 canvas → 614×384 + side bars).
- **Fill**: stretch to the full canvas (80×24 → 640×384; any distortion
  is the user's informed choice).

Divergence from the FPS doors, deliberate: syncalert sends **full-res
sixel by default** (pan=1;pad=1). Duke/DOOM default to server-side
half-res + client pan/pad=2 doubling, near-lossless for their 320×200
native buffers; for RA's fine 640×400 detail (6–8px UI fonts) that path
discards half the resolution, so syncalert has no half-res mode at all.
Frame-size cost is acceptable: the RTS present loop is
change-driven (dedupe + phase-2 dirty rects), not a constant 10–30fps
stream. Larger canvases scale up (160×50 = 1280×800 is a clean integer
2×).

Rejected alternative (recorded): full client-side sprite compositing
(SyncChess model scaled up — cached sprite sheets + per-sprite DrawJXL).
Near-zero steady-state bandwidth but thousands of draw commands in battle
scenes, SyncTERM-only, and requires hooking engine-internal draw calls
instead of the clean framebuffer seam. Elements of it may return for
static UI; see DEFERRED.md.

## Mouse & input

- SGR 1006 any-motion mouse tracking (already proven in both doors:
  enable ?1003h/?1006h, parse buttons/motion/wheel/release), mapped
  through the displayed-image rectangle (letterbox-aware).
- **The player's OS pointer is the cursor.** The engine's cursor sprite is
  suppressed, so pointer motion costs zero round trips and generates no
  dirty rects; only clicks/drags/edge-scroll cross the wire. This is the
  single biggest responsiveness win an RTS gets over the FPS doors.
- Cell resolution (8×16px cells over a 640×400 image) with game-side
  snap-to-target. C&C targeting is tile/object based and sidebar buttons
  are large; measured inadequacy, not assumption, promotes SGR-pixels
  (1016) off the deferred list.
- Keyboard: kitty-keyboard negotiation + SyncTERM evdev + legacy CSI
  fallback, cloned from syncduke. RTS needs are lighter (no hold-to-move).
- Known tradeoff: contextual cursor shapes (attack/sell/move) don't show
  on the OS arrow — deferred, with options listed in DEFERRED.md.

## Audio

- **SFX**: engine-decoded PCM → `termgfx_audio_sfx`; upload-once patch
  cache is ideal for C&C's small, heavily-reused voice/SFX set
  ("Acknowledged"); left/right pan from engine position via
  `_volume_lr`.
- **Music**: Klepacki AUD tracks decoded once → existing OGG encode +
  door-side disk cache (`data/syncalert/audio`) + C;L client-cache skip —
  each track uploads to a given client exactly once across all sessions.
- Capability tiers per termgfx audio probe (none / tone / digital).

## Multiplayer (v1: strictly intra-BBS)

Vanilla Conquer's own lockstep netcode over its portable UDP transport
(`common/wspudp.cpp`), door process to door process on the host/LAN.
Netcode untouched.

Why this engine federates well later, unlike the FPS doors: C&C lockstep
is *command-pipelined* (orders scheduled MaxAhead frames ahead; each peer
simulates and renders locally; only tiny order events cross the wire;
engineered for 200–600ms modem links) rather than *render-coupled*
per-tic sync. All rendering/selection/scrolling latency stays between
caller and their own BBS. Federation (inter-BBS) is nonetheless **out of
scope for v1** — see DEFERRED.md.

- **Lobby**: `exec/load/game_lobby.js` shared-file registry + creation
  mutex + SyncDOOM-style N-player deferred-connect muster, generalized to
  2–8 players. Launch args/env carry role + peer list.
- The registry access sits behind a small interface so a phase-2
  federation tracker is a backend swap, not a redesign.
- Caller time-expiry mid-game = player drop (RA's existing drop handling);
  warn thresholds / sysop grace policy decided during implementation.
- Determinism: identical door version + asset set enforced in the join
  handshake; RA's scenario/game CRC desync detection surfaces mismatches.

## Assets

EA released C&C Gold (2007) and RA95 (2008) as official freeware, but
never published a redistribution grant; community redistribution is
tolerated, not licensed. Posture: **nothing EA-copyrighted enters the
repo.**

- `fetch-assets.js` install-time helper: download the RA95 freeware CD image
  from established community mirrors, verify checksum, extract only the
  required MIX files (movies excluded — v1 skips FMV anyway) via
  Synchronet's `Archive` object, which reads ISO9660 through libarchive.
  It is JavaScript, not a shell script, so `install-xtrn.js` can run it
  (`[exec:]` only executes `.js`) and so it works on Windows too.
- Door startup validates assets and refuses with a clear sysop-facing
  message if missing/invalid.

## Error handling & testing

- Desync → RA CRC detection → user-visible message + per-node log
  (`data/syncalert/syncalert_n<node>.log`, syncduke convention).
- Missing sixel → text tier still functions (RTS-on-text usability is a
  deferred-list item, not a v1 promise).
- Offline capture mode (`SIXELOUT`-style) for render testing without a
  BBS session; 2-node loopback MP as the standard multiplayer smoke test.
- `build.sh` / `deploy.sh` per house pattern (out-of-source CMake;
  building does not deploy).

## Extensibility (recorded facts)

- **Tiberian Dawn**: near-free second title — same shared codebase,
  separate game library + assets; becomes `syncdawn`.
- **Tiberian Sun / RA2**: NOT reachable from this substrate. Their source
  was never GPL'd (2020 = TD+RA remaster logic; Feb 2025 = TD, RA,
  Renegade, Generals). TS assets are freeware (2010) but the only open
  implementations are reimplementations (OpenRA: C#/.NET, gameplay
  reimagined) — a separate project on a weaker foundation, not an
  extension of this one.

## References

- Vanilla Conquer: https://github.com/TheAssemblyArmada/Vanilla-Conquer
- EA GPL repos: github.com/electronicarts/{CnC_Remastered_Collection,
  CnC_Tiberian_Dawn, CnC_Red_Alert, CnC_Renegade, CnC_Generals_Zero_Hour}
- Lockstep confirmation: TIBERIANDAWN/QUEUE.CPP (OutList/DoList,
  MaxAhead frame-sync) in CnC_Remastered_Collection
- termgfx subsystem map: src/doors/termgfx/README.md; client image-cache
  precedent: xtrn/minesweeper/minesweeper.js, xtrn/synchess/synchess.js
- Freeware history: https://cncnz.com/features/freeware-classic-command-conquer-games/
