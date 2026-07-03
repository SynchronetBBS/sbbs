# SyncDOOM JS Waiting Room (Lobby Muster) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace SyncDOOM's create/join (creator + joiners connect immediately and wait at Doom's C netgame-sync screen) with a JS lobby muster: players assemble in a JS waiting room and their clients launch only once the muster completes.

**Architecture:** The creator's lobby writes a heartbeated "mustering" registry entry and hosts a JS waiting room; joiners register with a per-node `.wait` marker and wait in their own JS room. On Start (≥2 assembled) or auto at the target, the creator spawns the detached server **without `-gamesdir`** (so it writes no competing registry entry) with `-maxplayers K`, writes a `.go` marker, and launches its own client first (→ controller). Joiners see `.go` and launch. No engine/netcode change.

**Tech Stack:** Synchronet JavaScript (SpiderMonkey 1.8.5). Shared muster primitives go in `exec/load/game_lobby.js`; SyncDOOM specifics in `xtrn/syncdoom/{syncdoom_lib.js,lobby.js}`. Full design in `src/doors/syncdoom/WAITING_ROOM_DESIGN.md`. Reference implementation: the just-shipped SyncDuke waiting room (`xtrn/syncduke/lobby.js` `sd_wait_room`, `syncduke_lib.js` `.claim` helpers).

## Global Constraints

- **SpiderMonkey 1.8.5 only** — no `let`/`const`, arrow functions, template literals, `for..of`, `.includes`/`.find`. Use `var`, classic `for`, `.indexOf`, `.replace(/…/)`. Classic function expressions (`var f = function(){…}`) are allowed.
- **No engine/netcode (`src/doors/syncdoom/*.c`) change.** The server is used as-is; the muster spawns it **without `-gamesdir`**.
- **Marker files are non-`.ini`** (`.wait.<node>`, `.go`) so `gl.list_games`' `*.ini` glob never parses them. One writer per file: creator owns `.ini`+`.go`; each joiner owns only its `.wait.<node>`.
- **Exclusive create** for a joiner's `.wait` marker (mode `"wx"` → `O_EXCL`; `'x'` is the working flag, legacy `'e'` is deprecated/no-op).
- **The mustering entry reports `players ≥ 1`** (creator counts as 1) so `sd_list_games`' `players < 1` filter keeps it visible.
- **Start-early == auto-complete:** at "go", `K` = assembled count; the server gets `-maxplayers K` and the creator's client gets `-players K` (mirrors how `sd_create` pairs `-players`/`-maxplayers` today).
- **`game_lobby.js` changes are additive** (new functions only) — must not alter existing behavior or SyncDuke.
- **Commit policy:** commit direct to master; end each message body with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`; do NOT push. Tasks 1 & 2 (headless-tested model) commit on green; Tasks 3 & 4 (interactive UI) commit only after the human's live multi-node test.

---

### Task 1: Shared muster primitives (`game_lobby.js`)

Generic, headless-testable file-coordination helpers for an N-player muster: per-node waiter markers, a go marker, and a clear-all. Additive — no existing function changes.

**Files:**
- Modify: `exec/load/game_lobby.js` (add functions near the other registry helpers, ~line 245)
- Create: `exec/load/tests/test_game_lobby_muster.js`

**Interfaces:**
- Consumes: globals `File`, `directory`, `file_exists`, `file_remove`; existing `remove_game`.
- Produces (used by Tasks 2–4):
  - `waiter_path(entryPath, node)` → the `<stem>.wait.<node>` path string.
  - `write_waiter(entryPath, node, alias)` → bool (exclusive create; false if this node already has a marker or on error).
  - `list_waiters(entryPath)` → array of `{node: "<n>", alias: "<a>"}` (one per marker).
  - `remove_waiter(entryPath, node)` → bool (idempotent).
  - `go_path(entryPath)` → the `<stem>.go` path string.
  - `write_go(entryPath, addr)` → bool. `read_go(entryPath)` → addr string, or null if absent.
  - `clear_muster(entryPath)` → removes the `.go`, all `.wait.*`, and the `.ini` (via `remove_game`).

- [ ] **Step 1: Write the failing test**

Create `exec/load/tests/test_game_lobby_muster.js`:

```js
// jsexec exec/load/tests/test_game_lobby_muster.js
var gl = load({}, "game_lobby.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
var dir = backslash(system.temp_dir + "gl_muster_test");
mkpath(dir);
var entry = gl.write_game(dir, "host-25000", { status: "mustering", port: 25000, players: 1 });
chk("entry written", (entry && file_exists(entry)) ? "yes" : "no", "yes");

// waiter markers
chk("no waiters initially", gl.list_waiters(entry).length, 0);
chk("write waiter ok", gl.write_waiter(entry, 3, "Bob"), "true");
chk("same node rejected (exclusive)", gl.write_waiter(entry, 3, "Bob2"), "false");
gl.write_waiter(entry, 5, "Alice");
var w = gl.list_waiters(entry);
chk("two waiters", w.length, 2);
function aliasOf(list, node) { for (var i = 0; i < list.length; i++) if (String(list[i].node) === String(node)) return list[i].alias; return null; }
chk("waiter 3 alias", aliasOf(w, 3), "Bob");
chk("waiter 5 alias", aliasOf(w, 5), "Alice");
gl.remove_waiter(entry, 3);
chk("after remove one", gl.list_waiters(entry).length, 1);

// go marker
chk("no go initially", gl.read_go(entry), "null");
chk("write go ok", gl.write_go(entry, "127.0.0.1:25000"), "true");
chk("read go", gl.read_go(entry), "127.0.0.1:25000");

// clear removes everything
gl.clear_muster(entry);
chk("clear removes go", gl.read_go(entry), "null");
chk("clear removes waiters", gl.list_waiters(entry).length, 0);
chk("clear removes entry", file_exists(entry), "false");

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `jsexec exec/load/tests/test_game_lobby_muster.js`
Expected: FAIL — `gl.write_waiter`/`list_waiters`/`read_go`/etc. are undefined.

- [ ] **Step 3: Add the primitives to `game_lobby.js`**

In `exec/load/game_lobby.js`, immediately after the `remove_game` function, add:

```js
// --- N-player muster coordination (used by a lobby-driven waiting room) ---------
// A muster is a lobby-owned game entry (<stem>.ini) plus two marker kinds beside it:
//   <stem>.wait.<node>  -- one per joining node (joiner-owned); the host counts/lists them
//   <stem>.go           -- the host's launch signal, carrying the server address
// Markers use non-.ini names so list_games() never parses them. One writer per file.

// Path of a joining node's waiter marker for a game entry.
function waiter_path(entryPath, node) {
	return String(entryPath).replace(/\.ini$/i, ".wait." + node);
}

// Joiner: register in the muster (exclusive create -- one marker per node; false if
// this node already registered or on error). `alias` is stored for the host's list.
function write_waiter(entryPath, node, alias) {
	var f = new File(waiter_path(entryPath, node));
	if (!f.open("wx"))
		return false;
	f.write((alias || "") + "\n");
	f.close();
	return true;
}

// Host: the registered waiters as [{node, alias}] (one per <stem>.wait.* marker).
function list_waiters(entryPath) {
	var base = String(entryPath).replace(/\.ini$/i, ".wait.");
	var files = directory(base + "*"), out = [], i;
	for (i = 0; i < files.length; i++) {
		var node = files[i].substr(base.length);
		var alias = "";
		var f = new File(files[i]);
		if (f.open("r")) { alias = f.readln() || ""; f.close(); }
		out.push({ node: node, alias: alias });
	}
	return out;
}

// Joiner: drop our marker (leaving the muster). Idempotent.
function remove_waiter(entryPath, node) {
	var p = waiter_path(entryPath, node);
	if (file_exists(p))
		return file_remove(p);
	return true;
}

// Path of a game's go marker.
function go_path(entryPath) {
	return String(entryPath).replace(/\.ini$/i, ".go");
}

// Host: signal launch, carrying the server address joiners dial.
function write_go(entryPath, addr) {
	var f = new File(go_path(entryPath));
	if (!f.open("w+"))
		return false;
	f.write((addr || "") + "\n");
	f.close();
	return true;
}

// Joiner: the go address if the host has fired, else null.
function read_go(entryPath) {
	var p = go_path(entryPath);
	if (!file_exists(p))
		return null;
	var f = new File(p), addr = "";
	if (f.open("r")) { addr = f.readln() || ""; f.close(); }
	return addr;
}

// Host: tear down the whole muster -- the go marker, every waiter marker, the entry.
function clear_muster(entryPath) {
	var g = go_path(entryPath);
	if (file_exists(g))
		file_remove(g);
	var base = String(entryPath).replace(/\.ini$/i, ".wait.");
	var w = directory(base + "*"), i;
	for (i = 0; i < w.length; i++)
		file_remove(w[i]);
	remove_game(entryPath);
}
```

No export-list edit is needed: `game_lobby.js` ends with a bare `this;`, so every
top-level `function` declaration is automatically a property of the object
`load({}, "game_lobby.js")` returns (that's how `write_game`/`list_games`/etc. are
already exported). Declaring these eight functions at top level is sufficient —
`gl.write_waiter(...)` etc. resolve with no further change.

- [ ] **Step 4: Run the test to verify it passes**

Run: `jsexec exec/load/tests/test_game_lobby_muster.js`
Expected: PASS — final line `ALL PASS`, exit 0.

- [ ] **Step 5: Confirm SyncDuke's existing game_lobby test still passes (no regression)**

Run: `jsexec exec/load/tests/test_game_lobby_events.js`
Expected: `ALL PASS` (the additions are new functions; nothing existing changed).

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs
git add exec/load/game_lobby.js exec/load/tests/test_game_lobby_muster.js
git commit -m "game_lobby: N-player muster primitives (waiter markers, go signal, clear)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: SyncDOOM muster model (`syncdoom_lib.js`)

The SyncDOOM-side model: write/refresh the mustering entry, count assembled players, and surface `mustering` games in the browse list.

**Files:**
- Modify: `xtrn/syncdoom/syncdoom_lib.js` (add helpers + one constant near `SD_GAMES` ~line 31; extend `sd_list_games` ~250)
- Create: `src/doors/syncdoom/tests/test_muster_lib.js`

**Interfaces:**
- Consumes (from Task 1): `gl.write_game`, `gl.list_waiters`, `gl.list_games`.
- Produces (used by Tasks 3–4):
  - `SD_MUSTER_HEARTBEAT` = 20 (seconds between mustering-entry re-writes).
  - `sd_muster_stem(port)` → `"<hostid>-<port>"` stem string (host id + port).
  - `sd_write_muster(cfg, port, ws, mode, target, players)` → the entry `.ini` path (or null). Writes `status:"mustering"`, `host`, `wadset`, `mode`, `addr`, `port`, `target`, `maxplayers`, `players`, `heartbeat`.
  - `sd_muster_players(entryPath)` → assembled count = `gl.list_waiters(entryPath).length + 1` (the +1 is the host).
  - `sd_list_games(cfg)` — unchanged signature; now also returns `mustering` entries.

- [ ] **Step 1: Write the failing test**

Create `src/doors/syncdoom/tests/test_muster_lib.js`:

```js
// jsexec /home/rswindell/sbbs/src/doors/syncdoom/tests/test_muster_lib.js
user = { number: 1, alias: "Host" };
var lib = load({}, js.exec_dir + "../../../../xtrn/syncdoom/syncdoom_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
mkpath(system.data_dir + "syncdoom/games/");
var cfg = lib.sd_load_config();
var ws = { id: "shareware", name: "DOOM Shareware", maxplayers: 4 };

var entry = lib.sd_write_muster(cfg, 26000, ws, "deathmatch", 4, 1);
chk("muster entry written", (entry && file_exists(entry)) ? "yes" : "no", "yes");
var f = new File(entry); f.open("r"); var g = f.iniGetObject(); f.close();
chk("status mustering", g.status, "mustering");
chk("mode carried", g.mode, "deathmatch");
chk("target carried", g.target, "4");
chk("players >=1 (host)", (parseInt(g.players, 10) >= 1) ? "ok" : "bad", "ok");
chk("heartbeat present", (parseInt(g.heartbeat, 10) > 0) ? "ok" : "bad", "ok");

// assembled count = waiters + 1
chk("players() with 0 waiters", lib.sd_muster_players(entry), 1);
var gl2 = load({}, "game_lobby.js");
gl2.write_waiter(entry, 7, "Bob");
chk("players() with 1 waiter", lib.sd_muster_players(entry), 2);

// sd_list_games surfaces the mustering entry (players>=1)
var games = lib.sd_list_games(cfg);
function byPort(list, p) { for (var i = 0; i < list.length; i++) if (String(list[i].port) === String(p)) return list[i]; return null; }
var e = byPort(games, 26000);
chk("mustering listed", e ? e.status : "(missing)", "mustering");

gl2.clear_muster(entry);
writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncdoom/tests/test_muster_lib.js`
Expected: FAIL — `sd_write_muster`/`sd_muster_players`/`SD_MUSTER_HEARTBEAT` undefined.

- [ ] **Step 3: Add the constant + muster-entry helpers**

In `xtrn/syncdoom/syncdoom_lib.js`, after the `SD_GAMES`/`SD_EVENTS` definitions add:

```js
// Seconds between the host's mustering-entry re-writes (heartbeat). Under [net] stale
// (default 90) so joiners keep seeing the game while players assemble.
var SD_MUSTER_HEARTBEAT = 20;
```

Then add these functions (place them just before `sd_list_games`):

```js
// A registry stem unique across same-LAN hosts sharing the games dir: <hostid>-<port>,
// hostid = the sanitized local host name (matches the C server's mp_host_id scheme).
function sd_muster_stem(port) {
	var host = String(system.local_host_name || "host").replace(/[^A-Za-z0-9]+/g, "");
	return host + "-" + port;
}

// Write/refresh the lobby-owned "mustering" entry (single writer: the host). `players`
// is the assembled count INCLUDING the host (>=1) so sd_list_games' players<1 filter
// keeps it visible. Re-calling refreshes the heartbeat + player count. Returns the path.
function sd_write_muster(cfg, port, ws, mode, target, players) {
	var maxp = parseInt(ws.maxplayers || cfg.net.max_players, 10) || 4;
	if (maxp > 4) maxp = 4;
	return gl.write_game(SD_GAMES, sd_muster_stem(port), {
		host:       (typeof user != "undefined" && user.alias) ? user.alias : "Someone",
		wadset:     ws.id,
		mode:       mode,
		addr:       gl.advertise_addr(cfg.net) || "127.0.0.1",   // joiners dial this:port
		port:       port,
		target:     target,
		maxplayers: maxp,
		players:    (players > 0) ? players : 1,
		status:     "mustering",
		heartbeat:  time()
	});
}

// Assembled count for a mustering entry = registered waiters + 1 (the host).
function sd_muster_players(entryPath) {
	return gl.list_waiters(entryPath).length + 1;
}
```

- [ ] **Step 4: Surface `mustering` entries in `sd_list_games`**

`sd_list_games` currently drops entries with `players < 1`. A mustering entry always
reports `players >= 1`, so it already passes — **no filter change needed**. Confirm by
re-reading `sd_list_games`; if it additionally filtered on `status`, relax it to include
`"mustering"`. (As of this writing it filters only `g.port === undefined` and
`players < 1`, so leave it unchanged and rely on the test to prove the entry lists.)

- [ ] **Step 5: Run the test to verify it passes**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncdoom/tests/test_muster_lib.js`
Expected: PASS — `ALL PASS`, exit 0.

- [ ] **Step 6: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncdoom/syncdoom_lib.js src/doors/syncdoom/tests/test_muster_lib.js
git commit -m "syncdoom: muster-entry model (write/refresh + assembled-count helpers)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Host side — server-spawn variant, go, host waiting room, create rewiring (`lobby.js`)

The creator's path: spawn the server without a registry entry, the go step, the host waiting room, and `sd_create` rewired to muster. Interactive — not unit-tested; verified by reading + the live test.

**Files:**
- Modify: `xtrn/syncdoom/lobby.js` (`sd_spawn_server` ~71; add `sd_muster_go` + `sd_muster_host`; `sd_create` ~309)

**Interfaces:**
- Consumes: `sd_write_muster`, `sd_muster_players`, `SD_MUSTER_HEARTBEAT`, `sd_muster_stem` (Task 2); `gl.list_waiters`, `gl.write_go`, `gl.clear_muster`, `gl.creator_connect_host` (Task 1 + existing); existing `sd_play`, `sd_spawn_server`, `sd_draw_panel`, `sd_panel_rows_prev`, `sd_prompt_page_players`, `sd_send_pages`, `sd_pick_gamemode`, `sd_pick_wadset`, `sd_pick_skill`, `sd_default_skill`, `sd_wadset_args`, `cfg`.
- Produces (used by Task 4): the `.wait`/`.go` protocol and the mustering entry `sd_browse` reads.

- [ ] **Step 1: Give `sd_spawn_server` a "no registry" flag**

Replace the `sd_spawn_server` signature line and the `-gamesdir` line. Current:

```js
function sd_spawn_server(port, maxplayers, ws, mode)
{
	var cmd = SD_BINARY + " -spawnserver -port " + port
	    + " -maxplayers " + maxplayers
	    + " -gamesdir " + SD_GAMES
	    + " -host %a"                     // lowercase: quoted if it has a space
	    + " -wadset " + ws.id
	    + " -gamemode " + mode;
```

with (add a `register` param; include `-gamesdir` only when registering — the muster
path passes `false` so the server writes no competing registry entry):

```js
function sd_spawn_server(port, maxplayers, ws, mode, register)
{
	var cmd = SD_BINARY + " -spawnserver -port " + port
	    + " -maxplayers " + maxplayers
	    + " -host %a"                     // lowercase: quoted if it has a space
	    + " -wadset " + ws.id
	    + " -gamemode " + mode;
	if (register !== false)               // omit -gamesdir on the muster path
		cmd += " -gamesdir " + SD_GAMES;
```

(Existing callers pass no `register` arg, so `register !== false` is true and behavior is unchanged for them.)

- [ ] **Step 2: Add `sd_muster_go` and `sd_muster_host`**

Add both functions just above `sd_create`:

```js
// Fire the match: spawn the (unregistered) server sized to the assembled count K,
// signal go, and launch the creator's own client FIRST so it's the netgame controller
// (its -skill/-deathmatch define the match). `entry` is the mustering-entry path.
function sd_muster_go(entry, port, ws, mode, skill, K) {
	sd_spawn_server(port, K, ws, mode, false);   // no -gamesdir: lobby owns discovery
	mswait(500);                                 // let it bind before we connect
	gl.write_go(entry, gl.creator_connect_host(cfg.net) + ":" + port);
	gl.remove_game(entry);                       // no longer joinable via Browse
	console.beep();
	var extra = ["-players", String(K), "-skill", String(skill)];
	if (mode == "deathmatch")
		extra.push("-deathmatch");
	else if (mode == "altdeath")
		extra.push("-altdeath");
	try {
		sd_play(gl.creator_connect_host(cfg.net) + ":" + port, extra, sd_wadset_args(cfg, ws));
	} finally {
		gl.clear_muster(entry);                  // sweep .go + any leftover .wait.*
	}
}

// The host's JS waiting room: heartbeat the mustering entry, show who's assembled,
// and wait for the target (auto) or a manual Start (S, >=2). Returns "started" (a
// match was launched via sd_muster_go) or "cancel" (Q / hangup). Mirrors SyncDuke's
// sd_wait_room machinery (nodesync / Ctrl-P / panel / beep).
function sd_muster_host(entry, port, ws, mode, skill, target) {
	var mynode = bbs.node_num;
	var lastbeat = time();
	var lastcount = 1;                           // host alone
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";
	sd_panel_rows_prev = -1;
	var bg = function () {
		var K = sd_muster_players(entry);
		console.clear();
		console.print("\1n\1h\1cSyncDOOM \1y-\1n\1h\1c Waiting Room\1n\r\n\r\n");
		console.print(" Hosting \1h\1w" + ws.name + "\1n \1c(" + sd_mode_label(mode)
		    + ")\1n -- assembled \1h\1w" + K + " / " + target + "\1n\r\n");
		var wl = gl.list_waiters(entry), i;
		console.print("   \1n\1why " + user.alias + " \1k\1h(host)\1n\r\n");
		for (i = 0; i < wl.length; i++)
			console.print("   \1n\1w" + (wl[i].alias || ("node " + wl[i].node)) + "\1n\r\n");
		console.print("\r\n \1hS\1n start (needs 2+)   \1hQ\1n cancel   \1hP\1n page nodes\r\n");
	};
	bg();
	try {
		while (!js.terminated && bbs.online) {
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {
				console.clear();
				bbs.nodesync();
				sd_write_muster(cfg, port, ws, mode, target, sd_muster_players(entry));
				lastbeat = time();
				console.pause();
				bg();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();
			if (js.terminated || !bbs.online)
				return "cancel";

			var K = sd_muster_players(entry);
			if (K != lastcount) {                // someone joined/left -> beep on arrival, redraw
				if (K > lastcount)
					console.beep();
				lastcount = K;
				bg();
				sd_panel_rows_prev = -1;
			}
			if (K >= target) {                   // target met -> auto start
				sd_muster_go(entry, port, ws, mode, skill, K);
				return "started";
			}
			if (time() - lastbeat >= SD_MUSTER_HEARTBEAT) {
				sd_write_muster(cfg, port, ws, mode, target, K);
				lastbeat = time();
			}
			sd_draw_panel(bg);
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (!c)
				continue;
			if (c == "Q")
				return "cancel";
			if (c == "S") {
				if (sd_muster_players(entry) >= 2) {
					sd_muster_go(entry, port, ws, mode, skill, sd_muster_players(entry));
					return "started";
				}
				// else ignore -- need at least 2
			}
			if (c == "P") {
				sd_write_muster(cfg, port, ws, mode, target, sd_muster_players(entry));
				lastbeat = time();
				var targets = sd_prompt_page_players();
				sd_send_pages(targets, mode);
				console.pause();
				bg();
				sd_panel_rows_prev = -1;
			}
		}
		return "cancel";
	} finally {
		console.ctrlkey_passthru = oldctrl;
	}
}
```

(This uses `sd_draw_panel(bgfn)` — the optional-background variant SyncDuke added. **Confirm `xtrn/syncdoom/lobby.js`'s `sd_draw_panel` accepts a `bgfn`; if it doesn't yet, apply the same one-line change SyncDuke made** — `function sd_draw_panel(bgfn)` and `(bgfn || sd_draw_art)()` on the height-change branch — as part of this step.)

- [ ] **Step 3: Rewire `sd_create` to muster**

Replace `sd_create`'s body from the `var page_targets = …` line through the end of the function. Current tail (from `sd_spawn_server(port, n, ws, mode);` onward) launches the server + client immediately; replace the whole block after `skill` is resolved:

```js
	// Decide on paging up front -- the (blocking) prompt must NOT sit between the
	// server spawn and our connect below, or a Browse-joiner could connect first
	// and become the controller. We only deliver the pages (fast) after spawning.
	var page_targets = (n > 1) ? sd_prompt_page_players() : [];

	var port = gl.alloc_port(cfg.net);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free server port available.\1n\r\n");
		console.pause();
		return;
	}

	sd_spawn_server(port, n, ws, mode);
	mswait(500);                          // let the server bind before we connect
	...
	sd_play(gl.creator_connect_host(cfg.net) + ":" + port, extra, sd_wadset_args(cfg, ws));
}
```

with (a solo `n==1` still launches immediately; `n>1` musters):

```js
	var port = gl.alloc_port(cfg.net);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free server port available.\1n\r\n");
		console.pause();
		return;
	}

	// n == 1: a solo run (testing) -- spawn + connect immediately, no muster.
	if (n <= 1) {
		sd_spawn_server(port, 1, ws, mode);
		mswait(500);
		sd_play(gl.creator_connect_host(cfg.net) + ":" + port,
		    ["-players", "1", "-skill", String(skill)], sd_wadset_args(cfg, ws));
		return;
	}

	// n > 1: muster. Write the lobby-owned "mustering" entry, page up front, then host
	// the JS waiting room. The server + our client are launched only at "go"
	// (sd_muster_go), so nobody waits at Doom's blank sync screen.
	var page_targets = sd_prompt_page_players();
	var entry = sd_write_muster(cfg, port, ws, mode, n, 1);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}
	console.print("\r\n\1h\1gGame created.\1n Entering the waiting room; others \1hJ\1noin.\r\n");
	sd_send_pages(page_targets, mode);

	if (sd_muster_host(entry, port, ws, mode, skill, n) != "started")
		gl.clear_muster(entry);          // cancel/hangup -> tear the muster down
	// "started" -> sd_muster_go already ran + cleared the muster in its finally.
}
```

- [ ] **Step 4: Syntax-check**

Run: `jsexec -U -C /home/rswindell/sbbs/xtrn/syncdoom/lobby.js`
Expected: no SpiderMonkey `SyntaxError` (a runtime `ReferenceError` on `bbs`/`console` under jsexec is expected — it proves the file parses).

- [ ] **Step 5: Live validation (REQUIRED before commit — human runs it; combined with Task 4)**

Deferred to Task 4's combined live test (host + joiner are exercised together). Confirm here only that the file parses.

- [ ] **Step 6: Commit** (after Task 4's shared live test confirms both sides)

Hold this commit until the joiner side (Task 4) is implemented and the human has run the combined live test. Then commit host + joiner together, or as two commits in sequence:

```bash
cd /home/rswindell/sbbs
git add xtrn/syncdoom/lobby.js
git commit -m "syncdoom: host-side JS lobby muster (waiting room, go, unregistered server)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Joiner side — join waiting room + browse rewiring (`lobby.js`)

The joiner registers a waiter marker and waits in a JS room for the host's go, then launches. Interactive — validated by the combined live test.

**Files:**
- Modify: `xtrn/syncdoom/lobby.js` (add `sd_muster_join`; `sd_browse` ~387 join tail)

**Interfaces:**
- Consumes (Tasks 1–3): `gl.write_waiter`, `gl.remove_waiter`, `gl.read_go`, `gl.list_games`/`sd_list_games`; existing `sd_play`, `sd_draw_panel`, `sd_panel_rows_prev`, `sd_wadset_args`, `sd_find_wadset`, `sd_wadset_files_present`, `sd_show_note`, `sd_show_wad_msgs`, `cfg`. The mustering entry's fields (`host`, `wadset`, `mode`, `addr`, `port`, `target`) from Task 3.

- [ ] **Step 1: Add `sd_muster_join`**

Add just above `sd_browse`:

```js
// Join a muster: drop our waiter marker, then wait in a JS room for the host's "go"
// (we launch our client only then -- deferred connect). `sel` is the chosen mustering
// entry (from sd_list_games). `ws` is the already-resolved/validated wad set. Returns
// after the client exits, or when we leave (Q) / the host cancels.
function sd_muster_join(sel, ws) {
	if (!gl.write_waiter(sel.file, bbs.node_num, (typeof user != "undefined" && user.alias) ? user.alias : "")) {
		console.print("\r\n\1h\1wCouldn't register for that game (already joining?).\1n\r\n");
		console.pause();
		return;
	}
	var mynode = bbs.node_num;
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";
	sd_panel_rows_prev = -1;
	var bg = function () {
		console.clear();
		console.print("\1n\1h\1cSyncDOOM \1y-\1n\1h\1c Waiting Room\1n\r\n\r\n");
		console.print(" Joined \1h\1w" + sel.host + "\1n\1c's " + sel.wadset + " ("
		    + sd_mode_label(sel.mode) + ")\1n game.\r\n");
		console.print(" \1h\1yWaiting for the host to start...\1n\r\n");
		console.print("\r\n \1hQ\1n leave\r\n");
	};
	bg();
	try {
		while (!js.terminated && bbs.online) {
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {
				console.clear();
				bbs.nodesync();
				console.pause();
				bg();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();
			if (js.terminated || !bbs.online) {
				gl.remove_waiter(sel.file, mynode);
				return;
			}
			var addr = gl.read_go(sel.file);
			if (addr !== null) {              // host fired -> launch our client
				console.beep();
				gl.remove_waiter(sel.file, mynode);
				console.ctrlkey_passthru = oldctrl;
				var dial = (addr && addr.length) ? addr : (sel.addr + ":" + sel.port);
				sd_play(dial, [], sd_wadset_args(cfg, ws));
				return;
			}
			if (!file_exists(sel.file)) {     // host cancelled (entry gone, no go)
				console.print("\r\n\1h\1wThe host cancelled the game.\1n\r\n");
				gl.remove_waiter(sel.file, mynode);
				console.pause();
				return;
			}
			sd_draw_panel(bg);
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (c == "Q") {
				gl.remove_waiter(sel.file, mynode);
				return;
			}
		}
		gl.remove_waiter(sel.file, mynode);
	} finally {
		console.ctrlkey_passthru = oldctrl;
	}
}
```

- [ ] **Step 2: Rewire `sd_browse`'s join to use the muster**

In `sd_browse`, the status check and the final launch change. Replace the current joinability check:

```js
	// Vanilla Doom only accepts joins before the game starts.
	if (sel.status != "lobby") {
		console.print("\r\n\1h\1rThat game is already in progress; you can't join it.\1n\r\n");
		console.pause();
		return;
	}
```

with (a muster is joinable while `mustering`):

```js
	// A lobby muster is joinable while assembling.
	if (sel.status != "mustering") {
		console.print("\r\n\1h\1rThat game is already in progress; you can't join it.\1n\r\n");
		console.pause();
		return;
	}
```

And replace the final launch line:

```js
	sd_show_note(ws);
	sd_show_wad_msgs(cfg, ws);
	sd_play(sel.addr + ":" + sel.port, [], sd_wadset_args(cfg, ws));
}
```

with (enter the join waiting room instead of connecting immediately):

```js
	sd_show_note(ws);
	sd_show_wad_msgs(cfg, ws);
	sd_muster_join(sel, ws);
}
```

- [ ] **Step 3: Syntax-check**

Run: `jsexec -U -C /home/rswindell/sbbs/xtrn/syncdoom/lobby.js`
Expected: no `SyntaxError` (runtime `bbs`/`console` ReferenceError expected).

- [ ] **Step 4: Live validation (REQUIRED before commit — human runs it)**

Deploy the SyncDOOM lobby JS, then multiple nodes:
- (a) Node A `Create` with **3** players → the Waiting Room shows "1 / 3", "S start (needs 2+)". Nodes B, C `Browse` → join → A's list shows them assemble ("2 / 3", "3 / 3"), beeping on each arrival; the game stays listed in Browse for late joiners (heartbeat) past 90s.
- (b) With 2 assembled, A presses **S** → the two launch, connect, and play (below the target of 3).
- (c) Fresh game with target **3** → let it auto-go when the third joins → three launch and play.
- (d) A joiner presses **Q** → removed from A's list (count drops).
- (e) A (host) presses **Q** → joiners see "the host cancelled".
- (f) Degraded case: assemble, go, then hang up a joiner between go and connect → confirm only that match is affected and A can abort its stuck client (Doom sync screen).
Verify a **co-op** and a **deathmatch** muster; confirm the creator is controller (its skill/mode apply). **Hand to the human.**

- [ ] **Step 5: Commit** (after the human confirms the combined live test — commit Task 3 first if not already, then this)

```bash
cd /home/rswindell/sbbs
git add xtrn/syncdoom/lobby.js
git commit -m "syncdoom: joiner-side JS lobby muster (join waiting room, deferred connect)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

(If Task 3 and Task 4 are committed together because they were live-tested as one unit, stage `xtrn/syncdoom/lobby.js` once and use a single commit message covering both host and joiner sides.)

---

## Notes for the executor

- **Order:** Task 1 (gl primitives) → Task 2 (syncdoom model) → Task 3 (host UI) → Task 4 (joiner UI). Tasks 1–2 commit on green; Tasks 3–4 share ONE combined live multi-node test and commit only after the human confirms.
- **`game_lobby.js` is shared** — Task 1's additions must not change existing behavior; Task 1 Step 5 re-runs SyncDuke's `test_game_lobby_events.js` to prove it.
- **No engine/netcode change** — if you find yourself editing `src/doors/syncdoom/*.c`, stop; the only server-side lever is *omitting* `-gamesdir` (Task 3 Step 1), done from JS.
- **`sd_draw_panel(bgfn)`** — SyncDOOM's copy may not yet take the optional background fn (SyncDuke's does). Task 3 Step 2 notes applying the same one-line change if needed; verify before writing `sd_muster_host`.
- **Do not push.**
```
