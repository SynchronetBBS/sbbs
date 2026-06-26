# SyncDuke — vendored engine + platform seam

## Provenance

- Upstream: **Chocolate Duke3D** — `https://github.com/fabiensanglard/chocolate_duke3D`
- Pinned revision: **`1cfa1909aa1f5a5ab159301044ca499673bc2283`**
- License: GPLv2 (Duke Nukem 3D source, 3D Realms) + the Build engine (Ken Silverman).
- Vendored: `Engine/src/` (minus `enet/` — multiplayer-only, deferred) + `Game/src/`,
  unmodified. We never track upstream; this is a frozen snapshot like the vendored
  doomgeneric/Chocolate Doom in `src/doors/syncdoom/`.

## The platform seam — `Engine/src/display.c`

Like Chocolate Doom's `i_video.c`, **one file** implements the whole SDL platform
layer (video / palette / input / timer / mouse). Our headless shim
(`syncduke_plat.c`) **replaces `display.c`** while keeping its interface
(`display.h`). Symbols to provide:

### Video / framebuffer
- `uint8_t *frameplace` — the **8-bit indexed framebuffer** the classic renderer
  draws into (also `frameoffset`). In SDL it points at `surface->pixels`; in the
  shim it points at our own `320*200` buffer.
- `int32_t bytesperline` (= 320), `xdim`/`ydim`/`xres`/`yres` (= 320/200).
- `void _nextpage(void)` (display.c:1461) — **the page-flip; our capture point.**
  The renderer has finished drawing into `frameplace`; the shim hands it +
  the current palette to `sixel_encode` and writes to the door socket.
- `_setgamemode()` (display.c:940) / `go_to_new_vid_mode()` — set the video mode;
  the shim allocates the 320x200x8 buffer and points `frameplace` at it.
- Also exported and referenced elsewhere (must be provided/stubbed):
  `_platform_init`, `_uninitengine`, `setvmode`, `_updateScreenRect`,
  `clear2dscreen`, `_idle`, `screencapture`, `getvalidvesamodes`, `get_framebuffer`,
  and the 2D/16-color helpers `readpixel`/`drawpixel`/`setcolor16`/`drawpixel16`/
  `fillscreen16`/`drawline16`.

### Palette
- `int VBE_setPalette(uint8_t *palettebuffer)` (display.c:1330) — **palette set;
  our palette-capture point.** 256 entries, **6-bit VGA** → scale to 8-bit RGB.
- `int VBE_getPalette(int32_t start, int32_t num, uint8_t *palettebuffer)`.

### Input
- SDL event pump: `root_sdl_event_filter` / `handle_events` / `_handle_events`
  (display.c:523/560/570), `sdl_key_filter` (454), `_readlastkeyhit` (692).
  Replace with: read terminal bytes from the door socket → inject into the same
  key-state path the SDL filter feeds. **Task 5 detail:** trace how `sdl_key_filter`
  sets the game's key state (`Game/src/keyboard.c` + `control.c`) and feed that.
- Mouse: `setupmouse`/`readmousexy`/`readmousebstatus` — stub (no mouse v1).
- Joystick: `_joystick_*` — stub.

### Timer
- `uint32_t getticks(void)` (display.c:1877) — ms clock (SDL_GetTicks). Shim: a
  monotonic clock.

## Audio / multiplayer — use the shipped stubs (don't compile the real ones)
- Multiplayer: **`Engine/src/dummy_multi.c`** instead of `network.c`/`multi.c`/
  `mmulti*` (and we dropped `enet/`).
- Audio: **`Game/src/dummy_audiolib.c`** + `nomusic.c` + `nodpmi.c` instead of the
  SDL audio (`audiolib/dsl.c`, `multivoc.c`, `midi/sdl_midi.c`, …). (The large
  `Game/src/audiolib/` tree is vendored for its headers; most `.c` go uncompiled
  and could be trimmed later.)

## Build (GCC 14, 64-bit)
- Compile the **vendored sources unmodified** with the int↔pointer errors
  downgraded to warnings: `-Wno-error=int-conversion -Wno-error=implicit-int
  -Wno-error=incompatible-pointer-types` (plus likely `-Wno-error=implicit-function-declaration`).
- Defines: `-DPLATFORM_UNIX` (selects `unix_compat.h`).
- **64-bit runtime risk:** 20 `FP_OFF` casts (`(int32_t)pointer` → truncates a
  64-bit pointer) + mixed `intptr_t`/`int32_t`. It compiles 64-bit; if it crashes
  on a truncated pointer at boot, either fix the few hot `FP_OFF` spots or build
  the door 32-bit (`-m32`, needs `gcc-multilib` + `libc6-dev-i386`).
- Our shim files (`syncduke.c`, `syncduke_plat.c`, …) follow house style + uncrustify;
  the vendored engine sources do NOT (leave upstream style intact).
