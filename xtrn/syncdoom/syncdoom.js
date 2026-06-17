// syncdoom.js -- Synchronet lobby frontend for the syncdoom door.
//
// The xtrn entry point: presents the pre-game lobby and spawns the syncdoom C
// binary (located via js.exec_dir, in this same directory) to play. The model
// layer is in syncdoom_lib.js. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Browse & join registry-discovered games, create a co-op game, single-player,
// or join an external server by address. Deathmatch and a waiting-room UI are TODO.
//
// Copyright(C) 2026 Rob Swindell / syncdoom. GPL-2.0.

load(js.exec_dir + "syncdoom_lib.js");

var cfg = sd_load_config();

// ---------------------------------------------------------------------------
// Launch helpers
// ---------------------------------------------------------------------------

// Build and run the door for a play session. 'connect' is "host:port" to join
// (or null for single-player); extra and wsargs are arrays of extra flags and
// the WAD args. bbs.cmdstr() expands %H/%T/%R (socket, time, rows) before exec.
function sd_play(connect, extra, wsargs)
{
	// %H/%T/%R = socket/time/rows; %A = the user's alias, auto-quoted by cmdstr
	// (so spaces and special characters are handled correctly).
	var cmd = SD_BINARY + " -s%H -t%T -l%R -name %A";
	if (connect)
		cmd += " -connect " + connect;
	var i;
	for (i = 0; i < extra.length; i++)
		cmd += " " + extra[i];
	for (i = 0; i < wsargs.length; i++)
		cmd += " " + wsargs[i];

	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN, SD_DIR);
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
	    + " -host %A"
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

	console.print("\r\n\1h\1cWAD sets:\1n\r\n");
	var i;
	for (i = 0; i < list.length; i++) {
		console.print(format("  \1h\1y%2d\1n. \1h%s\1n%s\r\n",
		    i + 1, list[i].name,
		    list[i].desc ? "  \1n\1w-- " + list[i].desc : ""));
	}
	console.print("\r\nChoose [\1h1-" + list.length + "\1n], \1hQ\1n to cancel: ");

	var k = console.getkeys("Q", list.length);
	if (k == "Q" || !k)
		return null;

	// The list only contains sets whose files are installed (sd_list_wadsets
	// filters on ws.present), so no per-pick presence check is needed here.
	return list[k - 1];
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
	var ws = sd_pick_wadset("coop");
	if (!ws)
		return;

	// Match size: how many players the game waits for before starting. 1 means
	// start immediately (a solo run, useful for testing); higher waits for that
	// many to join. (A proper waiting room is still TODO -- until then a game
	// of >1 shows a blank screen while it waits for the others.)
	console.print("\r\nNumber of players (\1h1\1n = start now, up to 4): ");
	var n = parseInt(console.getkeys("", 4), 10);
	if (!n || n < 1)
		n = 1;

	var port = sd_alloc_port(cfg);
	if (port < 0) {
		console.print("\r\n\1h\1rNo free server port available.\1n\r\n");
		console.pause();
		return;
	}

	sd_spawn_server(port, n, ws, "coop");
	mswait(500);                          // let the server bind before we connect

	// For a multi-player game, the others join via Browse -- it's now in the
	// registry. 1-player starts immediately, no pause.
	if (n > 1) {
		console.print("\r\n\1h\1gGame created \1n-- waiting for " + (n - 1)
		    + " more player(s) to \1hB\1nrowse and join.\r\n");
		console.print("\r\nPress a key to enter the game and wait for them...");
		console.getkey();
	}

	// The creator joins their own server as the controlling player; the server
	// adopts this client's game settings (co-op) and starts when 'n' have joined.
	sd_play("127.0.0.1:" + port, ["-players", String(n)], sd_wadset_args(cfg, ws));
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
		console.print(format(" \1h\1y%2d\1n %-17s %-11s %-13s %s/%-4s \1n\1w%s\1n\r\n",
		    i + 1, g.host, g.mode, g.wadset, g.players, g.maxplayers, g.status));
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

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

function sd_main()
{
	while (!js.terminated && bbs.online) {
		console.clear();
		console.print("\r\n   \1h\1rS Y N C D O O M\1n   \1n\1wnetwork lobby\1n\r\n\r\n");
		console.print("   \1h\1yB\1n  Browse & join a network game\r\n");
		console.print("   \1h\1yC\1n  Create a co-op network game\r\n");
		console.print("   \1h\1yP\1n  Play single-player\r\n");
		console.print("   \1h\1yJ\1n  Join an external server by address\r\n");
		console.print("   \1h\1yQ\1n  Quit\r\n\r\n");
		console.print("   Command: ");

		var k = console.getkeys("BCPJQ");
		if (k == "Q")
			break;
		else if (k == "B")
			sd_browse();
		else if (k == "C")
			sd_create();
		else if (k == "P")
			sd_solo();
		else if (k == "J")
			sd_join_external();
	}
}

sd_main();
