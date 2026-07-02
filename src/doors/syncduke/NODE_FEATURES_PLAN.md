# SyncDuke Synchronet Node Features — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add SyncDOOM's in-game Synchronet node integration to SyncDuke — Ctrl-U who's-online listing, `NODE_EXT` status broadcast, and neutral incoming inter-node-message display.

**Architecture:** A new `syncduke_node.c` module owns all BBS/node state and a top-of-screen banner (rows + expiry + a change signature). `syncduke_plat.c`'s per-frame `_nextpage()` calls `syncduke_node_tick()` (deferred BBS I/O off the input path); `syncduke_io.c`'s `present()` folds the banner's signature into its frame de-dupe (forcing a repaint on appear/expire) and calls the banner draw after emitting each frame, so it overlays sixel/JXL/text alike. Everything gates on `sbbs_node_available()`.

**Tech Stack:** C (C99), `termgfx/sbbs_node.{c,h}` (already linked), CMake, the door's existing overlay/de-dupe/pacing machinery. Reference implementation: `src/doors/syncdoom/syncdoom.c`.

## Global Constraints

- Portable C — must compile clean under GCC/Clang with `-Werror`-grade cleanliness (this door builds via `cmake --build build --target syncduke`). Match surrounding style; run `uncrustify -c src/uncrustify.cfg` on new/edited door files as the closing step.
- No new dependency — `termgfx/sbbs_node.{c,h}` is already linked; use only its public API (below).
- All node features are **no-ops when `sbbs_node_available()` is 0** (non-Synchronet / DOOR32.SYS host with no `node.dab`). Never error there.
- Incoming node messages are shown **verbatim** — never add the word "paging".
- Exit clears **only the `NODE_EXT` flag** (via `sbbs_node_set_ext("")`), which is enough; do not special-case zeroing `node.exb`.
- BBS file I/O (listing, status, receive) runs on the **main loop** (`syncduke_node_tick()`), never in the input/keyhandler path.

## `sbbs_node` API (from `termgfx/sbbs_node.h` — consumed, not modified)

```c
int  sbbs_node_init(const char *home);      // once at startup; 1 if node.dab found
int  sbbs_node_available(void);             // 1 if a usable BBS context exists
int  sbbs_my_node(void);                    // this door's node number, or 0
int  sbbs_list_nodes(sbbs_node_info_t *out, int max, int selfnode); // count; skips selfnode if !=0
const char *sbbs_username(int usernum, char *buf, size_t bufsz);    // alias into buf
const char *sbbs_action_str(const sbbs_node_info_t *node, char *buf, size_t bufsz); // full phrase
const char *sbbs_node_ext(int num, char *buf, size_t bufsz);        // node's free-text status
void sbbs_node_set_ext(const char *text);   // set/clear our NODE_EXT free-text ("" clears the flag)
int  sbbs_recv_nmsg(int mynode, char *buf, size_t bufsz);           // >0 = a message was waiting
// sbbs_node_info_t { int number, useron, anon, action, aux, ext; unsigned connection; }
```

## File structure

- **Create** `src/doors/syncduke/syncduke_node.c` — node init/teardown, status broadcast, Ctrl-U request + who's-online build, incoming-message poll, banner state + draw + signature.
- **Modify** `src/doors/syncduke/syncduke.h` — declarations for the new module's public functions.
- **Modify** `src/doors/syncduke/syncduke_input.c` — Ctrl-U (0x15) → request flag; add 0x15 to the shortcut swallow set.
- **Modify** `src/doors/syncduke/syncduke_io.c` — `present()` folds the banner signature into the de-dupe and calls `syncduke_node_draw()` after each emit.
- **Modify** `src/doors/syncduke/syncduke_plat.c` — init at startup; call `syncduke_node_tick()` per frame in `_nextpage()`.
- **Modify** `src/doors/syncduke/CMakeLists.txt` — add `syncduke_node.c` to the sources.
- **Modify** `src/doors/syncduke/tests/test_kitty.c` — assert Ctrl-U is swallowed (never reaches Duke).

---

### Task 1: Module scaffold + init/teardown + status broadcast + per-frame tick

**Files:**
- Create: `src/doors/syncduke/syncduke_node.c`
- Modify: `src/doors/syncduke/syncduke.h` (add declarations, near the other `syncduke_*` prototypes)
- Modify: `src/doors/syncduke/syncduke_plat.c` (`_platform_init` ~line 124: init; `_nextpage` ~line 289 after `_handle_events()`: tick)
- Modify: `src/doors/syncduke/CMakeLists.txt` (add source)

**Interfaces:**
- Produces:
  - `void syncduke_node_init(void);` — resolve BBS context (`sbbs_node_init` with the door's `-home`), install the exit-clear atexit. Idempotent.
  - `void syncduke_node_tick(void);` — per-frame: refresh status on change (Tasks 2/4 add userlist build + nmsg poll here).
- Consumes: `sbbs_node_init`, `sbbs_node_available`, `sbbs_node_set_ext` (API above); `syncduke_door` for the `-home` value.

Note on `-home`: `syncduke_config.c` parses `-home` but stores it only as the process CWD (it `chdir`s there). `sbbs_node_init` uses `$SBBSCTRL` for `node.dab`/`node.exb` and only needs `home` to locate the data dir; pass `""` (the door has already `chdir`'d into `-home`, and `sbbs_node.c` also honors `$SBBSCTRL`/`$SBBSNODE`). Confirm `sbbs_node.c`'s use of `home` before relying on `""`; if it needs the real path, expose it from `syncduke_config.c` via a `const char *syncduke_home(void);` accessor.

- [ ] **Step 1: Confirm the `js_system.cpp` reader gates on `NODE_EXT`** (spec's one open check)

Run: `grep -n "vstatus\|NODE_EXT\|extdesc\|\.exb" src/sbbs3/js_system.cpp`
Expected: `node.vstatus` is populated from `node.exb` only when `NODE_EXT` is set (so clearing the flag suffices). If it reads `node.exb` unconditionally, note it and have the exit-clear also write an empty `node.exb` (i.e., keep full `sbbs_node_set_ext("")`, which already does both — so this is informational only; `set_ext("")` is safe either way).

- [ ] **Step 2: Create `syncduke_node.c` with init/teardown + status + tick skeleton**

```c
/*
 * syncduke_node.c -- door-native Synchronet node integration: who's-online
 * (Ctrl-U), a NODE_EXT status broadcast, and neutral incoming inter-node messages.
 * All BBS file I/O runs from syncduke_node_tick() (the main loop), never the input
 * path. Everything no-ops when sbbs_node_available() is false. Mirrors the in-game
 * node features of src/doors/syncdoom/syncdoom.c.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "sbbs_node.h"   /* termgfx: node listing / status / nmsg */
#include "syncduke.h"

static int  g_node_ok;                 /* sbbs_node_available() cached at init */

static void syncduke_node_atexit(void)
{
	if (g_node_ok)
		sbbs_node_set_ext("");         /* clear our NODE_EXT flag on exit */
}

void syncduke_node_init(void)
{
	static int inited;
	if (inited)
		return;
	inited = 1;
	sbbs_node_init("");                /* $SBBSCTRL/$SBBSNODE locate node.dab; CWD is -home */
	g_node_ok = sbbs_node_available();
	if (g_node_ok)
		atexit(syncduke_node_atexit);
}

/* Publish "playing SyncDuke" as our who's-online free-text, only when it changes. */
static void syncduke_node_status(void)
{
	static char last[64];
	const char *status = "playing SyncDuke";
	if (strcmp(status, last) != 0) {
		sbbs_node_set_ext(status);
		snprintf(last, sizeof(last), "%s", status);
	}
}

void syncduke_node_tick(void)
{
	if (!g_node_ok)
		return;
	syncduke_node_status();
	/* Task 2 adds: build who's-online banner if Ctrl-U was requested.
	 * Task 4 adds: poll sbbs_recv_nmsg ~1 Hz -> banner + bell. */
}
```

- [ ] **Step 3: Declare the public functions in `syncduke.h`**

Add near the other module prototypes (e.g. after the `syncduke_io.c` group):

```c
/* --- provided by syncduke_node.c (Synchronet who's-online / status / messages) --- */
void syncduke_node_init(void);   /* resolve BBS context; install exit status-clear */
void syncduke_node_tick(void);   /* per-frame: status broadcast (+ Ctrl-U build, nmsg poll) */
```

- [ ] **Step 4: Add the source to CMake**

In `src/doors/syncduke/CMakeLists.txt`, add `syncduke_node.c` to the `syncduke` target's source list (beside `syncduke_io.c` / `syncduke_input.c`).

- [ ] **Step 5: Wire init + tick into the platform layer**

In `syncduke_plat.c`, `_platform_init()` (~line 124), after the existing init, add:
```c
	syncduke_node_init();   /* Synchronet who's-online / status (no-op off a BBS) */
```
In `_nextpage()` (~line 289), immediately after `_handle_events();`, add:
```c
	syncduke_node_tick();   /* status broadcast; who's-online build; message poll */
```

- [ ] **Step 6: Build**

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | grep -E "error|warning|Built target syncduke"`
Expected: `[100%] Built target syncduke`, no errors/warnings from the new file.

- [ ] **Step 7: uncrustify the new file**

Run: `cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_node.c` then rebuild (Step 6) to confirm still clean.

- [ ] **Step 8: Commit**

```bash
git add src/doors/syncduke/syncduke_node.c src/doors/syncduke/syncduke.h \
        src/doors/syncduke/syncduke_plat.c src/doors/syncduke/CMakeLists.txt
git commit -m "syncduke: node module scaffold + status broadcast"
```

**Runtime check (deferred to end):** on the live BBS, launch SyncDuke and confirm another node's who's-online shows "playing SyncDuke", and that it reverts after exit.

---

### Task 2: Banner state + cross-tier overlay draw + present() integration

**Files:**
- Modify: `src/doors/syncduke/syncduke_node.c` (banner state, `syncduke_node_draw`, `syncduke_node_overlay_sig`)
- Modify: `src/doors/syncduke/syncduke.h` (declarations)
- Modify: `src/doors/syncduke/syncduke_io.c` (`present()` ~lines 1089-1098 de-dupe; after each emit; before `done:`)

**Interfaces:**
- Produces:
  - `uint32_t syncduke_node_overlay_sig(void);` — changes whenever the banner content or visibility changes (0 when no banner is live).
  - `void syncduke_node_draw(int cols, int rows);` — emit the banner ANSI (top strip) via `syncduke_out_put`, or nothing if not live/expired.
  - Internal: `syncduke_node_banner_set(char rows[][N], int nrows, int ms)` used by Tasks 2/3/4.
- Consumes: `syncduke_out_put` (io.c), `syncduke_now_ms`-equivalent time source used elsewhere in io.c (use the same clock `g_page_ov`-style expiry compares against; in SyncDuke that is the `syncduke_in_now_ms()`/`syncduke_now_us()` family — pick the ms one already used for pacing).

- [ ] **Step 1: Add banner state + accessors to `syncduke_node.c`**

```c
#define SD_NODE_OVROWS 8
#define SD_NODE_OVCOLS 96
static char     g_ov[SD_NODE_OVROWS][SD_NODE_OVCOLS];
static int      g_ov_rows;
static uint32_t g_ov_until;      /* ms deadline (same clock as pacing) */
static uint32_t g_ov_sig;        /* bumped on every content/visibility change */

extern uint32_t syncduke_in_now_ms(void);   /* the ms clock used by input/pacing */

static void banner_set(int nrows, int ms)
{
	if (nrows > SD_NODE_OVROWS) nrows = SD_NODE_OVROWS;
	g_ov_rows  = nrows;
	g_ov_until = syncduke_in_now_ms() + (uint32_t)ms;
	g_ov_sig++;                  /* mark dirty so present() repaints */
}

static int banner_live(void)
{
	if (g_ov_rows > 0 && (int32_t)(syncduke_in_now_ms() - g_ov_until) >= 0) {
		g_ov_rows = 0;           /* expired -> clear + mark dirty once */
		g_ov_sig++;
	}
	return g_ov_rows > 0;
}

uint32_t syncduke_node_overlay_sig(void) { (void)banner_live(); return g_ov_rows ? g_ov_sig : 0; }

void syncduke_node_draw(int cols, int rows)
{
	int i;
	(void)rows;
	if (!banner_live())
		return;
	for (i = 0; i < g_ov_rows; i++) {
		char   ob[SD_NODE_OVCOLS + 32];
		int    len = (int)strlen(g_ov[i]);
		int    n;
		if (len > cols) len = cols;
		/* white-on-red top strip, one node per row, left-aligned */
		n = snprintf(ob, sizeof ob, "\x1b[%d;1H\x1b[1;37;41m%-*.*s\x1b[0m",
		             i + 1, cols, len, g_ov[i]);
		if (n > 0)
			syncduke_out_put(ob, (size_t)n);
	}
}
```

- [ ] **Step 2: Declare the two public functions in `syncduke.h`**

```c
uint32_t syncduke_node_overlay_sig(void);   /* banner change signature (0 = no banner) */
void     syncduke_node_draw(int cols, int rows);  /* paint the who's-online/message banner */
```

- [ ] **Step 3: Fold the banner signature into present()'s de-dupe**

In `syncduke_io.c present()`, near line 1090 (after `sd_hud_sig = sd_hud_signature();`) add:
```c
	uint32_t node_sig = syncduke_node_overlay_sig();
```
Extend the de-dupe condition (line 1093-1097) so a banner change is never skipped — add this term to the `if (... ) goto done;`:
```c
	    && node_sig == syncduke_node_last_sig
```
and declare a file-static `static uint32_t syncduke_node_last_sig;` near the other `sd_hud_last_sig` state. When the banner changed, also force the image-tier repaint: right before the tier emit, add:
```c
	if (node_sig != syncduke_node_last_sig) {
		syncduke_have_last = 0;   /* image tier: force a full re-emit so the strip draws/erases */
		rt_invalidate();          /* text tier: same */
	}
```

- [ ] **Step 4: Draw the banner after the frame emit**

In `present()`, just before the `done:` label (after both the text and image emit branches have run and `n` bytes were emitted), add:
```c
	syncduke_node_draw(syncduke_out_w / 8, syncduke_out_h / 16);  /* who's-online / message strip */
	syncduke_node_last_sig = node_sig;
```
(Use the same cols/rows derivation `syncduke_emit_text` uses at io.c:439-440 — pixel canvas / 8 / 16. If `present()` already has cols/rows locals, use those.)

- [ ] **Step 5: Add a temporary self-test banner, build, eyeball, then remove**

Temporarily, in `syncduke_node_init()` after `g_node_ok = ...`, add `banner_set(...)` with one row "SyncDuke node banner test" and 4000 ms (only if `g_node_ok`), build, run once against the live BBS to confirm the strip paints over the current tier and auto-clears, then delete the temporary call.

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1`
Expected: `[100%] Built target syncduke`.

- [ ] **Step 6: uncrustify + rebuild + commit**

```bash
cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_node.c
# NOTE: do NOT whole-file uncrustify syncduke_io.c (its committed HUD block is not
# uncrustify-conformant and would churn); hand-match style for the small io.c edits.
cmake --build src/doors/syncduke/build --target syncduke 2>&1 | tail -1
git add src/doors/syncduke/syncduke_node.c src/doors/syncduke/syncduke.h src/doors/syncduke/syncduke_io.c
git commit -m "syncduke: cross-tier who's-online/message banner overlay"
```

---

### Task 3: Ctrl-U who's-online listing

**Files:**
- Modify: `src/doors/syncduke/syncduke_input.c` (`handle_key` ~lines 698-701 add 0x15; swallow-repeat guard ~line 714)
- Modify: `src/doors/syncduke/syncduke_node.c` (request flag; build who's-online in tick)
- Modify: `src/doors/syncduke/syncduke.h` (declaration)
- Test: `src/doors/syncduke/tests/test_kitty.c`

**Interfaces:**
- Produces: `void syncduke_node_userlist_request(void);` — sets a flag consumed by the next `syncduke_node_tick()`.
- Consumes: `sbbs_list_nodes`, `sbbs_my_node`, `sbbs_username`, `sbbs_node_ext`, `sbbs_action_str`.

- [ ] **Step 1: Write the failing test — Ctrl-U is swallowed (never reaches Duke)**

In `tests/test_kitty.c`, after the existing Ctrl-S test block, add:
```c
	/* Ctrl-U (0x15) = who's-online: door-level, never a Duke scancode. */
	while (syncduke_input_has_raw()) syncduke_input_pop_raw();
	feed("\x1b[117;5u");      syncduke_input_pump(pp[0], 33, 1);   /* kitty Ctrl-U */
	chk("Ctrl-U no rawq", syncduke_input_has_raw(), 0);
```
Add the stub the pump now references (near the other stubs at the top of the file):
```c
void syncduke_node_userlist_request(void) { }
```

- [ ] **Step 2: Run the test to verify it FAILS**

Run: `cd /home/rswindell/sbbs/src/doors/syncduke/tests && cc -I../Game/src -I../../termgfx -o /tmp/tk test_kitty.c ../syncduke_input.c ../syncduke_door.c ../../termgfx/caps.c && /tmp/tk 2>&1 | grep -E "Ctrl-U|FAIL"`
Expected: `Ctrl-U no rawq   got=<nonzero> ... FAIL` (Ctrl-U currently maps to a scancode / reaches the queue).

- [ ] **Step 3: Handle Ctrl-U in `handle_key` + swallow its repeat**

In `syncduke_input.c handle_key`, in the fresh-press block (~line 698, beside the other `if (c == 0x..)` shortcuts) add:
```c
		if (c == 0x15) { syncduke_node_userlist_request(); return; }   /* Ctrl-U: who's online */
```
And add `0x15` to the kitty auto-repeat swallow guard (~line 714, the `c == 0x13 || c == 0x14 || ...` list) so a held Ctrl-U doesn't leak.

- [ ] **Step 4: Implement `syncduke_node_userlist_request` + build in the tick**

In `syncduke_node.c`:
```c
static int g_want_userlist;
void syncduke_node_userlist_request(void) { g_want_userlist = 1; }

static void syncduke_node_userlist(void)
{
	sbbs_node_info_t nodes[16];
	char alias[26], act[80];
	int  n, i, r, max = SD_NODE_OVROWS;
	n = sbbs_list_nodes(nodes, (int)(sizeof(nodes)/sizeof(nodes[0])), sbbs_my_node());
	if (n == 0) {
		snprintf(g_ov[0], SD_NODE_OVCOLS, " No one else is online.");
		banner_set(1, 6000);
		return;
	}
	snprintf(g_ov[0], SD_NODE_OVCOLS, " Who's online (%d):", n);
	r = 1;
	for (i = 0; i < n && r < max; i++) {
		const char *who, *activity;
		if (r == max - 1 && n - i > 1) {
			snprintf(g_ov[r], SD_NODE_OVCOLS, "   ...and %d more", n - i);
			r++; break;
		}
		who = nodes[i].anon ? "UNKNOWN" : sbbs_username(nodes[i].useron, alias, sizeof(alias));
		activity = nodes[i].ext ? sbbs_node_ext(nodes[i].number, act, sizeof(act))
		                        : sbbs_action_str(&nodes[i], act, sizeof(act));
		snprintf(g_ov[r], SD_NODE_OVCOLS, "   %-20.20s  %.50s", who, activity);
		r++;
	}
	banner_set(r, 9000);
}
```
In `syncduke_node_tick()`, after the status call:
```c
	if (g_want_userlist) { g_want_userlist = 0; syncduke_node_userlist(); }
```
Declare `void syncduke_node_userlist_request(void);` in `syncduke.h`.

- [ ] **Step 5: Remove the test stub; rebuild the real test to verify it PASSES**

Delete the `syncduke_node_userlist_request(void) { }` stub from `test_kitty.c` (the real one now links from `syncduke_node.c`). The test build must include the new source:

Run: `cd /home/rswindell/sbbs/src/doors/syncduke/tests && cc -I../Game/src -I../../termgfx -o /tmp/tk test_kitty.c ../syncduke_input.c ../syncduke_node.c ../syncduke_door.c ../../termgfx/caps.c && /tmp/tk 2>&1 | tail -1`
Expected: `ALL PASS`.
(If `syncduke_node.c` pulls in symbols the test doesn't link, stub them in the test's stub block — mirror how `test_kitty.c` already stubs `syncduke_stats_toggle` etc. `syncduke_node.c` itself should only need `sbbs_node.*` + `syncduke_out_put`; stub `syncduke_out_put` and `syncduke_in_now_ms` in the test.)

- [ ] **Step 6: Build the door + uncrustify + commit**

```bash
cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1
cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_node.c
git add src/doors/syncduke/syncduke_node.c src/doors/syncduke/syncduke_input.c \
        src/doors/syncduke/syncduke.h src/doors/syncduke/tests/test_kitty.c
git commit -m "syncduke: Ctrl-U who's-online listing"
```

---

### Task 4: Incoming inter-node-message receive (neutral, + bell)

**Files:**
- Modify: `src/doors/syncduke/syncduke_node.c` (poll in tick; word-wrap into banner)

**Interfaces:**
- Consumes: `sbbs_recv_nmsg`, `sbbs_my_node`, `syncduke_out_put` (for the bell), the banner state.

- [ ] **Step 1: Implement the ~1 Hz poll + verbatim word-wrap + bell**

In `syncduke_node.c`:
```c
static void syncduke_node_recv(void)
{
	static uint32_t last;
	char raw[600], clean[600];
	const char *s; char *d;
	uint32_t now = syncduke_in_now_ms();
	int me, w, len, pos, r;
	if (now - last < 1000) return;
	last = now;
	me = sbbs_my_node();
	if (me < 1 || sbbs_recv_nmsg(me, raw, sizeof(raw)) <= 0) return;
	/* strip Ctrl-A codes + BEL, fold whitespace runs to one space -> one flowing string */
	for (s = raw, d = clean; *s && d < clean + sizeof(clean) - 1; s++) {
		if (*s == '\x01') { if (s[1]) s++; continue; }
		if (*s == '\x07') continue;
		if (*s == '\r' || *s == '\n' || *s == ' ') { if (d > clean && d[-1] != ' ') *d++ = ' '; continue; }
		*d++ = *s;
	}
	while (d > clean && d[-1] == ' ') d--;
	*d = '\0';
	/* word-wrap into the banner (verbatim -- NO "paging" label) */
	w = SD_NODE_OVCOLS - 2; if (w > 80) w = 80; if (w < 20) w = 20;
	len = (int)strlen(clean); pos = 0; r = 0;
	while (pos < len && r < SD_NODE_OVROWS) {
		int take = len - pos;
		if (take > w) { int b = w; while (b > 0 && clean[pos+b] != ' ') b--; take = (b > w/2) ? b : w; }
		memcpy(g_ov[r], clean + pos, (size_t)take); g_ov[r][take] = '\0';
		r++; pos += take; while (pos < len && clean[pos] == ' ') pos++;
	}
	banner_set(r, 9000);
	syncduke_out_put("\x07", 1);   /* audible alert */
}
```
In `syncduke_node_tick()`, after the userlist check:
```c
	syncduke_node_recv();
```

- [ ] **Step 2: Build**

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1`
Expected: `[100%] Built target syncduke`.

- [ ] **Step 3: uncrustify + commit**

```bash
cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_node.c
cmake --build src/doors/syncduke/build --target syncduke 2>&1 | tail -1
git add src/doors/syncduke/syncduke_node.c
git commit -m "syncduke: show incoming inter-node messages in-game"
```

---

### Task 5: Full validation

**Files:** none (validation only).

- [ ] **Step 1: Both doors build clean**

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1`
Expected: `[100%] Built target syncduke` (no new warnings).

- [ ] **Step 2: SyncDuke input unit tests still pass**

Run: `cd /home/rswindell/sbbs/src/doors/syncduke/tests && for t in test_kitty test_evdev; do cc -I../Game/src -I../../termgfx -o /tmp/$t $t.c ../syncduke_input.c ../syncduke_node.c ../syncduke_door.c ../../termgfx/caps.c 2>/dev/null && echo -n "$t: " && /tmp/$t | tail -1; done`
Expected: both `ALL PASS`. (Add `../syncduke_node.c` only if the suite references its symbols; otherwise keep the original link line.)

- [ ] **Step 3: Runtime validation on the live BBS (node.dab present)**

Manual, against a real Synchronet with ≥2 nodes:
1. Launch SyncDuke on node A; from node B's who's-online, confirm it shows **"playing SyncDuke"**.
2. In SyncDuke, press **Ctrl-U** → the top strip lists who's online (or "No one else is online."), auto-clears ~9 s, over whatever tier is active (sixel/JXL/text).
3. Send a node message to node A (e.g. from node B or the sysop) → node A's SyncDuke shows the message **verbatim** + a bell.
4. Quit SyncDuke back to the BBS → node A's who's-online reverts to the normal action (the `NODE_EXT` flag was cleared).
5. On a non-Synchronet run (no `node.dab`): Ctrl-U does nothing, no errors.

- [ ] **Step 4: Final commit if any validation fixes were needed** (else nothing to do).

---

## Self-review notes

- **Spec coverage:** Ctrl-U (Task 3), status broadcast (Task 1), incoming receive (Task 4), banner rendering (Task 2), init/teardown + flag-only clear (Task 1), non-Synchronet no-op (gated throughout), testing (Task 3 unit + Task 5 runtime). Paging-send intentionally absent (deferred).
- **Open item folded in:** the `js_system.cpp` `NODE_EXT`-gating check is Task 1 Step 1; `set_ext("")` is safe regardless of its outcome.
- **Deferred detail:** status text is "playing SyncDuke" (no level) for v1 — adding `(E#L#)` needs a `syncduke_game.c` level accessor and is a clean follow-up, not in this plan.
- **Rendering caveat to watch during Task 2:** the exact ms clock and the `cols/rows` derivation in `present()` must match what `syncduke_emit_text` uses (io.c:439-440); verify at implementation time.
