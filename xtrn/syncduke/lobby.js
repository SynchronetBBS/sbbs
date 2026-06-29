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
// Blocks until the door exits.
function sd_play(cmd) {
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
function sd_send_pages(targets, lev) {
	if (!targets || !targets.length)
		return;
	var who = (typeof user != "undefined" && user.alias) ? user.alias : "Someone";
	var body = "started a SyncDuke co-op game (" + lev.name
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
	sd_play(sd_cmd(null, null, null, null));
}

// Create a co-op match: pick a level, host it as master, advertise it in the
// registry, and enter the waiting room. The entry is removed when the door exits.
function sd_create() {
	var levnum = sd_pick_level();
	if (levnum === null)
		return;
	var lev = sd_find_level(levnum);

	var port = gl.alloc_port(cfg.net);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free game port available.\1n\r\n");
		console.pause();
		return;
	}

	// Decide paging up front (a blocking prompt), then write the registry entry and
	// deliver the pages before launching -- so a paged player sees the match listed.
	var page_targets = sd_prompt_page();

	var entry = sd_write_entry(cfg, port, lev.num);
	if (!entry) {
		console.print("\r\n\1h\1rCould not create the game (registry write failed).\1n\r\n");
		console.pause();
		return;
	}

	sd_send_pages(page_targets, lev);
	console.print("\r\n\1h\1gGame created:\1n \1wE1L" + lev.num + " " + lev.name
	    + "\1n. Entering the waiting room; another player can \1hJ\1noin.\r\n");

	// The creator's door IS the master (player 0). It binds and waits for a joiner,
	// then both play. We hold the registry entry for the match's lifetime and clear
	// it on exit (clean unwind -> finally; covers disconnect, timeout, normal quit).
	try {
		sd_play(sd_cmd("master", port, null, lev.num));
	} finally {
		gl.remove_game(entry);
	}
}

// Join a co-op match listed in the registry (this system or a same-LAN peer).
function sd_join() {
	var games = sd_list_open_games(cfg);
	if (!games.length) {
		console.print("\r\n\1h\1wNo co-op games are waiting.\1n\r\n");
		if (console.yesno("Create one now"))
			sd_create();
		return;
	}

	var sel;
	if (games.length == 1) {
		sel = games[0];
		console.print("\r\n\1h\1cJoining \1n\1w" + sel.host + "\1h\1c's game\1n ("
		    + "E1L" + sel.level + " " + sel.levelname + ")...\1n\r\n");
	} else {
		console.print("\r\n\1h\1cCo-op games waiting:\1n\r\n");
		console.print("    \1n\1wHost              Level\1n\r\n");
		var i;
		for (i = 0; i < games.length; i++) {
			var g = games[i];
			console.print(format(" \1h\1y%2d\1n %-17s E1L%s %s\r\n",
			    i + 1, g.host, g.level, g.levelname));
		}
		console.print("\r\nJoin which [\1h1-" + games.length + "\1n], \1hQ\1n to cancel: ");
		var k = console.getkeys("Q", games.length);
		if (k == "Q" || !k)
			return;
		sel = games[k - 1];
	}

	// Claim the only other slot: remove the entry so a third node doesn't also try to
	// join this strictly-2-player match (the master accepts just one join anyway).
	gl.remove_game(sel.file);
	sd_play(sd_cmd("join", null, sd_join_peer(sel), sel.level));
}

// Controls reference -- an external, sysop-editable display file (Ctrl-A codes)
// beside the door, mirroring SyncDOOM's controls.msg. printfile auto-pauses at EOF.
function sd_controls() {
	console.clear();
	console.printfile(SD_DIR + "controls.msg");
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

function sd_main() {
	while (!js.terminated && bbs.online) {
		console.clear();
		// The menu art is lobby.msg (Ctrl-A); its option keys (C/J/P/H/Q) are baked
		// into the themeable file. Pass control keys to us so Ctrl-T/Ctrl-U/etc. don't
		// draw over it, but keep Ctrl-P with the BBS for node paging. Restore after.
		console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
		var oldctrl = console.ctrlkey_passthru;
		console.ctrlkey_passthru = -1;
		console.ctrlkey_passthru = "-P";
		// CR/'?' return -> no action -> the loop redraws the menu.
		var k = console.getkeys("\rCJPH?Q");
		console.ctrlkey_passthru = oldctrl;
		console.clear();
		if (k == "Q")
			break;
		else if (k == "C")
			sd_create();
		else if (k == "J")
			sd_join();
		else if (k == "P")
			sd_solo();
		else if (k == "H")
			sd_controls();
		// '?' / Enter: redraw
	}
}

sd_main();
