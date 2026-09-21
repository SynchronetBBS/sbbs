# SyncDrive v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A playable Test Drive (1987) Synchronet door, `syncdrive`, built on
the vendored faithful C port (kylofon/test-drive-sdl3) and termgfx's shared
session engine (`termgfx_termio`).

**Architecture:** The vendored engine talks to its platform only through
`tdport/src/host.h`. We replace upstream's SDL `host.c` with `door/host_term.c`
on `termgfx_termio` (session, sixel present, input events, audio stream).
Pure helpers (frame palette mapping, key mapping, key scripting, speaker
synthesis, alias sanitizing, score lock) are separate files with standalone
unit tests. One engine patch (`flow_scores.c`) locks the BBS alias into the
high-score entry and merges the table under a cross-process lock.

**Tech Stack:** C11, CMake >= 3.13, termgfx (`../termgfx`), xpdev
(`../../xpdev`), Synchronet JS (SpiderMonkey 1.8.5) for `getdata.js` and
`deploy.js`.

**Spec:** `src/doors/syncdrive/DESIGN.md`

## Global Constraints

- Binary `syncdrive`; package dir `xtrn/syncdrive/`; door name passed to
  termio: `termgfx_termio_set_app_name("syncdrive")`.
- Engine: upstream `https://github.com/kylofon/test-drive-sdl3` at commit
  `a4d23077770fcc2cae81680fe4e3fd73163c0915` (2026-09-18), MIT. Vendored
  frozen; the only edit is the `flow_scores.c` patch in Task 6.
- No game data in the repo. Dev copy: `https://archive.org/download/TestDrive_1987/Test%20Drive.zip`;
  `TDEGA.EXE` md5 `cdf8d1a767d559db559a0b829e1bb3b0`.
- termgfx is built **without** `WITH_JXL`; no libjxl dependency.
- Keyboard only: `termgfx_termio_set_mouse(0)` before `termgfx_termio_init()`.
- No termgfx source changes in v1 (DESIGN.md sec. 14 is deferred).
- Keys: F2 = game sound toggle, Ctrl-F = `termgfx_termio_fit_cycle()`,
  Ctrl-J dropped, Ctrl-P passes through. termio owns Ctrl-S and Ctrl-Q/Ctrl-C.
- High-score alias: locked, 15-character field, characters outside
  0x20..0x7A become `?`, padded with spaces.
- `SCORES` lives in `--data-dir=` (default: the game dir); lock file
  `<data-dir>/SCORES.lck`.
- House style for OUR C files: `uncrustify -c src/uncrustify.cfg --replace
  --no-backup <files>` as each task's last code step. Never run it on
  `tdport/`.
- Identifiers: no two-letter prefixes; our stems are `frame_`, `keymap_`,
  `keyscript_`, `speaker_`, `alias_`, `scores_lock_`, `syncdrive_`, and the
  `host_*` names `host.h` dictates. Header guards end in `_H_` (no leading
  underscore).
- `strcmp(a, b) == 0`, never `!strcmp`. No em/en dashes anywhere. US spelling.
- JS: SpiderMonkey 1.8.5 (`var`, no arrows/let/const/template literals).
- Linux build only in this plan; the Windows/MSVC build is a follow-up.
- Git: commit on `master` in `~/sbbs`, only at the milestones marked below,
  only after the change was exercised. The index is shared: run
  `git diff --cached --stat` first and commit only our paths. Messages wrap at
  78 columns (check with `awk 'length > 78' msg`), end with
  `Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>`,
  and are committed with `git commit -F`. Never push.

---

## File Structure

```
src/doors/syncdrive/
  DESIGN.md                      (exists)
  plans/2026-09-18-syncdrive-v1.md (this file)
  .gitignore                     build tree, dev game data
  CLAUDE.md                      ours-vs-vendored rule, build/test commands
  PROVENANCE.md                  upstream ref, license, omissions, the patch
  README.md                      sysop-facing door documentation
  CMakeLists.txt                 syncdrive exe + tests subdir
  build.sh                       configure + build (Release/Debug/clean)
  deploy.js                      door_deploy() wrapper
  compat/SDL3/SDL.h              libc shim for tdport/src/mem.c
  tdport/LICENSE  tdport/PORTING.md  tdport/FORMATS.md
  tdport/src/...                 upstream tdport/src minus host.c, main.c
  door/
    frame.c/.h                   XRGB -> fixed 16-color indexed frame
    keymap.c/.h                  termgfx key events -> BIOS key words / XT
    keyscript.c/.h               SYNCDRIVE_KEYS headless key script parser
    speaker.c/.h                 PIT divisor + gate -> stereo PCM
    alias.c/.h                   BBS alias -> 15-char game name field
    scores_lock.c/.h             <data-dir>/SCORES.lck cross-process lock
    host_term_ext.h              door-only host calls used by the patch
    host_term.c                  tdport host.h on termgfx_termio
    syncdrive.c                  main()
  tests/
    CMakeLists.txt
    test_frame.c  test_keymap.c  test_keyscript.c  test_speaker.c
    test_alias.c  test_scores_lock.c
xtrn/syncdrive/
  install-xtrn.ini  getdata.js  README.md  syncdrive.example.ini
src/doors/build.sh               add the syncdrive row
```

---

### Task 1: Scaffold + frame conversion

**Files:**
- Create: `src/doors/syncdrive/.gitignore`, `CMakeLists.txt`, `build.sh`,
  `tests/CMakeLists.txt`, `door/frame.h`, `door/frame.c`, `tests/test_frame.c`

**Interfaces:**
- Produces:
  - `#define FRAME_W 320`, `#define FRAME_H 200`, `#define FRAME_COLORS 16`
  - `uint32_t frame_color(int i);` XRGB (0x00RRGGBB) of palette entry i
  - `void frame_palette(uint8_t pal768[768]);` entries 0..15 filled, rest 0
  - `int frame_index(uint32_t xrgb);` exact match, else nearest entry
  - `void frame_convert(const uint32_t *xrgb, uint8_t *idx, size_t n);`
  - Top-level CMake that the later tasks extend; `tests/` target list.

The engine's `gfx_compose()` maps each EGA palette register through a
200-line monitor decode (`ega_rgb()` in `tdport/src/platform/gfx.c`: bit0 B,
bit1 G, bit2 R, bit4 intensity, R+G without intensity = brown). That decode can
only produce 16 distinct colors, so the door uses one FIXED 16-entry palette:
entry i is the decode of register value `(i & 7) | ((i & 8) << 1)`. Because it
never changes, SyncTERM's persistent color registers are loaded once.

- [ ] **Step 1: Create the scaffold files**

`src/doors/syncdrive/.gitignore`:
```
/build/
/game/
```

`src/doors/syncdrive/CMakeLists.txt` (tests only for now; Task 5 adds the
executable above the `enable_testing()` block):
```cmake
cmake_minimum_required(VERSION 3.13)
project(syncdrive C CXX)   # CXX: termgfx links C++ libADLMIDI

# syncdrive -- Test Drive (1987) for Synchronet / SyncTERM, via the vendored
# faithful C port in tdport/ (kylofon/test-drive-sdl3) and termgfx's shared
# session engine. See DESIGN.md.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	enable_testing()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests tests)
endif()
```

`src/doors/syncdrive/tests/CMakeLists.txt`:
```cmake
# Standalone unit tests for syncdrive's pure door modules: no engine, no
# termgfx library link (it drags in C++/libADLMIDI). -UNDEBUG because these
# tests ARE their assert()s.

function(syncdrive_test name)
	add_executable(${name} ${ARGN})
	target_include_directories(${name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/../door
		${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx)
	set_target_properties(${name} PROPERTIES C_STANDARD 11)
	target_compile_options(${name} PRIVATE -UNDEBUG -Wall -Wextra)
	add_test(NAME ${name} COMMAND ${name})
endfunction()

syncdrive_test(test_frame
	${CMAKE_CURRENT_SOURCE_DIR}/../door/frame.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_frame.c)
```

`src/doors/syncdrive/build.sh` (mode 0755):
```sh
#!/bin/sh
# ===========================================================================
# build.sh - Configure and build syncdrive (Linux/Unix).
#
#   Usage:  ./build.sh             (release build)
#           ./build.sh debug       (Debug build)
#           ./build.sh clean       (delete the build tree, then exit)
#           ./build.sh clean all   (delete the build tree, then build)
#
# Builds out-of-source in ./build/ and runs the unit tests. Building does NOT
# touch any live install -- run `jsexec deploy.js` for that.
# ===========================================================================
set -e

SRCDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILDDIR="$SRCDIR/build"
CONFIG=Release
DOCLEAN=
DOALL=

for arg in "$@"; do
	case "$arg" in
	clean)             DOCLEAN=1 ;;
	all)               DOALL=1 ;;
	debug | Debug)     CONFIG=Debug ;;
	release | Release) CONFIG=Release ;;
	*) echo "build.sh: ignoring unknown argument '$arg'" >&2 ;;
	esac
done

if [ -n "$DOCLEAN" ]; then
	rm -rf "$BUILDDIR"
	[ -z "$DOALL" ] && exit 0
fi

cmake -S "$SRCDIR" -B "$BUILDDIR" -DCMAKE_BUILD_TYPE="$CONFIG"
cmake --build "$BUILDDIR" -j"$(nproc 2>/dev/null || echo 4)"
(cd "$BUILDDIR" && ctest --output-on-failure)

if [ -x "$BUILDDIR/syncdrive" ]; then
	echo "[build] Built: $BUILDDIR/syncdrive"
	echo "[build] Run 'jsexec deploy.js' to install it into the door's xtrn dir."
fi
```

- [ ] **Step 2: Write the failing test**

`src/doors/syncdrive/tests/test_frame.c`:
```c
/* Unit tests for frame.c: the fixed 16-color EGA 200-line palette and the
 * XRGB -> index conversion. */
#include <assert.h>
#include <string.h>
#include "frame.h"

int main(void)
{
	uint8_t  pal[768];
	uint32_t px[5];
	uint8_t  idx[5];
	int      i, j;

	/* Every entry maps back to itself, and all 16 are distinct. */
	for (i = 0; i < FRAME_COLORS; i++) {
		assert(frame_index(frame_color(i)) == i);
		for (j = 0; j < i; j++)
			assert(frame_color(i) != frame_color(j));
	}

	/* Spot values of the 200-line decode. */
	assert(frame_color(0) == 0x000000);
	assert(frame_color(1) == 0x0000AA);    /* blue */
	assert(frame_color(6) == 0xAA5500);    /* brown, not dark yellow */
	assert(frame_color(7) == 0xAAAAAA);
	assert(frame_color(8) == 0x555555);
	assert(frame_color(14) == 0xFFFF55);   /* yellow */
	assert(frame_color(15) == 0xFFFFFF);

	/* The pad byte of XRGB is ignored; an off-palette color goes to the
	 * nearest entry. */
	assert(frame_index(0xFF0000AA) == 1);
	assert(frame_index(0x00AB5601) == 6);

	/* Palette triples, unused entries zero. */
	memset(pal, 0x7E, sizeof pal);
	frame_palette(pal);
	assert(pal[6 * 3 + 0] == 0xAA && pal[6 * 3 + 1] == 0x55 && pal[6 * 3 + 2] == 0x00);
	assert(pal[15 * 3 + 0] == 0xFF && pal[15 * 3 + 1] == 0xFF && pal[15 * 3 + 2] == 0xFF);
	for (i = FRAME_COLORS * 3; i < 768; i++)
		assert(pal[i] == 0);

	/* Bulk conversion. */
	px[0] = frame_color(3);
	px[1] = frame_color(3);
	px[2] = frame_color(12);
	px[3] = 0x000000;
	px[4] = frame_color(15);
	frame_convert(px, idx, 5);
	assert(idx[0] == 3 && idx[1] == 3 && idx[2] == 12 && idx[3] == 0 && idx[4] == 15);
	return 0;
}
```

- [ ] **Step 3: Run it to verify it fails**

Run: `cd ~/sbbs/src/doors/syncdrive && ./build.sh`
Expected: FAIL at configure/build: `door/frame.c` / `frame.h` not found.

- [ ] **Step 4: Implement frame.h / frame.c**

`src/doors/syncdrive/door/frame.h`:
```c
/* frame.h -- the engine's composed XRGB frame -> termgfx indexed frame.
 *
 * The EGA picture only ever holds the 16 colors a 200-line monitor can show,
 * so the palette is fixed (see frame.c). */
#ifndef SYNCDRIVE_FRAME_H_
#define SYNCDRIVE_FRAME_H_

#include <stddef.h>
#include <stdint.h>

#define FRAME_W      320
#define FRAME_H      200
#define FRAME_COLORS 16

uint32_t frame_color(int i);
void     frame_palette(uint8_t pal768[768]);
int      frame_index(uint32_t xrgb);
void     frame_convert(const uint32_t *xrgb, uint8_t *idx, size_t n);

#endif
```

`src/doors/syncdrive/door/frame.c`:
```c
/* frame.c -- fixed 16-color palette and XRGB -> index conversion. */
#include <string.h>
#include "frame.h"

static uint32_t colors[FRAME_COLORS];
static int      colors_ready;

/* Same decode as tdport/src/platform/gfx.c ega_rgb(): a 200-line EGA monitor
 * reads bit0 B, bit1 G, bit2 R, bit4 intensity, and shows R+G without
 * intensity as brown. */
static uint32_t ega200_rgb(unsigned v)
{
	uint32_t r = (v & 4) ? 0xAA : 0;
	uint32_t g = (v & 2) ? 0xAA : 0;
	uint32_t b = (v & 1) ? 0xAA : 0;

	if ((v & 0x17) == 0x06)
		g = 0x55;
	if (v & 0x10) {
		r += 0x55;
		g += 0x55;
		b += 0x55;
	}
	return r << 16 | g << 8 | b;
}

static void colors_init(void)
{
	int i;

	if (colors_ready)
		return;
	for (i = 0; i < FRAME_COLORS; i++)
		colors[i] = ega200_rgb((unsigned)((i & 7) | ((i & 8) << 1)));
	colors_ready = 1;
}

uint32_t frame_color(int i)
{
	colors_init();
	return colors[i & (FRAME_COLORS - 1)];
}

void frame_palette(uint8_t pal768[768])
{
	int i;

	colors_init();
	memset(pal768, 0, 768);
	for (i = 0; i < FRAME_COLORS; i++) {
		pal768[i * 3 + 0] = (uint8_t)(colors[i] >> 16);
		pal768[i * 3 + 1] = (uint8_t)(colors[i] >> 8);
		pal768[i * 3 + 2] = (uint8_t)colors[i];
	}
}

int frame_index(uint32_t xrgb)
{
	int  i, best = 0;
	long best_d = -1;

	colors_init();
	xrgb &= 0xFFFFFF;
	for (i = 0; i < FRAME_COLORS; i++) {
		if (colors[i] == xrgb)
			return i;
	}
	for (i = 0; i < FRAME_COLORS; i++) {
		long dr = (long)((xrgb >> 16) & 0xFF) - (long)((colors[i] >> 16) & 0xFF);
		long dg = (long)((xrgb >> 8) & 0xFF) - (long)((colors[i] >> 8) & 0xFF);
		long db = (long)(xrgb & 0xFF) - (long)(colors[i] & 0xFF);
		long d  = dr * dr + dg * dg + db * db;

		if (best_d < 0 || d < best_d) {
			best_d = d;
			best   = i;
		}
	}
	return best;
}

void frame_convert(const uint32_t *xrgb, uint8_t *idx, size_t n)
{
	uint32_t last_c = 0;
	uint8_t  last_i = 0;
	size_t   k;

	last_i = (uint8_t)frame_index(0);
	for (k = 0; k < n; k++) {
		uint32_t c = xrgb[k] & 0xFFFFFF;

		if (c != last_c) {
			last_c = c;
			last_i = (uint8_t)frame_index(c);
		}
		idx[k] = last_i;
	}
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `cd ~/sbbs/src/doors/syncdrive && ./build.sh`
Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 6: Style**

Run: `cd ~/sbbs/src/doors/syncdrive && uncrustify -c ../../uncrustify.cfg --replace --no-backup door/frame.c door/frame.h tests/test_frame.c && ./build.sh`
Expected: tests still pass.

(No commit: first commit is at the end of Task 5.)

---

### Task 2: Key mapping + key script

**Files:**
- Create: `door/keymap.h`, `door/keymap.c`, `door/keyscript.h`,
  `door/keyscript.c`, `tests/test_keymap.c`, `tests/test_keyscript.c`
- Modify: `tests/CMakeLists.txt` (two targets)

**Interfaces:**
- Consumes: `termgfx_input_event_t`, `TERMGFX_EV_*`, `TERMGFX_KEY_*`,
  `TERMGFX_MOD_*` from `termgfx_termio.h` (header only).
- Produces:
  ```c
  enum { KEYMAP_ACT_NONE, KEYMAP_ACT_SOUND_TOGGLE, KEYMAP_ACT_FIT_CYCLE };
  typedef struct {
  	uint16_t bios;    /* INT 16h key word to queue; 0 = none */
  	uint8_t  xt;      /* XT scan code for the held-key table; 0 = none */
  	int      action;  /* KEYMAP_ACT_* door action */
  	int      down;    /* 1 KEY_DOWN, 0 KEY_UP */
  } keymap_result_t;
  keymap_result_t keymap_translate(const termgfx_input_event_t *ev);
  uint16_t keymap_sound_toggle_key(int sound_on);  /* Ctrl-Q or Ctrl-S word */

  enum { KEYSCRIPT_WAIT, KEYSCRIPT_KEY, KEYSCRIPT_QUIT };
  typedef struct { int kind; int ms; termgfx_input_event_t ev; } keyscript_step_t;
  int keyscript_parse(const char *s, keyscript_step_t *out, int max);
  ```

Key words are what INT 16h AH=00h returns: `(XT scan << 8) | ASCII`
(upstream `host.c` `bios_key()`). termio delivers Ctrl+letter as keycode = the
lowercase letter with `TERMGFX_MOD_CTRL` and ascii 0; a legacy terminal sends
only KEY_DOWN, never KEY_UP.

- [ ] **Step 1: Write the failing tests**

`src/doors/syncdrive/tests/test_keymap.c`:
```c
/* Unit tests for keymap.c: termgfx key events -> BIOS key words. */
#include <assert.h>
#include <string.h>
#include "keymap.h"

static keymap_result_t key(int type, int keycode, int ascii, int mods)
{
	termgfx_input_event_t ev;

	memset(&ev, 0, sizeof ev);
	ev.type    = type;
	ev.keycode = keycode;
	ev.ascii   = ascii;
	ev.mods    = mods;
	return keymap_translate(&ev);
}

#define DOWN(k, a, m) key(TERMGFX_EV_KEY_DOWN, (k), (a), (m))
#define UP(k, a, m)   key(TERMGFX_EV_KEY_UP, (k), (a), (m))

int main(void)
{
	keymap_result_t r;

	/* Cursor block: key word and held-key scan code. */
	r = DOWN(TERMGFX_KEY_UP, 0, 0);
	assert(r.bios == 0x4800 && r.xt == 0x48 && r.down == 1);
	r = DOWN(TERMGFX_KEY_LEFT, 0, 0);
	assert(r.bios == 0x4B00 && r.xt == 0x4B);
	r = DOWN(TERMGFX_KEY_RIGHT, 0, 0);
	assert(r.bios == 0x4D00 && r.xt == 0x4D);
	r = DOWN(TERMGFX_KEY_DOWN, 0, 0);
	assert(r.bios == 0x5000 && r.xt == 0x50);
	r = DOWN(TERMGFX_KEY_HOME, 0, 0);
	assert(r.bios == 0x4700 && r.xt == 0x47);
	r = DOWN(TERMGFX_KEY_PAGEUP, 0, 0);
	assert(r.bios == 0x4900 && r.xt == 0x49);
	r = DOWN(TERMGFX_KEY_END, 0, 0);
	assert(r.bios == 0x4F00 && r.xt == 0x4F);
	r = DOWN(TERMGFX_KEY_PAGEDOWN, 0, 0);
	assert(r.bios == 0x5100 && r.xt == 0x51);
	r = DOWN(TERMGFX_KEY_KP5, 0, 0);
	assert(r.bios == 0x4C00 && r.xt == 0);

	/* A release carries only the scan code, for the held-key table. */
	r = UP(TERMGFX_KEY_UP, 0, 0);
	assert(r.bios == 0 && r.xt == 0x48 && r.down == 0 && r.action == KEYMAP_ACT_NONE);

	/* A and Z shift gears; both cases, held scan codes. */
	r = DOWN('a', 'a', 0);
	assert(r.bios == 0x1E61 && r.xt == 0x1E);
	r = DOWN('A', 'A', TERMGFX_MOD_SHIFT);
	assert(r.bios == 0x1E41 && r.xt == 0x1E);
	r = DOWN('z', 'z', 0);
	assert(r.bios == 0x2C7A && r.xt == 0x2C);
	r = DOWN('q', 'q', 0);
	assert(r.bios == 0x1071 && r.xt == 0);

	/* Menu and text-entry keys. */
	assert(DOWN(TERMGFX_KEY_ENTER, 0, 0).bios == 0x1C0D);
	assert(DOWN(TERMGFX_KEY_ESCAPE, 0, 0).bios == 0x011B);
	assert(DOWN(TERMGFX_KEY_BACKSPACE, 0, 0).bios == 0x0E08);
	assert(DOWN(TERMGFX_KEY_TAB, 0, 0).bios == 0x0F09);
	assert(DOWN(TERMGFX_KEY_INSERT, 0, 0).bios == 0x5200);
	assert(DOWN(TERMGFX_KEY_DELETE, 0, 0).bios == 0x5300);
	assert(DOWN(' ', ' ', 0).bios == 0x3920);
	assert(DOWN('1', '1', 0).bios == 0x0231);
	assert(DOWN('0', '0', 0).bios == 0x0B30);
	assert(DOWN('!', '!', TERMGFX_MOD_SHIFT).bios == 0x0221);
	assert(DOWN('.', '.', 0).bios == 0x342E);
	assert(DOWN('?', '?', TERMGFX_MOD_SHIFT).bios == 0x353F);
	assert(DOWN(TERMGFX_KEY_F1, 0, 0).bios == 0x3B00);
	assert(DOWN(TERMGFX_KEY_F9, 0, 0).bios == 0x4300);

	/* Ctrl letters: the game's Ctrl-P pause and Ctrl-K pass through. */
	assert(DOWN('p', 0, TERMGFX_MOD_CTRL).bios == 0x1910);
	assert(DOWN('k', 0, TERMGFX_MOD_CTRL).bios == 0x250B);

	/* Door keys: F2 sound toggle, Ctrl-F fit toggle; neither reaches the game. */
	r = DOWN(TERMGFX_KEY_F2, 0, 0);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_SOUND_TOGGLE);
	r = DOWN('f', 0, TERMGFX_MOD_CTRL);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_FIT_CYCLE);
	r = UP(TERMGFX_KEY_F2, 0, 0);
	assert(r.action == KEYMAP_ACT_NONE);

	/* Ctrl-J (joystick mode) is dropped. */
	r = DOWN('j', 0, TERMGFX_MOD_CTRL);
	assert(r.bios == 0 && r.action == KEYMAP_ACT_NONE);

	/* Mouse events are not keys. */
	r = key(TERMGFX_EV_MOUSE_DOWN, 0, 0, 0);
	assert(r.bios == 0 && r.xt == 0 && r.action == KEYMAP_ACT_NONE);

	/* The sound toggle sends the game's own Ctrl-Q (off) or Ctrl-S (on). */
	assert(keymap_sound_toggle_key(1) == 0x1011);
	assert(keymap_sound_toggle_key(0) == 0x1F13);
	return 0;
}
```

`src/doors/syncdrive/tests/test_keyscript.c`:
```c
/* Unit tests for keyscript.c: the SYNCDRIVE_KEYS headless key script. */
#include <assert.h>
#include "keyscript.h"

int main(void)
{
	keyscript_step_t s[16];
	int              n;

	n = keyscript_parse("wait:1500,enter,right,a,ctrl-p,f2,space,quit", s, 16);
	assert(n == 8);
	assert(s[0].kind == KEYSCRIPT_WAIT && s[0].ms == 1500);
	assert(s[1].kind == KEYSCRIPT_KEY && s[1].ev.keycode == TERMGFX_KEY_ENTER);
	assert(s[1].ev.type == TERMGFX_EV_KEY_DOWN);
	assert(s[2].kind == KEYSCRIPT_KEY && s[2].ev.keycode == TERMGFX_KEY_RIGHT);
	assert(s[3].ev.keycode == 'a' && s[3].ev.ascii == 'a');
	assert(s[4].ev.keycode == 'p' && s[4].ev.mods == TERMGFX_MOD_CTRL && s[4].ev.ascii == 0);
	assert(s[5].ev.keycode == TERMGFX_KEY_F2);
	assert(s[6].ev.keycode == ' ' && s[6].ev.ascii == ' ');
	assert(s[7].kind == KEYSCRIPT_QUIT);

	assert(keyscript_parse("", s, 16) == 0);
	assert(keyscript_parse("esc,up,down,left,home,end,pgup,pgdn", s, 16) == 8);
	assert(s[0].ev.keycode == TERMGFX_KEY_ESCAPE);
	assert(s[7].ev.keycode == TERMGFX_KEY_PAGEDOWN);

	/* Errors: unknown token, bad wait, overflow. */
	assert(keyscript_parse("bogus", s, 16) == -1);
	assert(keyscript_parse("wait:x", s, 16) == -1);
	assert(keyscript_parse("a,b,c", s, 2) == -1);
	return 0;
}
```

Append to `tests/CMakeLists.txt`:
```cmake
syncdrive_test(test_keymap
	${CMAKE_CURRENT_SOURCE_DIR}/../door/keymap.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_keymap.c)

syncdrive_test(test_keyscript
	${CMAKE_CURRENT_SOURCE_DIR}/../door/keyscript.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_keyscript.c)
```

- [ ] **Step 2: Run to verify failure**

Run: `cd ~/sbbs/src/doors/syncdrive && ./build.sh`
Expected: FAIL: `keymap.h` / `keyscript.h` not found.

- [ ] **Step 3: Implement keymap**

`src/doors/syncdrive/door/keymap.h`:
```c
/* keymap.h -- termgfx key events -> the engine's BIOS keyboard (INT 16h key
 * words) and held-key table (XT scan codes), plus the door's own keys. */
#ifndef SYNCDRIVE_KEYMAP_H_
#define SYNCDRIVE_KEYMAP_H_

#include <stdint.h>
#include "termgfx_termio.h"

enum { KEYMAP_ACT_NONE, KEYMAP_ACT_SOUND_TOGGLE, KEYMAP_ACT_FIT_CYCLE };

typedef struct {
	uint16_t bios;
	uint8_t  xt;
	int      action;
	int      down;
} keymap_result_t;

keymap_result_t keymap_translate(const termgfx_input_event_t *ev);
uint16_t        keymap_sound_toggle_key(int sound_on);

#endif
```

`src/doors/syncdrive/door/keymap.c`:
```c
/* keymap.c -- see keymap.h. Key words follow upstream host.c bios_key(). */
#include <string.h>
#include "keymap.h"

static const uint8_t letter_scan[26] = {
	0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
	0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C
};

/* Printable non-letters, both shift states, by the character received. */
static const struct {
	char    ch;
	uint8_t scan;
} char_scan[] = {
	{ '1', 0x02 }, { '!', 0x02 }, { '2', 0x03 }, { '@', 0x03 }, { '3', 0x04 }, { '#', 0x04 },
	{ '4', 0x05 }, { '$', 0x05 }, { '5', 0x06 }, { '%', 0x06 }, { '6', 0x07 }, { '^', 0x07 },
	{ '7', 0x08 }, { '&', 0x08 }, { '8', 0x09 }, { '*', 0x09 }, { '9', 0x0A }, { '(', 0x0A },
	{ '0', 0x0B }, { ')', 0x0B }, { '-', 0x0C }, { '_', 0x0C }, { '=', 0x0D }, { '+', 0x0D },
	{ '[', 0x1A }, { '{', 0x1A }, { ']', 0x1B }, { '}', 0x1B }, { ';', 0x27 }, { ':', 0x27 },
	{ '\'', 0x28 }, { '"', 0x28 }, { '`', 0x29 }, { '~', 0x29 }, { '\\', 0x2B }, { '|', 0x2B },
	{ ',', 0x33 }, { '<', 0x33 }, { '.', 0x34 }, { '>', 0x34 }, { '/', 0x35 }, { '?', 0x35 },
	{ ' ', 0x39 }
};

static void special(keymap_result_t *r, int keycode)
{
	switch (keycode) {
		case TERMGFX_KEY_UP:        r->bios = 0x4800; r->xt = 0x48; break;
		case TERMGFX_KEY_DOWN:      r->bios = 0x5000; r->xt = 0x50; break;
		case TERMGFX_KEY_LEFT:      r->bios = 0x4B00; r->xt = 0x4B; break;
		case TERMGFX_KEY_RIGHT:     r->bios = 0x4D00; r->xt = 0x4D; break;
		case TERMGFX_KEY_HOME:      r->bios = 0x4700; r->xt = 0x47; break;
		case TERMGFX_KEY_END:       r->bios = 0x4F00; r->xt = 0x4F; break;
		case TERMGFX_KEY_PAGEUP:    r->bios = 0x4900; r->xt = 0x49; break;
		case TERMGFX_KEY_PAGEDOWN:  r->bios = 0x5100; r->xt = 0x51; break;
		case TERMGFX_KEY_KP5:       r->bios = 0x4C00; break;
		case TERMGFX_KEY_INSERT:    r->bios = 0x5200; break;
		case TERMGFX_KEY_DELETE:    r->bios = 0x5300; break;
		case TERMGFX_KEY_ENTER:     r->bios = 0x1C0D; break;
		case TERMGFX_KEY_ESCAPE:    r->bios = 0x011B; break;
		case TERMGFX_KEY_BACKSPACE: r->bios = 0x0E08; break;
		case TERMGFX_KEY_TAB:       r->bios = 0x0F09; break;
		case TERMGFX_KEY_F2:        r->action = KEYMAP_ACT_SOUND_TOGGLE; break;
		default:
			if (keycode >= TERMGFX_KEY_F1 && keycode <= TERMGFX_KEY_F9)
				r->bios = (uint16_t)((0x3B + (keycode - TERMGFX_KEY_F1)) << 8);
			break;
	}
}

static void printable(keymap_result_t *r, int keycode, int ascii, int mods)
{
	int    lower = (keycode >= 'A' && keycode <= 'Z') ? keycode + ('a' - 'A') : keycode;
	size_t k;

	if (lower >= 'a' && lower <= 'z') {
		int     i    = lower - 'a';
		uint8_t scan = letter_scan[i];

		if (mods & TERMGFX_MOD_CTRL) {
			if (lower == 'f')
				r->action = KEYMAP_ACT_FIT_CYCLE;
			else if (lower != 'j')
				r->bios = (uint16_t)(scan << 8 | (i + 1));
			return;
		}
		if (mods & TERMGFX_MOD_ALT) {
			r->bios = (uint16_t)(scan << 8);
			return;
		}
		r->bios = (uint16_t)(scan << 8 | (uint8_t)(ascii ? ascii : keycode));
		if (lower == 'a' || lower == 'z')
			r->xt = scan;
		return;
	}
	for (k = 0; k < sizeof char_scan / sizeof char_scan[0]; k++) {
		if (char_scan[k].ch == keycode) {
			r->bios = (uint16_t)(char_scan[k].scan << 8 | (uint8_t)keycode);
			return;
		}
	}
}

keymap_result_t keymap_translate(const termgfx_input_event_t *ev)
{
	keymap_result_t r;

	memset(&r, 0, sizeof r);
	if (ev->type != TERMGFX_EV_KEY_DOWN && ev->type != TERMGFX_EV_KEY_UP)
		return r;
	r.down = ev->type == TERMGFX_EV_KEY_DOWN;
	if (ev->keycode >= TERMGFX_KEY_FIRST)
		special(&r, ev->keycode);
	else
		printable(&r, ev->keycode, ev->ascii, ev->mods);
	if (!r.down) {
		r.bios   = 0;
		r.action = KEYMAP_ACT_NONE;
	}
	return r;
}

uint16_t keymap_sound_toggle_key(int sound_on)
{
	return sound_on ? 0x1011 : 0x1F13;
}
```

- [ ] **Step 4: Implement keyscript**

`src/doors/syncdrive/door/keyscript.h`:
```c
/* keyscript.h -- SYNCDRIVE_KEYS: a comma-separated key script for headless
 * (capture-mode) test runs, e.g. "wait:3000,enter,wait:2000,right,quit".
 * Tokens: wait:<ms>, quit, enter, esc, space, up, down, left, right, home,
 * end, pgup, pgdn, f1..f9, ctrl-<letter>, or one printable character. */
#ifndef SYNCDRIVE_KEYSCRIPT_H_
#define SYNCDRIVE_KEYSCRIPT_H_

#include "termgfx_termio.h"

enum { KEYSCRIPT_WAIT, KEYSCRIPT_KEY, KEYSCRIPT_QUIT };

typedef struct {
	int                   kind;
	int                   ms;
	termgfx_input_event_t ev;
} keyscript_step_t;

/* Returns the number of steps, or -1 on an unknown token, a bad wait, or more
 * than `max` steps. */
int keyscript_parse(const char *s, keyscript_step_t *out, int max);

#endif
```

`src/doors/syncdrive/door/keyscript.c`:
```c
/* keyscript.c -- see keyscript.h. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "keyscript.h"

static const struct {
	const char *name;
	int         keycode;
	int         ascii;
} names[] = {
	{ "enter", TERMGFX_KEY_ENTER, 0 }, { "esc", TERMGFX_KEY_ESCAPE, 0 },
	{ "space", ' ', ' ' }, { "up", TERMGFX_KEY_UP, 0 }, { "down", TERMGFX_KEY_DOWN, 0 },
	{ "left", TERMGFX_KEY_LEFT, 0 }, { "right", TERMGFX_KEY_RIGHT, 0 },
	{ "home", TERMGFX_KEY_HOME, 0 }, { "end", TERMGFX_KEY_END, 0 },
	{ "pgup", TERMGFX_KEY_PAGEUP, 0 }, { "pgdn", TERMGFX_KEY_PAGEDOWN, 0 }
};

static int parse_token(const char *t, size_t len, keyscript_step_t *st)
{
	size_t k;

	memset(st, 0, sizeof *st);
	st->kind    = KEYSCRIPT_KEY;
	st->ev.type = TERMGFX_EV_KEY_DOWN;
	if (len > 5 && strncmp(t, "wait:", 5) == 0) {
		char *end;
		long  ms = strtol(t + 5, &end, 10);

		if (end != t + len || ms < 0)
			return -1;
		st->kind = KEYSCRIPT_WAIT;
		st->ms   = (int)ms;
		return 0;
	}
	if (len == 4 && strncmp(t, "quit", 4) == 0) {
		st->kind = KEYSCRIPT_QUIT;
		return 0;
	}
	if (len == 6 && strncmp(t, "ctrl-", 5) == 0 && isalpha((unsigned char)t[5])) {
		st->ev.keycode = tolower((unsigned char)t[5]);
		st->ev.mods    = TERMGFX_MOD_CTRL;
		return 0;
	}
	if (len == 2 && t[0] == 'f' && t[1] >= '1' && t[1] <= '9') {
		st->ev.keycode = TERMGFX_KEY_F1 + (t[1] - '1');
		return 0;
	}
	for (k = 0; k < sizeof names / sizeof names[0]; k++) {
		if (strlen(names[k].name) == len && strncmp(t, names[k].name, len) == 0) {
			st->ev.keycode = names[k].keycode;
			st->ev.ascii   = names[k].ascii;
			return 0;
		}
	}
	if (len == 1 && t[0] > ' ' && t[0] < 0x7F && t[0] != ',') {
		st->ev.keycode = (unsigned char)t[0];
		st->ev.ascii   = (unsigned char)t[0];
		return 0;
	}
	return -1;
}

int keyscript_parse(const char *s, keyscript_step_t *out, int max)
{
	int n = 0;

	while (s != NULL && *s != '\0') {
		const char *comma = strchr(s, ',');
		size_t      len   = comma ? (size_t)(comma - s) : strlen(s);

		if (n >= max || parse_token(s, len, &out[n]) != 0)
			return -1;
		n++;
		s = comma ? comma + 1 : s + len;
	}
	return n;
}
```

- [ ] **Step 5: Run the tests**

Run: `cd ~/sbbs/src/doors/syncdrive && ./build.sh`
Expected: `100% tests passed, 0 tests failed out of 3`.

- [ ] **Step 6: Style**

Run: `uncrustify -c ../../uncrustify.cfg --replace --no-backup door/keymap.[ch] door/keyscript.[ch] tests/test_keymap.c tests/test_keyscript.c && ./build.sh`
Expected: tests pass.

---

### Task 3: PC-speaker synthesis

**Files:**
- Create: `door/speaker.h`, `door/speaker.c`, `tests/test_speaker.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```c
  #define SPEAKER_PIT_HZ  1193182.0
  #define SPEAKER_AMP     8192
  #define SPEAKER_RAMP_MS 2
  typedef struct { int rate; double phase, freq, gain, target, slew; } speaker_t;
  void speaker_init(speaker_t *s, int rate);
  void speaker_set(speaker_t *s, uint16_t divisor, int on);  /* divisor 0 = 65536 */
  void speaker_render(speaker_t *s, int16_t *stereo, size_t frames);
  ```

Band-limited square (PolyBLEP) so the 24 kHz stream does not alias, and a
2 ms gain ramp on gate changes so on/off edges do not click. A tone at or above
Nyquist is rendered as silence. Amplitude 8192 (-12 dBFS; a square's RMS
equals its peak).

- [ ] **Step 1: Write the failing test**

`src/doors/syncdrive/tests/test_speaker.c`:
```c
/* Unit tests for speaker.c. */
#include <assert.h>
#include <stdlib.h>
#include "speaker.h"

#define RATE 24000

static int16_t buf[RATE * 2];

static int rising_crossings(const int16_t *st, size_t frames)
{
	size_t k;
	int    n = 0;

	for (k = 1; k < frames; k++) {
		if (st[(k - 1) * 2] < 0 && st[k * 2] >= 0)
			n++;
	}
	return n;
}

int main(void)
{
	speaker_t s;
	size_t    k;
	int       n;

	/* Gated off from the start: pure silence. */
	speaker_init(&s, RATE);
	speaker_render(&s, buf, RATE);
	for (k = 0; k < RATE * 2; k++)
		assert(buf[k] == 0);

	/* 440 Hz: PIT divisor 1193182 / 440 = 2711.8 -> 2712 (439.97 Hz). */
	speaker_set(&s, 2712, 1);
	speaker_render(&s, buf, RATE);
	n = rising_crossings(buf, RATE);
	assert(n >= 438 && n <= 441);

	/* Stereo: both channels identical. Peak within the amplitude. */
	for (k = 0; k < RATE; k++) {
		assert(buf[k * 2] == buf[k * 2 + 1]);
		assert(abs(buf[k * 2]) <= SPEAKER_AMP + SPEAKER_AMP / 4);
	}

	/* Gate on from silence: the first sample is near zero (the ramp). */
	speaker_init(&s, RATE);
	speaker_set(&s, 2712, 1);
	speaker_render(&s, buf, 4);
	assert(abs(buf[0]) <= SPEAKER_AMP / 16);

	/* Gate off: silent once the ramp is over. */
	speaker_render(&s, buf, RATE / 10);
	speaker_set(&s, 2712, 0);
	speaker_render(&s, buf, RATE / 10);
	for (k = (size_t)(RATE * SPEAKER_RAMP_MS / 1000 + 1); k < RATE / 10; k++)
		assert(buf[k * 2] == 0);

	/* Divisor 1 (1.19 MHz, above Nyquist): silence. */
	speaker_init(&s, RATE);
	speaker_set(&s, 1, 1);
	speaker_render(&s, buf, RATE / 10);
	for (k = 0; k < RATE / 10 * 2; k++)
		assert(buf[k] == 0);

	/* Divisor 0 means 65536: 18.2 Hz. */
	speaker_init(&s, RATE);
	speaker_set(&s, 0, 1);
	speaker_render(&s, buf, RATE);
	n = rising_crossings(buf, RATE);
	assert(n >= 17 && n <= 19);
	return 0;
}
```

Append to `tests/CMakeLists.txt`:
```cmake
syncdrive_test(test_speaker
	${CMAKE_CURRENT_SOURCE_DIR}/../door/speaker.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_speaker.c)
target_link_libraries(test_speaker m)
```

- [ ] **Step 2: Run to verify failure**

Run: `./build.sh`
Expected: FAIL: `speaker.h` not found.

- [ ] **Step 3: Implement**

`src/doors/syncdrive/door/speaker.h`:
```c
/* speaker.h -- the PC speaker (PIT channel 2 + port 61h gate) as PCM. */
#ifndef SYNCDRIVE_SPEAKER_H_
#define SYNCDRIVE_SPEAKER_H_

#include <stddef.h>
#include <stdint.h>

#define SPEAKER_PIT_HZ  1193182.0
#define SPEAKER_AMP     8192
#define SPEAKER_RAMP_MS 2

typedef struct {
	int    rate;
	double phase;
	double freq;
	double gain;
	double target;
	double slew;
} speaker_t;

void speaker_init(speaker_t *s, int rate);
void speaker_set(speaker_t *s, uint16_t divisor, int on);
void speaker_render(speaker_t *s, int16_t *stereo, size_t frames);

#endif
```

`src/doors/syncdrive/door/speaker.c`:
```c
/* speaker.c -- band-limited (PolyBLEP) square wave with a gate ramp. */
#include <string.h>
#include "speaker.h"

static double polyblep(double t, double dt)
{
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.0;
	}
	if (t > 1.0 - dt) {
		t = (t - 1.0) / dt;
		return t * t + t + t + 1.0;
	}
	return 0.0;
}

void speaker_init(speaker_t *s, int rate)
{
	memset(s, 0, sizeof *s);
	s->rate = rate;
	s->slew = 1.0 / (rate * SPEAKER_RAMP_MS / 1000.0);
}

void speaker_set(speaker_t *s, uint16_t divisor, int on)
{
	s->freq   = SPEAKER_PIT_HZ / (divisor ? divisor : 65536);
	s->target = (on && s->freq < s->rate / 2.0) ? 1.0 : 0.0;
}

void speaker_render(speaker_t *s, int16_t *stereo, size_t frames)
{
	double dt = s->freq / s->rate;
	size_t k;

	for (k = 0; k < frames; k++) {
		double v = 0.0;

		if (s->gain < s->target) {
			s->gain += s->slew;
			if (s->gain > s->target)
				s->gain = s->target;
		} else if (s->gain > s->target) {
			s->gain -= s->slew;
			if (s->gain < s->target)
				s->gain = s->target;
		}
		if (s->gain > 0.0 && dt > 0.0 && dt < 0.5) {
			double half = s->phase + 0.5;

			if (half >= 1.0)
				half -= 1.0;
			v  = s->phase < 0.5 ? 1.0 : -1.0;
			v += polyblep(s->phase, dt);
			v -= polyblep(half, dt);
			s->phase += dt;
			if (s->phase >= 1.0)
				s->phase -= 1.0;
		}
		stereo[k * 2]     = (int16_t)(v * s->gain * SPEAKER_AMP);
		stereo[k * 2 + 1] = stereo[k * 2];
	}
}
```

- [ ] **Step 4: Run the tests**

Run: `./build.sh`
Expected: 4/4 tests pass.

- [ ] **Step 5: Style**

Run: `uncrustify -c ../../uncrustify.cfg --replace --no-backup door/speaker.[ch] tests/test_speaker.c && ./build.sh`

---

### Task 4: Alias field + score lock

**Files:**
- Create: `door/alias.h`, `door/alias.c`, `door/scores_lock.h`,
  `door/scores_lock.c`, `tests/test_alias.c`, `tests/test_scores_lock.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: xpdev `xp_lockfile()`, `unlock()` (`filewrap.h`).
- Produces:
  ```c
  #define ALIAS_FIELD 15
  void alias_sanitize(const char *in, char out[ALIAS_FIELD + 1]);
  void scores_lock_init(const char *data_dir);  /* remembers <dir>/SCORES.lck */
  int  scores_lock_acquire(void);               /* blocks; 0 ok, -1 error */
  void scores_lock_release(void);
  ```

The game's own name editor accepts 0x20..0x7A (`text_input_line()` in
`tdport/src/platform/input.c`), writes a 15-character space-padded field, and
the font has glyphs for that range. `alias_sanitize` produces the same shape:
empty input gives `""` (meaning "no alias"); otherwise 15 characters, each
outside 0x20..0x7A replaced by `?`, truncated, space-padded.

- [ ] **Step 1: Write the failing tests**

`src/doors/syncdrive/tests/test_alias.c`:
```c
/* Unit tests for alias.c. */
#include <assert.h>
#include <string.h>
#include "alias.h"

int main(void)
{
	char out[ALIAS_FIELD + 1];

	alias_sanitize("Digital Man", out);
	assert(strcmp(out, "Digital Man    ") == 0);

	alias_sanitize("A Very Long Alias Name", out);
	assert(strcmp(out, "A Very Long Ali") == 0);

	alias_sanitize("x{y}~z|\x81", out);      /* 0x7B..0x7E and high bytes */
	assert(strcmp(out, "x?y???z??      ") == 0);

	alias_sanitize("", out);
	assert(out[0] == '\0');
	alias_sanitize(NULL, out);
	assert(out[0] == '\0');
	return 0;
}
```

`src/doors/syncdrive/tests/test_scores_lock.c`:
```c
/* Unit test for scores_lock.c: a second process blocks until the first
 * releases. POSIX only (fork). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "scores_lock.h"

static long now_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000L + tv.tv_usec / 1000;
}

int main(void)
{
	char  dir[] = "/tmp/syncdrive_lockXXXXXX";
	pid_t pid;
	int   status;

	assert(mkdtemp(dir) != NULL);
	scores_lock_init(dir);
	assert(scores_lock_acquire() == 0);

	pid = fork();
	assert(pid >= 0);
	if (pid == 0) {
		long t0 = now_ms();

		scores_lock_init(dir);
		if (scores_lock_acquire() != 0)
			_exit(2);
		scores_lock_release();
		_exit(now_ms() - t0 >= 250 ? 0 : 1);
	}
	usleep(400 * 1000);
	scores_lock_release();
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return 0;
}
```

Append to `tests/CMakeLists.txt` (xpdev's `filewrap.c` compiled in as a
source, with its include dir):
```cmake
syncdrive_test(test_alias
	${CMAKE_CURRENT_SOURCE_DIR}/../door/alias.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_alias.c)

syncdrive_test(test_scores_lock
	${CMAKE_CURRENT_SOURCE_DIR}/../door/scores_lock.c
	${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev/filewrap.c
	${CMAKE_CURRENT_SOURCE_DIR}/test_scores_lock.c)
target_include_directories(test_scores_lock PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev)
```

- [ ] **Step 2: Run to verify failure**

Run: `./build.sh`
Expected: FAIL: `alias.h` / `scores_lock.h` not found. If `filewrap.c`
fails to link on its own (unresolved xpdev symbols), replace that source line
with a link to the xpdev library the way Task 5's executable does
(`add_subdirectory(../../xpdev)` in the top-level CMake, then
`target_link_libraries(test_scores_lock xpdev)`), and note it.

- [ ] **Step 3: Implement**

`src/doors/syncdrive/door/alias.h`:
```c
/* alias.h -- a BBS alias as Test Drive's 15-character high-score name. */
#ifndef SYNCDRIVE_ALIAS_H_
#define SYNCDRIVE_ALIAS_H_

#define ALIAS_FIELD 15

void alias_sanitize(const char *in, char out[ALIAS_FIELD + 1]);

#endif
```

`src/doors/syncdrive/door/alias.c`:
```c
/* alias.c -- see alias.h. The game's name editor accepts 0x20..0x7A. */
#include <stddef.h>
#include "alias.h"

void alias_sanitize(const char *in, char out[ALIAS_FIELD + 1])
{
	int i = 0;

	out[0] = '\0';
	if (in == NULL || *in == '\0')
		return;
	for (; i < ALIAS_FIELD && in[i] != '\0'; i++) {
		unsigned char c = (unsigned char)in[i];

		out[i] = (c >= 0x20 && c <= 0x7A) ? (char)c : '?';
	}
	for (; i < ALIAS_FIELD; i++)
		out[i] = ' ';
	out[ALIAS_FIELD] = '\0';
}
```

`src/doors/syncdrive/door/scores_lock.h`:
```c
/* scores_lock.h -- serializes SCORES read-merge-write across door processes
 * (and hosts sharing the data dir) with a lock on <data-dir>/SCORES.lck. */
#ifndef SYNCDRIVE_SCORES_LOCK_H_
#define SYNCDRIVE_SCORES_LOCK_H_

void scores_lock_init(const char *data_dir);
int  scores_lock_acquire(void);
void scores_lock_release(void);

#endif
```

`src/doors/syncdrive/door/scores_lock.c`:
```c
/* scores_lock.c -- see scores_lock.h. */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "filewrap.h"
#include "scores_lock.h"

static char lock_path[1024];
static int  lock_fd = -1;

void scores_lock_init(const char *data_dir)
{
	snprintf(lock_path, sizeof lock_path, "%s/SCORES.lck", data_dir);
}

int scores_lock_acquire(void)
{
	lock_fd = open(lock_path, O_RDWR | O_CREAT, 0666);
	if (lock_fd < 0)
		return -1;
	if (xp_lockfile(lock_fd, 0, 1, true) != 0) {
		close(lock_fd);
		lock_fd = -1;
		return -1;
	}
	return 0;
}

void scores_lock_release(void)
{
	if (lock_fd < 0)
		return;
	unlock(lock_fd, 0, 1);
	close(lock_fd);
	lock_fd = -1;
}
```

- [ ] **Step 4: Run the tests**

Run: `./build.sh`
Expected: 6/6 tests pass.

- [ ] **Step 5: Style**

Run: `uncrustify -c ../../uncrustify.cfg --replace --no-backup door/alias.[ch] door/scores_lock.[ch] tests/test_alias.c tests/test_scores_lock.c && ./build.sh`

---

### Task 5: Vendor the engine, the termio host, main(); headless run

**Files:**
- Create: `tdport/` (vendored), `compat/SDL3/SDL.h`, `PROVENANCE.md`,
  `CLAUDE.md`, `door/host_term_ext.h`, `door/host_term.c`, `door/syncdrive.c`
- Modify: `CMakeLists.txt` (executable, termgfx, xpdev),
  `src/doors/build.sh` (target row)

**Interfaces:**
- Consumes: everything from Tasks 1-4; `tdport/src/host.h` (implement every
  function it declares); termio API from `termgfx_termio.h`;
  `termgfx_door32_is_path()`, `termgfx_door32_read()` (`door32.h`);
  `termgfx_plat_now_ms()`, `termgfx_plat_sleep_ms()` (`termgfx_plat.h`);
  xpdev `fexistcase()`, `mkpath()`, `iniReadFile()`, `iniGetString()`,
  `iniGetInteger()`, `strListFree()`.
- Produces (`door/host_term_ext.h`, used by Task 6's patch and main):
  ```c
  const char *host_player_name(void);   /* sanitized 15-char alias, "" if none */
  void host_set_player_name(const char *alias);
  void host_set_data_dir(const char *dir);
  void host_scores_lock(void);
  void host_scores_unlock(void);
  void host_set_held_keys_allowed(bool on);   /* syncdrive.ini [input] held_keys */
  void host_set_keyscript(const char *s);     /* SYNCDRIVE_KEYS */
  ```

- [ ] **Step 1: Vendor tdport and fetch dev game data**

```bash
cd ~/sbbs/src/doors/syncdrive
tmp=$(mktemp -d)
git clone -q https://github.com/kylofon/test-drive-sdl3 "$tmp/td"
git -C "$tmp/td" checkout -q a4d23077770fcc2cae81680fe4e3fd73163c0915
mkdir -p tdport
cp -r "$tmp/td/tdport/src" tdport/src
rm tdport/src/host.c tdport/src/main.c
cp "$tmp/td/LICENSE" tdport/LICENSE
cp "$tmp/td/tdport/PORTING.md" tdport/PORTING.md
cp "$tmp/td/FORMATS.md" tdport/FORMATS.md
rm -rf "$tmp"
mkdir -p game && cd game
curl -sSL -o td.zip 'https://archive.org/download/TestDrive_1987/Test%20Drive.zip'
unzip -qo td.zip && rm td.zip
md5sum TDEGA.EXE
```
Expected: `cdf8d1a767d559db559a0b829e1bb3b0  TDEGA.EXE`. (`game/` is
gitignored.)

- [ ] **Step 2: The SDL shim**

`src/doors/syncdrive/compat/SDL3/SDL.h`:
```c
/* SDL3/SDL.h -- the handful of SDL calls tdport/src/mem.c makes, on libc, so
 * the vendored file compiles unedited. Nothing else in the build includes
 * SDL; the platform layer is door/host_term.c. */
#ifndef SYNCDRIVE_COMPAT_SDL_H_
#define SYNCDRIVE_COMPAT_SDL_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_malloc  malloc
#define SDL_calloc  calloc
#define SDL_realloc realloc
#define SDL_free    free

static inline const char *SDL_GetError(void)
{
	return strerror(errno);
}

static inline void *SDL_LoadFile(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	void *buf;
	long  n;

	if (f == NULL)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	buf = malloc(n > 0 ? (size_t)n : 1);
	if (buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) {
		free(buf);
		buf = NULL;
	}
	fclose(f);
	if (buf != NULL)
		*len = (size_t)n;
	return buf;
}

#endif
```

- [ ] **Step 3: host_term_ext.h and host_term.c**

`src/doors/syncdrive/door/host_term_ext.h`:
```c
/* host_term_ext.h -- door-side host calls beyond tdport's host.h, used by
 * main() and by the patched tdport/src/game/flow_scores.c. */
#ifndef SYNCDRIVE_HOST_TERM_EXT_H_
#define SYNCDRIVE_HOST_TERM_EXT_H_

#include <stdbool.h>

const char *host_player_name(void);
void        host_set_player_name(const char *alias);
void        host_set_data_dir(const char *dir);
void        host_scores_lock(void);
void        host_scores_unlock(void);
void        host_set_held_keys_allowed(bool on);
void        host_set_keyscript(const char *s);

#endif
```

`src/doors/syncdrive/door/host_term.c`:
```c
/* host_term.c -- tdport's host.h implemented on termgfx_termio: the door's
 * replacement for upstream's SDL host.c. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "host.h"
#include "mem.h"
#include "symbols.h"

#include "termgfx_termio.h"
#include "termgfx_plat.h"
#include "dirwrap.h"

#include "alias.h"
#include "frame.h"
#include "host_term_ext.h"
#include "keymap.h"
#include "keyscript.h"
#include "scores_lock.h"
#include "speaker.h"

#define AUDIO_RATE     24000              /* termio's mixer rate (TERMGFX_AUDIO_RATE) */
#define PRESENT_MIN_MS 8
#define KBD_SIZE       16
#define CATCHUP_TICKS  50                 /* at most 0.5 s of ticks per pump */
#define SCRIPT_MAX     256

static char game_dir[1024] = ".";
static char data_dir[1024] = ".";
static char player[ALIAS_FIELD + 1];

static void (*tick_handler)(void);
static bool (*frame_source)(u32 *);
static u32      frame[HOST_FRAME_MAX_W * HOST_FRAME_MAX_H];
static uint8_t  frame_idx[FRAME_W * FRAME_H];
static uint8_t  frame_pal[768];
static uint32_t last_present_ms;

static uint64_t clock_start_ms;
static uint64_t ticks_run;

static u16  kbd_buf[KBD_SIZE];
static int  kbd_head, kbd_tail;
static bool held_keys;                    /* off until the terminal sends a release */
static bool held_keys_allowed = true;     /* syncdrive.ini [input] held_keys = off */
static bool held[128];

static speaker_t spk;
static int       audio_on = -1;           /* -1 = not asked yet */
static int16_t   pcm[2 * 4096];
static size_t    pcm_frames;
static double    pcm_frac;

static keyscript_step_t script[SCRIPT_MAX];
static int              script_len, script_pos;
static uint32_t         script_due_ms;

static int frame_rate = 8;
static uint64_t next_frame_ms;

/* ------------------------------------------------------------ door-side API */

void host_set_player_name(const char *alias)
{
	alias_sanitize(alias, player);
}

const char *host_player_name(void)
{
	return player;
}

void host_set_data_dir(const char *dir)
{
	snprintf(data_dir, sizeof data_dir, "%s", dir);
	mkpath(data_dir);
	scores_lock_init(data_dir);
}

void host_scores_lock(void)
{
	if (scores_lock_acquire() != 0)
		fprintf(stderr, "syncdrive: SCORES lock failed in %s\n", data_dir);
}

void host_scores_unlock(void)
{
	scores_lock_release();
}

void host_set_held_keys_allowed(bool on)
{
	held_keys_allowed = on;
}

void host_set_keyscript(const char *s)
{
	script_len = keyscript_parse(s, script, SCRIPT_MAX);
	if (script_len < 0) {
		fprintf(stderr, "syncdrive: bad SYNCDRIVE_KEYS script\n");
		script_len = 0;
	}
	script_pos    = 0;
	script_due_ms = termgfx_plat_now_ms();
}

/* ------------------------------------------------------------ lifecycle */

bool host_init(const char *dir, int window_scale)
{
	(void)window_scale;
	snprintf(game_dir, sizeof game_dir, "%s", dir);
	frame_palette(frame_pal);
	speaker_init(&spk, AUDIO_RATE);
	clock_start_ms = termgfx_plat_now_ms();
	ticks_run      = 0;
	return true;
}

void host_shutdown(void)
{
	termgfx_termio_shutdown();
}

void host_set_tick_handler(void (*handler)(void))
{
	tick_handler = handler;
}

void host_set_frame_source(bool (*compose)(u32 *xrgb), int w, int h)
{
	(void)w;
	(void)h;              /* EGA build: always 320x200 */
	frame_source = compose;
}

/* ------------------------------------------------------------ keyboard */

static void kbd_push(u16 key)
{
	int next = (kbd_tail + 1) % KBD_SIZE;

	if (next == kbd_head)
		return;           /* full: the BIOS drops the key */
	kbd_buf[kbd_tail] = key;
	kbd_tail          = next;
}

static void handle_key(const termgfx_input_event_t *ev)
{
	keymap_result_t r = keymap_translate(ev);

	if (r.xt != 0 && r.xt < 128)
		held[r.xt] = r.down != 0;
	if (!r.down && held_keys_allowed)
		held_keys = true;         /* this terminal reports releases */
	if (r.bios != 0)
		kbd_push(r.bios);
	if (r.action == KEYMAP_ACT_SOUND_TOGGLE)
		kbd_push(keymap_sound_toggle_key((DSB(DS_snd_flags) & 4) != 0));
	else if (r.action == KEYMAP_ACT_FIT_CYCLE)
		termgfx_termio_fit_cycle();
}

static void run_script(uint32_t now)
{
	while (script_pos < script_len && (int32_t)(now - script_due_ms) >= 0) {
		const keyscript_step_t *st = &script[script_pos++];

		if (st->kind == KEYSCRIPT_WAIT)
			script_due_ms = now + (uint32_t)st->ms;
		else if (st->kind == KEYSCRIPT_QUIT)
			exit(0);
		else
			handle_key(&st->ev);
	}
}

static void poll_input(void)
{
	termgfx_input_event_t ev;

	termgfx_termio_pump();
	if (termgfx_termio_hung_up() || termgfx_termio_quit_requested())
		exit(0);          /* atexit runs termgfx_termio_shutdown() */
	while (termgfx_termio_next_event(&ev))
		handle_key(&ev);
	run_script(termgfx_plat_now_ms());
}

bool host_kbd_peek(u16 *key)
{
	poll_input();
	if (kbd_head == kbd_tail)
		return false;
	if (key)
		*key = kbd_buf[kbd_head];
	return true;
}

bool host_kbd_read(u16 *key)
{
	poll_input();
	if (kbd_head == kbd_tail)
		return false;
	if (key)
		*key = kbd_buf[kbd_head];
	kbd_head = (kbd_head + 1) % KBD_SIZE;
	return true;
}

void host_kbd_flush(void)
{
	poll_input();
	kbd_head = kbd_tail = 0;
}

u8 host_kbd_shift_flags(void)
{
	return 0;             /* no live modifier state from a terminal */
}

void host_set_held_keys(bool on)
{
	held_keys = on;
}

bool host_held_keys(void)
{
	return held_keys;
}

bool host_xt_key_down(u8 xt_scan)
{
	return xt_scan < 128 && held[xt_scan];
}

bool host_joy_read(s16 *x, s16 *y, u8 *buttons)
{
	(void)x;
	(void)y;
	(void)buttons;
	return false;
}

/* ------------------------------------------------------------ audio */

static void audio_for_one_tick(void)
{
	size_t n;

	if (audio_on < 0)
		audio_on = termgfx_termio_audio_available();
	if (!audio_on)
		return;
	pcm_frac += (double)AUDIO_RATE * PIT_DIV_GAME / PIT_HZ;
	n         = (size_t)pcm_frac;
	pcm_frac -= (double)n;
	if (pcm_frames + n > sizeof pcm / sizeof pcm[0] / 2)
		n = sizeof pcm / sizeof pcm[0] / 2 - pcm_frames;
	speaker_render(&spk, pcm + pcm_frames * 2, n);
	pcm_frames += n;
}

static void audio_flush(void)
{
	if (pcm_frames == 0)
		return;
	termgfx_termio_audio_stream(pcm, pcm_frames);
	pcm_frames = 0;
}

void host_speaker(u16 divisor, bool on)
{
	speaker_set(&spk, divisor, on);
}

/* ------------------------------------------------------------ video + clock */

static bool present_if_changed(void)
{
	if (frame_source == NULL || !frame_source(frame))
		return false;
	frame_convert(frame, frame_idx, FRAME_W * FRAME_H);
	termgfx_termio_present(frame_idx, frame_pal);
	last_present_ms = termgfx_plat_now_ms();
	return true;
}

void host_present_now(void)
{
	present_if_changed();
}

static uint64_t tick_due_ms(uint64_t n)
{
	return clock_start_ms + n * PIT_DIV_GAME * 1000u / PIT_HZ;
}

void host_pump(void)
{
	bool     worked = false;
	uint64_t now;
	int      budget = CATCHUP_TICKS;

	poll_input();
	now = termgfx_plat_now_ms();
	while (tick_due_ms(ticks_run + 1) <= now && budget-- > 0) {
		ticks_run++;
		if (tick_handler)
			tick_handler();
		audio_for_one_tick();
		worked = true;
	}
	if (budget < 0)           /* fell behind: resynchronize the clock */
		clock_start_ms = now - (tick_due_ms(ticks_run) - clock_start_ms);
	audio_flush();

	if ((uint32_t)now - last_present_ms >= PRESENT_MIN_MS && present_if_changed())
		worked = true;
	termgfx_termio_tick();
	if (!worked)
		termgfx_plat_sleep_ms(1);
}

void host_set_frame_rate(int fps)
{
	frame_rate = fps < 0 ? 0 : fps;
}

int host_frame_rate(void)
{
	return frame_rate;
}

void host_frame_begin(void)
{
	uint64_t period, now;

	host_pump();
	if (frame_rate <= 0)
		return;
	period = 1000u / (uint64_t)frame_rate;
	now    = termgfx_plat_now_ms();
	if (next_frame_ms == 0 || now > next_frame_ms + period)
		next_frame_ms = now;  /* resync after a stall */
	while (termgfx_plat_now_ms() < next_frame_ms)
		host_pump();
	next_frame_ms += period;
}

/* ------------------------------------------------------------ files + errors */

char *host_game_path(const char *name, bool create)
{
	char path[2048];

	if (strcasecmp(name, "SCORES") == 0) {
		snprintf(path, sizeof path, "%s/SCORES", data_dir);
		if (create || fexist(path))
			return strdup(path);
		return NULL;
	}
	snprintf(path, sizeof path, "%s/%s", game_dir, name);
	if (fexistcase(path) || create)
		return strdup(path);
	return NULL;
}

void host_free(void *p)
{
	free(p);
}

_Noreturn void host_fatal(const char *fmt, ...)
{
	char    msg[512];
	char    line[600];
	va_list ap;
	int     n;

	va_start(ap, fmt);
	vsnprintf(msg, sizeof msg, fmt, ap);
	va_end(ap);
	fprintf(stderr, "syncdrive: fatal: %s\n", msg);
	if (termgfx_termio_active()) {
		n = snprintf(line, sizeof line, "\x1b[1;1H\x1b[0m\x1b[2KTest Drive: %s\r\n", msg);
		termgfx_termio_write(line, (size_t)n);
		termgfx_termio_flush();
		termgfx_plat_sleep_ms(3000);
	}
	exit(3);
}
```

- [ ] **Step 4: main()**

`src/doors/syncdrive/door/syncdrive.c`:
```c
/* syncdrive.c -- door entry point: Test Drive (1987) over termgfx.
 *
 * usage: syncdrive [DOOR32.SYS] [-s<fd>] [--game-dir=DIR] [--data-dir=DIR]
 *                  [--check]
 * termgfx_termio_init() consumes the DOOR32.SYS path and -s<fd>. --check runs
 * the TDEGA.EXE loader alone and exits (for getdata.js). */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "mem.h"

#include "door32.h"
#include "termgfx_termio.h"
#include "ini_file.h"

#include "host_term_ext.h"

int game_main(void);      /* tdport/src/game/flow.c */

static const char *opt_value(const char *arg, const char *name)
{
	size_t n = strlen(name);

	return strncmp(arg, name, n) == 0 ? arg + n : NULL;
}

static void read_ini(int *frame_rate)
{
	FILE      *f = fopen("syncdrive.ini", "r");
	str_list_t ini;
	char       val[INI_MAX_VALUE_LEN];

	if (f == NULL)
		return;
	ini = iniReadFile(f);
	fclose(f);
	iniGetString(ini, "input", "held_keys", "auto", val);
	host_set_held_keys_allowed(strcmp(val, "off") != 0);
	*frame_rate = iniGetInteger(ini, "game", "frame_rate", *frame_rate);
	strListFree(&ini);
}

int main(int argc, char **argv)
{
	const char *game = ".";
	const char *data = NULL;
	const char *v;
	const char *keys;
	bool        check      = false;
	int         frame_rate = 8;
	char        exe_path[1100];
	char        err[256];
	int         i;

	for (i = 1; i < argc; i++) {
		if ((v = opt_value(argv[i], "--game-dir=")) != NULL)
			game = v;
		else if ((v = opt_value(argv[i], "--data-dir=")) != NULL)
			data = v;
		else if (strcmp(argv[i], "--check") == 0)
			check = true;
	}
	snprintf(exe_path, sizeof exe_path, "%s/TDEGA.EXE", game);
	if (check) {
		if (!mem_load_exe(exe_path, err, sizeof err)) {
			fprintf(stderr, "%s\n", err);
			return 1;
		}
		printf("TDEGA.EXE ok: image %u bytes, DGROUP %04X\n", mem_image_size, DGROUP);
		return 0;
	}

	termgfx_termio_set_app_name("syncdrive");
	termgfx_termio_set_mouse(0);
	termgfx_termio_init(argc, argv);
	atexit(termgfx_termio_shutdown);

	for (i = 1; i < argc; i++) {
		termgfx_door32_t d;

		if (termgfx_door32_is_path(argv[i]) && termgfx_door32_read(argv[i], &d) == 0)
			host_set_player_name(d.alias);
	}
	read_ini(&frame_rate);
	host_set_data_dir(data != NULL ? data : game);
	if (!host_init(game, 1))
		return 1;
	host_set_frame_rate(frame_rate);
	if ((keys = getenv("SYNCDRIVE_KEYS")) != NULL)
		host_set_keyscript(keys);
	if (!mem_load_exe(exe_path, err, sizeof err))
		host_fatal("%s", err);
	return game_main();
}
```

Check before building: `grep -n 'host_init\|gfx_init\|timer_init' tdport/src/game/flow.c`
confirms `game_main()` calls `gfx_init()` and `timer_init()` itself (upstream
`main.c` only called `mem_load_exe`, `host_init`, `game_main`). If it does
not, call them after `mem_load_exe` in `main()` in the same order upstream's
`main.c` used, and note it.

- [ ] **Step 5: Executable CMake**

Insert into `src/doors/syncdrive/CMakeLists.txt` above the tests block:
```cmake
set(TDPORT ${CMAKE_CURRENT_SOURCE_DIR}/tdport/src)
file(GLOB_RECURSE TD_SOURCES CONFIGURE_DEPENDS ${TDPORT}/*.c)
list(FILTER TD_SOURCES EXCLUDE REGEX "/platform/gfx_cga\\.c$")

add_executable(syncdrive ${TD_SOURCES}
	door/syncdrive.c
	door/host_term.c
	door/frame.c
	door/keymap.c
	door/keyscript.c
	door/speaker.c
	door/alias.c
	door/scores_lock.c)

# compat/ first: tdport/src/mem.c includes <SDL3/SDL.h>, which must be the shim.
target_include_directories(syncdrive BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_include_directories(syncdrive PRIVATE ${TDPORT} ${CMAKE_CURRENT_SOURCE_DIR}/door)
set_target_properties(syncdrive PROPERTIES LINKER_LANGUAGE CXX)

# tdport reads multi-byte values through byte pointers (see its mem.h).
set_source_files_properties(${TD_SOURCES} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-w")
target_compile_options(syncdrive PRIVATE -Wall -Wextra)

# termgfx without WITH_JXL: sixel only (DESIGN.md sec. 1).
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../termgfx termgfx)
target_link_libraries(syncdrive termgfx)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
	set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
	set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
	foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
		set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
	endforeach()
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev xpdev EXCLUDE_FROM_ALL)
endif()
target_include_directories(syncdrive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../xpdev)
target_link_libraries(syncdrive xpdev m pthread ${CMAKE_DL_LIBS})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../build/SynchronetMacros.cmake)
synchronet_gitinfo(syncdrive)
```

Add the row to `src/doors/build.sh`'s `targets()` list, after `syncscumm`:
```
	syncdrive    cmake  syncdrive
```
(keep the row's tab indentation; it sits inside a `<<-EOF` heredoc.)

- [ ] **Step 6: Build and run the loader check**

Run:
```bash
cd ~/sbbs/src/doors/syncdrive && ./build.sh && ./build/syncdrive --check --game-dir=game
```
Expected: 6/6 unit tests pass, then
`TDEGA.EXE ok: image 81424 bytes, DGROUP 1C9A`. Fix any compile error in
OUR files; tdport warnings are suppressed by design. A tdport compile ERROR
means the shim or include order is wrong, not a reason to edit tdport.

- [ ] **Step 7: Headless run to the title and car select**

The key script below is a starting point: the exact screens between launch
and car selection are discovered from the captured frames, then the script is
adjusted until the car-select screen is reached.

```bash
cd ~/sbbs/src/doors/syncdrive/game
rm -f /tmp/syncdrive.six
SYNCDRIVE_SIXELOUT=/tmp/syncdrive.six \
SYNCDRIVE_KEYS='wait:4000,enter,wait:3000,enter,wait:3000,enter,wait:3000,quit' \
timeout 60 ../build/syncdrive --game-dir=. --data-dir=/tmp; echo "exit=$?"
python3 - <<'EOF'
import re, subprocess, os
data = open('/tmp/syncdrive.six', 'rb').read()
frames = re.findall(rb'\x1bP.*?\x1b\\', data, re.S)
print(len(frames), 'frames')
os.makedirs('/tmp/syncdrive_frames', exist_ok=True)
for i, f in enumerate(frames):
    p = '/tmp/syncdrive_frames/f%03d.six' % i
    open(p, 'wb').write(f)
    subprocess.run(['convert', 'sixel:' + p, p[:-4] + '.png'], check=True)
EOF
ls /tmp/syncdrive_frames/*.png | head
```
Expected: `exit=0`, several frames, PNGs that show the Accolade logo, the
title, and eventually car selection. View them with the Read tool. Adjust
`SYNCDRIVE_KEYS` until the last frames show the car-select screen, and record
the working script in `CLAUDE.md` (Step 8) as the headless smoke test.
If the process hangs to the 60 s timeout, a busy-wait loop is not reaching
`host_pump()`/`poll_input()`: find it before going on.

- [ ] **Step 8: PROVENANCE.md and CLAUDE.md**

`src/doors/syncdrive/PROVENANCE.md`:
```markdown
# Provenance: tdport (Test Drive 1987, faithful C port)

- Upstream: https://github.com/kylofon/test-drive-sdl3
- Commit: a4d23077770fcc2cae81680fe4e3fd73163c0915 (2026-09-18)
- License: MIT (tdport/LICENSE), Copyright (c) 2026 Krzysztof Kania
- Copied: tdport/src/ (as tdport/src/), tdport/PORTING.md, FORMATS.md,
  LICENSE. Not copied: the reverse-engineering material (port/, tools/),
  which stays upstream.
- Omitted from tdport/src: host.c and main.c (SDL3). Replaced by
  door/host_term.c and door/syncdrive.c.
- Build shim: compat/SDL3/SDL.h maps the SDL calls in tdport/src/mem.c to
  libc, so mem.c compiles unedited.
- The game data (TDEGA.EXE, *.PES, ...) is not part of upstream or of this
  door; sysops supply it (xtrn/syncdrive/getdata.js).

## Local patches

None yet.
```

`src/doors/syncdrive/CLAUDE.md`:
```markdown
# Working in syncdrive

## Ours vs vendored

`tdport/` is a frozen copy of upstream (see PROVENANCE.md). Do not edit or
reformat it; the one exception is a patch recorded in PROVENANCE.md "Local
patches". Everything under `door/`, `tests/`, `compat/` and the build files is
ours: follow `../../uncrustify.cfg` and run uncrustify on changed files as
the last step.

The engine reaches the platform only through `tdport/src/host.h`, implemented
by `door/host_term.c`. Pure logic goes in its own `door/*.c` with a test in
`tests/`, so it can be tested without the engine or a terminal.

## Build and test

    ./build.sh            # configure, build, run unit tests
    ./build/syncdrive --check --game-dir=game

Dev game data lives in `game/` (gitignored). Fetch it from
https://archive.org/download/TestDrive_1987/Test%20Drive.zip; TDEGA.EXE md5
cdf8d1a767d559db559a0b829e1bb3b0.

## Headless smoke test

    cd game
    SYNCDRIVE_SIXELOUT=/tmp/syncdrive.six SYNCDRIVE_KEYS='<script from Task 5>' \
        timeout 60 ../build/syncdrive --game-dir=. --data-dir=/tmp

Split /tmp/syncdrive.six on DCS ... ST and convert each with
`convert sixel:<file> <png>` to look at the frames.
```
Replace `<script from Task 5>` with the script found in Step 7.

- [ ] **Step 9: Style**

Run: `uncrustify -c ../../uncrustify.cfg --replace --no-backup door/*.c door/*.h compat/SDL3/SDL.h && ./build.sh && ./build/syncdrive --check --game-dir=game`
Expected: tests pass, loader check ok.

- [ ] **Step 10: Commit (milestone 1: the door builds and runs headless)**

```bash
cd ~/sbbs
git diff --cached --stat          # must be empty or only our paths
git add src/doors/syncdrive src/doors/build.sh
git status --short src/doors/syncdrive | grep -v '^A ' ; true   # nothing under game/ or build/
cat > /tmp/syncdrive-msg1 <<'EOF'
syncdrive: Test Drive (1987) as a termgfx door

Vendor kylofon/test-drive-sdl3's faithful C port of TDEGA.EXE (MIT) and run
it on termgfx_termio in place of upstream's SDL host: a fixed 16-color EGA
palette for sixel, BIOS key words and held-key state from termgfx key
events, the PC speaker synthesized as band-limited PCM, and a key script for
headless runs. The original game data is not included; sysops supply it.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
EOF
awk 'length > 78' /tmp/syncdrive-msg1
git commit -F /tmp/syncdrive-msg1
```
Expected: the awk prints nothing; the commit contains DESIGN.md, the plan,
the scaffold, `tdport/`, and the `build.sh` row, and no `game/` or `build/`
files.

---

### Task 6: High-score patch (locked alias, locked merge)

**Files:**
- Modify: `tdport/src/game/flow_scores.c` (the one engine patch)
- Modify: `PROVENANCE.md` ("Local patches")

**Interfaces:**
- Consumes: `host_player_name()`, `host_scores_lock()`,
  `host_scores_unlock()` from `host_term_ext.h` (the include dir `door/` is
  already on the target's path).

- [ ] **Step 1: Replace `high_scores()` and `scores_enter_name()`**

In `tdport/src/game/flow_scores.c`, add after the existing includes:
```c
#include "host_term_ext.h"   /* SYNCDRIVE: alias + SCORES lock (PROVENANCE.md) */
```

Replace the body of `high_scores()` with:
```c
int high_scores(s32 new_score)
{
    /* original: char names[160], cars[160]; long scores[8] (uninitialised stack). One spare row here. */
    char names[9 * 20], cars[9 * 20];
    s32 scores[9];
    char name[20];
    memset(names, 0, sizeof names);
    memset(cars, 0, sizeof cars);
    memset(scores, 0, sizeof scores);

    scores_load(names, scores, cars);
    if (new_score > scores[7]) {
        /* SYNCDRIVE: take the name first, then re-read, insert and save under a lock, so two nodes
         * qualifying at once both land in the table (PROVENANCE.md patch 1). */
        scores_get_name(new_score, DSS(DS_g_selectedCar), name);
        if (name[0] != 0) {
            host_scores_lock();
            memset(names, 0, sizeof names);
            memset(cars, 0, sizeof cars);
            memset(scores, 0, sizeof scores);
            scores_load(names, scores, cars);
            if (new_score > scores[7]) {
                scores_insert(name, new_score, DSS(DS_g_selectedCar), names, scores, cars);
                scores_save(names, scores, cars);
            }
            host_scores_unlock();
        }
    }
    int r = scores_show(names, scores, cars);
    if (r == -1) r = credits_show();
    return r;
}
```

Replace `scores_enter_name()` with these two functions (keep the original
drawing calls exactly; only the input and the insertion change):
```c
/* 0x1482 scores_enter_name, split by SYNCDRIVE into scores_get_name + scores_insert (PROVENANCE.md patch 1) */
void scores_get_name(s32 score, int car, char *name)
{
    memset(name, 0, 20);
    gfx_clear_screen(0);
    snd_play_oneshot(far_rd(DGROUP, DS_g_songHiScore));
    gfx_select_target(gfx_screen_desc());
    FarPtr ll = load_archive(DSTR(EGA_CGA(0x2EA, 0x2E0)), EGA_CGA(2000, 1000)); /* "llogo.pes" (.cmp) */
    blit_copy_own(res_find(ll, flow_car_name(car)));      /* 4-char match "coun", "lotu", ... */
    gfx_set_text_colours(3, 0);
    draw_text_centered(DSTR(EGA_CGA(0x2F4, 0x2EA)), 0x96);                /* "You have qualified as one" */
    draw_text_centered(DSTR(EGA_CGA(0x30E, 0x304)), 0xA0);                /* "of Test Drive's best drivers." */
    gfx_draw_text(DSTR(EGA_CGA(0x32C, 0x322)), 0x14, 0xB4);               /* "Enter your name:" */
    draw_rect_outline(0xAC, 0xAF, 0x13C, 0xBE, 0xFF);     /* colour 0xFFFF */
    if (host_player_name()[0] != 0) {
        /* SYNCDRIVE: the caller's BBS alias, not editable */
        strncpy(name, host_player_name(), 19);
        gfx_draw_text(name, 0xB8, 0xB4);
        set_deadline(300);                                /* 3 s at 100 Hz, or any key */
        menu_key();
    } else {
        text_input_line(name, 15, 0xB8, 0xB4, 3000);
    }
    snd_stop_oneshot();
    (void)score;
}

void scores_insert(const char *name, s32 score, int car, char *names, s32 *scores, char *cars)
{
    int i, j;
    for (i = 0; i < 8; i++)
        if (scores[i] < score) break;                     /* hi signed, lo unsigned = signed long < */
    for (j = 6; j >= i; j--) {
        strncpy(names + (j + 1) * 20, names + j * 20, 20);
        strncpy(cars + (j + 1) * 20, cars + j * 20, 20);
        scores[j + 1] = scores[j];
    }
    strncpy(names + i * 20, name, 20);
    strncpy(cars + i * 20, flow_car_name(car), 20);
    scores[i] = score;
}
```

Update the prototypes: `grep -n 'scores_enter_name' tdport/src/game/*.h` and
replace that declaration with:
```c
void scores_get_name(s32 score, int car, char *name);
void scores_insert(const char *name, s32 score, int car, char *names, s32 *scores, char *cars);
```
(`grep -rn scores_enter_name tdport/src` must then return nothing.) If
`high_scores()` is defined above the two new functions and there is no header
declaration, add the two prototypes near the top of `flow_scores.c` instead.

- [ ] **Step 2: Build**

Run: `cd ~/sbbs/src/doors/syncdrive && ./build.sh && ./build/syncdrive --check --game-dir=game`
Expected: builds, tests pass, loader ok.

- [ ] **Step 3: Exercise the score path headlessly**

A qualifying score needs a finished stage, so this uses a SCORES file whose
eighth entry is 0: any completed or crashed stage then qualifies. From the
game's `SCORES` format (FORMATS.md "SCORES"), the zip's own SCORES can be
edited so all eight scores are 0; or delete it (the port starts from an
empty table, `scores_load()` "PORT:" branch).

```bash
cd ~/sbbs/src/doors/syncdrive/game
rm -rf /tmp/sdscores && mkdir /tmp/sdscores
printf '1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\n1\r\nIn\r\nMyDoorAlias\r\n' > /tmp/sdscores/door32.sys
SYNCDRIVE_SIXELOUT=/tmp/sdscore.six \
SYNCDRIVE_KEYS='<Task 5 script to car select>,<keys that start a stage and crash or quit it>,wait:8000,quit' \
timeout 120 ../build/syncdrive /tmp/sdscores/door32.sys --game-dir=. --data-dir=/tmp/sdscores
cat /tmp/sdscores/SCORES
```
Expected: SCORES contains a line starting `MyDoorAlias` (padded to 20) with
the car name, and the captured frames show the alias in the name box. The
DOOR32.SYS lines above are placeholders except line 7 (the alias); if
`termgfx_door32_read()` rejects the file, write the full 11-line DOOR32.SYS
layout from `termgfx/door32.h`'s header comment instead. Work out the
stage-starting keys from the frames, as in Task 5 Step 7. If a stage cannot
be finished headlessly, record that and leave this check to the live test
(Task 8).

- [ ] **Step 4: Record the patch**

Replace "None yet." in `PROVENANCE.md` with:
```markdown
1. `src/game/flow_scores.c`: `scores_enter_name()` split into
   `scores_get_name()` and `scores_insert()`. With a BBS alias
   (`host_player_name()`), the name box shows the alias, which cannot be
   edited, and waits for a key or 3 seconds. `high_scores()` takes the name
   first, then re-reads SCORES, inserts if the score still qualifies, and
   saves, all under `host_scores_lock()`, so concurrent sessions do not
   overwrite each other. Without an alias the original editor is used.
```

---

### Task 7: Package: xtrn/syncdrive, deploy.js, README

**Files:**
- Create: `xtrn/syncdrive/install-xtrn.ini`, `xtrn/syncdrive/getdata.js`,
  `xtrn/syncdrive/README.md`, `xtrn/syncdrive/syncdrive.example.ini`,
  `src/doors/syncdrive/deploy.js`, `src/doors/syncdrive/README.md`

**Interfaces:**
- Consumes: `syncdrive --check --game-dir=DIR` exit status (0 = ok);
  `door_deploy()` from `exec/load/door_deploy.js`.

- [ ] **Step 1: deploy.js**

`src/doors/syncdrive/deploy.js`:
```js
// deploy.js -- install the freshly-built syncdrive binary into the door dir.
//
//     jsexec src/doors/syncdrive/deploy.js
//
// xtrn.ini launches the binary directly (see xtrn/syncdrive/install-xtrn.ini),
// so it must be FLAT in the door dir. SpiderMonkey 1.8.5.
//
// Copyright(C) 2026 Rob Swindell. GPL-2.0.

load("door_deploy.js");

exit(door_deploy({
	name:   "syncdrive",
	srcdir: js.exec_dir,
	xtrn:   "syncdrive",
	subdir: false,
	direct_launch: true
}));
```

- [ ] **Step 2: install-xtrn.ini**

`xtrn/syncdrive/install-xtrn.ini`:
```ini
; Test Drive (syncdrive) installer data for install-xtrn.js
;   jsexec install-xtrn ../xtrn/syncdrive

Name: Test Drive
Desc: Accolade's 1987 sports-car racing classic, as a graphical BBS door. Requires a sixel-capable graphics terminal (e.g. SyncTERM).
By:   Rob Swindell / Accolade (original game)
Cats: Games
Subs: Racing, Driving
Inst: 2026/09/18

; The door: a native binary with raw socket I/O, fed the DOOR32.SYS drop file
; (%f). The high-score table lives in data/syncdrive/ (%j = the data dir),
; shared by every node. startup_dir is left unset, so the door runs from this
; directory, where the game data is installed.
[prog:SYNCDRIVE]
name          = Test Drive
cmd           = syncdrive%. %f --data-dir=%jsyncdrive/
type          = XTRN_DOOR32
settings      = XTRN_NATIVE | XTRN_BIN | XTRN_MULTIUSER | XTRN_NODISPLAY
execution_ars = ANSI
note          = Install the Test Drive door (native, DOOR32.SYS, shared high scores).

[copy:syncdrive.example.ini]
dest = syncdrive.ini

; The original Test Drive data is commercial content and is NOT shipped with
; this door. Drop your copy (the zip, the loose files, or a game folder) into
; this directory; getdata.js installs the files it needs. Re-runnable:
;     jsexec ../xtrn/syncdrive/getdata.js
[exec:getdata.js]
note     = Test Drive needs its original DOS game files (you supply them -- not shipped).
prompt   = Install the Test Drive game data now from a copy placed in the syncdrive door directory?
required = false
```

- [ ] **Step 3: syncdrive.example.ini**

`xtrn/syncdrive/syncdrive.example.ini`:
```ini
; syncdrive.ini -- Test Drive door settings. Copied from syncdrive.example.ini
; by the installer; edit the copy.

; Largest sixel image to send, in pixels (0 = let the terminal decide).
;sixel_max = 0

[input]
; auto: drive with held keys when the terminal reports key releases (SyncTERM,
;       kitty), else the original key-repeat behavior.
; off:  always the original key-repeat behavior.
held_keys = auto

[game]
; The drawing rate of the 1987 PC while driving. Frame-counted behavior (the
; gear-shift panel, traffic randomness, the crash animation) depends on it.
frame_rate = 8

[audio]
; enabled = true
; volume = 50
```

- [ ] **Step 4: getdata.js**

`xtrn/syncdrive/getdata.js`:
```js
// getdata.js -- install Test Drive (1987) game data for the syncdrive door
// from a copy the sysop placed in the door directory. Downloads nothing.
//
// Accepts: the loose files in the door dir, files in a subfolder (an
// extracted game folder), or an archive (.zip etc.) in the door dir. Copies
// TDEGA.EXE, CARS.TXT, TDSND.SND, *.PES, *.BIN, *.SS, verifies with
// `syncdrive --check`, and seeds data/syncdrive/SCORES from the copy's SCORES
// when there is no high-score table yet.
//
//     jsexec ../xtrn/syncdrive/getdata.js
//
// SpiderMonkey 1.8.5. Copyright (C) 2026 Rob Swindell. GPL-2.0.

var NAMED = ["tdega.exe", "cars.txt", "tdsnd.snd", "scores"];
var EXTS  = [".pes", ".bin", ".ss"];
var REQUIRED = ["tdega.exe", "cars.txt", "tdsnd.snd"];
var ARCHIVE_EXT = [".zip", ".7z", ".rar", ".arj", ".lzh", ".tar", ".tgz", ".tar.gz"];

function door_dir() { return backslash(js.exec_dir || js.startup_dir || "./"); }
function base_lc(p) { return String(file_getname(p)).toLowerCase(); }

function wanted(name)
{
	var lc = String(name).toLowerCase(), i;
	for (i = 0; i < NAMED.length; i++)
		if (lc == NAMED[i])
			return true;
	for (i = 0; i < EXTS.length; i++)
		if (lc.length > EXTS[i].length && lc.substr(lc.length - EXTS[i].length) == EXTS[i])
			return true;
	return false;
}

function is_archive(name)
{
	var lc = String(name).toLowerCase(), i;
	for (i = 0; i < ARCHIVE_EXT.length; i++)
		if (lc.length > ARCHIVE_EXT[i].length
		    && lc.substr(lc.length - ARCHIVE_EXT[i].length) == ARCHIVE_EXT[i])
			return true;
	return false;
}

function present(dir)
{
	var have = {}, all = directory(dir + "*"), i;
	for (i = 0; i < all.length; i++)
		if (all[i].charAt(all[i].length - 1) != "/" && wanted(file_getname(all[i])))
			have[base_lc(all[i])] = true;
	return have;
}

function copy_from_tree(dir, sub, depth, have)
{
	var all, i, e, b;
	if (depth < 0)
		return;
	all = directory(sub + "*");
	for (i = 0; i < all.length; i++) {
		e = all[i];
		if (e.charAt(e.length - 1) == "/") {
			copy_from_tree(dir, e, depth - 1, have);
			continue;
		}
		b = file_getname(e);
		if (!wanted(b) || have[b.toLowerCase()])
			continue;
		if (file_copy(e, dir + b.toUpperCase())) {
			have[b.toLowerCase()] = true;
			print("  copied " + b.toUpperCase());
		}
	}
}

function extract_archive(path, dir, have)
{
	var ar, list, i, name, b;
	try { ar = new Archive(path); list = ar.list(false); }
	catch (e) { return; }
	for (i = 0; i < list.length; i++) {
		name = list[i].name;
		if (!name || list[i].type == "directory")
			continue;
		b = file_getname(name);
		if (!wanted(b) || have[b.toLowerCase()])
			continue;
		try {
			ar.extract(dir, false, true, 0, name);
			if (file_getname(name) != b.toUpperCase() && file_exists(dir + b))
				file_rename(dir + b, dir + b.toUpperCase());
			have[b.toLowerCase()] = true;
			print("  extracted " + b.toUpperCase() + " (from " + file_getname(path) + ")");
		} catch (e2) {
			print("  ! failed to extract " + name + ": " + e2);
		}
	}
}

function main()
{
	var dir = door_dir(), have, top, i, missing = [], exe, rc, scores_dir, pes = 0, k;

	print("Test Drive (syncdrive) game-data installer");
	print("  door directory: " + dir);
	have = present(dir);
	top = directory(dir + "*");
	for (i = 0; i < top.length; i++) {
		if (top[i].charAt(top[i].length - 1) == "/")
			copy_from_tree(dir, top[i], 3, have);
		else if (is_archive(file_getname(top[i])))
			extract_archive(top[i], dir, have);
	}
	have = present(dir);
	for (i = 0; i < REQUIRED.length; i++)
		if (!have[REQUIRED[i]])
			missing.push(REQUIRED[i].toUpperCase());
	for (k in have)
		if (/\.pes$/.test(k))
			pes++;
	if (missing.length || pes == 0) {
		print("");
		print("No complete Test Drive (1987, DOS) copy found"
		    + (missing.length ? " (missing: " + missing.join(" ") + ")" : " (no .PES files)") + ".");
		print("Test Drive is commercial content and is NOT shipped with this door.");
		print("Put your copy (the zip, the loose files, or the game folder) in:");
		print("    " + dir);
		print("and re-run:  jsexec ../xtrn/syncdrive/getdata.js");
		return 1;
	}

	exe = dir + (system.platform.toLowerCase() == "win32" ? "syncdrive.exe" : "syncdrive");
	if (file_exists(exe)) {
		rc = system.exec(exe + " --check --game-dir=" + dir);
		if (rc != 0) {
			print("TDEGA.EXE is not the expected EGA build of Test Drive (1987); the door");
			print("cannot run it. Use the original DOS release's TDEGA.EXE.");
			return 1;
		}
	} else {
		print("  note: the syncdrive binary is not here yet (build, then run");
		print("  jsexec src/doors/syncdrive/deploy.js); skipping the TDEGA.EXE check.");
	}

	scores_dir = backslash(system.data_dir + "syncdrive");
	if (!file_exists(scores_dir + "SCORES") && file_exists(dir + "SCORES")) {
		mkpath(scores_dir);
		if (file_copy(dir + "SCORES", scores_dir + "SCORES"))
			print("  seeded the high-score table: " + scores_dir + "SCORES");
	}
	print("Success: Test Drive game data installed.");
	return 0;
}

if (typeof SYNCDRIVE_GETDATA_NO_MAIN == "undefined")
	exit(main());
```

- [ ] **Step 5: Try getdata.js against a scratch door dir**

```bash
d=$(mktemp -d)/syncdrive/ && mkdir -p "$d"
cp ~/sbbs/xtrn/syncdrive/getdata.js "$d"
cp ~/sbbs/src/doors/syncdrive/build/syncdrive "$d"
curl -sSL -o "$d/Test Drive.zip" 'https://archive.org/download/TestDrive_1987/Test%20Drive.zip'
/sbbs/exec/jsexec -n "$d/getdata.js"; echo "rc=$?"; ls "$d"
```
Expected: extracted files listed, `rc=0`, `Success`. Note: the seeding step
writes the live `data/syncdrive/SCORES` only if absent; to avoid touching the
live install here, run it before `data/syncdrive/` exists only if you intend
to seed it, else temporarily comment out the seeding and restore it. Run the
script a second time: it must again end in `Success` without re-copying.
Then `rm -rf` the scratch dir.

- [ ] **Step 6: README files**

`xtrn/syncdrive/README.md`: sysop-facing, describing behavior, not source
files (no C function names). Sections: what the door is; requirements
(sixel terminal, SyncTERM recommended); installing (`jsexec install-xtrn
../xtrn/syncdrive`, where to put the game data, that it is not shipped and
where the verified copy came from); controls table (arrows/keypad steer and
shift through the gear gate, A/Z shift, Esc quits a drive, Ctrl-P pause, F2
sound on/off, Ctrl-F fit to screen / true aspect, Ctrl-S stats, Ctrl-Q quit
the door); the shared high-score table in `data/syncdrive/SCORES` named by
BBS alias; `syncdrive.ini` settings. Plain ASCII punctuation.

`src/doors/syncdrive/README.md`: developer-facing: what the door is, a link to
DESIGN.md, PROVENANCE.md and CLAUDE.md, build and headless test commands
(same as CLAUDE.md), and the deploy command.

---

### Task 8: Live SyncTERM test, fixes, commit

This task needs the user: they deploy and play. Do not deploy into the live
install without their go-ahead (`jsexec deploy.js` and `install-xtrn` write
to `/sbbs`).

- [ ] **Step 1: Hand over the install commands**

Ask the user to run, from `~/sbbs/src/doors/syncdrive`:
```
jsexec deploy.js
jsexec install-xtrn ../xtrn/syncdrive
```
and to place the Test Drive zip in `xtrn/syncdrive/` before the installer's
data step (or re-run `jsexec ../xtrn/syncdrive/getdata.js` afterwards).

- [ ] **Step 2: Live checklist (with the user, SyncTERM 80x25)**

- Title and car select render; status line hidden; returned intact on exit.
- One full stage with legacy key-repeat input (as the door starts).
- Held-key driving takes over once SyncTERM reports releases: steering while a
  key is held, gear shifts with A/Z.
- Crash, police chase, stage results.
- High-score entry shows the alias in the box and cannot be edited; the entry
  appears in `data/syncdrive/SCORES`.
- F2 toggles sound; PC-speaker sound is clean (no clicks at note edges).
- Ctrl-F toggles 614x384 (bars) and 640x384 (fill); the user picks the default.
- Ctrl-S stats bar; Ctrl-P pause; Ctrl-Q quits back to the BBS prompt.
- Two nodes qualifying at once both land in the table.
- Any crash: get the stack trace with `coredumpctl info syncdrive` and fix
  the cause.

Record each result. Fix defects in our files (TDD: a failing unit test first
where the defect is in a pure module).

- [ ] **Step 3: Release notes check**

Check whether `docs/v322_new.md` lists new doors; if it does, add one
concise line for SyncDrive / Test Drive in the same style. If new doors are
not listed there, add nothing.

- [ ] **Step 4: Commit (milestone 2: playable, installable)**

```bash
cd ~/sbbs
git diff --cached --stat          # must be empty or only our paths
git add src/doors/syncdrive xtrn/syncdrive
cat > /tmp/syncdrive-msg2 <<'EOF'
syncdrive: shared high scores under the BBS alias; installable package

Split the high-score entry so the name box shows the caller's BBS alias
(not editable) and the table is re-read, merged and written under a lock
on data/syncdrive/SCORES.lck, so sessions that qualify at the same time do
not overwrite each other. Add xtrn/syncdrive: the installer data, a
getdata.js that installs a sysop-supplied copy of the original game files
and verifies TDEGA.EXE, and the sysop README.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
EOF
awk 'length > 78' /tmp/syncdrive-msg2
git commit -F /tmp/syncdrive-msg2
```
Adjust the message to what actually changed in Task 8's fixes. Do not push.

- [ ] **Step 5: Update memory**

Update `project_syncdrive_door.md` in the memory dir: status (committed,
live-tested), the fit decision from Ctrl-F, and any open items.
