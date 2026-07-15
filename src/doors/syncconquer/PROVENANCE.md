# Vendored source provenance

## vanilla/ — Vanilla Conquer

- Upstream: https://github.com/TheAssemblyArmada/Vanilla-Conquer
- Commit: 7f351daed0c19d7c4764dc4ebae1a70c7809ac1f (2025-11-14)
- License: GPLv3 with EA additional terms (vanilla/License.txt)
- Vendored subset: CMakeLists.txt, CMakePresets.json, License.txt,
  README.md, cmake/, common/, redalert/, tiberiandawn/, resources/
- Not vendored: tests/, tools/, scripts/, .github/

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

13. `redalert/sounddlg.cpp` + `redalert/options.cpp`: two music/sound-volume
    fixes for terminal play. (a) `sounddlg.cpp` raises `MSliderHeight`/
    `FXSliderHeight` from `5 * factor` to `8 * factor` -- the Options sound
    sliders were a 10px-tall band at 640x400 (RESFACTOR=2), narrower than one
    text-cell row, so a cell-granular mouse (SyncTERM, no SGR-Pixels) could
    click between them and miss; 8 base = one full 16px cell row so a click
    always lands. (b) `options.cpp` adds `extern "C" void
    SyncAlert_Music_Volume_Step(int dir)` right after `Set_Score_Volume()`,
    which nudges `Options.ScoreVolume` ~10% and applies it via the same
    `Set_Score_Volume(..., true)` the slider uses -- called from the door's
    `+`/`-` hotkeys (door_io.c door_io_hotkey + door_input.c evdev path) so
    music volume is adjustable without the mouse. Source of truth stays
    `Options.ScoreVolume` (soundio_termgfx.cpp already drives
    `termgfx_audio_music_volume` from it), so the hotkeys and slider agree.

14. `redalert/init.cpp`: `Anim_Init()` now also sets
    `AnimControl.OptionFlags |= VQAOPTF_AUDIO`. It set `VQAOPTF_CAPTIONS |
    VQAOPTF_EVA` but never enabled audio, so the VQA loader decoded no audio
    and the FMV cutscenes were silent (the movie's own audio-enable, in
    `Open_Movie()`, is in an `#if (0)` upstream block). With audio decoding on,
    the new door-side backend `door/vqaaudio_termgfx.cpp` (which replaces the
    vendored `common/vqaaudio_null.cpp` stub via the door CMake -- see the
    "Deliberate non-patches" note below) ships each decoded block to the client
    through termgfx's streaming-audio path, giving the cutscenes their dialog.
    One-line additive change; no other engine behavior affected.

15. `redalert/sounddlg.cpp`: the Sound Controls dialog now re-syncs its music
    slider to `Options.ScoreVolume` at the top of its processing loop. The
    door's `+`/`-` music-volume hotkeys (patch 13) call
    `Options.Set_Score_Volume()` directly, but the dialog only seeded the
    gauge once on entry and redrew it from its own drag events, so a hotkey
    change wasn't reflected until the menu was re-entered. Each loop pass now
    pushes ScoreVolume into the gauge only when ScoreVolume ITSELF changed
    since the previous pass (tracked in `last_score`) -- NOT when the gauge
    differs from ScoreVolume, which would fight the mouse: during a click or
    drag the gauge leads ScoreVolume (it commits below on SLIDER_MUSIC), so a
    gauge-vs-ScoreVolume test snapped the thumb back. Triggering on a real
    ScoreVolume change leaves the mouse free and still catches the hotkeys.

16. `redalert/scroll.cpp`: `ScrollClass::AI()` widened the edge-scroll trigger
    from a literal 1-pixel border to an `8 * RESFACTOR` band on all four sides
    (`at_screen_edge`). Upstream only scrolls when the mouse is at exactly
    x==0/y==0/width-1/height-1, which relies on a real mouse being clampable to
    the exact edge pixel. Over a terminal the pointer is cell-quantized (the
    door maps SGR cell reports to cell centres) and can't hit those pixels --
    the top/left especially, which the door can't snap to 0 without breaking
    top-row menu-button clicks -- so edge-scroll was nearly unusable. The band
    lets the outermost cell centre engage scrolling. Only affects the scroll
    trigger region; no click coordinate is moved, so menus are unaffected.

17. `redalert/options.cpp`: music-volume (ScoreVolume) persistence. Two parts.
    (a) `Save_Settings()` now writes `ScoreVolume` unconditionally; upstream
    (under `FIXIT_VERSION_3`) wrote it only when `Session.Type == GAME_NORMAL`,
    so a change made from the main menu or a Skirmish game saved only
    `MultiplayerScoreVolume` -- but `Load_Settings()` always applies
    `ScoreVolume`, so music volume reset to the 0.25 default every run (FX
    `Volume` has no such gate, which is why it alone persisted). (b) The door's
    `+`/`-` hotkey (`SyncAlert_Music_Volume_Step`, patch 13) now calls
    `Options.Save_Settings()` after adjusting, and no-ops at the min/max rail so
    held key-repeat doesn't rewrite the INI. The slider path relied on the
    options-menu Resume to save, which the hotkey never reaches. Together the
    music level now survives a restart from any context.

18. `redalert/menus.cpp` (`Main_Menu()`) and `redalert/egos.cpp` (the credits
    scroll) no longer override the music volume. Both saved `Options.ScoreVolume`
    and, when it was 0 (music muted), forced it to 0.4 so their theme
    (`THEME_INTRO` / `THEME_CREDITS`) stayed audible, restoring the saved value
    on exit. In the door that meant a player who muted music heard it "resume"
    at 0.4 on the main-menu title screen (and again when its idle attract fell
    through to the credits); the restore-on-exit could also discard a volume
    change made while there. All music playback is already gated on
    `ScoreVolume != 0` (`theme.cpp` `Play_Song`), and these were the only two
    non-commented calls that set it non-zero, so removing the
    save/override/restore in both makes a mute fully respected (silent), while
    non-muted volumes were unaffected (they carried over via the normal
    score-volume scaling).

19. `redalert/init.cpp`: starting a Skirmish/Internet/IPX game no longer
    overwrites the user's music volume. Under `FIXIT_VERSION_3` the game-start
    transition did `Options.ScoreVolume = Options.MultiScoreVolume` at all three
    sites (the multiplayer/skirmish music level is a separate setting upstream),
    so a player who muted music (`ScoreVolume = 0`) heard it return the moment a
    Skirmish began -- the intro respected the mute, then the game un-muted it.
    Flipped to `Options.MultiScoreVolume = Options.ScoreVolume` (the same
    direction `sounddlg.cpp` already syncs), so the door's single music-volume
    setting is authoritative everywhere and the MP level just follows it. This
    was the actual cause of "music starts after the opening and into gameplay";
    patches 17 (save) and 18 (menu/credits 0.4 override) covered the other
    screens.

20. `redalert/startup.cpp`: the exit-time settings re-save no longer clobbers
    what the game persisted during play. After `Main_Game()` returns, RA does
    `ini.Load(cfile); Settings.Save(ini); ini.Save(cfile)` to persist any
    video-settings changes -- but it reused the SAME `INIClass ini` loaded at
    startup, and `INIClass::Load()` MERGES rather than replaces, so the reload
    kept the startup snapshot of `[Options]`. `ini.Save()` then wrote those
    stale values back over the file a moment after `OptionsClass::Save_Settings`
    had written the real ones -- so a music-volume change (or any in-game
    options change) was silently reverted to its startup value on exit, i.e.
    never persisted across runs. Root-caused with a backtrace + write-verify
    diagnostic: `Save_Settings` wrote `ScoreVolume=0` and a re-read confirmed
    it, then the file flipped back to the startup value ~2s later (CRLF = RA's
    write) from this exit save. Fixed by loading the exit save into a FRESH
    `INIClass` so it reflects the on-disk state. This was the dominant blocker
    behind the volume-persistence symptoms (patches 17-19 addressed real but
    secondary save-gate/override/MP-split issues).

--- Tiberian Dawn (syncdawn) -------------------------------------------------

The following patches support the second title, syncdawn (Tiberian Dawn),
built from `tiberiandawn/` with the `SYNCCONQUER_TD` compile define. The
whole `tiberiandawn/` subdir is vendored at the same pinned commit as the
rest; these are the local edits on top of it (plus a couple of shared
`common/`/`redalert/` files the two titles both use).

21. **syncdawn hook neutralization (per-title music-volume hotkey).** The
    door's +/- music-volume hotkey calls an `extern "C"` engine function.
    `redalert/options.cpp` originally named it `SyncAlert_Music_Volume_Step`;
    renamed to the game-neutral `SyncConquer_Music_Volume_Step` in both
    `redalert/options.cpp` (RA: `Options.Set_Score_Volume(fixed(v,256), true)`)
    and a NEW hook added to `tiberiandawn/options.cpp` (TD: `ScoreVolume` is a
    `unsigned char` 0..255, `Set_Score_Volume(int)`), and the call site in
    `door/door_io.c`. So the one shared door TU links against either engine.

22. **syncdawn: enable FMV cutscene audio (TD).** `tiberiandawn/init.cpp`'s
    `Anim_Init()` set `VQAOPTF_CAPTIONS | VQAOPTF_EVA` but left `VQAOPTF_AUDIO`
    off (commented, inside a `#if (0)` block) -- identical to Red Alert before
    patch #14. Added `AnimControl.OptionFlags |= VQAOPTF_AUDIO;` so the VQA
    loader decodes audio and `door/vqaaudio_termgfx.cpp` ships the cutscene
    dialog to the terminal. The TD equivalent of patch #14. (TD launches
    WITHOUT RA's bootstrap-stall bug -- its `globals.cpp` hardcodes
    `GameInFocus = true`.)

23. **syncdawn: load fonts/palette from the cached mixes, with a fallback for
    fonts the freeware data omits.** `tiberiandawn/init.cpp`'s `Init_Game()`
    loaded the game fonts (`Font6Ptr`, the hi-res `MapFontPtr`/`Green12*`/
    `ScoreFontPtr`) and the default `TEMPERAT.PAL` palette via stock
    `Load_Alloc_Data(CCFileClass(...))`, which cannot read an already-**cached**
    mix -- and the door caches CCLOCAL.MIX (which holds those fonts) at
    bootstrap, so the stock open fails "CD not found" and the engine's fatal
    `Prog_End()` does `*((int*)0)=0`. Switched those reads to `MFCD::Retrieve()`
    (reads the cached mix directly), matching Red Alert's `Init_Fonts()`. The
    palette copy is NULL-guarded. Additionally, the freeware C&C95 "Gold" data
    ships NO 12-point green fonts (`12GREEN.FNT`/`12GRNGRD.FNT` absent from
    every mix -- verified `MFCD::Offset()==0`), so the hi-res branch falls back
    to the 6-point equivalents when a 12-point font is missing -- exactly what
    the engine's own low-res branch already does -- instead of a NULL-font crash
    at the first in-game score/sidebar text draw.

24. **syncdawn: keep the door's game-data dir on the engine file-search path.**
    `tiberiandawn/init.cpp`'s `Init_Game()` opens its mixfiles via `CCFileClass`,
    which searches the engine CWD plus the `CDFileClass` search drives. The door
    `chdir()`s the engine to the per-user `-home` dir (savegames + `CONQUER.INI`),
    NOT where the shared read-only MIX data lives; RA bridges this via its
    `PathsClass` DataPath, but TD's engine never calls `Paths.Init()` OR
    `Parse_Command_Line()` in this build. So `Init_Game()` reads the door's
    already-resolved assets dir directly -- via a new `extern "C"` accessor
    `door_engine_data_dir()` (`door/door_io.c`, returns the `-assets` dir) --
    and `CDFileClass::Add_Search_Drive()`s it before the first mix is opened.

    ALSO `common/cdfile.cpp` (`CDFileClass::Refresh_Search_Drives()`): every CD
    check (`Force_CD_Available()` -> `Change_Local_Dir()`) calls this, which
    `Clear_Search_Drives()`es the list and rebuilds it from `Paths.*_Path()` --
    wiping the door's assets dir. It runs right after the bootstrap mixes load
    and again on every in-game theater/scenario load, so without re-adding it
    here only the ~4 bootstrap mixes stayed reachable and everything registered
    afterwards (CONQUER, TRANSIT, SOUNDS, theaters, ...) silently failed to open
    -- e.g. `Choose_Side()` crashed in `Play_Sample(STRUGGLE.AUD)` because
    TRANSIT.MIX never registered. Fix: re-add `door_engine_data_dir()` at the
    end of every refresh. No-op ("") for a pure-vanilla build.

25. **syncdawn: widen the Sound Controls volume sliders for a cell-granular
    mouse.** `tiberiandawn/sounddlg.cpp`: the music and FX slider tracks were
    `5 * factor` px tall = 10px at 640x400. SyncTERM reports the mouse at
    character-cell granularity (~16px rows), so a click missed a 10px track.
    Bumped both to `8 * factor` = 16px = one cell row. Identical to Red Alert's
    patch #13.

26. **syncdawn: door version line in the menus.** `tiberiandawn/init.cpp` gains
    `SyncDawn_Version_Name()` (`#define SYNCDAWN_VERSION "v0.1"`), formatting
    `"v0.1 <git-commit-date> synchro.net"` from `common/gitinfo.h` (the SBBS
    repo's git state at build time -- Vanilla Conquer is vendored in it). Drawn
    in `menus.cpp` (main-menu footer -- the engine `VersionText` and the door
    line CENTERED as two stacked rows, raised one row off the dialog's bottom
    edge, like Red Alert's menu) and `goptions.cpp` (options footer, same line
    as `VersionText`, shifted left off the box edge so it clears the buttons),
    declared in `externs.h`. Mirrors Red Alert's patch #12
    (`SyncAlert_Version_Name`); the engine `VersionText` (`r<rev> ~<sha>`) is
    left intact, this only adds the door's own line.

27. **syncdawn: widen the Scroll Rate slider for a cell-granular mouse.**
    `tiberiandawn/gamedlg.cpp` (Game Controls dialog): the Scroll Rate slider's
    height `d_scroll_h` was `6 * factor` = 12px at 640x400, under SyncTERM's
    ~16px mouse cell. Bumped to `8 * factor` = 16px = one cell row so a click
    lands on it. (The sibling Game Speed slider was left at 12px -- it proved
    hittable in practice.) Same rationale as patches #13/#25.

28. **Widen the Visual Controls sliders for a cell-granular mouse (both
    titles).** `redalert/visudlg.cpp` and `tiberiandawn/visudlg.cpp`: the four
    Visual Controls sliders (Brightness / Color / Contrast / Tint) share one
    height (`SliderHeight` / `Slider_Height`), which was `5 * factor` = 10px --
    unhittable with a cell-granular (SyncTERM) mouse. Bumped both to `8 * factor`
    = 16px; the inter-slider spacing (`11 * factor` = 22px) already leaves room,
    so no layout overflow. (Also incidentally fixes a pre-existing visual
    artifact on these RA sliders that the door's `AllowHardwareBlitFills=false`
    fix -- software fills only -- had already resolved.)

29. **Keyboard map scrolling + edge-scroll pacing (`redalert/scroll.cpp`).**
    `ScrollClass::AI()` gains two things. (a) The mouse edge-scroll auto path
    now moves `_rate[rate] / 4` per tick instead of the full `_rate[rate]`:
    the door runs several game ticks per presented frame, so a full step per
    tick made the map fly; a quarter-cell accumulates into a smooth glide.
    (b) Keyboard scrolling -- the arrow keys scroll one cell N/S/E/W and
    Home/End jump the view to the map's top/bottom edge. Held keys scroll
    continuously by polling `Keyboard->Down()` (kitty/evdev report key
    releases, so the hold state is real); a legacy tap terminal that reports
    no key-up scrolls one cell per received tap instead. The keys are consumed
    (`input = KN_NONE`) in the map-input AI chain, ahead of any
    `Keyboard_Process()`, so behavior is identical in both doors and the arrows
    don't also drive Tiberian Dawn's sidebar-scroll binding. Extends patch #16.

30. **Keyboard map scrolling (`tiberiandawn/scroll.cpp`).** The keyboard-scroll
    half of patch #29 mirrored into TD's `ScrollClass::AI()` verbatim (arrows +
    Home/End + held-key `Down()` polling, consumed ahead of `Keyboard_Process`).
    TD's mouse edge-scroll is left as upstream (the 1-pixel border), which makes
    keyboard scroll the more useful path there.

31. **Keyboard side selection in `Choose_Side` (`tiberiandawn/intro.cpp`).**
    Stock `Choose_Side()` is mouse-only: you click the GDI or Nod emblem. Added
    keyboard control to the selection loop -- Left/Right pick GDI/Nod directly
    (matching their on-screen left/right layout), Tab toggles a highlighted
    side, and Enter/Space confirms it (GDI default). Since the door can't warp a
    terminal cursor for feedback, the highlighted side is outlined with a
    `Draw_Rect(WHITE)` on the composed low-res buffer (gated by a flag so a
    mouse-only player sees no change). Release edges are ignored (the `0x10FF`
    key mask drops `WWKEY_RLS_BIT`, so a Tab release would otherwise read as a
    second Tab press and toggle the highlight back). The GDI/Nod branches are
    factored into shared `chose_gdi`/`chose_nod` flags so mouse and keyboard run
    identical selection code.

32. **Guard the legacy `Winsock` shutdown behind `WINSOCK_IPX`
    (`tiberiandawn/winstub.cpp`).** The MSVC/Win32 build of `syncdawn` failed to
    compile: `error C2065: 'Winsock': undeclared identifier`. `TcpipManagerClass
    Winsock` is declared only in `common/fakesock.h`, which is entirely wrapped
    in `#ifndef _WIN32` — so on Windows the object does not exist, and
    `commonv` defines `WINSOCK_IPX` publicly (`common/CMakeLists.txt`) for
    exactly that reason. RA's `winstub.cpp` already fences every legacy
    `Winsock` call in `#ifndef WINSOCK_IPX`; TD's `WM_DESTROY` emergency-shutdown
    path did not, so it referenced the object unconditionally. Wrapped TD's two
    `Winsock.Get_Connected()` / `Winsock.Close()` lines in the same
    `#ifndef WINSOCK_IPX` guard, mirroring RA verbatim.

    Upstream never hits this because its Windows build always defines
    `SDL_BUILD`, which skips this whole non-SDL `WndProc` (the same code path
    whose Win16-era 32-bit assumptions force the door to Win32-only — see
    COMPILING.md). The door build is the first thing to compile it. TD's other
    `Winsock` reference (`Message_Handler`, the same file) is already behind
    `#ifdef FORCE_WINSOCK`, which nothing defines, so it needed no change.

33. **No Win32 window, and no spin-wait on its message pump
    (`tiberiandawn/startup.cpp`).** Patch #11's two premises — the headless door
    has *no Win32 window* and *no message pump* — were never ported to TD, and
    both bit the Windows build:

    - **The stray window.** TD's `main()` called `Create_Main_Window()` under a
      bare `#if defined(_WIN32) && !defined(SDL_BUILD)`. RA calls it too, but
      passes `command_show = 0` (`SW_HIDE`), so RA's window is created and never
      shown. TD instead derives `command_show` from `GetStartupInfo()`
      (defaulting to `SW_SHOWDEFAULT`) and then does `ShowWindow()` +
      `UpdateWindow()` + `SetFocus()` on a `WS_EX_TOPMOST | WS_POPUP |
      WS_MAXIMIZE` window — a fullscreen always-on-top "Command & Conquer" popup
      on the *BBS machine's* desktop, stealing focus, for a door whose video goes
      to the caller's terminal. Now gated on `!defined(NEW_VIDEO_BUILD)`:
      `MainWindow` stays NULL, which is what the rest of the door already assumes.

      Its title rendered as CJK glyphs, which is worth recording because it is
      *not* our bug and would resurface if the window ever came back: both
      winstubs register the class with `RegisterClassA` and pass an ANSI title to
      `CreateWindowExA`, but fall through to the `DefWindowProc` **macro**, which
      is `DefWindowProcW` under the `UNICODE` build. `WM_NCCREATE`'s default
      handler then reads `"Command & Conquer"` as UTF-16. Upstream never sees it:
      its Windows build defines `SDL_BUILD` and compiles none of this.

    - **The exit hang.** `main()`'s `ReadyToQuit` spin-wait (`PostMessage(WM_DESTROY)`
      then `do { Keyboard->Check(); } while (ReadyToQuit == 1);`) was ungated, so
      with no pump nothing could ever advance `ReadyToQuit` 1 -> 2 and "Exit Game"
      spun forever, burning CPU — exactly the hang patch #11 fixed for RA. Gated
      on `!defined(NEW_VIDEO_BUILD)` the same way. (TD has only the `main()`
      wait; it has no `Emergency_Exit()` `ReadyToQuit == 3` counterpart.)

    Safe with a NULL `MainWindow`: TD's remaining uses are `MessageBoxA(NULL, …)`
    on fatal paths, the harmless `PostMessage()` above, multiplayer-only
    `netdlg.cpp` (the door is single-player), and `common/vqaloader.cpp`'s
    `VQA_OpenAudio(handle, MainWindow)` — whose `hwnd` the door's
    `vqaaudio_termgfx.cpp` ignores. TD's `GameInFocus` is already unconditionally
    `true` (RA needed patch-forcing; TD does not), so losing the window's
    `WM_ACTIVATEAPP` does not silence TD's audio.

34. **syncdawn: re-sync the Sound Controls music slider to external
    volume changes (`tiberiandawn/sounddlg.cpp`).** The door's `+`/`-`
    music-volume hotkeys call `Options.Set_Score_Volume()` directly (patch
    #13), but the Sound Controls dialog only set the music slider's value once
    at open, so pressing `+`/`-` with the dialog up changed the volume audibly
    while the thumb stayed put. Added the RA fix (patch #15): track
    `last_score = Options.ScoreVolume` and, each loop iteration,
    `music.Set_Value()` + redraw when `ScoreVolume` changed. Triggers on a
    change to `ScoreVolume` (not gauge != ScoreVolume) so a mouse drag isn't
    snapped back mid-drag. TD uses `ScoreVolume` directly (no RA `* 256` fixed
    scaling). The RA equivalent was patch #15; this is its TD counterpart, which
    had been missed.

35. **Force a full tactical redraw on scroll (`redalert/display.cpp` +
    `tiberiandawn/display.cpp`).** `DisplayClass::Draw_It()` scrolls the map by
    reusing already-drawn pixels: it shifts the replicable block via an
    overlapping self-blit (`HidPage.Blit(HidPage, ...)`) or, for a DirectDraw
    hid page, a `SeenBuff.Blit(HidPage, ...)`, then redraws only the newly
    exposed edge. Neither moves content correctly in the door's plain software
    framebuffer, so any scroll (most visibly the Home/End jump, patch #29/#31)
    left **staircase remnants of shifted sprites + black gaps** in the 640x400
    buffer -- transport-independent (identical under sixel and JXL). Set
    `forced = true` as soon as a scroll is detected, so the blit-shift branch is
    skipped and the engine's own full-redraw path (its existing `else` fallback,
    used for full-size scrolls) runs instead. Same class as the
    `AllowHardwareBlitFills` FillRect no-op (a non-patch, worked around in
    `door/video_termgfx.cpp`); done as a vendored edit here because the branch
    is inside `Draw_It()` with no external hook. Costs the door nothing -- it
    re-encodes the whole frame every present regardless, so the partial-redraw
    optimization saved it none.

36. **Gate FMV playback on the door at play time (`redalert/conquer.cpp` +
    `tiberiandawn/conquer.cpp`).** `Play_Movie()` gains, right after the existing
    editor/`bNoMovies` guards, `if (door_movies_suppressed()) return;` (declared
    `extern "C" int door_movies_suppressed(void);`, defined in `door/door_io.c`).
    This implements the `<door>.ini [game] movies` tri-state: `true` always
    plays, `false` never, and the default *auto* plays a cutscene only when the
    client can actually play audio (`termgfx_audio_tier() == 1`) -- a silent FMV
    is a poor experience and a lot of bandwidth. It has to be decided at play
    time because the audio capability resolves a few client round-trips into the
    session, long after startup where `-NOMOVIES` is set. Done as a vendored edit
    because `Play_Movie()` is the only movie chokepoint and has no external hook.
    Side benefit: TD never read `bNoMovies` (RA-only global), so `[game] movies =
    false` was a no-op for TD before -- this gate makes it work for both titles.

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
