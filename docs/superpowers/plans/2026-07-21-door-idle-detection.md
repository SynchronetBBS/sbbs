# Graphical-door idle-user detection — Implementation Plan (phase 1)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give termgfx graphical doors a working idle-user timeout — a shared clock fed by real key events, a warn-then-exit behavior, and sysop configuration — proven end-to-end in one lobby door (syncretro) and one lobby-less door (syncconquer).

**Architecture:** A pure, dependency-free clock (`termgfx/idle.{h,c}`) is fed a monotonic millisecond stamp and told when genuine user input arrives. It never touches I/O, config or rendering. Each door calls `termgfx_idle_activity()` from its key/mouse dispatch — never from its terminal auto-reply path, because DSR pacing makes socket traffic worthless as a presence signal — polls once per present, paints its own warning, and exits through its existing time-limit path.

**Tech Stack:** C11, CMake, xpdev (`iniGetDuration`/`iniReadDuration`, `parse_duration`), Synchronet JS (SpiderMonkey 1.8.5), jsexec.

**Source spec:** `docs/superpowers/specs/2026-07-21-door-idle-detection-design.md`

## Global Constraints

- **Phase 1 scope is 4 tasks.** syncmoo1, syncdoom and syncduke follow in a phase-2 plan; syncscumm gets its own plan (it needs both a session-deadline concept and a transient-overlay primitive built in shared termio code).
- **Every door must be fully configurable with no Synchronet-side JS in the path.** `-i<seconds>` and the door's own `[idle]` ini section are the base contract, because these doors also run under other BBS software via DOOR32.SYS. A JS lobby is an additional Synchronet-only layer whose only added value is evaluating the exempt ARS. Any door change that works only when a lobby supplies `-i` is incomplete.
- **C must compile under GCC, Clang and MSVC.** MSVC is the most permissive; write for GCC/Clang first. No `goto` across an initialization, no reaching the end of a non-void function.
- **No reserved identifiers.** No leading `_` + uppercase, no `__` anywhere. Header guards use the trailing-underscore form (`TERMGFX_IDLE_H_`).
- **Run uncrustify as the closing step of every C change:** `uncrustify -c src/uncrustify.cfg --no-backup <files>`. Write tabs from the start; match each file's existing style.
- **JS must run under SpiderMonkey 1.8.5.** No `let`/`const`/arrow functions/template literals.
- **Commit messages wrap at 78 characters.** Write the message to a file and verify before committing — `awk 'length > 78 {print}' <file>` must print nothing — then `git commit -F <file>`.
- **Commit directly on master. Never `git push` without explicit approval.** The `~/sbbs` index is shared with the user: run `git diff --cached --stat` before staging and never `git commit --amend` there.
- **Never commit until a test run has actually exercised the change.** Static reasoning is not validation.
- **Never cite an unpushed commit by SHA** in a commit message or other durable text; refer to what it did.

**Verified build/test commands** (these were run against the current tree and work as written):

```bash
cmake -S src/doors/termgfx -B /tmp/tgtest -DTERMGFX_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/tgtest -j8
cd /tmp/tgtest && ctest --output-on-failure
```

---

### Task 1: The shared idle clock

The one piece every door depends on. Pure arithmetic over timestamps — no I/O, no config, no rendering — so it is fully unit-testable and both the doors that own their I/O loop and the ones behind `termgfx_termio` can use it unchanged.

**Files:**
- Create: `src/doors/termgfx/idle.h`
- Create: `src/doors/termgfx/idle.c`
- Create: `src/doors/termgfx/test/test_idle.c`
- Modify: `src/doors/termgfx/CMakeLists.txt:14-37` (the `add_library(termgfx STATIC ...)` source list)
- Modify: `src/doors/termgfx/test/CMakeLists.txt` (append a new test target)

**Interfaces:**
- Consumes: nothing.
- Produces: `termgfx_idle_t`, `termgfx_idle_state_t` (`TERMGFX_IDLE_ACTIVE`/`_WARN`/`_EXPIRED`), and
  `void termgfx_idle_init(termgfx_idle_t *idle, unsigned threshold_s, unsigned warn_s, uint32_t now_ms)`,
  `void termgfx_idle_activity(termgfx_idle_t *idle, uint32_t now_ms)`,
  `termgfx_idle_state_t termgfx_idle_poll(termgfx_idle_t *idle, uint32_t now_ms, unsigned *secs_left)`.

> **Note on the API shape.** The spec sketched these as file-static (no handle), mirroring `termgfx_termio`. This plan uses an explicit `termgfx_idle_t` instead, matching `termgfx_mouse_t` in `mouse.h` — the closer precedent for a small termgfx module, and the reason `test_mouse.c` can construct independent instances per assertion. Functionally identical for the single-instance doors.

- [ ] **Step 1: Write the failing test**

Create `src/doors/termgfx/test/test_idle.c`:

```c
#include <assert.h>
#include <stdint.h>
#include "idle.h"

int main(void) {
	termgfx_idle_t i;
	unsigned       left;

	/* threshold 0 disables the feature outright: however long we wait, the
	 * verdict stays ACTIVE. This is the "sysop did not configure it" path and
	 * the exempt-user path, so it must never warn. */
	termgfx_idle_init(&i, 0, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u + 3600000u, &left) == TERMGFX_IDLE_ACTIVE);

	/* 900s threshold with a 60s countdown: ACTIVE right up to 840s. */
	termgfx_idle_init(&i, 900, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 839000u, &left) == TERMGFX_IDLE_ACTIVE);

	/* At exactly threshold-warn the countdown opens, showing the full 60s. */
	assert(termgfx_idle_poll(&i, 1000u + 840000u, &left) == TERMGFX_IDLE_WARN);
	assert(left == 60);

	/* ...and counts down as the grace runs out. */
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_WARN);
	assert(left == 20);

	/* At the threshold it expires, and stays expired. */
	assert(termgfx_idle_poll(&i, 1000u + 900000u, &left) == TERMGFX_IDLE_EXPIRED);
	assert(termgfx_idle_poll(&i, 1000u + 999999u, &left) == TERMGFX_IDLE_EXPIRED);

	/* A keypress during the warning clears it and restarts the whole clock --
	 * no penalty, and the next warning is a full threshold away. */
	termgfx_idle_init(&i, 900, 60, 1000);
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_WARN);
	termgfx_idle_activity(&i, 1000u + 880000u);
	assert(termgfx_idle_poll(&i, 1000u + 880000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 1719000u, &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, 1000u + 1720000u, &left) == TERMGFX_IDLE_WARN);

	/* A misconfigured warn > timeout clamps: warn immediately, still expire at
	 * the threshold. It must not underflow into never-firing. */
	termgfx_idle_init(&i, 60, 600, 1000);
	assert(termgfx_idle_poll(&i, 1001u, &left) == TERMGFX_IDLE_WARN);
	assert(termgfx_idle_poll(&i, 1000u + 60000u, &left) == TERMGFX_IDLE_EXPIRED);

	/* The ms clock wrapping past UINT32_MAX must not read as a huge elapsed and
	 * expire a present user. Same int32_t-difference idiom the doors' existing
	 * deadline checks use. */
	termgfx_idle_init(&i, 900, 60, 0xFFFFF000u);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 1000u), &left) == TERMGFX_IDLE_ACTIVE);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 840000u), &left) == TERMGFX_IDLE_WARN);
	assert(termgfx_idle_poll(&i, (uint32_t)(0xFFFFF000u + 900000u), &left) == TERMGFX_IDLE_EXPIRED);

	return 0;
}
```

Append to `src/doors/termgfx/test/CMakeLists.txt`:

```cmake

# Unit test for idle.c -- the pure idle-user clock. Compiled as a bare SOURCE
# and linked against nothing, like test_chunk: the module is arithmetic over
# timestamps, with no I/O and no dependencies at all.
add_executable(test_idle
    ${CMAKE_CURRENT_SOURCE_DIR}/../idle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/test_idle.c)
target_include_directories(test_idle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
set_target_properties(test_idle PROPERTIES C_STANDARD 11)
target_compile_options(test_idle PRIVATE -UNDEBUG)
add_test(NAME idle COMMAND test_idle)
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake -S src/doors/termgfx -B /tmp/tgtest -DTERMGFX_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/tgtest -j8
```

Expected: FAIL at configure or compile — `Cannot find source file: .../idle.c`, or `fatal error: idle.h: No such file or directory`.

- [ ] **Step 3: Write the minimal implementation**

Create `src/doors/termgfx/idle.h`:

```c
#ifndef TERMGFX_IDLE_H_
#define TERMGFX_IDLE_H_

#include <stdint.h>

/* idle.h -- the shared idle-USER clock for termgfx game doors.
 *
 * WHY THIS EXISTS AT ALL, given that the BBS already has an inactivity
 * timeout: Synchronet's max_inactivity is enforced in input_thread(), against
 * a counter reset by any successful socket read. Every termgfx door paces its
 * frame loop with DSR acks -- it emits a frame plus ESC[6n and the terminal
 * answers, unprompted, about ten times a second -- so that counter is reset
 * continuously for the whole session and the threshold is never reached. The
 * BBS setting reads as configured and does nothing.
 *
 * So presence has to be judged on REAL INPUT, which only the door can tell
 * apart from its own pacing acks. Feed termgfx_idle_activity() from the key
 * and mouse dispatch ONLY -- never from the CSI path that handles the 'R'
 * pace-ack, 'c'/'S'/'t'/'u'/'y' capability replies, or the audio DSR ack, and
 * never from a synthetic key-up expiry timer, which fires for a user who has
 * already left.
 *
 * No I/O, no config parsing, no rendering, and no clock of its own: the caller
 * supplies a monotonic millisecond stamp. That keeps this usable both by the
 * doors that own their I/O loop and by those driven through termgfx_termio.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	TERMGFX_IDLE_ACTIVE = 0,   /* within the threshold                    */
	TERMGFX_IDLE_WARN,         /* inside the grace window; see secs_left  */
	TERMGFX_IDLE_EXPIRED       /* grace elapsed; the caller should exit   */
} termgfx_idle_state_t;

typedef struct {
	uint32_t threshold_ms;     /* 0 = feature disabled */
	uint32_t warn_ms;          /* clamped to threshold_ms at init */
	uint32_t last_ms;          /* stamp of the last real user input */
} termgfx_idle_t;

/* threshold_s == 0 disables: poll() then always returns ACTIVE. warn_s is
 * clamped to threshold_s, so a misconfigured warn > timeout degrades to "warn
 * immediately, exit at the threshold" rather than underflowing. */
void termgfx_idle_init(termgfx_idle_t *idle, unsigned threshold_s,
                       unsigned warn_s, uint32_t now_ms);

/* Call on GENUINE user input only. Resets the clock; no penalty for warning. */
void termgfx_idle_activity(termgfx_idle_t *idle, uint32_t now_ms);

/* Current verdict. *secs_left (may be NULL) is the countdown in whole seconds,
 * rounded up, and is meaningful only for TERMGFX_IDLE_WARN; it is 0 otherwise. */
termgfx_idle_state_t termgfx_idle_poll(termgfx_idle_t *idle, uint32_t now_ms,
                                       unsigned *secs_left);

#ifdef __cplusplus
}
#endif

#endif /* TERMGFX_IDLE_H_ */
```

Create `src/doors/termgfx/idle.c`:

```c
#include <stddef.h>

#include "idle.h"

void termgfx_idle_init(termgfx_idle_t *idle, unsigned threshold_s,
                       unsigned warn_s, uint32_t now_ms)
{
	if (idle == NULL)
		return;
	if (warn_s > threshold_s)
		warn_s = threshold_s;
	idle->threshold_ms = (uint32_t)threshold_s * 1000u;
	idle->warn_ms      = (uint32_t)warn_s * 1000u;
	idle->last_ms      = now_ms;
}

void termgfx_idle_activity(termgfx_idle_t *idle, uint32_t now_ms)
{
	if (idle != NULL)
		idle->last_ms = now_ms;
}

termgfx_idle_state_t termgfx_idle_poll(termgfx_idle_t *idle, uint32_t now_ms,
                                       unsigned *secs_left)
{
	int32_t  elapsed;
	uint32_t remain;

	if (secs_left != NULL)
		*secs_left = 0;
	if (idle == NULL || idle->threshold_ms == 0)
		return TERMGFX_IDLE_ACTIVE;

	/* Signed difference, so a wrapped millisecond clock reads as a small
	 * elapsed rather than ~4 billion. Same idiom as the doors' -t deadlines. */
	elapsed = (int32_t)(now_ms - idle->last_ms);
	if (elapsed < 0)
		elapsed = 0;

	if ((uint32_t)elapsed >= idle->threshold_ms)
		return TERMGFX_IDLE_EXPIRED;
	if ((uint32_t)elapsed < idle->threshold_ms - idle->warn_ms)
		return TERMGFX_IDLE_ACTIVE;

	remain = idle->threshold_ms - (uint32_t)elapsed;
	if (secs_left != NULL)
		*secs_left = (unsigned)((remain + 999u) / 1000u);
	return TERMGFX_IDLE_WARN;
}
```

In `src/doors/termgfx/CMakeLists.txt`, add `idle.c` to the `add_library(termgfx STATIC ...)` list, immediately after the `pace.c` line:

```cmake
    pace.c
    idle.c
```

- [ ] **Step 4: Run the tests to verify they pass**

```bash
cmake -S src/doors/termgfx -B /tmp/tgtest -DTERMGFX_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/tgtest -j8
cd /tmp/tgtest && ctest --output-on-failure
```

Expected: `3/3 Test #3: idle .... Passed`, and `100% tests passed, 0 tests failed out of 3` (`chunk` and `mouse` must still pass).

- [ ] **Step 5: Format and commit**

```bash
uncrustify -c src/uncrustify.cfg --no-backup src/doors/termgfx/idle.c src/doors/termgfx/idle.h src/doors/termgfx/test/test_idle.c
cd /home/rswindell/sbbs && git diff --cached --stat   # must be empty before staging
git add src/doors/termgfx/idle.c src/doors/termgfx/idle.h \
        src/doors/termgfx/test/test_idle.c \
        src/doors/termgfx/CMakeLists.txt src/doors/termgfx/test/CMakeLists.txt
```

Write the message to a file, verify width, then commit:

```bash
awk 'length > 78 { print "OVER:", length($0), $0 }' /tmp/msg.txt   # must print nothing
git commit -F /tmp/msg.txt
```

Message body should explain that socket traffic cannot signal presence in a DSR-paced door, so the clock is fed from real key dispatch instead.

---

### Task 2: `parse_duration()` default-unit argument

The lobby and the door read the **same** `[idle] timeout` ini key. xpdev's C `parse_duration()` treats a bare number as **seconds**; `game_lobby.js`'s treats it as **days**. Left alone, `timeout = 900` means 15 minutes to the door and 900 days to the lobby — a silent disable, the exact failure this feature exists to remove.

**Files:**
- Modify: `exec/load/game_lobby.js:525-544` (the `parse_duration()` comment block and function)
- Create: `xtrn/tests/game_lobby_test.js`

**Interfaces:**
- Consumes: nothing.
- Produces: `parse_duration(str, dflt_unit)` — `dflt_unit` is an optional one-letter unit (`"s"`, `"m"`, `"h"`, `"d"`, `"w"`, `"y"`) applied when `str` carries no suffix; omitted, it stays `"d"`. Returns whole seconds; 0 for blank/invalid/non-positive. Used by Task 3.

- [ ] **Step 1: Write the failing test**

Create `xtrn/tests/game_lobby_test.js`:

```js
// Unit test for exec/load/game_lobby.js parse_duration() -- pure, no network.
//
//     jsexec /home/rswindell/sbbs/xtrn/tests/game_lobby_test.js
//
// Alongside xtrn_mirror_test.js rather than under exec/tests/, matching the
// convention already set there.

var gl = load({}, "game_lobby.js");

var fail = 0;
function ok(cond, msg) { if (!cond) { fail++; print("FAIL: " + msg); } else print("ok: " + msg); }

/* The historical default is unchanged -- a bare number is DAYS. activity_max_age
   depends on this, so a regression here silently changes the activity feed. */
ok(gl.parse_duration("2") === 172800, "bare number still defaults to days");
ok(gl.parse_duration("0.5") === 43200, "fractional days");

/* Seconds as the explicit default: what the [idle] keys pass, so this parser
   and xpdev's parse_duration() agree on the same ini string. */
ok(gl.parse_duration("900", "s") === 900, "bare number with dflt_unit s");
ok(gl.parse_duration("2", "d") === 172800, "explicit dflt_unit d matches the old default");

/* An explicit suffix always wins over the default unit, in both directions. */
ok(gl.parse_duration("15m", "s") === 900, "suffix m overrides dflt_unit s");
ok(gl.parse_duration("60s", "d") === 60, "suffix s overrides dflt_unit d");
ok(gl.parse_duration("1h", "s") === 3600, "suffix h");
ok(gl.parse_duration("2w", "s") === 1209600, "suffix w");
ok(gl.parse_duration("1y", "s") === 31536000, "suffix y");

/* Blank/invalid/non-positive is 0 ("no limit") whatever the default unit. */
ok(gl.parse_duration("", "s") === 0, "empty string");
ok(gl.parse_duration("0", "s") === 0, "zero");
ok(gl.parse_duration("-5", "s") === 0, "negative");
ok(gl.parse_duration("abc", "s") === 0, "non-numeric");
ok(gl.parse_duration(undefined, "s") === 0, "undefined");

print(fail ? ("\n" + fail + " FAILURES") : "\nALL PASS");
exit(fail ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
/sbbs/exec/jsexec /home/rswindell/sbbs/xtrn/tests/game_lobby_test.js
```

Expected: FAIL — the `dflt_unit` cases report `FAIL: bare number with dflt_unit s` (the current function ignores a second argument and returns 900 days in seconds, `77760000`), and the script exits 1.

- [ ] **Step 3: Write the minimal implementation**

In `exec/load/game_lobby.js`, replace the comment block and function at lines 525-544 with:

```js
// Parse a duration string to SECONDS. A bare number takes `dflt_unit` (default
// "d" -- DAYS, fractions allowed, e.g. "0.5"); an optional letter suffix always
// overrides it: s, m(inutes), h(ours), d(ays), w(eeks), y(ears) -- e.g. "48h",
// "2w". Blank/invalid/<=0 -> 0 (which callers treat as "no limit"). Same suffix
// letters as xpdev's parse_duration.
//
// WHY dflt_unit EXISTS: xpdev's parse_duration() reads a bare number as
// SECONDS, and the doors use it (via iniGetDuration/iniReadDuration) on the
// very same [idle] ini keys this lobby reads. With a days default on this side
// only, "timeout = 900" would mean 15 minutes to the door and 900 days here --
// a silent disable. Duration keys shared with a door MUST pass "s"; the days
// default stays for activity_max_age, which predates this and is lobby-only.
function parse_duration(str, dflt_unit) {
	var n = parseFloat(str);
	if (isNaN(n) || n <= 0)
		return 0;
	var m = String(str).match(/[a-zA-Z]/);
	switch (m ? m[0].toLowerCase() : (dflt_unit || "d")) {
		case "s": return Math.round(n);
		case "m": return Math.round(n * 60);
		case "h": return Math.round(n * 3600);
		case "w": return Math.round(n * 604800);
		case "y": return Math.round(n * 31536000);
		default:  return Math.round(n * 86400);   // "d" or no suffix -> days
	}
}
```

- [ ] **Step 4: Run the test to verify it passes**

```bash
/sbbs/exec/jsexec /home/rswindell/sbbs/xtrn/tests/game_lobby_test.js
```

Expected: every line `ok: ...`, final line `ALL PASS`, exit status 0.

- [ ] **Step 5: Commit**

```bash
cd /home/rswindell/sbbs && git diff --cached --stat   # must be empty before staging
git add exec/load/game_lobby.js xtrn/tests/game_lobby_test.js
awk 'length > 78 { print "OVER:", length($0), $0 }' /tmp/msg.txt   # must print nothing
git commit -F /tmp/msg.txt
```

---

### Task 3: syncretro adoption (the lobby-door reference)

The door where the problem was observed, and the reference for the three doors that have a JS lobby. Delivers the first user-visible behavior.

> **Order note.** The spec suggested syncconquer first as the cheapest door. This plan does syncretro first instead: it is where the reported incident happened, and it exercises the lobby/ARS path that Task 2 just enabled. syncconquer follows as Task 4 and covers the lobby-less path.

**Files:**
- Modify: `src/doors/syncretro/syncretro_door.c` — `-i` arg parse near the `-t` handling at `:227`; idle state, activity entry point, and the EXPIRED check inside `sr_door_should_exit()` at `:112`
- Modify: `src/doors/syncretro/syncretro_input.c:341` — call the activity hook at the top of `sr_key_apply()`
- Modify: `src/doors/syncretro/syncretro_config.c:156` — read `[idle] timeout` and `[idle] warn`
- Modify: `src/doors/syncretro/syncretro.h` — declare the new door entry points
- Modify: `src/doors/syncretro/main.c:558` — call the per-frame idle tick beside `sr_io_stats_tick()`
- Modify: `exec/load/syncretro_lobby.js:295` — append `-i<seconds>` to the command line
- Modify: `xtrn/syncnes/syncretro.ini` — document the new `[idle]` section

**Interfaces:**
- Consumes: `termgfx_idle_t`, `termgfx_idle_init()`, `termgfx_idle_activity()`, `termgfx_idle_poll()`, `TERMGFX_IDLE_*` (Task 1); `parse_duration(str, "s")` (Task 2).
- Produces: `void sr_door_idle_activity(void)`, `void sr_door_idle_tick(void)` (declared in `syncretro.h`) — the pattern Task 4 and the phase-2 doors follow.

- [ ] **Step 1: Wire the clock into the door**

In `src/doors/syncretro/syncretro_door.c`, beside the existing deadline globals at `:41-42`, add:

```c
#include "idle.h"

static termgfx_idle_t g_idle;
static int            g_idle_arg = -1;   /* -i<seconds>; -1 = not given */
static int            g_idle_expired;
```

In `sr_door_resolve()`, beside the `-t` case at `:227`:

```c
		} else if (a[0] == '-' && a[1] == 'i' && a[2] != '\0' && all_digits(a + 2)) {
			g_idle_arg = atoi(a + 2);                            /* -i<seconds> */
```

Add the accessor and the two entry points:

```c
/* The idle threshold actually in force: -i wins when given (from a lobby it is
 * the ARS-aware answer, and -i0 is how an exempt user is positively excused),
 * otherwise the door's own ini. Mirrors how -t beats the drop file.
 *
 * The ini fallback is NOT a Synchronet-only nicety: this door also runs under
 * other BBS software via DOOR32.SYS, where no lobby and no ARS exist, and the
 * sysop configures the timeout entirely through syncretro.ini or a -i on the
 * static command line. Both paths must work with no JS involved. */
static unsigned sr_door_idle_threshold(void)
{
	return (g_idle_arg >= 0) ? (unsigned)g_idle_arg : sr_config_idle_timeout();
}

void sr_door_idle_activity(void)
{
	termgfx_idle_activity(&g_idle, sr_door_now_ms());
}

/* Once per frame: paint the countdown while it runs, latch the expiry for
 * sr_door_should_exit(). Re-arming the toast each second is what keeps it on
 * screen -- sr_io_toast()'s dwell is a fixed SR_TOAST_MS. */
void sr_door_idle_tick(void)
{
	static unsigned last_shown = 0;
	unsigned        left = 0;
	char            msg[64];

	switch (termgfx_idle_poll(&g_idle, sr_door_now_ms(), &left)) {
		case TERMGFX_IDLE_WARN:
			if (left != last_shown) {
				snprintf(msg, sizeof msg,
				         "Still there? Press any key -- disconnecting in %u second%s",
				         left, (left == 1) ? "" : "s");
				sr_io_toast(msg);
				last_shown = left;
			}
			break;
		case TERMGFX_IDLE_EXPIRED:
			g_idle_expired = 1;
			break;
		default:
			last_shown = 0;
			break;
	}
}
```

Arm it where the `-t` deadline is armed at `:455`:

```c
	termgfx_idle_init(&g_idle, sr_door_idle_threshold(),
	                  sr_config_idle_warn(), sr_door_now_ms());
```

**Ordering requirement:** `sr_config_read_ini()` must have run before this
point, or the ini fallback reads 0 and a no-lobby install silently gets no
timeout. Verify the call order in `sr_config_apply()`
(`syncretro_config.c:340-342`) and, if the arming currently happens first, move
it after the config read rather than reordering the config load.

And add the expiry to `sr_door_should_exit()` at `:112`, after the existing deadline check:

```c
	if (g_idle_expired) {
		fprintf(stderr, "syncretro: idle timeout -- no user input; exiting\n");
		return 1;
	}
```

Declare both entry points in `src/doors/syncretro/syncretro.h`, beside the existing `sr_io_toast()` declaration at `:159`:

```c
/* Idle-user detection: activity is fed from the REAL key path only (never the
 * DSR pace-ack path), and the tick paints the countdown once per frame. */
void sr_door_idle_activity(void);
void sr_door_idle_tick(void);
```

- [ ] **Step 2: Hook real input, and read the config**

At the top of `sr_key_apply()` in `src/doors/syncretro/syncretro_input.c:341`, before the bind lookup:

```c
	/* A genuine keystroke -- the one thing DSR pacing cannot forge. Deliberately
	 * here and not in the CSI dispatcher, which also sees the terminal's own
	 * pace-acks and capability replies. syncretro has no mouse path to hook. */
	sr_door_idle_activity();
```

In `src/doors/syncretro/syncretro_config.c`, add the globals beside the other `[video]`/`[audio]` ones at `:72-80`:

```c
static unsigned g_idle_timeout;        /* [idle] timeout; 0 = disabled */
static unsigned g_idle_warn = 60;      /* [idle] warn */
```

Read them inside `sr_config_read_ini()` at `:156`, beside the other `iniGet*` calls:

```c
		g_idle_timeout    = (unsigned)iniGetDuration(ini, "idle", "timeout", 0);
		g_idle_warn       = (unsigned)iniGetDuration(ini, "idle", "warn", 60);
```

and expose them:

```c
unsigned sr_config_idle_timeout(void) { return g_idle_timeout; }
unsigned sr_config_idle_warn(void)    { return g_idle_warn; }
```

Declare both in `src/doors/syncretro/syncretro.h` next to the other config accessors.

Call the tick in `src/doors/syncretro/main.c`, beside `sr_io_stats_tick()` at `:558`:

```c
			sr_door_idle_tick();    /* idle countdown / expiry latch */
```

- [ ] **Step 3: Build and verify it compiles clean**

```bash
cd /home/rswindell/sbbs/src/doors/syncretro && ./build.sh
```

Expected: build succeeds with no new warnings. Note that the built binary is symlinked live — see Step 5 before testing.

- [ ] **Step 4: Pass `-i` from the lobby**

In `exec/load/syncretro_lobby.js`, before the `cmd = ...` assignment at `:295`, resolve the threshold:

```js
	/* Idle timeout: seconds, or 0 when disabled or the user is exempt. The ARS
	 * is evaluated HERE because the door has no scfg_t and no user record --
	 * and -i0 is passed explicitly rather than omitting the flag, so "exempt"
	 * positively overrides any [idle] timeout in the door's own ini. "s" is the
	 * default unit so this agrees with xpdev's parse_duration() reading the
	 * same key on the door side. */
	var idle_cfg = syncretro_lobby_cfg.idle || {};
	var idle_ars = idle_cfg.exempt_ars || "EXEMPT H";
	var idle_secs = bbs.compare_ars(idle_ars)
	    ? 0 : syncretro_lobby_gl.parse_duration(idle_cfg.timeout, "s");
```

then append ` -i` + idle_secs to the command line built at `:295-300`, after the `-t%T` term:

```js
	cmd = syncretro_lobby_binary + (syncretro_lobby_stdio ? " -stdio" : " -s%H")
	    + " -t%T -i" + idle_secs + " -name %a -core " + syncretro_lobby_core
```

Load the `[idle]` section beside the `[lobby]` one at `syncretro_lobby.js:77`:

```js
		syncretro_lobby_cfg.idle = f.iniGetObject("idle") || {};
```

Document the section in `xtrn/syncnes/syncretro.ini`:

```ini
[idle]
; Disconnect a user who has stopped providing input. The BBS's own "Maximum
; Inactivity" cannot do this for a graphical door: it is reset by any socket
; traffic, and this door's frame pacing makes the terminal answer ~10x/second
; on its own. 0 or absent disables.
;timeout    = 15m
; Countdown shown before exiting. Any key clears it and resets the clock.
;warn       = 60s
; Users matching this ARS are never timed out. Default matches the BBS's own
; convention (an H-exempt user is exempt from inactivity everywhere else).
;exempt_ars = EXEMPT H
```

- [ ] **Step 5: Live-verify on a node**

A live door session pins the cached binary over the `/sbbs` CIFS mount: exit any running syncnes session before testing, or new launches still run the old bytes.

Set `timeout = 2m` and `warn = 30s` in `xtrn/syncnes/syncretro.ini`, then from a terminal session enter the NES door and confirm, in order:

1. Sit still. At 90s the countdown toast appears on the bottom row and ticks down each second.
2. Press a key. The toast clears and does not return for another full 2 minutes.
3. Sit still again through the countdown. At 2 minutes the door exits to the lobby, and `/sbbs/data/syncretro/syncretro_n<node>.log` records the idle exit.
4. Grant your user the `H` exemption, re-enter, and confirm no warning ever appears (the lobby passed `-i0`).

Then verify the **no-lobby path**, which is what a non-Synchronet BBS and a
directly-registered xtrn entry both use. Launch the door binary without the
lobby — from `xtrn/syncnes/`, with a DOOR32.SYS and no `-i` argument — and
confirm the countdown still fires from `[idle] timeout` in `syncretro.ini`
alone. Then add `-i0` to that same invocation and confirm it suppresses the
timeout, proving argument-over-ini precedence without any JS in the path. This
step is not optional: the ini fallback is the *only* configuration mechanism
available on a non-Synchronet install, and the lobby path passing `-i`
unconditionally means a broken fallback would never show up in steps 1-4.

Restore the ini to commented-out defaults afterward.

- [ ] **Step 6: Format and commit**

```bash
uncrustify -c src/uncrustify.cfg --no-backup \
    src/doors/syncretro/syncretro_door.c src/doors/syncretro/syncretro_input.c \
    src/doors/syncretro/syncretro_config.c src/doors/syncretro/syncretro.h \
    src/doors/syncretro/main.c
cd /home/rswindell/sbbs && git diff --cached --stat   # must be empty before staging
git add src/doors/syncretro/ exec/load/syncretro_lobby.js xtrn/syncnes/syncretro.ini
awk 'length > 78 { print "OVER:", length($0), $0 }' /tmp/msg.txt   # must print nothing
git commit -F /tmp/msg.txt
```

---

### Task 4: syncconquer adoption (the lobby-less reference)

The reference for the three doors with no JS lobby. Its `door_check_time_limit()` is already a warn-only stub whose own comment invites this escalation, and its two-stage parser makes auto-replies structurally incapable of reaching the key path.

**Files:**
- Modify: `src/doors/syncconquer/door/door_input.c:111` (`emit_key()`) and `:305` (`mouse_event()`)
- Modify: `src/doors/syncconquer/door/door_io.c` — `door_config_duration()` beside `door_config_bool()` at `:821`; `-i` parse in `door_resolve_args()` at `:248`; `door_check_idle()` beside `door_check_time_limit()` at `:1288`; call it in `door_io_present()` at `:2765`
- Modify: `src/doors/syncconquer/door/door_node.c` — a public `door_node_notice()` beside the existing banner users
- Modify: `src/doors/syncconquer/door/door.h` and `door_node.h` — declare the new entry points
- Modify: `xtrn/syncalert/syncalert.ini` and `xtrn/syncdawn/syncdawn.ini` — document `[idle]`

**Interfaces:**
- Consumes: `termgfx_idle_t`, `termgfx_idle_init()`, `termgfx_idle_activity()`, `termgfx_idle_poll()`, `TERMGFX_IDLE_*` (Task 1).
- Produces: `void door_io_idle_activity(void)` (declared in `door.h`), `void door_node_notice(const char *text, int ms)` (declared in `door_node.h`).

- [ ] **Step 1: Add the banner entry point**

`banner_set()` is static and its callers fill `g_ov[]` first, so add a public one-line notice beside `door_node_userlist_request()` in `src/doors/syncconquer/door/door_node.c:128`:

```c
/* One-line transient banner, for a door-level message that is not a page or a
 * who's-online list (the idle countdown). Same overlay the other two use, so
 * it inherits their draw path and expiry. */
void door_node_notice(const char *text, int ms)
{
	snprintf(g_ov[0], SA_NODE_OVCOLS, " %s", (text != NULL) ? text : "");
	banner_set(1, ms);
}
```

Declare it in `src/doors/syncconquer/door/door_node.h`.

- [ ] **Step 2: Add a duration config reader, the `-i` argument, and the check**

In `src/doors/syncconquer/door/door_io.c`, beside `door_config_bool()` at `:821`:

```c
/* Read a duration <door>.ini setting as SECONDS ("15m", "900", "1h"); returns
 * `def` when the file or key is absent. Bare numbers are seconds, per xpdev. */
static unsigned door_config_duration(const char *section, const char *key, unsigned def)
{
	char     path[700];
	FILE    *f;
	unsigned val = def;

	if (!door_ini_path(path, sizeof path))
		return def;
	f = iniOpenFile(path, FALSE);   /* read-only; do NOT create */
	if (f != NULL) {
		val = (unsigned)iniReadDuration(f, section, key, (double)def);
		fclose(f);
	}
	return val;
}
```

Add the idle state beside the time-limit globals at `:1286`:

```c
#include "idle.h"

static termgfx_idle_t g_idle;
static int            g_idle_arg = -1;   /* -i<seconds>; -1 = not given */
```

Parse `-i<seconds>` in `door_resolve_args()` at `:248` (same loop that already recognizes the drop file and `-home`), then arm the clock in `door_io_init()` where `g_time_start_ms` is set:

```c
	termgfx_idle_init(&g_idle,
	                  (g_idle_arg >= 0) ? (unsigned)g_idle_arg
	                                    : door_config_duration("idle", "timeout", 0),
	                  door_config_duration("idle", "warn", 60),
	                  door_now_ms());
```

Add the per-present check beside `door_check_time_limit()` at `:1288`:

```c
/* Idle-USER timeout. Distinct from the time limit above: that one bounds the
 * session, this one notices nobody is driving it. Warn-then-exit, because a
 * player watching an FMV is silent but present. */
static void door_check_idle(void)
{
	static unsigned last_shown = 0;
	unsigned        left = 0;
	char            msg[96];

	switch (termgfx_idle_poll(&g_idle, door_now_ms(), &left)) {
		case TERMGFX_IDLE_WARN:
			if (left != last_shown) {
				snprintf(msg, sizeof msg,
				         "Still there? Press any key -- disconnecting in %u second%s",
				         left, (left == 1) ? "" : "s");
				door_node_notice(msg, 1500);
				last_shown = left;
			}
			break;
		case TERMGFX_IDLE_EXPIRED:
			fprintf(stderr, DOOR_SHORT_NAME ": idle timeout -- no user input; exiting\n");
			exit(0);
		default:
			last_shown = 0;
			break;
	}
}
```

Call it in `door_io_present()` at `:2765`, beside the existing checks:

```c
	door_check_idle();        /* idle-user countdown / forced exit */
```

Add `void door_io_idle_activity(void)` (calling `termgfx_idle_activity(&g_idle, door_now_ms())`) and declare it in `door.h`.

- [ ] **Step 3: Hook real input**

In `src/doors/syncconquer/door/door_input.c`, at the top of `emit_key()` (`:111`), before the F1 interception — so the help key counts as activity too:

```c
	/* A genuine keystroke. Safe here because door_csi_final() consumes every
	 * terminal auto-reply upstream and re-emits only unrecognized bytes, so a
	 * pace-ack can never reach this function. */
	door_io_idle_activity();
```

And in `mouse_event()` (`:305`), after the pixel-mode auto-detect block — motion alone queues no event in this door, so the report handler is the only place that sees it:

```c
	door_io_idle_activity();
```

- [ ] **Step 4: Build and run the shared tests**

```bash
cd /home/rswindell/sbbs/src/doors/syncconquer && ./build.sh
cd /tmp/tgtest && ctest --output-on-failure
```

Expected: build succeeds with no new warnings; all 3 termgfx tests still pass.

- [ ] **Step 5: Live-verify on a node**

Exit any running syncalert/syncdawn session first (the live binary is a symlink to the build output, and a running session pins the old bytes).

Add to `xtrn/syncalert/syncalert.ini`:

```ini
[idle]
timeout = 2m
warn    = 30s
```

Enter the door and confirm: the red banner countdown appears at 90s and ticks; a keypress clears it and resets the clock; **moving the mouse alone also clears it**; and at 2 minutes with no input the door exits. Then confirm the `-i` path overrides the ini by adding `-i0` to the `cmd` in `ctrl/xtrn.ini` and verifying no warning ever appears.

Restore both files afterward, and document the `[idle]` section (commented out, with the same explanatory text used in Task 3) in `xtrn/syncalert/syncalert.ini` and `xtrn/syncdawn/syncdawn.ini`.

- [ ] **Step 6: Format and commit**

```bash
uncrustify -c src/uncrustify.cfg --no-backup \
    src/doors/syncconquer/door/door_io.c src/doors/syncconquer/door/door_input.c \
    src/doors/syncconquer/door/door_node.c src/doors/syncconquer/door/door_node.h \
    src/doors/syncconquer/door/door.h
cd /home/rswindell/sbbs && git diff --cached --stat   # must be empty before staging
git add src/doors/syncconquer/door/ xtrn/syncalert/syncalert.ini xtrn/syncdawn/syncdawn.ini
awk 'length > 78 { print "OVER:", length($0), $0 }' /tmp/msg.txt   # must print nothing
git commit -F /tmp/msg.txt
```

---

## Phase 2 and 3 (separate plans)

- **Phase 2 — syncmoo1, syncdoom, syncduke.** Mechanical once Tasks 3 and 4 exist, but each needs its own reading: syncmoo1 has no overlay at all (build one modeled on syncretro's `sr_io_toast()`); syncdoom and syncduke need multi-site activity hooks because they grew parallel legacy-byte and native kitty/evdev dispatchers, and syncduke has no unified exit check. Hooking their deeper common points (`keyq_push()`, `rawq_push()`) is **rejected** — synthetic key-up expiry timers also call them, which would forge activity for a user who has left.
- **Phase 3 — syncscumm.** Needs a session-deadline concept introduced (`termgfx_termio.c:618` currently reads the drop file's `time_limit_ms` and discards it) *and* a transient-overlay primitive, both in shared termio code, plus per-title ini handling. Its input hook is a one-liner at `termgfx_termio.c:3069`, and everything built there is inherited by syncrpg.
