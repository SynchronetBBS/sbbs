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
syncretro_io.c/.h   terminal I/O: enter/probe/leave + the present path (termgfx compose+emit+pace)
syncretro_input.c/.h  BBS socket decode (ESC/CSI/APC/kitty) -> a cached RetroPad state
syncretro_door.c/.h   DOOR32.SYS / -s<fd> / -name / -home / -t session setup
syncretro_config.c/.h per-user sandbox + core/system/save/ROM dir resolution
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
double fps = core.av.timing.fps;         /* e.g. Intellivision ~59.92 */
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
| `GET_SYSTEM_DIRECTORY`           | the BIOS dir (per-install, sysop-supplied -- see §10) |
| `GET_SAVE_DIRECTORY`             | the per-user sandbox (SRAM + save states, §11)        |
| `GET_VARIABLE` / `SET_CORE_OPTIONS(_V2)` | expose core options (region, controller) via door config |
| `SET_INPUT_DESCRIPTORS`          | capture the core's button labels -> input help overlay |
| `GET_LOG_INTERFACE`              | route core logs to the door log                       |
| `SET_HW_RENDER`                  | **refuse** (return false) -- software cores only (§13) |
| unknown                          | return false (core adapts / continues)                |

---

## 6. Terminal I/O (`syncretro_io.c`)

Mirrors `../syncmoo1/syncmoo1_io.c`. Owns:

- **enter / probe / leave** via termgfx (`termgfx_term_enter/probe/leave`,
  capability probe through `termgfx_caps_*` + the `SGR-pixels`/DA responses fed
  back from `syncretro_input`).
- **`sr_io_present(rgb, w, h)`** -- the door's own present function (termgfx has
  no single `present()`): fit/center the frame to the probed terminal geometry
  (`termgfx_geom_fit` / `termgfx_geom_center`), encode via the active tier
  (`termgfx_jxl_encode` / sixel / `text.c`), and emit to the socket under AIMD
  pacing. A `SYNCRETRO_SIXELOUT=<path>` capture mode (as in syncmoo1) writes one
  self-contained frame per call for offline verification with no live terminal.
- **teardown** that restores the BBS terminal on any exit path (`try/finally`
  equivalent in C: a single cleanup path from `main`).

---

## 7. Input mapping (`syncretro_input.c`)

RetroPad is a fixed abstract joypad (d-pad, A/B/X/Y, L/R, Start/Select, analog).
`syncretro_input` reuses the termgfx input decode (socket read, ESC/CSI/APC,
kitty-keyboard) and maps keys onto a RetroPad bitfield + axes cached for
`retro_input_state`.

The interesting part is **per-console controller quirks**. Intellivision's
controller is a 12-key keypad + a 16-direction disc + three side buttons -- it
does not map cleanly to a d-pad+buttons. The `SET_INPUT_DESCRIPTORS` data the
core supplies (see §5) gives the button labels, which drive an on-screen key
overlay (number row -> keypad, arrows/WASD -> disc, etc.). This overlay is
per-core polish, not core plumbing.

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
control on the audio channel) is an open problem, not a wiring task.

**M1 is video-only:** `sr_audio_feed()` accepts and discards the stream (keeps
the core happy). Audio is tracked as research (§16, M4): candidate directions
include down-sampled/opus-chunked streaming, or a "sonify only discrete events"
heuristic. Video is fully feasible today; audio is the asterisk -- do not treat
it as a launch blocker.

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
(`GET_SYSTEM_DIRECTORY`) is shared, read-only, per-install. The core `.so` path
is resolved before `dlopen`. No environment variable is required at runtime
(Synchronet has no per-door env in `xtrn.ini`); paths come from the command line
/ door config, with env vars only as dev-run overrides.

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

Out-of-source CMake, same shape as the sibling doors (`./build.sh`,
`./build.sh debug`, `./build.sh clean`). Links `../termgfx` and `xpdev`
(sockets); adds `${CMAKE_DL_LIBS}` for `dlopen`. `libjxl` is optional (degrades
to sixel/text). No vendored-engine source list -- just `libretro.h` and our
`*.c`. `./deploy.sh` installs the built binary into the door's `xtrn` dir
(a no-op on a symlink install where the live binary already symlinks the build
output).

---

## 15. Milestones

- **M1 -- video-only vertical slice.** dlopen FreeIntv, load a ROM, render frames
  as sixel, keyboard->RetroPad, correct enter/probe/leave + teardown. Audio
  discarded. Proves the frontend end to end on one console.
- **M2 -- input polish.** Per-core controller overlays (Intellivision keypad),
  `SET_INPUT_DESCRIPTORS`-driven help, save-state hotkeys.
- **M3 -- multi-core.** Config-selected cores; dynamic core discovery; the
  `getdata.js`/`install-xtrn` content flow; JXL + text tiers verified.
- **M4 -- audio research.** The streaming-PCM-to-terminal problem (§8).
- **M5 -- core options + save states** surfaced through the door config and a
  per-user save UI.

---

## 16. Legal

libretro API: permissive (frontend may be implemented freely). Cores: their own
licenses (FreeIntv is GPLv2+). **Console BIOS and game ROMs are commercial
content -- SyncRetro ships none of it; sourcing legally-owned copies is the
sysop's responsibility**, exactly as with `../syncmoo1`. This door's own glue is
GPL-2.0, matching the door family.
