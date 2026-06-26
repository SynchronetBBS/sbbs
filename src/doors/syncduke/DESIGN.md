# SyncDuke — design spec (v1: single-player first light)

Date: 2026-06-24
Status: approved design; implementation not started.

## Summary

Port Duke Nukem 3D to a Synchronet terminal door, mirroring SyncDOOM: a vendored
Build-engine snapshot whose platform layer is replaced by a headless shim that
produces an 8-bit framebuffer, rendered to the user's terminal via the shared
`libtermgfx`. v1 is the smallest thing that proves "Duke plays over SyncTERM."

## Goals (v1)

- Chocolate Duke3D (Build engine + Duke game) building headless, in-tree.
- A headless platform shim: framebuffer/palette out, terminal input in, our
  timer, no audio.
- Render the 320x200 8-bit frame to the terminal via `libtermgfx`'s sixel encoder.
- Keyboard controls (movement / fire / use / menu).
- Shareware Episode 1 (*L.A. Meltdown*) playable over SyncTERM.

## Non-goals (v1) — explicit

Single-player only; sixel-only; keyboard-only; no JS lobby; no multiplayer; no
JXL/text tiers; no audio. These layer on afterward, the way SyncDOOM grew.

## Engine choice

**Chocolate Duke3D** — faithful, minimal, classic-8-bit-software-renderer-only;
the Build parallel to Chocolate Doom, which doomgeneric (and thus SyncDOOM) is
built on. Vendored as a **pinned snapshot in-tree** (we never track upstream),
exactly as the vendored doomgeneric / Chocolate Doom. Unmaintained since 2019, so
expect a one-time pass to build on modern GCC (structured for MSVC later).

EDuke32 was rejected: large/complex (OpenGL renderers, CON scripting, mapster32),
the framebuffer point is deeply buried, and its active-maintenance edge is moot
for a pinned vendor snapshot.

## Architecture

```
src/doors/syncduke/
  <vendored Build engine + Duke game sources>   # pinned Chocolate Duke3D snapshot
  syncduke.c        # our backend: the headless platform shim + door glue
  CMakeLists.txt    # links libtermgfx + xpdev; parallels syncdoom
```

Links `libtermgfx` (`src/doors/termgfx/`). Mirrors `src/doors/syncdoom/`. As
`syncdoom.c` is doomgeneric's backend, `syncduke.c` is the Build engine's backend.

## The shim (replaces Chocolate Duke3D's SDL baselayer)

- **Video** — intercept the Build engine's page-flip; read its 8-bit indexed
  framebuffer + 256-color palette (scale VGA 6-bit → 8-bit RGB). That is exactly
  `sixel_encode()`'s input (indexed pixels + 768-byte palette), so the render
  path is short. Emit to the terminal socket.
- **Input** — read terminal bytes from the door socket (xpdev sockwrap, as
  syncdoom does) → map to Duke scancodes (v1: movement, fire, use, menu).
- **Timer** — drive the Build clock from our loop.
- **Audio** — stubbed out (silent; no audio over a BBS terminal).

Duke-specific for now — **not** a generic "buildgeneric" abstraction (YAGNI; we
can generalize if Blood / Shadow Warrior ever come up).

## termgfx reuse / extraction coupling

v1 consumes `libtermgfx`'s `sixel_encode` (the only renderer module extracted so
far) plus the door socket I/O. The JXL/text tiers, the frame-pacing controller
(AIMD/DSR), the terminal-capability probe, and the overlay/banner still live in
`syncdoom.c`; those are pulled into `termgfx` **incrementally after v1, driven by
SyncDuke as the second consumer** — which is what finally validates the `termgfx`
interface against two real users (the whole rationale for the library). SyncDuke
both *uses* termgfx and *pulls its extraction forward*.

## Data

Shareware **DUKE3D.GRP** (Episode 1) — freely redistributable; fetched/installed
like the WADs, **not committed** to the repo. Ship a `syncduke.example.ini` + an
`install-xtrn` entry, mirroring syncdoom.

## Build

CMake, parallel to syncdoom: vendored engine sources + `syncduke.c`, link
`libtermgfx` + `xpdev`; JXL not needed in v1. Expect a one-time modern-toolchain
cleanup of the 2019 Chocolate Duke3D sources.

## First implementation task (the main unknown)

Pull the Chocolate Duke3D source and identify (a) the page-flip / screen-present
function and the framebuffer + palette globals to intercept, and (b) how cleanly
the SDL baselayer separates from the engine — this determines the shim's exact
seam. Everything else follows the SyncDOOM template.
