# termgfx truecolor present path Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a truecolor present path (`termgfx_termio_present_rgbx`) to the
shared `termgfx_termio` module and move the median-cut quantizer into the shared
library, so a truecolor source (EasyRPG, and later retro/doom) can hand termgfx a
32-bit XRGB frame — without changing syncscumm's existing indexed output.

**Architecture:** `termgfx_termio` today drives every tier (sixel / JXL / no-gfx)
from an 8-bit indexed frame + a 256-color palette (`present(idx, pal)`). This adds
a parallel entry point `present_rgbx(xrgb, w, h)` for 32-bit sources: the sixel
tier quantizes the RGB internally (via the quantizer, moved from syncscumm into
termgfx), the JXL tier encodes the RGB directly (no indexed round-trip), and the
existing indexed path is untouched. This is the M0 milestone of the SyncRPG
sub-project-2 spec (`docs/superpowers/specs/2026-07-19-syncrpg-door-design.md`).

**Tech Stack:** C (the shared module and quantizer are plain C11), the existing
`termgfx` CMake library build, syncscumm's `door/module.mk` + `test/*.sh` shell
harness for the regression gates.

## Global Constraints

- **No behavior change to syncscumm.** syncscumm is live; its indexed
  `present(idx, pal)` path and its quantizer output must stay identical. Gate
  with its boot suite (9/9), unit tests (20/20), and a **PPM / decoded-image**
  comparison — **NOT** a raw sixel-byte diff (a syncscumm sixel capture is not
  reproducible across builds: ScummVM bakes `__DATE__`/`__TIME__` into an
  on-screen version string, and palette-register serialization order is
  build-nondeterministic; see the baseline manifest §8).
- **Portable C.** Must compile under GCC, Clang, and MSVC. Only GCC is exercised
  here; the MSVC build is generated from `door/module.mk` by create_project and
  is not run on this host — flag it, don't assume it.
- **Hand-formatted.** `termgfx/*.c/.h`, `door/*.cpp`, `test/*.sh`, `test_*.c` use
  tabs and match existing style; uncrustify must NOT be run.
- **Reserved-identifier rule.** No new leading-underscore-capital or
  double-underscore identifiers / include guards; use the trailing-underscore
  form (`TERMGFX_QUANT_H_`).
- **Frame format.** `present_rgbx` takes `w*h` 32-bit pixels, memory byte order
  **R, G, B, X** (X = ignored pad), stride `w*4`. termgfx reads bytes
  `[0],[1],[2]` as R,G,B at stride 4 with no swizzle.
- **Commit direct to master; do NOT push.** Wrap commit-message bodies at ≤78
  chars and verify with `awk 'length > 78'` before committing; never `--amend`
  the shared index.

## File Structure

- `src/doors/termgfx/termgfx_quant.{c,h}` — **created** by `git mv` from
  `src/doors/syncscumm/door/sst_quant.{c,h}`; symbols renamed `sst_quant_*` →
  `termgfx_quant_*`. The deterministic median-cut RGB888→256-color quantizer,
  now shared. One responsibility: quantization. No termgfx_termio dependency.
- `src/doors/termgfx/termgfx_termio.h` — **modified**: declare `present_rgbx`.
- `src/doors/termgfx/termgfx_termio.c` — **modified**: implement `present_rgbx`
  (tier branch) reusing the existing geometry/tier/pacing/emit machinery;
  `#include "termgfx_quant.h"`.
- `src/doors/termgfx/CMakeLists.txt` — **modified**: add `termgfx_quant.c`.
- `src/doors/syncscumm/door/video_term.cpp` — **modified**: repoint the quantizer
  include + call to the shared `termgfx_quant`.
- `src/doors/syncscumm/door/module.mk` — **modified**: drop `sst_quant.o` (now in
  the lib).
- `src/doors/syncscumm/test/test_termgfx_quant.c`,
  `src/doors/syncscumm/test/unit_termgfx_quant.sh` — **renamed** from
  `test_sst_quant.c` / `unit_sst_quant.sh`, repointed to `termgfx_quant`.
- `src/doors/syncscumm/test/test_termgfx_present_rgbx.c`,
  `src/doors/syncscumm/test/unit_termgfx_present_rgbx.sh` — **created**: unit test
  for the new truecolor path.

---

### Task 1: Move the quantizer into termgfx (`sst_quant` → `termgfx_quant`)

**Files:**
- Rename: `src/doors/syncscumm/door/sst_quant.c` →
  `src/doors/termgfx/termgfx_quant.c`; `.h` likewise.
- Modify: `src/doors/termgfx/CMakeLists.txt`,
  `src/doors/syncscumm/door/video_term.cpp`,
  `src/doors/syncscumm/door/module.mk`
- Rename: `src/doors/syncscumm/test/test_sst_quant.c` →
  `test_termgfx_quant.c`; `unit_sst_quant.sh` → `unit_termgfx_quant.sh`

**Interfaces:**
- Produces: `int termgfx_quant_rgb(const uint8_t *rgb, int w, int h, uint8_t
  *out_idx, uint8_t *out_pal768)` — the same signature/semantics as
  `sst_quant_rgb`, deterministic, returns the color count used. Consumed by
  syncscumm's `video_term.cpp` (now) and `termgfx_termio.c`'s `present_rgbx`
  (Task 3).

- [ ] **Step 1: Move the files with history.**
```bash
cd src/doors
git mv syncscumm/door/sst_quant.c termgfx/termgfx_quant.c
git mv syncscumm/door/sst_quant.h termgfx/termgfx_quant.h
```

- [ ] **Step 2: Rename the symbols + guard** in the moved files. Apply exactly
  these substitutions to `termgfx/termgfx_quant.{c,h}` (identifier renames only —
  no logic change):
```bash
sed -i -E \
  -e 's/\bsst_quant_/termgfx_quant_/g' \
  -e 's/\bSST_QUANT_/TERMGFX_QUANT_/g' \
  -e 's/#include "sst_quant.h"/#include "termgfx_quant.h"/' \
  termgfx/termgfx_quant.c termgfx/termgfx_quant.h
```
  Verify the include guard is now `TERMGFX_QUANT_H_` (conforming) and no
  `sst_quant` identifiers remain:
```bash
grep -nE 'sst_quant|SST_QUANT' termgfx/termgfx_quant.c termgfx/termgfx_quant.h
```
  Expected: no output.

- [ ] **Step 3: Add it to the termgfx library.** In
  `src/doors/termgfx/CMakeLists.txt`, add `termgfx_quant.c` to the same source
  list that already lists `termgfx_termio.c` / `sst_plat.c` (match the existing
  formatting — one entry per line, tabs/spaces as the file uses).

- [ ] **Step 4: Repoint syncscumm's consumer.** In
  `src/doors/syncscumm/door/video_term.cpp`, change the include
  `#include "sst_quant.h"` → `#include "termgfx_quant.h"` (the `-I` path to
  `termgfx` is already on the door build) and the call `sst_quant_rgb(` →
  `termgfx_quant_rgb(`. In `src/doors/syncscumm/door/module.mk`, remove
  `sst_quant.o` from the object list (the lib now supplies it).

- [ ] **Step 5: Migrate the quant unit test.**
```bash
cd src/doors/syncscumm/test
git mv test_sst_quant.c test_termgfx_quant.c
git mv unit_sst_quant.sh unit_termgfx_quant.sh
sed -i -E -e 's/\bsst_quant_/termgfx_quant_/g' -e 's/\bSST_QUANT_/TERMGFX_QUANT_/g' \
  -e 's|#include "sst_quant.h"|#include "termgfx_quant.h"|' test_termgfx_quant.c
```
  Then in `unit_termgfx_quant.sh` repoint the compiled source and paths: the
  driver currently compiles `"$DOOR/door/sst_quant.c"` — change it to
  `"$DOOR/../termgfx/termgfx_quant.c"`, add `-I"$DOOR/../termgfx"` to the `cc`
  line if not already present, update `$HERE/test_sst_quant.c` →
  `$HERE/test_termgfx_quant.c`, the `/tmp/test_*` output name, and any comment
  naming the old file.

- [ ] **Step 6: Run the quant unit test — the primary gate for this move.**

Run: `cd src/doors/syncscumm && ./build.sh && ./test/unit_termgfx_quant.sh`
Expected: builds clean; the test prints its pass line (`... QUANT OK ...`) and
exits 0 — the quantizer is byte-for-byte the same code, just relocated.

- [ ] **Step 7: Commit.**
```bash
git add -A src/doors/termgfx src/doors/syncscumm/door src/doors/syncscumm/test
git commit -F <msg-file>   # message wrapped <=78, verified with awk
```

---

### Task 2: Declare `termgfx_termio_present_rgbx` in the shared header

**Files:**
- Modify: `src/doors/termgfx/termgfx_termio.h`

**Interfaces:**
- Produces: `void termgfx_termio_present_rgbx(const uint8_t *xrgb, int w, int h)`
  — consumed by Task 3's implementation and (later) the syncrpg door.

- [ ] **Step 1: Add the declaration** next to the existing
  `termgfx_termio_present(const uint8_t *idx, const uint8_t *pal)` declaration,
  with a doc comment:
```c
/* Truecolor present: hand a native-resolution 32-bit frame. `xrgb` is w*h
 * pixels, memory byte order R,G,B,X (the 4th byte is ignored padding), stride
 * w*4. The sixel tier quantizes internally; the JXL tier encodes the RGB
 * directly (no indexed round-trip). Parallel to termgfx_termio_present(), which
 * stays the entry point for native-indexed sources. */
void termgfx_termio_present_rgbx(const uint8_t *xrgb, int w, int h);
```

- [ ] **Step 2: Confirm it compiles as a declaration** (the lib still builds with
  only the header change and no definition yet is fine only if nothing references
  it — so this task commits together with Task 3, or leave uncommitted until Task
  3 defines it). Proceed directly to Task 3; commit both together in Task 3.

---

### Task 3: Implement `present_rgbx` (tier branch) in `termgfx_termio.c`

**Files:**
- Modify: `src/doors/termgfx/termgfx_termio.c`

**Interfaces:**
- Consumes: `termgfx_quant_rgb` (Task 1), the existing internal emit/geometry/
  pacing helpers in `termgfx_termio.c`.
- Produces: the definition of `termgfx_termio_present_rgbx`.

**Context for the implementer (read the file — these are the anchors):**
`termgfx_termio.c` already contains the indexed present pipeline. The relevant
internal helpers (names as of the current file — verify):
- `termgfx_termio_present(const uint8_t *fb, const uint8_t *pal8)` — the indexed
  entry point; it selects the tier, computes emit geometry (`termgfx_geom_fit`
  into `ew`/`eh` from `g_canvas_w`/`g_canvas_h`), and dispatches to the sixel or
  JXL emit.
- `sst_emit_sixel(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int
  emit_pal)` — scales the indexed frame to `ew*eh` (`sst_scale_idx`) and calls
  `sixel_encode(idx, ew, eh, pal8, ...)`.
- `sst_emit_jxl(const uint8_t *fb, const uint8_t *pal8, int ew, int eh, int dx,
  int dy)` — packs indexed→RGB (`sst_pack_rgb`) then `jxl_encode`s.
- `SST_FB_W`/`SST_FB_H` (== `TERMGFX_TERMIO_FB_W/H`) are the native frame size for
  syncscumm; for `present_rgbx` the native size is the caller's `w`/`h`, so the
  scale helpers must take `w`/`h` as parameters rather than assuming the fixed
  320x200.

**Design of `present_rgbx`:** mirror `termgfx_termio_present`'s tier/geometry/
pacing flow, but produce the encoder input from XRGB instead of indexed:
- Compute emit geometry (`ew`/`eh`, `dx`/`dy`) the same way `present()` does, but
  fit the caller's native `w`/`h` (not `SST_FB_W`/`H`).
- **sixel tier:** nearest-neighbor scale the XRGB `w*h` frame to a packed RGB888
  `ew*eh` buffer (a new `sst_scale_rgbx_to_rgb(xrgb, w, h, ew, eh)` helper —
  read bytes `[i*4+0..2]`, skip `[i*4+3]`), quantize it with
  `termgfx_quant_rgb(rgb, ew, eh, idx, pal768)`, then `sixel_encode(idx, ew, eh,
  pal768, ...)` — the same encoder the indexed path uses.
- **JXL tier:** scale XRGB→RGB888 at `ew*eh` (same helper) and `jxl_encode` that
  RGB directly — skipping the indexed round-trip `sst_emit_jxl` does.
- **no-gfx tier:** same as `present()` (nothing to draw).
- Reuse the existing pacing/backpressure/deferred-present bookkeeping — factor
  the shared tail of `present()` into a helper the two entry points call, OR (if
  that refactor is too invasive to keep syncscumm byte-identical) duplicate the
  minimal dispatch in `present_rgbx` and leave `present()` untouched. **Prefer
  the least-invasive change that leaves `present()`'s emitted bytes identical.**

- [ ] **Step 1: Add the XRGB scale helper** (packed RGB888 out, pad byte
  dropped), near `sst_scale_idx`:
```c
/* NN-scale a native w x h XRGB (R,G,B,X) frame to a packed RGB888 ew x eh
 * buffer (pad byte dropped). Backs present_rgbx's sixel + JXL tiers. */
static const uint8_t *sst_scale_rgbx_to_rgb(const uint8_t *xrgb, int w, int h,
                                            int ew, int eh)
{
	if (!ensure_cap(&g_rgb_buf, &g_rgb_cap, (size_t)ew * eh * 3))
		return NULL;
	for (int y = 0; y < eh; y++) {
		const uint8_t *srow = xrgb + (size_t)(y * h / eh) * w * 4;
		uint8_t *o = g_rgb_buf + (size_t)y * ew * 3;
		for (int x = 0; x < ew; x++) {
			const uint8_t *p = srow + (size_t)(x * w / ew) * 4;
			*o++ = p[0]; *o++ = p[1]; *o++ = p[2];
		}
	}
	return g_rgb_buf;
}
```
  (Add `static uint8_t *g_rgb_buf; static size_t g_rgb_cap;` alongside the
  existing `g_idx_buf`/`g_sixel_buf` scratch declarations.)

- [ ] **Step 2: Implement `present_rgbx`** mirroring `present()`'s tier/geometry
  dispatch, using the helper for both graphical tiers and `termgfx_quant_rgb` for
  the sixel tier. Write it against the real internals of `present()` in the file
  (the exact geometry/pacing calls). Keep `present()` (indexed) unchanged.

- [ ] **Step 3: Build the library.**

Run: `cd src/doors/syncscumm && ./build.sh`
Expected: `libtermgfx.a` + `build/syncscumm` build clean (syncscumm links the new
symbol into the lib but does not call it — a compile/link smoke check).

- [ ] **Step 4: Commit** Tasks 2 + 3 together (header decl + definition).

---

### Task 4: Unit-test the truecolor path

**Files:**
- Create: `src/doors/syncscumm/test/test_termgfx_present_rgbx.c`,
  `src/doors/syncscumm/test/unit_termgfx_present_rgbx.sh`

**Interfaces:**
- Consumes: `termgfx_termio_present_rgbx`, the `SYNCSCUMM_SIXELOUT` capture mode
  (the existing headless sixel-capture path that `termgfx_termio_init` enters
  when the env var is set — writes each present to a file, no tty needed).

- [ ] **Step 1: Write the test.** Drive `present_rgbx` headlessly and assert the
  captured output. Use `SYNCSCUMM_SIXELOUT` to a temp file, `init`, feed a small
  known XRGB frame, `shutdown`, then decode/inspect the captured sixel. Minimum
  assertions:
  - A **solid-color** XRGB frame (all pixels R=200,G=40,B=10,X=255) produces a
    sixel whose single color register is that RGB (allowing the sixel 0-255 vs
    0-100 scale conversion the encoder applies) — proves the R,G,B,X byte order
    and the quantize→encode path.
  - A **two-color** frame (left half color A, right half color B) yields exactly
    two color registers, both present — proves the quantizer sees distinct
    colors and the pad byte is ignored (set X to differing garbage per pixel to
    confirm it does not affect output).
```c
/* test_termgfx_present_rgbx.c -- headless XRGB present via SYNCSCUMM_SIXELOUT.
 * Feeds a known 32-bit R,G,B,X frame and checks the captured sixel's palette. */
#include "termgfx_termio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ... build a w*h*4 XRGB buffer, init with SYNCSCUMM_SIXELOUT set to a temp
 *     path, call termgfx_termio_present_rgbx(buf, w, h), shutdown, then read the
 *     temp file and assert it contains the expected "#<n>;2;r;g;b" register(s).
 *     Vary the X byte per-pixel and assert the output is unchanged vs X=255. */
```
  (Write the full body against the same capture-mode init pattern the existing
  `test_termgfx_termio_audio.c` / `test_termgfx_termio.c` use — those show how to
  init headlessly and where the capture file lands.)

- [ ] **Step 2: Write the driver** `unit_termgfx_present_rgbx.sh`, mirroring
  `unit_termgfx_termio.sh`'s single-binary compile pattern (compile the test +
  `"$DOOR/../termgfx/termgfx_termio.c"` + link `libtermgfx.a` + `libxpdev_static.a`,
  with the JXL/sndfile `-l` handoff blocks copied from that driver).

- [ ] **Step 3: Run it.**

Run: `cd src/doors/syncscumm && ./test/unit_termgfx_present_rgbx.sh`
Expected: both assertions pass, exit 0.

- [ ] **Step 4: Commit.**

---

### Task 5: Verify syncscumm is not regressed

**Files:**
- Run only: `src/doors/syncscumm/test/boot_*.sh`,
  `test/unit_termgfx_termio.sh`, the PPM capture.

- [ ] **Step 1: Boot suite, unedited.**

Run: `cd src/doors/syncscumm && for t in test/boot_*.sh; do "$t"; done`
(run `boot_queen.sh` standalone — it needs >2 min).
Expected: the 9 `BOOT-* OK` lines with the frame counts in the baseline manifest
§2. No edits to the boot scripts.

- [ ] **Step 2: Unit suite.**

Run: `./test/unit_termgfx_termio.sh` (expect 20/20) and
`./test/unit_termgfx_quant.sh` (the relocated quantizer, expect pass).

- [ ] **Step 3: PPM render gate (indexed path unchanged).** Regenerate the BASS
  PPM set exactly per baseline manifest §4/§7 and confirm the aggregate matches
  the golden `b2cbf878...` (and `diff -rq` against the saved baseline `ppm/` if it
  survives). This proves the quantizer move + the new `present_rgbx` symbol did
  not perturb syncscumm's indexed output.
  **Do NOT** diff raw sixel captures — they are build-nondeterministic (manifest
  §8). Any PPM difference is a regression: stop and fix before proceeding.

- [ ] **Step 4:** No commit (verification only) unless a fix was needed. If green,
  the M0 milestone is complete.

---

## Notes for the follow-on door plan (M1–M4)

Not part of this plan; recorded so the milestone hands off cleanly:
- The door consumes `termgfx_termio_present_rgbx` from `video_term`
  (`GetDisplaySurface()` 32-bit buffer, with EasyRPG's `main_surface` format set
  to the R,G,B,X order this path expects → zero-copy).
- Pinned game source: official Playism 2012 English Yume Nikki 0.10a
  (archive.org `yume-nikki-010a`); headless extraction via the inner
  `YumeNikki.lzh` (spec §Data & control flow).
