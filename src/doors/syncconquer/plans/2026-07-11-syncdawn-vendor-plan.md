# SyncDawn (Tiberian Dawn) — vendoring + patch-adaptation plan

Scope for adding **Tiberian Dawn** as the second title of the SyncConquer door
family (`syncdawn`), alongside the shipped Red Alert door (`syncalert`). Per
DESIGN.md / DEFERRED.md this was always the planned second title; TD is "mostly
asset/content work" because the door shell, the `common/` engine, `termgfx`, and
the DOOR32 layer are all shared. The heaviest single piece is vendoring +
patching the `tiberiandawn/` engine source.

The door lives in the **same** `src/doors/syncconquer/` tree (shared shell + both
vendored engines → `syncalert` **and** `syncdawn` binaries); only the
`xtrn/syncdawn/` install bundle is a separate per-title thing (already drafted:
`install-xtrn.ini`, `fetch-assets.js`, `.gitignore`).

## What is already in place

- `xtrn/syncdawn/` install bundle drafted (registration + asset fetcher stub).
- Top `CMakeLists.txt` already guards `add_subdirectory(tiberiandawn)` behind
  `BUILD_VANILLATD OR BUILD_REMASTERTD` (PROVENANCE patch #3), so enabling TD is
  a flag flip once the source is vendored.
- The entire `door/` seam (`video_termgfx.cpp`, `wwkeyboard_termgfx.cpp`,
  `soundio_termgfx.cpp`, `vqaaudio_termgfx.cpp`, `door_core.c`, `door_io.c`,
  `door_input.c`, `door_node.c`) implements Vanilla Conquer's `common/`
  platform interfaces (`video.h` / `wwkeyboard.h` / `sound.h`) — engine-agnostic,
  so it is reused as-is by the TD target.
- termgfx (graphics/audio/caps), the shared DOOR32 parser (`termgfx_door32_read`),
  and the stdio-door plumbing are all shared library code.

## The 20 local patches, split by reuse (PROVENANCE.md)

**A. `common/` patches — FREE for TD** (TD links the same patched `common/`; no
re-work, only verify they still apply after the re-vendor):

| # | File | What |
|---|------|------|
| 1 | `common/CMakeLists.txt` | add `wwkeyboard_null.cpp` to no-video sources |
| 2 | `common/wwkeyboard_null.cpp` | NEW null keyboard backend |
| 4 | `common/video_null.cpp` | `VideoSurfaceDummy` gets real backing memory |
| 6 | `common/soundio_common.cpp` | `Maintenance_Callback` re-arm |
| 8 | `common/soundio_common.cpp` | `File_Stream_Sample_Vol` whole-file decode |
| 9 | `common/ini.h` + `common/paths.cpp` | stop truncating long INI values |
| 10 | `common/utfargs.h` | Windows: keep CRT argv (gated on a build define) |

**B. `redalert/`-specific patches — need a `tiberiandawn/` equivalent** (the same
*class* of fix, re-applied to TD's own copy of that code; each must be verified
against TD's actual source since the functions differ in detail):

| # | RA file(s) | Fix | TD note |
|---|-----------|-----|---------|
| 5 | conquer/globals/init.cpp | reconnect `-NOMOVIES` | TD has `Play_Movie`/`bNoMovies` too |
| 7 | init.cpp | `Cache_Bootstrap_Mixfile` (kill swallowed `assert(ok)`) | TD `Init_Bootstrap_Mixfiles` |
| 11 | startup.cpp | `ReadyToQuit` spin-wait gate (Win, `NEW_VIDEO_BUILD`) | TD startup.cpp |
| 12 | version/function.h/menus/goptions | door version footer | rename hook, see below |
| 13 | sounddlg.cpp + options.cpp | slider height + `*_Music_Volume_Step` | door hotkey hook |
| 14 | init.cpp | `VQAOPTF_AUDIO` (FMV dialog audio) | TD `Anim_Init` / movie path |
| 15 | sounddlg.cpp | re-sync music slider to ScoreVolume | TD sound dialog |
| 16 | scroll.cpp | widen edge-scroll to an 8·RESFACTOR band | TD scroll.cpp |
| 17 | options.cpp | ScoreVolume persistence (save gate + hotkey save) | TD options |
| 18 | menus.cpp + egos.cpp | stop forcing music to 0.4 when muted | TD menu/credits |
| 19 | init.cpp | MP music-volume direction | **likely N/A** — TD has no skirmish/MP, so no `MultiScoreVolume` split |
| 20 | startup.cpp | exit-save into a FRESH `INIClass` | TD startup.cpp |

So: **7 free, ~12 to port (one probably N/A).** The volume cluster (13/15/17/18,
+19) is the bulk and all revolves around TD's `Options`/`sounddlg`/`theme`
mechanism — port and verify together.

**C. Build enable (#3):** already done; flip `BUILD_VANILLATD` on and add the TD
build target.

## Phases

**Phase 0 — vendor the engine.** Add `vanilla/tiberiandawn/` from the same
upstream commit already pinned (`7f351daed0…`, Vanilla Conquer), the same way
`redalert/` was vendored. Update PROVENANCE.md's vendored-subset + "Not vendored"
lines. Re-confirm the 7 `common/` patches still apply. ~183 KLOC added.

**Phase 1 — build target.** Add a `VanillaTD` target paralleling `VanillaRA` in
the top `CMakeLists.txt`: enable `BUILD_VANILLATD`, fold the same `door/` seam
TUs in, define `NEW_VIDEO_BUILD` + the keep-CRT-argv define + TD's compile defs,
re-apply the console-subsystem + include-path non-patches (they're post-hoc,
target-scoped — clone them for `VanillaTD`). Goal: **it links** (this will
surface any TD-only missing symbols the door seam must satisfy).

**Phase 2 — port the ~12 redalert patches to `tiberiandawn/`.** Structural/
mechanical ones first (#5 movies, #7 bootstrap-cache, #11 quit-hang, #14 movie
audio, #16 edge-scroll, #20 exit-save), then the volume cluster (#13/15/17/18;
confirm #19 is N/A). Each verified against TD's real source, not assumed.

**Phase 3 — door↔engine hook adaptation.** The `door/` seam is engine-agnostic
except a few named hooks it calls into the RA engine:
- `SyncAlert_Music_Volume_Step` (redalert/options.cpp) — the `+`/`-` hotkey hook.
- `SyncAlert_Version_Name` (redalert/version.cpp) — the menu footer.
- `soundio_termgfx.cpp`'s coupling to `redalert/theme.cpp` (`Play_Song`,
  `File_Stream_Vol`, `ThemeClass::Still_Playing`).
- Asset / per-user INI names resolved in `door_io.c` (`REDALERT.INI`, the RA MIX
  set) → TD's `CONQUER.INI` + TD MIX set.

**Naming decision (do this in Phase 3):** rename these door↔engine hooks from
`SyncAlert_*` to a **game-neutral `SyncConquer_*`** and define them in *both*
`redalert/` and `tiberiandawn/` `options.cpp`/`version.cpp`. Each binary links
its own engine's copy, so the shared `door/` seam stops being RA-specific. (TD's
`theme.cpp` `ThemeClass` API needs a compatibility check against what
`soundio_termgfx.cpp` assumes.)

Also verify TD's `Bootstrap()` has the same `GameInFocus` spin the M1 note
documents (init.cpp `do { Keyboard->Check(); } while(!GameInFocus)`); the shared
`video_termgfx.cpp` (`NEW_VIDEO_BUILD` + `Get_Video_Mouse`) already covers it if
so.

**Phase 4 — assets + launch.** Pin the freeware TD source in
`xtrn/syncdawn/fetch-assets.js` (its top TODO: image URL + SHA-1 + MIX member
paths, list confirmed against TD's `Init_Bulk_Data` mix registration). Wire the
per-user `-home` (`tibdawn/`, `CONQUER.INI` + saves). Live-test a campaign
mission start over SyncTERM.

## Open questions / risks

- **TD data set**: confirm exactly which MIX files Vanilla TD registers (drives
  both `fetch-assets.js` and the bootstrap-cache patch #7 file list).
- **No skirmish**: TD is campaign-only for a solo caller — no lobby/AI-skirmish
  entry (simpler than RA, but the "pick a mission/faction" flow differs; the
  door's single-player launch path may need a small TD-specific menu nudge).
- **Movies work** (not skipped): RA plays FMV cutscenes — video plus A/V-synced
  audio — via the live `door/vqaaudio_termgfx.cpp` backend (which replaces the
  `vqaaudio_null.cpp` stub) over the shared Westwood VQA player in `common/`. TD
  uses the same VQA path, so its briefing/win/lose cutscenes should work once
  patch #14 (`VQAOPTF_AUDIO`, in TD's movie init) is ported and the TD movie
  data is fetched. So port #14 as a real requirement, include the TD movie
  mix(es) in `fetch-assets.js`, and confirm TD's movie packaging (MOVIES.MIX vs
  loose VQAs). The stale "v1 skips FMV" note in the RA `fetch-assets.js` header
  predates the VQA backend and should be corrected there too.
- **theme.cpp API drift**: RA's and TD's `ThemeClass` may differ enough that
  `soundio_termgfx.cpp` needs `#if`/an abstraction rather than a straight reuse.
- **Resolution**: RA renders 640×400; confirm Vanilla TD does too (the door's
  640×400 framebuffer + sixel/JXL geometry assume it).

## Rough effort

Vendoring + build-to-link (Phase 0–1) is a day or two of CMake/link wrangling.
The ~12 patch ports (Phase 2) are individually small but each needs TD-source
verification. Phase 3 hook rename + theme check is the real integration risk.
Net: a solid but bounded effort — not a new door, because the shell/common/
termgfx/DOOR32 layers carry over. Matches DEFERRED.md's "mostly asset/content
work" once the engine is in.
