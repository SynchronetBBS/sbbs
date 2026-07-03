// syncduke_lib.js -- model layer for the SyncDuke lobby (no UI, no bbs/console
// prompting, so it can be exercised headless with jsexec). Loaded by lobby.js;
// layers the Duke-specific bits (config, levels, the door command line, the
// registry entry shape) over the shared game_lobby.js. SpiderMonkey 1.8.5.
//
// SyncDuke v1 networking is peer master/join (proven 2-player LAN co-op), not a
// dedicated server: the CREATOR's door runs as the "master" (player 0) and binds
// 0.0.0.0:<port>; a JOINER's door runs as player 1 and dials the master. The C
// door isn't registry-aware, so THIS lobby writes the registry entry before the
// master launches and removes it when the door exits (or when a joiner claims the
// only other slot). Because the master binds all interfaces, a joiner simply dials
// the entry's advertised address, or 127.0.0.1 for same-host play.
//
// Copyright(C) 2026 Rob Swindell / SyncDuke. GPL-2.0.

// Loaded default-form at lobby.js's top level, so these land in the lobby's scope:
// EX_NATIVE/EX_BIN (bbs.exec), NODE_*/K_*/P_* (menu + node helpers).
load("sbbsdefs.js");

var gl = load({}, "game_lobby.js");
var presence = load({}, "presence_lib.js");   // stock node-presence lib (set_node_ext_status)

// The door's own directory (where the syncduke binary + syncduke.ini live):
// js.exec_dir is the directory of the running script -- xtrn/syncduke/ here.
var SD_DIR    = js.exec_dir;
// "%." is a Synchronet cmdstr specifier: ".exe" on Windows, blank on *nix, so one
// config runs syncduke.exe on Windows yet a non-Windows "syncduke" in the same dir.
var SD_BINARY = SD_DIR + "syncduke%.";
var SD_CFG    = SD_DIR + "syncduke.ini";
// Shared game registry (this lobby writes/reads it; same-LAN hosts may share it
// over the mount, like SyncDOOM's). Created on demand.
var SD_GAMES  = backslash(system.data_dir + "syncduke/games/");
// Door-written activity log (start/level/death); read by the recent-activity view.
// The C door appends here when launched with -eventlog (see sd_cmd); the lobby
// ensures the parent dir exists and prunes it. Same dir convention as SyncDOOM's.
var SD_EVENTS = system.data_dir + "syncduke/events.jsonl";
// Seconds between the waiting room's registry re-writes (heartbeat). Must be well
// under [net] stale (default 90) so a joiner keeps seeing the game during a long wait.
var SD_WAIT_HEARTBEAT = 20;

// Episode 1 ("L.A. Meltdown") co-op levels of the shareware DUKE3D.GRP. `num` is
// the 1-based level the engine warps to via /l<num> (/v1 selects the episode).
// E1L7+ are dukematch-only maps, excluded from co-op. (Names per the GRP's
// GAME.CON definelevelname table.)
var SD_LEVELS = [
	{ num: 1, name: "Hollywood Holocaust" },
	{ num: 2, name: "Red Light District" },
	{ num: 3, name: "Death Row" },
	{ num: 4, name: "Toxic Dump" },
	{ num: 5, name: "The Abyss" },
	{ num: 6, name: "Launch Facility" }
];

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Read syncduke.ini's [net] (with the per-host [net:<hostname>] overlay) and fill
// defaults. Returns { net: {...} }. The [grp]/[video]/[game] sections are the C
// door's concern (it reads the ini itself); the lobby only needs [net].
function sd_load_config() {
	var cfg = { net: {}, lobby: {}, dukematch: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.net = gl.read_overlaid(f, "net");
		cfg.lobby = f.iniGetObject("lobby") || {};   // [lobby] live = true -> live panel
		cfg.dukematch = f.iniGetObject("dukematch") || {};   // deathmatch sub-mode + options
		f.close();
	}
	gl.apply_defaults(cfg.net, {
		// UDP port range the master listens on (one per concurrent match on a host).
		port_low: 25000,
		port_high: 25015,
		// How long a waiting (unjoined) match stays visible. The master holds its slot
		// up to the door's handshake timeout (~60s), so 90s covers it plus mount lag.
		stale: 90,
		// Cross-host (same-LAN) play: the address a joiner on another host dials. Blank
		// = same-host only (joiners use 127.0.0.1). The master always binds 0.0.0.0, so
		// no separate bind setting is needed -- advertise is the address it's reached at.
		advertise: "",
		// Default skill (1=Piece of Cake .. 4=Damn I'm Good) co-op starts on. The door's
		// warp path uses Duke's own default unless we add a /s flag later.
		skill: 2
	});
	// Dukematch sub-mode + arena options (lobby-side; the door reads none of these --
	// they become engine /c//m//t flags in sd_cmd). Classic DM defaults: spawn, no monsters.
	gl.apply_defaults(cfg.dukematch, {
		submode: "spawn",         // "spawn" (/c1, weapons respawn) or "nospawn" (/c3)
		monsters: false,          // classic dukematch is player-vs-player only
		respawn_items: false      // respawn items/inventory over time
	});
	return cfg;
}

// The level descriptor for a 1-based number, or the first level (E1L1) as default.
function sd_find_level(num) {
	var i;
	for (i = 0; i < SD_LEVELS.length; i++)
		if (SD_LEVELS[i].num == parseInt(num, 10))
			return SD_LEVELS[i];
	return SD_LEVELS[0];
}

// Per-user writable home dir for the door (config + saves land here, not in the
// shared GRP dir). Synchronet auto-cleans data/user/####/ when a user is removed,
// so per-user door data belongs there. Mirrors SyncDOOM's -home / the direct
// launcher's "%juser/%4/duke/".
function sd_home() {
	return system.data_dir + "user/" + format("%04u", user.number) + "/duke/";
}

// ---------------------------------------------------------------------------
// Door command line
// ---------------------------------------------------------------------------

// Build the door command for a play session (pre-cmdstr; bbs.cmdstr expands the
// %-specifiers before exec):
//   role  = "master" | "join" | null (single-player)
//   port  = the master's listen port (role "master")
//   peer  = "host:port" the joiner dials (role "join")
//   level = 1-based co-op level (master & join must match; ignored single-player)
// The socket/time/alias come via -s%H/-t%T/-name%a (no DOOR32.SYS drop file, like
// SyncDOOM); the door auto-probes the terminal size. Co-op auto-starts the level
// with the engine's DOS-style slash warp args /v1 /l<level> /c2 (/c2 = cooperative;
// the engine's parser only honors '/' options, not '-'). Single-player launches
// bare so Duke's own menu drives episode/skill selection.
// role  = "master" | "join" | null (single-player)
// mode  = "coop" | "dm" | "dmnospawn" (net games; default "coop"); the /c flag.
// dmopts = { monsters, respawn_items } (e.g. cfg.dukematch); consulted for DM only.
// The engine parses only '/' options. On the warp-args launch there is no in-game
// start-packet exchange (that only fires from Duke's own new-game menu), so BOTH
// peers must pass matching flags on the command line: the /c mode comes from the
// registry entry (identical for both), and /m//t come from [dukematch] in
// syncduke.ini -- a file shared by every host of a multi-host BBS (like [net]), so
// the arena options are identical across hosts without any in-game reconciliation.
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

// ---------------------------------------------------------------------------
// Game registry (this lobby is the writer for the peer-master model)
// ---------------------------------------------------------------------------

// A registry filename stem unique across same-LAN hosts sharing the games dir:
// "<host>-<port>" with the host sanitized to a safe token.
function sd_entry_name(port) {
	var host = String(system.local_host_name || "host").replace(/[^A-Za-z0-9]+/g, "");
	return host + "-" + port;
}

// Write the "waiting for a player" registry entry for a match the creator is about
// to host as master. `addr` is the advertised cross-host address (blank for
// same-host). Returns the entry's file path (remove it when the match ends).
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
		created:   time(),
		heartbeat: time()                          // refreshed each re-write; keeps the
		                                           // entry visible past [net] stale during
		                                           // the master's indefinite wait
	});
}

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

// The host:port a joiner dials for a registry entry. The master binds 0.0.0.0, so
// an advertised address (cross-host, same LAN) is reachable, and 127.0.0.1 reaches
// a same-host master.
function sd_join_peer(entry) {
	var host = (entry.addr && gl.trim(entry.addr).length) ? gl.trim(entry.addr) : "127.0.0.1";
	return host + ":" + entry.port;
}

// Joinable matches: registry entries still "waiting" (not yet claimed/full). v1 is
// strictly 2-player, so a claimed game's entry is removed by the joiner and simply
// disappears -- anything still listed is open.
function sd_list_open_games(cfg) {
	var all = gl.list_games(SD_GAMES, cfg.net.stale), out = [], i;
	for (i = 0; i < all.length; i++)
		if (String(all[i].status) == "waiting")
			out.push(all[i]);
	return out;
}

// ---------------------------------------------------------------------------
// Activity feed formatting (door-written events.jsonl -> one display line)
// ---------------------------------------------------------------------------

// One event's plain-body description for the activity feed / panel; the display path
// (gl.event_feed and gl.activity_cell) adds the "[age]" prefix + coloring, matching
// SyncDOOM. Returns null for event types the feed doesn't surface, so they're skipped.
// SyncDuke's "level" event carries the level just ENTERED (its secs is the prior
// level's elapsed time), so it reads as "reached", not "cleared in Ns".
function sd_event_text(e) {
	switch (e.type) {
		case "frag":  return e.killer + " fragged " + e.victim;
		case "start": return e.user + " joined (" + (e.mode || "single") + ")";
		case "level": return e.user + " reached " + e.map;
		case "death": return e.user + " died on " + e.map;
	}
	return null;
}

// ---------------------------------------------------------------------------
// Live who's-online + recent-activity panel (bottom of the lobby)
// ---------------------------------------------------------------------------

// Up to `max` recent displayable events (most recent first) for the panel. SyncDuke's
// sd_event_text renders every event type, so nothing is filtered today -- but this
// routes through the shared filter so a future hidden type just works.
function sd_recent_events(max, max_age) {
	return gl.recent_events(SD_EVENTS, max, sd_event_text, max_age);
}

// Bottom-of-lobby panel cells via the shared composer: live nodes (SyncDuke players
// first) then recent activity (age-limited by `max_age` seconds, 0 = no limit). The
// lobby.js draw loop bottom-anchors these.
function sd_panel_cells(cols, max_age, max_rows) {
	max_rows = (max_rows > 0) ? max_rows : 6;
	return gl.panel_cells(cols, "SyncDuke", sd_recent_events(max_rows, max_age), sd_event_text, max_rows);
}

this;
