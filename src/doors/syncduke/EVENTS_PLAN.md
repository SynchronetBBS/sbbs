# SyncDuke events.jsonl + shared game_lobby.js events layer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SyncDuke's door writes an `events.jsonl` activity log (`start`/`level`/`death`), the shared `game_lobby.js` gains generic events read/prune/feed helpers, and SyncDuke's lobby shows a "recent activity" view using them.

**Architecture:** A new door module `syncduke_events.c` (includes `duke3d.h`) owns the `-eventlog` path, a JSON writer, the event emitters, and a per-frame detector called from `_nextpage()`. `game_lobby.js` gains game-agnostic `read_events`/`prune_events`/`event_feed`/`ago`/`mmss`. SyncDuke's lobby passes `-eventlog`, supplies a per-event text formatter, and renders the feed via the shared helpers.

**Tech Stack:** C99 (door, CMake build), SpiderMonkey-1.8.5 JS (`exec/load/game_lobby.js`, `xtrn/syncduke/*.js`), `exec/load/json_lines.js`, jsexec (headless JS test). Reference: `src/doors/syncdoom/syncdoom.c` (`sd_event_*`), `xtrn/syncdoom/syncdoom_lib.js` (`sd_recent_events`/`sd_event_feed`).

## Global Constraints

- Scope = Sub-project A only. **Do NOT modify SyncDOOM or `syncdoom_lib.js`.**
- Portable C99; door builds via `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke`. (Target compiles with `-w`; still write warning-clean code.)
- Door adds NO new dependency; `-eventlog` empty ⇒ logging fully off (no-op), never errors.
- Event writes are single append-mode line writes (atomic, no lock), one JSON object per line — SyncDOOM's model. Schema mirrors SyncDOOM so the shared reader parses both.
- New door file `syncduke_events.c` must be uncrustify-clean (`src/uncrustify.cfg`); pre-existing edited C files hand-matched, NOT whole-file uncrustified.
- All JS is SpiderMonkey 1.8.5 compatible (no modern ES: no `let`/`const`/arrow/`for..of`/template strings).
- `game_lobby.js` additions are game-agnostic and additive (no existing export changes — SyncDuke's current lobby use must keep working).
- Commits on `master` (no branch, no push); every commit message ends with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## File structure

- **Create** `src/doors/syncduke/syncduke_events.c` — `-eventlog` path storage, JSON escaper + emit, `start`/`level`/`death` emitters, per-frame detector.
- **Modify** `src/doors/syncduke/syncduke.h` — declarations.
- **Modify** `src/doors/syncduke/syncduke_config.c` — parse `-eventlog`; retain `-home` via a `syncduke_home()` accessor (for `usernum`).
- **Modify** `src/doors/syncduke/syncduke_io.c` — expose `syncduke_active_tier_name()`.
- **Modify** `src/doors/syncduke/syncduke_plat.c` — call `syncduke_events_tick()` per frame.
- **Modify** `src/doors/syncduke/CMakeLists.txt` — add `syncduke_events.c`.
- **Modify** `exec/load/game_lobby.js` — add `read_events`/`prune_events`/`event_feed`/`ago`/`mmss`.
- **Create** `exec/load/tests/test_game_lobby_events.js` — headless jsexec test for the new API.
- **Modify** `xtrn/syncduke/syncduke_lib.js` — `SD_EVENTS`, `-eventlog` in `sd_cmd`, `sd_event_text`.
- **Modify** `xtrn/syncduke/lobby.js` — "recent activity" menu item + `sd_show_activity` + prune on entry.

## Event schema (locked)

One JSON object per line. `start` deviates from SyncDOOM only by omitting the `desc`
(SyncDuke tracks no TERM-description string) — the shared reader tolerates missing fields
and the feed never uses it.

- `start` — `{"time":T,"type":"start","node":N,"usernum":N,"user":"A","term":{"cols":C,"rows":R},"tier":"S","build":{"hash":"H","date":"D"},"mode":"single|co-op","skill":K}`
- `level` — `{"time":T,"type":"level","node":N,"user":"A","map":"E#L#","secs":S,"skill":K}`
- `death` — `{"time":T,"type":"death","node":N,"user":"A","map":"E#L#","secs":S}`

`map`=`E{ud.volume_number+1}L{ud.level_number+1}`; `skill`=`ud.player_skill+1`; `mode`="co-op" if `syncduke_net_role` is `master`/`join` else "single"; `secs` from a `totalclock` delta (÷ `TICSPERSEC`).

---

### Task 1: `-eventlog` arg + events module (writer + `start` event) + per-frame tick

**Files:**
- Create: `src/doors/syncduke/syncduke_events.c`
- Modify: `src/doors/syncduke/syncduke.h`, `src/doors/syncduke/syncduke_config.c`, `src/doors/syncduke/syncduke_io.c`, `src/doors/syncduke/syncduke_plat.c`, `src/doors/syncduke/CMakeLists.txt`

**Interfaces produced:**
- `const char *syncduke_eventlog_path(void);` (config.c) — the `-eventlog` path, `""` if unset.
- `const char *syncduke_home(void);` (config.c) — the `-home` value (`""` if none), for `usernum`.
- `const char *syncduke_active_tier_name(void);` (io.c) — name of the current tier.
- `void syncduke_events_tick(void);` (events.c) — per-frame; this task emits only `start`.

- [ ] **Step 1: confirm `totalclock`/`TICSPERSEC` + engine globals**

Run: `grep -rnE "totalclock|TICSPERSEC|player_skill|volume_number|level_number" src/doors/syncduke/Game/src/*.h | head`
Expected: `totalclock` (extern) and `TICSPERSEC` (== 120) exist; `ud.player_skill/volume_number/level_number` are `ud` fields. Note the exact type of `totalclock`. If `TICSPERSEC` differs, use that value in `secs` math.

- [ ] **Step 2: parse `-eventlog` and add `syncduke_home()` in `syncduke_config.c`**

In `syncduke_config_init()`'s arg loop (beside `-home`/`-charset`), add:
```c
		else if (strcmp(argv[i], "-eventlog") == 0)
			strncpy(eventlog, argv[i + 1], sizeof(eventlog) - 1);
```
Declare `char eventlog[PATH_MAX] = "";` with the other locals, and after the loop store both home and eventlog into file statics:
```c
	snprintf(g_home_path, sizeof(g_home_path), "%s", home);
	snprintf(g_eventlog_path, sizeof(g_eventlog_path), "%s", eventlog);
```
Add near the other config globals/accessors:
```c
static char g_home_path[PATH_MAX];
static char g_eventlog_path[PATH_MAX];
const char *syncduke_home(void)           { return g_home_path; }
const char *syncduke_eventlog_path(void)  { return g_eventlog_path; }
```

- [ ] **Step 3: expose the active tier name in `syncduke_io.c`**

`sd_tier_name()` is static and `syncduke_last_tier` holds the active tier. Add:
```c
const char *syncduke_active_tier_name(void) { return sd_tier_name(syncduke_last_tier); }
```
(Place after `sd_tier_name`'s definition. If `syncduke_last_tier` isn't in scope there, use the file-static that present() sets.)

- [ ] **Step 4: create `syncduke_events.c` (writer + `start` + tick emitting only `start`)**

```c
/*
 * syncduke_events.c -- door-written events.jsonl activity log (start/level/death)
 * for the lobby's recent-activity feed. One compact JSON line per event, appended
 * (atomic, no lock) to the -eventlog path; "" disables logging. Includes duke3d.h
 * for the engine state the events describe. Mirrors src/doors/syncdoom/syncdoom.c's
 * sd_event_* model. Detector runs once per frame from _nextpage (syncduke_plat.c).
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#include "duke3d.h"
#include "syncduke.h"
#include "git_hash.h"   /* GIT_HASH, GIT_DATE */

/* Minimal JSON string escaper (aliases / tier / build). */
static const char *ev_esc(const char *s, char *out, size_t sz)
{
	size_t o = 0;
	if (s == NULL) s = "";
	for (; *s && o + 7 < sz; s++) {
		unsigned char c = (unsigned char)*s;
		if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
		else if (c < 0x20)         { o += (size_t)snprintf(out + o, sz - o, "\\u%04x", c); }
		else                        { out[o++] = (char)c; }
	}
	out[o] = '\0';
	return out;
}

static void ev_emit(const char *json)
{
	const char *path = syncduke_eventlog_path();
	FILE       *f;
	char        line[700];
	int         n;
	if (path[0] == '\0')
		return;
	n = snprintf(line, sizeof(line), "%s\n", json);
	if (n < 1 || (f = fopen(path, "ab")) == NULL)
		return;
	fwrite(line, 1, (size_t)n, f);   /* single append -> atomic line */
	fclose(f);
}

static int ev_real_game(void)
{
	uint8_t gm = ps[myconnectindex].gm;
	return (gm & MODE_GAME) && !(gm & MODE_DEMO) && ud.recstat != 2;
}

static const char *ev_mode(void)
{
	extern char syncduke_net_role[16];
	return (strcmp(syncduke_net_role, "master") == 0 || strcmp(syncduke_net_role, "join") == 0)
	       ? "co-op" : "single";
}

static void ev_map(char *buf, size_t sz)
{
	snprintf(buf, sz, "E%dL%d", ud.volume_number + 1, ud.level_number + 1);
}

/* usernum from the -home path (".../user/<num>/duke/"), like syncdoom. */
static int ev_usernum(void)
{
	const char *u = strstr(syncduke_home(), "user/");
	return u ? atoi(u + 5) : 0;
}

static void ev_start(void)
{
	char j[700], ua[80], ti[24], hh[24], hd[48];
	int  cols = (syncduke_term_px_w() > 0 ? syncduke_term_px_w() : 640) / 8;
	int  rows = (syncduke_term_px_h() > 0 ? syncduke_term_px_h() : 400) / 16;
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"start\",\"node\":%d,\"usernum\":%d,\"user\":\"%s\","
	         "\"term\":{\"cols\":%d,\"rows\":%d},\"tier\":\"%s\","
	         "\"build\":{\"hash\":\"%s\",\"date\":\"%s\"},\"mode\":\"%s\",\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(), ev_usernum(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), cols, rows,
	         ev_esc(syncduke_active_tier_name(), ti, sizeof(ti)),
	         ev_esc(GIT_HASH, hh, sizeof(hh)), ev_esc(GIT_DATE, hd, sizeof(hd)),
	         ev_mode(), ud.player_skill + 1);
	ev_emit(j);
}

void syncduke_events_tick(void)
{
	static int in_game;
	if (syncduke_eventlog_path()[0] == '\0')
		return;
	if (ev_real_game() && !in_game) {          /* entered a real game -> start */
		in_game = 1;
		ev_start();
	} else if (!ev_real_game() && in_game) {
		in_game = 0;
	}
	/* Task 2 adds level-change + death detection here. */
}
```

- [ ] **Step 5: declare in `syncduke.h`**

```c
/* --- provided by syncduke_events.c (events.jsonl activity log) --- */
void syncduke_events_tick(void);   /* per-frame: emit start/level/death to -eventlog */
```
Also declare the new accessors near their siblings:
```c
const char *syncduke_eventlog_path(void);   /* -eventlog path ("" = off) */
const char *syncduke_home(void);            /* -home value ("" = none) */
const char *syncduke_active_tier_name(void);/* current render tier name */
```

- [ ] **Step 6: call the tick from `_nextpage()` + add the source to CMake**

In `syncduke_plat.c _nextpage()`, right after `syncduke_node_tick();` (or after `_handle_events();` if node tick isn't present), add:
```c
	syncduke_events_tick();   /* events.jsonl activity log (no-op without -eventlog) */
```
In `CMakeLists.txt`, add `syncduke_events.c` to the `syncduke` sources (beside `syncduke_node.c`).

- [ ] **Step 7: build**

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1`
Expected: `[100%] Built target syncduke`.

- [ ] **Step 8: uncrustify the new file, rebuild, commit**

```bash
cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_events.c
cmake --build src/doors/syncduke/build --target syncduke 2>&1 | tail -1
git add src/doors/syncduke/syncduke_events.c src/doors/syncduke/syncduke.h \
        src/doors/syncduke/syncduke_config.c src/doors/syncduke/syncduke_io.c \
        src/doors/syncduke/syncduke_plat.c src/doors/syncduke/CMakeLists.txt
git commit -m "syncduke: events.jsonl writer + start event + -eventlog arg"
```

---

### Task 2: `level` and `death` events (detection in the tick)

**Files:** Modify `src/doors/syncduke/syncduke_events.c`

**Interfaces consumed:** `ev_emit`, `ev_real_game`, `ev_map`, `syncduke_player_dead()`, `totalclock`, `ud.*`.

- [ ] **Step 1: add the two emitters**

```c
static uint32_t ev_level_start;    /* totalclock at the current level's entry */

static int ev_secs(void)
{
	return (int)((uint32_t)totalclock - ev_level_start) / TICSPERSEC;
}

static void ev_level(int secs)     /* secs = the PREVIOUS level's elapsed */
{
	char j[300], ua[80], map[16];
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"level\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d,\"skill\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), map, secs, ud.player_skill + 1);
	ev_emit(j);
}

static void ev_death(void)
{
	char j[300], ua[80], map[16];
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"death\",\"node\":%d,\"user\":\"%s\","
	         "\"map\":\"%s\",\"secs\":%d}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ua, sizeof(ua)), map, ev_secs());
	ev_emit(j);
}
```

- [ ] **Step 2: wire detection into `syncduke_events_tick()`**

Replace the `/* Task 2 adds ... */` comment and extend the tick's state so it detects
level changes and death edges while in a real game:
```c
void syncduke_events_tick(void)
{
	static int      in_game, dead_last;
	static int      last_vol = -1, last_lev = -1;
	int             vol = ud.volume_number, lev = ud.level_number;

	if (syncduke_eventlog_path()[0] == '\0')
		return;

	if (ev_real_game() && !in_game) {
		in_game = 1;
		ev_start();
		ev_level_start = (uint32_t)totalclock;      /* first level of the session */
		last_vol = vol; last_lev = lev;
		ev_level(0);                                /* entered first level */
	} else if (!ev_real_game() && in_game) {
		in_game = 0;
		last_vol = last_lev = -1;
	}

	if (in_game && (vol != last_vol || lev != last_lev)) {   /* level change */
		int prev = ev_secs();                       /* elapsed on the level we just left */
		last_vol = vol; last_lev = lev;
		ev_level_start = (uint32_t)totalclock;
		ev_level(prev);
	}

	if (in_game) {                                  /* death rising edge */
		int dead = syncduke_player_dead();
		if (dead && !dead_last)
			ev_death();
		dead_last = dead;
	} else {
		dead_last = 0;
	}
}
```
(Remove the now-superseded `in_game`-only block from Task 1; this is the full tick.)

- [ ] **Step 3: build**

Run: `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1`
Expected: `[100%] Built target syncduke`.

- [ ] **Step 4: uncrustify, rebuild, commit**

```bash
cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_events.c
cmake --build src/doors/syncduke/build --target syncduke 2>&1 | tail -1
git add src/doors/syncduke/syncduke_events.c
git commit -m "syncduke: emit level + death events"
```

---

### Task 3: shared events API in `game_lobby.js` (TDD via jsexec)

**Files:**
- Modify: `exec/load/game_lobby.js`
- Create: `exec/load/tests/test_game_lobby_events.js`

**Interfaces produced (added to the object `game_lobby.js` returns):**
- `read_events(path, max)` → array of parsed objects (last `max`; `[]` if absent/error).
- `prune_events(path, cap, keep)` → rewrite keeping last `keep` if line count > `cap`; returns kept count.
- `event_feed(path, max, fmt)` → array of strings (`fmt(ev)` per event; falsy → skipped), most-recent first.
- `ago(t)` → "just now" / "Nm ago" / "Nh ago" / "Nd ago".
- `mmss(secs)` → "M:SS".

- [ ] **Step 1: write the failing test**

Create `exec/load/tests/test_game_lobby_events.js`:
```js
// jsexec exec/load/tests/test_game_lobby_events.js
var gl = load({}, "game_lobby.js");
var jl = load({}, "json_lines.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=" + got + " want=" + want + (ok ? " ok" : (fails++, " FAIL")));
}
var path = system.temp_dir + "gl_ev_test.jsonl";
var f = new File(path); f.open("w+"); f.close();      // fresh
jl.add(path, { time: 100, type: "level", map: "E1L1" });
jl.add(path, { time: 200, type: "death", map: "E1L1" });
f = new File(path); f.open("a"); f.write("this is not json\n"); f.close();   // bad line
jl.add(path, { time: 300, type: "level", map: "E1L2" });

var ev = gl.read_events(path, 10);
chk("read count (recover skips bad line)", ev.length, 3);
chk("read newest last", ev[ev.length - 1].map, "E1L2");

var feed = gl.event_feed(path, 10, function (e) {
	return e.type === "level" ? ("level " + e.map) : null;   // skip death
});
chk("feed skips non-level", feed.length, 2);
chk("feed newest first", feed[0], "level E1L2");

var kept = gl.prune_events(path, 2, 2);                // 3 lines > cap 2 -> keep 2
chk("prune kept", kept, 2);
chk("prune file now 2", gl.read_events(path, 10).length, 2);

chk("mmss", gl.mmss(125), "2:05");
writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
```

- [ ] **Step 2: run it to verify it FAILS**

Run: `cd /home/rswindell/sbbs && jsexec -c ctrl exec/load/tests/test_game_lobby_events.js 2>&1 | tail -12`
Expected: FAIL / error — `gl.read_events` etc. are not functions yet.

- [ ] **Step 3: implement the API in `game_lobby.js`**

Load `json_lines` once near the top of `game_lobby.js` (beside its other `load`s):
```js
var _json_lines = load({}, "json_lines.js");
```
Add these functions and include them in the returned object:
```js
function read_events(path, max) {
	if (!file_exists(path)) return [];
	var all = _json_lines.get(path, -(max * 6), 0, true);   // last-ish N, recover bad lines
	if (typeof all != "object") return [];                   // get() returns a string on error
	return (all.length > max) ? all.slice(all.length - max) : all;
}
function prune_events(path, cap, keep) {
	var all = _json_lines.get(path, 0, 0, true);
	if (typeof all != "object") return 0;
	if (all.length <= cap) return all.length;
	var tail = all.slice(all.length - keep), f = new File(path), i;
	if (!f.open("w+")) return all.length;
	for (i = 0; i < tail.length; i++) f.writeln(JSON.stringify(tail[i]));
	f.close();
	return tail.length;
}
function event_feed(path, max, fmt) {
	var ev = read_events(path, max), out = [], i, s;
	for (i = ev.length - 1; i >= 0; i--) {                   // most recent first
		s = fmt(ev[i]);
		if (s) out.push(s);
	}
	return out;
}
function ago(t) {
	var d = (time() - t);
	if (d < 60) return "just now";
	if (d < 3600) return Math.floor(d / 60) + "m ago";
	if (d < 86400) return Math.floor(d / 3600) + "h ago";
	return Math.floor(d / 86400) + "d ago";
}
function mmss(secs) {
	secs = Math.max(0, secs | 0);
	var m = Math.floor(secs / 60), s = secs % 60;
	return m + ":" + (s < 10 ? "0" : "") + s;
}
```
Add `read_events: read_events, prune_events: prune_events, event_feed: event_feed, ago: ago, mmss: mmss,` to the object literal `game_lobby.js` returns. (If it uses `file_exists`, it's a Synchronet global; keep using it as elsewhere in the file.)

- [ ] **Step 4: run the test to verify it PASSES**

Run: `cd /home/rswindell/sbbs && jsexec -c ctrl exec/load/tests/test_game_lobby_events.js 2>&1 | tail -12`
Expected: `ALL PASS`.

- [ ] **Step 5: confirm SyncDuke's lobby still loads game_lobby.js cleanly**

Run: `cd /home/rswindell/sbbs && jsexec -c ctrl -e 'var gl=load({}, "game_lobby.js"); print(typeof gl.read_events + " " + typeof gl.alloc_port);'`
Expected: `function function` (new API present, existing API intact).

- [ ] **Step 6: commit**

```bash
git add exec/load/game_lobby.js exec/load/tests/test_game_lobby_events.js
git commit -m "game_lobby: shared events.jsonl read/prune/feed + time helpers"
```

---

### Task 4: SyncDuke lobby wiring (`-eventlog`, formatter, activity view, prune)

**Files:** Modify `xtrn/syncduke/syncduke_lib.js`, `xtrn/syncduke/lobby.js`

**Interfaces consumed:** `gl.event_feed`, `gl.prune_events`, `gl.ago` from Task 3.

- [ ] **Step 1: `SD_EVENTS`, `-eventlog` in `sd_cmd`, and `sd_event_text` in `syncduke_lib.js`**

Add near `SD_GAMES`:
```js
var SD_EVENTS = system.data_dir + "syncduke/events.jsonl";   // door-written activity log
```
In `sd_cmd()`, append the arg to the command string (quote the path):
```js
	cmd += ' -eventlog "' + SD_EVENTS + '"';
```
Add the per-event formatter (returns null for unknown types so the feed skips them):
```js
function sd_event_text(e) {
	switch (e.type) {
		case "start": return e.user + " joined (" + (e.mode || "single") + ")";
		case "level": return e.user + " reached " + e.map;
		case "death": return e.user + " died on " + e.map;
	}
	return null;
}
```

- [ ] **Step 2: activity view + prune-on-entry in `lobby.js`**

Add a menu entry (e.g. key `A`) whose handler prints the feed:
```js
function sd_show_activity() {
	console.clear();
	console.print("\1h\1wSyncDuke (Nukem 3D) \1y- \1wrecent activity\1n\r\n\r\n");
	var feed = gl.event_feed(SD_EVENTS, 15, sd_event_text), i;
	if (!feed.length)
		console.print("\1n\1kNo recent activity.\1n\r\n");
	for (i = 0; i < feed.length; i++)
		console.print("\1k\1h" + gl.ago(/*ts via closure? no*/ 0) + "\1n");   // see Step 3
	console.pause();
}
```
(The exact rendering is refined in Step 3 — the feed formatter already produces the text; prepend the relative time there.)

- [ ] **Step 3: fold the timestamp into the formatter so the feed line is self-contained**

Change `sd_event_text` (Step 1) to include the age, so `event_feed` yields ready-to-print lines:
```js
function sd_event_text(e) {
	var age = "\1k\1h" + gl.ago(e.time) + "\1n ";
	switch (e.type) {
		case "start": return age + "\1w" + e.user + " joined (" + (e.mode || "single") + ")\1n";
		case "level": return age + "\1w" + e.user + " reached " + e.map + "\1n";
		case "death": return age + "\1w" + e.user + " died on " + e.map + "\1n";
	}
	return null;
}
```
And simplify `sd_show_activity()` to just print each feed line:
```js
function sd_show_activity() {
	console.clear();
	console.print("\1h\1wSyncDuke (Nukem 3D) \1y- \1wrecent activity\1n\r\n\r\n");
	var feed = gl.event_feed(SD_EVENTS, 15, sd_event_text), i;
	if (!feed.length)
		console.print("\1n\1kNo recent activity.\1n\r\n");
	for (i = 0; i < feed.length; i++)
		console.print(feed[i] + "\r\n");
	console.pause();
}
```
Wire the `A` key into the lobby's main menu prompt + dispatch (match how the existing options are registered), and prune on lobby entry (near where the menu first draws):
```js
	gl.prune_events(SD_EVENTS, 2000, 1000);
```

- [ ] **Step 4: parse/load check**

Run: `cd /home/rswindell/sbbs && jsexec -c ctrl -e 'load(js.exec_dir); print("ok");' 2>&1 | tail -3` — or, more directly, syntax-check both files:
`jsexec -c ctrl -e 'try{ var s=new File("xtrn/syncduke/syncduke_lib.js"); print("exists="+s.exists);}catch(e){print(e);}'`
Then confirm no SM1.8.5 syntax errors by loading `syncduke_lib.js` under jsexec from its dir if feasible; otherwise rely on the runtime pass. Expected: no syntax error.

- [ ] **Step 5: commit**

```bash
git add xtrn/syncduke/syncduke_lib.js xtrn/syncduke/lobby.js
git commit -m "syncduke lobby: recent-activity view via shared events feed"
```

---

### Task 5: Full validation

**Files:** none (validation only).

- [ ] **Step 1: door builds clean** — `cmake --build /home/rswindell/sbbs/src/doors/syncduke/build --target syncduke 2>&1 | tail -1` → `[100%] Built target syncduke`.
- [ ] **Step 2: shared JS test passes** — `cd /home/rswindell/sbbs && jsexec -c ctrl exec/load/tests/test_game_lobby_events.js 2>&1 | tail -1` → `ALL PASS`.
- [ ] **Step 3: SyncDuke lobby still links `game_lobby.js`** — `jsexec -c ctrl -e 'var gl=load({},"game_lobby.js"); print([typeof gl.read_events, typeof gl.event_feed, typeof gl.alloc_port].join(" "));'` → `function function function`.
- [ ] **Step 4: SyncDOOM untouched** — `git diff --stat 99ca50df41..HEAD -- src/doors/syncdoom xtrn/syncdoom` → empty (no SyncDOOM changes).
- [ ] **Step 5: runtime (user, live BBS):** launch SyncDuke via the lobby; confirm `data/syncduke/events.jsonl` gains a `start` line, a `level` line on each level change, and a `death` line on death; the lobby's "recent activity" view renders the feed (newest first, relative times); prune bounds the file.

---

## Self-review notes

- **Spec coverage:** door `-eventlog`+writer+start/level/death (T1/T2); `game_lobby.js` read/prune/feed/ago/mmss (T3); SyncDuke lobby `-eventlog`+formatter+activity+prune (T4); SyncDOOM untouched (constraint + T5 step 4); testing (T3 jsexec + T5 runtime). `start` term omits `desc` per the spec's stated deviation.
- **Type consistency:** `syncduke_eventlog_path`/`syncduke_home`/`syncduke_active_tier_name`/`syncduke_events_tick` used consistently across T1/T2/plat; `read_events`/`prune_events`/`event_feed`/`ago`/`mmss` names identical in T3 (impl+test) and T4 (consumer).
- **Confirm-at-impl:** `totalclock` type + `TICSPERSEC==120` (T1 step 1); the `syncduke_last_tier` symbol visibility for the tier-name accessor (T1 step 3); the lobby menu-key registration idiom (T4 step 3, match existing options).
- **JS dialect:** `.slice`/`switch`/`Math.floor` are SM1.8.5-safe; no `let`/`const`/arrow/templates used.
