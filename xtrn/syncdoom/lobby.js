// lobby.js -- Synchronet lobby frontend for the syncdoom door (optional).
//
// The xtrn entry point: presents the pre-game lobby and spawns the syncdoom C
// binary (located via js.exec_dir, in this same directory) to play. The door
// runs standalone without this lobby; it's the optional menu that adds Browse/
// Create/co-op. The model layer is in syncdoom_lib.js. SpiderMonkey
// 1.8.5-compatible (no modern ES).
//
// Join registry-discovered games, create a multi-player game, single-player,
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
	var cmd = SD_BINARY + " -s%H -t%T -name %a -home " + home
	        + ' -eventlog "' + system.data_dir + 'syncdoom/events.jsonl"';
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

// The configured advertise address (trimmed), or "" if unset/blank. Resolved
// from cfg.net.advertise, which sd_load_config() has already overlaid with any
// host-specific [net:<hostname>] value.
function sd_advertise_addr()
{
	var a = cfg.net.advertise;
	if (a === undefined || a === null)
		return "";
	return sd_trim(String(a));
}

// The configured server bind (listen) address, or "" if unset/blank -> the
// server's own 127.0.0.1 default (same-host play only). Distinct from advertise:
// bind is the local interface the socket accepts on; advertise is the address
// peers dial. Resolved from cfg.net.bind, host-overlaid like advertise.
function sd_bind_addr()
{
	var b = cfg.net.bind;
	if (b === undefined || b === null)
		return "";
	return sd_trim(String(b));
}

// Address the match creator dials to reach the server it just spawned. The
// creator always shares the server's host, but the server binds the address it
// listens on (sd_bind_addr() || advertise; its own 127.0.0.1 default when both
// are blank). A socket bound to a SPECIFIC interface IP does not receive
// loopback datagrams, so the creator can't blindly use 127.0.0.1 -- it must dial
// the very address the server bound (a host can always reach its own local IP).
// A wildcard bind (0.0.0.0/any/*) or an unset bind both accept on loopback, so
// dial 127.0.0.1 there (you can't send to a wildcard; loopback is guaranteed).
function sd_creator_connect_host()
{
	var bind = sd_bind_addr() || sd_advertise_addr();
	var lc = String(bind).toLowerCase();
	if (!bind || lc == "0.0.0.0" || lc == "any" || lc == "*")
		return "127.0.0.1";
	return bind;
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

	// advertise = the address joiners on OTHER hosts dial (recorded in the shared
	// browse registry); bind = the local interface the server listens on. They're
	// independent. Both blank -> server defaults to loopback (same-host only).
	// For cross-host play set BOTH: bind (0.0.0.0, or this host's LAN IP) so the
	// socket accepts off-box, and advertise (LAN IP / public name) so the registry
	// tells joiners where to dial. Per-host [net:<hostname>] overlays apply to each.
	var adv = sd_advertise_addr();
	if (adv)
		cmd += " -advertise " + adv;
	// bind defaults to the advertise address: if the sysop said "peers reach me at
	// X" (advertise) but didn't say where to listen (bind), listen on X. An explicit
	// [net] bind always wins -- needed when advertise is a public/NAT name this host
	// can't bind directly (set bind to 0.0.0.0 or the local LAN IP), or to widen to
	// all interfaces. Both blank -> the server's own 127.0.0.1 (same-host) default.
	var bind = sd_bind_addr() || adv;
	if (bind)
		cmd += " -bindaddr " + bind;

	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE, SD_DIR);
}

// ---------------------------------------------------------------------------
// Pickers
// ---------------------------------------------------------------------------

// Show a WAD set's pre-launch caveat (the [wadset:*] `note`), if any, and wait
// for a keypress so the player sees it before the door launches.
function sd_show_note(ws)
{
	if (!ws || !ws.note)
		return;
	console.print("\r\n\1h\1y" + ws.name + ":\1n \1w" + ws.note + "\1n\r\n");
	console.pause();
}

// Display any "<wadname>.msg" file sitting beside a WAD in the set (a long-form
// companion to a `note` -- e.g. a map pack's readme/intro). Shown a page at a
// time, with a pause, before the door launches. Checks the set's IWAD, PWADs,
// and merge WADs (each "foo.wad" -> "foo.msg" in the WAD dir), each at most once.
function sd_show_wad_msgs(cfg, ws)
{
	if (!ws)
		return;
	var dir = sd_wad_dir(cfg);
	var wads = [];
	if (ws.iwad)
		wads.push(ws.iwad);
	var fields = [ws.pwad, ws.merge];
	var f, i;
	for (f = 0; f < fields.length; f++) {
		if (!fields[f])
			continue;
		var list = String(fields[f]).split(",");
		for (i = 0; i < list.length; i++) {
			var n = sd_trim(list[i]);
			if (n.length)
				wads.push(n);
		}
	}

	var seen = {};
	for (i = 0; i < wads.length; i++) {
		var dot = wads[i].lastIndexOf(".");
		var base = (dot > 0) ? wads[i].substr(0, dot) : wads[i];
		var path = dir + base + ".msg";
		var key = path.toLowerCase();
		if (seen[key])
			continue;
		seen[key] = true;
		if (file_exists(path)) {
			console.clear();
			console.printfile(path);      // auto-pause when a page fills
			console.pause();
		}
	}
}

// Present the WAD-set picker for 'mode' (or all). Returns a wadset or null.
function sd_pick_wadset(mode)
{
	var list = sd_list_wadsets(cfg, mode);
	if (!list.length) {
		console.print("\r\nNo WAD sets are configured.\r\n");
		console.pause();
		return null;
	}
	var ws;
	if (list.length == 1) {
		ws = list[0];                 // only one choice -- don't prompt
	} else {
		// uselect(): arrow/number-navigable picker. Items are 0-based; the final
		// uselect(n) call displays the menu, pre-selecting item n -- the [wads]
		// `default` set when configured -- and returns the chosen index, or <0 if
		// the user backs out. The list only holds sets whose files are installed
		// (sd_list_wadsets filters on ws.present), so no per-pick presence check.
		var i;
		var def = 0;
		// Fit each item to the real terminal width. uselect frames a line as
		// "->(N) <label> <-", so reserve ~10 cols for that decoration. Show
		// "name -- desc" only when the whole thing fits; otherwise just the name
		// (a half-truncated description reads worse than none). Hard-trim only if
		// even the name alone overflows.
		var avail = (console.screen_columns || 80) - 10;
		for (i = 0; i < list.length; i++) {
			var label = list[i].name;
			if (list[i].desc) {
				var full = label + " - " + list[i].desc;
				if (full.length <= avail)
					label = full;
			}
			if (label.length > avail)
				label = sd_trim(label.substr(0, avail - 3)) + "...";
			console.uselect(i, "WAD set", label);
			if (cfg.wads.default && list[i].id == cfg.wads.default)
				def = i;
		}
		var sel = console.uselect(def);
		if (sel < 0)
			return null;
		ws = list[sel];
	}
	sd_show_note(ws);
	sd_show_wad_msgs(cfg, ws);
	return ws;
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

// Resolve the default skill (1-5) the Create prompt should start on: the wadset's
// own [wadset:*] skill if set, else the [net] skill house default, else 3 (Hurt
// Me Plenty -- Doom's own default). Clamped to the valid 1-5 range.
function sd_default_skill(ws)
{
	var s = (ws && ws.skill !== undefined && ws.skill !== "") ? parseInt(ws.skill, 10)
	      : parseInt(cfg.net.skill, 10);
	if (isNaN(s) || s < 1) s = 3;
	if (s > 5)             s = 5;
	return s;
}

// Prompt for the match skill level. Returns 1-5, or null to abort. Each item's
// uselect value is the Doom skill number, and the execute call's argument is the
// default-highlighted value (Enter selects it) -- so `def` (wadset/[net]) is the
// initial choice. Doom takes the skill on the *controller's* (creator's) command
// line as -skill N; joiners inherit it through the netgame, like the DM flags.
function sd_pick_skill(def)
{
	var names = [
		"I'm Too Young To Die",
		"Hey, Not Too Rough",
		"Hurt Me Plenty",
		"Ultra-Violence",
		"Nightmare!"
	];
	var i;
	for (i = 0; i < names.length; i++)
		console.uselect(i + 1, "Skill level", names[i]);
	var sel = console.uselect(def);       // execute, default-highlighting `def`
	if (sel < 0)                          // Q / Ctrl-C -> abort the create
		return null;
	return sel;
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

// The access-requirement string for this door's own external-program entry, so
// we only page users who may actually run it. Found by matching the lobby's own
// directory + a JS-module command ("?lobby") in the external-programs config.
// "" (the default when not found) makes compare_ars() treat everyone as allowed.
function sd_door_ars()
{
	if (typeof xtrn_area == "undefined" || !xtrn_area.prog_list)
		return "";
	function strip(s) { return s ? String(s).replace(/[\/\\]+$/, "") : ""; }
	var ours = strip(fullpath(SD_DIR));
	var list = xtrn_area.prog_list, i, p, fallback = "";
	for (i = 0; i < list.length; i++) {
		p = list[i];
		if (!p.startup_dir || strip(fullpath(p.startup_dir)) != ours)
			continue;
		if (p.cmd && p.cmd.charAt(0) == "?")   // the JS lobby -- the entry users launch
			return p.execution_ars || "";
		fallback = p.execution_ars || fallback; // else the native door's reqs
	}
	return fallback;
}

function sd_mode_label(mode)
{
	if (mode == "deathmatch") return "deathmatch";
	if (mode == "altdeath")   return "deathmatch (altdeath)";
	return "co-op";
}

// Find other active nodes whose users can run this door (the page candidates).
// Nodes in quiet mode / WFC / logon are skipped, as is our own node.
function sd_page_targets()
{
	var ars = sd_door_ars();
	var me  = bbs.node_num;
	var list = system.node_list, targets = [], i;
	for (i = 0; i < list.length; i++) {
		var node_num = i + 1;
		if (node_num == me)
			continue;
		var nd = list[i];
		if (nd.status != NODE_INUSE)          // skip WFC/quiet/logon/offline nodes
			continue;
		if (!nd.useron)
			continue;
		var u;
		try { u = new User(nd.useron); } catch (e) { continue; }
		if (!u || !u.number)
			continue;
		if (ars && !u.compare_ars(ars))       // honor the door's access requirements
			continue;
		targets.push(node_num);
	}
	return targets;
}

// Ask (up front, BEFORE the server is spawned) whether to page the available
// players. Returns the node list to page, or [] if none/declined. The prompt is
// deliberately kept out of the spawn->connect window: a blocking prompt there
// would let a Browse-joiner connect first and steal the host (controller) slot.
function sd_prompt_page_players()
{
	var targets = sd_page_targets();
	if (!targets.length)
		return [];
	if (!console.yesno("\r\nPage " + targets.length + " active node"
	    + (targets.length == 1 ? "" : "s") + " to join"))
		return [];
	return targets;
}

// Deliver the join invitation to the chosen nodes -- fast and non-interactive,
// so it's safe to call in the spawn->connect window. Uses the sysop-configurable
// NodeMsgFmt header (text.dat) so it looks like any other inter-node message.
function sd_send_pages(targets, mode)
{
	if (!targets || !targets.length)
		return;
	var who = (typeof user != "undefined" && user.alias) ? user.alias : "Someone";
	// NodeMsgFmt is the sysop-configurable "Node N: <who> sent you a message:"
	// header + body template: %2d=node, %s=sender, %s=body (it supplies its own
	// coloring and trailing CRLF, so the body is passed as plain text).
	var body = "started a SyncDOOM " + sd_mode_label(mode)
	    + " game; run SyncDOOM to join.";
	var msg = format(bbs.text("NodeMsgFmt"), bbs.node_num, who, body);
	var paged = 0, i;
	for (i = 0; i < targets.length; i++)
		if (system.put_node_message(targets[i], msg))
			paged++;
	console.print("\r\n\1n\1cPaged \1h" + paged + "\1n\1c node"
	    + (paged == 1 ? "" : "s") + ".\1n\r\n");
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
	// above Doom's MAXPLAYERS (4) -- the game has only 4 player slots/colors/starts.
	// (NET_MAXPLAYERS is 8, but that's the protocol envelope incl. observers, not
	// the Doom player limit; a 5th player would overrun players[]/playeringame[].)
	var maxp = parseInt(ws.maxplayers || cfg.net.max_players, 10) || 4;
	if (maxp > 4) maxp = 4;
	if (maxp < 1) maxp = 1;
	console.print("\r\nNumber of players (\1h1\1n=start now, up to " + maxp
	    + ", \1hQ\1n=abort) [\1h2\1n]: ");
	var n = console.getnum(maxp);
	if (n < 0)                            // Q / Ctrl-C -> abort the create
		return;
	if (n < 1)                            // bare Enter -> default to 2 players
		n = (maxp >= 2) ? 2 : maxp;

	// Skill level. Like the player count, this is a blocking prompt that MUST be
	// resolved before we spawn the server and connect, or a Browse-joiner could
	// win the controller race while we sit at the prompt. Default from the wadset
	// (or [net]) ini. Lobby-driven matches autostart (-warp), bypassing Doom's
	// own skill menu, so this is where difficulty gets chosen.
	var skill = sd_pick_skill(sd_default_skill(ws));
	if (skill === null)                   // Q / Ctrl-C -> abort the create
		return;

	// Decide on paging up front -- the (blocking) prompt must NOT sit between the
	// server spawn and our connect below, or a Browse-joiner could connect first
	// and become the controller. We only deliver the pages (fast) after spawning.
	var page_targets = (n > 1) ? sd_prompt_page_players() : [];

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
	if (n > 1) {
		console.print("\r\n\1h\1gGame created.\1n Entering the waiting room; "
		    + "others \1hJ\1noin a multi-player game.\r\n");
		sd_send_pages(page_targets, mode);
	}

	// The creator is the controller, so its -deathmatch / -altdeath AND -skill set
	// the match type/difficulty; co-op needs no mode flag (Doom's default). Joiners
	// get the mode and skill from the server (controller's connect data).
	var extra = ["-players", String(n), "-skill", String(skill)];
	if (mode == "deathmatch")
		extra.push("-deathmatch");
	else if (mode == "altdeath")
		extra.push("-altdeath");
	sd_play(sd_creator_connect_host() + ":" + port, extra, sd_wadset_args(cfg, ws));
}

// Browse the registry and join a game on this system. The joiner inherits the
// host's WAD set from the registry -- no address typing, no WAD-set guessing.
function sd_browse()
{
	var games = sd_list_games(cfg);
	if (!games.length) {
		console.print("\r\n\1h\1wNo network games are running.\1n\r\n");
		if (console.yesno("Create a game now"))
			sd_create();
		return;
	}

	var sel;
	if (games.length == 1) {
		// Only one game running -- skip the list and the "join which?" prompt and
		// just join it (still validated below).
		sel = games[0];
		console.print("\r\n\1h\1cJoining \1n\1w" + sel.host + "\1h\1c's game\1n ("
		    + sel.wadset + ", " + sel.mode + ", " + sel.players + "/"
		    + sel.maxplayers + ")...\1n\r\n");
	} else {
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
		sel = games[k - 1];
	}

	// Vanilla Doom only accepts joins before the game starts.
	if (sel.status != "lobby") {
		console.print("\r\n\1h\1rThat game is already in progress; you can't join it.\1n\r\n");
		console.pause();
		return;
	}

	// Inherit the host's WAD set (look it up by id, then verify it's installed).
	var ws = sd_find_wadset(cfg, sel.wadset);
	if (!ws) {
		console.print("\r\n\1h\1rThis system has no WAD set '" + sel.wadset
		    + "'; can't join.\1n\r\n");
		console.pause();
		return;
	}
	if (!sd_wadset_files_present(cfg, ws)) {
		console.print("\r\n\1h\1rThe WAD files for '" + ws.name
		    + "' are not installed here.\1n\r\n");
		console.pause();
		return;
	}

	sd_show_note(ws);
	sd_show_wad_msgs(cfg, ws);
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
	// printfile auto-pauses when the page fills -- that IS the "press a key"
	// before we return and the menu redraws over it, so no explicit pause here
	// (which would double up). controls.msg is sized to ~one screen so the pause
	// lands after the last line rather than mid-list.
	console.printfile(SD_DIR + "controls.msg");
}

// ---------------------------------------------------------------------------
// Main menu
// ---------------------------------------------------------------------------

// Full-screen DOOM ANSI attract, shown once on lobby entry. The art is sysop-
// provided (drop *.ans in the [lobby] art_dir); a random one is paged, then any
// key drops into the menu. Silent (and skipped) when no art is installed, or when
// [lobby] attract = false. Tall art (the classic 48-row portraits) pages normally.
function sd_attract()
{
	if (cfg.lobby.attract === false || cfg.lobby.attract === "false")
		return;
	var files = sd_attract_files(cfg);
	if (!files.length)
		return;
	console.clear();
	console.printfile(files[random(files.length)]);   // pages + pauses at EOF
	console.line_counter = 0;                          // clean slate for the menu draw
}

// Recent-activity view: frags, level clears and deaths from the door event log.
function sd_show_activity()
{
	console.clear();
	console.print("\1h\1wSyncDOOM \1y- \1wrecent activity\1n\r\n\r\n");
	var feed = sd_event_feed(18);
	if (!feed.length)
		console.print("  \1k\1h(nothing yet -- go play a game!)\1n\r\n");
	else {
		var i;
		for (i = 0; i < feed.length; i++)
			console.print("  " + feed[i] + "\r\n");
	}
	console.print("\r\n");
	console.pause();
}

// --- live lobby panel ------------------------------------------------------
// Draw the full lobby art (its bottom rows are left blank for the panel).
function sd_draw_art()
{
	console.clear();
	console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
}

// Repaint the live who's-online/activity panel, bottom-anchored and growing
// upward over the art as more nodes appear. Only the panel rows are rewritten
// each tick (no art redraw -> no flicker); when the panel's height changes the
// art is redrawn first so rows it vacated don't keep a stale panel.
var sd_panel_rows_prev = -1;
function sd_draw_panel()
{
	var cols = console.screen_columns, rows = console.screen_rows;
	var p = sd_panel_cells(cols);
	var cells = p.cells, nrows = cells.length;   // one full-width cell per row
	if (sd_panel_rows_prev >= 0 && nrows != sd_panel_rows_prev)
		sd_draw_art();                       // height changed -> restore art under it
	sd_panel_rows_prev = nrows;
	var top = rows - nrows + 1, r;
	for (r = 0; r < nrows; r++) {
		console.gotoxy(1, top + r);
		console.print("\1n");
		console.cleartoeol();
		console.print(cells[r]);
	}
	console.gotoxy(1, rows);                  // park the cursor out of the panel body
}

// Live lobby: refresh the panel ~1/s and return the first valid menu key. The
// blocking getkeys() used in the static menu calls nodesync() for us (telegrams,
// sysop interrupt, forced chat); the poll loop must do it explicitly.
function sd_lobby_wait(allow_ext)
{
	var allowed = "JCPLHQ" + (allow_ext ? "E" : "");   // '?'/Enter -> redraw (handled below)
	var mynode = bbs.node_num;
	// Pass control keys to us so the BBS's built-in Ctrl-T/Ctrl-U/etc. don't draw
	// over the live panel (the poll loop ignores them) -- but keep Ctrl-P with the
	// BBS so node paging still works from the lobby. Restored on exit.
	var oldctrl = console.ctrlkey_passthru;
	console.ctrlkey_passthru = -1;
	console.ctrlkey_passthru = "-P";
	sd_panel_rows_prev = -1;                  // first draw shouldn't trigger an art redraw
	try {
		while (!js.terminated && bbs.online) {
			// A waiting telegram/short-message prints to the screen, so show it on a
			// cleared screen and pause, then restore the lobby. nodesync() also handles
			// sysop interrupt (-> hangup) and forced local chat.
			var pending = (system.node_list[mynode - 1].misc & (NODE_NMSG | NODE_MSGW)) != 0;
			if (pending) {
				console.clear();
				bbs.nodesync();              // displays the waiting message(s)
				console.pause();
				sd_draw_art();
				sd_panel_rows_prev = -1;
			} else
				bbs.nodesync();              // sync node record / handle interrupt
			if (js.terminated || !bbs.online)
				break;                       // nodesync() may have hung us up
			sd_draw_panel();
			var c = console.inkey(K_UPPER | K_NOECHO, 1000);
			if (!c)
				continue;                    // timeout -> refresh
			if (c == "\r" || c == "\n" || c == "?") {   // redraw the lobby menu art
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

function sd_main()
{
	mkpath(system.data_dir + "syncdoom/");   // door's -eventlog append won't create it
	sd_prune_events(2000, 1000);      // bound the activity log on entry (rare rewrite)

	// Join-by-address is for external/cross-system servers; off unless the sysop
	// opts in with [net] allow_external = true in syncdoom.ini.
	var allow_ext = (cfg.net.allow_external === true
	                 || cfg.net.allow_external === "true");
	// The live who's-online/activity panel is opt-in ([lobby] live = true): it
	// repaints the bottom rows ~1/s, so it suits ANSI terminals only.
	var live = (cfg.lobby
	            && (cfg.lobby.live === true || cfg.lobby.live === "true"));

	sd_attract();                    // optional one-shot DOOM ANSI splash on entry

	while (!js.terminated && bbs.online) {
		var k;
		// The lobby menu is lobby.msg -- DOOM ANSI art (by "cool t" / tdd, converted
		// to Ctrl-A with PabloDraw), its J/C/P/H/Q options remapped to our actions.
		if (live) {
			// The art's bottom blank lines (and rows above, as needed) host the live
			// panel; the panel loop draws the art and polls for a key.
			sd_draw_art();
			k = sd_lobby_wait(allow_ext);
		} else {
			console.clear();
			// The art IS the menu: its option keys are baked into the themeable
			// lobby.msg (J/C/P/L/H/Q, plus E where the sysop enables external joins
			// and adds it). No hardcoded hint strings here.
			console.printfile(SD_DIR + "lobby.msg", P_NOPAUSE);
			// Pass control keys to us so Ctrl-T/Ctrl-U/etc. don't draw over the art,
			// but keep Ctrl-P with the BBS for node paging. Restore after.
			var oldctrl = console.ctrlkey_passthru;
			console.ctrlkey_passthru = -1;
			console.ctrlkey_passthru = "-P";
			// CR is in the set so Enter returns (-> no action -> the loop redraws);
			// '?' likewise falls through to a redraw.
			k = console.getkeys("\rJCPLH?" + (allow_ext ? "E" : "") + "Q");
			console.ctrlkey_passthru = oldctrl;
		}
		console.clear();                 // wipe the art before the chosen action draws
		if (k == "Q")
			break;
		else if (k == "J")
			sd_browse();
		else if (k == "C")
			sd_create();
		else if (k == "P")
			sd_solo();
		else if (k == "L")
			sd_show_activity();
		else if (k == "H")
			sd_controls();
		// '?' or Enter: no action -> the loop top redraws lobby.msg
		else if (k == "E" && allow_ext)
			sd_join_external();
	}
}

sd_main();
