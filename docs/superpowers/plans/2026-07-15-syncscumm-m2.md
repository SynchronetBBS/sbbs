# SyncSCUMM M2 Implementation Plan — terminal video via libtermgfx

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace M1's PPM-dump-only sink with a real terminal renderer — probed
sixel (JXL tier optional at the end), dirty rects, per-frame palette handling,
server-side cursor compositing, overlay quantization — ending in a live
SyncTERM checkpoint watching Beneath a Steel Sky's intro.

**Architecture:** Two-layer split copied from SyncConquer: `door/sst_io.{h,c}`
is a **pure C** terminal-session layer (fd resolution, probe/replies, tiers,
dirty rects, pacing, present) that never includes a ScummVM header and is unit-
testable standalone; `door/video_term.{h,cpp}` is a thin ScummVM
`GraphicsManager` subclass (extending M1's dump manager, so the M1 boot test
stays green) that forwards frame/palette/cursor/overlay state into `sst_io`.
libtermgfx supplies encoders/probe-parsers/pacing-primitives/geometry only —
the present loop is ours, modeled on `syncconquer/door/door_io.c`.

**Tech Stack:** libtermgfx (static, via a small umbrella CMake) + xpdev
(OBJECT lib wrapped into a static archive), ScummVM's configure/make (our libs
appended to the generated `build/config.mk`), POSIX.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-07-14-syncscumm-design.md`; M1 plan:
  `docs/superpowers/plans/2026-07-14-syncscumm-m1.md`. `DOOR=/home/rswindell/sbbs/src/doors/syncscumm`.
- **No new vendor-tree modifications.** The vendored `scummvm/` keeps exactly
  M1's two touches (configure case + symlink). All new code in `door/`,
  `test/`, `build.sh`, and `xtrn/syncscumm/`.
- **`test/boot_bass.sh` must pass after every task** (M1 regression gate).
- `door/*.cpp` implementing ScummVM APIs: ScummVM style (tabs). `door/sst_*.c`:
  sibling-door C style (tabs, `sst_` prefix on exports). No reserved-namespace
  identifiers in our names (guards end `_H_`).
- Palette: ScummVM hands **8-bit** RGB triplets. Do NOT copy SyncConquer's
  `scale_palette()` `<<2` (that converts 6-bit VGA; copying it here would blow
  out colors).
- Game surface is 320x200 CLUT8. Presented canvas defaults to 640x400
  (SyncTERM 80x25 after DECSSDT status-off); real geometry comes from the
  `termgfx_term_probe` reply.
- `termgfx_geom_fit(vw, vh, src_w, src_h, scale_max, &ew, &eh)`:
  **`scale_max` is an emitted-WIDTH PIXEL CAP** (use `SST_SCALE_MAX 2048`),
  not a scale factor — probe-verified 2026-07-15; passing a small factor
  yields a few-pixel-wide frame.
- Build bridge (probe-verified): umbrella CMake builds `libtermgfx.a` +
  `libxpdev_static.a` (xpdev is an OBJECT lib: wrap with
  `add_library(xpdev_static STATIC $<TARGET_OBJECTS:xpdev>)`); `build.sh`
  appends `LIBS += <abs>/libtermgfx.a <abs>/libxpdev_static.a -lpthread` to the
  **generated** `build/config.mk` after configure (ScummVM's link rule places
  `$(LIBS)` after objects — Makefile.common:132). termgfx alone does NOT link:
  `apc.c` needs xpdev's `safe_snprintf`.
- Git: direct on master; **path-scoped `git add` only; never
  reset --hard / checkout . / stash** (shared tree — see memory
  `no-destructive-git-in-shared-tree`); repo-wide `git status --short` before
  every commit; message lines <= 78 chars (awk-verify); trailer
  `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`; NO pushes.
- SDD scratch artifacts use the `syncscumm-m2-` name prefix
  (`.superpowers/sdd/` is shared across concurrent runs).

## Key upstream facts (verified against the vendored tree / termgfx source)

- `updateScreen()` is polled at up to ~100 Hz by SCUMM's `waitForTimer()`
  (`engines/scumm/scumm.cpp:2943`) — the no-change path must return fast.
- SCUMM fades push (often partial) `setPalette()` **every logical frame**
  (`engines/scumm/palette.cpp:911-933` → `:1772`): a dirty palette must force
  a full-frame emit with `emit_palette=1` (SyncConquer's rule, door_io.c:2829).
- `setShakePos` may stay a no-op (NullGraphicsManager precedent);
  `setFocusRectangle`/`clearFocusRectangle` already default to no-ops.
- Cursor: engines send CLUT8 + keycolor sprites via `setMouseCursor(buf, w, h,
  hotspotX, hotspotY, keycolor, dontScale, format=nullptr, mask=nullptr)`
  (`common/system.h:1518`); with all cursor `kFeature*` reported false, only
  the key-color path must work. The backend owns the cursor position
  (`warpMouse`; windowed.h precedent) — `DefaultEventManager` only mirrors it.
- Overlay: GUI supports CLUT8 in principle but no in-tree backend ships it;
  we advertise RGB565 like NullGraphicsManager (`null-graphics.h:80`) and
  quantize at present time. `gui_theme=builtin` needs zero theme files
  (`gui/ThemeEngine.cpp:2004-2010`; default `scummremastered` falls back:
  `gui/gui-manager.cpp:87-100`).
- `addSysArchivesToSearchSet(Common::SearchSet &s, int priority)`
  (`common/system.h:1854`): SearchMan calls it at priority -1
  (`common/archive.cpp:623-635`); a correct override removes `--extrapath`.
- Dirty-rect machinery is NOT in termgfx (no shared present loop exists yet);
  port SyncConquer's `dr_*` from `syncconquer/door/door_io.c:2386-2705`,
  adapted to 320x200 (partial bottom tile row: 200/16 = 12.5).
- Comm-type-0 stdio fallback (`door_io.c:1356`): with no DOOR32.SYS/`-s<fd>`,
  fd 1/0 are the terminal — a dev tty run just works. Terminal output must be
  **gated on** `isatty(1) || door32/-s present || SYNCSCUMM_SIXELOUT set`, so
  the headless boot test (stdout = pipe) never receives escape output.

---

### Task 1: Search-set override + boot-test hardening (kill --extrapath)

**Files:**
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` (add override)
- Modify: `src/doors/syncscumm/test/boot_bass.sh` (drop --extrapath; tidies)
- Modify: `src/doors/syncscumm/test/fetch_bass.sh` (curl -f)
- Modify: `src/doors/syncscumm/PROVENANCE.md` (prune-recipe addendum)

**Interfaces:**
- Produces: env contract `SYNCSCUMM_DATA=<dir>` — a directory added to
  ScummVM's search set (engine-data files live there). Every later task's
  run/test commands export it instead of passing `--extrapath`.

- [ ] **Step 1: Update the boot test first (this is the failing test)**

In `test/boot_bass.sh`, replace the run line and its lead-in:

```sh
DUMP=$(mktemp -d)
trap 'rm -rf "$DUMP" "$INI"' EXIT
INI=$(mktemp)
SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
SYNCSCUMM_DUMP="$DUMP" timeout 20 "$BIN" --path="$GAME" \
  -c "$INI" sky \
  > "$DUMP/boot.log" 2>&1 || true   # timeout kill (124/143) is the normal end
```

(Deliberate changes: `--extrapath` gone; `SYNCSCUMM_DATA` added; the trap is
installed right after the FIRST mktemp — with `INI` initially unset,
`rm -rf "$DUMP" ""` is harmless — closing the prior review's leak window.)

- [ ] **Step 2: Run it — must FAIL**

```bash
cd $DOOR && ./test/boot_bass.sh; echo "exit=$?"
```

Expected: `FAIL: only 0 frames dumped` (or ≤1), exit=1 — sky.cpt is no longer
findable without the extrapath, the sky engine errors out before rendering.

- [ ] **Step 3: Implement the override**

In `door/syncscumm.cpp`, add after the existing includes (keep
`FORBIDDEN_SYMBOL_EXCEPTION_getenv` — already present):

```cpp
#include "backends/fs/posix/posix-fs.h"
#include "common/fs.h"
```

Add to the class declaration:

```cpp
	void addSysArchivesToSearchSet(Common::SearchSet &s, int priority) override;
```

Add the implementation (before `main`):

```cpp
// Engine runtime data (sky.cpt, lure.dat, ...) and, later, GUI themes are
// found via the search set instead of --extrapath: SYNCSCUMM_DATA names the
// directory (the door install sets it; dev runs point it at
// scummvm/dists/engine-data). SearchMan invokes this at priority -1, so
// explicit game paths always win.
void OSystem_Synchronet::addSysArchivesToSearchSet(Common::SearchSet &s, int priority) {
	const char *data = getenv("SYNCSCUMM_DATA");
	if (data && *data)
		s.add("syncscumm-data", new Common::FSDirectory(data, 4), priority);
	// Last resort, matching the default OSystem behavior (cf. null.cpp).
	s.addDirectory(".", ".", priority - 1);
}
```

- [ ] **Step 4: Rebuild, run the test — must PASS**

```bash
cd $DOOR && ./build.sh 2>&1 | tail -2 && ./test/boot_bass.sh
```

Expected: `BOOT-BASS OK (<N> frames)`, N in the same ballpark as before (~165).

- [ ] **Step 5: Tidies riding along (review-mandated, brief-inherited)**

In `test/fetch_bass.sh`: change `curl -sL -o` to `curl -sfL -o` (fail fast on
HTTP errors instead of relying on the checksum mismatch).

In `PROVENANCE.md`, extend the re-add recipe paragraph with one sentence:

```markdown
After pruning, remove now-empty directories:
`find dists/engine-data -mindepth 1 -type d -empty -delete`.
```

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door/syncscumm.cpp src/doors/syncscumm/test \
        src/doors/syncscumm/PROVENANCE.md
git commit -m "syncscumm: find engine data via the search set, not --extrapath" \
  -m "SYNCSCUMM_DATA names the engine-data directory; the backend's
addSysArchivesToSearchSet override (the M1 review's start-here item)
registers it, so runs no longer need --extrapath.  Rides along: curl -f
in the fixture fetcher, the boot test's cleanup trap installed after the
first mktemp, and the PROVENANCE prune recipe's empty-dir addendum." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 2: Build plumbing — termgfx + xpdev into the ScummVM link

**Files:**
- Create: `src/doors/syncscumm/door/CMakeLists.txt`
- Modify: `src/doors/syncscumm/build.sh`

**Interfaces:**
- Produces: `build/libs/termgfx/libtermgfx.a` and `build/libs/libxpdev_static.a`;
  the door binary links them (verify: `nm build/scummvm | grep sixel_encode`).
  Compile flags gain `-I$HERE/../termgfx`. Later tasks include termgfx headers
  from door C/C++ freely.

- [ ] **Step 1: Write the umbrella CMakeLists (probe-verified content)**

Create `src/doors/syncscumm/door/CMakeLists.txt`:

```cmake
# Umbrella build for the door's C libraries: libtermgfx (static) and xpdev
# (an OBJECT library upstream -- wrapped into a static archive so ScummVM's
# make can link it).  Consumed by build.sh; not part of ScummVM's build.
# termgfx's apc.c needs xpdev's safe_snprintf even on the video-only path.
cmake_minimum_required(VERSION 3.11)
project(syncscumm_libs C CXX)

set(WITHOUT_CRYPTO ON CACHE BOOL "" FORCE)
set(XP_CRYPTO_BACKEND "none" CACHE STRING "" FORCE)
foreach(_aud OSS SDL_AUDIO ALSA PORTAUDIO PULSEAUDIO PIPEWIRE COREAUDIO)
    set(WITHOUT_${_aud} ON CACHE BOOL "" FORCE)
endforeach()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../termgfx termgfx)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../xpdev xpdev EXCLUDE_FROM_ALL)
add_library(xpdev_static STATIC $<TARGET_OBJECTS:xpdev>)
```

- [ ] **Step 2: Extend build.sh**

Replace the body after `mkdir -p "$HERE/build"` with:

```sh
# Stage 1: the door's C libraries (termgfx + xpdev), via their own CMake.
cmake -S "$HERE/door" -B "$HERE/build/libs" -DCMAKE_BUILD_TYPE=Release
cmake --build "$HERE/build/libs" --target termgfx xpdev_static -j"$(nproc)"

# Stage 2: ScummVM with our backend.
cd "$HERE/build"
"$HERE/scummvm/configure" --backend="$BACKEND" --disable-all-engines \
  --enable-engine=scumm,sky,queen,lure,drascula --disable-detection-full
# Bridge our libraries into the generated build config: config.mk accumulates
# LIBS with +=, and Makefile.common's link rule places $(LIBS) after the
# objects, so appending here is ordering-correct and touches no vendor file.
{
	echo "LIBS += $HERE/build/libs/termgfx/libtermgfx.a"
	echo "LIBS += $HERE/build/libs/libxpdev_static.a -lpthread"
	echo "CXXFLAGS += -I$HERE/../termgfx"
} >> config.mk
# VER_REV= : the vendored tree lives inside the sbbs repo; without this,
# ScummVM's version probe appends the HOST repo's git state ("dirty").
make -j"$(nproc)" VER_REV=
ls -la "$HERE/build/scummvm"
```

(Preserve the existing header comment block and the `BACKEND` arg handling.)

- [ ] **Step 3: Build and verify linkage + regression**

```bash
cd $DOOR && rm -rf build && ./build.sh 2>&1 | tail -2
nm build/scummvm | grep -c " T sixel_encode\| T termgfx_apc_image"
./test/boot_bass.sh
```

Expected: link OK; nm count >= 1 (symbols present — note: until Task 4
references them the linker may drop unused archive members, in which case add
the check after Task 4 instead and record that here); `BOOT-BASS OK`.
If nm reports 0 at this task, that is NOT a failure — the acceptance for
Task 2 is: both archives build, configure+make succeed with the appended
config.mk lines, boot test green.

- [ ] **Step 4: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door/CMakeLists.txt src/doors/syncscumm/build.sh
git commit -m "syncscumm: link libtermgfx + xpdev into the ScummVM build" \
  -m "Stage-1 umbrella CMake builds libtermgfx.a and wraps xpdev's OBJECT
library into a static archive (apc.c needs xpdev's safe_snprintf even
video-only); build.sh appends both to the GENERATED build/config.mk
after configure -- LIBS lands after the objects in Makefile.common's
link rule, and no vendor file changes." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 3: sst_io — pure-C terminal session core (probe, replies, pacing, stats)

**Files:**
- Create: `src/doors/syncscumm/door/sst_io.h`
- Create: `src/doors/syncscumm/door/sst_io.c`
- Create: `src/doors/syncscumm/test/test_sst_io.c`
- Create: `src/doors/syncscumm/test/unit_sst_io.sh`
- Modify: `src/doors/syncscumm/door/module.mk` (add sst_io.o)

**Interfaces:**
- Consumes: termgfx `term.h` strings, `caps.h` parsers, `pace.h` primitives,
  `door32.h` drop-file parsing (all via `-I../termgfx` from Task 2).
- Produces (C API, callable from C++ via the header's extern "C" block):

```c
int  sst_io_init(int argc, char **argv);   /* fd resolution; 1 = terminal session active, 0 = headless */
void sst_io_shutdown(void);                /* leave strings + flush (no-op if headless) */
int  sst_io_active(void);                  /* terminal session running? */
void sst_io_pump(void);                    /* drain input: probe replies, DSR acks, hotkeys */
int  sst_io_quit_requested(void);          /* user pressed q / Ctrl-C */
/* present-side entry points added in Task 4 (sst_io_present) */
```

The activation rule (Global Constraints): a terminal session starts only when
a DOOR32.SYS path argv is given, or `-s<fd>`/`SYNCSCUMM_SOCK` supplies an fd,
or stdout `isatty()`, or `SYNCSCUMM_SIXELOUT` is set (file-capture mode).
Otherwise `sst_io_init` returns 0 and every other call is a no-op — which is
what keeps the M1 boot test byte-identical.

- [ ] **Step 1: Write the unit test first**

`test/test_sst_io.c` — drives the session over a socketpair, feeding canned
terminal replies and asserting probe emission + parser state:

```c
/* Unit test for sst_io: run the session against a socketpair, play the
 * terminal's side with canned replies, assert what the door emits and
 * what its parsers conclude.  cc'd + run by unit_sst_io.sh. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sst_io.h"

static int drain(int fd, char *buf, size_t cap)
{
	ssize_t n, total = 0;
	while ((n = recv(fd, buf + total, cap - 1 - total, MSG_DONTWAIT)) > 0)
		total += n;
	buf[total] = 0;
	return (int)total;
}

int main(void)
{
	int sv[2];
	static char out[262144];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	char fdarg[32];
	snprintf(fdarg, sizeof(fdarg), "-s%d", sv[1]);
	char *argv[] = { (char *)"test_sst_io", fdarg, NULL };

	assert(sst_io_init(2, argv) == 1);
	assert(sst_io_active() == 1);
	sst_io_flush();
	drain(sv[0], out, sizeof(out));
	/* probe burst: enter, status-off, term probe, DA1+CTDA, JXL query */
	assert(strstr(out, "\x1b[?25l"));                 /* cursor hidden (term_enter) */
	assert(strstr(out, "\x1b[c"));                     /* DA1 */
	assert(strstr(out, "SyncTERM:Q;JXL"));             /* JXL query */

	/* play a sixel-capable DA1 reply + CTDA, then a DSR ack */
	const char *replies = "\x1b[?63;4c" "\x1b[=67;84;101;114;109;1;330c" "\x1b[24;80R";
	assert(send(sv[0], replies, strlen(replies), 0) > 0);
	sst_io_pump();
	assert(sst_io_have_sixel() == 1);

	/* hotkeys: Ctrl-S toggles stats, q requests quit */
	assert(send(sv[0], "\x13", 1, 0) == 1);
	sst_io_pump();
	assert(sst_io_stats_visible() == 1);
	assert(send(sv[0], "q", 1, 0) == 1);
	sst_io_pump();
	assert(sst_io_quit_requested() == 1);

	sst_io_shutdown();
	drain(sv[0], out, sizeof(out));
	assert(strstr(out, "\x1b[?25h"));                  /* cursor restored (term_leave) */
	printf("SST_IO OK\n");
	return 0;
}
```

`test/unit_sst_io.sh` (755):

```sh
#!/bin/sh
# Build + run the sst_io unit test against the stage-1 libraries.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
[ -f "$DOOR/build/libs/termgfx/libtermgfx.a" ] || { echo "run ./build.sh first"; exit 1; }
cc -o /tmp/test_sst_io -I"$DOOR/door" -I"$DOOR/../termgfx" \
   "$HERE/test_sst_io.c" "$DOOR/door/sst_io.c" \
   "$DOOR/build/libs/termgfx/libtermgfx.a" "$DOOR/build/libs/libxpdev_static.a" -lpthread
/tmp/test_sst_io
```

- [ ] **Step 2: Run it — must fail to compile (sst_io.h absent)**

```bash
cd $DOOR && ./test/unit_sst_io.sh
```

Expected: `sst_io.h: No such file or directory`.

- [ ] **Step 3: Implement sst_io.h**

```c
/* sst_io.h -- SyncSCUMM's terminal-session layer (pure C, no ScummVM).
 * Modeled on syncconquer/door/door_io.c; termgfx supplies the encoders,
 * probe parsers, pacing primitives and control strings -- the session,
 * tier dispatch and present loop live here. */
#ifndef SYNCSCUMM_SST_IO_H_
#define SYNCSCUMM_SST_IO_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SST_FB_W 320
#define SST_FB_H 200

int  sst_io_init(int argc, char **argv);
void sst_io_shutdown(void);
int  sst_io_active(void);
void sst_io_pump(void);
void sst_io_flush(void);
int  sst_io_quit_requested(void);

/* parser-state probes (unit test + stats bar) */
int  sst_io_have_sixel(void);
int  sst_io_is_syncterm(void);
int  sst_io_jxl_supported(void);
int  sst_io_stats_visible(void);

/* Task 4 adds: void sst_io_present(const uint8_t *idx320x200, const uint8_t *pal768); */

#ifdef __cplusplus
}
#endif

#endif /* SYNCSCUMM_SST_IO_H_ */
```

- [ ] **Step 4: Implement sst_io.c**

Adapt from `syncconquer/door/door_io.c` — the specific pieces, with their
upstream line references so the implementer diffs against proven code:

```c
/* sst_io.c -- terminal session: fd resolution, probe burst, reply parsing,
 * hotkeys, pacing bookkeeping.  Present path arrives in Task 4. */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "sst_io.h"
#include "term.h"
#include "caps.h"
#include "pace.h"
#include "door32.h"

static int g_fd = -1, g_fd_in = -1;      /* -1 = headless (no session) */
static int g_active;
static int g_quit, g_stats;
static int g_have_sixel, g_is_syncterm, g_jxl, g_probe_replied;
static int g_cterm_ver;
static int g_canvas_w = 640, g_canvas_h = 400;   /* until the probe answers */
static FILE *g_capture;                  /* SYNCSCUMM_SIXELOUT mode */

/* ---- output staging (door_io.c:~1370 pattern) ---- */
static uint8_t g_out[1 << 18];
static size_t  g_out_len;

static void out_put(const void *p, size_t n)
{
	while (n) {
		size_t room = sizeof(g_out) - g_out_len;
		size_t take = n < room ? n : room;
		memcpy(g_out + g_out_len, p, take);
		g_out_len += take; p = (const uint8_t *)p + take; n -= take;
		if (g_out_len == sizeof(g_out))
			sst_io_flush();
	}
}
static void out_puts(const char *s) { out_put(s, strlen(s)); }

void sst_io_flush(void)
{
	size_t off = 0;
	if (g_fd < 0 || !g_out_len) { g_out_len = 0; return; }
	while (off < g_out_len) {
		ssize_t n = write(g_fd, g_out + off, g_out_len - off);
		if (n < 0) { if (errno == EAGAIN || errno == EINTR) continue; break; }
		off += (size_t)n;
	}
	g_out_len = 0;
}
```

fd resolution in `sst_io_init` (door_io.c:1297-1364 pattern, simplified):
walk argv for `-s<fd>` and for a path whose basename case-insensitively is
`door32.sys` (parse via `termgfx_door32_read()`; comm type 2 = socket handle);
else `SYNCSCUMM_SOCK` env; else if `SYNCSCUMM_SIXELOUT` is set open that file
into `g_capture` and use it as a frame sink with no input; else if
`isatty(1)` use fds 1/0; **else return 0 (headless)**. On a real fd: set
non-blocking input, `TCP_NODELAY` + larger `SO_SNDBUF` when it's a socket.
Then emit the probe burst verbatim (door_io.c:2738-2770 order, minus audio
and minus mouse — M3 owns input modes):

```c
	out_puts(termgfx_term_enter);
	out_puts(termgfx_term_status_off);
	out_puts(termgfx_term_probe);
	out_puts("\x1b[c\x1b[<c");
	out_puts(termgfx_query_jxl);
	sst_io_flush();
```

Reply parsing in `sst_io_pump()`: accumulate input into a ring; walk ESC
sequences; on CSI final `c` → DA1 (param 4 sets `g_have_sixel`) or CTDA
(`\x1b[=67;84;101;114;109;<maj>;<min>c` → `g_is_syncterm=1`,
`g_cterm_ver = maj*1000+min`); on final `R`/`n` → cursor-report/DSR ack (Task
4 wires the pace ring; for now set `g_probe_replied=1`); on final `t` →
`\x1b[4;<h>;<w>t` canvas-size report updates `g_canvas_w/h`; JXL reply via
`termgfx_caps_parse_jxl()` over the accumulated buffer sets `g_jxl`. Plain
bytes: `0x13` (Ctrl-S) toggles `g_stats`; `'q'`, `0x03` set `g_quit`.
Also run every accumulated byte through the JXL parser FIRST, since that
reply is a DSR-style `\x1b[=7;...n` — keep the accumulation buffer until
both DA1 and JXL answered (or 2s deadline), then shrink to a 256-byte tail.

`sst_io_shutdown()`: emit `termgfx_term_leave`, flush, close capture file.

- [ ] **Step 5: Add sst_io.o to module.mk (unguarded list, beside syncscumm.o)**

```make
MODULE_OBJS := \
	syncscumm.o \
	video_dump.o \
	sst_io.o
```

- [ ] **Step 6: Unit test green; full build + regression green**

```bash
cd $DOOR && ./test/unit_sst_io.sh && ./build.sh 2>&1 | tail -1 && ./test/boot_bass.sh
```

Expected: `SST_IO OK`, link line, `BOOT-BASS OK`.

- [ ] **Step 7: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door/sst_io.h src/doors/syncscumm/door/sst_io.c \
        src/doors/syncscumm/door/module.mk src/doors/syncscumm/test/test_sst_io.c \
        src/doors/syncscumm/test/unit_sst_io.sh
git commit -m "syncscumm: sst_io terminal-session core (probe, replies, hotkeys)" \
  -m "Pure-C session layer modeled on syncconquer's door_io.c: fd
resolution (DOOR32.SYS / -s<fd> / SYNCSCUMM_SOCK / tty / SIXELOUT
capture, else headless no-op), staged output, the canonical probe burst,
CSI reply parsing (DA1 sixel, CTDA, canvas size, JXL), Ctrl-S stats and
q/Ctrl-C quit.  Unit-tested over a socketpair with canned replies;
headless boot test unaffected by design (no tty, no session)." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 4: Present path — dirty-rect sixel + pacing + graphics manager

**Files:**
- Modify: `src/doors/syncscumm/door/sst_io.h` / `sst_io.c` (present API)
- Create: `src/doors/syncscumm/door/video_term.h`
- Create: `src/doors/syncscumm/door/video_term.cpp`
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` (instantiate; pump/quit in pollEvent)
- Modify: `src/doors/syncscumm/door/module.mk` (add video_term.o)
- Create: `src/doors/syncscumm/test/boot_sixel.sh`

**Interfaces:**
- Produces:
```c
void sst_io_present(const uint8_t *idx, const uint8_t *pal768); /* idx: SST_FB_W*SST_FB_H */
```
  and C++ `SyncscummTermGraphicsManager : public SyncscummDumpGraphicsManager`
  whose `updateScreen()` = inherited dump behavior + `sst_io_present()` when
  the session is active. `OSystem_Synchronet::pollEvent()` calls
  `sst_io_pump()` and converts `sst_io_quit_requested()` into
  `Common::EVENT_QUIT` (one-shot).

- [ ] **Step 1: Write the capture test first**

`test/boot_sixel.sh` (755) — SIXELOUT capture mode makes this fully headless:

```sh
#!/bin/sh
# M2 acceptance (offline half): boot BASS with the terminal path forced into
# file-capture mode; assert real sixel frames were emitted.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
DOOR=$(dirname "$HERE")
BIN="$DOOR/build/scummvm"
GAME="$HERE/games/bass"
[ -x "$BIN" ] || { echo "FAIL: $BIN not built"; exit 1; }
[ -f "$GAME/sky.dsk" ] || { echo "FAIL: run test/fetch_bass.sh first"; exit 1; }
CAP=$(mktemp -d)
trap 'rm -rf "$CAP"' EXIT
INI="$CAP/scummvm.ini"
SYNCSCUMM_DATA="$DOOR/scummvm/dists/engine-data" \
SYNCSCUMM_SIXELOUT="$CAP/frames.six" timeout 20 "$BIN" --path="$GAME" \
  -c "$INI" sky > "$CAP/boot.log" 2>&1 || true
[ -s "$CAP/frames.six" ] || { echo "FAIL: no sixel output"; exit 1; }
# DCS sixel introducer + raster attributes + palette definition + ST
grep -qa $'\x1bP' "$CAP/frames.six" || { echo "FAIL: no DCS introducer"; exit 1; }
grep -qa '"1;1;'   "$CAP/frames.six" || { echo "FAIL: no raster attributes"; exit 1; }
grep -qa '#0;2;'   "$CAP/frames.six" || { echo "FAIL: no palette registers"; exit 1; }
SIZE=$(wc -c < "$CAP/frames.six")
[ "$SIZE" -gt 10000 ] || { echo "FAIL: capture too small ($SIZE)"; exit 1; }
echo "BOOT-SIXEL OK ($SIZE bytes)"
```

- [ ] **Step 2: Run it — must FAIL (`no sixel output`)**

```bash
cd $DOOR && ./test/boot_sixel.sh; echo "exit=$?"
```

- [ ] **Step 3: Implement the present path in sst_io.c**

Port from `syncconquer/door/door_io.c`, adapted — the implementer should keep
that file open and diff-read each ported block:

Constants (SyncConquer values, resized):

```c
#define SST_TILE          16
#define SST_TX            (SST_FB_W / SST_TILE)                      /* 20 */
#define SST_TY            ((SST_FB_H + SST_TILE - 1) / SST_TILE)     /* 13: bottom row is 8px */
#define SST_MAX_BOXES     16
#define SST_MAX_COMPONENTS 128
#define SST_MERGE_GAP     2
#define SST_FALLBACK_PCT  45
#define SST_SCALE_MAX     2048
#define SST_PACE_DEADLINE_MS 750
#define SST_DSR_RING      16
#define SST_PROBE_GRACE_MS 500
#define SST_AUTO_DEPTH_MAX 8
```

Pieces, in port order:
1. `sst_now_ms()` — CLOCK_MONOTONIC.
2. Tile diff + coalesce: port `dr_diff_coalesce` (door_io.c:2498-2520) and
   `dr_coalesce` (door_io.c:2425-2492) with the SST_ constants. The bottom
   tile row is 8 px tall — the memcmp span for row `ty == SST_TY-1` is
   `SST_FB_H - ty*SST_TILE` rows.
3. Fit/center/scale: `termgfx_geom_fit(g_canvas_w, g_canvas_h, SST_FB_W,
   SST_FB_H, SST_SCALE_MAX, &ew, &eh)` then `termgfx_geom_gfx_clamp` +
   `termgfx_geom_center(g_canvas_w, g_canvas_h, ew, eh, 8, 16, &dx, &dy,
   &col, &row)`; nearest-neighbor scale of the CLUT8 buffer (door_io.c:
   1876-1889 pattern with SST_FB_W/H).
4. Full-frame emit: cursor-home to (row,col) via `\x1b[<row>;<col>H`,
   `sixel_encode(&buf, &cap, scaled, ew, eh, pal, emit_pal)`, out_put.
5. Dirty emit: port `door_dirty_sixel_present` (door_io.c:2553-2625): snap
   boxes to cell bounds (8x16), round box height up to 6-row sixel bands,
   per-box `sixel_encode` at the box size, cursor save/position/restore.
   Dirty path only when: same tier, have last frame, `!pal_dirty`, no
   geometry change (door_io.c:2906-2928 gating).
6. Palette: `pal_dirty = memcmp(last_pal, pal768, 768) != 0` → full frame +
   `emit_palette=1`. **No <<2 scaling** (8-bit input — Global Constraints).
7. Pacing: DSR ring + `termgfx_rtt_sample` + `termgfx_aimd_update`
   (door_io.c:2075-2121 port); per-frame `\x1b[6n` appended, `g_inflight++`;
   present returns early when `g_inflight >= depth` and deadline unexpired.
   In capture mode (`g_capture`), skip pacing and write each full frame
   (no dirty rects) to the file, overwriting per frame is NOT wanted here —
   append, the test asserts total size; cap capture at 200 frames.
8. Stats bar (Ctrl-S): port `door_stats_draw` (door_io.c:2326-2384)
   simplified to one row at the canvas bottom: tier, fps, KB/frame, depth,
   RTT — written as normal text with SGR colors, then cursor re-hidden.
9. `sst_io_present(idx, pal)` ties 1-8 together (door_io_present shape,
   door_io.c:2707+): first-call probe-grace wait, dedupe identical frames
   (memcmp) when `!pal_dirty`, choose full vs dirty, flush, pace bookkeeping.

- [ ] **Step 4: video_term.{h,cpp} + wiring**

`door/video_term.h`:

```cpp
/* Terminal-rendering graphics manager: extends the M1 dump manager (so
 * SYNCSCUMM_DUMP keeps working) and forwards each changed frame to the
 * sst_io present path when a terminal session is active. */
#ifndef SYNCSCUMM_VIDEO_TERM_H_
#define SYNCSCUMM_VIDEO_TERM_H_

#include "video_dump.h"

class SyncscummTermGraphicsManager : public SyncscummDumpGraphicsManager {
public:
	void updateScreen() override;
};

#endif /* SYNCSCUMM_VIDEO_TERM_H_ */
```

`door/video_term.cpp`:

```cpp
#include "common/scummsys.h"

#if defined(USE_SYNCHRONET_DRIVER)

#include "video_term.h"

extern "C" {
#include "sst_io.h"
}

void SyncscummTermGraphicsManager::updateScreen() {
	if (!_dirty)
		return;
	SyncscummDumpGraphicsManager::updateScreen();   /* clears _dirty, dumps PPM if enabled */
	if (sst_io_active() && _screen.getPixels())
		sst_io_present((const byte *)_screen.getPixels(), _palette);
}

#endif
```

(Requires `_dirty`, `_screen`, `_palette` to be `protected` in
`video_dump.h` — change `private:` to `protected:` there; that is the whole
video_dump.h diff for this task.)

In `door/syncscumm.cpp`:
- `main()`: call `sst_io_init(argc, argv)` before `scummvm_main`. ScummVM
  rejects argv options it doesn't know, so the door-only argv entries
  (`-s<fd>` and the DOOR32.SYS path) must not reach it: have `sst_io_init`
  record which argv indices it consumed (expose
  `int sst_io_consumed(int idx)` in sst_io.h), then build a filtered argv
  copy in `main()` containing every entry sst_io did NOT consume, and pass
  that to `scummvm_main`. Register `sst_io_shutdown` via `atexit()` so
  `quit()`'s `exit(0)` restores the terminal.
- `initBackend()`: `_graphicsManager = new SyncscummTermGraphicsManager();`
- `pollEvent()`: after the existing timer/mixer updates:

```cpp
	sst_io_pump();
	if (sst_io_quit_requested()) {
		static bool sentQuit = false;
		if (!sentQuit) {
			sentQuit = true;
			event.type = Common::EVENT_QUIT;
			return true;
		}
	}
	return false;
```

module.mk objects list gains `video_term.o`.

- [ ] **Step 5: Rebuild; capture test PASSES; regressions green**

```bash
cd $DOOR && ./build.sh 2>&1 | tail -1 && ./test/boot_sixel.sh && ./test/boot_bass.sh && ./test/unit_sst_io.sh
```

Expected: `BOOT-SIXEL OK (<big> bytes)`, `BOOT-BASS OK`, `SST_IO OK`.

- [ ] **Step 6: Eyeball on a real tty (implementer smoke, not the user gate)**

Run `SYNCSCUMM_DATA=$DOOR/scummvm/dists/engine-data $DOOR/build/scummvm
--path=$DOOR/test/games/bass -c /tmp/s.ini sky` inside any sixel terminal you
have; expect the intro to animate; q exits and restores the terminal. Record
what you saw (or that no sixel tty was available) in the report.

- [ ] **Step 7: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door src/doors/syncscumm/test/boot_sixel.sh
git commit -m "syncscumm: dirty-rect sixel present path + terminal manager" \
  -m "sst_io_present ports SyncConquer's proven present loop to 320x200:
tile diff + component coalescing, cell-snapped 6-row-banded per-box
sixel emission, palette-dirty full-frame fallback (SCUMM fades repaint
the palette every frame), DSR-ack AIMD pacing, Ctrl-S stats row.
SyncscummTermGraphicsManager extends the dump manager so SYNCSCUMM_DUMP
and the M1 boot test behave identically; quit flows back as EVENT_QUIT
and atexit restores the terminal.  boot_sixel.sh asserts real DCS
frames via the SYNCSCUMM_SIXELOUT capture sink." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 5: Cursor compositing

**Files:**
- Modify: `src/doors/syncscumm/door/video_term.h` / `video_term.cpp`
- Modify: `src/doors/syncscumm/door/syncscumm.cpp` (feature flags stay false — verify only)

**Interfaces:**
- Produces: `SyncscummTermGraphicsManager` overrides `setMouseCursor`,
  `setCursorPalette`, `showMouse`, `warpMouse`; presented frames have the
  cursor composited (game surface itself untouched). M3's mouse events will
  call the same `warpMouse` path.

- [ ] **Step 1: Extend video_term.h**

```cpp
class SyncscummTermGraphicsManager : public SyncscummDumpGraphicsManager {
public:
	SyncscummTermGraphicsManager();
	void updateScreen() override;
	void setMouseCursor(const void *buf, uint w, uint h, int hotspotX, int hotspotY,
	                    uint32 keycolor, bool dontScale = false,
	                    const Graphics::PixelFormat *format = NULL,
	                    const byte *mask = NULL) override;
	void setCursorPalette(const byte *colors, uint start, uint num) override;
	bool showMouse(bool visible) override;
	void warpMouse(int x, int y) override;

private:
	void compose();

	Graphics::Surface _composed;         /* game + cursor (+ overlay, Task 6) */
	byte _cursorPal[256 * 3];
	byte *_cursorBuf;
	uint _cursorW, _cursorH;
	int _cursorHotX, _cursorHotY;
	uint32 _cursorKey;
	bool _cursorVisible, _cursorUsePal;
	int _cursorX, _cursorY;
};
```

- [ ] **Step 2: Implement**

`setMouseCursor`: `format` must be null/CLUT8 (assert
`!format || format->isCLUT8()`); copy `buf` into an owned `_cursorBuf`
(realloc to w*h), store metrics + keycolor; ignore `mask` (all cursor
features report false, so engines won't send one). `setCursorPalette`:
store into `_cursorPal`, set `_cursorUsePal = true` — note
`kFeatureCursorPalette` stays FALSE so SCUMM-era engines use the game
palette; storing is future-proofing only, and `_cursorUsePal` is only set by
an engine that called it anyway. `showMouse`: swap+return old.
`warpMouse(x, y)`: clamp to 0..SST_FB_W/H-1, store.

`compose()`: copy `_screen` into `_composed` (create on first use, CLUT8
320x200), then if `_cursorVisible && _cursorBuf`, blit key-colored:

```cpp
	int ox = _cursorX - _cursorHotX, oy = _cursorY - _cursorHotY;
	for (uint cy = 0; cy < _cursorH; cy++) {
		int ty = oy + (int)cy;
		if (ty < 0 || ty >= SST_FB_H) continue;
		const byte *src = _cursorBuf + cy * _cursorW;
		byte *dst = (byte *)_composed.getBasePtr(0, ty);
		for (uint cx = 0; cx < _cursorW; cx++) {
			int tx = ox + (int)cx;
			if (tx < 0 || tx >= SST_FB_W) continue;
			if (src[cx] != (byte)_cursorKey)
				dst[tx] = src[cx];
		}
	}
```

`updateScreen()` now presents `_composed` instead of `_screen` — and must
also present when only the cursor moved: track
`_lastCursorX/_lastCursorY/_lastCursorShown` and treat a change as dirty.
(The sst_io diff machinery turns a cursor move into 1-2 small dirty tiles
automatically — no special-casing.)

- [ ] **Step 3: Verify**

```bash
cd $DOOR && ./build.sh 2>&1 | tail -1 && ./test/boot_sixel.sh && ./test/boot_bass.sh
```

Both green (BASS's intro shows a cursor only at the menu, so the capture test
is a no-crash/no-regression gate here; the visual check lands in Task 7's
live gate). Also assert the boot log stays free of cursor asserts:
`grep -ci assert "$CAP/boot.log"` manually if in doubt.

- [ ] **Step 4: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door
git commit -m "syncscumm: composite the game cursor server-side" \
  -m "Key-colored CLUT8 cursor blit into a composed presentation surface
(game surface untouched); position owned by the backend per the
windowed-manager convention, moved via warpMouse (M3's mouse events
reuse the same path).  Cursor motion flows through the ordinary dirty-
tile diff as 1-2 small rects." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 6: Overlay (GUI) rendering via quantization

**Files:**
- Create: `src/doors/syncscumm/door/sst_quant.h`
- Create: `src/doors/syncscumm/door/sst_quant.c`
- Create: `src/doors/syncscumm/test/test_sst_quant.c`
- Create: `src/doors/syncscumm/test/unit_sst_quant.sh`
- Modify: `src/doors/syncscumm/door/video_term.h` / `video_term.cpp`
- Modify: `src/doors/syncscumm/door/module.mk` (add sst_quant.o)

**Interfaces:**
- Produces:

```c
/* Median-cut quantize RGB888 -> 256-color indexed. out_idx: w*h bytes,
 * out_pal768: 256 RGB triplets. Deterministic. Returns color count used. */
int sst_quant_rgb(const uint8_t *rgb, int w, int h,
                  uint8_t *out_idx, uint8_t *out_pal768);
```

  and overlay support in the graphics manager: RGB565 overlay surface
  (`getOverlayFormat` like NullGraphicsManager), `copyRectToOverlay`/
  `grabOverlay`/`clearOverlay` real, `showOverlay/hideOverlay` real; when the
  overlay is visible, `compose()` produces an RGB888 composite (game under,
  overlay over) that is quantized and presented (palette marked dirty each
  such frame).

- [ ] **Step 1: Quantizer unit test first**

`test/test_sst_quant.c`:

```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sst_quant.h"

int main(void)
{
	enum { W = 64, H = 64 };
	static uint8_t rgb[W * H * 3], idx[W * H], pal[768];
	int x, y, n;
	/* 4-band synthetic image: exact colors must survive quantization */
	for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
		uint8_t *p = rgb + (y * W + x) * 3;
		switch ((y / 16) & 3) {
		case 0: p[0] = 255; p[1] = 0;   p[2] = 0;   break;
		case 1: p[0] = 0;   p[1] = 255; p[2] = 0;   break;
		case 2: p[0] = 0;   p[1] = 0;   p[2] = 255; break;
		case 3: p[0] = 255; p[1] = 255; p[2] = 255; break;
		}
	}
	n = sst_quant_rgb(rgb, W, H, idx, pal);
	assert(n >= 4 && n <= 256);
	/* every pixel must round-trip exactly (only 4 distinct colors) */
	for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
		const uint8_t *want = rgb + (y * W + x) * 3;
		const uint8_t *got = pal + idx[y * W + x] * 3;
		assert(memcmp(want, got, 3) == 0);
	}
	/* stress: 10k random colors must not crash and must fill <= 256 */
	srand(42);
	for (x = 0; x < W * H * 3; x++) rgb[x] = (uint8_t)rand();
	n = sst_quant_rgb(rgb, W, H, idx, pal);
	assert(n > 0 && n <= 256);
	printf("SST_QUANT OK (%d colors)\n", n);
	return 0;
}
```

`test/unit_sst_quant.sh` (755): same shape as unit_sst_io.sh but compiling
`test_sst_quant.c sst_quant.c` only (no libs needed).

- [ ] **Step 2: Run — fails to compile (header absent)**

- [ ] **Step 3: Implement sst_quant.{h,c}**

Median-cut, deterministic, self-contained (~150 lines): build a histogram of
15-bit RGB (5:5:5 buckets); if distinct buckets <= 256, emit them directly
(exact round-trip — what the unit test's first half asserts); else classic
median-cut over the bucket list to 256 boxes, palette = per-box weighted
mean, index via nearest-palette lookup table over the 15-bit space.

- [ ] **Step 4: Overlay in the graphics manager**

video_term.h additions (move the `extern "C" { #include "sst_io.h" }` block
from video_term.cpp up into this header — the member arrays below need
`SST_FB_W`/`SST_FB_H`):

```cpp
	Graphics::PixelFormat getOverlayFormat() const override { return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0); }
	void showOverlay(bool inGUI) override;
	void hideOverlay() override;
	void clearOverlay() override;
	void grabOverlay(Graphics::Surface &surface) const override;
	void copyRectToOverlay(const void *buf, int pitch, int x, int y, int w, int h) override;
	int16 getOverlayHeight() const override { return SST_FB_H; }
	int16 getOverlayWidth() const override { return SST_FB_W; }
private:
	Graphics::Surface _overlay;          /* RGB565, 320x200 */
	byte _quantIdx[SST_FB_W * SST_FB_H];
	byte _quantPal[768];
```

Implementation: `_overlay.create(SST_FB_W, SST_FB_H, getOverlayFormat())` on
first use; copyRect bounds-asserted like copyRectToScreen; show/hide set
`_overlayVisible` (base member) + mark dirty. In `compose()`: when
`_overlayVisible`, build RGB888 (expand game CLUT8 through `_palette`, then
overwrite with overlay pixels — RGB565 expanded; the GUI draws opaque), blit
cursor in RGB, `sst_quant_rgb()` into `_quantIdx/_quantPal`, and present
those with the palette always marked dirty (quantized palettes differ frame
to frame). When not visible, the CLUT8 path from Tasks 4-5 runs unchanged.

- [ ] **Step 5: Verify**

```bash
cd $DOOR && ./test/unit_sst_quant.sh && ./build.sh 2>&1 | tail -1 && ./test/boot_sixel.sh && ./test/boot_bass.sh
```

All green. (The GUI only appears on F5/menus — input lands in M3 — so the
overlay path's live exercise is the Task 7 checkpoint's `--gui-launcher`-less
scope; the unit test + no-regression is the gate here.)

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door src/doors/syncscumm/test/test_sst_quant.c \
        src/doors/syncscumm/test/unit_sst_quant.sh
git commit -m "syncscumm: RGB565 overlay rendered through median-cut quantization" \
  -m "The GUI overlay (F5 dialogs, error prompts) draws into an RGB565
surface like the null backend advertises; when visible, the composed
game+overlay+cursor frame is quantized to 256 colors (exact passthrough
when <=256 distinct 15-bit buckets, median-cut otherwise) and presented
with the palette marked dirty.  Game-only frames keep the native CLUT8
path untouched." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

---

### Task 7: JXL tier + minimal BBS entry + live SyncTERM checkpoint

**Files:**
- Modify: `src/doors/syncscumm/door/CMakeLists.txt` (libjxl find, WITH_JXL)
- Modify: `src/doors/syncscumm/door/sst_io.c` (tier select + JXL emit)
- Create: `xtrn/syncscumm/install-xtrn.ini`
- Modify: `src/doors/syncscumm/DESIGN.md` (status line -> M2)

**Interfaces:**
- Produces: automatic tier selection `JXL > sixel` (JXL only when the probe
  answered AND the build had libjxl); a BBS test entry so the user's SyncTERM
  session can run the door. Live checkpoint ends the milestone.

- [ ] **Step 1: CMake JXL block (adapted from syncconquer/CMakeLists.txt:157-231, non-MSVC arm only)**

Append to `door/CMakeLists.txt`:

```cmake
option(WITHOUT_JPEG_XL "Build without the JPEG XL graphics tier (sixel only)" OFF)
if(NOT WITHOUT_JPEG_XL)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(JPEG_XL libjxl)
    endif()
    if(NOT JPEG_XL_FOUND)
        find_library(JPEG_XL_LIBRARIES NAMES jxl)
        if(JPEG_XL_LIBRARIES)
            set(JPEG_XL_FOUND TRUE)
        endif()
    endif()
    if(JPEG_XL_FOUND)
        target_compile_definitions(termgfx PRIVATE WITH_JXL)
        target_include_directories(termgfx PRIVATE ${JPEG_XL_INCLUDE_DIRS})
        # Recorded for build.sh to append to ScummVM's LIBS:
        file(WRITE ${CMAKE_BINARY_DIR}/jxl_libs.txt "${JPEG_XL_LIBRARIES}")
    endif()
endif()
```

And in `build.sh`, after the existing LIBS appends:

```sh
if [ -f "$HERE/build/libs/jxl_libs.txt" ]; then
	for l in $(cat "$HERE/build/libs/jxl_libs.txt" | tr ';' ' '); do
		echo "LIBS += -l$l"
	done >> config.mk
fi
```

- [ ] **Step 2: Tier + JXL emit in sst_io.c**

Tier function (sa_auto_tier shape, no PPM/HALF — spec forbids text tiers):

```c
static int sst_tier(void)
{
#ifdef WITH_JXL
	if (g_jxl) return 1;      /* SST_TIER_JXL */
#endif
	return 0;                  /* SST_TIER_SIXEL (also the pre-reply default) */
}
```

NOTE: sst_io.c is compiled by ScummVM's make, which does NOT define WITH_JXL —
gate on a runtime flag instead: `int g_jxl_built = termgfx_jxl_encode(NULL,
NULL, NULL, 0, 0, 0, 0) != (size_t)-1;` — **check jxl.h first**: the stub
returns 0; a real build with zero dims also returns 0, so instead add
`sst_io_set_jxl_built(int)` called from... this is the one place the plan
does NOT hand you a verified recipe. Acceptable options (pick one, document
in the report): (a) also append `CXXFLAGS += -DWITH_JXL` to config.mk from
build.sh when jxl_libs.txt exists (defines it for door objects too — cleanest);
(b) runtime probe encoding a 1x1 pixel. Option (a) is recommended.

JXL emit path: full frames and dirty boxes both via RGB888 expansion +
`termgfx_jxl_encode(&jbuf, &jcap, rgb, w, h, 2.0f, 1)` +
`termgfx_apc_image(&abuf, &acap, "syncscumm_dr.jxl", "DrawJXL", jbuf, jn,
dx+rx, dy+ry, 1)` (blob mode; door_io.c:2658-2705 port). JXL path skips cell
snapping (pixel-exact).

- [ ] **Step 3: Verify headless**

```bash
cd $DOOR && ./build.sh 2>&1 | tail -1 && ./test/boot_sixel.sh && ./test/boot_bass.sh && ./test/unit_sst_io.sh
```

All green (capture mode pins the sixel path, so JXL is exercised at the live
checkpoint on SyncTERM, which answers the JXL probe).

- [ ] **Step 4: Minimal BBS test entry**

Create `xtrn/syncscumm/install-xtrn.ini` (modeled on xtrn/syncalert's — the
full multi-game installer is M5; this single entry is the live-test vehicle):

```ini
Name: SyncSCUMM
Desc: ScummVM point-and-click adventures (M2 test entry)
By:   Rob Swindell / ScummVM team
Cats: Games

[prog:SYNCSCUMM]
name          = SyncSCUMM (Beneath a Steel Sky)
cmd           = /home/rswindell/sbbs/src/doors/syncscumm/build/scummvm %f --path=/home/rswindell/sbbs/src/doors/syncscumm/test/games/bass -c %juser/%4/scumm/scummvm.ini sky
type          = XTRN_DOOR32
settings      = XTRN_NATIVE | XTRN_BIN | XTRN_MULTIUSER | XTRN_NODISPLAY
execution_ars = ANSI
```

Set `SYNCSCUMM_DATA` for the door via the ScummVM config instead of env (env
can't be set per-xtrn): simplest M2 route — sst_io/main already falls back to
`s.addDirectory(".", ...)`; pass the data dir explicitly by ALSO appending
`--extrapath=/home/rswindell/sbbs/src/doors/syncscumm/scummvm/dists/engine-data`
to the cmd line above (the search-set override remains the install-time
mechanism for M5; the test entry may use the explicit flag).
The implementer runs `jsexec install-xtrn ../xtrn/syncscumm` (answer prompts;
door lands in ctrl/xtrn.ini) and verifies the section appears. Note in the
report that `%juser/%4/scumm/` matches the spec's save-dir convention parent.

- [ ] **Step 5: DESIGN.md status**

Change the Status line to:
`Status: M2 complete (terminal video: sixel/JXL dirty-rect path) — pending live validation`

- [ ] **Step 6: Commit, then the LIVE CHECKPOINT (user gate — do not skip)**

```bash
cd /home/rswindell/sbbs && git status --short | grep -v "^??" | head -20
git add src/doors/syncscumm/door/CMakeLists.txt src/doors/syncscumm/build.sh \
        src/doors/syncscumm/door/sst_io.c src/doors/syncscumm/DESIGN.md \
        xtrn/syncscumm/install-xtrn.ini
git commit -m "syncscumm: JXL tier, BBS test entry, M2 status" \
  -m "Automatic JXL-over-sixel tier selection when the terminal answers the
probe and libjxl was found at build time (WITH_JXL reaches door objects
via the generated config.mk); pixel-exact JXL dirty boxes ride the APC
blob path.  xtrn/syncscumm/install-xtrn.ini is the minimal M2 live-test
entry (the real multi-game installer is M5)." \
  -m "Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>"
```

Then STOP and report to the controller/user: the live checkpoint is a human
connecting via SyncTERM, running the door, and confirming: intro animates,
colors correct (no double-brightness — the <<2 trap), fades smooth, Ctrl-S
stats row shows tier/fps/RTT, q exits cleanly with the terminal restored.
Any defect found becomes a fix commit on this task before M2 is called done.
