# SyncSCUMM M3 — Keyboard & mouse input (design)

Date: 2026-07-17
Status: approved by user (design dialogue in session "syncscumm")
Predecessors: `2026-07-14-syncscumm-design.md` (door design),
M2 plan `../plans/2026-07-15-syncscumm-m2.md` (terminal video),
M4 design `2026-07-16-syncscumm-m4-audio-design.md` (audio)

## Goal

Make a ScummVM adventure playable — starting with Beneath a Steel Sky
(BASS) — by delivering real **keyboard and mouse** events into the
engine. Today `OSystem_Synchronet::pollEvent()` only ever emits
`EVENT_QUIT`; no key or pointer input reaches ScummVM, so a point-and-click
game cannot be played at all.

Acceptance bar (user-set): BASS fully playable on **SyncTERM** (pixel
mouse, JXL video) and **Foot** (cell mouse, sixel video) — walk by
clicking, use the verb/inventory interface, open ScummVM's Global Main
Menu (GMM) to save/load/quit, and skip cutscenes (Esc / `.`).

## Prerequisite already in place

The graphics side of the cursor is **already built** (landed in M2 for the
GUI overlay): `door/video_term.cpp` implements `setMouseCursor()`,
`warpMouse()`, `showMouse()`, `setCursorPalette()`, and `compose()` already
composites "game + cursor (+ overlay)". M3 is therefore purely the **input
plumbing** — turning terminal input into `Common::Event`s and calling
`warpMouse()` so the already-working compositor draws the pointer.

## Approach (user-approved)

Mouse is **absolute point-and-click**, modelled on syncconquer's
`door/door_input.c` + `door/door_io.c`, which already proved the whole
approach: SGR pixel mouse (DEC 1016) with an automatic **cell** (DEC 1006)
fallback, pixel-mode auto-detection, and canvas-pixel → game-framebuffer
rescaling. syncdoom's cell-based *steering* model is deliberately **not**
used — it is the wrong model for a point-and-click cursor.

The one architectural decision (user chose **B**): the mouse handshake is
**extracted into termgfx first**, syncconquer is refactored onto it, and
syncscumm then consumes the shared module — rather than duplicating the
handshake door-side and extracting later. Only the button-field classifier
(`termgfx_sgr_classify()`) is shared today; the enable/probe/latch/DECRPM
logic currently lives inside syncconquer and moves to termgfx here.

Keyboard reuses termgfx's existing key-mode negotiator
(`termgfx/keymode.{c,h}` — kitty CSI-u + SyncTERM evdev) plus a door-side
legacy CSI/SS3 decoder, mirroring syncconquer's `door_input.c`. syncscumm
maps the decoded keys to ScummVM `Common::KeyState` instead of Win32 `VK_*`.

## The mouse fallback ladder (automatic)

The terminal selects the tier by what it reports back — no manual switch:

- **Pixel (DEC 1016):** we send `?1006h` + `?1016h` + the `?1016$p` DECRQM
  probe. If the terminal confirms 1016 (DECRPM `$y` reply) **or** auto-detect
  fires (a reported coordinate exceeds the text grid — only pixels can do
  that), reports are read as canvas pixels and mapped straight to game
  coords. This is the SyncTERM (≥1.330) path.
- **Cell (DEC 1006):** a terminal that ignores `?1016h` still sends 1006
  cell coordinates. The pixel latch stays false, so the door maps each cell
  to its centre in canvas pixels (using the real `ESC[16t` cell size) then
  rescales to game coords. This is the Foot path — adequate for BASS (even
  Foot's 6×13 cell over a ~4× upscaled 320×200 game is ≈1.5 game-px
  horizontal granularity, and BASS hotspots are large).
- **None:** a terminal with no SGR mouse sends nothing → keyboard-only.
  Keyboard-cursor emulation (arrow keys driving a virtual pointer) is
  **deferred**; such a terminal still gets keyboard for menus, just no
  pointer.

## Architecture

### New shared component: `termgfx/mouse.{h,c}`

Terminal-protocol mouse state only — **no game geometry**. Extracted from
syncconquer's `door_io.c`.

- `termgfx_mouse_t` — opaque-ish state struct: pixel-mode latch, and
  whatever the enable/probe path needs.
- Enable / restore emit strings:
  - enable: `?1000h?1002h?1003h` (motion tracking) + `?1006h` (SGR) +
    `?1016h` (SGR-Pixels) + `?1016$p` (DECRQM pixel probe).
  - restore: `?1003l?1006l?1016l` (syncconquer's current teardown string).
- `termgfx_mouse_on_decrpm(m, params, n)` — feed a parsed DECRPM
  `ESC[?1016;Ps$y` reply; latches pixel mode when `Ps` says 1016 is set.
- `termgfx_mouse_note_pixel_report(m)` / `termgfx_mouse_pixels(m)` — the
  auto-detect latch + query (report-past-the-grid heuristic; the *caller*
  owns the grid dimensions and decides when to call `note`).
- `termgfx_mouse_report(m, b, x, y, release, out)` — classify a raw SGR
  report via `termgfx_sgr_classify()`, filling
  `out = { kind, button, wheel, x, y, pixels }`. Coordinate **mapping stays
  door-side** (each game's framebuffer + displayed-image rect differ).

Boundary rule: the module knows the mouse *protocol*; each door owns its
*coordinate space*. This mirrors the split `sgrmouse.h` already documents.

### syncconquer refactor

Replace the inline enable / pixel-latch / DECRPM logic in `door_io.c`
(`door_io_mouse_pixels`, `door_io_note_pixel_report`, the `?1016$p` send,
the `$y` parse, the enable/restore strings) with calls into
`termgfx/mouse`. Keep its canvas→game mapping (`door_calc_rect`,
`door_io_cell_size`) and `door_input.c` unchanged. Requirement:
**behaviour-identical** — verified by rebuild + its existing boot/mouse
path. This is the shared-behaviour regression risk called out in
`termgfx/CLAUDE.md`; treat any diff in conquer's emitted bytes as a defect.

### syncscumm mouse wiring: `door/sst_io.c` + `door/syncscumm.cpp`

`sst_io.c`:
- Own a `termgfx_mouse_t`; emit the enable string at init, restore at
  shutdown.
- In `sst_io_pump()`'s CSI parser, handle the SGR mouse final bytes
  `M`/`m` → `termgfx_mouse_report()`; feed DECRPM `$y` → the module.
- Map the reported `x,y` to the **active coordinate space** using existing
  `g_canvas_*` / `g_cell_*` + the displayed-image rect:
  - **game space** normally (current game width/height), and
  - **overlay space** (`SST_FB_W × SST_FB_H`) when the GUI overlay is
    visible. The graphics manager knows overlay state; `sst_io` queries it.
    Both `event.mouse` and the `warpMouse()` call must use this same space.
- Maintain the current pointer `(x,y)` and a button bitmask; expose a small
  FIFO of pending input events (mouse + key) to the backend via a
  `sst_io_next_event()`-style API.

`syncscumm.cpp` `pollEvent()`:
- After the existing quit/hangup check, drain `sst_io`'s event FIFO:
  - motion → `EVENT_MOUSEMOVE`, `event.mouse` = mapped point, **and** call
    `_graphicsManager->warpMouse(x,y)` so `compose()` draws the cursor;
  - buttons → `EVENT_LBUTTONDOWN/UP`, `EVENT_RBUTTONDOWN/UP`
    (`EVENT_MBUTTONDOWN/UP` if middle seen);
  - wheel → `EVENT_WHEELUP` / `EVENT_WHEELDOWN`.
- Return one event per call (ScummVM polls in a loop); keep the tick calls
  (`checkTimers()`, mixer `tick()`) that already run at the top.

### syncscumm keyboard wiring: `door/sst_io.c` + `door/syncscumm.cpp`

`sst_io.c`:
- Negotiate key mode at init via `termgfx/keymode` (kitty / evdev enable +
  settling window), restore at shutdown.
- Decode incoming keys — kitty CSI-u via `termgfx_kitty_parse()` +
  `termgfx_kitty_ctrl/shift/alt()`, SyncTERM evdev via
  `termgfx_evdev_ascii()` / `termgfx_evdev_modifier()`, and a door-side
  legacy CSI/SS3 decoder for plain terminals — into a neutral
  `{ keycode, ascii, mods, down }`, mirroring `door_input.c`. Push onto the
  same event FIFO as mouse.

`syncscumm.cpp` `pollEvent()`:
- Key events → `EVENT_KEYDOWN` / `EVENT_KEYUP` with
  `Common::KeyState{ keycode = KEYCODE_*, ascii, flags = KBD_CTRL/ALT/SHIFT }`.
- Mapping: arrows → `KEYCODE_UP/DOWN/LEFT/RIGHT`; `F1..F9` →
  `KEYCODE_F1..F9`; `Enter/Esc/Backspace/Tab/PageUp/PageDown/Home/End` →
  their `KEYCODE_*`; printable ASCII passes through (`keycode` = the ascii
  value for a–z/0–9 per ScummVM convention, `ascii` = the character).

### Hotkey reconciliation (user decision)

The door currently eats `+`/`-` as a stream-volume hotkey, which would
shadow typing `+`/`-` into a ScummVM save-name field. **Resolution: remove
the door-level `+`/`-` volume hotkey and pass `+`/`-` through to ScummVM.**
Volume is controlled in-engine via the GMM → Options **Music / SFX / Speech
volume** sliders (`gui/options.cpp`), which scale the PCM before the door
streams it — same audible result for the listener, so nothing playable is
lost. The removed `+`/`-` controlled the *terminal stream* level in dB
(output headroom); the sliders control the *internal mix*. **Ctrl-S**
(stats bar) and the quit hotkey stay door-level — neither collides with GUI
text entry.

## Quantizer-stability gate — deferred (user-approved)

Earlier notes said the sixel quantizer's per-frame LUT churn should be
fixed *before* mouse input. Deferred out of M3: cursor moves are
dirty-rect-bounded (only the pointer box re-encodes), and the primary BASS
target (SyncTERM) uses JXL, which never touches the quantizer. Revisit only
if interactive **sixel** on Foot proves janky in the live pass. The prior
findings (#3/#5/#6/#7/#9 from the M2 review) remain tracked for a later
quantizer pass.

## Testing

- `termgfx/mouse` unit test (same style as `sgrmouse` / `xterm_ceiling`):
  feed raw button codes + DECRPM `$y` replies + past-the-grid coordinates;
  assert classification, pixel-latch (both the DECRPM and auto-detect
  paths), and the enable/restore emit strings.
- syncconquer regression: rebuild + run its existing boot/mouse path;
  confirm its emitted mouse bytes are unchanged (byte-identical to
  pre-refactor).
- syncscumm coord-map unit test: exercise canvas-px → game and
  canvas-cell → game/overlay mapping in isolation (no ScummVM link needed),
  including the overlay-vs-game space switch and image-rect clamping.
- Harness: extend `scratchpad/capture_real.py` (the Foot-like pty harness)
  to inject SGR mouse reports + key sequences and observe the door's
  behaviour end to end.
- Live acceptance: BASS on SyncTERM (pixel) and Foot (cell) — walk via
  clicks, drive the verb/inventory UI, open the GMM (save/load/quit), skip a
  cutscene. This is the milestone's done bar.

## Out of scope for M3

- Keyboard-cursor emulation for mouse-less terminals (deferred tier 3).
- The sixel quantizer-stability pass (deferred, above).
- Per-user save-dir wiring and the runtime-state `.gitignore` (separate
  deferred housekeeping, tracked in the project memory / `install-xtrn.ini`).
- Any change to JXL/sixel video, audio, or the capability-probe flow beyond
  adding the mouse enable/probe alongside the existing probes.
