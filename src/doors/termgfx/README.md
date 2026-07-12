# termgfx

A small C library of the **reusable, engine-agnostic pieces** behind Synchronet's
framebuffer game doors — render a game's framebuffer to the user's terminal,
transport it efficiently, probe what the terminal can do, and wire in the BBS
(who's-online / paging).

It was carved out of [SyncDOOM](../syncdoom/) once a second consumer
([SyncDuke](../syncduke/)) needed the same machinery, so the renderer and
transport are shared rather than copy/pasted. Any new framebuffer door (Duke,
Doom, …) links termgfx and supplies only its engine glue.

## Design

- **I/O-free.** No module here touches a socket. Each encoder/builder writes its
  bytes into a caller-owned buffer (grown via `realloc`) and returns the length;
  the door emits them through its own output path. The cap-probe helpers likewise
  only *build the query* and *parse the reply bytes* — the door owns the
  socket/poll/timeout read loop. This keeps the library portable and lets each
  door pace and buffer however it likes.
- **Game-agnostic.** Inputs are plain framebuffers — paletted indices + a 256-color
  palette (sixel/text), or RGB888 (JXL/text) — plus width/height. No engine, BBS,
  or platform types leak into the graphics API. (`sbbs_node` is the one BBS-aware
  module; only it pulls in Synchronet headers.)
- **Optional tiers degrade cleanly.** The JPEG XL encoder compiles to a real
  libjxl implementation when `WITH_JXL` is defined, else a stub that returns 0 so
  callers always link and fall back to another tier.

## Modules

| File | Provides | Purpose |
|------|----------|---------|
| `sixel.{c,h}` | `sixel_encode`, `sixel_encode_aspect` | Indexed **DECSIXEL** encoder. Color registers map 1:1 to palette indices; the palette is (re)emitted only when it changes. `*_aspect` adds a pan/pad pixel-aspect so a half-size sixel can be scaled up by the terminal. |
| `jxl.{c,h}` | `termgfx_jxl_encode` | **JPEG XL** encoder (RGB888 → codestream) via libjxl, behind `#ifdef WITH_JXL` (else a return-0 stub). Lossy, low-latency settings for game frames. |
| `apc.{c,h}` | `termgfx_apc_image` | **SyncTERM APC cached-image transport.** Builds the `C;S` *Store* (base64 payload) + `C;Draw{JXL,PPM}` *Draw* escape sequence that caches an image and blits it. |
| `caps.{c,h}` | `termgfx_query_jxl`, `termgfx_caps_parse_jxl` | **Capability cap-probe.** The `Q;JXL` query string + an I/O-free scanner for the reply, so a door learns whether the terminal can decode JXL. |
| `text.{c,h}` | `rt_config`, `rt_render_frame` | **Text / block-character render tiers** (half-block, blocks+shades, quadrant, sextant) — an ANSI fallback for terminals without sixel/JXL. RGB888 in, cell-diffed terminal bytes out. Adapted from [ludocode/doom-cli](https://github.com/ludocode/doom-cli) (GPLv2). |
| `term.{c,h}` | `termgfx_term_enter/probe/leave` | Canonical terminal control strings (clear/home, hide cursor, disable autowrap, **reset DECSDM sixel-scrolling**, the pixel-canvas probe). |
| `door32.{c,h}` | `termgfx_door32_read`, `termgfx_stderr_capture` | **The DOOR32.SYS drop file** — how a door reaches the player. Reports a *socket* (comm type 2) **or** a *stdio door* (comm type **0** = "local": no socket, the BBS redirected our stdin/stdout — Synchronet's `XTRN_STDIO`, Mystic on \*nix), and names the reason when a drop file is no use (serial, unknown type, dead handle, truncated). Lived copy-pasted in five doors, which had drifted and none of which knew what comm type 0 meant. `termgfx_stderr_capture()` is its companion: on a stdio door the BBS `dup2()`s stderr onto the player's stream, so it must go to a file. |
| `sbbs_node.{c,h}` | node list / paging helpers | Door-native access to Synchronet's online-node list and inter-node paging (who's-online / page-a-node), without an SCFG load. The only BBS-aware module. |
| `audio.{c,h}` | APC builders + Ogg/Opus encode + resample | Low-level **SyncTERM audio APC** string builders (`C;S` Store, `A;Load/Queue/Flush/Volume`), the libsndfile **Ogg/Opus** encoder (with linear up-resample of non-Opus-legal rates), and the VOC→PCM helper. I/O-free. |
| `audio_mgr.{c,h}` | stateful audio policy | The manager a door owns: capability tier, upload-once sample cache, SFX voice-steal pool, looping-SFX voices, the **music** channel (upload + loop + live volume), the door-side Ogg disk cache, and the C;L client-cache-skip. Emits through a door callback. |
| `audio_midi.{c,h}` | MIDI/MUS → PCM | ADLMIDI (OPL3) render of MIDI/MUS lumps to PCM for the music path (used by SyncDuke/SyncDOOM; SyncConquer feeds already-decoded ADPCM PCM instead). |

A door typically: emits `termgfx_term_enter` + the cap probes at start, then per
frame packs its framebuffer and calls `sixel_encode` / `termgfx_jxl_encode` /
`rt_render_frame` for the chosen tier — wrapping the image tiers in
`termgfx_apc_image` (JXL) — and `out_put`s the returned bytes itself.

## Using it

From a door's `CMakeLists.txt`:

```cmake
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(<door> termgfx)
```

The library's PUBLIC include directory is propagated to consumers, so
`#include "sixel.h"`, `"jxl.h"`, `"apc.h"`, `"caps.h"`, `"text.h"`, `"term.h"`,
`"sbbs_node.h"` just work.

For the **JPEG XL** tier, the door's CMake finds system **libjxl** (pkg-config or
`find_library`) and propagates `WITH_JXL` + the libjxl include dir to the
`termgfx` target, then links the libjxl libraries at the door's final link; the
actual encoder symbols resolve there. Without libjxl, termgfx still builds (sixel
+ text tiers) and `termgfx_jxl_encode` returns 0. See
[`../syncduke/CMakeLists.txt`](../syncduke/CMakeLists.txt) /
[`../syncdoom/CMakeLists.txt`](../syncdoom/CMakeLists.txt) for the find block.

termgfx itself links nothing from the engine/BBS/xpdev — those symbols resolve
when the door links them. (`sbbs_node.c` only needs Synchronet *headers* at
compile time, which the door's include paths already provide.)

## Status / scope

Shared today: the **encoders** (sixel, JXL, text), the **APC transport**, the
**cap-probe** parsing, the terminal control strings, and `sbbs_node`. Each door
still owns its own present/pacing loop and tier *dispatch* — unifying those
behind a single `termgfx_present()` is a planned next step, gated on reconciling
the doors' (deliberately) diverged pacing/scaling so neither regresses.

## Audio (SyncTERM APC) — gain model & hard-won lessons

The audio path (`audio_mgr` over `audio`) drives SyncTERM's `SyncTERM:A`
audio APC. A few facts about that pipeline are non-obvious and cost real
debugging time — capture them before touching door audio:

**Gain staging (know this before setting any volume).** SyncTERM opens every
APC channel at a **−12 dB stream base** (`audio_apc.c`, `AUDIO_APC_BASE_DB`).
`A;Volume` is an **absolute** channel level that **replaces** that base — it is
*not* multiplied into it — and it maxes at 0 dB (`V=100`). One-shot SFX never
send `A;Volume`, so they mix at `−12 dB + their per-Queue level`; the music
path instead queues its buffer at `VL=VR=100` (0 dB) and drives the level with
`A;Volume`. Net effect: **music at a given `A;Volume` percent sits ~12 dB above
an SFX queued at the same percent.** Then the mixer sums all streams and runs
the sum through a **tanh soft-clip** (`xpbeep.c`, `soft_clip_narrow`) that is
only transparent well below full scale. Consequences:
- Keep per-stream levels with headroom; a single stream driven near 0 dBFS
  (especially hot, already-clipping source material) hits the saturator's knee
  and adds audible harmonics *by itself*, before anything else is mixed in.
- The ~12 dB music-over-SFX imbalance is shared by **all** termgfx doors. It is
  a mix-balance question, **not** a distortion bug — don't "fix" it by slamming
  `A;Volume` down (that only makes music too quiet and can't undo distortion
  baked into the encoded track). If you want music and SFX balanced, do it
  deliberately and consistently across doors.

**Feeding decoded audio: prefer whole-file over chunked streaming.** When a
door hands `audio_mgr` PCM decoded by a vendored engine, watch for a
**persistent-state codec streamed through a chunked/ring buffer** (Westwood
AUD is IMA-ADPCM; the predictor + step index carry sample-to-sample). Any
desync at a ring-block boundary is not a one-time glitch — the predictor
**drifts and accumulates**, typically into a growing **DC offset** that
progressively swallows the real signal. It reads as loud RMS but any codec
high-passes the DC away, so the encoded track fades toward silence and the
shrinking music rides the DC into clipping. This is exactly what bit
SyncConquer (see [`../syncconquer/PROVENANCE.md`](../syncconquer/PROVENANCE.md)
patch 8): scores lost ~12 dB of music over a 5-minute track through the ring
path, while the *identical* codec decoding the same file from **one contiguous
buffer** was bit-exact against the reference decoder. Since these doors
accumulate the whole track before shipping one looping Ogg anyway, **decode
whole-file from a single buffer** — it's simpler, avoids the boundary desync,
and matches how one-shot SFX already decode correctly.

**Debugging audio artifacts — isolate the layer.** "Distortion" can enter at
the source, the decode, or the lossy encode; don't guess. Concrete probes that
paid off:
- **DC vs AC** — split the signal (a 1-pole high-pass gives the AC/music; the
  per-window mean gives the DC). A collapsing **zero-crossing rate** (a full
  window never crossing zero) with high RMS is a DC/predictor-divergence tell,
  not musical content.
- **Progressive vs constant** — envelope the track in thirds. A monotonic decay
  over the whole track points at accumulating decoder state, not a fixed gain.
- **Ground truth** — decode the source with an independent reference
  (`ffmpeg -c:a adpcm_ima_ws` for Westwood AUD) and compare. If the reference
  is clean and yours drifts, the bug is in *your* decode/streaming, not the
  source. If both clip identically, the source is genuinely hot.
- **Whole-file vs streaming A/B** — run the same bytes through your codec once
  from a single buffer and once through the live stream; divergence localizes
  the bug to the streaming layer.

**Delivery timing matters to the manager.** A whole-file decode delivers the
entire track *before* the engine's "start" call, whereas a stream dribbles it
in after. If the door's commit/duration logic assumes streaming (data arriving
after start), whole-file delivery can look like a one-shot and get reaped
immediately — SyncConquer splits on accumulated **duration** (a ≥30 s pre-start
buffer is a whole-file score → looping music with a duration model; shorter is
a one-shot SFX/voice). Mind this if you change how a door feeds `audio_mgr`.

## Terminal compatibility

termgfx leans on a handful of terminal capabilities, and support varies widely.
The renderer degrades cleanly — a terminal that lacks an image tier falls back to
the universal ANSI **text/block tiers**, and a terminal that lacks native-key
input falls back to legacy CSI keys — but the *experience* depends on what the
client actually implements. Capabilities termgfx exercises:

- **Sixel** (DECSIXEL) — the baseline image tier.
- **Sixel vertical scaling** — honoring the pixel-aspect pan/pad
  (`sixel_encode_aspect`) so a half-height sixel is upscaled *by the terminal*.
  Terminals that render sixels at their literal pixel size only ("full-size")
  show a half-height picture instead of a scaled one; the door must then emit a
  full-size sixel (more bytes, slower) for correct geometry.
- **JXL image APC** — SyncTERM's cached-image APC (`C;S` store + `C;Draw`) with a
  JPEG XL payload. SyncTERM-proprietary.
- **Audio APC** — SyncTERM's audio APC (digital SFX + OPL3 music). SyncTERM-proprietary.
- **Kitty keyboard** — the kitty keyboard protocol, for precise physical-key /
  modifier input (WASD + arrows + numpad). Distinct from SyncTERM's own **evdev**
  physical-key protocol; SyncTERM does **not** implement kitty.

### Known-terminal matrix (seed)

Legend: ✔ supported · ✘ not supported · ? untested. This is a starting point, not
a survey — expand it as terminals are actually tested.

| Terminal | Sixel | Sixel vert. scaling | JXL image APC | Audio APC | Kitty keys |
|----------|:-----:|:-------------------:|:-------------:|:---------:|:----------:|
| **SyncTERM** (baseline target) | ✔ | ✔ | ✔ | ✔ | ✘ (uses evdev) |
| **xterm** (v390) | ✔ | ✘ (full-size only) | ✘ | ✘ | ✘ |
| **foot** (1.62.2) | ✔ | ✔ | ✘ | ✘ | ✔ |
| **Contour** (0.6.3.8249) | ✔ | ✔ | ✘ | ✘ | ✔ |
| **WezTerm** (Windows, 20260627-110829) | ✔ | ✘ (full-size only) | ✘ | ✘ | ✘ |

Notes:
- **xterm 390** renders sixels only at full size — it has no client-side vertical
  scaling — so the aspect-scaled half-height path (`sixel_encode_aspect`) doesn't
  size correctly there; a full-size sixel is required for proper geometry.
- **WezTerm** (verified on Windows, build 20260627-110829-e46fe38c) does Sixel but likewise ignores the pixel-aspect
  scaling — it renders full-size only, so it needs the full-size sixel path (the
  door's `SD_SIXEL_FULL` tier) for correct geometry — and does **not** implement the
  kitty keyboard protocol (falls back to legacy CSI keys).
- **foot 1.62.2** and **Contour** both do kitty-keyboard input and sixel *with*
  vertical scaling.
- **JXL image APC** and **Audio APC** are SyncTERM-specific transports; no
  third-party terminal implements them, so those columns are ✘ for everything but
  SyncTERM.
- Contour's numpad / shift-modified **kitty key encodings** still have quirks under
  investigation (they differ from foot's); the graphics + base kitty-keys work, so
  it's in the matrix — the encoding quirks are a separate input-parser issue.
- **Windows Terminal 1.25** is also a kitty-keyboard terminal but has its own
  numpad / shift-modified key quirks; left out of the matrix until characterized.

### Toward a fuller matrix

The intent is to grow this into a proper compatibility/capability matrix in the
spirit of <https://wiki.synchro.net/resource:term>, but scoped to the specific
capabilities termgfx uses (the columns above) rather than general terminal
features — so a sysop can tell at a glance which client gives the best experience
for these doors. That includes calling out the **SyncTERM-specific** capabilities
as their own dimensions — key events (its evdev physical-key protocol), the JXL
image APC, and audio playback (the audio APC) — since those are what set the
baseline target apart from the third-party terminals.

## License

`text.c`/`text.h` are adapted from ludocode/doom-cli (GPLv2). The remaining
modules were extracted from the Synchronet doors and follow the Synchronet
project license; in practice termgfx is consumed by GPL doors (Duke Nukem 3D /
Doom source). See the consuming door's README for the full attribution.
