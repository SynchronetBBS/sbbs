// lobby.js -- SyncRetro's launcher: pick a cartridge, play it, keep the board.
//
// The model layer (syncivision_lib.js) is UI-free and tested under jsexec. This
// file is the only place that touches console/bbs/user. See
// src/doors/syncretro/LAUNCHER.md.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("sbbsdefs.js");                       /* EX_NATIVE, EX_BIN, K_*, P_* */
load(js.exec_dir + "syncivision_lib.js");

var gl = load({}, "game_lobby.js");        /* rpad/clip/ago + live_nodes */

var SR_DIR    = js.exec_dir;               /* xtrn/syncivision/ */
var SR_BINARY = SR_DIR + "syncretro%.";    /* "%." = .exe on Windows, blank here */
var SR_CORE   = "freeintv_libretro.so";
var SR_CFG    = SR_DIR + "syncretro.ini";

var CELL_W      = 36;
var HEADER_ROWS = 5;                       /* title, board x2, blank, blank */
var FOOTER_ROWS = 2;

function cfg()
{
	var f = new File(SR_CFG);
	var out = { dir: "roms", ext: ["int", "bin", "rom"], exclude: [] };
	var ini;

	if (!f.open("r"))
		return out;
	ini = f.iniGetObject("roms");
	f.close();
	if (!ini)
		return out;
	if (ini.dir)
		out.dir = ini.dir;
	if (ini.ext)
		out.ext = String(ini.ext).split(",").map(function (s) { return s.trim(); });
	if (ini.exclude)
		out.exclude = String(ini.exclude).split(",").map(function (s) { return s.trim(); });
	return out;
}

/* The BIOS is what a player actually trips over: without it FreeIntv paints its
 * own LOAD EXEC FAIL screen and the door looks broken. Say so here instead. */
function bios_missing()
{
	var missing = [];

	if (!file_exists(SR_DIR + "exec.bin"))
		missing.push("exec.bin");
	if (!file_exists(SR_DIR + "grom.bin"))
		missing.push("grom.bin");
	return missing;
}

function draw(roms, page, pages, board, cols, per_col)
{
	var i, j, line, idx;

	console.clear();
	console.putmsg("\1h\1cSyncRetro \1n\1c-- Intellivision\1n");
	console.crlf();

	if (board.length) {
		console.putmsg("\1hMost played:\1n ");
		for (i = 0; i < board.length && i < 5; i++)
			console.putmsg(format("%d. %s (%d)  ", i + 1,
			                      gl.clip(board[i].title, 18), board[i].count));
		console.crlf();
	}
	console.crlf();

	for (i = 0; i < per_col; i++) {
		line = "";
		for (j = 0; j < cols; j++) {
			idx = i + j * per_col;
			if (idx >= pages[page].length)
				break;
			line += gl.rpad(sv_cell(pages[page][idx].num, pages[page][idx], CELL_W),
			                CELL_W) + " ";
		}
		if (line !== "")
			console.putmsg(line + "\r\n");
	}

	console.crlf();
	console.putmsg(format("\1h[1-%d]\1n play  \1h[/]\1n search  \1h[N]\1n ext  "
	                      + "\1h[P]\1n rev  \1h[Q]\1n uit        page %d/%d",
	                      roms.length, page + 1, pages.length));
	console.crlf();
}

function play(rom)
{
	var home = system.data_dir + "user/" + format("%04d", user.number) + "/intv";
	var cmd, started, secs;

	if (!sv_quote_safe(rom.name)) {
		console.putmsg("\r\n\1h\1rThat cartridge's filename contains a quote, "
		    + "backslash, backtick or dollar sign, which the door's command line "
		    + "cannot carry. Ask the sysop to rename it.\1n\r\n");
		console.pause();
		return;
	}
	if (!file_exists(rom.path)) {
		console.putmsg("\r\n\1h\1rThat cartridge is gone. Rescanning.\1n\r\n");
		console.pause();
		return;
	}
	mkpath(home);

	/* The quotes are load-bearing: xtrn.cpp splits the command line on bare
	 * spaces, and every real cartridge name has them. A quote diverts the line
	 * through $SHELL -c, which reassembles the argument -- the same path
	 * `-name %a` already takes on every SyncDOOM and SyncDuke launch. */
	cmd = SR_BINARY + " -s%H -t%T -name %a -core " + SR_CORE
	    + " -home " + home + ' "' + rom.path + '"';

	started = time();
	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN, SR_DIR);
	secs = time() - started;

	/* Logged whatever the door's exit status: a crash is still a play. */
	sv_log_play(sv_plays_path(system.data_dir), {
		t:     time(),
		user:  user.number,
		alias: user.alias,
		rom:   rom.name,
		secs:  secs
	});
}

function main()
{
	var c        = cfg();
	var missing  = bios_missing();
	var roms, plays, board, cols, per_col, per_page, pages, page, key, i, filter;

	if (missing.length) {
		console.putmsg("\r\n\1h\1rThe Intellivision BIOS is missing from "
		    + SR_DIR + ": " + missing.join(" and ") + "\r\n"
		    + "Without it no cartridge will run. Ask the sysop.\1n\r\n");
		console.pause();
		return;
	}

	roms = sv_discover(SR_DIR + backslash(c.dir), c.ext, c.exclude);
	if (!roms.length) {
		console.putmsg("\r\n\1h\1rNo cartridges found in " + c.dir + "/.\1n\r\n");
		console.pause();
		return;
	}
	for (i = 0; i < roms.length; i++)
		roms[i].num = i + 1;               /* alphabetical, and it never moves */

	filter = roms;
	page   = 0;
	for (;;) {
		plays    = sv_read_plays(sv_plays_path(system.data_dir));
		board    = sv_top_played(plays, 5);
		cols     = sv_columns(console.screen_columns, CELL_W);
		per_col  = sv_page_rows(console.screen_rows, HEADER_ROWS, FOOTER_ROWS);
		per_page = cols * per_col;
		pages    = sv_paginate(filter, per_page);
		if (page >= pages.length)
			page = pages.length ? pages.length - 1 : 0;
		if (!pages.length) {
			console.putmsg("\r\n\1hNothing matches. \1n");
			console.pause();
			filter = roms;
			continue;
		}

		draw(roms, page, pages, board, cols, per_col);

		key = console.getkey(K_UPPER);
		if (key === "Q")
			return;
		if (key === "N") { if (page + 1 < pages.length) page++; continue; }
		if (key === "P") { if (page > 0) page--; continue; }
		if (key === "/") {
			console.putmsg("\r\nSearch: ");
			var term = console.getstr(30, K_LINE).toLowerCase();
			filter = term === "" ? roms : roms.filter(function (r) {
				return r.title.toLowerCase().indexOf(term) >= 0;
			});
			page = 0;
			continue;
		}
		if (key >= "0" && key <= "9") {
			console.ungetstr(key);
			var n = console.getnum(roms.length);
			if (n >= 1 && n <= roms.length)
				play(roms[n - 1]);          /* numbers index the FULL list */
			continue;
		}
	}
}

main();
