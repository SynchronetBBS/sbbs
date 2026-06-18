// lobby.js -- Synchronet lobby frontend for the syncdoom door (optional).
//
// The xtrn entry point: presents the pre-game lobby and spawns the syncdoom C
// binary (located via js.exec_dir, in this same directory) to play. The door
// runs standalone without this lobby; it's the optional menu that adds Browse/
// Create/co-op. The model layer is in syncdoom_lib.js. SpiderMonkey
// 1.8.5-compatible (no modern ES).
//
// Browse & join registry-discovered games, create a co-op game, single-player,
// or (sysop opt-in) join an external server by address.
//
// Copyright(C) 2026 Rob Swindell / syncdoom. GPL-2.0.

load(js.exec_dir + "syncdoom_lib.js");

var cfg = sd_load_config();

// ---------------------------------------------------------------------------
// Launch helpers
// ---------------------------------------------------------------------------

// Build and run the door for a play session. 'connect' is "host:port" to join
// (or null for single-player); extra and wsargs are arrays of extra flags and
// the WAD args. bbs.cmdstr() expands the % specifiers before exec.
function sd_play(connect, extra, wsargs)
{
	// %H/%T = socket/time; %a = the alias, *lowercase* so cmdstr quotes
	// it when it contains a space -- which routes external() through the shell
	// (it sees the quote) so a multi-word alias survives as one argument. No -l:
	// the door auto-detects the screen size (live probe, then terminal.ini, which
	// it locates via the $SBBSNODE env var Synchronet sets -- no drop file needed).
	// -home gives the door a per-user writable sandbox for its config + saved
	// games (the door mkpath()s and chdir()s into it, so saves land in
	// <home>/.savegame/). Synchronet auto-cleans data/user/####/ when a user is
	// removed, so per-user door data belongs there -- data/user/####/doom/.
	// Without -home the door falls back to its launch cwd (the read-only install
	// dir) and the first save aborts the door with an I_Error.
	var home = system.data_dir + "user/" + format("%04u", user.number) + "/doom/";
	var cmd = SD_BINARY + " -s%H -t%T -name %a -home " + home;
	if (connect)
		cmd += " -connect " + connect;
	var i;
	for (i = 0; i < extra.length; i++)
		cmd += " " + extra[i];
	for (i = 0; i < wsargs.length; i++)
		cmd += " " + wsargs[i];

	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN, SD_DIR);

	// The door writes frames straight to the socket, bypassing Synchronet's
	// console line counter. That leaves the counter stale, so the next screen
	// (the menu redraw) thinks the page is full and fires an errant [Hit a key]
	// pause -- invisible under JXL (the full-screen image covers it) but plainly
	// visible in sixel mode. Reset the counter so the menu paints cleanly.
	console.line_counter = 0;
}

// Spawn a detached dedicated server for a match on the given port (returns
// immediately; runs headless until the match empties). Uses -spawnserver so it
// survives this session. The server owns the match size (connect order doesn't
// matter) and writes the registry entry the lobby discovers -- so we hand it the
// games dir (the C door isn't Synchronet-aware) plus the match metadata. %A is
// the auto-quoted alias.
function sd_spawn_server(port, maxplayers, ws, mode)
{
	var cmd = SD_BINARY + " -spawnserver -port " + port
	    + " -maxplayers " + maxplayers
	    + " -gamesdir " + SD_GAMES
	    + " -host %a"                     // lowercase: quoted if it has a space
	    + " -wadset " + ws.id
	    + " -gamemode " + mode;

	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE, SD_DIR);
}

// ---------------------------------------------------------------------------
// Pickers
// ---------------------------------------------------------------------------

// Present the WAD-set picker for 'mode' (or all). Returns a wadset or null.
function sd_pick_wadset(mode)
{
	var list = sd_list_wadsets(cfg, mode);
	if (!list.length) {
		console.print("\r\nNo WAD sets are configured.\r\n");
		console.pause();
		return null;
	}
	if (list.length == 1)
		return list[0];               // only one choice -- don't prompt

	// uselect(): arrow/number-navigable picker. Items are 0-based; uselect() with
	// no args displays the menu and returns the chosen index, or <0 if the user
	// backs out. The list only holds sets whose files are installed
	// (sd_list_wadsets filters on ws.present), so no per-pick presence check here.
	var i;
	for (i = 0; i < list.length; i++)
		console.uselect(i, "WAD set", list[i].name
		    + (list[i].desc ? "  -- " + list[i].desc : ""));
	var sel = console.uselect();
	if (sel < 0)
		return null;
	return list[sel];
}

// Present the game-mode picker. Returns the mode id ("coop"/"deathmatch"/
// "altdeath") or null. Deathmatch & altdeath reach Doom as -deathmatch /
// -altdeath on the *controller's* (creator's) command line; joiners inherit the
// mode from the server, and the dedicated server records it for the browse list.
function sd_pick_gamemode()
{
	var modes = [
		{ id: "coop",       name: "Co-op  (team vs. monsters)" },
		{ id: "deathmatch", name: "Deathmatch" },
		{ id: "altdeath",   name: "Deathmatch 2.0  (weapons & items respawn)" }
	];
	var i;
	for (i = 0; i < modes.length; i++)
		console.uselect(i, "Game mode", modes[i].name);
	var sel = console.uselect();
	if (sel < 0)
		return null;
	return modes[sel].id;
}

// ---------------------------------------------------------------------------
// Menu actions
// ---------------------------------------------------------------------------

function sd_solo()
{
	var ws = sd_pick_wadset("solo");
	if (ws)
		sd_play(null, [], sd_wadset_args(cfg, ws));
}

function sd_create()
{
	var mode = sd_pick_gamemode();
	if (!mode)
		return;
	var ws = sd_pick_wadset(mode);
	if (!ws)
		return;

	// Match size: how many players the game waits for before starting. 1 means
	// start immediately (a solo run, useful for testing); higher waits for that
	// many to join. (A proper waiting room is still TODO -- until then a game
	// of >1 shows a blank screen while it waits for the others.)
	// Player cap: the wadset's maxplayers if set, else [net] max_players, never
	// above Doom's NET_MAXPLAYERS (8).
	var maxp = parseInt(ws.maxplayers || cfg.net.max_players, 10) || 8;
	if (maxp > 8) maxp = 8;
	if (maxp < 1) maxp = 1;
	console.print("\r\nNumber of players (\1h1\1n=start now, up to " + maxp
	    + ", \1hQ\1n=abort) [\1h2\1n]: ");
	var n = console.getnum(maxp);
	if (n < 0)                            // Q / Ctrl-C -> abort the create
		return;
	if (n < 1)                            // bare Enter -> default to 2 players
		n = (maxp >= 2) ? 2 : maxp;

	var port = sd_alloc_port(cfg);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free server port available.\1n\r\n");
		console.pause();
		return;
	}

	sd_spawn_server(port, n, ws, mode);
	mswait(500);                          // let the server bind before we connect

	// Enter the door and connect right away -- the creator MUST reach the server
	// before any Browse-joiner does: the first client to connect becomes the
	// controller (the host who starts the match). The old "press a key to enter"
	// pause held the creator outside the door, so a joiner could connect first,
	// become host, and launch without the creator -- whose late connect then hit
	// an in-progress server (no mid-game joins) and bounced back to the lobby.
	// The door's own waiting room shows the "waiting for players" UI from here.
	if (n > 1)
		console.print("\r\n\1h\1gGame created \1n-- entering the waiting room; "
		    + "others \1hB\1nrowse & join.\r\n");

	// The creator is the controller, so its -deathmatch / -altdeath sets the match
	// type; co-op needs no flag (Doom's default). Joiners get the mode from the server.
	var extra = ["-players", String(n)];
	if (mode == "deathmatch")
		extra.push("-deathmatch");
	else if (mode == "altdeath")
		extra.push("-altdeath");
	sd_play("127.0.0.1:" + port, extra, sd_wadset_args(cfg, ws));
}

// Browse the registry and join a game on this system. The joiner inherits the
// host's WAD set from the registry -- no address typing, no WAD-set guessing.
function sd_browse()
{
	var games = sd_list_games(cfg);
	if (!games.length) {
		console.print("\r\n\1h\1wNo network games are running.\1n  Use \1hC\1nreate to start one.\r\n");
		console.pause();
		return;
	}

	console.print("\r\n\1h\1cNetwork games:\1n\r\n");
	console.print("    \1n\1wHost              Mode        WAD set       Players  Status\1n\r\n");
	var i;
	for (i = 0; i < games.length; i++) {
		var g = games[i];
		// players/max as one fixed-width field so Status lines up under its header.
		console.print(format(" \1h\1y%2d\1n %-17s %-11s %-13s %-7s  \1n\1w%s\1n\r\n",
		    i + 1, g.host, g.mode, g.wadset, g.players + "/" + g.maxplayers, g.status));
	}
	console.print("\r\nJoin which [\1h1-" + games.length + "\1n], \1hQ\1n to cancel: ");

	var k = console.getkeys("Q", games.length);
	if (k == "Q" || !k)
		return;
	var sel = games[k - 1];

	// Vanilla Doom only accepts joins before the game starts.
	if (sel.status != "lobby") {
		console.print("\r\n\1h\1rThat game is already in progress -- you can't join it.\1n\r\n");
		console.pause();
		return;
	}

	// Inherit the host's WAD set (look it up by id, then verify it's installed).
	var ws = sd_find_wadset(cfg, sel.wadset);
	if (!ws) {
		console.print("\r\n\1h\1rThis system has no WAD set '" + sel.wadset
		    + "' -- can't join.\1n\r\n");
		console.pause();
		return;
	}
	if (!sd_wadset_files_present(cfg, ws)) {
		console.print("\r\n\1h\1rThe WAD files for '" + ws.name
		    + "' are not installed here.\1n\r\n");
		console.pause();
		return;
	}

	sd_play(sel.addr + ":" + sel.port, [], sd_wadset_args(cfg, ws));
}

// Join an arbitrary server by address (external / cross-system). Manual because
// there's no registry entry to inherit the WAD set from.
function sd_join_external()
{
	console.print("\r\nServer address (\1hhost:port\1n), blank to cancel: ");
	var addr = console.getstr("", 60, K_LINE);
	if (!addr || !sd_trim(addr).length)
		return;
	addr = sd_trim(addr);

	console.print("Select the WAD set this game is using (must match the host):\r\n");
	var ws = sd_pick_wadset();
	if (ws)
		sd_play(addr, [], sd_wadset_args(cfg, ws));
}

// Controls reference -- an external, sysop-editable display file (Ctrl-A codes)
// beside the door: edit/translate/retheme controls.msg without touching JS.
// Mirrors the in-game F1 help (m_menu.c M_DrawReadThis1), which has to stay
// C-drawn since it renders into the Doom frame. Shown on demand from the menu --
// NOT auto-printed before a launch, which would delay the co-op creator's
// connect and lose the host/controller race.
function sd_controls()
{
	console.clear();
	console.printfile(SD_DIR + "controls.msg");   // auto-pause when a page fills
	console.pause();
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

function sd_main()
{
	// Join-by-address is for external/cross-system servers; off unless the sysop
	// opts in with [net] allow_external = true in syncdoom.ini.
	var allow_ext = (cfg.net.allow_external === true
	                 || cfg.net.allow_external === "true");

	while (!js.terminated && bbs.online) {
		console.clear();
		// The lobby menu is lobby.msg -- DOOM ANSI art (by "cool t" / tdd, converted
		// to Ctrl-A with PabloDraw), its B/C/P/H/Q options remapped to our actions.
		// The art is the menu, so there's no command prompt. The optional external-
		// join (J) has no art slot, so show a one-line hint only when it's enabled.
		console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
		if (allow_ext)
			console.print("\r\n   \1h\1yJ\1n = join an external server\r\n");

		var k = console.getkeys("BCPH?" + (allow_ext ? "J" : "") + "Q");
		console.clear();                 // wipe the art before the chosen action draws
		if (k == "Q")
			break;
		else if (k == "B")
			sd_browse();
		else if (k == "C")
			sd_create();
		else if (k == "P")
			sd_solo();
		else if (k == "H" || k == "?")
			sd_controls();
		else if (k == "J" && allow_ext)
			sd_join_external();
	}
}

sd_main();
