// lobby.js -- Synchronet lobby frontend for the SyncDuke door (optional).
//
// The xtrn entry point: presents the pre-game lobby and spawns the syncduke C
// binary (located via js.exec_dir, this directory) to play. The door runs fine
// standalone without this lobby; it's the optional menu that adds co-op Create/
// Join on top of single-player. The model layer is in syncduke_lib.js, over the
// shared exec/load/game_lobby.js. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// SyncDuke v1 co-op is peer 2-player LAN: Create hosts a match as the "master"
// (player 0) and writes a registry entry the other nodes can Join; Join dials a
// listed match as player 1. Both warp to the same level and the move-FIFO lockstep
// runs (see syncduke_net.c). Single-player drops straight into Duke's own menus.
//
// Copyright(C) 2026 Rob Swindell / SyncDuke. GPL-2.0.

load(js.exec_dir + "syncduke_lib.js");

var cfg = sd_load_config();

// ---------------------------------------------------------------------------
// Launch
// ---------------------------------------------------------------------------

// Run the door for a play session. `cmd` is sd_cmd()'s pre-cmdstr string;
// bbs.cmdstr() expands the %-specifiers (socket, time, alias, ".exe") before exec.
// `map` is the picked [map:*] entry (null = stock Duke), logged so the node log
// names what was played -- the Terminal Server's own "Executing external program"
// line only names the lobby. Same "X-" code, so one grep finds both.
// Blocks until the door exits.
function sd_play(cmd, map) {
	bbs.logline("X-", "Playing Duke Nukem 3D: " + (map ? map.key : "stock content"));
	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN, SD_DIR);
	// The door writes frames straight to the socket, bypassing Synchronet's console
	// line counter, leaving it stale -- the next screen would fire an errant
	// [Hit a key]. Reset it so the menu repaints cleanly.
	console.line_counter = 0;
}

// ---------------------------------------------------------------------------
// Pickers
// ---------------------------------------------------------------------------

// Present the co-op level picker. Returns a 1-based level number, or null to abort.
function sd_pick_level() {
	var i;
	for (i = 0; i < SD_LEVELS.length; i++)
		console.uselect(i, "a level", "E1L" + SD_LEVELS[i].num + "  " + SD_LEVELS[i].name);
	var sel = console.uselect(0);            // default-highlight E1L1
	if (sel < 0)
		return null;
	return SD_LEVELS[sel].num;
}

// ---------------------------------------------------------------------------
// Paging the other active nodes (optional, on Create)
// ---------------------------------------------------------------------------

// Ask whether to page the door's other active players to join. Returns the node
// list to page, or [] if none/declined.
function sd_prompt_page() {
	var targets = gl.page_targets(gl.door_ars(SD_DIR));
	if (!targets.length)
		return [];
	if (!console.yesno("\r\nPage " + targets.length + " active node"
	    + (targets.length == 1 ? "" : "s") + " to join"))
		return [];
	return targets;
}

// Deliver the join invitation (fast, non-interactive).
function sd_send_pages(targets, lev, modelabel) {
	if (!targets || !targets.length)
		return;
	var who = (typeof user != "undefined" && user.alias) ? user.alias : "Someone";
	var body = "started a SyncDuke (Nukem 3D) " + (modelabel || "co-op") + " game (" + lev.name
	    + "); run SyncDuke to join.";
	var paged = gl.send_pages(targets, who, body);
	console.print("\r\n\1n\1cPaged \1h" + paged + "\1n\1c node"
	    + (paged == 1 ? "" : "s") + ".\1n\r\n");
}

// ---------------------------------------------------------------------------
// Menu actions
// ---------------------------------------------------------------------------

// Single-player: launch the door with no net/warp so Duke's own menus drive
// episode/skill/level selection.
function sd_solo() {
	var pick = sd_pick_map();
	if (pick.cancel)
		return;
	sd_play(sd_cmd(null, null, null, null, null, null, pick.map), pick.map);
}

// Pick what to play: stock Duke, or one of the sysop's [map:*] extras (user maps /
// add-on GRPs from syncduke.ini). Returns { cancel: true } on abort, else
// { map: null | entry } (null = stock). With nothing configured there's no prompt
// at all -- stock Duke launches directly, exactly as before.
function sd_pick_map() {
	if (!cfg.maps || !cfg.maps.length)
		return { map: null };
	console.uselect(0, "what to play", "Duke Nukem 3D");
	for (var i = 0; i < cfg.maps.length; i++)
		console.uselect(i + 1, "what to play", cfg.maps[i].key);
	var sel = console.uselect(0);           // default-highlight the stock game
	if (sel < 0)
		return { cancel: true };
	return { map: (sel == 0) ? null : cfg.maps[sel - 1] };
}

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

// Create a co-op match: pick a level, host it as master, advertise it in the
// registry, and enter the waiting room. The entry is removed when the door exits.
function sd_create() {
	var pick = sd_pick_map();
	if (pick.cancel)
		return;
	var map = pick.map;

	var lev;
	if (map && map.file) {
		// A usermap IS the level -- no level picker; 7 = the engine's usermap slot,
		// and everything downstream (pages, waiting room, entry) shows the map name.
		lev = { num: 7, name: map.key };
	} else {
		var levnum = sd_pick_level();
		if (levnum === null)
			return;
		lev = sd_find_level(levnum);
	}

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

	// Decide paging up front (a blocking prompt), then write the registry entry and
	// deliver the pages before launching -- so a paged player sees the match listed.
	var page_targets = sd_prompt_page();

	// Clear any stale claim marker left by a prior game that reused this port/stem,
	// BEFORE writing the entry -- so a real joiner (who can only claim once the entry
	// below is listed) can never race this cleanup, and the room can't mistake an old
	// marker for a joiner.
	var stale_claim = SD_GAMES + sd_entry_name(port) + ".claim";
	if (file_exists(stale_claim))
		file_remove(stale_claim);

	var entry = sd_write_entry(cfg, port, lev.num, mode, map);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}
	gl.release_create_lock(SD_GAMES);   // game registered/joinable -> free the setup lock
	                                     // (mp_create's finally is the safety net for aborts)

	sd_send_pages(page_targets, lev, modelabel);

	// Enter the JS waiting room (indefinite): it heartbeats the entry so joiners keep
	// seeing it, shows who's online, and lets the host Q-cancel or P-page. It returns
	// "joined" only once a joiner drops the claim marker -- and ONLY THEN do we launch
	// the master door (so its handshake connects in ~1-2s instead of hanging). On
	// cancel/hangup we tear the game down without ever launching (no silent solo game).
	try {
		if (sd_wait_room(entry, port, lev, mode, map) != "joined")
			return;                          // Q / hangup -> the finally clears the game
		// A joiner committed -> the match is starting. Drop the entry (and the joiner's
		// claim marker) from the registry NOW, so a third node never sees an in-progress
		// game listed as joinable. The running doors don't read the registry, and the
		// joiner already has the peer address, so removing it before launch is safe (this
		// restores the old remove-on-join behavior the claim-marker model changed).
		sd_clear_game(entry);
		sd_play(sd_cmd("master", port, null, lev.num, mode, cfg.dukematch, map), map);
	} finally {
		// Idempotent: covers the cancel/hangup return above and any unexpected throw out
		// of the waiting room; a no-op on the joined path (already cleared before launch).
		sd_clear_game(entry);
	}
}

// Human label for a registry entry's mode token (legacy entries lack it -> co-op).
function sd_mode_label(mode) {
	if (mode == "dm")        return "Dukematch";
	if (mode == "dmnospawn") return "Dukematch*";   // * = no weapon/item respawn
	return "Co-op";
}

// [M]ultiplayer: run the shared lobby flow (gl.multiplayer_flow) with SyncDuke's
// callbacks. Replaces the old separate [J]oin/[C]reate menu keys. No external join.
function sd_multiplayer() {
	gl.multiplayer_flow({
		games_dir: SD_GAMES,
		list: function () { return sd_list_open_games(cfg); },
		label: function (g) {
			if (g.mapname)                    // configured extra: the name says it all
				return g.host + "'s " + sd_mode_label(g.mode) + " game (" + g.mapname + ")";
			return g.host + "'s " + sd_mode_label(g.mode) + " game (E1L"
			    + g.level + " " + g.levelname + ")";
		},
		create: sd_create,
		join: sd_join_selected
	});
}

// Join an already-selected entry (the shared flow's join callback): claim it
// exclusively by dropping a marker file beside the master's entry (a separate
// file, so the master's heartbeat re-writes never clobber it). The claim is an
// EXCLUSIVE create: if it returns false, another node claimed this game a moment
// ago -- bail cleanly instead of launching a door that can't connect. We do NOT
// remove the entry (the master owns its lifecycle + clears it on exit).
function sd_join_selected(sel) {
	// Resolve the host's content choice BEFORE claiming: if this host's syncduke.ini
	// lacks the entry's [map:<name>] (a multi-host config skew -- the ini is normally
	// shared), bail without burning the game's one claim.
	var map = sd_map_by_name(cfg, sel.mapname);
	if (sel.mapname && !map) {
		console.print("\r\n\1h\1rThis game plays \"" + sel.mapname
		    + "\", which isn't configured in syncduke.ini on this host.\1n\r\n");
		console.pause();
		return;
	}
	if (!sd_claim_game(sel)) {
		console.print("\r\n\1h\1wThat game was just claimed by someone else.\1n\r\n");
		console.pause();
		return;
	}
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level, sel.mode || "coop", cfg.dukematch, map), map);
}

// Controls reference -- an external, sysop-editable display file (Ctrl-A codes)
// beside the door, mirroring SyncDOOM's controls.msg. printfile auto-pauses at EOF.
function sd_controls() {
	console.clear();
	console.printfile(SD_DIR + "controls.msg");
}

// Recent-activity view: start/level/death lines from the door-written event log
// (SD_EVENTS), most recent first. Mirrors SyncDOOM's activity screen.
function sd_show_activity() {
	console.clear();
	console.print("\1h\1wSyncDuke (Nukem 3D) \1y- \1wrecent activity\1n\r\n\r\n");
	var feed = gl.event_feed(SD_EVENTS, gl.activity_max(cfg), sd_event_text, gl.activity_max_age(cfg)), i;
	if (!feed.length)
		console.print("  \1k\1h(nothing yet -- go play a game!)\1n\r\n");
	else
		for (i = 0; i < feed.length; i++)
			console.print("  " + feed[i] + "\r\n");
	console.print("\r\n");
	console.pause();
}

// ---------------------------------------------------------------------------
// Live lobby panel (who's-online + recent activity, bottom-anchored) -- opt-in
// via [lobby] live = true. Mirrors SyncDOOM's: the cell composition is shared
// (gl.panel_cells, via sd_panel_cells), the drawing is here (gl is UI-free).
// ---------------------------------------------------------------------------

// Draw the full lobby art (its bottom rows host the panel).
function sd_draw_art() {
	console.clear();
	console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
}

// Repaint the live panel, bottom-anchored and growing upward over the art. Only the
// panel rows are rewritten each tick (no art redraw -> no flicker); when the panel's
// height changes the art is redrawn first so vacated rows don't keep a stale panel.
var sd_panel_rows_prev = -1;
function sd_draw_panel(bgfn) {
	var cols = console.screen_columns, rows = console.screen_rows;
	var p = sd_panel_cells(cols, gl.activity_max_age(cfg), gl.panel_rows(cfg));
	var cells = p.cells, nrows = cells.length;
	if (sd_panel_rows_prev >= 0 && nrows != sd_panel_rows_prev)
		(bgfn || sd_draw_art)();             // height changed -> restore background
	sd_panel_rows_prev = nrows;
	var top = rows - nrows + 1, r;
	for (r = 0; r < nrows; r++) {
		console.gotoxy(1, top + r);
		console.print("\1n");
		console.cleartoeol();
		console.print(cells[r]);
	}
	console.gotoxy(cols, rows);               // park the cursor in the lower-right corner
}

// Live lobby: refresh the panel ~1/s and return the first valid menu key. The static
// menu's blocking getkeys() calls nodesync() for us (telegrams, sysop interrupt,
// forced chat); the poll loop must do it explicitly.
function sd_lobby_wait() {
	var allowed = "MSLHQ";                    // '?'/Enter -> redraw (handled below)
	var mynode = bbs.node_num;
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";          // keep Ctrl-P (paging) with the BBS
	sd_panel_rows_prev = -1;                  // first draw shouldn't trigger an art redraw
	try {
		while (!js.terminated && bbs.online) {
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {                    // a waiting telegram prints to the screen
				console.clear();
				bbs.nodesync();
				console.pause();
				sd_draw_art();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();               // sync node record / handle interrupt
			if (js.terminated || !bbs.online)
				break;
			sd_draw_panel();
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (!c)
				continue;                    // timeout -> refresh
			if (c == "\r" || c == "\n" || c == "?") {   // redraw the menu art
				sd_draw_art();
				sd_panel_rows_prev = -1;
				continue;
			}
			if (allowed.indexOf(c) >= 0)
				return c;
			// unmapped key (incl. passed-through Ctrl-keys) -> ignore, keep refreshing
		}
		return "Q";
	} finally {
		console.ctrlkey_passthru = oldctrl;
	}
}

// The waiting room's static background: title + what's being hosted + the key hints.
// The live who's-online + activity panel (sd_draw_panel) paints below it and refreshes
// each tick; this bg is redrawn only on entry and when the panel height changes.
function sd_wait_bg(lev, modelabel) {
	console.clear();
	console.print("\1n\1h\1cSyncDuke \1y-\1n\1h\1c Waiting Room\1n\r\n\r\n");
	console.print(" Hosting \1h\1wE1L" + lev.num + " " + lev.name + "\1n \1c(" + modelabel + ")\1n.\r\n");
	console.print(" \1h\1yWaiting for a player to join...\1n\r\n");
	var canpage = gl.page_targets(gl.door_ars(SD_DIR)).length > 0;
	console.print("\r\n \1hQ\1n cancel" + (canpage ? "    \1hP\1n page nodes" : "") + "\r\n");
}

// The master's waiting room: heartbeat the registry entry so joiners keep seeing it,
// show the live who's-online panel, and wait -- indefinitely -- for a joiner to drop
// the claim marker. Returns "joined" (launch the door) or "cancel" (host Q, hangup, or
// telegram/interrupt exit). Mirrors sd_lobby_wait's nodesync/Ctrl-P/panel machinery.
function sd_wait_room(entry, port, lev, mode, map) {
	var modelabel = (mode == "coop") ? "co-op" : "dukematch";
	var mynode = bbs.node_num;
	var lastbeat = time();
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";          // keep Ctrl-P (paging) with the BBS
	sd_panel_rows_prev = -1;
	var bg = function () { sd_wait_bg(lev, modelabel); };
	bg();
	presence.set_node_ext_status("waiting for 1 more player (SyncDuke)");
	try {
		while (!js.terminated && bbs.online) {
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {                    // a waiting telegram prints to the screen
				console.clear();
				bbs.nodesync();
				sd_write_entry(cfg, port, lev.num, mode, map);   // heartbeat before the blocking
				lastbeat = time();                          // pause, so the entry doesn't age
				console.pause();                            // out while the host reads it
				bg();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();               // sync node record / handle interrupt
			if (js.terminated || !bbs.online)
				return "cancel";
			if (sd_game_claimed(entry)) {     // a joiner dropped the claim -> go
				console.beep();               // alert the host the game is starting
				return "joined";
			}
			if (time() - lastbeat >= SD_WAIT_HEARTBEAT) {   // keep the entry fresh/visible
				sd_write_entry(cfg, port, lev.num, mode, map);
				lastbeat = time();
			}
			sd_draw_panel(bg);
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (!c)
				continue;                    // timeout -> refresh
			if (c == "Q")
				return "cancel";
			if (c == "P") {                  // page more nodes, then keep waiting
				sd_write_entry(cfg, port, lev.num, mode, map);   // heartbeat before the blocking
				lastbeat = time();                          // page prompt + pause
				var targets = sd_prompt_page();
				if (targets.length) {           // no pageable nodes / declined -> no empty [Hit a key]
					sd_send_pages(targets, lev, modelabel);
					console.pause();
				}
				bg();
				sd_panel_rows_prev = -1;
			}
			// any other key -> ignore, keep waiting
		}
		return "cancel";
	} finally {
		presence.set_node_ext_status("");
		console.ctrlkey_passthru = oldctrl;
	}
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

function sd_main() {
	// The door logs start/level/death to SD_EVENTS when launched with -eventlog;
	// ensure its dir exists (the door's append won't create it) and keep it bounded.
	mkpath(system.data_dir + "syncduke/");
	gl.prune_events(SD_EVENTS, 2000, 1000);
	gl.enter_sound(SD_DIR, cfg);     // optional one-shot sound on entry (SyncTERM audio)
	// The live who's-online/activity panel is opt-in ([lobby] live = true): it
	// repaints the bottom rows ~1/s, so it suits ANSI terminals only.
	var live = (cfg.lobby && (cfg.lobby.live === true || cfg.lobby.live === "true"));
	while (!js.terminated && bbs.online) {
		var k;
		if (live) {
			// The art's bottom rows host the live panel; the loop draws art + polls.
			sd_draw_art();
			k = sd_lobby_wait();
		} else {
			console.clear();
			// The menu art is lobby.msg (Ctrl-A); its option keys (M/P/L/H/Q) are
			// baked into the themeable file. Pass control keys to us so Ctrl-T/etc.
			// don't draw over it, but keep Ctrl-P with the BBS for node paging.
			console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
			var oldctrl = console.ctrlkey_passthru;
			console.ctrlkey_passthru = -1;
			console.ctrlkey_passthru = "-P";
			// CR/'?' return -> no action -> the loop redraws the menu.
			k = console.getkeys("\rMSLH?Q");
			console.ctrlkey_passthru = oldctrl;
		}
		console.clear();
		if (k == "Q")
			break;
		else if (k == "M")
			sd_multiplayer();
		else if (k == "S")
			sd_solo();
		else if (k == "L")
			sd_show_activity();
		else if (k == "H")
			sd_controls();
		// '?' / Enter: redraw
	}
}

sd_main();
