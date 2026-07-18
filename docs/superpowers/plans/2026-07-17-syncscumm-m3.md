# SyncSCUMM M3 — Keyboard & Mouse Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a ScummVM adventure (Beneath a Steel Sky first) playable over the terminal by delivering real keyboard and mouse events into the engine.

**Architecture:** The cursor compositor already exists (M2). M3 is pure input plumbing. syncconquer's proven absolute-pixel-mouse handshake is extracted into a new shared `termgfx/mouse` module (and syncconquer is refactored onto it), then syncscumm's `sst_io.c` enables + parses SGR mouse and keymode-decoded keys into a small event FIFO, and `syncscumm.cpp`'s `pollEvent()` drains that FIFO into `Common::Event`s (calling `warpMouse()` so the existing compositor draws the pointer).

**Tech Stack:** C (termgfx, `sst_io.c`), C++ (`syncscumm.cpp`, ScummVM `OSystem` backend), CMake + each door's `build.sh`, xterm SGR mouse (DEC 1006/1016), termgfx keymode (kitty CSI-u / SyncTERM evdev).

## Global Constraints

- **Uncrustify per directory:** `termgfx/*` and `syncconquer/door/*` are uncrustify-managed — write tabs from the start and run `uncrustify -c ../../uncrustify.cfg --replace --no-backup <file>` as the closing step. **`syncscumm/door/*` is HAND-FORMATTED — do NOT run uncrustify on it** (it churns 40+ preexisting lines); match the local style.
- **syncconquer must stay behaviour-identical:** the refactor (Task 2) may not change one byte syncconquer emits or how it maps coordinates. Its `vanilla/` tree is untouched. Treat any emitted-byte diff as a defect.
- **No reserved identifiers:** header guards use the trailing-underscore form (`TERMGFX_MOUSE_H_`); no leading-underscore-capital or `__` anywhere.
- **Style nits:** `strcmp(a,b) == 0`, never `!strcmp(...)`. US spelling.
- **Coordinate target is 320×200 always:** `SST_FB_W`×`SST_FB_H`. The GUI overlay reports the same dimensions (`getOverlayWidth/Height` in `video_term.h`), so game-space and overlay-space mouse mapping are identical — there is no space switch to implement.
- **Build via build.sh:** each door builds out-of-tree with its own `build.sh`; the live binary is a symlink to the build output (a rebuild is live immediately). A running door session pins the old binary — exit the door before concluding a change didn't take.
- **JS/subtitles/audio/video untouched:** M3 adds only the mouse enable/probe alongside existing probes and the input event path. No change to JXL/sixel/audio.

---

### Task 1: `termgfx/mouse` module — enable/restore strings + SGR classify + pixel-mode latch

Extract the terminal-protocol mouse logic currently inline in `syncconquer/door/door_io.c` into a shared module. Coordinate mapping does NOT move (it is game-specific). This task creates the module and its unit test; it does not yet wire any door to it.

**Files:**
- Create: `src/doors/termgfx/mouse.h`
- Create: `src/doors/termgfx/mouse.c`
- Modify: `src/doors/termgfx/CMakeLists.txt` (add `mouse.c` to the library sources; add the `mouse` test)
- Test: `src/doors/termgfx/test/test_mouse.c`

**Interfaces:**
- Consumes: `termgfx_sgr_classify()` from `sgrmouse.h` (already exists).
- Produces:
  ```c
  /* mouse.h */
  #ifndef TERMGFX_MOUSE_H_
  #define TERMGFX_MOUSE_H_
  #include "sgrmouse.h"

  typedef struct {
      int pixels;    /* SGR-Pixels (DEC 1016) confirmed/auto-detected active */
  } termgfx_mouse_t;

  /* One classified SGR report, in the terminal's OWN units (canvas pixels
   * when .pixels, else 1-based text cells). The door maps x/y to game coords. */
  typedef struct {
      termgfx_sgr_kind_t kind;   /* MOVE / BUTTON / WHEEL */
      int button;                /* BUTTON: 0 left, 1 middle, 2 right */
      int wheel;                 /* WHEEL: -1 up, +1 down */
      int col, row;              /* raw report coords (1-based), pass-through */
      int pixels;                /* snapshot of the pixel-mode latch at report time */
  } termgfx_mouse_report_t;

  /* Escape strings the door emits. enable turns on motion tracking + SGR +
   * SGR-Pixels and probes 1016 via DECRQM; restore turns them back off. */
  extern const char *const termgfx_mouse_enable;    /* "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p" */
  extern const char *const termgfx_mouse_restore;   /* "\x1b[?1003l\x1b[?1006l\x1b[?1016l" */

  /* Feed a DECRPM reply (ESC[?<mode>;<Ps>$y) parsed into params[n]. Latches
   * pixel mode when it confirms mode 1016 is set (Ps==1 or Ps==3). No-op
   * otherwise. */
  void termgfx_mouse_on_decrpm(termgfx_mouse_t *m, const int *params, int n);

  /* Auto-detect latch: the caller (which owns the text-grid dimensions) calls
   * this when a report coord exceeds the grid — proof of pixel reporting. */
  void termgfx_mouse_note_pixel_report(termgfx_mouse_t *m);
  int  termgfx_mouse_pixels(const termgfx_mouse_t *m);

  /* Classify one SGR report's button field `b` (via termgfx_sgr_classify) and
   * package it with the raw coords. release is set for the 'm'-terminated form
   * (button-up); it does not change classification, the door uses it. */
  void termgfx_mouse_report(termgfx_mouse_t *m, int b, int col, int row,
                            int release, termgfx_mouse_report_t *out);
  #endif /* TERMGFX_MOUSE_H_ */
  ```
  Note: `termgfx_mouse_report()` sets `out->pixels = m->pixels` so the door reads a consistent snapshot; `release` is copied into no field here (the door already has it from the parse) — keep the parameter for symmetry with the door's call site and future use.

- [ ] **Step 1: Write the failing test**

`src/doors/termgfx/test/test_mouse.c` — model on the existing `test/` unit tests (a `main()` with `assert()`s, no framework):

```c
#include <assert.h>
#include <string.h>
#include "mouse.h"

int main(void) {
    termgfx_mouse_t m = {0};

    /* enable/restore strings are exactly the DEC modes syncconquer sends */
    assert(strcmp(termgfx_mouse_enable,  "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p") == 0);
    assert(strcmp(termgfx_mouse_restore, "\x1b[?1003l\x1b[?1006l\x1b[?1016l") == 0);

    /* starts in cell mode */
    assert(termgfx_mouse_pixels(&m) == 0);

    /* DECRPM confirming 1016 set (Ps=1) latches pixel mode; Ps=2 (reset) does not */
    int reset[2] = {1016, 2};
    termgfx_mouse_on_decrpm(&m, reset, 2);
    assert(termgfx_mouse_pixels(&m) == 0);
    int set[2] = {1016, 1};
    termgfx_mouse_on_decrpm(&m, set, 2);
    assert(termgfx_mouse_pixels(&m) == 1);

    /* auto-detect latch is idempotent */
    termgfx_mouse_t m2 = {0};
    termgfx_mouse_note_pixel_report(&m2);
    assert(termgfx_mouse_pixels(&m2) == 1);

    /* classify: left press, hover (motion), wheel up */
    termgfx_mouse_report_t r;
    termgfx_mouse_report(&m2, 0, 10, 20, 0, &r);
    assert(r.kind == TERMGFX_SGR_BUTTON && r.button == 0 && r.col == 10 && r.row == 20 && r.pixels == 1);
    termgfx_mouse_report(&m2, 35, 10, 20, 0, &r);   /* xterm no-button hover */
    assert(r.kind == TERMGFX_SGR_MOVE);
    termgfx_mouse_report(&m2, 64, 1, 1, 0, &r);
    assert(r.kind == TERMGFX_SGR_WHEEL && r.wheel == -1);

    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Add the test to `termgfx/CMakeLists.txt` first (see Step 3's CMake edit), then:
Run: `cd src/doors/termgfx && cmake -S . -B build >/dev/null && cmake --build build --target test_mouse 2>&1 | tail -5`
Expected: FAIL — `mouse.h`/`mouse.c` do not exist (compile error / undefined refs).

- [ ] **Step 3: Write the module + register it in CMake**

`mouse.h` as in the Interfaces block above. `mouse.c`:

```c
#include <stddef.h>
#include "mouse.h"

const char *const termgfx_mouse_enable  = "\x1b[?1003h\x1b[?1006h\x1b[?1016h\x1b[?1016$p";
const char *const termgfx_mouse_restore = "\x1b[?1003l\x1b[?1006l\x1b[?1016l";

void termgfx_mouse_on_decrpm(termgfx_mouse_t *m, const int *params, int n)
{
    /* ESC[?1016;Ps$y — Ps: 0 not recognized, 1 set, 2 reset, 3 permanently set,
     * 4 permanently reset. Latch pixel mode only when 1016 is (permanently) set. */
    if (n >= 2 && params[0] == 1016 && (params[1] == 1 || params[1] == 3))
        m->pixels = 1;
}

void termgfx_mouse_note_pixel_report(termgfx_mouse_t *m)
{
    m->pixels = 1;
}

int termgfx_mouse_pixels(const termgfx_mouse_t *m)
{
    return m->pixels;
}

void termgfx_mouse_report(termgfx_mouse_t *m, int b, int col, int row,
                          int release, termgfx_mouse_report_t *out)
{
    (void)release;
    out->kind   = termgfx_sgr_classify(b, &out->button, &out->wheel);
    out->col    = col;
    out->row    = row;
    out->pixels = m->pixels;
}
```

CMake edit (`termgfx/CMakeLists.txt`): add `mouse.c` to the termgfx library source list (next to `sgrmouse.c`), and register the unit test the same way `test_sgrmouse`/the other `test/` binaries are registered (an `add_executable(test_mouse test/test_mouse.c)` linking the termgfx lib, plus `add_test(...)` if the file uses `enable_testing()`). Match the file's existing pattern exactly.

- [ ] **Step 4: Run test to verify it passes**

Run: `cd src/doors/termgfx && cmake --build build --target test_mouse 2>&1 | tail -3 && ./build/test_mouse && echo PASS`
Expected: builds clean, `test_mouse` exits 0, prints `PASS`.

- [ ] **Step 5: Uncrustify + commit**

```bash
cd src/doors/termgfx
uncrustify -c ../../uncrustify.cfg --replace --no-backup mouse.c mouse.h test/test_mouse.c
git add mouse.c mouse.h test/test_mouse.c CMakeLists.txt
git commit -F - <<'EOF'
termgfx: add a shared SGR mouse handshake module

Extract the terminal-protocol half of syncconquer's absolute-pixel mouse
-- the DEC 1003/1006/1016 enable/restore strings, the SGR-Pixels latch
(DECRQM reply + past-the-grid auto-detect) and the button-field classify
-- into termgfx/mouse, so a second door (syncscumm) can share it instead
of duplicating the handshake. Coordinate mapping stays door-side; only
the protocol lives here, mirroring sgrmouse.h's split. Unit-tested.
EOF
```

---

### Task 2: Refactor syncconquer onto `termgfx/mouse` (behaviour-identical)

Replace the inline enable/restore/latch/DECRPM logic in `door_io.c` with the module. Keep `door_input.c` and the coordinate mapping unchanged (they call `door_io_*` accessors, which remain).

**Files:**
- Modify: `src/doors/syncconquer/door/door_io.c` — the mouse-enable send (~2773–2783), the `g_mouse_pixels` state (~1643), `door_io_mouse_pixels()`/`door_io_note_pixel_report()` (~2272–2296), the DECRPM `'y'` case (~3059–3063), and the restore string (~1225).
- Test: rebuild + its existing boot/mouse path (no new unit test; the requirement is a null diff in emitted bytes).

**Interfaces:**
- Consumes: `termgfx/mouse.h` (Task 1).
- Produces: no new public interface. `door_io_mouse_pixels()` etc. keep their signatures (now backed by a `static termgfx_mouse_t g_mouse;`).

- [ ] **Step 1: Introduce the module state, delegate the accessors**

Replace `static int g_mouse_pixels;` with `static termgfx_mouse_t g_mouse;` (add `#include "mouse.h"`). Rewrite the accessors to delegate:
```c
int  door_io_mouse_pixels(void)      { return termgfx_mouse_pixels(&g_mouse); }
void door_io_note_pixel_report(void) {
    if (!termgfx_mouse_pixels(&g_mouse)) {
        termgfx_mouse_note_pixel_report(&g_mouse);
        fprintf(stderr, DOOR_SHORT_NAME ": SGR-Pixels auto-detected (report exceeded text grid)\n");
    }
}
```
(The log line is preserved verbatim so the byte output and node-log text are unchanged.)

- [ ] **Step 2: Route the enable/restore strings and DECRPM through the module**

- Enable send: replace the literal `"\x1b[?1003h\x1b[?1006h"` + `"\x1b[?1016h\x1b[?1016$p"` pair (~2773–2783) with `door_out_puts(termgfx_mouse_enable);`. **Verify the concatenation is byte-identical** to the two literals (it is: `?1003h?1006h?1016h?1016$p`). Keep the surrounding comments.
- Restore send (~1225): replace `"\x1b[?1003l\x1b[?1006l\x1b[?1016l"` with `door_out_puts(termgfx_mouse_restore);` (byte-identical).
- DECRPM `'y'` case (~3059–3063): replace the inline `if (np >= 2 && p[0] == 1016 ...) g_mouse_pixels = 1;` with `termgfx_mouse_on_decrpm(&g_mouse, p, np);` (parse `p`/`np` exactly as the existing case does).

- [ ] **Step 3: Build syncconquer**

Invoke the synchronet-build skill's door path.
Run: `cd src/doors/syncconquer && ./build.sh 2>&1 | tail -15`
Expected: clean build, no warnings from the edited file.

- [ ] **Step 4: Verify behaviour-identical emitted bytes**

The enable/restore strings are compile-time constants; confirm equality mechanically rather than by eye:
Run: `cd src/doors/termgfx && printf '%s' "$(./build/... )"` — simplest check is a tiny assert already covered by Task 1's test (`termgfx_mouse_enable`/`restore` equal the exact literals syncconquer used). Additionally grep the diff to confirm no other mouse bytes changed:
Run: `cd src/doors/syncconquer && git diff door/door_io.c | grep -E '^\+' | grep -E '1003|1006|1016' `
Expected: the only added `?1003/1006/1016` occurrences are inside comments; all live sends now go through `termgfx_mouse_enable/restore`.

- [ ] **Step 5: Uncrustify + commit**

```bash
cd src/doors/syncconquer
uncrustify -c ../../uncrustify.cfg --replace --no-backup door/door_io.c
git add door/door_io.c
git commit -F - <<'EOF'
syncconquer: use termgfx's shared mouse handshake

Delegate the DEC 1003/1006/1016 enable/restore, the SGR-Pixels latch and
the DECRPM reply to termgfx/mouse instead of open-coding them here. The
emitted bytes and the coordinate mapping are unchanged -- door_input.c
and the door_io_* accessors keep their behaviour; only their backing
state moves into a termgfx_mouse_t. First consumer of the extraction.
EOF
```

---

### Task 3: syncscumm — enable mouse, parse SGR reports, map to game coords, queue events

Add the mouse enable to the probe burst, parse SGR reports and the DECRPM reply in `csi_final()`, factor the present path's fit+center rect into a shared helper, map reports to 320×200 game coords, and expose an input-event FIFO + geometry accessors.

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.h` — add the event type + FIFO API.
- Modify: `src/doors/syncscumm/door/sst_io.c` — enable send (~1163), `csi_final()` `'M'/'m'/'y'` cases (~669), extract `sst_image_rect()` from the present fit+center math, add the mapper + FIFO.
- Test: `src/doors/syncscumm/test/test_sst_mouse.c` (new; add a build+run block to `test/unit_sst_io.sh` mirroring the `test_sst_io_canvas` block — see "Test harness" in Notes).

**Interfaces:**
- Consumes: `termgfx/mouse.h`; existing `g_canvas_w/h`, `g_cell_w/h`, `g_term_cols/rows` state in `sst_io.c`.
- Produces (in `sst_io.h`):
  ```c
  typedef enum {
      SST_EV_MOUSE_MOVE, SST_EV_MOUSE_DOWN, SST_EV_MOUSE_UP,
      SST_EV_WHEEL, SST_EV_KEY_DOWN, SST_EV_KEY_UP
  } sst_ev_type_t;

  /* Modifier bitmask (key events). */
  #define SST_MOD_SHIFT 1
  #define SST_MOD_ALT   2
  #define SST_MOD_CTRL  4

  typedef struct {
      sst_ev_type_t type;
      int x, y;        /* mouse: game coords, 0..SST_FB_W/H-1 */
      int button;      /* MOUSE_DOWN/UP: 0 left 1 middle 2 right */
      int wheel;       /* WHEEL: -1 up, +1 down */
      int keycode;     /* KEY_*: an SST_KEY_* code (Task 5) or a raw ASCII byte */
      int ascii;       /* KEY_*: printable char, else 0 */
      int mods;        /* KEY_*: SST_MOD_* bitmask */
  } sst_input_event_t;

  /* Pop the next queued input event. Returns 1 and fills *ev, or 0 if empty.
   * Drained by the ScummVM backend's pollEvent(). */
  int sst_io_next_event(sst_input_event_t *ev);
  ```

- [ ] **Step 1: Write the failing test (coordinate mapping + FIFO)**

`test/test_sst_mouse.c` drives the mapper through a small test seam. Each fresh-session test is its own binary (file-static state, no reset) — mirror `test_sst_io_canvas`. Add test-only entries (guarded `#ifdef SST_TEST` in `sst_io.c`) — `sst_io_test_set_geom(...)` sets the geometry globals to a known canvas/cell/grid, `sst_io_test_mouse_report(...)` calls the same internal `sst_mouse_report()` the parser calls — and let the test read the FIFO via `sst_io_next_event()`:

```c
#include <assert.h>
#include "sst_io.h"
int sst_io_test_set_geom(int canvas_w, int canvas_h, int cell_w, int cell_h,
                         int cols, int rows, int pixels);
int sst_io_test_mouse_report(int b, int col, int row, int release);

int main(void) {
    sst_input_event_t ev;

    /* Cell mode: 640x400 canvas, 8x16 cell, 80x25 grid, image fills canvas.
     * A click at the center cell maps near the center of the 320x200 game. */
    sst_io_test_set_geom(640, 400, 8, 16, 80, 25, /*pixels=*/0);
    sst_io_test_mouse_report(/*b=*/0, /*col=*/40, /*row=*/13, /*release=*/0);
    assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE);
    assert(ev.x > 140 && ev.x < 180 && ev.y > 80 && ev.y < 120);
    assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_DOWN && ev.button == 0);
    assert(!sst_io_next_event(&ev));   /* queue drained */

    /* Pixel mode: report already in canvas pixels. Top-left maps to (0,0). */
    sst_io_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/1);
    sst_io_test_mouse_report(0, 1, 1, 0);
    assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE && ev.x == 0 && ev.y == 0);

    /* Auto-detect: cell-mode geom but a report past the grid latches pixels. */
    sst_io_test_set_geom(1280, 800, 8, 16, 160, 50, /*pixels=*/0);
    sst_io_test_mouse_report(35, 900, 400, 0);   /* col 900 > 160 cols → pixels */
    assert(sst_io_next_event(&ev) && ev.type == SST_EV_MOUSE_MOVE);
    /* now interpreted as pixels: x ≈ 900/1280*320 */
    assert(ev.x > 210 && ev.x < 240);
    return 0;
}
```

- [ ] **Step 2: Run to verify it fails**

Add the build+run block to `test/unit_sst_io.sh` (see "Test harness" in Notes; add `-DSST_TEST` to that block's `cc` so the seams compile), then:
Run: `cd src/doors/syncscumm && ./build.sh >/dev/null 2>&1; sh test/unit_sst_io.sh 2>&1 | tail -8`
Expected: FAIL — `sst_io_test_*` / `sst_io_next_event` undefined.

- [ ] **Step 3: Implement — FIFO, rect helper, mapper**

In `sst_io.c`:
- Add a fixed-capacity ring FIFO of `sst_input_event_t` (e.g. 64 entries; drop oldest on overflow — input latency beats unbounded memory) with `static void sst_push_event(const sst_input_event_t *)` and the public `sst_io_next_event()`.
- Extract the fit+center rect from `sst_io_present()` (its "Fit + center" math, see `sst_io.c:76`) into `static void sst_image_rect(int *ew, int *eh, int *dx, int *dy)` and call it from both present and the mapper (DRY — the mouse must map against the exact rect the frame was drawn in, including the sixel reserved bottom row). Keep present's output byte-identical.
- Add `static termgfx_mouse_t g_mouse;` and the mapper, porting `syncconquer/door/door_input.c:305–417` (`mouse_event`) but: map to `SST_FB_W`×`SST_FB_H` (not `DOOR_FB_*`), push `SST_EV_*` events to the FIFO instead of `emit_mouse/emit_key`, and drop the legacy modifier-synthesis-on-click block (ScummVM reads discrete button events, not held-modifier polling — keyboard modifiers arrive as their own key events in Task 5). Grid comes from `g_term_cols/rows`; cell from `g_cell_w/h` (fallback 8×16); pixel latch via `termgfx_mouse_*`.
- Emit order per report: always a `SST_EV_MOUSE_MOVE` (position), then `SST_EV_MOUSE_DOWN`/`UP` for a button (release→UP), or `SST_EV_WHEEL` for a notch.

- [ ] **Step 4: Wire the parser + enable send**

- In `csi_final()` add:
  ```c
  case 'M': case 'm': {            /* xterm SGR mouse: ESC[<b;col;row M/m */
      int q[3];
      if (csi_params(q, 3) >= 3)
          sst_mouse_report(q[0], q[1], q[2], fin == 'm');
      return;
  }
  case 'y': {                      /* DECRPM: ESC[?1016;Ps$y */
      np = csi_params(p, 4);
      termgfx_mouse_on_decrpm(&g_mouse, p, np);
      return;
  }
  ```
  (`sst_mouse_report` is the internal entry the test seam also calls; it runs the auto-detect check against `g_term_cols/rows`, then the mapper.)
- In the probe burst (after `out_puts(termgfx_query_jxl);`, ~1163) add `out_puts(termgfx_mouse_enable);`. In `sst_io_shutdown()` add `out_puts(termgfx_mouse_restore);` before the terminal-restore (guard with `g_active`).

- [ ] **Step 5: Run tests to verify they pass**

Run: `cd src/doors/syncscumm && ./build.sh >/dev/null 2>&1; sh test/unit_sst_io.sh 2>&1 | tail -12` (runs `test_sst_mouse` plus the whole existing suite — `test_sst_io`, `test_sst_io_canvas`, etc. — confirming the rect extraction moved no frame). Also run `sh test/boot_bass.sh` and `sh test/boot_sixel.sh`.
Expected: all pass.

- [ ] **Step 6: Commit** (no uncrustify — hand-formatted door)

```bash
cd src/doors/syncscumm
git add door/sst_io.c door/sst_io.h test/test_sst_mouse.c test/unit_sst_io.sh
git commit -F - <<'EOF'
syncscumm: parse SGR mouse into a game-coordinate event queue

Enable DEC 1003/1006/1016 mouse (via termgfx/mouse) alongside the other
startup probes, parse the SGR reports and the DECRQM reply in
csi_final(), and map each report -- pixel-granular under 1016, else the
cell centre in real cell pixels -- onto the 320x200 game surface through
the same fit+center rect the frame is drawn in (factored out of the
present path so the two can't disagree). Reports land in a small input
FIFO the backend drains next. Coordinate-mapping unit test added.
EOF
```

---

### Task 4: syncscumm.cpp — deliver mouse events + warpMouse (PLAYABLE-MOUSE CHECKPOINT)

Drain the FIFO in `pollEvent()` into `Common::Event`s and call `warpMouse()` so the existing compositor draws the pointer at the cursor. After this task BASS is mouse-playable; keyboard (menus/save) follows in Tasks 5–6.

**Files:**
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` — `pollEvent()` (173–187).

**Interfaces:**
- Consumes: `sst_io_next_event()`, `sst_input_event_t`, `SST_EV_*` (Task 3); `_graphicsManager->warpMouse()` (exists).
- Produces: nothing new.

- [ ] **Step 1: Extend pollEvent to drain one input event per call**

Keep the tick calls and the quit/hangup check at the top. After them, before `return false;`:

```cpp
    sst_input_event_t iev;
    if (sst_io_next_event(&iev)) {
        switch (iev.type) {
        case SST_EV_MOUSE_MOVE:
            _graphicsManager->warpMouse(iev.x, iev.y);   /* compositor draws the cursor here */
            event.type = Common::EVENT_MOUSEMOVE;
            event.mouse = Common::Point(iev.x, iev.y);
            return true;
        case SST_EV_MOUSE_DOWN:
        case SST_EV_MOUSE_UP: {
            bool down = (iev.type == SST_EV_MOUSE_DOWN);
            event.mouse = Common::Point(iev.x, iev.y);
            if (iev.button == 0)
                event.type = down ? Common::EVENT_LBUTTONDOWN : Common::EVENT_LBUTTONUP;
            else if (iev.button == 2)
                event.type = down ? Common::EVENT_RBUTTONDOWN : Common::EVENT_RBUTTONUP;
            else
                event.type = down ? Common::EVENT_MBUTTONDOWN : Common::EVENT_MBUTTONUP;
            return true;
        }
        case SST_EV_WHEEL:
            event.mouse = Common::Point(iev.x, iev.y);
            event.type = (iev.wheel < 0) ? Common::EVENT_WHEELUP : Common::EVENT_WHEELDOWN;
            return true;
        default:
            break;   /* key events: Task 6 */
        }
    }
    return false;
```

Include note: `common/events.h` (already included) defines these types and `Common::Point` (`common/rect.h`, pulled in transitively). Add `#include "common/rect.h"` if the build complains.

- [ ] **Step 2: Build the door**

Run: `cd src/doors/syncscumm && ./build.sh 2>&1 | tail -15`
Expected: clean build.

- [ ] **Step 3: Headless smoke — cursor composites at the reported point**

Extend `scratchpad/capture_real.py` (the Foot-like pty harness) to, after the intro settles, send an SGR pixel report for a button press at a known canvas point and capture the next frame; decode with `sixdec.py` and confirm the cursor sprite appears at the mapped location. (If the harness can't easily assert pixels, this step is a manual visual check of the captured frame.)
Expected: cursor drawn at the clicked location; no crash.

- [ ] **Step 4: Commit**

```bash
cd src/doors/syncscumm
git add door/syncscumm.cpp
git commit -F - <<'EOF'
syncscumm: deliver mouse events to ScummVM

Drain the session's input FIFO in pollEvent(): motion becomes
EVENT_MOUSEMOVE (and warpMouse() so the existing compositor draws the
cursor there), buttons become L/M/R button events, wheel notches become
EVENT_WHEELUP/DOWN. Beneath a Steel Sky is now mouse-playable -- click to
walk and to drive the verb/inventory interface. Keyboard follows.
EOF
```

---

### Task 5: syncscumm — keymode negotiation + key decode into the FIFO

Negotiate kitty/evdev key modes via termgfx keymode, decode incoming keys (kitty CSI-u, evdev, legacy CSI/SS3 + printable bytes) into neutral `SST_KEY_*` codes, and queue them. Door-reserved keys (Ctrl-S stats, quit) are consumed, not forwarded.

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.h` — add the `SST_KEY_*` enum.
- Modify: `src/doors/syncscumm/door/sst_io.c` — keymode enable/restore in the probe burst + shutdown; extend the `P_NORMAL` byte handler (930–957) and the CSI/SS3 path to decode keys; **remove the `+`/`-` volume hotkey** (953–956) so those bytes become ordinary keys.
- Test: `src/doors/syncscumm/test/test_sst_input.c` (new; its own block in `test/unit_sst_io.sh`), using a `sst_io_test_feed(const char *bytes, size_t n)` seam that runs raw bytes through the parser and asserts emitted key events.

**Interfaces:**
- Consumes: `termgfx/keymode.h` — `termgfx_keymode_enable_kitty/evdev`, `termgfx_keymode_restore`, `termgfx_kitty_parse`, `termgfx_kitty_ctrl/shift/alt`, `termgfx_evdev_ascii`, `termgfx_evdev_modifier`, `termgfx_keymode_query_kitty`. Port the decode structure from `syncconquer/door/door_input.c` (kitty/evdev/legacy sections).
- Produces (in `sst_io.h`): neutral key codes
  ```c
  /* Non-ASCII keys carried in sst_input_event_t.keycode. Printable keys use
   * their ASCII value directly (keycode == ascii); these cover the rest. */
  enum {
      SST_KEY_FIRST = 0x100,
      SST_KEY_UP, SST_KEY_DOWN, SST_KEY_LEFT, SST_KEY_RIGHT,
      SST_KEY_HOME, SST_KEY_END, SST_KEY_PAGEUP, SST_KEY_PAGEDOWN,
      SST_KEY_INSERT, SST_KEY_DELETE,
      SST_KEY_ENTER, SST_KEY_ESCAPE, SST_KEY_BACKSPACE, SST_KEY_TAB,
      SST_KEY_F1, SST_KEY_F2, SST_KEY_F3, SST_KEY_F4, SST_KEY_F5,
      SST_KEY_F6, SST_KEY_F7, SST_KEY_F8, SST_KEY_F9
  };
  ```

- [ ] **Step 1: Write failing key-decode tests**

Add a `sst_io_test_feed(const char *bytes, size_t n)` seam (`#ifdef SST_TEST`) that runs raw bytes through the same parser `sst_io_pump()` uses, then assert:

```c
/* a printable byte 'a' → KEY_DOWN keycode=='a' ascii=='a' */
sst_io_test_feed("a", 1);
assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN && ev.keycode == 'a' && ev.ascii == 'a');

/* '+' is no longer eaten as a volume hotkey — it reaches ScummVM */
sst_io_test_feed("+", 1);
assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN && ev.keycode == '+' && ev.ascii == '+');

/* legacy arrow: ESC [ A → KEY_DOWN keycode==SST_KEY_UP */
sst_io_test_feed("\x1b[A", 3);
assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN && ev.keycode == SST_KEY_UP);

/* legacy F5: ESC [ 1 5 ~ → SST_KEY_F5 */
sst_io_test_feed("\x1b[15~", 5);
assert(sst_io_next_event(&ev) && ev.type == SST_EV_KEY_DOWN && ev.keycode == SST_KEY_F5);

/* Ctrl-S stays door-reserved (stats toggle) — NOT forwarded */
size_t before = 0; /* drain */ while (sst_io_next_event(&ev)) {}
sst_io_test_feed("\x13", 1);
assert(!sst_io_next_event(&ev));
```

- [ ] **Step 2: Run to verify it fails**

Add the `test_sst_input` block to `test/unit_sst_io.sh` (Notes), then:
Run: `cd src/doors/syncscumm && sh test/unit_sst_io.sh 2>&1 | tail -8`
Expected: FAIL (no key events queued; `+` still swallowed by the volume hotkey).

- [ ] **Step 3: Implement the decode**

In `sst_io.c`:
- Probe burst: after the mouse enable, send `out_puts(termgfx_keymode_query_kitty);` and enable kitty/evdev per the keymode API (mirror `door_input.c`'s enable + settling; use the existing `g_is_syncterm`/CTDA state to choose evdev). In shutdown send `termgfx_keymode_restore`.
- `P_NORMAL` handler (930–957): keep Ctrl-S (0x13) and quit (`q`/Ctrl-C) door-reserved. **Delete the `+`/`-` volume branch (953–956).** For every other byte, translate control/printable bytes to a key event:
  - `0x0d`→`SST_KEY_ENTER`, `0x1b` handled by the ESC state machine (a lone ESC that isn't an introducer → `SST_KEY_ESCAPE` on the reprocess path), `0x08`/`0x7f`→`SST_KEY_BACKSPACE`, `0x09`→`SST_KEY_TAB`; Ctrl-letters (`0x01`–`0x1a`, excluding the reserved ones) → keycode = the letter with `SST_MOD_CTRL`; printable `0x20`–`0x7e` → keycode == ascii == the byte.
- CSI/SS3 final-byte path: map arrows (`A/B/C/D`→UP/DOWN/RIGHT/LEFT), `H`/`F`→HOME/END, and the `~`-terminated numerics (`1~`/`7~` Home, `4~`/`8~` End, `5~` PageUp, `6~` PageDown, `2~` Insert, `3~` Delete, `11–15~`→F1–F5, `17–21~`→F6–F9) — the standard xterm table; SS3 `OP–OS`→F1–F4.
- kitty CSI-u (`u` final): `termgfx_kitty_parse()` → base keycode + `mod`/`ev`; map to `SST_KEY_*`/ascii and `SST_MOD_*` via `termgfx_kitty_ctrl/shift/alt`; only queue key-down/up per `ev`. evdev reports: `termgfx_evdev_ascii()`/`termgfx_evdev_modifier()`. Reuse the exact structure of `door_input.c`'s kitty/evdev sections; emit `SST_EV_KEY_DOWN`/`UP` to the FIFO instead of `emit_key`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd src/doors/syncscumm && sh test/unit_sst_io.sh 2>&1 | tail -12`
Expected: all pass; `+`/`-` now queue key events, Ctrl-S still reserved.

- [ ] **Step 5: Commit**

```bash
cd src/doors/syncscumm
git add door/sst_io.c door/sst_io.h test/test_sst_input.c test/unit_sst_io.sh
git commit -F - <<'EOF'
syncscumm: decode keyboard into the input event queue

Negotiate the kitty/evdev key modes through termgfx keymode and decode
kitty CSI-u, SyncTERM evdev and legacy CSI/SS3 keys into neutral key
codes queued for the backend. Ctrl-S (stats) and quit stay door-level;
everything else -- including + / - -- is forwarded, so ScummVM's own menu
and text fields see the keys. Drops the door + / - volume hotkey (the
engine's GMM Options dialog carries the volume sliders). Decode tested.
EOF
```

---

### Task 6: syncscumm.cpp — deliver key events to ScummVM

Translate the queued `SST_KEY_*`/ascii/mods into `Common::KeyState` and emit `EVENT_KEYDOWN`/`KEYUP`.

**Files:**
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` — the `default:` arm of the `pollEvent()` switch from Task 4.

**Interfaces:**
- Consumes: `SST_EV_KEY_DOWN/UP`, `SST_KEY_*`, `SST_MOD_*`.
- Produces: nothing new.

- [ ] **Step 1: Add a keycode translation + emit**

Replace the Task 4 `default: break;` with key handling:

```cpp
        case SST_EV_KEY_DOWN:
        case SST_EV_KEY_UP: {
            Common::KeyCode kc;
            uint16 ascii = (iev.ascii != 0) ? (uint16)iev.ascii : 0;
            switch (iev.keycode) {
            case SST_KEY_UP: kc = Common::KEYCODE_UP; break;
            case SST_KEY_DOWN: kc = Common::KEYCODE_DOWN; break;
            case SST_KEY_LEFT: kc = Common::KEYCODE_LEFT; break;
            case SST_KEY_RIGHT: kc = Common::KEYCODE_RIGHT; break;
            case SST_KEY_HOME: kc = Common::KEYCODE_HOME; break;
            case SST_KEY_END: kc = Common::KEYCODE_END; break;
            case SST_KEY_PAGEUP: kc = Common::KEYCODE_PAGEUP; break;
            case SST_KEY_PAGEDOWN: kc = Common::KEYCODE_PAGEDOWN; break;
            case SST_KEY_INSERT: kc = Common::KEYCODE_INSERT; break;
            case SST_KEY_DELETE: kc = Common::KEYCODE_DELETE; break;
            case SST_KEY_ENTER: kc = Common::KEYCODE_RETURN; ascii = Common::ASCII_RETURN; break;
            case SST_KEY_ESCAPE: kc = Common::KEYCODE_ESCAPE; ascii = Common::ASCII_ESCAPE; break;
            case SST_KEY_BACKSPACE: kc = Common::KEYCODE_BACKSPACE; ascii = Common::ASCII_BACKSPACE; break;
            case SST_KEY_TAB: kc = Common::KEYCODE_TAB; ascii = Common::ASCII_TAB; break;
            case SST_KEY_F1: kc = Common::KEYCODE_F1; break;
            case SST_KEY_F2: kc = Common::KEYCODE_F2; break;
            case SST_KEY_F3: kc = Common::KEYCODE_F3; break;
            case SST_KEY_F4: kc = Common::KEYCODE_F4; break;
            case SST_KEY_F5: kc = Common::KEYCODE_F5; break;
            case SST_KEY_F6: kc = Common::KEYCODE_F6; break;
            case SST_KEY_F7: kc = Common::KEYCODE_F7; break;
            case SST_KEY_F8: kc = Common::KEYCODE_F8; break;
            case SST_KEY_F9: kc = Common::KEYCODE_F9; break;
            default:
                /* printable / control ASCII: keycode == the byte */
                kc = (Common::KeyCode)iev.keycode;
                break;
            }
            byte flags = 0;
            if (iev.mods & SST_MOD_CTRL)  flags |= Common::KBD_CTRL;
            if (iev.mods & SST_MOD_ALT)   flags |= Common::KBD_ALT;
            if (iev.mods & SST_MOD_SHIFT) flags |= Common::KBD_SHIFT;
            event.type = (iev.type == SST_EV_KEY_DOWN) ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
            event.kbd = Common::KeyState(kc, ascii, flags);
            return true;
        }
```

- [ ] **Step 2: Build the door**

Run: `cd src/doors/syncscumm && ./build.sh 2>&1 | tail -15`
Expected: clean build. (`Common::KeyState`, `KEYCODE_*`, `ASCII_*`, `KBD_*` are in `common/keyboard.h`, pulled in via `common/events.h`.)

- [ ] **Step 3: Headless smoke — a key reaches the engine**

Feed an F5 (or Ctrl-F5, whichever the build's keymap binds to the Global Main Menu) through the pty harness after the intro and confirm the GMM opens in the captured frame (or that ScummVM logs the keymap action). Confirm typing letters into the save-name field works during the live pass (Task 7).
Expected: the engine reacts to the key; no crash.

- [ ] **Step 4: Commit**

```bash
cd src/doors/syncscumm
git add door/syncscumm.cpp
git commit -F - <<'EOF'
syncscumm: deliver keyboard events to ScummVM

Translate the queued key codes into Common::KeyState -- arrows, F1-F9,
Enter/Esc/Backspace/Tab/Home/End/PageUp-Dn/Insert/Delete and printable
ASCII, with Ctrl/Alt/Shift modifiers -- and emit EVENT_KEYDOWN/KEYUP.
The Global Main Menu (save/load/quit/options), cutscene skips and
save-name entry are now reachable, completing M3 input.
EOF
```

---

### Task 7: Live acceptance — BASS on SyncTERM and Foot

Not a code task; the milestone's done bar. Exit any running door session first (a live session pins the old binary).

- [ ] **SyncTERM (pixel mouse, JXL):** launch the door, play BASS — click to walk, drive the verb/inventory UI, right-click behaviour, open the Global Main Menu, Save and Load (type a save name), skip a cutscene (Esc / `.`), Quit cleanly. Confirm the cursor tracks smoothly (pixel granularity).
- [ ] **Foot (cell mouse, sixel):** repeat the same interactions. Confirm the cell-granular cursor still lands on hotspots and the GUI is usable. Watch for any interactive-sixel jank (the deferred quantizer gate — note it if present, don't fix here).
- [ ] Confirm the removed `+`/`-` volume hotkey is unmissed (volume via GMM → Options), and Ctrl-S stats + quit still work.
- [ ] Update the project memory (`project_syncscumm.md`) with the M3 result and any live-found issues.

---

## Notes for the implementer

- **Test harness (`test/unit_sst_io.sh`).** Unit tests live in `test/`, need `./build.sh` run first (they link `build/libs/termgfx/libtermgfx.a`, which includes `mouse.c` once Task 1 adds it to termgfx's CMake), and each fresh-session test is its own binary. Add a block per new test mirroring the `test_sst_io_canvas` block, adding `-DSST_TEST` so the seams compile:
  ```sh
  cc -o /tmp/test_sst_mouse -DSST_TEST $JXL_DEFINE -I"$DOOR/door" -I"$DOOR/../termgfx" $XPDEV_INC \
     "$HERE/test_sst_mouse.c" "$DOOR/door/sst_io.c" \
     "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" \
     -lpthread -lm $JXL_LIBS $SNDFILE_LIBS
  /tmp/test_sst_mouse
  ```
  (Same shape for `test_sst_input`.) The `SST_TEST` seams — `sst_io_test_set_geom()`, `sst_io_test_mouse_report()`, `sst_io_test_feed()` — are `#ifdef SST_TEST` blocks in `sst_io.c`; the shipped door never defines `SST_TEST`.
- **Read order:** syncconquer's `door/door_io.c` (mouse enable/latch/DECRPM, `door_calc_rect`, `door_io_cell_size`) and `door/door_input.c` (`mouse_event` 305–417; the kitty/evdev/legacy key sections) are the reference implementation for Tasks 1/3/5 — port their structure, don't reinvent it.
- **The rect must match the frame.** The sixel tier reserves the bottom row; the mapper must use the same rect the current tier drew, or clicks near the bottom miss. That's why Task 3 factors `sst_image_rect()` out of the present path rather than recomputing.
- **One event per pollEvent call.** ScummVM polls in a loop until `pollEvent` returns false; return exactly one queued event per call and keep the per-call `checkTimers()`/mixer `tick()` at the top (audio must keep flowing while input drains).
- **Don't forward door-reserved keys.** Ctrl-S and quit are consumed in `sst_io.c` and never reach the FIFO, so ScummVM never sees them.
```
