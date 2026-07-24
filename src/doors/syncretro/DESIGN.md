# SyncRetro -- design

A Synchronet / SyncTERM **door** that plays legacy game consoles by hosting a
[**libretro**](https://www.libretro.com/) core and rendering it to the terminal
through the shared [`../termgfx`](../termgfx) library -- the same graphics /
audio / pacing / input stack used by `../syncdoom`, `../syncduke`,
`../syncconquer`, and `../syncmoo1`.

Unlike those doors, SyncRetro vendors **no game engine of its own**. The
"engine" is an external libretro core (`FreeIntv_libretro.so`,
`fceumm_libretro.so`, ...) loaded at runtime. SyncRetro is a minimal libretro
**frontend**: it drives the core's pull-model run loop and bridges the core's
video / audio / input callbacks onto termgfx and the BBS socket.

First target: **Intellivision**, via the **FreeIntv** core. The architecture is
console-agnostic -- any software-rendered 2D core is a drop-in.

---

## 1. Why libretro is the seam

Every other door in this tree works by finding a *pluggable backend* inside a
vendored engine and rendering through it without patching upstream: 1oom's `hw`
backend (`../syncmoo1`), the DOOM / Duke headless shims, Vanilla Conquer. The
seam differs per engine, and each door re-implements the same video / audio /
input plumbing against a different internal API.

**libretro standardizes that seam across every console.** A core exposes exactly
the surface termgfx needs, as stable C callbacks:

| libretro callback            | termgfx / door side                                   |
|------------------------------|-------------------------------------------------------|
| `retro_video_refresh_t`      | convert framebuffer -> `syncretro_io` present (sixel/JXL/text, paced) |
| `retro_audio_sample_batch_t` | -> termgfx audio (see §9 -- the hard part)             |
| `retro_input_poll_t`         | drain the BBS socket, refresh the cached pad state     |
| `retro_input_state_t`        | return the cached RetroPad bit/axis                    |
| `retro_environment_t`        | frontend policy: pixel format, dirs, options, logging  |

So the plumbing is written **once**, as a frontend, and every console is just a
different core `.so`. This is strictly more leverage than a per-engine door, and
the frontend is *smaller* than one engine's shim because libretro already did
the abstraction.

The consequence for our own contract (see [CLAUDE.md](CLAUDE.md)): there is no
"ours vs. vendored source tree" line, because we vendor no engine source. We own
100% of the C here; cores stay untouched **by construction** -- we only ever see
a compiled `.so` and the public `libretro.h` API header.

---

## 2. Module map

```
main.c              frontend entry: door setup -> config -> load core -> run loop -> teardown
retro_core.c/.h     dlopen the core .so, resolve retro_* symbols, own its lifecycle
retro_env.c         the retro_environment_t callback (the RETRO_ENVIRONMENT_* switch)
retro_bridge.c      the hot callbacks: video / audio / input_poll / input_state
syncretro_io.c      terminal I/O: enter/probe/leave + the present path (termgfx compose+emit+pace)
syncretro_input.c   BBS socket decode (ESC/CSI/APC/kitty/evdev) -> a cached RetroPad state
syncretro_door.c    DOOR32.SYS / -s<fd> / -name / -home / -t session setup
syncretro_config.c  per-user sandbox + core/system/save/ROM path resolution
syncretro_quant.c/.h  truecolor frame -> 256 colors + palette, for the sixel tier
syncretro.h         cross-module contract shared by every syncretro_*.c (peer of retro_core.h)
libretro.h          VENDORED MIT-licensed libretro API header (see PROVENANCE.md)
```

`retro_core.h` is deliberately separate from `syncretro.h`: the former is the
thin wrapper over the libretro core ABI; the latter is our door-glue contract.

---

## 3. Data flow -- the pull loop

libretro is **pull-model**: the frontend calls `retro_run()` once per frame, and
*inside* that call the core synchronously invokes the video / audio / input
callbacks. The main loop (`main.c`) is therefore tiny:

```c
double fps = core.av.timing.fps;         /* FreeIntv reports 60.0 */
for (;;) {
    if (sr_door_should_exit()) break;    /* carrier drop / time limit / quit key */
    core.run();                          /* -> video_refresh + audio_batch + input_state fire */
    sr_pace_to_rate(fps);                /* keep the CORE at native speed (see below) */
}
```

### The frame-rate decision (the crux)

A BBS pipe cannot carry 60 fps of sixel. The wrong fix is to slow `retro_run()`
to match the pipe -- that pitch-shifts audio and runs the game in slow motion.
Instead:

- **Run the core at its native fps** (`sr_pace_to_rate`), so emulation timing and
  audio stay correct.
- **Emit *video* only when the pipe is ready.** `video_refresh` hands every frame
  to `syncretro_io`, but termgfx's AIMD in-flight/ACK pacing (`termgfx_aimd_update`
  / `termgfx_rtt_sample`, the same mechanism SyncDOOM uses) decides when a frame
  is actually encoded and written. Frames the pipe can't take are coalesced /
  dropped. Audio (if enabled) is never dropped.

So: native-rate emulation, frame-dropped presentation, real-time audio. This
falls out of reusing termgfx's existing pacing rather than inventing anything.

### Input model

`retro_input_poll` (called by the core inside `retro_run`) does a **non-blocking**
drain of the BBS socket via `syncretro_input`, updating a cached RetroPad state.
`retro_input_state` just returns the cached bit/axis. No threads: everything
stays inside libretro's single-threaded pull model.

---

## 4. The four bridge callbacks (`retro_bridge.c`)

```c
/* video: convert the core framebuffer, hand to the present path (which tiers+paces) */
static void video_refresh(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data) return;                              /* NULL == "dup last frame" */
    sr_fb_to_rgb(data, g_pixfmt, w, h, pitch, g_rgb);   /* 0RGB1555/RGB565/XRGB8888 -> RGB24 */
    sr_io_present(g_rgb, w, h);                     /* termgfx compose + emit, AIMD-paced */
}

/* audio: interleaved S16 stereo at the core's sample rate (see §9) */
static size_t audio_batch(const int16_t *pcm, size_t frames) {
    return sr_audio_feed(pcm, frames);
}

/* input: drain the socket once per frame, cache the pad; state reads the cache */
static void    input_poll(void)                                          { sr_input_pump(); }
static int16_t input_state(unsigned port, unsigned dev, unsigned i, unsigned id) {
    return sr_pad_get(port, dev, id);
}
```

The pixel format (`g_pixfmt`) is negotiated once via
`RETRO_ENVIRONMENT_SET_PIXEL_FORMAT`; `sr_fb_to_rgb()` is the only format-aware
code. termgfx owns tiering (JXL -> sixel -> text) and pacing, so the bridge is
just *convert + hand off*.

---

## 5. The environment callback (`retro_env.c`)

One `bool retro_environment(unsigned cmd, void *data)` switch. The commands that
matter for a terminal frontend:

| Command                          | Handling                                             |
|----------------------------------|------------------------------------------------------|
| `SET_PIXEL_FORMAT`               | accept `XRGB8888` / `RGB565`; stash for the converter |
| `GET_SYSTEM_DIRECTORY`           | the BIOS dir (per-install, sysop-supplied -- see §10). **Never NULL** -- see below |
| `GET_SAVE_DIRECTORY`             | the per-user sandbox (SRAM + save states, §11). **Never NULL** |
| `GET_VARIABLE` / `SET_CORE_OPTIONS(_V2)` | expose core options (region, controller) via door config |
| `SET_INPUT_DESCRIPTORS`          | not handled -- the help overlay is driven by our own static binding table instead (see §7); deferred to M3, for a core we know nothing about |
| `GET_LOG_INTERFACE`              | route core logs to the door log                       |
| `SET_HW_RENDER`                  | **refuse** (return false) -- software cores only (§13) |
| unknown                          | return false (core adapts / continues)                |

**The directory queries must always answer with a valid path.** libretro allows
a frontend to decline them (NULL meaning "not set"), but cores do not reliably
null-check the result: FreeIntv's `retro_init()` hands the `GET_SYSTEM_DIRECTORY`
answer to `fill_pathname_join()`, which `strlcpy()`s it -- so declining, *or*
accepting with a NULL pointer, segfaults the core before `retro_load_game()` is
ever reached. `retro_env.c` substitutes `"."` rather than pass NULL. Treat this
as frontend policy for every core, not a FreeIntv special case (CLAUDE.md: a
misbehaving core is fixed on our side, never patched).

---

## 6. Terminal I/O (`syncretro_io.c`)

Mirrors `../syncmoo1/syncmoo1_io.c`. Owns:

- **enter / probe / leave** via termgfx (`termgfx_term_enter/probe/leave`,
  capability probe through `termgfx_caps_*` + the `SGR-pixels`/DA responses fed
  back from `syncretro_input`).
- **`sr_io_present(rgb, w, h)`** -- the door's own present function (termgfx has
  no single `present()`): quantize (below), fit/center the frame to the probed
  terminal geometry (`termgfx_geom_fit` / `termgfx_geom_center`), encode via the
  active tier (`termgfx_jxl_encode` / sixel / `text.c`), and emit to the socket
  under DSR-ACK pacing. A `SYNCRETRO_SIXELOUT=<path>` capture mode (as in
  syncmoo1) writes one self-contained frame per call for offline verification
  with no live terminal.
- **teardown** that restores the BBS terminal on any exit path (`try/finally`
  equivalent in C: a single cleanup path from `main`).

Two things differ from every sibling door, both because a libretro core is not a
fixed-format engine:

1. **The frame is truecolor**, where DOOM/Duke/1oom all render into an 8-bit
   palette. Sixel is a 256-color format, so `syncretro_quant.c` reduces each
   frame first. With <= 256 distinct colors -- every legacy console -- the
   palette *is* the frame's colors and the reduction is exact; the palette is
   built sorted, so it depends only on the color *set* and stays byte-identical
   across frames, which is what lets the sixel color registers be sent once.
   Busier frames fall back to a fixed 6x6x6 cube plus a 40-step gray ramp.
2. **The frame size is not a compile-time constant** and a core may change it
   mid-session (`SET_GEOMETRY`), so every buffer grows on demand and the image
   rect is recomputed whenever the source dimensions change -- not only when a
   probe reply narrows the canvas.

The de-dupe (memcmp the source frame) is load-bearing here in a way it isn't for
an event-driven engine: a core hands us *every* frame at its native 60 fps, even
when nothing on screen moved.

---

## 7. Input mapping (`syncretro_input.c`)

RetroPad is a fixed abstract joypad (d-pad, A/B/X/Y, L/R, Start/Select, analog).
`syncretro_input` decodes the socket (ESC/CSI/APC) and maps keys onto a RetroPad
bitfield cached for `retro_input_state`.

**The key-up problem is the crux.** A RetroPad is a *held-state* device -- the
core asks "is UP down right now?" every frame -- but a terminal sends bytes on
press and *nothing* on release. Three paths, in descending fidelity (the same
ladder `../syncduke` climbs for Build's scancode queue):

| Path | Negotiation | Release edges |
|------|-------------|---------------|
| **evdev** (SyncTERM) | CTDA cap 8 -> `CSI = 1 h` + `CSI = 2 h` | yes, and the key identity is a layout-independent physical code |
| **kitty** | `CSI ? u` query -> push flags `CSI > 11 u` | yes, keyed by (layout-dependent) codepoint |
| **byte path** | none | no: a press arms a short auto-release timer that auto-repeat refreshes |

The two native paths are mutually exclusive in practice (SyncTERM doesn't speak
kitty; kitty terminals don't advertise CTDA cap 8) and the guards enforce it.
SyncTERM re-reports the keys *already held* the instant physical reports are
enabled, so press edges are dropped for a short settle window -- otherwise the
Enter still held from the BBS menu would press Start.

Whichever mode is negotiated is undone on exit (`CSI = 2 l` / `CSI = 1 l`,
`CSI < u`), as in every sibling door. One difference: they restore only on a
normal exit (their hangup `_exit()`s, assuming the socket is already dead),
whereas `sr_door_hangup()` runs the restore first, so a read-side hangup with the
write direction still open also hands the terminal back.

The interesting part is **per-console controller quirks**. Intellivision's
controller is a 12-key keypad + a 16-direction disc + three side buttons -- it
does not map cleanly to a d-pad+buttons. Rather than lean on
`SET_INPUT_DESCRIPTORS` (see §5) -- not every core sends it, and its strings
describe the core's own RetroPad convention, not the remap we impose -- M2 puts
one static binding table (`syncretro_binds.c`) behind both the key handler and
the on-screen help, so the two cannot drift apart; see
[M2_INPUT.md](M2_INPUT.md). A core that sends no descriptors at all -- MAME
2003-Plus, for the arcade console -- gets a hand-curated per-game label table
instead ([GAMES_INI.md](GAMES_INI.md)). This overlay is per-core polish, not
core plumbing.

---

## 8. Audio (`retro_bridge.c` / termgfx) -- the honest hard part

This is where SyncRetro differs most from the game-engine doors, and where M1
deliberately punts.

The engine doors emit **discrete audio events** -- SFX by id, music tracks --
which termgfx uploads to SyncTERM's APC audio cache once and triggers
(`termgfx_audio_sfx`, `termgfx_audio_music`; see `../termgfx/audio_mgr.h`). A
console core instead hands us a **continuous PCM stream** at ~44.1 kHz via
`retro_audio_sample_batch`.

termgfx has **no sink shaped like a live audio ring** -- the APC cache is built
for discrete, cache-once-and-replay assets, not a real-time stream. Streaming
emulator audio to a terminal over a BBS pipe (latency + bandwidth + no flow
control on the audio channel) looked like an open problem, not a wiring task.

**M4 solved it, and not by building a ring in termgfx.** SyncTERM's own audio
APC already provides one: `Queue` moves a slot onto a mixer channel's FIFO, and
consecutive queues play back to back -- that FIFO *is* the live audio ring this
section says termgfx lacks. `sr_audio_feed()` downmixes the core's
stereo-duplicated PCM to mono, chunks it into 100 ms pieces, and streams them as
Opus over that FIFO with a three-chunk prebuffer against jitter. See
[M4_AUDIO.md](M4_AUDIO.md) for the full design, the PSG-synthesis alternative
rejected in its §3, and the measurements the tuning knobs come from.

---

## 9. Core & content acquisition

Three kinds of external input, each with a different provenance / legal status:

1. **The core `.so`** (e.g. `FreeIntv_libretro.so`) -- GPL / freely
   redistributable. Bundled or fetched (like `../syncdoom/get-binary.js`), or
   built from the libretro core's own source. Located via a door config key /
   `-core <path>`; `dlopen`ed at startup.
2. **System BIOS ROMs** (Intellivision `exec.bin` + `grom.bin`, etc.) --
   **copyrighted; the sysop supplies a legally-owned copy.** Same situation as
   `../syncmoo1`'s MoO1 LBX files. Installed into the BIOS dir and returned to
   the core via `GET_SYSTEM_DIRECTORY`.
3. **Game ROMs** -- likewise copyrighted, sysop-supplied.

The `getdata.js` + `install-xtrn.ini` flow built for `../syncmoo1` ports over
directly (extract/copy from a copy the sysop drops into the door dir; nothing is
downloaded that isn't freely redistributable). Deferred to the xtrn/ bundle, not
part of this src skeleton.

---

## 10. Per-user sandbox & directories (`syncretro_config.c`)

Mirrors `../syncmoo1/syncmoo1_config.c`. `-home <dir>` gives a per-user sandbox
(`data/user/<num>/retro/`); the frontend `mkpath`s it and reports it to the core
as `GET_SAVE_DIRECTORY` (SRAM + save states isolate per user/node). The BIOS dir
(`GET_SYSTEM_DIRECTORY`) is shared, read-only, per-install -- by default the
door's own launch directory (`xtrn/syncivision`), which is where a sysop drops
`exec.bin`/`grom.bin` beside the core. Cartridges live in a `roms/`
sub-directory, so a bare ROM name on the command line resolves.

Everything is absolutized **before** `sr_config_apply()`'s `chdir` into the
sandbox -- the dirs, the core `.so`, *and* the ROM. This is easy to get wrong:
the chdir happens before `main()` ever calls `rc_core_open()` /
`rc_core_load_game()`, so a relative `roms/4-tris.rom` passed straight through
would be looked up inside the per-user sandbox. Cores also *keep* the directory
pointers they are handed and re-join paths onto them for the whole session.

No environment variable is required at runtime (Synchronet has no per-door env in
`xtrn.ini`); paths come from the command line / door config, with env vars
(`SYNCRETRO_SYSTEM`, `SYNCRETRO_CORE`, `SYNCRETRO_ROM`, `SYNCRETRO_SIXELOUT`)
only as dev-run overrides.

---

## 11. Door setup (`syncretro_door.c`)

Identical contract to the other doors: a `door32.sys` path argument
(auto-detected by filename) or `-s<fd>` supplies the client socket + time left;
`-name <handle>`, `-home <dir>`, `-t<seconds>`. The door probes the live
terminal geometry at connect (DOOR32.SYS carries no screen size). Reuses
`../termgfx/sbbs_node` for node status / who's-online where applicable.

---

## 12. Constraints

- **Software-rendered cores only.** `SET_HW_RENDER` is refused, so GL/Vulkan
  cores (N64, PSX-hw, ...) are out of scope -- correct for a terminal, and it
  keeps the frontend free of any GPU dependency. All legacy 2D consoles
  (Intellivision, ColecoVision, NES, SMS/Genesis, PC Engine, ...) are software.
- **libretro API version** is checked at load (`retro_api_version()`); a
  mismatch fails cleanly rather than crashing on an ABI skew.
- **No threads in the core path** -- the pull loop is single-threaded (termgfx's
  async music transcode, if ever used, stays inside termgfx).

---

## 13. Vendoring -- ours vs. the one vendored header

The only vendored artifact is **`libretro.h`**, the MIT-licensed libretro API
header (from libretro-common). It is committed, pinned, and never hand-edited;
re-vendor from upstream per [PROVENANCE.md](PROVENANCE.md) rather than patching
the committed copy. Everything else in this directory is ours (house-style
**tabs**, unlike syncmoo1's 4-space deviation -- see [CLAUDE.md](CLAUDE.md)).

---

## 14. Build

Out-of-source CMake, same shape as the sibling doors. **POSIX:** `./build.sh`,
`./build.sh debug`, `./build.sh clean`; links `../termgfx` and `xpdev`, adds
`${CMAKE_DL_LIBS}` for `dlopen`, `libjxl` optional. **Windows:** `build.bat` /
`build.bat clean` (Win32/MSVC/Release, static libjxl via a classic-mode vcpkg
prefix; `ws2_32` for Winsock) -- see M6 in sec 15 and [CLAUDE.md](CLAUDE.md).
No vendored-engine source list -- just `libretro.h` and our `*.c`. `deploy.js`
(run via `jsexec`, both platforms) installs the built binary where the lobby
looks for it -- flat at the door root on Windows, or in an `<os>-<arch>` sub-dir
on *nix (`linux-x64`, ...); it is a jsexec script so it shares the lobby's
`sv_target()` and can't disagree about that location.

---

## 15. Milestones

- **M1 -- video-only vertical slice. DONE.** dlopen FreeIntv, load a ROM, render
  frames as sixel, keyboard->RetroPad, correct enter/probe/leave + teardown.
  Audio discarded. Proves the frontend end to end on one console.

  Verified with FreeIntv end-to-end (its own halt screen decodes cleanly out of a
  `SYNCRETRO_SIXELOUT` capture), a synthetic core exercising all three pixel
  formats and both native key paths, and a fake terminal on a socketpair for the
  pacing/teardown/carrier-drop paths. The Intellivision BIOS is still needed to
  play an actual game, but it is *not* needed to exercise the frontend.
- **M2 -- input polish. Input DONE.** All twelve Intellivision keypad keys from a
  bare terminal keyboard (digits ride the RetroPad right-analog angle; 5, 0,
  Clear and Enter are button bits), one binding table driving both the key
  handler and the help screen, and door-owned pause and help screens replacing
  the core's framebuffer-drawn ones. See M2_INPUT.md. Per-core controller
  overlay ART is deferred (sysop-supplied, needs a pointer device);
  `SET_INPUT_DESCRIPTORS`-driven help moves to M3, where cores are unknown.
- **M3 -- multi-core.** Config-selected cores; dynamic core discovery; the
  `getdata.js`/`install-xtrn` content flow; JXL + text tiers verified.
- **M4 -- audio. DONE.** The core's PCM streamed to SyncTERM's audio APC as
  100 ms Opus chunks on one mixer channel's FIFO, with a three-chunk (~300 ms)
  cushion, silent chunks replayed from a single cached sample, and `A;Update`
  as the underrun signal. PSG register synthesis was considered and rejected
  (M4_AUDIO.md §3): it is deaf to the Intellivoice, and no other era-mate core
  exposes its sound registers through the public libretro ABI.
- **M5 -- core options + save states** surfaced through the door config and a
  per-user save UI, including the save-state hotkeys.
- **M6 -- Windows. DONE.** The door builds and runs on Win32 (MSVC), configured
  by `build.bat` and installed by `deploy.js` (via jsexec), in the spirit of the
  sibling doors. All of the Windows-vs-POSIX difference is confined to two files (see
  [CLAUDE.md](CLAUDE.md)'s Portability section):
    * `syncretro_plat.c` / `.h` -- the whole clock / sleep / non-blocking
      descriptor I/O / console-less-stderr seam, over xpdev. A Winsock `SOCKET`
      is not a CRT fd, so it uses `send`/`recv`/`ioctlsocket` on Windows and
      `read`/`write`/`fcntl` on POSIX; `sockwrap.h` normalizes `WSAGetLastError()`
      onto the POSIX errno values so ONE `classify()` covers both. `io.c`,
      `input.c`, `door.c`, `main.c` lost their direct syscalls and carry no
      `#ifdef`.
    * `retro_core.c` -- `dlopen`/`dlsym`/`dlclose` vs `LoadLibrary` /
      `GetProcAddress` / `FreeLibrary`. NOT routed through xpdev's `xp_dlopen()`:
      that mangles the name (`lib%s.so` / `%s.dll`, version suffixes) and cannot
      load a core by the explicit path `sr_config_core_path()` yields, so the
      shim loads the path verbatim. Cores are `.dll`, not `.so`.
    * `main.c`'s pacer -- `clock_nanosleep(TIMER_ABSTIME)` has no Windows
      equivalent, so the absolute-deadline design (what keeps it from drifting)
      was reframed into the monotonic-microsecond domain (`sr_plat_now_us`) with
      a whole-millisecond `SLEEP()` and a recheck loop absorbing the sub-ms
      rounding. `next` accumulates regardless of actual sleep, so rounding never
      becomes drift.
    * `syncretro_config.c` -- `realpath`/`access` -> dirwrap's `FULLPATH()` /
      `fexist()`; `getcwd`/`chdir` from `<direct.h>` under
      `_CRT_NONSTDC_NO_DEPRECATE`.
    * Build: `CMakeLists.txt`'s MSVC branch pulls the static libjxl from a
      classic-mode vcpkg prefix (`x86-windows-static-md`); absent it the door
      degrades to the sixel/text tiers. libsndfile (Ogg audio) is likewise
      optional -- without it `termgfx_audio_have_ogg() == 0` and the door is
      simply silent, a state the audio module already handles as first-class.
  Win32 (x86) is the one supported Windows target: a Win32 door runs on both a
  Win32 and a future Win64 Synchronet host (the DOOR32.SYS handle is
  32-bit-significant and crosses the process-bitness boundary), and a Win32 door
  loads a Win32 core `.dll`.

---

## 16. Legal

libretro API: permissive (frontend may be implemented freely). Cores: their own
licenses (FreeIntv is GPLv2+). **Console BIOS and game ROMs are commercial
content -- SyncRetro ships none of it; sourcing legally-owned copies is the
sysop's responsibility**, exactly as with `../syncmoo1`. This door's own glue is
GPL-2.0, matching the door family.
