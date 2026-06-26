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
- **64-bit runtime risk — RESOLVED, tractable.** The engine boots and renders the
  title screen at 64-bit. Upstream had *already* 64-bit-ported the renderer core
  (`bufplce[]` is `intptr_t`, `palookupoffse[]`/`globalbufplc` are `uint8_t*`, the
  asm-derived inner loops pass real `uint8_t*` dests), so only a few *stragglers*
  truncate pointers. We fix those in place as we hit them rather than build 32-bit.

### Vendored 64-bit straggler patches (keep this list — re-apply on re-vendor)
1. **`Engine/src/unix_compat.h`** — `FP_OFF(x)` was `((int32_t)(x))`; widened to
   `((intptr_t)(x))` so the pointer round-trip uses (`(void*)FP_OFF(colhere)` in
   `initfastcolorlookup`, the `palookup` pointer builds) survive LP64 instead of
   crashing `clearbufbyte`. No-op on a 32-bit build. Sites that still cast
   `(int32_t)FP_OFF(...)` into an `int32_t` global (`palookupoffs`/`asm3`) are
   legacy/dead paths — the live renderer uses the `*e[]` pointer variants.
2. **`Engine/src/engine.c` `clearview()`** — local `int32_t p` held a framebuffer
   pointer (`p = frameplace + ...`) → truncated on LP64 → `clearbufbyte` SIGSEGV in
   `Logo()`. Changed to `intptr_t p`. (Siblings `clearallviews`/`plotpixel`/
   `getpixel` keep the pointer typed as `uint8_t*`, already clean.)
3. **CON-script VM (the big one — `gamedef.c`/`game.c`/`actors.c` + `duke3d.h`/
   `global.c`).** The CON VM stores absolute `&script[...]` addresses inside int32
   slots. Widened `script[MAXSCRIPTSIZE]`, `scriptptr`/`insptr`/`labelcode`,
   `actorscrptr[]`/`parsing_actor`, and `hittype.temp_data[6]` to `intptr_t`
   (`labelcnt` stays int32). Then: `g_t`/`tempscrptr`/`moveptr` → `intptr_t*`; the
   `(int32_t)scriptptr` stores → `(intptr_t)`; the `(int32_t*)` script-pointer casts
   → `(intptr_t*)`. **Byte-offset landmine:** sites that index an action/move struct
   as `*(int32_t*)(g_t[x]+N)` use BYTE offsets that assumed 4-byte script words —
   scaled to `N/4*sizeof(intptr_t)` (gamedef.c lines ~2190-2192, ~3156-3162;
   game.c `animatesprites` `t4+8`→`+2*sizeof`). `game.c`'s `t1`/`t4` (=T2/T5, move/
   action ptrs) and `actors.c` SE21 `l=(int32_t)&floorz` were bare-int pointer
   holders → widened / given a dedicated `intptr_t`/`uint8_t*`.
4. **Renderer palookup/texture pointers in int32 (`engine.c`/`draw.c`).** Only hit
   in actual 3D gameplay (title/menu use 2D rotatesprite). `slopalookup[16384]` and
   grouscan's `j`/`mptr*`/`nptr*` held palookup POINTERS as int32 → `intptr_t`;
   `slopevlin()`'s `i3` param + its `uint32_t ebx` register read the palookup
   pointer (`*(uintptr_t*)i3`, step `sizeof(intptr_t)`). The `asm3`/`palookupoffs`/
   `palookupoffse[]` assignments dropped the `(int32_t)` truncating cast and use
   pointer arithmetic (`palookup[globalpal] + shade`). These ARE the "legacy/dead"
   sites note 1 dismissed — they're live in the wall/floor/sprite mappers.
- **DONE: Duke E1L1 renders + plays headless at 64-bit** (boot→title→menu→new
  game→3D world+HUD+weapon+movement, no crash). Expect a few more renderer/CON
  stragglers for not-yet-exercised content (translucency, voxels, specific
  enemies/effects, weapon fire); same shape, same fix. Each surfaces as a
  `clearbufbyte`/`drawpixel`/mapper SIGSEGV at a `0x00000000xxxxxxxx`-ish address.
- **KNOWN-DEFERRED: `menues.c` savegame** still does 32-bit pointer↔offset
  relocation (`(int32_t)&script[0]`, `dfwrite(...,4,...)`) — compiles (warnings),
  but quicksave/load is broken on LP64. Out of v1 first-light scope; fix when saves
  are wanted (widen the relocation casts + element sizes to `sizeof(intptr_t)`).

### Palette format (the headless capture point)
`setbrightness()` (engine.c) builds a **256×4-byte BGRA, 6-bit (0..63)** buffer and
hands it to `VBE_setPalette()`. The shim converts that to an 8-bit RGB table
(`r*255/63`) that the framebuffer indices render through (PPM now, sixel later).
Treating it as 3-byte stride gives a recognizable-but-psychedelic image — the tell
that the stride is wrong.

- Build flags unchanged: `-DPLATFORM_UNIX`, `-w -fcommon`, the int↔pointer
  `-Wno-error=` set. The 32-bit fallback (`-m32`, needs `gcc-multilib` +
  `libc6-dev-i386`) is no longer expected to be needed.
- Our shim files (`syncduke.c`, `syncduke_plat.c`, …) follow house style + uncrustify;
  the vendored engine sources do NOT (leave upstream style intact) — except the two
  documented straggler patches above.
