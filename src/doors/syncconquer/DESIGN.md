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
  build.sh/deploy.js (*nix), build.bat/jsexec deploy.js (Windows/MSVC), this doc,
  DEFERRED.md.
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

Door shell (per-door today, like syncdoom/syncduke): client I/O from the
DOOR32.SYS/`-s` handoff, input parser (cloned from syncduke, minus the
FPS steering model, plus RTS pointer mapping), present/pacing/tier loop,
sbbs_node integration (who's-online, node.exb status, paging), logging.

## How the door reaches the player

Two ways, and the BBS chooses. The drop file is parsed by
[`../termgfx/door32.c`](../termgfx/door32.h), shared with the sibling doors.

- **A socket** -- `DOOR32.SYS` line 2 (comm type **2**), or `-s<fd>`. What
  Synchronet gives it. Synchronet does *not* hand a door the raw client socket: it
  interposes a loopback socketpair and pumps through its own threads, so the BBS
  does the telnet negotiation and the SSH crypto and the door never sees an IAC
  byte. That is why it works over SSH without knowing SSH exists.

- **The BBS's stdin/stdout** -- `DOOR32.SYS` comm type **0** ("local"): there is no
  socket because the BBS redirected our stdio. Synchronet writes that for an
  `XTRN_STDIO` program; Mystic does it when forking a door on \*nix. The door reads
  fd 0 and writes fd 1. **\*nix only** -- on Windows a door is handed a Winsock
  `SOCKET`, and the I/O seam cannot read a CRT pipe.

On a stdio door the BBS `dup2()`s stderr onto the player's stream, so diagnostics
are captured to `data/syncalert/syncalert_n<node>.log` rather than painted over the
game.

The door does not speak telnet itself: a BBS handing over the RAW client socket
would need IAC negotiation, `IAC IAC` unescaping, and to survive a NAWS resize
whose payload bytes look like keystrokes (one of them, `0x11`, is the quit key).

## Rendering & responsiveness (approach C: hybrid, staged)

**Phase 1 — proven full-frame pipeline.** Sixel/JXL/text tiers (plus
syncdoom's full-frame PPM APC tier — near-free code parity, useful on
JXL-less SyncTERM builds; not a default anywhere), capability probe
(DA1/CTDA, Q;JXL), geometry letterbox, whole-frame dedupe, DSR-ACK +
AIMD pipeline pacing, F4 tier cycling. Fastest path to playable; all
terminal classes covered.

**Phase 2 — dirty-rectangle responsiveness layer, added to libtermgfx
engine-agnostically** (all doors benefit): a shared framebuffer differ ships
only the regions that changed each present — over a cached/sixel static
background — plus a content-hashed image cache for static UI art. It keeps
Phase 1's change-driven present but sends only the pixels that actually moved.
The full specification (differ, coalescing, per-tier rect transport, fallback,
constraints, and expected throughput) is the **"Phase 2: Dirty-rectangle
rendering"** section below.

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

## Phase 2: Dirty-rectangle rendering

Phase 1 sends a whole frame whenever anything changes. Phase 2 keeps that
change-driven present but ships **only the pixels that actually changed** — the
RTS equivalent of moving sprites over a static background, done at the
framebuffer seam rather than by hooking engine draw calls (contrast the
rejected full-sprite-compositing approach above). It lives entirely in
**libtermgfx**, so SyncDuke and SyncDOOM inherit it too.

### Architecture: one differ, per-tier encoder backend

The **differ is transport-agnostic** — it compares the just-rendered frame to
the previous one and emits a set of changed rectangles. Only the per-rect
**encoder backend** varies by terminal, and it follows the graphics tier the
full-frame path already picked for that client. The door already retains
`g_last_fb` (the previous framebuffer) for whole-frame dedupe, so the differ
extends existing state rather than adding a subsystem.

### The differ

1. Diff the new framebuffer against `g_last_fb` at cell granularity, producing
   a dirty-cell bitmap.
2. **Coalesce** nearby dirty cells into a small number of bounding boxes — the
   first tuning knob. Too fine (many tiny rects) wastes per-rect header/encode
   overhead and compresses poorly; too coarse (one union box) re-sends
   unchanged pixels between scattered changes. Cap the rect count.
3. If the changed union exceeds a **fallback threshold** (% of frame), abandon
   rects and send a whole frame — the second knob. It bounds the worst case
   (map scroll, screen-wide explosion, palette change, tier switch) and
   guarantees the dirty path never does worse than Phase 1.

### Rect transport, by tier

Capability floors (CTerm rev = cterm.c CVS `$Revision:`, encoded as
`major*1000 + minor`): **Sixel 1189, APC PPM 1316, JPEG-XL 1318, blob verbs
1329.** Since APC PPM (1316) is newer than Sixel (1189), **every PPM-capable
SyncTERM also has Sixel** — there is no PPM-only client, so no separate
PPM-rect backend is needed.

- **JXL client** (SyncTERM ≥ 1318) → **`DrawJXL` rects.** JXL-encode each
  coalesced bounding box, placed pixel-exact via DX/DY. Cost ≈ changed-fraction
  × full-frame JXL, with no break-even cliff. Encode latency is the price;
  coalescing to 1–3 boxes/frame keeps it near the one full-frame encode the
  door already does.
- **No-JXL SyncTERM and non-SyncTERM sixel terminals** (xterm/mlterm/wezterm) →
  **partial sixel rects** over a cached/sixel background. Deliberately **no
  separate PPM-rect backend**: because PPM ⟹ Sixel, a no-JXL SyncTERM reuses
  the same sixel-rect path needed for pure sixel terminals, collapsing the
  whole non-JXL world to one backend.
- **Text tiers** (half/quadrant/sextant) → already differential; the text
  renderer diffs its character grid and repaints only changed cells.

Optional micro-optimization: on a JXL client the very smallest rects may go as
raw **`DrawPPM`** (a memcpy, zero encode latency) — but that is a latency
tunable, not a bandwidth strategy (see "Why not PPM rects on JXL" below).

### Background handling

The "static background" is two things, handled separately:

- **Static UI art** (sidebar frame, cameo icons, shell/menu screens) → the
  **image-cache manager**: content-hashed `C;S` store + `C;L` client-cache
  skip, mirroring `audio_mgr`'s upload-once pattern (the technique
  minesweeper.js and synchess.js already use). Uploaded once per client, ever,
  then drawn from the client's own cache.
- **Dynamic terrain** (the tactical map — static only between scrolls and fog
  updates) → handled by the differ; only changed terrain cells ship.

**Do not send the background as raw PPM.** It is the single largest transfer,
so it wants maximum compression or zero re-send: a full 640×400 raw PPM is
~1 MB base64, vs ~100–300 KB as sixel, vs ~0 if cached. Raw PPM for the
biggest, least-changing thing is the worst choice.

### Expected throughput

Dirty-rect cost ≈ (changed fraction) × full-frame cost, so the win ≈
1 / changed-fraction, capped by scrolling. Measured full-frame anchors
(syncalert, from node logs): full-res **sixel ~80–150 KB/frame at native
640×400** (200–630 KB at maximized canvases); text tiers ~80–140 KB; JXL
unmeasured, est. ~30–60 KB.

| Scene | ~Changed | vs full-frame |
| --- | --- | --- |
| Idle / studying map | 0% | already free (dedupe) |
| Unit micro / harvester / building | ~4–6% | ~3–5× native, ~10–20× maximized |
| Moderate combat | ~10–15% | ~2–3× |
| Heavy battle | ~25%+ | ~1.5× |
| **Map scroll** | ~85% | ~1.1–1.3× (the floor) |

The gain is dramatic on **sixel** (verbose full frames) — roughly **5–15×
average** across typical play (~1.5–3 MB/s → ~150–400 KB/s at native size) —
and grows with canvas size (more static area). On **JXL** the full frame is
already compact, so dirty-rect helps only via JXL-encoded rects (the
proportional win), not raw PPM. **Map scroll is the hard floor** on every tier:
the whole viewport turns over, so rects degenerate to ~full-frame. These are
model estimates — validate with the measurement probe (below) before committing
thresholds.

### Why not PPM rects on JXL

Raw PPM is ~25× less byte-dense than JXL (4 B/px on the wire vs ~0.15 B/px).
Break-even against re-sending a whole ~40 KB JXL frame is ~10,000 changed px ≈
**~4% of the frame** (~17 units): below that, PPM rects are cheaper; above it,
they cost *more* than the whole JXL frame. That crossover is too fragile to
base a bandwidth strategy on — hence `DrawJXL` rects on JXL clients, with PPM
reserved only as a smallest-rect latency option.

### Constraints

- **SF #258** (SyncTERM's partial-band sixel bug, root-caused in
  `cterm_dec.c`): a partial (non-6-px-multiple) last band reuses a stale band
  buffer. Sixel rects therefore round their height **up to a 6-px band** until
  the client fix ships. This is a SyncTERM-sixel bug only — pure sixel
  terminals aren't affected (their band alignment is inherent), and the JXL/PPM
  APC paths are pixel-positioned and sidestep it entirely.
- **JXL encode latency** — bounded by coalescing to a few boxes per frame.
- **Map-scroll floor** — no APC scroll-blit primitive exists, so a scroll
  re-sends the whole tactical viewport; the fallback threshold catches it.

### Implementation order

0. **Measurement probe (no transport change).** Run the differ + coalescer and
   *log* the would-be rect bytes per frame per tier during real play, while
   still sending full frames. Yields true thresholds and win factors from
   actual gameplay before any wire change.
1. **Differ + coalescer** in libtermgfx, gated behind the probe/flag.
2. **Sixel-rect backend** (biggest win; also covers pure sixel terminals) +
   the image-cache manager for static art.
3. **`DrawJXL`-rect backend** for JXL clients.
4. Tune coalescing + fallback threshold against the Phase-0 measurements.

### Deferred: SyncTERM sprite-blit primitives (CTerm ≥ 1.332)

SyncTERM's "scaled graphics APC blits" (rev 1.332) add ZX/ZY integer zoom,
FX/FY mirroring, off-screen clipping (signed DX/DY), and mask/transparency
(the mask buffer, `C;LoadPBM`) to DrawPPM/DrawJXL/P;Paste — a real sprite
blitter. These would enable SyncTERM-only extras layered on the portable path:
static-UI compositing from cached sheets, and a native-res + client-ZX/ZY-zoom
transport (smaller uploads at large canvases). Deferred because they're
SyncTERM-only (no other terminal has them) and the framebuffer differ stays the
cross-terminal plan; see DEFERRED.md ("Client-side sprite compositing").

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
  required MIX files (REDALERT.MIX + MAIN.MIX — the FMV movies ride inside
  MAIN.MIX, so cutscenes play) via
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
- `build.sh` / `deploy.js` (*nix) and `build.bat` / `jsexec deploy.js`
  (Windows/MSVC, mirroring SyncDuke/SyncDOOM) per house pattern
  (out-of-source CMake; building does not deploy).

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
