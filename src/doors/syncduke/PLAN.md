# SyncDuke v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make shareware Duke Nukem 3D (Episode 1) playable single-player over a Synchronet/SyncTERM terminal door, rendered via `libtermgfx`'s sixel encoder.

**Architecture:** Vendor a pinned Chocolate Duke3D (Build engine + Duke game) snapshot into `src/doors/syncduke/`; replace its SDL platform layer (video/input/timer/sound) with a headless shim (`syncduke.c` + `syncduke_plat.c`) that captures the 8-bit framebuffer + palette and renders it to the door socket via `sixel_encode`, and feeds terminal input back as Duke key events. CMake build paralleling `src/doors/syncdoom/`.

**Tech Stack:** C; CMake; xpdev (sockwrap/dirwrap); `libtermgfx` (`sixel_encode`); Build engine (Chocolate Duke3D).

## Global Constraints

- Vendored engine is a **pinned Chocolate Duke3D snapshot** (`github.com/fabiensanglard/chocolate_duke3D`, GPLv2); we never track upstream — same as the vendored doomgeneric/Chocolate Doom.
- **Portable C:** must build under GCC now, structured for MSVC later (`CONTRIBUTING.md`). No reserved identifiers; trailing-underscore include guards.
- **uncrustify** (`src/uncrustify.cfg`) applies to OUR files only (`syncduke.c`, `syncduke_plat.c`) — NOT the vendored engine sources (leave upstream style intact, like the vendored Doom).
- **Render input** to the lib: 8-bit indexed framebuffer + 256-entry palette. Build palettes are 6-bit VGA → scale to 8-bit. Encoder: `size_t sixel_encode(uint8_t **buf, size_t *cap, const uint8_t *idx, int w, int h, const uint8_t *pal, int emit_palette)` from `termgfx/sixel.h`.
- **Door I/O** via xpdev sockwrap on the DOOR32.SYS/`-s` socket — follow `src/doors/syncdoom/syncdoom.c`'s pattern (non-blocking, staged out-buffer).
- **Data:** shareware `DUKE3D.GRP` — NOT committed; located via config at runtime.
- **v1 non-goals:** no multiplayer, no JS lobby, no JXL/text tiers, no audio, keyboard-only.

---

## Task 1: Acquire, pin, and map the Chocolate Duke3D source

**Files:**
- Create: `src/doors/syncduke/` (vendored `Engine/` + `Game/` sources)
- Create: `src/doors/syncduke/SEAM.md` (notes: the exact symbols later tasks hook)

**Deliverable:** the pinned sources in-tree + a SEAM.md naming the platform seam.

- [ ] **Step 1: Clone + pin.** `git clone https://github.com/fabiensanglard/chocolate_duke3D /tmp/cduke`; `cd /tmp/cduke && git rev-parse HEAD` — record the SHA in SEAM.md as the pinned revision.
- [ ] **Step 2: Vendor the sources.** Copy `Engine/` and `Game/` into `src/doors/syncduke/` (preserve subdirs). Do NOT copy `vs2005/`, `xcode/`, autotools, or any binaries.
- [ ] **Step 3: Identify the video seam.** From `src/doors/syncduke/`, grep the Build engine for the page-flip + framebuffer + palette:
  - `grep -rnE "nextpage|showframe|palfadergb|setpalette|getpalette|frameplace|bytesperline" Engine/`
  - Record in SEAM.md: the page-flip function name, the framebuffer pointer/global (the 8-bit screen buffer the classic renderer draws into), and the palette get/set.
- [ ] **Step 4: Identify input + timer + sound seams.** `grep -rnE "SDL_|keyhandler|bgetchar|keystatus|getticks|totalclock|initsb|SDL_mixer" Engine/ Game/` — record: the keyboard state/queue the engine reads, the millisecond clock (`getticks`/`totalclock`), and the sound-init entry points (to stub).
- [ ] **Step 5: Record the SDL platform file(s).** The file(s) implementing the above with SDL (e.g. an `sdlayer`/`baselayer`-style `.c`) are the ones we REPLACE. Name them in SEAM.md.
- [ ] **Step 6: Commit** the vendored snapshot + SEAM.md: `git add src/doors/syncduke && git commit -m "syncduke: vendor pinned Chocolate Duke3D snapshot + seam notes"`.

**Verification:** SEAM.md names a concrete page-flip fn, framebuffer global, palette get/set, keyboard source, ms clock, and sound-init; the SDL file(s) to replace are identified.

---

## Task 2: Headless build — de-SDL + CMake link

**Files:**
- Create: `src/doors/syncduke/CMakeLists.txt`
- Create: `src/doors/syncduke/syncduke_plat.c` (stub platform layer — replaces the SDL file from Task 1 Step 5)
- Create: `src/doors/syncduke/build.sh` (optional convenience, mirror syncdoom's)

**Interfaces:**
- Produces: a buildable `syncduke` target linking `termgfx` + `xpdev`.

- [ ] **Step 1: CMakeLists**, modeled on `src/doors/syncdoom/CMakeLists.txt`: list the vendored `Engine/`+`Game/` `.c` sources + `syncduke.c` + `syncduke_plat.c`; EXCLUDE the SDL platform file named in SEAM.md; `add_subdirectory(../termgfx termgfx)` + `target_link_libraries(syncduke termgfx)`; add xpdev like syncdoom; `target_include_directories` for the engine's own headers. No JXL, no SDL.
- [ ] **Step 2: Stub `syncduke_plat.c`** — provide empty/no-op definitions for every symbol the engine expected from the SDL file (the functions listed in SEAM.md): video set/flip/palette, keyboard, getticks (return 0 for now), sound init (no-op). Goal: satisfy the linker, nothing functional yet.
- [ ] **Step 3: Minimal `syncduke.c`** with a `main()` that just calls the engine's init far enough to link (or a temporary empty `main`), so the target links.
- [ ] **Step 4: Build.** `mkdir -p src/doors/syncduke/build && cd $_ && cmake .. && cmake --build . -j"$(nproc)"`. Fix modern-GCC errors in the vendored sources as they arise (document each fix briefly — these are the "2019 → modern toolchain" cleanups).
- [ ] **Step 5: Commit** once it compiles+links: `git add … && git commit -m "syncduke: headless CMake build, SDL platform stubbed out"`.

**Verification:** `cmake --build` produces a `syncduke` binary (does nothing useful yet).

---

## Task 3: Boot the engine to a frame (video capture)

**Files:**
- Modify: `src/doors/syncduke/syncduke_plat.c`
- Modify: `src/doors/syncduke/syncduke.c`
- Test: `src/doors/syncduke/tests/test_palette.c`

**Interfaces:**
- Produces: `extern uint8_t sd_fb[320*200];` (latest 8-bit frame), `extern uint8_t sd_pal[768];` (current palette, 8-bit RGB), set on each engine page-flip.
- Produces: `void sd_vga6_to_rgb8(const uint8_t *in768, uint8_t *out768);` (palette scale).

- [ ] **Step 1: Write the failing palette test** `tests/test_palette.c`:
```c
#include <assert.h>
#include <stdint.h>
void sd_vga6_to_rgb8(const uint8_t *in, uint8_t *out);
int main(void){
    uint8_t in[768], out[768];
    for(int i=0;i<768;i++) in[i]=(uint8_t)(i%64);   // 0..63 VGA range
    sd_vga6_to_rgb8(in,out);
    assert(out[0]==0); assert(out[63%768]==/*63*255/63*/255);
    /* spot-check a mid value: 32 -> round(32*255/63)=130 */
    uint8_t one[768]={32}, o2[768]; sd_vga6_to_rgb8(one,o2); assert(o2[0]==130);
    return 0;
}
```
- [ ] **Step 2: Run it, expect link failure** (`sd_vga6_to_rgb8` undefined). Compile: `gcc tests/test_palette.c syncduke_plat.c -o /tmp/tp` → FAIL.
- [ ] **Step 3: Implement `sd_vga6_to_rgb8`** in `syncduke_plat.c`:
```c
void sd_vga6_to_rgb8(const uint8_t *in, uint8_t *out){
    for(int i=0;i<768;i++) out[i]=(uint8_t)((in[i]*255+31)/63); /* 6-bit -> 8-bit, rounded */
}
```
- [ ] **Step 4: Run the test, expect PASS.**
- [ ] **Step 5: Implement the page-flip hook.** In `syncduke_plat.c`, in the page-flip function named in SEAM.md, copy the engine's 8-bit framebuffer (the global from SEAM.md) into `sd_fb` and the current (6-bit) palette through `sd_vga6_to_rgb8` into `sd_pal`. Define `sd_fb`/`sd_pal` here.
- [ ] **Step 6: Drive engine init + one tick** from `syncduke.c` (mirror `doomgeneric_Create`/`Tick`: call the engine/game init, then loop the game tick). Point the engine at the shareware `DUKE3D.GRP` (env or hardcode a path for now).
- [ ] **Step 7: Dump-to-PPM probe.** Add a temporary `-dumpframe` path: after N ticks, write `sd_fb`+`sd_pal` as a PPM and exit. Run with the shareware GRP; open the PPM.
- [ ] **Step 8: Commit:** `git commit -m "syncduke: capture engine framebuffer + palette; PPM probe of the title screen"`.

**Verification:** the dumped PPM shows the Duke title/menu screen (proves the engine boots headless and we capture the real frame); palette test passes.

---

## Task 4: Render the frame to the terminal (termgfx sixel)

**Files:**
- Modify: `src/doors/syncduke/syncduke.c`
- Create: `src/doors/syncduke/sd_io.c` (socket out-buffer — minimal, mirrors syncdoom's `out_put`/`out_flush`)

**Interfaces:**
- Consumes: `sd_fb`, `sd_pal` (Task 3); `sixel_encode(...)` (`termgfx/sixel.h`).
- Produces: `void sd_present(void);` — encode + emit one frame.

- [ ] **Step 1: Socket out-buffer** `sd_io.c`: `sd_out_put(const void*,size_t)` (stage) + `sd_out_flush(void)` (non-blocking `sendsocket` on the DOOR32 socket) — copy the structure of `syncdoom.c`'s `out_put`/`out_flush` (the proven non-blocking pattern). Open the socket from DOOR32.SYS / `-s` like syncdoom.
- [ ] **Step 2: `sd_present()`** in `syncduke.c`:
```c
static uint8_t *s_sx; static size_t s_sxcap;
void sd_present(void){
    static int first=1;
    sd_out_put("\x1b[H", 3);                 /* cursor home */
    size_t n = sixel_encode(&s_sx,&s_sxcap, sd_fb, 320,200, sd_pal, first);
    first = 0;                                /* re-emit palette only on change later */
    sd_out_put(s_sx, n);
    sd_out_flush();
}
```
- [ ] **Step 3: Call `sd_present()` each frame** in the main loop (replace the PPM probe). Keep `-dumpframe` available behind a flag.
- [ ] **Step 4: Build + run over SyncTERM** against the shareware GRP.
- [ ] **Step 5: Commit:** `git commit -m "syncduke: render the engine frame to the terminal via termgfx sixel"`.

**Verification:** connect with SyncTERM → the Duke title screen renders as sixel. (Palette re-emit/optimization comes when the pacing extraction lands — v1 may re-emit each frame.)

---

## Task 5: Terminal input → Duke keys

**Files:**
- Modify: `src/doors/syncduke/syncduke_plat.c`
- Create: `src/doors/syncduke/sd_input.c`
- Test: `src/doors/syncduke/tests/test_keymap.c`

**Interfaces:**
- Produces: `int sd_map_key(const char *seq, int len);` → Build/Duke scancode, or -1.
- Feeds the engine keyboard source named in SEAM.md.

- [ ] **Step 1: Failing key-map test** `tests/test_keymap.c`: assert `sd_map_key("\x1b[A",3)` → the UP scancode, `sd_map_key("w",1)` → forward, `sd_map_key(" ",1)` → fire/open per Duke defaults, `sd_map_key("\x1b",1)` → ESC. (Use the Build scancode constants from the engine headers found in Task 1.)
- [ ] **Step 2: Run, expect fail** (undefined).
- [ ] **Step 3: Implement `sd_map_key`** (table: arrows/WASD → move/strafe/turn; Ctrl/Space → fire/open; Enter/Esc → menu) in `sd_input.c`.
- [ ] **Step 4: Run, expect pass.**
- [ ] **Step 5: Wire it up.** Read socket bytes non-blocking in the loop; for each key, push the mapped scancode into the engine's keyboard state/queue (the seam from SEAM.md), with a short auto-release (key-down then timed key-up) since a terminal sends keypresses, not hold/release.
- [ ] **Step 6: Commit:** `git commit -m "syncduke: terminal input -> Duke key events"`.

**Verification:** in SyncTERM, movement + turn + fire + open + menu all respond.

---

## Task 6: Timer + main loop + clean exit

**Files:**
- Modify: `src/doors/syncduke/syncduke_plat.c`, `syncduke.c`

- [ ] **Step 1: Real `getticks`** — implement the ms-clock seam (SEAM.md) from a monotonic clock (`xp_timer`/`clock_gettime`), so the engine's timing advances.
- [ ] **Step 2: Loop pacing** — cap the present rate (e.g. ~20–30 fps); run engine ticks at the engine's expected rate. Keep it simple (a sleep/elapsed check) — the AIMD pacing controller is a later termgfx extraction, out of v1 scope.
- [ ] **Step 3: Hangup handling** — when the socket read/write reports the client gone, save nothing and exit cleanly (return to the BBS).
- [ ] **Step 4: Build + play; Step 5: Commit:** `git commit -m "syncduke: monotonic timer, frame pacing, clean hangup"`.

**Verification:** smooth-enough play; disconnect returns to the BBS without hanging.

---

## Task 7: Data, config, install

**Files:**
- Create: `src/doors/syncduke/syncduke.example.ini`
- Create: `src/doors/syncduke/install-xtrn.ini`
- Modify: `src/doors/syncduke/syncduke.c` (GRP path resolution)

- [ ] **Step 1: GRP resolution** — locate `DUKE3D.GRP` via a config key (a `[grp] dir`/`file`), mirroring how syncdoom resolves its WAD dir; pass it to the engine. Clear error if absent.
- [ ] **Step 2: `syncduke.example.ini`** — the `[grp]` dir + any door knobs; documented like `syncdoom.example.ini`.
- [ ] **Step 3: `install-xtrn.ini`** — a `[prog:SYNCDUKE]` entry (`cmd = ?…` or the binary), `[copy:syncduke.example.ini]`→`syncduke.ini`, per `xtrn/CLAUDE.md`. (DUKE3D.GRP is NOT shipped.)
- [ ] **Step 4: Commit:** `git commit -m "syncduke: GRP resolution + example config + installer"`.

**Verification:** `jsexec install-xtrn ../src/doors/syncduke` registers it (or the xtrn install path you use); the door launches against the shareware GRP.

---

## Task 8: First-light integration, uncrustify, ship

- [ ] **Step 1: uncrustify** OUR files only: `uncrustify -c src/uncrustify.cfg --no-backup --replace src/doors/syncduke/{syncduke.c,syncduke_plat.c,sd_io.c,sd_input.c}` (NOT the vendored engine).
- [ ] **Step 2: Clean build** from scratch (`rm -rf build && cmake .. && cmake --build .`).
- [ ] **Step 3: End-to-end play** — shareware Episode 1 (L.A. Meltdown) single-player over SyncTERM: title → new game → move/fire/use through the first level.
- [ ] **Step 4: Final commit** (sources + CMake + shim + config; NOT the GRP): `git commit -m "syncduke: v1 first light — shareware Duke E1 playable over SyncTERM"`.

**Verification (v1 success):** shareware E1 *L.A. Meltdown* playable single-player over SyncTERM, rendered via libtermgfx sixel.

---

## Self-review notes

- **Spec coverage:** engine choice (Task 1), headless shim video/input/timer/audio (Tasks 2–6), termgfx sixel render (Task 4), data/config/install (Task 7), build cleanup (Tasks 2,8), first-light success criterion (Task 8). All §s of DESIGN.md map to a task.
- **Discovery dependency:** Tasks 3/5/6 consume the exact engine symbols pinned by Task 1's SEAM.md — that indirection is inherent to porting a third-party engine, not a placeholder. Every other specific (CMake shape, `sixel_encode` signature, socket pattern, palette scale, key-map) is concrete here.
- **Out of v1 (tracked for later):** JXL + text tiers, AIMD frame-pacing, terminal-cap probe, overlays, MP, JS lobby — these arrive by extracting them from `syncdoom.c` into `termgfx` with SyncDuke as the second consumer (see `[[project-termgfx-door-library]]`).
