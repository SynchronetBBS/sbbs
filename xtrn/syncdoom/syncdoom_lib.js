// syncdoom_lib.js -- model layer for the syncdoom lobby (no UI, no bbs/user
// dependencies, so it can be exercised headless with jsexec). Loaded by lobby.js.
//
// Layers the DOOM-specific bits (wad sets, map-warp args, event text, the
// who's-online + recent-activity panel) over the shared exec/load/game_lobby.js,
// so the logic every network-game lobby needs -- [net] config with per-host
// overlay + defaults, UDP port allocation, the game registry, node paging, the
// live-node panel primitives, and events.jsonl read/prune -- is shared with
// SyncDuke rather than duplicated here. SpiderMonkey 1.8.5-compatible (no modern
// ES).
//
// Copyright(C) 2026 Rob Swindell / syncdoom. GPL-2.0.

// Loaded default-form by lobby.js, so its constants land in the lobby's scope:
// EX_NATIVE/EX_BIN (bbs.exec), K_*/P_*/NODE_* (menu + node helpers). (LOG_WARNING,
// used in sd_load_config below, is a built-in -- present here either way.)
load("sbbsdefs.js");

var gl = load({}, "game_lobby.js");   // shared lobby model layer (gl.* helpers)

// The door's own directory (where the syncdoom binary + syncdoom.ini live).
// js.exec_dir is the directory of the *running* script; when lobby.js loads
// this lib, that is xtrn/syncdoom/.
var SD_DIR    = js.exec_dir;
// "%." is a Synchronet cmdstr specifier: ".exe" on Windows, blank on *nix
// (expanded by bbs.cmdstr() at launch). So one config runs syncdoom.exe on
// Windows yet won't collide with a non-Windows "syncdoom" in the same dir.
var SD_BINARY = SD_DIR + "syncdoom%.";
var SD_CFG    = SD_DIR + "syncdoom.ini";
var SD_GAMES  = backslash(system.data_dir + "syncdoom/games/");
var SD_EVENTS = system.data_dir + "syncdoom/events.jsonl";   // door-written activity log

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Read syncdoom.ini -> { net:{}, wads:{}, wadsets:[{id,name,iwad,...}], lobby:{} }
// with defaults filled in. The [net] section (with its per-host [net:<hostname>]
// overlay) and net defaults come from the shared gl helpers; the wadset catalog is
// DOOM-specific.
function sd_load_config()
{
	var cfg = { net: {}, wads: {}, wadsets: [], lobby: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.lobby   = f.iniGetObject("lobby") || {};
		cfg.net     = gl.read_overlaid(f, "net");   // [net] + per-host [net:<hostname>]
		cfg.wads    = f.iniGetObject("wads") || {};
		cfg.wadsets = f.iniGetAllObjects("id", "wadset:");
		f.close();
	}

	gl.apply_defaults(cfg.net, {
		port_low: 20000,
		port_high: 20063,
		max_games: 8,
		max_players: 4,
		idle_timeout: 60,
		stale: 30,
		// Default skill (1-5: ITYTD/HNTR/HMP/UV/NM) the Create flow's prompt starts on;
		// a [wadset:*] skill overrides it per set. 3 = Hurt Me Plenty (Doom's default).
		skill: 3
	});

	var i;
	for (i = 0; i < cfg.wadsets.length; i++) {
		var ws = cfg.wadsets[i];
		if (ws.name === undefined)    ws.name = ws.id;
		if (ws.enabled === undefined) ws.enabled = true;
		if (ws.modes === undefined)   ws.modes = "solo, coop, deathmatch, altdeath";

		// Validate WAD files once, here. A set whose files aren't installed is
		// hidden from the player's menu (sd_list_wadsets), but we log it so the
		// sysop knows a configured set is unusable.
		ws.present = sd_wadset_files_present(cfg, ws);
		if (!ws.present && ws.enabled !== false && ws.enabled !== "false") {
			log(LOG_WARNING, "syncdoom: WAD set '" + ws.id + "' (" + ws.name
			    + ") hidden: missing file(s) in " + sd_wad_dir(cfg));
		}
	}
	return cfg;
}

// Absolute WAD directory, from [wads] dir (blank = the door dir).
function sd_wad_dir(cfg)
{
	return gl.resolve_dir(SD_DIR, cfg.wads.dir, "");
}

// The lobby attract-art files a sysop dropped in [lobby] art_dir (blank = <door>/art),
// or []. A sysop drops full-screen DOOM ANSI (*.ans/*.asc) there; nothing ships in it,
// so the attract is silent until they do. (Don't redistribute copyrighted art.)
function sd_attract_files(cfg)
{
	return gl.attract_files(SD_DIR, cfg.lobby && cfg.lobby.art_dir);
}

// Does a wadset offer the given mode ("solo"/"coop"/"deathmatch"/"altdeath")?
function sd_wadset_has_mode(ws, mode)
{
	var list = String(ws.modes).toLowerCase().split(",");
	var i;
	for (i = 0; i < list.length; i++)
		if (gl.trim(list[i]) == mode)
			return true;
	return false;
}

// Enabled wadsets that offer 'mode' (or all enabled if mode is undefined).
function sd_list_wadsets(cfg, mode)
{
	var out = [];
	var i;
	for (i = 0; i < cfg.wadsets.length; i++) {
		var ws = cfg.wadsets[i];
		if (ws.enabled === false || ws.enabled === "false")
			continue;
		if (ws.present === false)        // files not installed -- hidden
			continue;
		if (mode && !sd_wadset_has_mode(ws, mode))
			continue;
		out.push(ws);
	}
	return out;
}

function sd_find_wadset(cfg, id)
{
	var i;
	for (i = 0; i < cfg.wadsets.length; i++)
		if (cfg.wadsets[i].id == id)
			return cfg.wadsets[i];
	return null;
}

// Convert a map label ("MAP07", "E5M1") to -warp args (["-warp","7"] etc.).
function sd_warp_args(map)
{
	if (!map)
		return [];
	map = String(map).toUpperCase();
	var m = map.match(/^MAP(\d+)$/);
	if (m)
		return ["-warp", String(parseInt(m[1], 10))];
	m = map.match(/^E(\d+)M(\d+)$/);
	if (m)
		return ["-warp", String(parseInt(m[1], 10)), String(parseInt(m[2], 10))];
	return [];
}

// Build the doomgeneric WAD args (absolute paths) for a wadset:
//   -iwad <dir>/<iwad> [-file <dir>/<p1> <dir>/<p2> ...] [-warp ...]
function sd_wadset_args(cfg, ws)
{
	var dir = sd_wad_dir(cfg);
	var args = ["-iwad", sd_wad_path(dir, ws.iwad)];
	var i;

	// Friendly WAD-set name for the door's who's-online status ("playing SyncDOOM:
	// <name> (E1M1)"). Quoted because it usually contains spaces; the door falls
	// back to the -iwad basename when -wadname is absent.
	if (ws.name)
		args.push("-wadname", '"' + String(ws.name).replace(/"/g, "") + '"');

	if (ws.pwad) {
		var list = String(ws.pwad).split(",");
		args.push("-file");
		for (i = 0; i < list.length; i++) {
			var name = gl.trim(list[i]);
			if (name.length)
				args.push(sd_wad_path(dir, name));
		}
	}
	if (ws.merge) {
		var ml = String(ws.merge).split(",");
		for (i = 0; i < ml.length; i++) {
			var mn = gl.trim(ml[i]);
			if (mn.length) {
				args.push("-merge");
				args.push(sd_wad_path(dir, mn));
			}
		}
	}
	if (ws.deh) {
		var dl = String(ws.deh).split(",");
		args.push("-deh");
		for (i = 0; i < dl.length; i++) {
			var dn = gl.trim(dl[i]);
			if (dn.length)
				args.push(sd_wad_path(dir, dn));
		}
	}
	var warp = sd_warp_args(ws.map);
	for (i = 0; i < warp.length; i++)
		args.push(warp[i]);

	return args;
}

// Case-insensitive real on-disk path of a WAD <name> in <dir>. Synchronet's
// file_getcase() returns the actual path, correcting case on a case-sensitive
// (Linux) filesystem -- DOS/Windows WADs are commonly UPPER-case (DOOM2.WAD), so a
// configured "doom2.wad" still resolves. Falls back to the plain join when there's
// no such file (the caller then treats it as absent).
function sd_wad_path(dir, name)
{
	return file_getcase(dir + name) || (dir + name);
}

// Are all of a wadset's WAD files actually present in the WAD dir? (Join must
// verify the local copy matches before connecting.)
function sd_wadset_files_present(cfg, ws)
{
	var dir = sd_wad_dir(cfg);
	if (!file_exists(sd_wad_path(dir, ws.iwad)))
		return false;
	if (ws.pwad) {
		var list = String(ws.pwad).split(",");
		var i;
		for (i = 0; i < list.length; i++) {
			var name = gl.trim(list[i]);
			if (name.length && !file_exists(sd_wad_path(dir, name)))
				return false;
		}
	}
	if (ws.deh) {
		var dl = String(ws.deh).split(",");
		var j;
		for (j = 0; j < dl.length; j++) {
			var dn = gl.trim(dl[j]);
			if (dn.length && !file_exists(sd_wad_path(dir, dn)))
				return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Game registry (discovery) -- read-only here; the dedicated server is the
// single writer of its own entry.
// ---------------------------------------------------------------------------

// Live, joinable games. The shared gl.list_games() reads data/syncdoom/games/*.ini
// and applies staleness/reaping (a missing heartbeat falls back to the file mtime).
// On top of that we apply DOOM's joinability rules: an entry must carry a port, and
// an empty match (players < 1 -- everyone left, or it hasn't been joined yet; the
// dedicated server is already counting down its idle-timeout to self-quit) is hidden
// so it doesn't linger in the Join menu. (players is written by the server each
// heartbeat as NET_SV_ConnectedClients().)
function sd_list_games(cfg)
{
	var all = gl.list_games(SD_GAMES, cfg.net.stale), out = [], i;
	for (i = 0; i < all.length; i++) {
		var g = all[i];
		if (g.port === undefined)
			continue;
		if (parseInt(g.players, 10) < 1)
			continue;
		out.push(g);
	}
	return out;
}

// ---------------------------------------------------------------------------
// Activity feed (door-written events.jsonl)
// ---------------------------------------------------------------------------

// One-line description of an event for the lobby, or null to hide it (the debug
// 'start'/'end' records and player 'death's -- the matching 'frag' already shows).
function sd_event_text(e)
{
	switch (e.type) {
		case "frag":
			return e.killer + " fragged " + e.victim;
		case "level":
			return e.user + " cleared " + e.map + " in " + gl.mmss(e.secs);
		case "death":
			if (e.cause == "player")  return null;
			if (e.cause == "suicide") return e.user + " blew themselves up";
			if (e.cause == "monster") return e.user + " was killed";
			return e.user + " died";          // environment
	}
	return null;
}

// Up to `max` recent *displayable* event objects (most recent first) -- i.e. the
// ones sd_event_text() renders (frags / level clears / non-player deaths). Reads an
// oversampled window from the shared gl.read_events() so the non-displayable events
// interleaved in the tail don't shrink the result below `max`.
function sd_recent_events(max, max_age)
{
	return gl.recent_events(SD_EVENTS, max, sd_event_text, max_age);
}

// ---------------------------------------------------------------------------
// Live lobby panel (who's online + recent activity)
// ---------------------------------------------------------------------------

// Build the bottom-of-lobby panel's cells via the shared composer: live nodes
// (SyncDOOM players first) then recent activity, padded to a minimum height.
// The lobby.js draw loop bottom-anchors these. Returns { cells, cw }.
function sd_panel_cells(cols, max_age, max_rows)
{
	max_rows = (max_rows > 0) ? max_rows : 6;
	return gl.panel_cells(cols, "SyncDOOM", sd_recent_events(max_rows, max_age), sd_event_text, max_rows);
}
