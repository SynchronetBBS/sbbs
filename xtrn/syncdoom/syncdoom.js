// syncdoom.js -- Synchronet lobby frontend for the syncdoom door.
//
// The xtrn entry point: presents the pre-game lobby and spawns the syncdoom C
// binary (located via js.exec_dir, in this same directory) to play. The model
// layer is in syncdoom_lib.js. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// MVP: single-player, create a co-op game, and join by address. Browse/registry
// discovery and deathmatch are TODO.
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

// Spawn a detached dedicated server for a match of 'maxplayers' on the given
// port (returns immediately; the server runs headless until the match empties).
// Uses the C -spawnserver mode so it survives this session. The server owns the
// match size, so it doesn't matter which client connects first.
function sd_spawn_server(port, maxplayers)
{
	bbs.exec(SD_BINARY + " -spawnserver -port " + port
	    + " -maxplayers " + maxplayers, EX_NATIVE, SD_DIR);
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

	sd_spawn_server(port, n);
	mswait(500);                          // let the server bind before we connect

	// For a multi-player game, tell the creator the join address so they can
	// relay it (until Browse lands). 1-player starts immediately, no pause.
	if (n > 1) {
		console.print("\r\n\1h\1gGame created \1n-- waiting for " + (n - 1)
		    + " more player(s).\r\n");
		console.print("They should choose \1hJ\1noin and enter:  \1h\1c127.0.0.1:"
		    + port + "\1n   (same host)\r\n");
		console.print("\r\nPress a key to enter the game and wait for them...");
		console.getkey();
	}

	// The creator joins their own server as the controlling player; the server
	// adopts this client's game settings (co-op) and starts when 'n' have joined.
	sd_play("127.0.0.1:" + port, ["-players", String(n)], sd_wadset_args(cfg, ws));
}

function sd_join()
{
	console.print("\r\nServer address (\1hhost:port\1n), blank to cancel: ");
	var addr = console.getstr("", 60, K_LINE);
	if (!addr || !sd_trim(addr).length)
		return;
	addr = sd_trim(addr);

	console.print("Select the WAD set this game is using:\r\n");
	var ws = sd_pick_wadset();            // must match the host's set
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
		console.print("   \1h\1yP\1n  Play single-player\r\n");
		console.print("   \1h\1yC\1n  Create a co-op network game\r\n");
		console.print("   \1h\1yJ\1n  Join a game by address\r\n");
		console.print("   \1h\1yQ\1n  Quit\r\n\r\n");
		console.print("   Command: ");

		var k = console.getkeys("PCJQ");
		if (k == "Q")
			break;
		else if (k == "P")
			sd_solo();
		else if (k == "C")
			sd_create();
		else if (k == "J")
			sd_join();
	}
}

sd_main();
