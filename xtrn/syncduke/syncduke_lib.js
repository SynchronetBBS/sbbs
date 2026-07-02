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
	var cfg = { net: {}, lobby: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.net = gl.read_overlaid(f, "net");
		cfg.lobby = f.iniGetObject("lobby") || {};   // [lobby] live = true -> live panel
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
function sd_write_entry(cfg, port, level) {
	var lev = sd_find_level(level);
	return gl.write_game(SD_GAMES, sd_entry_name(port), {
		host:      (typeof user != "undefined" && user.alias) ? user.alias : "Someone",
		addr:      gl.advertise_addr(cfg.net),     // "" => same-host (joiner uses 127.0.0.1)
		port:      port,
		level:     lev.num,
		levelname: lev.name,
		status:    "waiting",
		players:   1,
		maxplayers: 2,
		created:   time()
	});
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
