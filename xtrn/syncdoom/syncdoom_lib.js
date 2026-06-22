// syncdoom_lib.js -- model layer for the syncdoom lobby (no UI, no bbs/user
// dependencies, so it can be exercised headless with jsexec). Loaded by
// lobby.js. SpiderMonkey 1.8.5-compatible (no modern ES).
//
// Copyright(C) 2026 Rob Swindell / syncdoom. GPL-2.0.

load("sbbsdefs.js");
load("sockdefs.js");

// The door's own directory (where the syncdoom binary + syncdoom.ini live).
// js.exec_dir is the directory of the *running* script; when lobby.js loads
// this lib, that is xtrn/syncdoom/.
var SD_DIR    = js.exec_dir;
// "%." is a Synchronet cmdstr specifier: ".exe" on Windows, blank on *nix
// (expanded by bbs.cmdstr() at launch). So one config runs syncdoom.exe on
// Windows yet won't collide with a non-Windows "syncdoom" in the same dir.
var SD_BINARY = SD_DIR + "syncdoom%.";
var SD_CFG    = SD_DIR + "syncdoom.ini";
var SD_GAMES  = system.data_dir + "syncdoom/games/";

// Trim leading & trailing whitespace (SM1.8.5-safe).
function sd_trim(s)
{
	return String(s).replace(/^\s+/, "").replace(/\s+$/, "");
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// Read syncdoom.ini -> { net:{}, wads:{}, wadsets:[{id,name,iwad,...}] } with
// defaults filled in.
function sd_load_config()
{
	var cfg = { net: {}, wads: {}, wadsets: [], lobby: {} };
	var f = new File(SD_CFG);
	if (f.open("r")) {
		cfg.lobby   = f.iniGetObject("lobby") || {};
		cfg.net     = f.iniGetObject("net")  || {};
		// Host-specific overrides: [net:<local_host_name>] overlays [net], so a
		// single shared install (one syncdoom.ini seen by several physical hosts)
		// can give each host its own advertise address / port range without
		// separate config files. Keys present here win over the base [net].
		var hostnet = f.iniGetObject("net:" + system.local_host_name);
		if (hostnet) {
			for (var hk in hostnet)
				cfg.net[hk] = hostnet[hk];
		}
		cfg.wads    = f.iniGetObject("wads") || {};
		cfg.wadsets = f.iniGetAllObjects("id", "wadset:");
		f.close();
	}

	if (cfg.net.port_low    === undefined) cfg.net.port_low    = 20000;
	if (cfg.net.port_high   === undefined) cfg.net.port_high   = 20063;
	if (cfg.net.max_games   === undefined) cfg.net.max_games   = 8;
	if (cfg.net.max_players === undefined) cfg.net.max_players = 4;
	if (cfg.net.idle_timeout === undefined) cfg.net.idle_timeout = 60;
	if (cfg.net.stale       === undefined) cfg.net.stale       = 30;
	// Default skill (1-5: ITYTD/HNTR/HMP/UV/NM) the Create flow's prompt starts on;
	// a [wadset:*] skill overrides it per set. 3 = Hurt Me Plenty (Doom's default).
	if (cfg.net.skill       === undefined) cfg.net.skill       = 3;

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
	var d = cfg.wads.dir;
	if (!d)
		return SD_DIR;
	// Absolute (unix /... or Windows X:\...) used as-is; else under the door dir.
	if (d.charAt(0) == "/" || d.charAt(0) == "\\" || d.charAt(1) == ":")
		return backslash(d);
	return backslash(SD_DIR + d);
}

// Absolute lobby attract-art directory, from [lobby] art_dir (blank = <door>/art).
// A sysop drops full-screen DOOM ANSI (*.ans/*.asc) here; nothing ships in it, so
// the attract is silent until they do. (Don't redistribute copyrighted art.)
function sd_attract_dir(cfg)
{
	var d = (cfg.lobby && cfg.lobby.art_dir) ? String(cfg.lobby.art_dir) : "art";
	if (d.charAt(0) == "/" || d.charAt(0) == "\\" || d.charAt(1) == ":")
		return backslash(d);
	return backslash(SD_DIR + d);
}

// The art files a sysop has dropped in the attract dir, or []. Filters by
// extension case-insensitively (classic art is often upper-case *.ANS, and
// directory() is case-sensitive on *nix).
function sd_attract_files(cfg)
{
	var all = directory(sd_attract_dir(cfg) + "*");
	var art = [];
	for (var i = 0; i < all.length; i++)
		if (/\.(ans|asc|ansi)$/i.test(all[i]))
			art.push(all[i]);
	return art;
}

// Does a wadset offer the given mode ("solo"/"coop"/"deathmatch"/"altdeath")?
function sd_wadset_has_mode(ws, mode)
{
	var list = String(ws.modes).toLowerCase().split(",");
	var i;
	for (i = 0; i < list.length; i++)
		if (sd_trim(list[i]) == mode)
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

	if (ws.pwad) {
		var list = String(ws.pwad).split(",");
		args.push("-file");
		for (i = 0; i < list.length; i++) {
			var name = sd_trim(list[i]);
			if (name.length)
				args.push(sd_wad_path(dir, name));
		}
	}
	if (ws.merge) {
		var ml = String(ws.merge).split(",");
		for (i = 0; i < ml.length; i++) {
			var mn = sd_trim(ml[i]);
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
			var dn = sd_trim(dl[i]);
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
			var name = sd_trim(list[i]);
			if (name.length && !file_exists(sd_wad_path(dir, name)))
				return false;
		}
	}
	if (ws.deh) {
		var dl = String(ws.deh).split(",");
		var j;
		for (j = 0; j < dl.length; j++) {
			var dn = sd_trim(dl[j]);
			if (dn.length && !file_exists(sd_wad_path(dir, dn)))
				return false;
		}
	}
	return true;
}

// Find a bindable UDP port in the [net] range, or -1. (A small TOCTOU window
// exists between this test-bind and the server's bind; acceptable at lobby
// contention levels.)
function sd_alloc_port(cfg)
{
	var lo = parseInt(cfg.net.port_low, 10);
	var hi = parseInt(cfg.net.port_high, 10);
	var p;
	for (p = lo; p <= hi; p++) {
		var s = new Socket(SOCK_DGRAM);
		var ok = s.bind(p);
		s.close();
		if (ok)
			return p;
	}
	return -1;
}

// ---------------------------------------------------------------------------
// Game registry (discovery) -- read-only here; the dedicated server is the
// single writer of its own entry.
// ---------------------------------------------------------------------------

// Live games: parse data/syncdoom/games/*.ini, dropping entries whose
// heartbeat is older than [net] stale seconds.
function sd_list_games(cfg)
{
	var out = [];
	if (!file_isdir(SD_GAMES))
		return out;

	var files = directory(SD_GAMES + "*.ini");
	var now = time();
	var stale = parseInt(cfg.net.stale, 10) || 30;
	var i;
	for (i = 0; i < files.length; i++) {
		var f = new File(files[i]);
		if (!f.open("r"))
			continue;
		var g = f.iniGetObject();
		f.close();
		if (!g || g.port === undefined)
			continue;
		g.file = files[i];
		// A clean shutdown removes its own .ini; an unclean death (kill/crash)
		// leaves it frozen at its last heartbeat. Reap (delete) it once it's well
		// past stale -- x3 leaves margin for SMB attribute-cache lag on a shared
		// cross-host games dir, and a still-live server re-creates its file on the
		// next ~3s heartbeat anyway. In the merely-stale window just hide it (it
		// may be that cache lag, and the heartbeat could recover).
		var age = (g.heartbeat !== undefined)
		    ? (now - parseInt(g.heartbeat, 10)) : (stale * 3 + 1);
		if (age > stale * 3) {
			file_remove(files[i]);
			continue;        // dead server -- reap its orphaned registry entry
		}
		if (age > stale)
			continue;        // stale (maybe SMB lag); server presumed dead -- hide
		// An empty match (everyone left, or it hasn't been joined yet) is not
		// meaningfully joinable -- the dedicated server is already counting down
		// its idle-timeout to self-quit. Hide it so it doesn't linger in the
		// Join menu for that window. (players is written by the server each
		// heartbeat as NET_SV_ConnectedClients().)
		if (parseInt(g.players, 10) < 1)
			continue;
		out.push(g);
	}
	return out;
}
