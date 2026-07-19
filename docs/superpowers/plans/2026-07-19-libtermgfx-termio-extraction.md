# libtermgfx terminal-I/O extraction — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Move syncscumm's `door/sst_io.c` terminal-I/O orchestration into a
shared libtermgfx module `termgfx_termio` (a canonical renderer/input/socket
engine all graphical doors can converge on), and refactor syncscumm onto it with
**byte-identical** behavior.

**Architecture:** `termgfx_termio.{c,h}` is a single-instance-per-process C
module in `src/doors/termgfx/`, driven by a door's engine backend: the door
hands it native-resolution RGB frames (`present`), ticks it, and pulls decoded
input events. It drives the already-shared libtermgfx primitives (`caps`,
`geometry`, `sixel`, `jxl`, `pace`, `mouse`, `keymode`, `door32`) — those are NOT
touched. syncscumm keeps its door-specific glue (`.ini` reads, the
`SST_KEY_*`→`Common::KEYCODE` map, the `SYNCSCUMM_DUMP` path, audio via the
shared `termgfx_stream_*`).

**Tech Stack:** C (module) + C++ (syncscumm backend), the existing termgfx CMake
+ syncscumm `build.sh`/`module.mk`, ScummVM's make. GCC/Clang on *nix, MSVC on
Windows (the create_project build).

## Global Constraints

- **Byte-identical wire output.** This is a pure move; syncscumm's outbound
  bytes on a deterministic headless run must be identical before and after
  (verified by diff — see Task 2 and Task 6).
- **No behavior change to syncscumm** (it is live on the BBS): no new features,
  no re-tuning, no re-ordering of I/O.
- **Single-instance-per-process structure is preserved.** No opaque-context /
  multi-instance redesign — the module keeps its file-static state. That is what
  makes this a move.
- **Door-specific glue stays in the door.** Only game/engine-agnostic
  orchestration moves.
- **Portable C/C++** — must compile under GCC, Clang, and MSVC. No new
  reserved-namespace identifiers; conforming header guards.
- **`door/*.c`/`.cpp` and `test/*.sh` are HAND-FORMATTED** — do NOT run
  uncrustify; match the existing file style, write tabs.
- **Commit direct to master**, never push without explicit OK, never `--amend`
  in the shared tree.

## File Structure

- **Create:**
  - `src/doors/termgfx/termgfx_termio.h` — the door-facing API + the moved
    `termgfx_key_*` enum and `termgfx_input_event_t` type.
  - `src/doors/termgfx/termgfx_termio.c` — the moved `sst_io.c` body, renamed.
- **Delete:**
  - `src/doors/syncscumm/door/sst_io.c`, `src/doors/syncscumm/door/sst_io.h`.
- **Modify:**
  - `src/doors/syncscumm/door/syncscumm.cpp` — include + call the new API.
  - `src/doors/syncscumm/build.sh`, `src/doors/syncscumm/door/module.mk` —
    link `termgfx_termio` from libtermgfx instead of compiling local `sst_io.o`.
  - `src/doors/termgfx/CMakeLists.txt` (or the termgfx build list) — add
    `termgfx_termio.c` to the shared library.
  - The `sst_io` unit tests (`test/test_sst_io_*.c`, `test/unit_sst_io.sh`) —
    follow the module: renamed to `termgfx_termio`, symbol references updated,
    **assertions unchanged**.

Note: the `test/boot_*.sh` acceptance scripts drive the syncscumm *binary* and
reference no `sst_io` symbols — they are the behavior gate and are **not**
edited.

---

### Task 1: Design and write the shared API header

**Files:**
- Create: `src/doors/termgfx/termgfx_termio.h`
- Read (survey): `src/doors/syncscumm/door/sst_io.h`, and for the superset,
  `src/doors/{syncretro/syncretro_io.c,syncduke/syncduke_io.c,syncconquer/door_io.c}`
  and `src/doors/syncdoom/{i_video.c,r_draw.c}`.

**Interfaces:**
- Produces: the `termgfx_termio_*` function prototypes, the `termgfx_key_*`
  enum, and `termgfx_input_event_t`, consumed by Tasks 3–5.

- [ ] **Step 1: Survey the other doors' `*_io.c`** for operations syncscumm's
  API lacks (variable frame geometry, palette handling, edge-scroll mouse
  zones). Record any that must be in the shared API vs left a door-side hook.
  Output: a short note appended to the plan file under this task.

- [ ] **Step 2: Write `termgfx_termio.h`** — mirror `sst_io.h`'s current
  surface, renamed, plus anything Step 1 surfaced. Conforming guard
  (`TERMGFX_TERMIO_H_`). Prototypes:

```c
/* frame source */
void termgfx_termio_present(const uint8_t *rgb, int w, int h);
void termgfx_termio_tick(void);
/* lifecycle / transport */
int  termgfx_termio_init(int argc, char **argv);
void termgfx_termio_shutdown(void);
int  termgfx_termio_hung_up(void);
/* input */
int  termgfx_termio_next_event(termgfx_input_event_t *ev);
int  termgfx_termio_quit_requested(void);
int  termgfx_termio_menu_requested(void);
void termgfx_termio_set_menu_key(int letter);
/* capability query */
int  termgfx_termio_audio_available(void);
```
  (The exact `present()` pixel-format contract — packed RGB vs RGBA, stride — is
  pinned in Task 3 to match `sst_io.c`'s current internal surface; the header
  documents whatever that is. Do NOT invent a new format.)

- [ ] **Step 3: Compile-check the header** in isolation (`gcc -fsyntax-only -x c
  termgfx_termio.h` behind a tiny includer) to catch missing types.
  Expected: clean.

- [ ] **Step 4: Commit** `git add src/doors/termgfx/termgfx_termio.h && git commit`.

---

### Task 2: Baseline — capture the golden wire output and confirm green

**Files:**
- Read/run: `src/doors/syncscumm/test/*.sh`, the wirecap diagnostic path in
  `sst_io.c` (touch-file `<SBBSDATA>/syncscumm/wirecap` → `/tmp/syncscumm.<pid>.raw`).

**Interfaces:**
- Produces: `baseline.raw` (golden outbound bytes) + a recorded "all green" test
  run, consumed by Task 6's diff.

- [ ] **Step 1: Build syncscumm at HEAD** (`./build.sh`) and run the full boot
  suite. Run: `for t in test/boot_*.sh; do "$t"; done`
  Expected: every `BOOT-* OK`.

- [ ] **Step 2: Run the unit binaries.** Run: `test/unit_sst_io.sh` (and the
  other `test/test_sst_io_*` drivers). Expected: all pass.

- [ ] **Step 3: Capture the golden wire bytes.** Pick ONE deterministic headless
  scenario (a boot-test-style run of a fixed title through a fixed number of
  frames) with the wirecap touch-file armed, so it writes
  `/tmp/syncscumm.<pid>.raw`. Copy it to
  `docs/superpowers/plans/artifacts/termio-baseline.raw`.
  Expected: a non-empty capture; note its sha256.

- [ ] **Step 4: Commit** the baseline artifact + its sha256 (in the plan or a
  README next to it) so Task 6 can diff against it.

---

### Task 3: Move `sst_io.c` → `termgfx/termgfx_termio.c`, renamed

**Files:**
- Rename: `src/doors/syncscumm/door/sst_io.c` → `src/doors/termgfx/termgfx_termio.c`
- Delete: `src/doors/syncscumm/door/sst_io.h` (superseded by Task 1's header)
- Modify: `src/doors/termgfx/CMakeLists.txt` (add the new source to the lib)

**Interfaces:**
- Consumes: `termgfx_termio.h` (Task 1).
- Produces: `termgfx_termio.c` compiling as part of libtermgfx.

- [ ] **Step 1: `git mv`** the file to `src/doors/termgfx/termgfx_termio.c`
  (preserves history).

- [ ] **Step 2: Rename symbols by exact mapping.** Apply, in the moved file:
  `sst_io_` → `termgfx_termio_`, `SST_KEY_` → `TERMGFX_KEY_`,
  `sst_input_event_t` → `termgfx_input_event_t`, `SST_MOD_` → `TERMGFX_MOD_`,
  `SST_EV_` → `TERMGFX_EV_`, and `#include "sst_io.h"` → `#include
  "termgfx_termio.h"`. (These are the only public symbols; internal `static`
  `g_*`/`sst_*` helpers may keep their names — they are file-local.) Verify the
  rename touched nothing unintended: `grep -n 'sst_io_\|SST_KEY_' termgfx_termio.c`
  → empty.

- [ ] **Step 3: Pin the `present()` contract.** Read the current frame-input path
  (what `syncscumm.cpp` hands the present code) and make `termgfx_termio_present`
  take exactly that buffer format; document it in the header. No conversion added.

- [ ] **Step 4: Add `termgfx_termio.c` to the termgfx build** (CMakeLists) and
  compile the library alone. Run: `cmake --build <termgfx build> --target termgfx`
  Expected: compiles (it still references door-agnostic termgfx primitives only;
  any residual door-specific reference is a defect to move back to the door).

- [ ] **Step 5: Commit** the move + rename.

---

### Task 4: Wire syncscumm onto the shared module

**Files:**
- Modify: `src/doors/syncscumm/door/syncscumm.cpp`,
  `src/doors/syncscumm/build.sh`, `src/doors/syncscumm/door/module.mk`

**Interfaces:**
- Consumes: `termgfx_termio.h` + the shared lib (Tasks 1, 3).

- [ ] **Step 1: Repoint syncscumm.cpp.** Replace `#include "sst_io.h"` with
  `#include "../../termgfx/termgfx_termio.h"` and rename every `sst_io_*` call and
  `SST_KEY_*`/`SST_MOD_*`/`SST_EV_*` reference (in the `pollEvent` key switch,
  `resolveMenuKey`, the quit/menu checks) to the `termgfx_termio_*` /
  `TERMGFX_*` names. The `SST_KEY_*` → `Common::KEYCODE_*` mapping logic itself
  is unchanged — only the enum names on the left.

- [ ] **Step 2: Update the build to link the shared module, not compile local
  `sst_io.o`.** In `door/module.mk` drop `sst_io.o` from the object list; in
  `build.sh` ensure `termgfx_termio.c` is built into `libtermgfx.a` (Task 3) and
  linked. Remove the now-dead local reference.

- [ ] **Step 3: Build syncscumm.** Run: `./build.sh`
  Expected: links; produces `build/syncscumm`.

- [ ] **Step 4: Commit** the syncscumm wiring.

---

### Task 5: Migrate the `sst_io` unit tests with the module

**Files:**
- Rename/modify: `src/doors/syncscumm/test/test_sst_io_*.c`,
  `src/doors/syncscumm/test/unit_sst_io.sh` (and any `unit_sst_io*` driver).

**Interfaces:**
- Consumes: the shared module + header.

- [ ] **Step 1: Follow the code.** Rename the `sst_io` unit sources/drivers to
  `termgfx_termio` and update their `#include` + symbol references to the new
  names. **Assertions and test logic are unchanged** — they test the same
  behavior of the same (now moved) code.

- [ ] **Step 2: Run them.** Run: `test/unit_termgfx_termio.sh` (+ the renamed
  `test_termgfx_termio_*`). Expected: all pass, same as the Task 2 baseline.

- [ ] **Step 3: Commit** the test migration.

---

### Task 6: Verify behavior-identical

**Files:**
- Run: `src/doors/syncscumm/test/boot_*.sh`; diff against the Task 2 baseline.

- [ ] **Step 1: Boot suite unedited.** Run: `for t in test/boot_*.sh; do "$t"; done`
  Expected: every `BOOT-* OK`, with NO edits to the boot scripts.

- [ ] **Step 2: Wire-byte diff.** Re-run the exact Task 2 scenario with wirecap
  armed on the refactored binary; `cmp` its `/tmp/syncscumm.<pid>.raw` against
  `termio-baseline.raw`. Expected: **identical** (`cmp` silent, exit 0). Any diff
  is a regression — stop and fix before proceeding.

- [ ] **Step 3: Portability check.** Confirm the module + syncscumm build clean
  under the available compiler(s); note explicitly if only GCC was exercised so
  the MSVC create_project build is re-checked separately (per the portability
  constraint).

- [ ] **Step 4: Commit** any fixes; this task lands only when Steps 1–2 are green.

---

### Task 7: Live confirmation (sysop)

- [ ] **Step 1:** Deploy the refactored `syncscumm` binary and have the sysop
  play a title on the real BBS, confirming no change in render, input, audio,
  quit, and the Ctrl-G menu. This is the final gate before the syncrpg
  sub-project builds on the module.

---

## Self-Review

- **Spec coverage:** the spec's scope (move orchestration; keep primitives;
  door-glue stays; behavior-identical via boot tests + wire-diff + live) maps to
  Tasks 1/3/4 (move), 2/6 (wire-diff + tests), 5 (unit tests follow), 7 (live).
  The "design the API against all doors" activity is Task 1 Step 1.
- **Spec discrepancy fixed here:** the spec said unit tests "pass unedited" —
  clarified: the *boot* tests pass unedited (behavior gate); the *unit* tests
  move+rename WITH the code (same assertions). Flag to the sysop at review.
- **No placeholders:** each step names exact files, exact renames, exact
  commands, and expected results. The 3000-line body is moved mechanically
  (git mv + the explicit symbol mapping), not re-typed — the "code" for that
  step is the mapping + verification, which is complete and unambiguous.
- **Type consistency:** the `termgfx_termio_*` / `termgfx_key_*` /
  `termgfx_input_event_t` names are used identically in Tasks 1, 3, 4, 5.

## Execution Handoff

Two options:
1. **Subagent-Driven (recommended)** — a fresh subagent per task, review between
   tasks. Good for a delicate refactor: each task's wire-diff/test gate is a
   clean checkpoint.
2. **Inline Execution** — execute in this session with checkpoints.

Which approach?
