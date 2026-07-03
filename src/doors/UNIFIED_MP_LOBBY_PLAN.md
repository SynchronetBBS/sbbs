# Unified `[M]ultiplayer` Lobby Entry — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `[J]oin` and `[C]reate` lobby keys in SyncDOOM and SyncDuke with a single `[M]ultiplayer` option whose short Y/N flow funnels users into one game, guarded by a short-lived `file_mutex` create-lock.

**Architecture:** The **entire `[M]` flow is shared** in `exec/load/game_lobby.js` as `gl.multiplayer_flow(opts)`, parameterized by door-specific callbacks (`list`/`label`/`create`/`join`/`external`) and the games dir. Each door's `lobby.js` gets a ~10-line `sd_multiplayer()` that calls it with its callbacks, a `sd_join_selected(sel)` join helper, one release line in `sd_create`, and a menu/`lobby.msg` rewire. The create-lock (also in `game_lobby.js`) is `file_mutex`-based.

**Tech Stack:** Synchronet JavaScript (SpiderMonkey 1.8.5), the `game_lobby.js` model layer, the built-in `file_mutex()`/`file_exists()`/`file_remove()` globals, `jsexec` for headless tests.

**Design doc:** `src/doors/UNIFIED_MP_LOBBY_DESIGN.md` (note: this plan realizes the design's "shared flow" variant — the `[M]` flow lives in `game_lobby.js`, not duplicated per door).

## Global Constraints

- **SpiderMonkey 1.8.5 only** — `var` only (no `let`/`const`/arrow/template-literals/`for..of`); no `Array.prototype.includes`/`.find`. Match the existing lobby code.
- **Both doors stay behaviorally aligned**; the flow is literally shared, so only the callbacks (labels, fields, external) differ.
- **`game_lobby.js` changes are additive** — new top-level functions before the final `this;`, exported automatically as `gl.*`.
- **Cross-host:** the lock file lives in the games dir (`SD_GAMES`), the shared cross-host registry dir.
- **`file_mutex(path [,text] [,max_age])`** returns `true` if acquired, `false` if a non-stale lock is held; a lock older than `max_age` seconds is overwritten (stale-reap). Release = remove the file. **`max_age` = 120 s.**
- **`console.yesno(question)`** returns bool and adds its own Yes/No prompt — pass just the question phrase.

---

## File Structure

| File | Responsibility | Change |
|------|----------------|--------|
| `exec/load/game_lobby.js` | shared model layer | **Add** create-lock (`acquire_create_lock`/`release_create_lock` + `create_lock_path`) and the shared flow (`multiplayer_flow`, `mp_create`, `mp_wait_for_game`, `mp_pick`) |
| `xtrn/syncdoom/lobby.js` | SyncDOOM lobby | **Add** thin `sd_multiplayer()`, `sd_join_selected(sel)`, `sd_mp_external_offer()`; **one release line** in `sd_create`; **rewire** menu (`M` replaces `J`/`C`/`E`) |
| `xtrn/syncdoom/lobby.msg` | SyncDOOM menu art | **Edit** key list (`M` replaces `J`/`C`; drop `E`) |
| `xtrn/syncduke/lobby.js` | SyncDuke lobby | **Add** thin `sd_multiplayer()`, `sd_join_selected(sel)`; **one release line** in `sd_create`; **rewire** menu (`M` replaces `J`/`C`) |
| `xtrn/syncduke/lobby.msg` | SyncDuke menu art | **Edit** key list (`M` replaces `J`/`C`) |

---

## Task 1: Shared create-lock + `[M]` flow in `game_lobby.js`

**Files:**
- Modify: `exec/load/game_lobby.js` (add functions before the final `this;`)
- Test: a throwaway `jsexec` script in the scratchpad (create-lock only — the flow is interactive and is exercised by the door live-tests)

**Interfaces produced (all become `gl.*`):**
- `acquire_create_lock(dir)` → `bool`; `release_create_lock(dir)` → removes the lock (idempotent).
- `multiplayer_flow(opts)` → runs the `[M]` flow. `opts`:
  - `games_dir` — the door's registry/lock dir (`SD_GAMES`).
  - `list()` → array of waiting games.
  - `label(g)` → one-line string describing game `g` for the prompts.
  - `create()` → the door's create flow (registers a game and calls `gl.release_create_lock(games_dir)` at its register point; this flow's `finally` is the safety net).
  - `join(g)` → join game `g`.
  - `external` (optional) → a function offering external-by-address join (self-gates on the door's config); omit for doors without it.

- [ ] **Step 1: Write the failing create-lock test**

Scratchpad `test_create_lock.js`:

```js
var gl = load({}, "game_lobby.js");
var dir = system.temp_dir;
gl.release_create_lock(dir);
var a = gl.acquire_create_lock(dir);   // true
var b = gl.acquire_create_lock(dir);   // false (held)
gl.release_create_lock(dir);
var c = gl.acquire_create_lock(dir);   // true (reacquired)
gl.release_create_lock(dir);
print("a=" + a + " b=" + b + " c=" + c);
print((a === true && b === false && c === true) ? "PASS" : "FAIL");
```

- [ ] **Step 2: Run it — expect FAIL** (`gl.acquire_create_lock is not a function`).

Run: `cd /sbbs && jsexec <scratchpad>/test_create_lock.js`

- [ ] **Step 3: Add create-lock + shared flow**

Before the final `this;` in `exec/load/game_lobby.js`, add:

```js
// ---- create-lock: a short-lived mutex serializing the "create a game" setup
// window so a second user who picks Multiplayer while someone is mid-create waits
// and then joins, instead of spawning a duplicate game. Lives in the games dir
// (cross-host). file_mutex() reaps a lock older than the max-age, so an abandoned
// setup self-clears with no heartbeat; release is a plain remove.
var CREATE_LOCK_MAX_AGE = 120;   // seconds; comfortably longer than the create picks

function create_lock_path(dir) {
	return backslash(dir) + "creating.lock";
}

function acquire_create_lock(dir) {
	var who = (typeof system != "undefined" ? system.local_host_name : "host")
	    + "-" + (typeof bbs != "undefined" ? bbs.node_num : 0);
	return file_mutex(create_lock_path(dir), who, CREATE_LOCK_MAX_AGE);
}

function release_create_lock(dir) {
	var p = create_lock_path(dir);
	if (file_exists(p))
		file_remove(p);
}

// ---- shared [M]ultiplayer lobby flow (both doors). Interactive; parameterized by
// the door via `opts` (games_dir, list(), label(g), create(), join(g), external?).
// Sequential Y/N: join an existing game, else create; a second game while one
// exists is the rarest path, reached only as a follow-up Y/N.
function multiplayer_flow(opts) {
	var games = opts.list();

	if (games.length == 0) {
		if (console.yesno("\r\nNo multiplayer games are waiting. Create one"))
			mp_create(opts);
		else if (opts.external)
			opts.external();
		return;
	}

	if (games.length == 1) {
		if (console.yesno("\r\nJoin " + opts.label(games[0]))) {
			opts.join(games[0]);
			return;
		}
		if (console.yesno("Create one")) {
			mp_create(opts);
			return;
		}
		if (opts.external)
			opts.external();
		return;
	}

	var sel = mp_pick(games, opts);       // >= 2 games
	if (sel) {
		opts.join(sel);
		return;
	}
	if (console.yesno("Create one")) {
		mp_create(opts);
		return;
	}
	if (opts.external)
		opts.external();
}

// Numbered picker for >= 2 games. Returns the chosen game, or null to skip.
function mp_pick(games, opts) {
	var i;
	console.print("\r\n\1h\1cMultiplayer games:\1n\r\n");
	for (i = 0; i < games.length; i++)
		console.print("  \1h\1w" + (i + 1) + "\1n) " + opts.label(games[i]) + "\r\n");
	console.print("\r\nJoin which? [\1h1\1n-\1h" + games.length
	    + "\1n], or \1hN\1n to skip: ");
	var n = console.getnum(games.length);
	if (n < 1 || n > games.length)
		return null;
	return games[n - 1];
}

// Create under the setup-window lock; if another node holds it, poll instead.
function mp_create(opts) {
	if (acquire_create_lock(opts.games_dir)) {
		try { opts.create(); }
		finally { release_create_lock(opts.games_dir); }
		return;
	}
	mp_wait_for_game(opts);
}

// A game is being set up by another node: wait screen + ~1.5s poll. A game
// appearing -> back into the flow's join prompt. Lock freed and still no game
// (creator bailed pre-register) -> we create. Q aborts.
function mp_wait_for_game(opts) {
	console.clear();
	console.print("\r\n\1h\1wA multiplayer game is being set up -- please wait...\1n"
	    + "   (\1hQ\1n=cancel)\r\n");
	while (!js.terminated && bbs.online) {
		if (opts.list().length > 0) {
			multiplayer_flow(opts);
			return;
		}
		if (acquire_create_lock(opts.games_dir)) {
			try { opts.create(); }
			finally { release_create_lock(opts.games_dir); }
			return;
		}
		if (console.inkey(K_UPPER | K_NOECHO, 1500) == "Q")
			return;
	}
}
```

- [ ] **Step 4: Run the create-lock test — expect PASS** (`a=true b=false c=true`, `PASS`).

- [ ] **Step 5: Load-check `game_lobby.js` exports the flow**

Run: `cd /sbbs && jsexec -r "var gl=load({},'game_lobby.js'); print(typeof gl.multiplayer_flow + ' ' + typeof gl.acquire_create_lock);"`
Expected: `function function`.

- [ ] **Step 6: Commit**

```bash
git add exec/load/game_lobby.js
git commit -m "game_lobby: shared [M]ultiplayer flow + create-lock (file_mutex)"
```

---

## Task 2: SyncDOOM thin wrapper + wiring

**Files:**
- Modify: `xtrn/syncdoom/lobby.js`, `xtrn/syncdoom/lobby.msg`

**Interfaces:**
- Consumes: `gl.multiplayer_flow(opts)`, `gl.release_create_lock(SD_GAMES)` (Task 1); existing `sd_list_games(cfg)` (games with `.host`/`.wadset`/`.mode`/`.players`/`.maxplayers`), `sd_create()`, `sd_muster_join(sel, ws)`, the wadset-validation `sd_browse` runs before joining, `sd_join_external()`, `cfg.net.allow_external`.

**Reference — read first:** `sd_browse()` (its select→validate→`sd_muster_join` tail), `sd_create()` (the `var entry = sd_write_muster(...)` success point), the menu dispatch block.

- [ ] **Step 1: Add the thin wrapper + helpers**

```js
// [M]ultiplayer: run the shared lobby flow with SyncDOOM's callbacks.
function sd_multiplayer() {
	gl.multiplayer_flow({
		games_dir: SD_GAMES,
		list: function () { return sd_list_games(cfg); },
		label: function (g) {
			return g.host + "'s game (" + g.wadset + ", " + g.mode + ", "
			    + g.players + "/" + g.maxplayers + ")";
		},
		create: sd_create,
		join: sd_join_selected,
		external: sd_mp_external_offer
	});
}

// Join an already-selected mustering entry: validate its WAD set (as sd_browse
// does), then hand off to the muster joiner.
function sd_join_selected(sel) {
	var ws = sd_resolve_wadset(sel);      // the wadset validation sd_browse uses
	if (ws)
		sd_muster_join(sel, ws);
}

// Offer external-by-address join, self-gated on the sysop opt-in.
function sd_mp_external_offer() {
	var allow = (cfg.net.allow_external === true || cfg.net.allow_external === "true");
	if (allow && console.yesno("\r\nJoin an external server by address"))
		sd_join_external();
}
```

If `sd_browse` validates the wadset inline rather than via a named helper, first extract that block into `sd_resolve_wadset(sel)` (returns the ws object or `null`, messaging the user on failure) and call it from both `sd_browse` and `sd_join_selected`.

- [ ] **Step 2: Add the release line to `sd_create`**

After the muster register success block:

```js
	var entry = sd_write_muster(cfg, port, ws, mode, n, 1);
	if (!entry) {
		// ...existing failure handling...
	}
```

add:

```js
	gl.release_create_lock(SD_GAMES);   // game registered/joinable -> free the setup lock
	                                    // (mp_create's finally is the safety net for aborts)
```

Leave the solo (`n <= 1`) path untouched — it registers no game, so `mp_create`'s `finally` releases the lock.

- [ ] **Step 3: Rewire the menu dispatch** — replace the `J`/`C`/`E` arms with a single `M`:

```js
		else if (k == "M")
			sd_multiplayer();
		else if (k == "P")
			sd_solo();
		else if (k == "L")
			sd_show_activity();
		else if (k == "H")
			sd_controls();
```

Remove the `else if (k == "J") sd_browse();`, `else if (k == "C") sd_create();`, and `else if (k == "E" && allow_ext) sd_join_external();` arms. If the `allow_ext` local in `sd_main` is now unused, remove it. `sd_browse`/`sd_create`/`sd_join_external` stay defined (reached via the flow).

- [ ] **Step 4: Update `lobby.msg`** — `[M]ultiplayer` replaces the `[J]oin`/`[C]reate` (and `[E]xternal`) lines; keep `[P]`/`[L]`/`[H]`/`[Q]`. Match the file's existing Ctrl-A color codes and layout.

- [ ] **Step 5: Parse-check**

Run: `cd /sbbs && jsexec -r "try{ load('/home/rswindell/sbbs/xtrn/syncdoom/lobby.js'); }catch(e){ print(e.name+': '+e.message); }"`
Expected: a runtime error at the `syncdoom_lib.js` load line (parsed OK), **not** a `SyntaxError`.

- [ ] **Step 6: Manual live-test** (record in the report — the door commit waits on this):
  1. `[M]`, no games → "Create one?" N returns to menu, Y enters create.
  2. Node A creating; Node B `[M]` during A's picks → "please wait…" → drops into "Join …? [Y/N]" the moment A registers.
  3. Two games → numbered list + "Create one?" on skip.
  4. `[net] allow_external=true` → declining join+create offers "Join an external server by address?".
  5. Menu shows `[M]`, not `[J]`/`[C]`; `[P]`/`[L]`/`[H]`/`[Q]` still work.

- [ ] **Step 7: Commit (after live-test confirms):**

```bash
git add xtrn/syncdoom/lobby.js xtrn/syncdoom/lobby.msg
git commit -m "SyncDOOM: unify [J]oin/[C]reate into [M]ultiplayer (shared flow)"
```

---

## Task 3: SyncDuke thin wrapper + wiring

Mirror of Task 2 onto SyncDuke's `.claim` internals; **no external** (omit `external` from `opts` and drop `sd_mp_external_offer`).

**Files:** `xtrn/syncduke/lobby.js`, `xtrn/syncduke/lobby.msg`.

**Reference — read first:** `sd_join()` (its select→`sd_play(sd_cmd("join", …))` tail), `sd_create()` (the `var entry = sd_write_entry(...)` success point), the menu dispatch.

- [ ] **Step 1: Add the thin wrapper + join helper**

```js
function sd_multiplayer() {
	gl.multiplayer_flow({
		games_dir: SD_GAMES,
		list: function () { return sd_list_games(cfg); },
		label: function (g) {
			return g.host + "'s game (" + (g.wadset || g.level) + ", " + g.mode + ")";
		},
		create: sd_create,
		join: sd_join_selected
	});
}

// Join an already-selected entry: extract SyncDuke's existing join tail from
// sd_join (the sd_play(sd_cmd("join", ...)) call + any level/wadset validation).
function sd_join_selected(sel) {
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level, sel.mode || "coop", cfg.dukematch));
}
```

Verify the exact fields (`g.level`/`g.wadset`, `g.mode`, `g.host`) against SyncDuke's `sd_list_games` output and adjust the `label` accordingly; reuse `sd_join_selected` from `sd_join`'s own selection path too.

- [ ] **Step 2: Add the release line to `sd_create`** — after SyncDuke's register block:

```js
	var entry = sd_write_entry(cfg, port, lev.num, mode);
	if (!entry) {
		// ...existing failure handling...
	}
```

add:

```js
	gl.release_create_lock(SD_GAMES);   // game registered/joinable -> free the setup lock
```

- [ ] **Step 3: Rewire the menu dispatch** — replace the `C`/`J` arms with `M`:

```js
		else if (k == "M")
			sd_multiplayer();
```

(remove `else if (k == "C") sd_create();` and `else if (k == "J") sd_join();`; both stay defined, reached via the flow).

- [ ] **Step 4: Update `xtrn/syncduke/lobby.msg`** — `[M]ultiplayer` replaces `[J]oin`/`[C]reate`; keep `[P]`/`[L]`/`[H]`/`[Q]`.

- [ ] **Step 5: Parse-check** — `jsexec -r "try{ load('/home/rswindell/sbbs/xtrn/syncduke/lobby.js'); }catch(e){ print(e.name+': '+e.message); }"`; expect a runtime error at the `syncduke_lib.js` load line, not a `SyntaxError`.

- [ ] **Step 6: Manual live-test** — Task 2 Step 6 items 1-3, 5 (no external), against SyncDuke's claim model.

- [ ] **Step 7: Commit (after live-test confirms):**

```bash
git add xtrn/syncduke/lobby.js xtrn/syncduke/lobby.msg
git commit -m "SyncDuke: unify [J]oin/[C]reate into [M]ultiplayer (shared flow)"
```

---

## Self-Review notes (for the executor)

- **Spec coverage:** menu merge (T2/T3 s3-4), Y/N flow + ≥2 picker (T1 `multiplayer_flow`/`mp_pick`), create-lock acquire/release/poll (T1; release line T2/T3 s2), in-progress exclusion (relies on `sd_list_games` — no code), external gated & folded (T2 `sd_mp_external_offer`; omitted T3). Covered.
- **DRY:** the flow is shared in `game_lobby.js`; doors carry only callbacks + one release line + wiring. No duplicated flow logic.
- **Idempotent release:** `release_create_lock` guards with `file_exists`, so the double release (in `sd_create` at register + in `mp_create`'s `finally`) is safe.
- **`multiplayer_flow` re-entry** from `mp_wait_for_game` is shallow/user-driven; acceptable.
- **Names to verify against real code:** SyncDOOM `sd_resolve_wadset` (extract from `sd_browse` if inline); SyncDuke `sd_join_peer`/`sd_cmd` and the `sd_list_games` game fields — reuse/rename, don't guess.
