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
| `sbbs_node.{c,h}` | node list / paging helpers | Door-native access to Synchronet's online-node list and inter-node paging (who's-online / page-a-node), without an SCFG load. The only BBS-aware module. |

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

## License

`text.c`/`text.h` are adapted from ludocode/doom-cli (GPLv2). The remaining
modules were extracted from the Synchronet doors and follow the Synchronet
project license; in practice termgfx is consumed by GPL doors (Duke Nukem 3D /
Doom source). See the consuming door's README for the full attribution.
