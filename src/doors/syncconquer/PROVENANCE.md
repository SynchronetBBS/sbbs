# Vendored source provenance

## vanilla/ — Vanilla Conquer

- Upstream: https://github.com/TheAssemblyArmada/Vanilla-Conquer
- Commit: 7f351daed0c19d7c4764dc4ebae1a70c7809ac1f (2025-11-14)
- License: GPLv3 with EA additional terms (vanilla/License.txt)
- Vendored subset: CMakeLists.txt, CMakePresets.json, License.txt,
  README.md, cmake/, common/, redalert/, resources/
- Not vendored: tiberiandawn/ (deferred syncdawn title), tests/, tools/,
  scripts/, .github/

## Local patches (keep this list complete)

1. `common/CMakeLists.txt`: add `wwkeyboard_null.cpp` to the
   no-video-backend source list (upstream has no keyboard backend for
   headless builds; vanillara otherwise fails to link).
2. `common/wwkeyboard_null.cpp`: NEW FILE — null keyboard backend
   (template for the M2 terminal keyboard shim).
3. `CMakeLists.txt`: guard `add_subdirectory(tiberiandawn)` with
   `BUILD_VANILLATD OR BUILD_REMASTERTD` and `add_subdirectory(tools)`
   with `BUILD_TOOLS`, so the vendored subset configures.
4. `common/video_null.cpp`: `VideoSurfaceDummy` now holds real backing
   memory (ctor allocates `w*h`, dtor frees it; `GetData`/`GetPitch`
   return it; `IsReadyToBlit`/`LockWait`/`Unlock` return `true`) instead
   of the stub `nullptr`/`0`/`false` upstream ships for a truly
   video-less build — the door needs `VisiblePage.Lock()`/`Get_Offset()`
   to hand back a real, readable framebuffer. `IsAllocated()` is left
   returning `false` deliberately (see the M1 note below: it's an
   inverted "video memory OK" flag read by startup.cpp).
5. `redalert/conquer.cpp`, `redalert/globals.cpp`, `redalert/init.cpp`:
   reconnect the `-NOMOVIES` command-line flag. Upstream declares/reads
   `bNoMovies` but never defines it and gates both the extern and the
   `-NOMOVIES` parsing on `CHEAT_KEYS` (off by default) — so the flag was
   dead code (see plans/m2-proto/FACTS.md #6). The patch: define
   `bool bNoMovies = false;` in `globals.cpp` alongside `GameInFocus`,
   un-gate the `extern`/parsing in `init.cpp`, and early-return from
   `Play_Movie()` in `conquer.cpp` when it's set. All three hunks are
   applied verbatim from `plans/m2-proto/keystone.patch`.
6. `common/soundio_common.cpp`: `Maintenance_Callback()` re-arms a
   tracker's `MoreSource` flag when a short `Sample_Copy()` leaves
   `QueueBuffer`/`FilePending` still holding queued ring data, instead
   of always latching it false. Upstream defect: `MoreSource` is set
   true in exactly one place (`Play_Sample_Handle`'s one-time
   synchronous kickoff drain) and never again, while that same kickoff
   (reached via `Stream_Sample_Vol`/`File_Stream_Preload` for a file-
   streamed sample) unconditionally nulls `QueueBuffer`/`QueueSize`
   before draining, then only re-chains `QueueBuffer` to the next ring
   block AFTER it returns -- so the kickoff's short first-block copy
   always looked like end-of-stream and permanently killed the
   tracker, truncating every streamed Theme (the sole caller is
   `redalert/theme.cpp`'s `Play_Song`) to about one ring block (~8KB).
   Plain one-shot SFX (`Play_Sample`) never populate `QueueBuffer`/
   `FilePending`, so this is a no-op for them. Upstream candidate.
7. `redalert/init.cpp`: `Init_Bootstrap_Mixfiles()`'s three
   Bootstrap-critical `MFCD::Cache()` calls (`LOCAL.MIX`, `HIRES.MIX`,
   `LORES.MIX`) checked their result with `assert(ok)`, a no-op in
   Release builds. Upstream defect (M2 Task 5's gdb root-cause,
   reproduced independently on an isolated pre-Task-5 build too, so
   pre-existing and load-dependent, not a Task 5 regression): under
   transient memory/IO pressure, `MixFileClass::Cache()` correctly
   detects a short read and returns `false`, leaving that mixfile's
   `Data == NULL` -- but the swallowed `assert` let `Bootstrap()`
   carry on regardless, and the very next `MFCD::Retrieve()` caller
   (`Bootstrap()`'s own `memmove(&GamePalette[0],
   MFCD::Retrieve("TEMPERAT.PAL"), 768L)`, `init.cpp:2466`) dereferenced
   the NULL result and crashed (SIGSEGV, `Bootstrap() <- Init_Game() <-
   Main_Game() <- main()`).

   Fix: new `Cache_Bootstrap_Mixfile(filename)` helper (right above
   `Init_Bootstrap_Mixfiles()`) replaces all three `assert(ok)` sites.
   It calls the vendored `Emergency_Exit(1)` (already used elsewhere in
   this same file, e.g. `Init_Mouse()`, for an unrecoverable startup
   failure) after logging which mixfile failed to `stderr` -- fails
   loudly at the cache attempt itself, the closest point to the actual
   cause, instead of letting a stale NULL reach an unguarded
   `memmove()`/similar caller. Deliberately does NOT call the vendored
   `Fatal()`/`Prog_End(msg, true)` helpers: `Prog_End()` with
   `fatal=true` does `*((int*)0) = 0;` -- an intentional null-pointer
   write, presumably to trigger a platform crash-reporter under
   `REMASTER_BUILD` -- which would just relocate the same SIGSEGV
   symptom in this headless vanilla build, not fix it. Also
   deliberately does NOT use `WWMessageBox()` (the dialog-box idiom
   `Init_Mouse()` uses for its own fatal path): `Init_Bootstrap_Mixfiles()`'s
   own comment says it exists to bootstrap enough of the system
   *so that* the error dialog can be displayed, so if `LOCAL.MIX`
   itself is what failed to cache, the dialog's own font data may not
   be available yet -- a chicken-and-egg case the original code
   structure already avoided.

   The `#ifdef FIXIT_CSII` / `FIXIT_ANTS` / `WOLAPI_INTEGRATION` blocks
   earlier in the same function have the identical `assert(ok)`
   pattern on `EXPAND2.MIX`/`HIRES1.MIX`/`LORES1.MIX`/`EXPAND.MIX`/
   `WOLAPI.MIX`, and `Init_Bulk_Data()` (`init.cpp`, `CONQUER.MIX`/
   `SOUNDS.MIX`/`RUSSIAN.MIX`/`ALLIES.MIX`) doesn't check `Cache()`'s
   result at all -- left unpatched here since none of those macros are
   defined for this door's build (confirmed: absent from every
   `target_compile_definitions()` in this tree's `CMakeLists.txt`
   files, so those blocks are dead code for us) and `Init_Bulk_Data()`
   wasn't part of the crash frame this task root-caused. Worth a
   follow-up if either path is ever exercised. Upstream candidate.

   RED/GREEN evidence gathered via temporary fault injection in
   `common/mixfile.h`'s `Cache(Buffer const*)` (an env-var-gated forced
   short read on a named mixfile, `SYNCALERT_TASK5B_FAULT_INJECT`),
   fully reverted after capturing both a crashing core dump (RED,
   pre-fix) and a clean `exit(1)` with the diagnostic message (GREEN,
   post-fix) against the identical forced failure -- see
   `.superpowers/sdd/task-5-report.md`'s "Task 5b" section for the
   transcript. `mixfile.h` itself carries no diff in the final commit.

   M2 Task 4c hardened and completed this patch with two follow-on
   changes to the same function/file, both still local patches to this
   one entry (not separate numbered items, since they touch the exact
   mechanism above):

   - **Stale-tracker gate**: the re-arm condition now also requires
     `st->IsScore`. `Stop_Sample()` clears `Active`/`QueueBuffer`/
     `FileHandle` but NOT `FilePending`/`FilePendingSize`, and
     `Get_Free_Sample_Handle()` (the only place that resets `IsScore`)
     clears just `IsScore` -- so a tracker recycled from a prior
     streamed Theme into a plain one-shot SFX could carry a stale
     nonzero `FilePending` into its new life, re-arming `MoreSource`
     for the SFX and decoding the old Theme's leftover ring bytes as
     garbage into it. `IsScore` is set true only by
     `File_Stream_Sample_Vol()` and cleared only by
     `Get_Free_Sample_Handle()`, so this is a true no-op for every
     non-streaming tracker.
   - **Single ownership of the ring "stage next block" pop**:
     `File_Callback()` used to duplicate (byte-for-byte, same guard)
     the "`QueueBuffer == nullptr && FilePending` -> stage the next
     ring block" mutation that `Maintenance_Callback()`'s own tail
     block already performs -- and `File_Callback()` calls
     `Maintenance_Callback()` several times per invocation (once per
     successful disk read in its refill loop, plus once more at the
     end), so that tail block had always already run for the same
     tracker by the time `File_Callback()` reached its own copy. Kept
     as a defensive cleanup (single mutator of this bookkeeping,
     matching a reviewer-suggested direction) even though it measured
     as behaviorally neutral on its own -- the actual root cause of a
     Theme staying stuck around ~4.3s of decoded audio despite the
     `MoreSource` fix was a THIRD, independent bug, in
     `door/soundio_termgfx.cpp`, documented in that file's own header
     and in task-4-report.md's "Task 4c" section: two independent
     pollers of `SoundImp_Sample_Status()` (`Maintenance_Callback()`'s
     own reap check and `redalert/theme.cpp`'s
     `ThemeClass::Still_Playing()`) raced over the same shared edge-
     detector state, causing the Theme to restart roughly every 40ms
     long before it could ever reach end-of-track.

8. `common/soundio_common.cpp`: `File_Stream_Sample_Vol()` now decodes a
   streamed score whole-file instead of through the incremental ring
   buffer. It reads the entire `.AUD` into one contiguous buffer and runs
   it through `Play_Sample_Handle()` -- the same single-buffer path
   one-shot SFX (`Play_Sample`) already use -- instead of
   `File_Stream_Preload()` + the `File_Callback()`/`Maintenance_Callback()`
   ring machinery. Upstream defect (M2 live-test finding 4): the ring path
   desyncs the persistent SOS/IMA-ADPCM decoder across ring-block
   boundaries -- the predictor drifts into a growing DC offset and
   progressively loses the actual music (measured ~12 dB of AC lost, DC
   climbing to -9 dBFS, over a 5-minute score), so the encoded Ogg fades
   toward near-silence and loops back loud. The byte-identical codec
   decoding the SAME file from one contiguous buffer is exact -- it matches
   ffmpeg's `adpcm_ima_ws` reference bit for bit (AC flat within 0.5 dB, DC
   ~-65 dBFS), and one-shot SFX (already single-buffer) were never
   affected. `Play_Sample_Handle()` eagerly decodes to completion here
   (`SoundImp_Get_Sample_Free_Buffer_Count()` never blocks in this door),
   leaving `Remainder == 0`, so the compressed buffer is dead the moment it
   returns and is freed immediately; `Original` is cleared first so the
   freed pointer can't collide with a later `Play_Sample()` identity check.
   The door doesn't need real-time streaming -- `door/soundio_termgfx.cpp`
   accumulates the whole track and ships one looping Ogg -- and now treats
   a whole-track (>= 30s) pre-`Start_Sample()` accumulation as looping
   music with a duration-model "still playing" (see that file). This
   supersedes patch 6 and the Task-4c ring follow-ons above as the score
   decode path: those addressed *truncation* and *restart-racing* within
   the ring model; finding 4 showed the ring model itself corrupts the
   decode, so scores now bypass it entirely. (Patch 6 / the Task-4c gates
   still stand as documentation of the ring path, still compiled, and still
   correct for any future ring-streamed caller.) Fault-isolated with an
   offline harness decoding the extracted `.AUD` through this same
   `soscodec.cpp` from one buffer (perfect) vs. the live ring stream
   (drifting) vs. ffmpeg (perfect); see task notes. Upstream candidate.

Applied but NOT vendored-file patches (kept out of PROVENANCE's local-
patch list on purpose -- they're this project's own CMakeLists.txt/build
scaffolding, not edits to vendored files): `CMakeLists.txt` (top of
syncconquer/) folds `door/video_termgfx.cpp`, `door/wwkeyboard_termgfx.cpp`,
and `door/door_core.c` into the `VanillaRA` target, drops
`wwkeyboard_null.cpp` from `commonv`'s sources, and defines
`NEW_VIDEO_BUILD` for it -- all post-hoc, after `add_subdirectory(vanilla)`
returns, rather than via `common/CMakeLists.txt`'s own null branch (the
`keystone.patch` CMakeLists.txt hunk is deliberately NOT applied; see that
file's top comment). The same top-level file also works around vanilla/'s
and redalert/'s several uses of `CMAKE_SOURCE_DIR` (module path, resources/
icon build, `#include "common/..."` search path) that assumed vanilla/ was
the top-level project, which stopped being true once it became a
subdirectory here; `resources` at this directory's top is a symlink to
`vanilla/resources` for the same reason (CMake always resets
`CMAKE_SOURCE_DIR` to the true top of the source tree per directory scope,
so it can't be overridden -- each use gets its own narrow fix, documented
inline in `CMakeLists.txt`).
9. `common/ini.h` + `common/paths.cpp`: stop truncating long INI values.
   `INIClass::MAX_LINE_LENGTH` was 128, so `Read_Line()` cut every INI
   line at 128 characters, and `PathsClass::Init()` then read `[Paths]`
   into a matching `char[128]` (upstream's own `// TODO max ini line
   size.`) and dropped any value that didn't fit. The door writes
   `DataPath=`/`UserPath=` lines whose values are filesystem paths, so a
   deep-enough install silently lost its assets directory and
   `Bootstrap()` aborted on `LOCAL.MIX` -- reachable from a stock
   Synchronet tree whose `data/user/<n>/` sits under a long prefix. The
   patch raises `MAX_LINE_LENGTH` to 4224 (a `PATH_MAX` value plus its
   key) and sizes the `paths.cpp` buffer as `char[PATH_MAX]`, adding a
   `<limits.h>` include and a `PATH_MAX` fallback for toolchains that
   don't define it. Incidentally hardens `INIClass::Put_String()`, whose
   only guard against overrunning its own `char[MAX_LINE_LENGTH]` is an
   `assert()` that compiles out in Release.

10. `common/utfargs.h`: `UtfArgs`'s Windows constructor no longer replaces
    `ArgV` with a fresh `CommandLineToArgvU(GetCommandLineW(), &ArgC)` parse
    when `SYNCALERT_KEEP_CRT_ARGV` is defined (our `CMakeLists.txt` defines it
    for the `VanillaRA` target, whose `redalert/startup.cpp` is the header's
    only includer). `door_io.c`'s pre-main constructor rewrites argv in place
    -- neutering the door's own `-s<fd>` / `-home <dir>` / DOOR32.SYS drop-file
    tokens and forcing `-NOMOVIES` into a freed slot -- and upstream's
    re-derivation from the raw process command line silently discarded every
    one of those edits. Windows-only, and invisible until the door was first
    built for Windows: on *nix `main()` passes the CRT's argv straight through.
    Symptoms it caused: movies were never suppressed (the intro and mission
    FMV played, with music but no dialog audio, because `vqaaudio_null.cpp`
    stubs the VQA player's audio hooks -- see this project's `CMakeLists.txt`),
    and the door's own flags reached `Parse_Command_Line()`, whose `strstr()`
    substring scan over every token can mis-set an engine option from an
    unrelated path. `UNICODE`/`_UNICODE` stay defined, so `utf.h`'s
    `TCHARToUTF8` and `common/`'s UTF-8 path handling (`paths_win.cpp`,
    `rawfile.cpp`) are untouched; only the argv re-derivation is skipped, and
    the engine then sees the CRT's argv exactly as it does on *nix.

11. `redalert/startup.cpp`: the two `ReadyToQuit` spin-waits (`main()`'s
    `while (ReadyToQuit == 1)` on a clean exit, and `Emergency_Exit()`'s
    `while (ReadyToQuit == 3)` before its `exit(code)`) are now additionally
    gated on `!defined(NEW_VIDEO_BUILD)`. Both post `WM_DESTROY` to
    `MainWindow` and then spin calling `Keyboard->Check()` until
    `winstub.cpp`'s `Windows_Procedure()` advances `ReadyToQuit`. The headless
    door has no Win32 window (`MainWindow` is NULL) and no message pump, so
    nothing ever advances it: "Exit Game" hung forever, burning CPU in
    `Keyboard->Check()` -> `door_io_pump()` -> a non-blocking `recv()`, with
    the door process alive, the terminal still looping its music, and no
    further frames drawn. Windows-only, and upstream never compiles it: its
    Windows build always defines `SDL_BUILD`. `NEW_VIDEO_BUILD` is the right
    guard -- upstream sets it for exactly the backends (SDL1/SDL2) that have no
    legacy WndProc, so this is a no-op for every upstream configuration. The
    `ReadyToQuit = 1` store and the `PostMessage()` are left alone (a post to a
    NULL window is harmless with no pump).

12. `redalert/version.cpp` + `redalert/function.h` + `redalert/menus.cpp` +
    `redalert/goptions.cpp`: append the SyncConquer door's own version info to
    the engine version line in the menu footers. `version.cpp` gains
    `SyncAlert_Version_Name()` (right after the global `Version_Name()`), which
    formats `"<ver> <date> synchro.net"` from `SYNCALERT_VERSION` (a `#define`,
    bump per release) plus `GitCommitDate` (`common/gitinfo.h`, already
    included) -- the date is the SBBS repo's build-time commit date, since
    Vanilla Conquer is vendored inside it. It omits the git hash on purpose: it
    is drawn on the SAME line as `Version_Name()`, which already carries the
    identical short hash. `function.h` declares it next to `Version_Name()`.
    The two menu `Fancy_Text_Print` footers add it next to `Version_Name()`, so
    the engine's `r<rev> ~<hash> E` (revision + edition/expansion markers) is
    preserved and the door's `<ver> <date> synchro.net` sits with it. Placement
    differs by dialog width: `goptions.cpp`'s options box is wide, so both share
    one line (`"%s\r%s  %s"` = scenario, then engine+door version); `menus.cpp`'s
    main-menu box is narrower and the combined line overflowed its left edge, so
    the door line drops to its own second line there (`"%s\r%s"`). An earlier
    cut stacked a full second version line that overflowed onto the option
    buttons and duplicated the hash. `Version_Name()` is unchanged; display-only,
    no engine behavior change.

## Deliberate non-patches (worked around outside `vanilla/`)

Things that needed changing for the door but were solved WITHOUT touching
the vendored tree — recorded here so a re-vendor doesn't "fix" them by
patching upstream, and so the indirection isn't a surprise:

1. **Icon generation** (`redalert/CMakeLists.txt`'s `include(BuildIcons)` +
   `make_icon(INPUT "${CMAKE_SOURCE_DIR}/resources/vanillara_icon.svg")`).
   Upstream `make_icon()` `FATAL_ERROR`s when its INPUT doesn't exist, and
   `resources/` here is a **git symlink** to `vanilla/resources` — which a
   Windows checkout materializes as a plain text file (the link target as
   text) unless `core.symlinks` is on with Developer Mode. So configure
   died on Windows before compiling anything. Rather than patch the
   vendored CMake, `../cmake/BuildIcons.cmake` (ours) shadows the vendored
   module: our `cmake/` dir is prepended to `CMAKE_MODULE_PATH`, so
   `include(BuildIcons)` resolves to our no-op `make_icon()`. A console BBS
   door has no window and wants no icon, and upstream already treats the
   icon as optional (`ProductVersion.cmake`: ICON — "no icon will be
   included if not provided"; vendored `BuildIcons` only warns when
   ImageMagick is absent).

2. **Link subsystem.** `redalert/CMakeLists.txt` links `VanillaRA` with
   `/subsystem:windows /ENTRY:mainCRTStartup` under MSVC — correct for a
   desktop `vanillara.exe`, wrong for a door, which needs a console so the
   CRT hands `main()` valid std handles (otherwise all stdio, including
   fatal startup diagnostics, is silently discarded). Our `CMakeLists.txt`
   re-applies `/subsystem:console` on the target after
   `add_subdirectory(vanilla)` returns; the last `/subsystem` on the link
   line wins, so no vendored edit is needed.

## Updating

Re-vendor by diffing upstream at a new commit against this subset,
re-applying the local patches above, and updating the commit hash here.
The non-patches above live outside `vanilla/` and survive a re-vendor
untouched — but re-check them if upstream reworks its icon/CMake or link
options.

## Headless startup behavior (M1 observations)

Observed 2026-07-06 with the headless build (null video/audio/keyboard)
and real assets (REDALERT.MIX + MAIN.MIX beside the binary):

- Symptom: `vanillara` starts, opens no data files, and idles forever
  (killed by test timeouts, exit=137). This is NOT an asset-path
  problem and NOT asset-independent idling.
- Root cause: `Bootstrap()` spins in
  `do { Keyboard->Check(); } while (!GameInFocus);`
  (redalert/init.cpp:2412-2415) before any MIX registration.
  `GameInFocus` initializes to false unless `NEW_VIDEO_BUILD` is
  defined (redalert/globals.cpp:193-195), and only the SDL1/SDL2
  builds define it (common/CMakeLists.txt:204-207) -- so the null
  video build waits for a window-focus event that never comes.
- Proof: flipping `GameInFocus=1` in the live process (gdb) unblocks
  it -- the engine then opens REDALERT.MIX from the binary's
  directory (argv-path DataPath resolution works), probes the
  optional loose `hires.mix` along its search order
  (`~/.config/vanilla-conquer/vanillara/` UserPath, then DataPath,
  then cwd), and reaches the main timer loop. Asset loading headless
  is proven working.
- A blanket `-DNEW_VIDEO_BUILD` does NOT link with video_null:
  `common/wwmouse.cpp` then requires `Get_Video_Mouse(int&, int&)`,
  which only the SDL video backends implement.
- M2 implications: the termgfx video backend must define
  `NEW_VIDEO_BUILD` and implement `Get_Video_Mouse()` (pointer state
  comes from SGR mouse reports), or set `GameInFocus` itself; and
  UserPath must be redirected per-user (paths ini `UserPath` key)
  instead of `~/.config/vanilla-conquer/`.
