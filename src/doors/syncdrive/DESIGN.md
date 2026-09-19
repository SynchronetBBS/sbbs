# SyncDrive: Test Drive (1987) as a Synchronet door

**Status:** design approved 2026-09-18, pre-implementation. v1 needs no
termgfx changes; sec. 14 is deferred until the fit has been judged live.
**Binary:** `syncdrive`. **Package:** `xtrn/testdrive/`.
**Engine:** [kylofon/test-drive-sdl3](https://github.com/kylofon/test-drive-sdl3)
(`tdport/`, MIT), a faithful C reimplementation of Accolade / Distinctive
Software's *Test Drive* (1987), EGA build (`TDEGA.EXE`).
**Canvas:** native 320x200, 16 EGA colors.

SyncDrive renders the faithful Test Drive port to a BBS caller's terminal
through termgfx's shared session engine (`termgfx_termio`), the way SyncRPG and
SyncSCUMM do. Single player, keyboard only.

## 1. Goals / non-goals

**Goals**
- Play Test Drive over SyncTERM and any other sixel terminal.
- Use the whole SyncTERM screen: the status line hidden where the terminal
  allows it (termio does this, sec. 10). The picture starts at true aspect
  (614x384 on SyncTERM at 80x25, with side bars); Ctrl-F switches to fill
  (640x384) so both can be judged live before choosing a default (sec. 14).
- Drive with held keys on terminals that report key releases (kitty, SyncTERM
  evdev) and with the original key-repeat behavior everywhere else.
- PC-speaker sound through the termgfx audio stream.
- One BBS-wide high-score table, with entries named by the caller's BBS alias.

**Non-goals (v1)**
- CGA and Hercules builds (`TDCGA.EXE`).
- The testdrive-enhanced renderer (1280x800, 60 fps, a render thread pool per
  process). Possible later as a sysop option at a low `--res-scale`; it hooks in
  through `gfx_set_overlay()` and does not change the engine.
- The `sbbs_node` who's-online / paging overlay.
- Mouse and joystick input.
- The JPEG XL tier. termgfx encodes JXL lossy (distance 2.0) from the
  upscaled RGB frame, which smears the hard edges and 1-pixel text of 16-color
  pixel art; sixel is lossless for this content, and with dirty-rectangle
  updates and palette subsetting it is already small. SyncMOO1, the other
  320x200 palettized door, is sixel-only for the same reason. The door builds
  termgfx without `WITH_JXL`, which also drops the libjxl dependency.

## 2. Why this engine, and why a native door

- The engine is small (about 10K lines of C), single-threaded, and talks to its
  platform only through `host.h`. Only upstream's `host.c` and `main.c` include
  SDL, plus a handful of `SDL_malloc`/`SDL_LoadFile` calls in `mem.c`.
- The original's 16-color 320x200 screen at 8 fps while driving is cheap to
  carry as sixel: few colors, and only the road area changes between frames.
- SyncRetro's tier rule (a game with a native reimplementation gets a native
  door, never a libretro core) rules out running `TD.EXE` in a DOSBox core.

## 3. Game data (sysop-supplied)

The engine loads `TDEGA.EXE` itself into an emulated real-mode address space:
it unpacks EXEPACK, applies relocations, and keeps every game global at its
original address. It identifies the build by two strings at fixed DGROUP
offsets, so only that exact release works. It also reads `CARS.TXT`,
`TDSND.SND`, the `*.PES` archives, and the car `*.BIN` / `*.SS` files.

Test Drive is abandonware, not freeware: the rights passed from Accolade to
Infogrames/Atari, and reportedly to Bigben/Nacon. No game data ships with the
door. The copy at
`https://archive.org/download/TestDrive_1987/Test%20Drive.zip` was verified
against the engine's loader: `TDEGA.EXE` is 66517 bytes, md5
`cdf8d1a767d559db559a0b829e1bb3b0`, and loads with image size 81424 bytes,
DGROUP 1C9A.

`xtrn/testdrive/getdata.js` follows the SyncMOO1 model: the sysop drops the zip,
the loose files, or an extracted game folder into the door directory, and the
script copies the needed files into place. It downloads nothing. It verifies
the result by running `syncdrive --check`, and seeds `data/testdrive/SCORES`
from the copy's `SCORES` file when no table exists yet.

## 4. Layout

```
src/doors/syncdrive/
  tdport/               frozen snapshot of upstream tdport/src (minus host.c,
                        main.c), plus LICENSE, PORTING.md, FORMATS.md
  compat/SDL3/SDL.h     maps SDL_malloc/calloc/realloc/free/LoadFile/GetError
                        to libc so tdport/mem.c compiles unedited
  door/
    syncdrive.c         main(): termio init, argv, mem_load_exe, game_main
    host_term.c         implements tdport's host.h on termgfx_termio
    host_term_ext.h     door-only host calls used by the score patch (sec. 8)
    keymap.c/.h         termgfx key events -> XT scan-code key words
    keyscript.c/.h      SYNCDRIVE_KEYS key script for headless test runs
    alias.c/.h          BBS alias -> the game's 15-character name field
    frame.c/.h          XRGB frame -> indexed 320x200 + palette
    speaker.c/.h        PIT divisor + gate -> band-limited square-wave PCM
    scores_lock.c       cross-process lock around the SCORES merge
  tests/                unit tests for the door/ modules
  CMakeLists.txt  build.sh  deploy.js   (Windows build.bat: follow-up)
  PROVENANCE.md  CLAUDE.md  README.md
xtrn/testdrive/
  install-xtrn.ini  getdata.js  README.md  syncdrive.example.ini
```

`host_term.c` is split so the pure parts (key mapping, frame conversion,
speaker synthesis) can be tested without an engine or a terminal.

## 5. Build

One CMake stage, like SyncMOO1: `add_subdirectory(../termgfx)` and xpdev, the
tdport sources, and our door sources into one `syncdrive` executable. termgfx
is built without `WITH_JXL` (sec. 1). The include path puts `compat/` ahead of
any system SDL, so `mem.c` sees the shim.
tdport is compiled with its own warning set absorbed in the build (upstream
uses `-std=c11 -fno-strict-aliasing`); house style (`../../uncrustify.cfg`)
applies to our files only. Git build info comes from `synchronet_gitinfo()`.
No `install()`: `deploy.js` (via `exec/load/door_deploy.js`) places the binary.

## 6. The host (`host_term.c`)

The engine owns the main loop: every busy-wait in the original calls
`host_pump()` once per iteration. `host_pump()`:

1. `termgfx_termio_pump()`; exit on `hung_up()` or `quit_requested()`.
2. Run the 100.0404 Hz ticks that are due, measured on a monotonic clock
   (`PIT_DIV_GAME` = 11927 PIT counts per tick). Cap the catch-up so a stall
   does not replay seconds of ticks at once.
3. Drain input events into the BIOS key queue and the held-key table (sec. 7).
4. Synthesize speaker PCM for the ticks just run and feed
   `termgfx_termio_audio_stream()`.
5. Call the frame source; when it reports a change, convert and present.
   Call `termgfx_termio_tick()` so a frame deferred by pacing still goes out.
6. Sleep briefly when no tick was due, so polling loops do not spin a CPU.

`host_frame_begin()` / `host_set_frame_rate()` keep upstream's 8 fps driving
pace (frame-counted behavior depends on it). `host_present_now()` presents
immediately, for the unpaced dissolve effects.

**Video.** The engine composes an XRGB 320x200 frame. `frame.c` maps each pixel
to its index in the current 16-entry EGA palette (built from the palette the
engine last set, with a lookup fallback for any color not in it) and presents
through `termgfx_termio_present()`. Staying indexed keeps sixel palette
subsetting and SyncTERM's persistent color registers. The 4:3 display aspect
comes from termio's aspect handling. The door starts in termio's default
true-aspect fit; Ctrl-F calls `termgfx_termio_fit_cycle()` to toggle fill,
the same key SyncConquer and SyncSCUMM use for it.

**Files.** `host_game_path()` does the case-insensitive lookup in the door's
working directory (the door dir, where the data lives), except `SCORES`, which
resolves to `<data-dir>/SCORES` (sec. 8).

**Errors.** `host_fatal()` logs the message to the door's stderr log, shows it
on the caller's screen for a moment, and exits with status 3 like the original.

## 7. Input

Keyboard only: `termgfx_termio_set_mouse(0)` before init, so mouse motion
neither costs bandwidth nor resets the idle clock.

| Terminal key | Engine key word |
|---|---|
| arrows, Home/End/PgUp/PgDn, keypad 5 | XT scan codes 0x47..0x51 (the gear gate and steering) |
| A, Z | 0x1E, 0x2C |
| Enter, Esc, Backspace, printable ASCII | scan code + ASCII, for menus and text entry |
| F2 | toggles the game's sound (injects the game's Ctrl-Q / Ctrl-S) |
| Ctrl-F | door key: toggles true-aspect / fill fit (`termgfx_termio_fit_cycle()`), as in SyncConquer and SyncSCUMM |

- termgfx reserves Ctrl-S (stats bar) and Ctrl-Q / Ctrl-C (quit), so the game's
  own sound keys cannot reach it; F2 replaces them.
- Ctrl-J (the game's switch to joystick control) is dropped: `host_joy_read()`
  always reports no joystick. Ctrl-K (keyboard control) is harmless and passes.
- Ctrl-P (the game's pause) passes through; termio does not reserve it.
- **Held keys.** `host_xt_key_down()` answers from a held-key table fed by
  KEY_DOWN / KEY_UP events. The door starts in the original key-repeat mode
  (`host_set_held_keys(false)`) and switches to held-key mode the first time
  the terminal delivers a KEY_UP, which only kitty and SyncTERM evdev
  reporting do. A terminal without release events keeps the original behavior
  for the whole session.

## 8. High scores: the one engine patch

Upstream `high_scores()` loads `SCORES`, waits while the player types a name,
then writes the whole table back. On a BBS that has two problems: two nodes
that qualify at the same time lose one entry (the last write wins), and the
typed name can be anything.

The patch to `tdport/game/flow_scores.c`, recorded in `PROVENANCE.md`:

- **Name.** When the door has a caller alias (from DOOR32.SYS line 7 via
  termio, exposed as `host_player_name()` in `host_term_ext.h`),
  `scores_enter_name()` draws the alias in the entry box instead of calling
  `text_input_line()`, then waits for a key or 3 seconds. The alias is fixed;
  the player cannot edit it. It is truncated to the field's 15 characters, and
  any character outside the game font is replaced with `?`. With no alias (a
  local dev run), upstream's editable prompt remains.
- **Merge.** The qualifying entry is inserted into a freshly read table under a
  lock: `host_scores_lock()`, `scores_load()`, insert if the score still
  qualifies against the current table, `scores_save()`, `host_scores_unlock()`.
  The lock is xpdev's `lock()` on `<data-dir>/SCORES.lck`, so it holds across
  processes and across hosts sharing the data directory over SMB.

`SCORES` keeps the original file format (lines with the CRC check), so a copy
remains readable by the DOS game.

## 9. Audio

`host_speaker(divisor, on)` sets the PIT channel 2 frequency (1193182 /
divisor Hz) and gate. `speaker.c` renders a band-limited square wave at
termio's mixer rate (24 kHz, stereo interleaved) with a short attack/release
ramp on gate changes, so on/off edges do not click. PCM goes to
`termgfx_termio_audio_stream()`, which drops it on sessions that cannot play
digital audio. Level is set with headroom below full scale (a square wave's
RMS equals its peak); the sysop's `[audio]` volume applies after decode.

## 10. BBS integration

`install-xtrn.ini`:

```
[prog:TESTDRIV]
name     = Test Drive
cmd      = syncdrive%. %f --data-dir=%jtestdrive/
type     = XTRN_DOOR32
settings = XTRN_NATIVE | XTRN_BIN | XTRN_MULTIUSER | XTRN_NODISPLAY
```

The startup directory is the door dir (the game data). `--data-dir` holds
`SCORES` and is created with `mkpath()` at startup. termio handles DOOR32.SYS,
the socket, `-t` time left, the idle clock, the Ctrl-S stats bar, the graphics
gate for text-only terminals (as in SyncRPG and SyncSCUMM), and teardown. It
also hides the SyncTERM status line: termio sends the hide sequence before the
terminal probe, so the probed screen size includes the reclaimed row, and
restores the status-line type it read back from the terminal when the door
exits. A terminal without a status line ignores the sequence. The door adds
nothing for this.
Door-only argv entries are stripped with `termgfx_termio_consumed()` before the
engine's options (`--frame-rate`, `--bios-keys`) are parsed. `--check` runs
the loader alone and exits, for `getdata.js`.

Door settings, `syncdrive.ini` (template `syncdrive.example.ini`): termio's
standard keys (`sixel_max`, `[audio]`), plus `[input] held_keys = auto|off`
and `[game] frame_rate = 8`.

## 11. Testing

- **Unit** (`tests/`): keymap (every mapped key, the F2 injection, Ctrl-J
  dropped, the held-key latch), frame conversion (palette mapping, a color not
  in the palette), speaker (frequency from divisor, silence when gated, no
  step at gate edges).
- **Headless:** a capture-mode run (`SYNCDRIVE_SIXELOUT`) with a scripted key
  sequence reaches the title and car-select screens; the captured frames are
  inspected.
- **Live, SyncTERM:** title, car select, one full stage in both input modes,
  a crash, the police chase, results, high-score entry showing the locked
  alias, and a clean return to the BBS prompt. Both fit modes (Ctrl-F) viewed
  while driving, to settle the default (sec. 14). Two simultaneous qualifying
  sessions both land in the table.

## 12. Provenance

tdport is a copied, frozen snapshot of upstream `tdport/src` at a recorded
commit, not a submodule. `PROVENANCE.md` lists the upstream commit and date,
the MIT license, the files omitted (`host.c`, `main.c`), the compat shim, and
the single local patch (sec. 8). The reverse-engineering specs and tools stay
upstream (`port/`, `tools/`); `PORTING.md` and `FORMATS.md` are copied for
reference. `CLAUDE.md` in the door dir states the ours-vs-vendored rule.

## 13. Open questions for planning

1. How the engine's palette reaches `gfx_compose()` (for the indexed mapping in
   `frame.c`), and whether `gfx.c` exposes it without a patch.
2. Whether termio exposes the DOOR32.SYS alias, or `syncdrive.c` reads it with
   `termgfx_door32_read()`.
3. How `text_input_line()` and the game font handle characters outside ASCII,
   to define the `?` substitution exactly.
4. The exact sleep/tick policy in `host_pump()` against termio's pacing, so the
   8 fps driving loop neither stalls on backpressure nor spins.

## 14. Deferred: termgfx additions for a full-width default

v1 ships termio's current fit. Which of the two looks becomes the default is
decided after seeing both live (Ctrl-F):

- **Side bars kept:** nothing below is built. At 614 wide the scale is 1.92x
  on both axes, which is not an integer, so terminal-side scaling could not
  apply anyway.
- **Full width chosen:** either the door calls `termgfx_termio_set_fit_fill(1)`
  at startup (all scaling door-side, no termgfx change), or the two additions
  below are built to get the same picture with SyncTERM doing the horizontal
  doubling, about half the sixel bytes.

The rest of this section is the design for those additions, kept for that
decision.

On SyncTERM at 80x25 with the status line hidden, the canvas is 640x400.
termio keeps a sixel off the last text row (a final band on that row scrolls
the page), leaving 640x384. Today termio then fits the 320x200 frame at true
aspect, 614x384 with 13-pixel side bars, scaled 1.92x on both axes entirely
door-side, so pixel columns come out uneven. SyncMOO1 solved this in its own
I/O layer (8% stretch allowance, terminal-side 2x horizontally); SyncDrive
needs it from termio. Two additions to `termgfx_termio`, each opt-in and off by
default, so SyncRPG and SyncSCUMM keep their current output byte for byte.

**1. Stretch allowance.**
`void termgfx_termio_set_fit_stretch(int pct)`, called before
`termgfx_termio_init()`. The aspect fit passes `pct` to
`termgfx_geom_fit_ex()` instead of 0, so a fit within `pct` percent of the
canvas fills it. With 8, the frame fills 640x384 on SyncTERM (a 4% vertical
squash) and keeps true aspect in any canvas that is further off. FILL mode
(`termgfx_termio_fit_cycle()`) is unaffected.

**2. Terminal-side sixel scaling.**
`void termgfx_termio_set_sixel_upscale(int enable)`, called before
`termgfx_termio_init()`.

- Applies to CTerm terminals only (SyncTERM, identified by the DA1/CTDA reply
  termio already parses), which implement the raster attribute's pan/pad as an
  integer pixel scale. Every other terminal keeps today's door-side scaling.
  termio does not run `termgfx_sixel_vscale_probe()`, which would put a probe
  sixel on the wire ahead of the first frame; its test suite requires the
  first image sent to be the frame (see `sixel.h`, "WHICH DOORS RUN THIS").
- Per axis, `termgfx_geom_sixel_scale()` picks the largest factor (at most 2)
  whose encode still carries the whole native frame. For 320x200 into 640x384
  that is pad 2 (320 encoded columns, doubled by the terminal) and pan 1 (200
  rows scaled door-side to 384). Full frames use `sixel_encode_aspect()`.
- The dirty-rectangle path encodes each box at the reduced size with the same
  pan/pad. Box x positions are cell-aligned in display pixels, so at pad 2 they
  map to native columns in steps of cell width / 2 (4 for 8-pixel cells);
  box heights keep the existing cell-and-band rounding
  (`vstep` = LCM(cell height, 6)), which also keeps SyncTERM clear of the
  partial-band bug (SF #258).
- The stretch and scale choices are made in `termgfx_image_rect_src()`, the one
  place that sizes both the full frame and the dirty boxes, so the two cannot
  disagree about geometry.

**Sixel on SyncTERM is now a main path.** With JXL off (sec. 1), SyncTERM gets
the sixel full-frame and dirty-rectangle paths instead of JXL. Comments in
`termgfx_termio.c` that say SyncTERM never reaches the sixel dirty path are
corrected as part of this work.

**Tests.** termgfx's existing termio tests pass unchanged with both options
off, which is how SyncRPG and SyncSCUMM run. New tests with both on, for a
CTerm peer: a full frame declares pan 1 / pad 2 and a 320-column raster at
640x384; dirty boxes carry the same pan/pad, native widths, and cell-aligned
positions; a non-CTerm peer gets the door-side scaled encode.

