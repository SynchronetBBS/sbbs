// game_lobby.js -- shared model layer for Synchronet "network game door" lobbies.
//
// A reusable, game-agnostic library for the JS frontends that browse/create/join
// multi-player sessions of a native door (SyncDuke, SyncDOOM, ...). It owns the
// parts every such lobby needs and no game cares about differently:
//
//   * [net] config with per-host [net:<hostname>] overlay + defaults
//   * UDP port allocation within a configured range
//   * bind / advertise / creator-connect address resolution (same-host vs LAN)
//   * a file-based game registry under data/<game>/games/ (read, write, reap)
//   * paging the door's other active nodes (honoring its access requirements)
//   * the live who's-online panel + attract-art helpers
//   * small string/format utilities
//
// It is UI-free (no console/bbs prompting) EXCEPT the multiplayer_flow/mp_* helpers
// (the shared [M]ultiplayer lobby flow), which prompt via console/bbs and are
// exercised only by the doors' live tests; everything else runs headless under
// jsexec. The per-game lobby.js layers the menu and the door command line on top.
//
// Load it into its own scope:  var gl = load({}, "game_lobby.js");
// then call gl.alloc_port(net), gl.list_games(dir, stale), etc. The session-bound
// helpers (page_targets, send_pages, live_nodes) need a Terminal Server context;
// everything else (config, ports, registry, addresses) runs under jsexec too.
//
// Two networking shapes are supported, because the doors differ:
//   * SyncDOOM-style: a detached dedicated server (C) writes & heartbeats its own
//     registry entry; the lobby only reads it (list_games).
//   * SyncDuke-style: a peer "master" player owns the match and the LOBBY writes
//     the registry entry (write_game) before launch and removes it (remove_game)
//     when the door exits; the joiner claims (removes) it on join. Such entries
//     carry no heartbeat, so freshness falls back to the file's mtime.
//
// SpiderMonkey 1.8.5-compatible (no modern ES). Copyright(C) 2026 Rob Swindell.
// GPL-2.0.

load("sbbsdefs.js");    // NODE_INUSE, NODE_QUIET, NODE_NMSG, ...
load("sockdefs.js");    // SOCK_DGRAM
var _json_lines = load({}, "json_lines.js");

// ---------------------------------------------------------------------------
// String / format helpers (operate on plain text unless noted)
// ---------------------------------------------------------------------------

// Trim leading & trailing whitespace (SM1.8.5-safe; no String.trim reliance).
function trim(s) { return String(s).replace(/^\s+/, "").replace(/\s+$/, ""); }

// Strip Ctrl-A attribute codes (\1x) so a string's .length == its visible width.
function plain(s) { return String(s).replace(/\x01./g, ""); }

function clip(s, w) { s = String(s); return s.length > w ? s.substr(0, w) : s; }
function rpad(s, w) { s = String(s); while (s.length < w) s += " "; return s; }
function lpad(s, w) { s = String(s); while (s.length < w) s = " " + s; return s; }

// Seconds -> "M:SS".
function mmss(secs) {
	var m = Math.floor(secs / 60), s = secs % 60;
	return m + ":" + (s < 10 ? "0" : "") + s;
}

// Short relative-age tag for a unix time ("[now]" / "[5m]" / "[2h]" / "[3d]").
function ago(t) {
	var d = time() - t;
	if (d < 0)     d = 0;
	if (d < 60)    return "[now]";
	if (d < 3600)  return "[" + Math.floor(d / 60) + "m]";
	if (d < 86400) return "[" + Math.floor(d / 3600) + "h]";
	return "[" + Math.floor(d / 86400) + "d]";
}

// ---------------------------------------------------------------------------
// Configuration: per-host [section:<hostname>] overlay
// ---------------------------------------------------------------------------

// Read an ini section overlaid with its per-host variant: [section] is the base,
// [section:<local_host_name>] overrides it key-by-key. Lets one shared install
// (a single ini seen by several physical hosts over a mount) give each host its
// own advertise address / port range with no separate file. `f` is an OPEN File.
// Returns a plain object (possibly empty).
function read_overlaid(f, section) {
	var base = f.iniGetObject(section) || {};
	var host = f.iniGetObject(section + ":" + system.local_host_name);
	if (host) {
		for (var k in host)
			base[k] = host[k];
	}
	return base;
}

// Apply default key-values to a config object (only where the key is absent), so
// the per-game loader can declare its [net] defaults in one place. Mutates and
// returns `obj`.
function apply_defaults(obj, defaults) {
	for (var k in defaults)
		if (obj[k] === undefined)
			obj[k] = defaults[k];
	return obj;
}

// ---------------------------------------------------------------------------
// Network address resolution
//
// Two independent [net] addresses:
//   bind      = the local interface the server/master socket accepts on.
//   advertise = the address peers on OTHER hosts dial (recorded in the registry).
// Both blank -> the door's own loopback default (same-host play only). For
// cross-host play set advertise (LAN IP / public name) and, when advertise is a
// name this host can't bind directly, also set bind (0.0.0.0 or the LAN IP).
// ---------------------------------------------------------------------------

function advertise_addr(net) {
	var a = (net && net.advertise !== undefined && net.advertise !== null) ? net.advertise : "";
	return trim(String(a));
}

function bind_addr(net) {
	var b = (net && net.bind !== undefined && net.bind !== null) ? net.bind : "";
	return trim(String(b));
}

// Address the match creator dials to reach the server/master it just spawned on
// THIS host. A socket bound to a specific interface IP does not receive loopback
// datagrams, so the creator can't blindly use 127.0.0.1 -- it must dial the very
// address the socket bound (a host can always reach its own local IP). A wildcard
// bind (0.0.0.0/any/*) or an unset bind both accept on loopback, so dial 127.0.0.1
// there (you can't send to a wildcard; loopback is guaranteed).
function creator_connect_host(net) {
	var b = bind_addr(net) || advertise_addr(net);
	var lc = String(b).toLowerCase();
	if (!b || lc == "0.0.0.0" || lc == "any" || lc == "*")
		return "127.0.0.1";
	return b;
}

// ---------------------------------------------------------------------------
// UDP port allocation
// ---------------------------------------------------------------------------

// Find a bindable UDP port in [net].port_low..port_high, or -1 if none are free.
// (A small TOCTOU window exists between this test-bind and the server's bind;
// acceptable at lobby contention levels.)
function alloc_port(net) {
	var lo = parseInt(net.port_low, 10);
	var hi = parseInt(net.port_high, 10);
	if (isNaN(lo) || isNaN(hi))
		return -1;
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
// Game registry (discovery)  --  data/<game>/games/*.ini
//
// Each running match is one .ini whose ROOT (unnamed) section holds its metadata
// (host, addr, port, status, plus whatever the game adds). Freshness is judged by
// a `heartbeat` field when present (a C dedicated server writes one every few
// seconds); otherwise by the file's mtime (a lobby-written peer entry is static
// for its lifetime). Stale entries are hidden; long-dead ones are reaped.
// ---------------------------------------------------------------------------

// Ensure the registry directory exists; returns it (with a trailing slash).
function games_dir_ensure(dir) {
	if (!file_isdir(dir))
		mkpath(dir);
	return dir;
}

// Freshness age (seconds) of a registry entry: from its `heartbeat` field if the
// writer maintains one, else from the file's mtime.
function entry_age(g, path) {
	var now = time();
	if (g.heartbeat !== undefined)
		return now - parseInt(g.heartbeat, 10);
	var mt = file_date(path);
	return (mt > 0) ? (now - mt) : 0;
}

// Live games: parse data/<game>/games/*.ini, dropping entries staler than
// `stale_secs`. An entry past stale*3 is REAPED (its orphaned file removed -- a
// crashed/killed writer never cleans up after itself); x3 leaves margin for SMB
// attribute-cache lag on a shared cross-host games dir. Each returned object is
// the entry's root section plus `.file` (its path) and `.age` (seconds). The
// caller applies game-specific joinability rules (status, player count). Returns
// [] when the dir is absent.
function list_games(dir, stale_secs) {
	var out = [];
	if (!file_isdir(dir))
		return out;
	var stale = parseInt(stale_secs, 10) || 30;
	var files = directory(dir + "*.ini");
	var i;
	for (i = 0; i < files.length; i++) {
		var f = new File(files[i]);
		if (!f.open("r"))
			continue;
		var g = f.iniGetObject();      // root (unnamed) section
		f.close();
		if (!g)
			continue;
		var age = entry_age(g, files[i]);
		if (age > stale * 3) {
			file_remove(files[i]);     // dead writer -- reap its orphaned entry
			continue;
		}
		if (age > stale)
			continue;                  // stale (maybe SMB lag); writer presumed gone -- hide
		g.file = files[i];
		g.age = age;
		out.push(g);
	}
	return out;
}

// Write/replace a registry entry (the lobby-written, peer-master path). `obj`'s
// keys become the file's root section. Returns the entry's path, or null on
// failure. Use a unique, stable `name` (e.g. the port, or host+port) so a host
// can run several matches without collision.
function write_game(dir, name, obj) {
	games_dir_ensure(dir);
	var path = dir + name + ".ini";
	var f = new File(path);
	// "w+" (truncate + read/write): iniSetObject re-reads the file to merge, so the
	// fp must be readable -- a plain "w" makes iniSetObject's iniReadFile fail.
	if (!f.open("w+"))
		return null;
	var ok = f.iniSetObject(null, obj);   // null section == root
	f.close();
	return ok ? path : null;
}

// Remove a registry entry by path (idempotent: a missing file is fine). Used when
// the match ends (master door exits) or is claimed (joiner takes the last slot).
function remove_game(path) {
	if (path && file_exists(path))
		return file_remove(path);
	return true;
}

// --- N-player muster coordination (used by a lobby-driven waiting room) ---------
// A muster is a lobby-owned game entry (<stem>.ini) plus two marker kinds beside it:
//   <stem>.wait.<node>  -- one per joining node (joiner-owned); the host counts/lists them
//   <stem>.go           -- the host's launch signal, carrying the server address
// Markers use non-.ini names so list_games() never parses them. One writer per file.

// Path of a joining node's waiter marker for a game entry.
function waiter_path(entryPath, node) {
	return String(entryPath).replace(/\.ini$/i, ".wait." + node);
}

// Joiner: register in the muster (exclusive create -- one marker per node; false if
// this node already registered or on error). `alias` is stored for the host's list.
function write_waiter(entryPath, node, alias) {
	var f = new File(waiter_path(entryPath, node));
	if (!f.open("wx"))
		return false;
	f.write((alias || "") + "\n");
	f.close();
	return true;
}

// Host: the registered waiters as [{node, alias}] (one per <stem>.wait.* marker).
function list_waiters(entryPath) {
	var base = String(entryPath).replace(/\.ini$/i, ".wait.");
	var files = directory(base + "*"), out = [], i;
	for (i = 0; i < files.length; i++) {
		var node = files[i].substr(base.length);
		var alias = "";
		var f = new File(files[i]);
		if (f.open("r")) { alias = f.readln() || ""; f.close(); }
		out.push({ node: node, alias: alias });
	}
	return out;
}

// Joiner: drop our marker (leaving the muster). Idempotent.
function remove_waiter(entryPath, node) {
	var p = waiter_path(entryPath, node);
	if (file_exists(p))
		return file_remove(p);
	return true;
}

// Path of a game's go marker.
function go_path(entryPath) {
	return String(entryPath).replace(/\.ini$/i, ".go");
}

// Host: signal launch, carrying the server address joiners dial.
function write_go(entryPath, addr) {
	var f = new File(go_path(entryPath));
	if (!f.open("w+"))
		return false;
	f.write((addr || "") + "\n");
	f.close();
	return true;
}

// Joiner: the go address if the host has fired, else null.
function read_go(entryPath) {
	var p = go_path(entryPath);
	if (!file_exists(p))
		return null;
	var f = new File(p), addr = "";
	if (f.open("r")) { addr = f.readln() || ""; f.close(); }
	return addr;
}

// Host: tear down the whole muster -- the go marker, every waiter marker, the entry.
function clear_muster(entryPath) {
	var g = go_path(entryPath);
	if (file_exists(g))
		file_remove(g);
	var base = String(entryPath).replace(/\.ini$/i, ".wait.");
	var w = directory(base + "*"), i;
	for (i = 0; i < w.length; i++)
		file_remove(w[i]);
	remove_game(entryPath);
}

// ---------------------------------------------------------------------------
// Paging the door's other active nodes  (Terminal Server context)
// ---------------------------------------------------------------------------

// The access-requirement string for this door's own external-program entry, so we
// only page users who may actually run it. Found by matching the lobby's own
// directory + a JS-module command ("?<mod>") in the external-programs config. ""
// (the default when not found) makes compare_ars() treat everyone as allowed.
// `door_dir` is the lobby's directory (js.exec_dir of the launching script).
function door_ars(door_dir) {
	if (typeof xtrn_area == "undefined" || !xtrn_area.prog_list)
		return "";
	function strip(s) { return s ? String(s).replace(/[\/\\]+$/, "") : ""; }
	var ours = strip(fullpath(door_dir));
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

// Active nodes (node numbers) whose user may run this door -- the page candidates.
// Skips our own node and any in WFC / quiet / logon. `ars` is door_ars()'s result.
function page_targets(ars) {
	var me = bbs.node_num;
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

// Deliver a join invitation to the chosen nodes -- fast and non-interactive. Uses
// the sysop-configurable NodeMsgFmt header (text.dat) so it looks like any other
// inter-node message. `who` is the inviter's alias, `body` the plain-text line
// (NodeMsgFmt supplies its own color + trailing CRLF). Returns the count delivered.
function send_pages(targets, who, body) {
	if (!targets || !targets.length)
		return 0;
	var msg = format(bbs.text("NodeMsgFmt"), bbs.node_num, who || "Someone", body);
	var paged = 0, i;
	for (i = 0; i < targets.length; i++)
		if (system.put_node_message(targets[i], msg))
			paged++;
	return paged;
}

// ---------------------------------------------------------------------------
// Attract art (sysop-dropped full-screen ANSI shown on lobby entry)
// ---------------------------------------------------------------------------

// Resolve a configured directory relative to the door dir, or use it as-is when
// absolute (unix /... or Windows X:\...). Trailing-slashed.
function resolve_dir(door_dir, cfg_dir, dflt) {
	var d = (cfg_dir !== undefined && cfg_dir !== null && cfg_dir !== "")
	        ? String(cfg_dir) : dflt;
	if (d.charAt(0) == "/" || d.charAt(0) == "\\" || d.charAt(1) == ":")
		return backslash(d);
	return backslash(door_dir + d);
}

// The art files a sysop dropped in the attract dir, or []. Case-insensitive on
// the extension (classic art is often UPPER-case *.ANS, directory() is
// case-sensitive on *nix).
function attract_files(door_dir, cfg_dir) {
	var all = directory(resolve_dir(door_dir, cfg_dir, "art") + "*");
	var art = [], i;
	for (i = 0; i < all.length; i++)
		if (/\.(ans|asc|ansi)$/i.test(all[i]))
			art.push(all[i]);
	return art;
}

// ---------------------------------------------------------------------------
// Live who's-online panel  (Terminal Server context)
//
// presence_lib.js is Synchronet's canonical node-status formatter (the BBS's own
// who's-online). We use it so the panel matches the BBS wording, and split each
// line into the username portion and the activity tail by asking for the status
// with and without the username (the length delta is exactly the name) -- so we
// can color name and activity separately without re-implementing its anon/sysop
// rules. `marker` is the door's name as it appears in a playing node's status
// (e.g. "SyncDuke"); those nodes sort first and color differently.
// ---------------------------------------------------------------------------

var _presence = null;
function get_presence() {
	if (_presence)
		return _presence;
	if (typeof bbs == "object" && bbs && bbs.mods)
		_presence = bbs.mods.presence_lib
		            || (bbs.mods.presence_lib = load({}, "presence_lib.js"));
	else
		_presence = load({}, "presence_lib.js");
	return _presence;
}

// In-use nodes for the panel, players of `marker` first, excluding our own node.
// Each entry { num, name, rest, playing }.
function live_nodes(maxn, marker) {
	var presence = get_presence();
	var nl = system.node_list, out = [], n;
	var mynode = (typeof bbs == "object" && bbs && bbs.node_num) ? bbs.node_num : 0;
	var is_sysop = (typeof user == "object" && user && user.is_sysop) ? true : false;
	for (n = 0; n < nl.length; n++) {
		var nd = system.get_node(n + 1);
		if (nd.status != NODE_INUSE && nd.status != NODE_QUIET)
			continue;
		if (mynode && (n + 1) == mynode)
			continue;
		var full = plain(presence.node_status(nd, is_sysop, {}, n));
		var rest = plain(presence.node_status(nd, is_sysop, { exclude_username: true }, n));
		if (!full.length)
			continue;
		var namelen = full.length - rest.length;
		if (namelen < 0)
			namelen = 0;
		out.push({ num: n + 1, name: full.substr(0, namelen), rest: rest,
		           playing: (marker && full.indexOf(marker) >= 0) });
	}
	out.sort(function (a, b) {
		if (a.playing != b.playing) return a.playing ? -1 : 1;
		return a.num - b.num;
	});
	if (maxn && out.length > maxn)
		out = out.slice(0, maxn);
	return out;
}

// One node cell, exactly `cw` visible chars: the name bright (green for a player
// of this game, else white) and the activity/connection normal grey. Clipped (no
// wrap). Color codes are zero-width so the visible width stays cw.
function node_cell(node, cw) {
	var nc = node.playing ? "\1h\1g" : "\1h\1w";
	if (node.name.length >= cw)
		return nc + clip(node.name, cw) + "\1n";
	var rw = cw - node.name.length;
	return nc + node.name + "\1n" + rpad(clip(node.rest, rw), rw) + "\1n";
}

function blank_cell(cw) { return rpad("", cw); }

// ---------------------------------------------------------------------------
// Events log (events.jsonl) -- generic read/prune + a formatted activity feed.
// Game-agnostic: each event is whatever object the caller wrote via
// json_lines.add() (e.g. { time, type, map, ... }); this layer only knows how
// to read the tail, prune the file, and hand back caller-formatted strings.
// ---------------------------------------------------------------------------

// Last `max` parsed events from `path`, oldest first, [] if absent/unreadable.
function read_events(path, max) {
	if (!file_exists(path)) return [];
	var all = _json_lines.get(path, -(max * 6), 0, true);   // last-ish N, recover bad lines
	if (typeof all != "object") return [];                   // get() returns a string on error
	return (all.length > max) ? all.slice(all.length - max) : all;
}

// Rewrite `path` keeping only the last `keep` events if it holds more than `cap`
// *parsed* events (malformed lines are dropped by recover before the count).
// Returns the resulting kept count (or current count if under cap).
function prune_events(path, cap, keep) {
	var all = _json_lines.get(path, 0, 0, true);
	if (typeof all != "object") return 0;
	if (all.length <= cap) return all.length;
	var tail = all.slice(all.length - keep), f = new File(path), i;
	if (!f.open("w+")) return all.length;
	for (i = 0; i < tail.length; i++) f.writeln(JSON.stringify(tail[i]));
	f.close();
	return tail.length;
}

// Last `max` displayable events as Ctrl-A colored lines "[age] <fmt(ev)>", most
// recent first. fmt(ev) returns the plain body; this adds the dim relative-age
// prefix + coloring (the same look as the bottom-panel activity cells). `max_age`
// (seconds, optional) hides events older than that.
function event_feed(path, max, fmt, max_age) {
	var ev = recent_events(path, max, fmt, max_age), out = [], i;
	for (i = 0; i < ev.length; i++)                          // recent_events is newest-first
		out.push("\1k\1h" + ago(ev[i].time) + "\1n \1w" + fmt(ev[i]) + "\1n");
	return out;
}

// Parse a duration string to SECONDS. A bare number is DAYS (fractions allowed, e.g.
// "0.5"); an optional letter suffix overrides the unit: s, m(inutes), h(ours),
// d(ays), w(eeks), y(ears) -- e.g. "48h", "2w", "0.5d". Blank/invalid/<=0 -> 0 (which
// callers treat as "no limit"). Same suffix letters as xpdev's parse_duration; its
// bare unit is seconds, ours is days (a lobby-config convenience -- JS iniGetValue
// has no native duration parsing yet).
function parse_duration(str) {
	var n = parseFloat(str);
	if (isNaN(n) || n <= 0)
		return 0;
	var m = String(str).match(/[a-zA-Z]/);
	switch (m ? m[0].toLowerCase() : "d") {
		case "s": return Math.round(n);
		case "m": return Math.round(n * 60);
		case "h": return Math.round(n * 3600);
		case "w": return Math.round(n * 604800);
		case "y": return Math.round(n * 31536000);
		default:  return Math.round(n * 86400);   // "d" or no suffix -> days
	}
}

// [lobby] activity-feed config accessors (shared by both doors' lobbies).
// activity_max: max events in the full-screen feed (default 18). activity_max_age:
// hide activity older than this, via parse_duration (days by default); 0 = no limit.
function activity_max(cfg) {
	var n = (cfg && cfg.lobby) ? parseInt(cfg.lobby.activity_max, 10) : NaN;
	return (n > 0) ? n : 18;
}
function activity_max_age(cfg) {
	return (cfg && cfg.lobby) ? parse_duration(cfg.lobby.activity_max_age) : 0;
}
// [lobby] panel_rows: max rows in the bottom live panel (nodes + activity). Default 6.
function panel_rows(cfg) {
	var n = (cfg && cfg.lobby) ? parseInt(cfg.lobby.panel_rows, 10) : NaN;
	return (n > 0) ? n : 6;
}

// ago(t) and mmss(secs) already exist above (shared with the who's-online view,
// bracket format matching SyncDOOM's sd_ago); the events feed reuses them.

// ---------------------------------------------------------------------------
// Live who's-online + recent-activity panel  (Terminal Server context)
//
// UI-free: these build the cell STRINGS; the door's lobby.js draws them (bottom-
// anchored, refreshed on a timer). `text_fn(e)` is the door's own per-event
// formatter (it returns falsy to hide an event type from the feed/panel).
// ---------------------------------------------------------------------------

// Up to `max` recent *displayable* event objects (most recent first) -- the ones
// text_fn(e) renders. Oversamples the raw read so hidden events interleaved in the
// tail don't shrink the result below `max`. `max_age` (seconds, optional) hides
// events older than that.
function recent_events(path, max, text_fn, max_age) {
	var all = read_events(path, max * 6), out = [], i, now = time();
	for (i = all.length - 1; i >= 0 && out.length < max; i--) {
		if (max_age && (now - all[i].time) > max_age)
			continue;                            // older than the configured window
		if (text_fn(all[i]))
			out.push(all[i]);
	}
	return out;
}

// One recent-activity cell, exactly `cw` visible chars: "[age] description" (dim
// grey age tag, plain grey body). `text_fn(e)` supplies the description.
function activity_cell(e, cw, text_fn) {
	var tag = ago(e.time);
	var bw = cw - tag.length - 1;
	var body = rpad(clip(text_fn(e) || "", bw), bw);
	return "\1k\1h" + tag + "\1n " + body + "\1n";
}

// Build a bottom-of-lobby panel's cells (one full-width cell per row -- less
// truncation than two narrow columns): live nodes first (players of `marker` first,
// via live_nodes), then recent-activity cells from `events` (newest first), padded
// with blanks to >= 3 rows and capped at `max_rows` (default 6). `cols` sizes the
// single column; `text_fn` renders each event body. Returns { cells, cw }.
function panel_cells(cols, marker, events, text_fn, max_rows) {
	var maxRows = (max_rows > 0) ? max_rows : 6;
	var minRows = (maxRows < 3) ? maxRows : 3;   // blank-pad floor, never above maxRows
	var cw = cols - 1;                        // one cell, full width (avoid last-col wrap)
	var cells = [], i;
	var nodes = live_nodes(maxRows, marker);
	for (i = 0; i < nodes.length; i++)
		cells.push(node_cell(nodes[i], cw));
	for (i = 0; cells.length < maxRows && i < events.length; i++)
		cells.push(activity_cell(events[i], cw, text_fn));
	while (cells.length < minRows)
		cells.push(blank_cell(cw));
	return { cells: cells, cw: cw };
}

// ---- create-lock: a short-lived mutex serializing the "create a game" setup
// window so a second user who picks Multiplayer while someone is mid-create waits
// and then joins, instead of spawning a duplicate game. Lives in the games dir
// (cross-host). file_mutex() reaps a lock older than the max-age, so an abandoned
// setup self-clears with no heartbeat; release is a plain remove.
var CREATE_LOCK_MAX_AGE = 120;   // seconds; comfortably longer than the create picks

function create_lock_path(dir) {
	return backslash(dir) + "creating.lock";
}

// Identity written into the lock file, so release only removes OUR lock (not a
// later creator's that reused the fixed path after we self-released at register).
function create_lock_owner() {
	return (typeof system != "undefined" ? system.local_host_name : "host")
	    + "-" + (typeof bbs != "undefined" ? bbs.node_num : 0);
}

function acquire_create_lock(dir) {
	games_dir_ensure(dir);   // file_mutex() can't create the lock if games/ is absent (fresh install)
	return file_mutex(create_lock_path(dir), create_lock_owner(), CREATE_LOCK_MAX_AGE);
}

// Remove OUR create-lock (idempotent; ownership-scoped so the safety-net release
// after a long-running create() can't yank a different node's later lock).
function release_create_lock(dir) {
	var p = create_lock_path(dir);
	if (!file_exists(p))
		return;
	var f = new File(p), owner = "";
	if (f.open("r")) {
		owner = trim(f.read());
		f.close();
	}
	if (owner == create_lock_owner())
		file_remove(p);
}

// ---- shared [M]ultiplayer lobby flow (both doors). Interactive; parameterized by
// the door via `opts` (games_dir, list(), label(g), create(), join(g), external?).
// Sequential Y/N: join an existing game, else create; a second game while one
// exists is the rarest path, reached only as a follow-up Y/N.
function multiplayer_flow(opts) {
	var games = opts.list();

	if (games.length == 0) {
		if (console.yesno("\r\nNo multiplayer games are waiting. Create one"))
			mp_create(opts);
		else if (opts.external)
			opts.external();
		return;
	}

	if (games.length == 1) {
		if (console.yesno("\r\nJoin " + opts.label(games[0]))) {
			opts.join(games[0]);
			return;
		}
		if (console.yesno("Create one")) {
			mp_create(opts);
			return;
		}
		if (opts.external)
			opts.external();
		return;
	}

	var sel = mp_pick(games, opts);       // >= 2 games
	if (sel) {
		opts.join(sel);
		return;
	}
	if (console.yesno("Create one")) {
		mp_create(opts);
		return;
	}
	if (opts.external)
		opts.external();
}

// Numbered picker for >= 2 games. Returns the chosen game, or null to skip.
function mp_pick(games, opts) {
	var i;
	console.print("\r\n\1h\1cMultiplayer games:\1n\r\n");
	for (i = 0; i < games.length; i++)
		console.print("  \1h\1w" + (i + 1) + "\1n) " + opts.label(games[i]) + "\r\n");
	console.print("\r\nJoin which? [\1h1\1n-\1h" + games.length
	    + "\1n], or \1hN\1n to skip: ");
	var n = console.getnum(games.length);
	if (n < 1 || n > games.length)
		return null;
	return games[n - 1];
}

// Create under the setup-window lock; if another node holds it, poll instead.
function mp_create(opts) {
	if (acquire_create_lock(opts.games_dir)) {
		try { opts.create(); }
		finally { release_create_lock(opts.games_dir); }
		return;
	}
	mp_wait_for_game(opts);
}

// A game is being set up by another node: wait screen + ~1.5s poll. A game
// appearing -> back into the flow's join prompt. Lock freed and still no game
// (creator bailed pre-register) -> we create. Q aborts.
function mp_wait_for_game(opts) {
	console.clear();
	console.print("\r\n\1h\1wA multiplayer game is being set up -- please wait...\1n"
	    + "   (\1hQ\1n=cancel)\r\n");
	while (!js.terminated && bbs.online) {
		if (opts.list().length > 0) {
			multiplayer_flow(opts);
			return;
		}
		if (acquire_create_lock(opts.games_dir)) {
			try { opts.create(); }
			finally { release_create_lock(opts.games_dir); }
			return;
		}
		if (console.inkey(K_UPPER | K_NOECHO, 1500) == "Q")
			return;
	}
}

this;
