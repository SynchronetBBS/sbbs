# SyncDuke Dukematch Mode + Frag Activity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let two SyncDuke players play Duke3D deathmatch, selectable from the lobby's Create flow, with per-kill "frag" events surfacing in the lobby's recent-activity feed and live panel.

**Architecture:** The Duke engine already implements deathmatch (the `/c` command-line flag) and already tracks frags (the global `frags[killer][victim]` matrix) and peer names (`ud.user_name[]`, synced on the existing co-op connect). So the work is: (1) lobby JS emits the right `/c` flag and carries a `mode` through the game registry; (2) the door's C event layer drains its own row of the frag matrix into `frag` JSON events; (3) the lobby feed renders them. The shared `exec/load/game_lobby.js` is reused unchanged.

**Tech Stack:** Synchronet door — C (vendored Chocolate-Duke3D + a headless shim, built via CMake/`build.sh`) and Synchronet JavaScript (SpiderMonkey 1.8.5) for the lobby. Full design in `src/doors/syncduke/DUKEMATCH_DESIGN.md`.

## Global Constraints

- **JS is SpiderMonkey 1.8.5 only** — no `let`/`const`, no arrow functions, no `for..of`, no template literals, no `.find`/`.includes`/`.trim` on strings via ES5-plus; use `var`, classic `for`, `indexOf`. (SyncDuke lobby files already obey this.)
- **C must compile clean under GCC/Clang first, then MSVC** — GCC/Clang are the portability floor; a Windows-only compile is not verification. No identifiers with leading-underscore-capital or double-underscore. Reaching the end of a non-void function without `return` is a GCC/Clang error.
- **Run `uncrustify` (`src/uncrustify.cfg`) on every touched `.c`/`.h` file** as the closing step of that task; write tabs from the start.
- **US English spelling** in comments/strings (color/behavior).
- **Registry back-compat:** an in-flight game entry written by the current (pre-dukematch) lobby has no `mode` field and MUST still list and join as co-op.
- **Frag wording is SyncDOOM-verbatim:** `"<killer> fragged <victim>"`.
- **Mode tokens (lobby/registry namespace):** `"coop"` | `"dm"` | `"dmnospawn"`. **Door `ev_mode()` output (independent namespace):** `"single"` | `"co-op"` | `"dukematch"` | `"dukematch-nospawn"`.
- **Engine `/c` mapping (verified `Game/src/game.c:7364`):** `/c1` = Dukematch spawn, `/c2` = Cooperative, `/c3` = Dukematch no-spawn (`ud.m_coop` = digit − 1). `/m` = monsters off (`game.c:7417`). `/t` = respawn items/monsters/inventory (`game.c:7404`).
- **Commit policy:** commit direct to master; end each commit message with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`. Do NOT push. Per project rule, the C and lobby-UI tasks require a live 2-player validation before their commit is offered — flag to the human when a task needs that.

---

### Task 1: Lobby model — mode→flag, `[dukematch]` config, registry `mode`, frag text (`syncduke_lib.js`)

All the headless-testable model logic for the feature, in one file, with one jsexec test. No UI, no C — pure functions over strings/objects.

**Files:**
- Modify: `xtrn/syncduke/syncduke_lib.js` (functions `sd_load_config` ~57, `sd_cmd` ~115, `sd_write_entry` ~141, `sd_event_text` ~184)
- Create: `src/doors/syncduke/tests/test_dukematch_lib.js`

**Interfaces:**
- Consumes: `gl.read_overlaid`, `gl.apply_defaults`, `gl.write_game`, `gl.list_games` (existing `game_lobby.js` helpers, unchanged); `File.iniGetObject`.
- Produces (used by Task 2 lobby UI):
  - `sd_cmd(role, port, peer, level, mode, dmopts)` — `mode` is `"coop"|"dm"|"dmnospawn"` (default `"coop"` when falsy); `dmopts` is an object `{monsters, respawn_items}` (e.g. `cfg.dukematch`), consulted only for DM modes.
  - `sd_write_entry(cfg, port, level, mode)` — writes registry entry with `mode: mode || "coop"`; returns the file path.
  - `sd_load_config()` — return value gains `cfg.dukematch = { submode, monsters, respawn_items }`.
  - `sd_event_text(e)` — now renders `e.type === "frag"` as `killer + " fragged " + victim`.

- [ ] **Step 1: Write the failing test**

Create `src/doors/syncduke/tests/test_dukematch_lib.js`:

```js
// jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_dukematch_lib.js
//
// Headless unit tests for the SyncDuke dukematch model layer (syncduke_lib.js).
// Loads the checkout lib as a namespace object (it ends with `this;`) and stubs
// `user` (jsexec has none). The lib's internal bare load of game_lobby.js resolves
// to the live install copy -- fine, this task does not touch game_lobby.js.
user = { number: 1, alias: "Duke" };

var lib = load({}, js.exec_dir + "../../../../xtrn/syncduke/syncduke_lib.js");
var fails = 0;
function chk(n, got, want) {
	var ok = String(got) === String(want);
	writeln("  " + n + " got=[" + got + "] want=[" + want + "]" + (ok ? " ok" : (fails++, " FAIL")));
}
function has(n, hay, needle) {
	var ok = String(hay).indexOf(needle) >= 0;
	writeln("  " + n + (ok ? " ok" : (fails++, " FAIL -- [" + needle + "] not in [" + hay + "]")));
}
function hasnot(n, hay, needle) {
	var ok = String(hay).indexOf(needle) < 0;
	writeln("  " + n + (ok ? " ok" : (fails++, " FAIL -- [" + needle + "] unexpectedly in [" + hay + "]")));
}

// --- sd_cmd: mode -> /c flag ---
var coop = lib.sd_cmd("master", 25000, null, 1, "coop");
has("coop -> /c2", coop, "/c2");   hasnot("coop no /c1", coop, "/c1");
var dm = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: false, respawn_items: false });
has("dm -> /c1", dm, "/c1");       hasnot("dm no /c2", dm, "/c2");
var dmns = lib.sd_cmd("master", 25000, null, 1, "dmnospawn", { monsters: false, respawn_items: false });
has("dmnospawn -> /c3", dmns, "/c3");
var deflt = lib.sd_cmd("master", 25000, null, 1);           // omitted mode -> coop (back-compat)
has("default mode -> /c2", deflt, "/c2");

// --- sd_cmd: dukematch options (/m monsters-off default, /t item respawn) ---
has("dm monsters-off default -> /m", dm, "/m");
var dmMon = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: true, respawn_items: false });
hasnot("dm monsters on -> no /m", dmMon, "/m");
var dmResp = lib.sd_cmd("master", 25000, null, 1, "dm", { monsters: false, respawn_items: true });
has("dm respawn_items -> /t", dmResp, "/t");
hasnot("dm no respawn_items -> no /t", dm, "/t");
hasnot("coop never adds /m", coop, "/m");

// --- sd_load_config: [dukematch] defaults present ---
var cfg = lib.sd_load_config();
chk("cfg.dukematch.submode default", cfg.dukematch.submode, "spawn");
chk("cfg.dukematch.monsters default", cfg.dukematch.monsters, "false");

// --- registry mode round-trip (real fs, cleaned up) ---
mkpath(system.data_dir + "syncduke/games/");
var f1 = lib.sd_write_entry(cfg, 29001, 1, "dm");
var f2 = lib.sd_write_entry(cfg, 29002, 1);                // no mode -> coop
var open = lib.sd_list_open_games(cfg);
function byPort(list, p) { for (var i = 0; i < list.length; i++) if (String(list[i].port) === String(p)) return list[i]; return null; }
var e1 = byPort(open, 29001), e2 = byPort(open, 29002);
chk("entry dm mode", e1 && e1.mode, "dm");
chk("entry default mode coop", e2 && e2.mode, "coop");
if (f1) file_remove(f1);
if (f2) file_remove(f2);

// --- sd_event_text: frag case (SyncDOOM-verbatim) ---
chk("frag text", lib.sd_event_text({ type: "frag", killer: "Duke", victim: "Rob" }), "Duke fragged Rob");
chk("death text unchanged", lib.sd_event_text({ type: "death", user: "Duke", map: "E1L1" }), "Duke died on E1L1");
chk("unknown type -> null", lib.sd_event_text({ type: "bogus" }), "null");

writeln(fails ? ("\n" + fails + " FAILURE(S)") : "\nALL PASS");
exit(fails ? 1 : 0);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_dukematch_lib.js`
Expected: FAIL — `sd_cmd` ignores the 5th/6th args so `dm -> /c1` fails (emits `/c2`), `cfg.dukematch.submode` is `undefined`, and `frag text` is `null`. (If the lib fails to load, confirm `game_lobby.js` resolves from the live install; the lib's bare `load({}, "game_lobby.js")` needs the live `/sbbs/exec/load/game_lobby.js`, which exists.)

- [ ] **Step 3: Add `[dukematch]` to `sd_load_config`**

In `xtrn/syncduke/syncduke_lib.js`, change the `sd_load_config` opening object and add the `apply_defaults` block. Replace:

```js
function sd_load_config() {
	var cfg = { net: {}, lobby: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.net = gl.read_overlaid(f, "net");
		cfg.lobby = f.iniGetObject("lobby") || {};   // [lobby] live = true -> live panel
		f.close();
	}
```

with:

```js
function sd_load_config() {
	var cfg = { net: {}, lobby: {}, dukematch: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.net = gl.read_overlaid(f, "net");
		cfg.lobby = f.iniGetObject("lobby") || {};   // [lobby] live = true -> live panel
		cfg.dukematch = f.iniGetObject("dukematch") || {};   // deathmatch sub-mode + options
		f.close();
	}
```

Then, immediately before the closing `return cfg;` of `sd_load_config`, add a second `apply_defaults` (after the existing `gl.apply_defaults(cfg.net, {...})` block):

```js
	// Dukematch sub-mode + arena options (lobby-side; the door reads none of these --
	// they become engine /c//m//t flags in sd_cmd). Classic DM defaults: spawn, no monsters.
	gl.apply_defaults(cfg.dukematch, {
		submode: "spawn",         // "spawn" (/c1, weapons respawn) or "nospawn" (/c3)
		monsters: false,          // classic dukematch is player-vs-player only
		respawn_items: false      // respawn items/inventory over time
	});
	return cfg;
}
```

- [ ] **Step 4: Extend `sd_cmd` with `mode` + `dmopts`**

Replace the whole `sd_cmd` function:

```js
function sd_cmd(role, port, peer, level) {
	var cmd = SD_BINARY + " -s%H -t%T -name %a -home " + sd_home()
	    + ' -eventlog "' + SD_EVENTS + '"';   // door appends start/level/death here
	if (role == "master")
		cmd += " -netrole master -netport " + port;
	else if (role == "join")
		cmd += " -netrole join -netpeer " + peer;
	if (role)                                   // co-op: warp both peers to the level
		cmd += " /v1 /l" + parseInt(level, 10) + " /c2";
	return cmd;
}
```

with:

```js
// role  = "master" | "join" | null (single-player)
// mode  = "coop" | "dm" | "dmnospawn" (net games; default "coop"); the /c flag.
// dmopts = { monsters, respawn_items } (e.g. cfg.dukematch); consulted for DM only.
// The engine parses only '/' options. The master's start packet carries mode/respawn
// to the joiner, so only the master strictly needs /m//t; passing them on the joiner
// too is harmless (overwritten by the start packet), and we pass the same /c for both.
function sd_cmd(role, port, peer, level, mode, dmopts) {
	var cmd = SD_BINARY + " -s%H -t%T -name %a -home " + sd_home()
	    + ' -eventlog "' + SD_EVENTS + '"';   // door appends start/level/death/frag here
	if (role == "master")
		cmd += " -netrole master -netport " + port;
	else if (role == "join")
		cmd += " -netrole join -netpeer " + peer;
	if (role) {                                 // net game: warp both peers to the level
		var cflag = (mode == "dm") ? "/c1" : (mode == "dmnospawn") ? "/c3" : "/c2";
		cmd += " /v1 /l" + parseInt(level, 10) + " " + cflag;
		if (mode == "dm" || mode == "dmnospawn") {
			// Monsters off unless explicitly enabled (classic DM default).
			var monstersOn = dmopts && (dmopts.monsters === true || dmopts.monsters === "true");
			if (!monstersOn)
				cmd += " /m";
			if (dmopts && (dmopts.respawn_items === true || dmopts.respawn_items === "true"))
				cmd += " /t";
		}
	}
	return cmd;
}
```

- [ ] **Step 5: Add `mode` to `sd_write_entry`**

Replace the `sd_write_entry` signature and its `gl.write_game` object to include `mode` (add the `mode` param and one property; leave everything else):

```js
function sd_write_entry(cfg, port, level, mode) {
	var lev = sd_find_level(level);
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
		created:   time()
	});
}
```

(Note: `sd_list_open_games` needs no change — `gl.list_games` deserializes every field, so `mode` rides along automatically; a legacy entry simply lacks the key, and Task 2's join path defaults it to `"coop"`.)

- [ ] **Step 6: Add the `frag` case to `sd_event_text`**

In `sd_event_text`, add the `frag` case as the FIRST case in the switch:

```js
function sd_event_text(e) {
	switch (e.type) {
		case "frag":  return e.killer + " fragged " + e.victim;
		case "start": return e.user + " joined (" + (e.mode || "single") + ")";
		case "level": return e.user + " reached " + e.map;
		case "death": return e.user + " died on " + e.map;
	}
	return null;
}
```

- [ ] **Step 7: Run the test to verify it passes**

Run: `jsexec /home/rswindell/sbbs/src/doors/syncduke/tests/test_dukematch_lib.js`
Expected: PASS — final line `ALL PASS`, exit 0.

- [ ] **Step 8: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncduke/syncduke_lib.js src/doors/syncduke/tests/test_dukematch_lib.js
git commit -m "syncduke: lobby model for dukematch (mode->/c flag, [dukematch] config, registry mode, frag text)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: Lobby UI — Create prompts game type, Join shows mode (`lobby.js`)

Wire the model from Task 1 into the interactive lobby: Create asks Co-op vs Dukematch (before the level picker), Join lists the mode, and both thread `mode` into `sd_write_entry`/`sd_cmd`. Interactive console UI — not unit-tested; verified by reading + the live 2-player test.

**Files:**
- Modify: `xtrn/syncduke/lobby.js` (functions `sd_create` ~85, `sd_join` ~135, `sd_send_pages` ~68; add `sd_pick_mode`)

**Interfaces:**
- Consumes (from Task 1): `sd_cmd(role, port, peer, level, mode, dmopts)`, `sd_write_entry(cfg, port, level, mode)`, `cfg.dukematch`.
- Produces: no new exports (lobby.js is the entry point); `sd_pick_mode()` is file-local.

- [ ] **Step 1: Add `sd_pick_mode` above `sd_create`**

In `xtrn/syncduke/lobby.js`, add this function just before `sd_create` (after `sd_pick_level`). It mirrors `sd_pick_level`'s `console.uselect` idiom:

```js
// Present the game-type picker (co-op vs dukematch). Returns "coop", "dm", or
// "dmnospawn" (the [dukematch] submode decides spawn vs no-spawn), or null to abort.
// One prompt only -- the DM sub-mode + arena options come from [dukematch] config,
// consistent with how skill/port are config-driven rather than prompted.
function sd_pick_mode() {
	console.uselect(0, "a game type", "Co-op (play the level together)");
	console.uselect(1, "a game type", "Dukematch (deathmatch)");
	var sel = console.uselect(0);           // default-highlight Co-op
	if (sel < 0)
		return null;
	if (sel == 0)
		return "coop";
	var sub = (cfg.dukematch && String(cfg.dukematch.submode).toLowerCase() == "nospawn");
	return sub ? "dmnospawn" : "dm";
}
```

- [ ] **Step 2: Thread `mode` through `sd_create`**

In `sd_create`, add the mode pick at the top and thread it into the registry write, the paging text, the "Game created" line, and the master launch. Replace the current body from the level-pick through the `sd_play` call. The current code is:

```js
function sd_create() {
	var levnum = sd_pick_level();
	if (levnum === null)
		return;
	var lev = sd_find_level(levnum);
```
...through...
```js
	sd_send_pages(page_targets, lev);
	console.print("\r\n\1h\1gGame created:\1n \1wE1L" + lev.num + " " + lev.name
	    + "\1n. Entering the waiting room; another player can \1hJ\1noin.\r\n");

	try {
		sd_play(sd_cmd("master", port, null, lev.num));
	} finally {
		gl.remove_game(entry);
	}
}
```

Change: (a) after `var lev = sd_find_level(levnum);` insert the mode pick; (b) pass `mode` to `sd_write_entry`; (c) pass mode + `cfg.dukematch` to `sd_cmd`; (d) label the "Game created" line. The resulting function:

```js
function sd_create() {
	var levnum = sd_pick_level();
	if (levnum === null)
		return;
	var lev = sd_find_level(levnum);

	var mode = sd_pick_mode();
	if (mode === null)
		return;
	var modelabel = (mode == "coop") ? "co-op" : "dukematch";

	var port = gl.alloc_port(cfg.net);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free game port available.\1n\r\n");
		console.pause();
		return;
	}

	var page_targets = sd_prompt_page();

	var entry = sd_write_entry(cfg, port, lev.num, mode);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}

	sd_send_pages(page_targets, lev, modelabel);
	console.print("\r\n\1h\1gGame created:\1n \1wE1L" + lev.num + " " + lev.name
	    + "\1n \1n\1c(" + modelabel + ")\1n. Entering the waiting room; another player can \1hJ\1noin.\r\n");

	try {
		sd_play(sd_cmd("master", port, null, lev.num, mode, cfg.dukematch));
	} finally {
		gl.remove_game(entry);
	}
}
```

- [ ] **Step 3: Add the mode label to `sd_send_pages`**

`sd_send_pages` gains a `modelabel` param so the page invite names the game type. Replace:

```js
function sd_send_pages(targets, lev) {
	if (!targets || !targets.length)
		return;
	var who = (typeof user != "undefined" && user.alias) ? user.alias : "Someone";
	var body = "started a SyncDuke (Nukem 3D) co-op game (" + lev.name
	    + "); run SyncDuke to join.";
```

with:

```js
function sd_send_pages(targets, lev, modelabel) {
	if (!targets || !targets.length)
		return;
	var who = (typeof user != "undefined" && user.alias) ? user.alias : "Someone";
	var body = "started a SyncDuke (Nukem 3D) " + (modelabel || "co-op") + " game (" + lev.name
	    + "); run SyncDuke to join.";
```

- [ ] **Step 4: Show + thread `mode` in `sd_join`**

In `sd_join`, add a Mode column to the multi-game list, name the mode in the single-game fast path, and pass `sel.mode` to `sd_cmd`. The current multi-game block prints:

```js
		console.print("\r\n\1h\1cCo-op games waiting:\1n\r\n");
		console.print("    \1n\1wHost              Level\1n\r\n");
		var i;
		for (i = 0; i < games.length; i++) {
			var g = games[i];
			console.print(format(" \1h\1y%2d\1n %-17s E1L%s %s\r\n",
			    i + 1, g.host, g.level, g.levelname));
		}
```

Replace that block with (adds a Mode column via a small helper):

```js
		console.print("\r\n\1h\1cGames waiting:\1n\r\n");
		console.print("    \1n\1wHost              Mode       Level\1n\r\n");
		var i;
		for (i = 0; i < games.length; i++) {
			var g = games[i];
			console.print(format(" \1h\1y%2d\1n %-17s %-10s E1L%s %s\r\n",
			    i + 1, g.host, sd_mode_label(g.mode), g.level, g.levelname));
		}
```

Change the single-game fast path from:

```js
	if (games.length == 1) {
		sel = games[0];
		console.print("\r\n\1h\1cJoining \1n\1w" + sel.host + "\1h\1c's game\1n ("
		    + "E1L" + sel.level + " " + sel.levelname + ")...\1n\r\n");
	} else {
```

to:

```js
	if (games.length == 1) {
		sel = games[0];
		console.print("\r\n\1h\1cJoining \1n\1w" + sel.host + "\1h\1c's " + sd_mode_label(sel.mode)
		    + " game\1n (E1L" + sel.level + " " + sel.levelname + ")...\1n\r\n");
	} else {
```

And change the final launch line from:

```js
	gl.remove_game(sel.file);
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level));
```

to:

```js
	gl.remove_game(sel.file);
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level, sel.mode || "coop", cfg.dukematch));
```

- [ ] **Step 5: Add the `sd_mode_label` helper**

Add this small helper just above `sd_join` (a legacy entry with no `mode` reads as Co-op):

```js
// Human label for a registry entry's mode token (legacy entries lack it -> co-op).
function sd_mode_label(mode) {
	if (mode == "dm")        return "Dukematch";
	if (mode == "dmnospawn") return "Dukematch*";   // * = no weapon/item respawn
	return "Co-op";
}
```

- [ ] **Step 6: Syntax-check the lobby loads clean**

Run: `jsexec -c /home/rswindell/sbbs/xtrn/syncduke/lobby.js` (compile-only; the door binary + a real node aren't needed to catch a syntax error). If `-c` is unavailable in this build, run `jsexec /home/rswindell/sbbs/xtrn/syncduke/lobby.js` and confirm it fails only later (e.g. missing `bbs`/`console` at runtime), not with a SyntaxError.
Expected: no SpiderMonkey `SyntaxError`. (Runtime errors about `console`/`bbs` being undefined under jsexec are expected and fine — this only proves the file parses.)

- [ ] **Step 7: Live validation (REQUIRED before commit)**

Deploy (`cd /home/rswindell/sbbs/src/doors/syncduke && ./build.sh` is only needed for C; for this JS task copy `lobby.js` + `syncduke_lib.js` to the live `/sbbs/xtrn/syncduke/` via the door's `deploy.sh`, or however the sysop deploys). Then from two BBS nodes: node A runs SyncDuke → `C`reate → the game-type picker appears → choose Dukematch → pick a level → waiting room says "(dukematch)"; node B → `J`oin → the list shows `Mode = Dukematch`. Confirm both drop into a deathmatch (frag bar visible top of screen, no monsters by default). **Hand this to the human to run; do not claim it done from static reasoning.**

- [ ] **Step 8: Commit** (after the human confirms the live test)

```bash
cd /home/rswindell/sbbs
git add xtrn/syncduke/lobby.js
git commit -m "syncduke: lobby Create prompts co-op vs dukematch; Join shows game mode

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: Door frag events (C — `syncduke_game.c`, `syncduke.h`, `syncduke_events.c`)

Emit a `frag` JSON event per kill made by this player, and report the real deathmatch sub-mode from `ev_mode()`. Compiled C over the vendored engine's globals.

**Files:**
- Modify: `src/doors/syncduke/syncduke_game.c` (add `syncduke_next_frag`)
- Modify: `src/doors/syncduke/syncduke.h` (declare it, ~line 118 after `syncduke_game_status`)
- Modify: `src/doors/syncduke/syncduke_events.c` (`ev_mode` ~55, add `ev_frag`, drain in `syncduke_events_tick` ~120)

**Interfaces:**
- Consumes: engine globals `frags[MAXPLAYERS][MAXPLAYERS]` (`Game/src/global.c:199`, declared `Game/src/duke3d.h:671`), `myconnectindex`, `ud.multimode`, `ud.coop`, `ud.user_name[]`; door helpers `syncduke_door_alias()`, `syncduke_home()`, `syncduke_eventlog_path()`, and the file-local `ev_esc`/`ev_map`/`ev_emit` in `syncduke_events.c`.
- Produces: `int syncduke_next_frag(int *victim)` — returns 1 and sets `*victim` for each not-yet-reported kill by this player; 0 when drained. Emits `{"type":"frag","killer":<alias>,"victim":<name>,"map":"E#L#",...}` lines; `ev_mode()` now returns `"dukematch"`/`"dukematch-nospawn"`/`"co-op"`/`"single"`.

- [ ] **Step 1: Add `syncduke_next_frag` to `syncduke_game.c`**

Append to `src/doors/syncduke/syncduke_game.c` (it already includes `duke3d.h`):

```c
/* --- dukematch frag detection (see syncduke_events.c) --------------------------
 * The deterministic lockstep sim runs every player on both door instances, so the
 * global frags[killer][victim] matrix is identical on each.  We report ONLY our own
 * row (frags[myconnectindex][*]); the peer reports its own -- so each frag is logged
 * exactly once, no central writer, no dedup (mirrors SyncDOOM's sd_next_frag).  A
 * static shadow lets a multi-frag tic drain one victim per call.  Self-frags stay the
 * existing 'death' event (they don't increment the cross-player matrix -- player.c's
 * fraggedself path), so they're intentionally not reported here. */
int syncduke_next_frag(int *victim)
{
	static short seen[MAXPLAYERS];
	static int   seen_valid;
	int          me = myconnectindex, j;

	/* Only in a real dukematch (multiplayer, not co-op). */
	if (!(ud.multimode > 1 && ud.coop != 1)) {
		seen_valid = 0;
		return 0;
	}
	if (!seen_valid) {                 /* first call this match: adopt current counts */
		for (j = 0; j < MAXPLAYERS; j++)
			seen[j] = frags[me][j];
		seen_valid = 1;
		return 0;
	}
	for (j = 0; j < MAXPLAYERS; j++) {
		if (j == me)
			continue;
		if (frags[me][j] > seen[j]) {
			seen[j]++;
			if (victim != NULL)
				*victim = j;
			return 1;
		}
	}
	return 0;
}
```

- [ ] **Step 2: Declare it in `syncduke.h`**

In `src/doors/syncduke/syncduke.h`, under the `provided by syncduke_game.c` block, after the `syncduke_game_status(...)` line (~118), add:

```c
int syncduke_next_frag(int *victim);   /* dukematch: next not-yet-reported frag BY US -> *victim player #, else 0 */
```

- [ ] **Step 3: Rewrite `ev_mode` to report the real DM sub-mode**

In `src/doors/syncduke/syncduke_events.c`, replace the current `ev_mode`:

```c
static const char *ev_mode(void)
{
	extern char syncduke_net_role[16];
	return (strcmp(syncduke_net_role, "master") == 0 || strcmp(syncduke_net_role, "join") == 0)
	       ? "co-op" : "single";
}
```

with:

```c
static const char *ev_mode(void)
{
	extern char syncduke_net_role[16];
	if (strcmp(syncduke_net_role, "master") != 0 && strcmp(syncduke_net_role, "join") != 0)
		return "single";
	/* In a net game ud.coop reflects the master's chosen mode (set from the /c flag on
	 * the master, from the start packet on the joiner): 1 = co-op, 0 = DM spawn,
	 * 2 = DM no-spawn. */
	if (ud.coop == 1)
		return "co-op";
	return (ud.coop == 2) ? "dukematch-nospawn" : "dukematch";
}
```

- [ ] **Step 4: Add `ev_frag`**

In `src/doors/syncduke/syncduke_events.c`, add `ev_frag` right after `ev_death` (it reuses `ev_esc`/`ev_map`/`ev_emit` and the same JSON shape). Victim name comes from `ud.user_name[victim]`, with Duke's own "player N" fallback (`player.c:2662`) when a name hasn't arrived:

```c
static void ev_frag(int victim)
{
	char j[400], ka[80], va[80], vn[24], map[16];
	const char *vname = ud.user_name[victim];
	if (vname == NULL || vname[0] == '\0') {
		snprintf(vn, sizeof(vn), "player %d", victim + 1);
		vname = vn;
	}
	ev_map(map, sizeof(map));
	snprintf(j, sizeof(j),
	         "{\"time\":%ld,\"type\":\"frag\",\"node\":%d,\"killer\":\"%s\",\"victim\":\"%s\","
	         "\"map\":\"%s\"}",
	         (long)time(NULL), sbbs_my_node(),
	         ev_esc(syncduke_door_alias(), ka, sizeof(ka)),
	         ev_esc(vname, va, sizeof(va)), map);
	ev_emit(j);
}
```

- [ ] **Step 5: Drain frags in `syncduke_events_tick`**

In `syncduke_events_tick`, inside the existing `if (in_game) { ... }` death-edge block, add the frag drain. The current block is:

```c
	if (in_game) {                                  /* death rising edge */
		int dead = syncduke_player_dead();
		if (dead && !dead_last)
			ev_death();
		dead_last = dead;
	} else {
		dead_last = 0;
	}
```

Replace with:

```c
	if (in_game) {                                  /* frags by us, then our death edge */
		int victim;
		int dead = syncduke_player_dead();
		while (syncduke_next_frag(&victim))
			ev_frag(victim);
		if (dead && !dead_last)
			ev_death();
		dead_last = dead;
	} else {
		dead_last = 0;
	}
```

- [ ] **Step 6: Build (GCC/Clang first — the portability floor)**

Run: `cd /home/rswindell/sbbs/src/doors/syncduke && ./build.sh`
Expected: clean build, `xtrn/syncduke/syncduke` (the symlinked binary) rebuilt with no warnings on the touched files. If the build system is CMake-out-of-tree, follow `build.sh`'s own steps. (Do NOT `make install`; the user deploys.)

- [ ] **Step 7: Uncrustify the touched C files**

Run: `cd /home/rswindell/sbbs && uncrustify -c src/uncrustify.cfg --no-backup src/doors/syncduke/syncduke_game.c src/doors/syncduke/syncduke_events.c src/doors/syncduke/syncduke.h`
Then re-run `./build.sh` to confirm it still compiles after reformatting.
Expected: files reformatted in place (tabs), build still clean.

- [ ] **Step 8: Live 2-player frag validation (REQUIRED before commit)**

Two nodes, Create → Dukematch → Join (Task 2 UI). Frag each other a few times. Then inspect the event log:
`tail -f /sbbs/data/syncduke/events.jsonl` (live install path) — expect `{"type":"frag","killer":"<A>","victim":"<B>",...}` lines with correct aliases, and `start` events now showing `"mode":"dukematch"`. Confirm a co-op game still logs `"mode":"co-op"` and NO frag lines. **Hand to the human to run.**

- [ ] **Step 9: Commit** (after the human confirms the live test)

```bash
cd /home/rswindell/sbbs
git add src/doors/syncduke/syncduke_game.c src/doors/syncduke/syncduke.h src/doors/syncduke/syncduke_events.c
git commit -m "syncduke: door emits dukematch frag events + reports DM mode

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: Docs — config template, installer note, comparison matrix

Document the new `[dukematch]` config and flip the comparison doc's deathmatch row. No code; folded into one doc task.

**Files:**
- Modify: `xtrn/syncduke/syncduke.example.ini` (add `[dukematch]` section)
- Modify: `xtrn/syncduke/install-xtrn.ini` (`Desc:` + lobby note)
- Modify: `xtrn/syncdoom/install-xtrn.ini` (`Desc:` harmonization — same "SyncTERM graphics" overstatement)
- Modify: `src/doors/syncduke/SYNCDOOM_VS_SYNCDUKE.md` (deathmatch/frag rows → Yes)

**Interfaces:** none (documentation).

- [ ] **Step 1: Add `[dukematch]` to `syncduke.example.ini`**

In `xtrn/syncduke/syncduke.example.ini`, add this section (place it after `[net]`, before `[game]`, to sit near the other multiplayer config):

```ini
[dukematch]
; Deathmatch options, read by the LOBBY (lobby.js) when a player picks "Dukematch"
; in the Create menu -- the door binary reads none of these. (Co-op ignores them.)
;
; submode -- spawn: weapons/items respawn (the arcade default); nospawn: grab-once.
submode = spawn
;
; monsters -- classic dukematch is player-vs-player only (monsters off). Set true for
; a monsters-in-the-arena variant.
monsters = false
;
; respawn_items -- respawn items/inventory over time (independent of weapon spawn).
respawn_items = false
```

- [ ] **Step 2: Note dukematch in `install-xtrn.ini`**

In `xtrn/syncduke/install-xtrn.ini`, make two edits. Change the `Desc:` line from:

```
Desc: Duke Nukem 3D for the BBS -- SyncTERM sixel graphics, single-player + LAN co-op
```

to (drop both "sixel" and "SyncTERM" — the graphics aren't sixel-only OR SyncTERM-only: sixel runs on xterm/mlterm/foot/WezTerm/Windows Terminal and the text/block tiers on any ANSI terminal; SyncTERM just gets the best tier, it isn't exclusive):

```
Desc: Duke Nukem 3D for the BBS -- graphics, single-player + 2-player LAN co-op & dukematch
```

And in the `[prog:SYNCDUKE_LOBBY]` `note = ...` line, change `create/join 2-player LAN co-op` to `create/join 2-player LAN co-op or dukematch` (leave the rest of the note unchanged).

- [ ] **Step 2b: Harmonize the SyncDOOM `Desc:` (same overstatement)**

`xtrn/syncdoom/install-xtrn.ini` has the same inaccurate "SyncTERM graphics" wording (SyncDOOM shares the identical JXL/sixel/text tier stack). Change its `Desc:` line from:

```
Desc: Multiplayer DOOM for the BBS -- SyncTERM graphics, co-op & deathmatch
```

to:

```
Desc: Multiplayer DOOM for the BBS -- graphics, co-op & deathmatch
```

(Only the `Desc:` line — no other SyncDOOM change.)

- [ ] **Step 3: Flip the deathmatch rows in `SYNCDOOM_VS_SYNCDUKE.md`**

Read the file first (it was rewritten 2026-07-02; confirm the lines below still match before editing — adjust to the current text if they've shifted). Make these edits, keeping the doc's existing tone/table format:

1. **Deathmatch mode** matrix row (~line 29). Change the SyncDuke cell from:
   `Engine has Dukematch, **but the door never selects it**`
   to:
   `**Yes** — 2-player Dukematch (spawn / no-spawn), lobby-selectable`

2. **Multiplayer transport** row (~line 28). Change SyncDuke's `co-op cadence` to `co-op **or** dukematch` (still 2-player LAN UDP).

3. **Door activity/events** row (~line 35). Update SyncDuke's parenthetical from `(starts/level-reaches/deaths)` to `(starts/level-reaches/deaths/**frags**)`.

4. **§1 "Deathmatch / multiplayer"** prose. The SyncDuke summary line (~76) `no dedicated server, no deathmatch, no >2 players` becomes `no dedicated server, 2-player dukematch (no >2 players, no dedicated server)`. In the bullets (~93–95) that say deathmatch is "present in the engine but never selected" and list it under "Genuinely absent", rewrite to: the lobby now selects Dukematch (spawn/no-spawn) via the `/c` flag and the door emits `frag` events into the feed; remove "deathmatch mode" from the "genuinely absent" list (dedicated/detached server and >2 players remain absent).

5. **Bottom "Multiplayer maturity" / "where SyncDuke trails"** section (~288–291). Drop "deathmatch (Dukematch) selection" from the trailing-features list; keep "dedicated/detached server" and ">2-player support".

Do NOT introduce internal C symbol references into sysop-facing prose beyond what the doc already does. Show a `git diff` of the doc in your report so the reviewer can check the wording.

- [ ] **Step 4: Sanity-check the ini parses**

Run: `jsexec -r "var f=new File('/home/rswindell/sbbs/xtrn/syncduke/syncduke.example.ini'); f.open('r'); var o=f.iniGetObject('dukematch'); f.close(); print(JSON.stringify(o));"`
Expected: prints `{"submode":"spawn","monsters":false,"respawn_items":false}` (proves the new section is well-formed and reads back).

- [ ] **Step 5: Commit**

```bash
cd /home/rswindell/sbbs
git add xtrn/syncduke/syncduke.example.ini xtrn/syncduke/install-xtrn.ini xtrn/syncdoom/install-xtrn.ini src/doors/syncduke/SYNCDOOM_VS_SYNCDUKE.md
git commit -m "syncduke: document dukematch mode ([dukematch] config, comparison doc)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Notes for the executor

- **Order matters:** Task 1 (model) → Task 2 (UI, consumes Task 1) → Task 3 (C, independent of 1/2 but the live test in Task 2 and Task 3 are the same 2-player session — you may run one combined live validation covering both before committing each) → Task 4 (docs).
- **Live tests are the human's to run.** Tasks 2 and 3 both end in a 2-player validation; batch them into a single live session if convenient, but each task's commit still waits on its own confirmation.
- **`game_lobby.js` is intentionally untouched** — if you find yourself editing it, stop and reconsider; the design routes everything through the existing `gl.*` API.
- **Do not push** (`git push`) — commits stay local until the user explicitly says to push.
```
