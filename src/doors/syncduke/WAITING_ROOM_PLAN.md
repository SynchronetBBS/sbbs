# SyncDuke Multiplayer Waiting Room + Clean-Exit Safety Net — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the SyncDuke master's silent, blocking "Loading…"/60s-timeout-to-solo with an interactive JS waiting room (quit / who's-online / page, indefinite) plus a door safety net so a failed multiplayer connect exits cleanly instead of dropping into single-player.

**Architecture:** The lobby stops launching the master door immediately. `sd_create` writes the registry entry and enters a JS **waiting room** that heartbeats the entry (so joiners keep seeing it during an indefinite wait) and polls for a joiner's **`.claim` marker file**; it launches the master door only once claimed. `sd_join` drops the claim (instead of removing the entry) and launches. In the door, `initmultiplayers` exits cleanly when a configured net role can't complete its handshake, rather than falling back to `numplayers=1`.

**Tech Stack:** Synchronet JavaScript (SpiderMonkey 1.8.5) for the lobby; C (vendored Chocolate-Duke3D shim, built via `build.sh`) for the door. Full design in `src/doors/syncduke/WAITING_ROOM_DESIGN.md`.

## Global Constraints

- **JS is SpiderMonkey 1.8.5 only** — no `let`/`const`, arrow functions, template literals, `for..of`, `.includes`/`.find`. Use `var`, classic `for`, `.indexOf`, `.replace(/…/)`.
- **Registry files are `*.ini`** (`gl.write_game` writes `<dir><name>.ini`); the claim marker MUST use a **non-`.ini`** extension (`.claim`) so `gl.list_games`' `*.ini` glob never parses it as a game.
- **Single writer per file:** the master owns/writes the `.ini` entry (creating + heartbeating + removing); the joiner only writes the `.claim` marker. Never have both write the same file.
- **Registry freshness:** `gl.list_games` hides entries older than `[net] stale` (default 90s) and reaps past `stale*3`. An indefinite wait therefore REQUIRES the master to refresh its entry faster than `stale`.
- **Mode tokens** (from the committed dukematch work) are `"coop"` | `"dm"` | `"dmnospawn"`.
- **Portable C** (GCC/Clang first, then MSVC); no reserved identifiers; run **uncrustify** (`src/uncrustify.cfg`) on touched `.c`/`.h`; US English spelling.
- **Commit policy:** commit direct to master; end each message body with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`; do NOT push. Task 1 (model, headless-tested) commits on green; Tasks 2 & 3 (interactive UI + C) commit only AFTER the human's live 2-node test.

---

### Task 1: Registry model — heartbeat field + `.claim` coordination helpers (`syncduke_lib.js`)

Headless-testable model changes: give `sd_write_entry` a `heartbeat` field, and add the claim-marker helpers the waiting room and joiner use.

**Files:**
- Modify: `xtrn/syncduke/syncduke_lib.js` (`sd_write_entry` ~141; add helpers + one constant)
- Create: `src/doors/syncduke/tests/test_waiting_room_lib.js`

**Interfaces:**
- Consumes: `gl.write_game`/`gl.remove_game` (unchanged `game_lobby.js`), `File`, `file_exists`, `file_remove`, `time`.
- Produces (used by Task 2):
  - `sd_write_entry(cfg, port, level, mode)` — now also writes `heartbeat: time()` (re-calling it re-heartbeats). Returns the `.ini` path.
  - `sd_claim_path(entryPath)` — string transform `…/<stem>.ini` → `…/<stem>.claim`.
  - `sd_claim_game(entry)` — joiner writes the claim marker beside `entry.file`; returns true/false.
  - `sd_game_claimed(entryPath)` — master: has a joiner dropped the claim? (bool)
  - `sd_clear_game(entryPath)` — master: remove the `.ini` entry AND any `.claim`.
  - Constant `SD_WAIT_HEARTBEAT` (seconds between entry re-writes; `20`).

- [ ] **Step 1: Write the failing test**

Create `src/doors/syncduke/tests/test_waiting_room_lib.js`:

```js
// jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_waiting_room_lib.js
//
// Headless tests for the SyncDuke waiting-room registry model (syncduke_lib.js):
// the heartbeat field + the .claim coordination helpers. Loads the lib as a
// namespace object and stubs `user` (jsexec has none).
user = { number: 1, alias: "Duke" };

var lib = load({}, js.exec_dir + "../../../../xtrn/syncduke/syncduke_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}

mkpath(system.data_dir + "syncduke/games/");
var cfg = lib.sd_load_config();

// --- heartbeat field is written and fresh ---
var path = lib.sd_write_entry(cfg, 29050, 1, "coop");
chk("entry written", (path && file_exists(path)) ? "yes" : "no", "yes");
var f = new File(path); f.open("r"); var g = f.iniGetObject(); f.close();
var hb = parseInt(g.heartbeat, 10);
chk("heartbeat present + recent", (hb > 0 && (time() - hb) < 5) ? "ok" : ("bad:" + g.heartbeat), "ok");

// --- claim path transform ---
chk("claim path transform", lib.sd_claim_path(path), path.replace(/\.ini$/, ".claim"));

// --- claim round-trip: not claimed -> claim -> claimed; second claim rejected ---
chk("not claimed initially", lib.sd_game_claimed(path), "false");
chk("claim write ok", lib.sd_claim_game({ file: path }), "true");
chk("claimed after claim", lib.sd_game_claimed(path), "true");
chk("second claim rejected (exclusive)", lib.sd_claim_game({ file: path }), "false");

// --- clear removes BOTH entry and claim, releasing the exclusive lock ---
lib.sd_clear_game(path);
chk("entry removed", file_exists(path), "false");
chk("claim removed", file_exists(lib.sd_claim_path(path)), "false");

// --- heartbeat re-write advances the timestamp on the same file ---
var p2 = lib.sd_write_entry(cfg, 29051, 1, "dm");
var f2 = new File(p2); f2.open("r"); var g2a = f2.iniGetObject(); f2.close();
lib.sd_write_entry(cfg, 29051, 1, "dm");                 // re-heartbeat (same stem/path)
var f3 = new File(p2); f3.open("r"); var g2b = f3.iniGetObject(); f3.close();
chk("re-write keeps same path", p2, lib.sd_write_entry(cfg, 29051, 1, "dm"));
chk("heartbeat monotonic", (parseInt(g2b.heartbeat,10) >= parseInt(g2a.heartbeat,10)) ? "ok" : "back", "ok");
lib.sd_clear_game(p2);

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_waiting_room_lib.js`
Expected: FAIL — `g.heartbeat` is `undefined`, and `sd_claim_path`/`sd_claim_game`/`sd_game_claimed`/`sd_clear_game` are not defined (ReferenceError or `undefined` results).

- [ ] **Step 3: Add the `heartbeat` field to `sd_write_entry`**

In `xtrn/syncduke/syncduke_lib.js`, add `heartbeat: time(),` to the object `sd_write_entry` passes to `gl.write_game` (place it right after `created: time()`), so the current function's `gl.write_game(...)` object becomes:

```js
	return gl.write_game(SD_GAMES, sd_entry_name(port), {
		host:      (typeof user != "undefined" && user.alias) ? user.alias : "Someone",
		addr:      gl.advertise_addr(cfg.net),     // "" => same-host (joiner uses 127.0.0.1)
		port:      port,
		level:     lev.num,
		levelname: lev.name,
		mode:      mode || "coop",                 // "coop" | "dm" | "dmnospawn"
		status:    "waiting",
		players:   1,
		maxplayers: 2,
		created:   time(),
		heartbeat: time()                          // refreshed each re-write; keeps the
		                                           // entry visible past [net] stale during
		                                           // the master's indefinite wait
	});
```

(`gl.entry_age` prefers a `heartbeat` field over file mtime, so re-calling `sd_write_entry` for the same port refreshes freshness robustly, including over a shared mount.)

- [ ] **Step 4: Add the constant + claim helpers**

In `xtrn/syncduke/syncduke_lib.js`, near the `SD_GAMES`/`SD_EVENTS` definitions add the constant:

```js
// Seconds between the waiting room's registry re-writes (heartbeat). Must be well
// under [net] stale (default 90) so a joiner keeps seeing the game during a long wait.
var SD_WAIT_HEARTBEAT = 20;
```

Then add these helpers (put them right after `sd_write_entry`):

```js
// The claim-marker path for a registry entry: same stem, ".claim" instead of ".ini".
// A separate file (not the ".ini") so the master's heartbeat re-writes and the
// joiner's claim never write the same file (no two-writer clobber).
function sd_claim_path(entryPath) {
	return String(entryPath).replace(/\.ini$/i, ".claim");
}

// Joiner: drop the claim marker beside the master's entry (identified by entry.file,
// as returned by gl.list_games). Opened "wx" -- EXCLUSIVE create (O_EXCL, js_file.cpp):
// the open fails if the file already exists, so the FIRST joiner to claim wins
// atomically and any later joiner gets false (game already taken) and can bail without
// launching a doomed door. (Note: 'x' is the working exclusive flag; the legacy 'e'
// char is deprecated/no-op in Synchronet.) The master polls for the marker then
// launches. Returns true if WE claimed it, false if already claimed or on error.
function sd_claim_game(entry) {
	var f = new File(sd_claim_path(entry.file));
	if (!f.open("wx"))                     // exclusive create -> false if already claimed
		return false;
	f.write("claimed " + time() + "\n");
	f.close();
	return true;
}

// Master: has a joiner claimed our game (dropped the marker) yet?
function sd_game_claimed(entryPath) {
	return file_exists(sd_claim_path(entryPath));
}

// Master: end the match/cancel -- remove our entry AND any claim marker (idempotent).
function sd_clear_game(entryPath) {
	var c = sd_claim_path(entryPath);
	if (file_exists(c))
		file_remove(c);
	gl.remove_game(entryPath);
}
```

- [ ] **Step 5: Run the test to verify it passes**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_waiting_room_lib.js`
Expected: PASS — final line `ALL PASS`, exit 0.

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncduke/syncduke_lib.js src/doors/syncduke/tests/test_waiting_room_lib.js
git commit -m "syncduke: registry heartbeat + .claim coordination helpers for the waiting room

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: JS waiting room + create/join rewiring (`lobby.js`)

The interactive waiting room and the create/join flow changes. Console UI — not unit-tested; verified by reading + the live 2-node test.

**Files:**
- Modify: `xtrn/syncduke/lobby.js` (`sd_draw_panel` ~231; `sd_create` ~106; `sd_join` ~189–190; add `sd_wait_bg` + `sd_wait_room`)

**Interfaces:**
- Consumes (from Task 1): `sd_write_entry`, `sd_claim_game(entry)` (exclusive; false if already claimed), `sd_game_claimed(entryPath)`, `sd_clear_game(entryPath)`, `sd_claim_path(entryPath)`, `SD_WAIT_HEARTBEAT`, `SD_GAMES`, `sd_entry_name(port)`. Also existing `sd_draw_art`, `sd_draw_panel`, `sd_panel_rows_prev`, `sd_prompt_page`, `sd_send_pages`, `cfg`, `sd_cmd`, `sd_play`, and the globals `file_exists`/`file_remove`.
- Produces: no new exports (lobby.js is the entry point).

- [ ] **Step 1: Make `sd_draw_panel` accept an optional background-redraw fn**

`sd_draw_panel` currently calls `sd_draw_art()` when the panel height changes — which would wipe the waiting room's own header. Give it an optional `bgfn` (defaulting to `sd_draw_art`). Replace the current function head:

```js
var sd_panel_rows_prev = -1;
function sd_draw_panel() {
	var cols = console.screen_columns, rows = console.screen_rows;
	var p = sd_panel_cells(cols, gl.activity_max_age(cfg), gl.panel_rows(cfg));
	var cells = p.cells, nrows = cells.length;
	if (sd_panel_rows_prev >= 0 && nrows != sd_panel_rows_prev)
		sd_draw_art();                       // height changed -> restore art under it
	sd_panel_rows_prev = nrows;
```

with (only the signature line and the height-change line change):

```js
var sd_panel_rows_prev = -1;
function sd_draw_panel(bgfn) {
	var cols = console.screen_columns, rows = console.screen_rows;
	var p = sd_panel_cells(cols, gl.activity_max_age(cfg), gl.panel_rows(cfg));
	var cells = p.cells, nrows = cells.length;
	if (sd_panel_rows_prev >= 0 && nrows != sd_panel_rows_prev)
		(bgfn || sd_draw_art)();             // height changed -> restore background
	sd_panel_rows_prev = nrows;
```

(The existing menu caller `sd_lobby_wait` calls `sd_draw_panel()` with no argument, so it still redraws the lobby art — unchanged behavior.)

- [ ] **Step 2: Add `sd_wait_bg` and `sd_wait_room`**

Add both functions just above `sd_main` (after `sd_lobby_wait`):

```js
// The waiting room's static background: title + what's being hosted + the key hints.
// The live who's-online + activity panel (sd_draw_panel) paints below it and refreshes
// each tick; this bg is redrawn only on entry and when the panel height changes.
function sd_wait_bg(lev, modelabel) {
	console.clear();
	console.print("\1n\1h\1cSyncDuke \1y-\1n\1h\1c Waiting Room\1n\r\n\r\n");
	console.print(" Hosting \1h\1wE1L" + lev.num + " " + lev.name + "\1n \1c(" + modelabel + ")\1n.\r\n");
	console.print(" \1h\1yWaiting for a player to join...\1n\r\n");
	console.print("\r\n \1hQ\1n cancel    \1hP\1n page nodes\r\n");
}

// The master's waiting room: heartbeat the registry entry so joiners keep seeing it,
// show the live who's-online panel, and wait -- indefinitely -- for a joiner to drop
// the claim marker. Returns "joined" (launch the door) or "cancel" (host Q, hangup, or
// telegram/interrupt exit). Mirrors sd_lobby_wait's nodesync/Ctrl-P/panel machinery.
function sd_wait_room(entry, port, lev, mode) {
	var modelabel = (mode == "coop") ? "co-op" : "dukematch";
	var mynode = bbs.node_num;
	var lastbeat = time();
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";          // keep Ctrl-P (paging) with the BBS
	sd_panel_rows_prev = -1;
	var bg = function () { sd_wait_bg(lev, modelabel); };
	bg();
	try {
		while (!js.terminated && bbs.online) {
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {                    // a waiting telegram prints to the screen
				console.clear();
				bbs.nodesync();
				console.pause();
				bg();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();               // sync node record / handle interrupt
			if (js.terminated || !bbs.online)
				return "cancel";
			if (sd_game_claimed(entry))       // a joiner dropped the claim -> go
				return "joined";
			if (time() - lastbeat >= SD_WAIT_HEARTBEAT) {   // keep the entry fresh/visible
				sd_write_entry(cfg, port, lev.num, mode);
				lastbeat = time();
			}
			sd_draw_panel(bg);
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (!c)
				continue;                    // timeout -> refresh
			if (c == "Q")
				return "cancel";
			if (c == "P") {                  // page more nodes, then keep waiting
				var targets = sd_prompt_page();
				sd_send_pages(targets, lev, modelabel);
				console.pause();
				bg();
				sd_panel_rows_prev = -1;
			}
			// any other key -> ignore, keep waiting
		}
		return "cancel";
	} finally {
		console.ctrlkey_passthru = oldctrl;
	}
}
```

- [ ] **Step 3: Rewire `sd_create` to use the waiting room**

In `sd_create`, replace everything from the `var entry = sd_write_entry(...)` line through the end of the function:

```js
	var entry = sd_write_entry(cfg, port, lev.num, mode);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}

	sd_send_pages(page_targets, lev, modelabel);
	console.print("\r\n\1h\1gGame created:\1n \1wE1L" + lev.num + " " + lev.name
	    + "\1n \1n\1c(" + modelabel + ")\1n. Entering the waiting room; another player can \1hJ\1noin.\r\n");

	// The creator's door IS the master (player 0). It binds and waits for a joiner,
	// then both play. We hold the registry entry for the match's lifetime and clear
	// it on exit (clean unwind -> finally; covers disconnect, timeout, normal quit).
	try {
		sd_play(sd_cmd("master", port, null, lev.num, mode, cfg.dukematch));
	} finally {
		gl.remove_game(entry);
	}
}
```

with (clears a stale claim BEFORE the entry is listed, then gates the launch on a real claim):

```js
	// Clear any stale claim marker left by a prior game that reused this port/stem,
	// BEFORE writing the entry -- so a real joiner (who can only claim once the entry
	// below is listed) can never race this cleanup, and the room can't mistake an old
	// marker for a joiner.
	var stale_claim = SD_GAMES + sd_entry_name(port) + ".claim";
	if (file_exists(stale_claim))
		file_remove(stale_claim);

	var entry = sd_write_entry(cfg, port, lev.num, mode);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}

	sd_send_pages(page_targets, lev, modelabel);

	// Enter the JS waiting room (indefinite): it heartbeats the entry so joiners keep
	// seeing it, shows who's online, and lets the host Q-cancel or P-page. It returns
	// "joined" only once a joiner drops the claim marker -- and ONLY THEN do we launch
	// the master door (so its handshake connects in ~1-2s instead of hanging). On
	// cancel/hangup we tear the game down without ever launching (no silent solo game).
	if (sd_wait_room(entry, port, lev, mode) != "joined") {
		sd_clear_game(entry);
		return;
	}
	try {
		sd_play(sd_cmd("master", port, null, lev.num, mode, cfg.dukematch));
	} finally {
		sd_clear_game(entry);
	}
}
```

- [ ] **Step 4: Rewire `sd_join` to claim instead of removing the entry**

In `sd_join`, replace the final two lines:

```js
	// Claim the only other slot: remove the entry so a third node doesn't also try to
	// join this strictly-2-player match (the master accepts just one join anyway).
	gl.remove_game(sel.file);
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level, sel.mode || "coop", cfg.dukematch));
```

with:

```js
	// Claim the game by dropping a marker file beside the master's entry (a separate
	// file, so the master's heartbeat re-writes never clobber it). The claim is an
	// EXCLUSIVE create: if it returns false, another node claimed this game a moment
	// ago -- bail cleanly instead of launching a door that can't connect. We do NOT
	// remove the entry (the master owns its lifecycle + clears it on exit).
	if (!sd_claim_game(sel)) {
		console.print("\r\n\1h\1wThat game was just claimed by someone else.\1n\r\n");
		console.pause();
		return;
	}
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level, sel.mode || "coop", cfg.dukematch));
```

- [ ] **Step 5: Syntax-check the lobby parses**

Run: `jsexec -U -C /home/rswindell/sbbs/xtrn/syncduke/lobby.js`
Expected: no SpiderMonkey `SyntaxError`. It will error at RUNTIME on an undefined `bbs`/`console` under jsexec (expected — proves the file parses). (`jsexec -e` does not work in this build; `-C` = compile/syntax check, `-U` runs it without a live session.)

- [ ] **Step 6: Live validation (REQUIRED before commit — human runs it)**

Deploy the lobby JS, then two BBS nodes:
- Node A: `C`reate a game (co-op) → the **Waiting Room** appears: title, "Waiting for a player to join…", who's-online panel updating, `Q`/`P` hints. Leave it sitting >90s and confirm node B can STILL see + join it (heartbeat works).
- Node B: `J`oin it → node A's room detects the claim and launches; both connect within ~1–2s and play. Repeat with a **dukematch** game.
- Node A: press **Q** in the room → returns to the menu, game removed (node B no longer sees it). Press **P** → pages nodes, then keeps waiting.
**Hand this to the human; do not claim done from static reasoning.**

- [ ] **Step 7: Commit** (after the human confirms the live test)

```bash
cd /home/rswindell/sbbs
git add xtrn/syncduke/lobby.js
git commit -m "syncduke: interactive multiplayer waiting room (heartbeat + claim-poll, Q/P), no silent solo

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Door clean-exit safety net (`syncduke_net.c`)

When the lobby launches the door with a net role but the handshake can't complete, exit cleanly instead of dropping the host into a solo game.

**Files:**
- Modify: `src/doors/syncduke/syncduke_net.c` (`initmultiplayers` ~198)

**Interfaces:**
- Consumes: `sd_net_handshake(role)` (returns 1 connected / 0 failed — existing), `syncduke_log` (existing), the resolved `role`.
- Produces: no new symbols; behavior change only (net-role handshake failure → `_exit(0)`).

- [ ] **Step 1: Change the failure branch in `initmultiplayers`**

In `src/doors/syncduke/syncduke_net.c`, the current tail of `initmultiplayers` is:

```c
	/* default single-player unless a net role is configured */
	numplayers = 1; myconnectindex = 0;
	connecthead = 0; connectpoint2[0] = -1;
	if (role && (strcmp(role, "master") == 0 || strcmp(role, "join") == 0))
		sd_net_handshake(role);
}
```

Replace the `if (role …)` line with a checked call that exits cleanly on failure:

```c
	/* default single-player unless a net role is configured */
	numplayers = 1; myconnectindex = 0;
	connecthead = 0; connectpoint2[0] = -1;
	if (role && (strcmp(role, "master") == 0 || strcmp(role, "join") == 0)) {
		if (!sd_net_handshake(role)) {
			/* The lobby launched us for a MULTIPLAYER game (master/join) but the peer
			 * never connected.  Exit cleanly (freeing the BBS node) instead of silently
			 * dropping the host into a single-player match -- a lone game is never what
			 * a Create/Join asked for.  A true single-player launch has no net role and
			 * never reaches here, so it still plays solo.  _exit(0) matches the door's
			 * hangup path (syncduke_door.c): skip the engine's atexit cleanup, which
			 * could block on the socket. */
			syncduke_log("net: multiplayer requested (role=%s) but no peer connected -- exiting (no solo fallback)", role);
			fprintf(stderr, "syncduke: multiplayer peer did not connect -- exiting\n");
			fflush(stderr);
			_exit(0);
		}
	}
}
```

Ensure `unistd.h` (for `_exit`) is included on the non-Windows path — it already is (the file includes `<unistd.h>` under `#else`). On Windows, `_exit` is provided by `<stdlib.h>`/`<process.h>`; the file already includes `<stdlib.h>`. If a build error names `_exit`, add `#include <stdlib.h>` at the top (it is almost certainly already present).

- [ ] **Step 2: Build (GCC/Clang first)**

Run: `cd /home/rswindell/sbbs/src/doors/syncduke && ./build.sh`
Expected: clean build of the door binary, no new warnings on `syncduke_net.c`. (Do NOT `make install` — the user deploys.)

- [ ] **Step 3: Uncrustify the touched file**

Run: `cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_net.c`
Then re-run `./build.sh` to confirm it still compiles.
Expected: reformatted in place (tabs), build still clean.

- [ ] **Step 4: Live validation (REQUIRED before commit — human runs it)**

Two nodes: Node A Creates and (via Task 2's room) node B Joins, but **hang up node B before it connects** (or join then immediately drop). Confirm node A's master door **exits back to the lobby**, NOT into a solo game. Separately confirm a normal single-player launch (main-menu **P = play solo**, no net role) still plays. **Hand to the human.**

- [ ] **Step 5: Commit** (after the human confirms the live test)

```bash
cd /home/rswindell/sbbs
git add src/doors/syncduke/syncduke_net.c
git commit -m "syncduke: door exits cleanly when a multiplayer peer never connects (no solo fallback)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the executor

- **Order:** Task 1 (model, commit on green) → Task 2 (waiting room, consumes Task 1) → Task 3 (C safety net, independent of 1/2). Tasks 2 and 3 share ONE combined live 2-node session; batch that validation, but each still commits only after the human confirms.
- **This builds on the committed dukematch work** (`sd_create`/`sd_join`/`sd_cmd` already thread `mode`/`cfg.dukematch`; `sd_write_entry` already carries `mode`). Do not revert any of that.
- **`game_lobby.js` stays unchanged** — all new state lives in `syncduke_lib.js`/`lobby.js` and the `.claim` files. If you find yourself editing `game_lobby.js`, stop and reconsider.
- **Two-writer rule:** never make the joiner write the `.ini` entry — only the `.claim`. The master is the sole `.ini` writer (create/heartbeat/remove). This is the whole reason the claim is a separate file.
- **Do not push.**
```
