# Porting rules — Test Drive (1987) → SDL3

Faithful reimplementation of **TDEGA.EXE**. Behaviour, timing, integer arithmetic and visible quirks
must match the original. Specs: `../port/spec/*.md` (read `../port/README.md` and `../port/RE_GUIDE.md`
first). Symbols: `../port/symbols.csv` → generated `src/symbols.h`. Ground truth when a spec is unclear:
`python ../tools/x86dis.py ../work/TDEGA_unp.exe dis <image hex offset> <len>` (run from `tdport/`).

## Architecture

```
main.c        args, mem_load_exe(), host_init(), game_main()
mem.h/.c      real-mode memory: TDEGA.EXE image at segment 0x1000, DGROUP 0x1C9A, heap above   (coordinator)
host.h/.c     SDL3: window/present, 100.0404 Hz tick, BIOS keyboard queue, gamepad, speaker, files (coordinator)
symbols.h     generated DS_*/FN_* constants                                                       (coordinator)
platform/gfx.*      planar graphics: targets, blitters, fills, text, dissolve, scroll, palette     (graphics agent)
platform/timer.*    base timer ISR body, song interpreter, delays/deadlines, sound API             (platform agent)
platform/input.*    input_poll_drive, getkey family, joystick emulation, name editor               (platform agent)
platform/res.*      DOS block allocator, archive cache, PES/raw loaders, res_find, rand8, CRT rand (platform agent)
game/game.h         prototypes of game_flow / scene_render / simulation functions                  (coordinator)
game/flow*.c        game_flow spec: main (game_main), screens, run_game, run_stage, scores         (flow agent)
game/scene*.c       scene_render spec: projection, road/object drawing, cockpit, crash, ending     (scene agent)
game/sim*.c         simulation spec: stage setup 0x4792, driving ISR 0x3B1F and everything it runs (sim agent)
```

Only `host.c` and `main.c` include SDL. Game and platform code talks to the host through `host.h`.

## Build variants (`build.h`)

TDCGA.EXE (the CGA build, also run as `tdcga herc` for Hercules) is the same C game code linked with a
different graphics layer. The port builds it from the same sources:

* `TD_CGA=1` loads TDCGA.EXE (DGROUP 0x1A8A, screen segment 0xB800) and compiles `platform/gfx_cga.c`
  instead of `platform/gfx.c`. `TD_HERC=1` additionally selects the Hercules path of TDCGA's `main`
  and the Hercules picture.
* `symbols.h` has a TDCGA block. `tools/xmap.py` aligns the two executables and translates
  `port/symbols.csv` into `port/cga/symbols_cga.csv`, then `tools/gen_symbols.py` regenerates the header.
  Symbols that TDCGA lacks are left undefined there, so using one in a CGA build fails to compile.
* In shared code, a value that differs is written `EGA_CGA(ega, cga)`, e.g. a raw DS offset or a colour.
  A statement that differs goes in `#if TD_CGA`, with a `/* TDCGA 0xADDR */` comment.
  `port/cga/game_diffs.txt` and `port/cga/call_swaps.txt` (generated) list where the game code differs.
* Specs: `port/cga/graphics.md` (graphics layer) and `port/cga/game_code.md` (game-code differences).

## Memory model (`mem.h`)

* The original's data stays **in `mem[]` at its original address**. Every global, table, string, sprite
  handle table, the road stream, the font and the car `.BIN` buffer (DS:268F) is read and written there:
  `DSW(DS_road_pos)`, `DSB(DS_run_state)`, `DSS(DS_car_x)`, `far_rd(DGROUP, DS_spr_dash)`.
  Names come from `symbols.h`. For an offset without a name, write the raw offset with a comment:
  `DSW(0x1873) /* heading_acc */`. Never shadow game state in C variables that outlive a function.
* Code-segment variables used by the hand-written asm (`CS:5A64` current target, `CS:3B18`, …) are
  `CSW(0x5A64)` etc.
* Far pointers are `FarPtr {off, seg}`, passed by value. Sprites, archives, song streams and target
  descriptors are all `FarPtr`s into `mem[]`. Segment `0xA000` plane segments mean the EGA screen
  (graphics module only).
* Strings given to text routines may be DGROUP strings: `(const char *)mp(DGROUP, 0x0B16)`.
* Fixed-width arithmetic: use `u8/s8/u16/s16/u32/s32` exactly as the original register widths; cast
  at every step where the original truncates. Signed right shift = `(s16)x >> n` (arithmetic on our
  compilers). Emulate carries/borrows explicitly where the asm uses `adc/sbb/rcl/rcr`.
* **Division** goes through `div32_16 / idiv32_16 / div16_8 / idiv16_8` (they return 0xFFFF like the
  game's INT 0 hook). Use them for every DIV/IDIV the original performs, not just the risky ones.
* Keep the original's buffer overruns and aliasing when a spec marks them as faithful quirks (e.g.
  the traffic list copy bug at 0x45D1 writes across DS:0945–0A14). Never write outside `mem[]`.

## Timing (`host.h`, `timer.h`)

* The host calls `timer_host_tick()` (installed by `timer_init`) at exactly 100.0404 Hz from
  `host_pump()`. It runs the driving ISR if one is installed (`timer_set_driving_isr`), otherwise
  `timer_isr()`. The driving ISR (`sim_timer_isr`) calls `timer_isr()` first, like the original far call.
* **Every busy-wait loop in the original must call `host_pump()` once per iteration** (delays,
  deadlines, key polls, "wait for fire"). Platform wait helpers already do; game code that loops on its
  own must too.
* `run_stage`'s per-frame loop calls `host_pump()` once at the top of each iteration, before
  `snapshot_sim_state()`. Ticks (and therefore the simulation) only advance inside `host_pump()`, which
  matches the original's asynchronous ISR closely enough and keeps the snapshot consistent.
* Screen output is composed from the EGA planes by `gfx_compose()` and presented by the host when
  something changed. Effects the original ran unpaced (dissolves in `stage_results`) call
  `host_present_now()` after each step.

## Porting a function

1. One C function per original function, named as in `symbols.h` (`FN_<name>`), with a leading comment
   `/* 0xADDR name — spec section */`. Asm routines with register arguments become C functions with
   explicit parameters/returns named after the registers they model (`s16 proj_x_main(s16 bx_x, s16 dx_z)`).
2. Follow the spec pseudocode; confirm against the disassembly wherever the spec says `likely`/`guess`,
   or where signedness, carries or evaluation order matter.
3. Mark deliberate deviations with `/* PORT: ... */` (e.g. dropped copy protection, removed port I/O).
   Mark unresolved doubts with `/* TODO(verify): ... */` and list them in your report.
4. No `static` game state: state lives in `mem[]` (a `static` is fine for pure host-side caches).
5. Don't reformat or restructure files owned by another module. If you need a declaration that isn't
   in a shared header, add it to **your own** header, and list it in your report.

## Build and checks

From `tdport/` with MinGW on PATH (`export PATH="/c/msys64/mingw64/bin:$PATH"`):

```
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/tdport.exe --game-dir ../Game --check
```

While other modules are unfinished the link may fail with undefined references to their functions;
that is expected. Your own files must **compile without warnings**. Check a single file with
`gcc -std=c11 -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing -fsyntax-only -Isrc -I/c/msys64/mingw64/include src/<file>.c`.
You may write throwaway verification scripts in your scratch area (e.g. compare a blit against
`../tools/tdres.py` output). Do not create HTML pages. Do not launch the game window.
