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

var SYNCRETRO_DIR    = js.exec_dir;               /* xtrn/syncivision/ */
/* The native artifacts -- the door binary and the libretro core -- live in a
 * per-target sub-directory (sv_target(): win32, linux-x64, linux-arm64,
 * darwin-arm64, freebsd-x64, ...) so one shared install can serve several hosts
 * -- different OSes AND different architectures -- without their same-named
 * binaries colliding. deploy.js and getcore.js put them there; the BIOS and
 * cartridges are platform-independent and stay at the door root. SYNCRETRO_CORE is
 * passed to the door RELATIVE to its launch dir (SYNCRETRO_DIR), which the door
 * resolves against its cwd (bbs.exec runs it in SYNCRETRO_DIR). "%." on SYNCRETRO_BINARY
 * expands to .exe on Windows, blank elsewhere; the core extension is explicit. */
var SYNCRETRO_TARGET = sv_target(system.platform, system.architecture);
/* Native path separator: js.exec_dir is trailing-slashed with the OS separator
 * ("\" on Windows, "/" elsewhere), so its last char is it. The binary path uses
 * it because xtrn.cpp/CreateProcess want native separators, and -- crucially --
 * the command must start with the drive letter (Windows) or "/" (*nix) so
 * external() recognizes it as ABSOLUTE and does not prepend the startup dir. A
 * leading quote defeats that check (cmdline[1] != ':'), so SYNCRETRO_BINARY is
 * used UNQUOTED in the command below, exactly like the sibling doors' SD_BINARY. */
var SYNCRETRO_SEP    = SYNCRETRO_DIR.charAt(SYNCRETRO_DIR.length - 1);

/* Where the binary/core live. Windows is FLAT (sv_target() -> "", so SYNCRETRO_SUB
 * is ""): a .exe/.dll never collides with a *nix "syncretro"/".so" in a shared
 * dir. A *nix host uses an "<os>-<arch>" sub-dir (linux-x64, ...) so two *nixes
 * don't collide -- and the lobby PREFERS that sub-dir but FALLS BACK to the flat
 * door dir when it isn't populated (a single-host install, or the legacy
 * symlink-deploy layout where `syncretro` symlinks straight to the build output).
 * Binary and core are probed INDEPENDENTLY, since deploy.js and getcore.js
 * install them separately. The probe uses the RESOLVED names (".exe" on Windows);
 * the command keeps "%." for cmdstr() to expand. */
var SYNCRETRO_SUB    = SYNCRETRO_TARGET ? SYNCRETRO_TARGET + SYNCRETRO_SEP : "";
var SYNCRETRO_EXE    = "syncretro" + (/^win/i.test(system.platform) ? ".exe" : "");
var SYNCRETRO_CNAME  = "freeintv_libretro." + sv_core_ext(sv_platform(system.platform));
var SYNCRETRO_BPFX   = file_exists(SYNCRETRO_DIR + SYNCRETRO_SUB + SYNCRETRO_EXE)   ? SYNCRETRO_SUB : "";
var SYNCRETRO_CPFX   = file_exists(SYNCRETRO_DIR + SYNCRETRO_SUB + SYNCRETRO_CNAME) ? SYNCRETRO_SUB : "";
var SYNCRETRO_BINARY = SYNCRETRO_DIR + SYNCRETRO_BPFX + "syncretro%.";
var SYNCRETRO_CORE   = SYNCRETRO_CPFX + SYNCRETRO_CNAME;
var SYNCRETRO_CFG    = SYNCRETRO_DIR + "syncretro.ini";

var CELL_W      = 36;
var HEADER_ROWS = 5;                       /* title, board x2, blank, blank */
var FOOTER_ROWS = 2;

function cfg()
{
	var f = new File(SYNCRETRO_CFG);
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

	if (!file_exists(SYNCRETRO_DIR + "exec.bin"))
		missing.push("exec.bin");
	if (!file_exists(SYNCRETRO_DIR + "grom.bin"))
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

/* Returns true when the caller needs to rescan (the picked cartridge is no
 * longer on disk), false otherwise. */
function play(rom)
{
	var home = system.data_dir + "user/" + format("%04d", user.number) + "/intv";
	var cmd, started, secs;

	if (!sv_quote_safe(rom.name)) {
		console.putmsg("\r\n\1h\1rThat cartridge's filename contains a quote, "
		    + "backslash, backtick or dollar sign, which the door's command line "
		    + "cannot carry. Ask the sysop to rename it.\1n\r\n");
		console.pause();
		return false;
	}
	if (!file_exists(rom.path)) {
		console.putmsg("\r\n\1h\1rThat cartridge is gone. Rescanning.\1n\r\n");
		console.pause();
		return true;
	}
	mkpath(home);

	/* Quoting is load-bearing on the ARGUMENTS: xtrn.cpp splits the command line
	 * on bare spaces, and every real cartridge name (and a user's -home path) has
	 * them, so those are quoted -- which also routes external() through the shell
	 * on *nix, reassembling the argument. But SYNCRETRO_BINARY (the leading
	 * program token) is deliberately NOT quoted: external() only recognizes an
	 * absolute path -- and skips prepending the startup dir to it -- when it
	 * begins with the drive letter (Windows: cmdline[1]==':') or "/" (*nix). A
	 * leading quote hides that, so a quoted absolute path gets the startup dir
	 * prepended and becomes a broken doubled command on Windows. The sibling doors
	 * use SD_BINARY unquoted for the same reason. -name %a is NOT quoted here:
	 * cmdstr() already quotes the alias itself, and doubling it would hand the
	 * door literal quote characters. */
	cmd = SYNCRETRO_BINARY + " -s%H -t%T -name %a -core " + SYNCRETRO_CORE
	    + ' -home "' + home + '" "' + rom.path + '"';

	started = time();
	/* EX_NODISPLAY: on Windows, spawn the native door with CREATE_NO_WINDOW so
	 * no per-session console window pops up on the BBS machine (xtrn.cpp) -- the
	 * door draws to the CLIENT's terminal, and its own diagnostics are captured
	 * to data/syncretro/syncretro_n<node>.log. No-op on *nix (no console window).
	 * This is the lobby-launched equivalent of a registered xtrn's XTRN_NODISPLAY
	 * setting; EX_NODISPLAY is the proper EX_* spelling of that bit for bbs.exec. */
	bbs.exec(bbs.cmdstr(cmd), EX_NATIVE | EX_BIN | EX_NODISPLAY, SYNCRETRO_DIR);
	secs = time() - started;

	/* Logged whatever the door's exit status: a crash is still a play. */
	sv_log_play(sv_plays_path(system.data_dir), {
		t:     time(),
		user:  user.number,
		alias: user.alias,
		rom:   rom.name,
		secs:  secs
	});
	return false;
}

function main()
{
	var c        = cfg();
	var missing  = bios_missing();
	var roms, plays, board, cols, per_col, per_page, pages, page, key, i, filter;

	if (missing.length) {
		console.putmsg("\r\n\1h\1rThe Intellivision BIOS is missing from "
		    + SYNCRETRO_DIR + ": " + missing.join(" and ") + "\r\n"
		    + "Without it no cartridge will run. Ask the sysop.\1n\r\n");
		console.pause();
		return;
	}

	roms = sv_discover(SYNCRETRO_DIR + backslash(c.dir), c.ext, c.exclude);
	if (!roms.length) {
		console.putmsg("\r\n\1h\1rNo cartridges found in " + c.dir + "/.\1n\r\n");
		console.pause();
		return;
	}
	for (i = 0; i < roms.length; i++)
		roms[i].num = i + 1;               /* alphabetical, and it never moves */

	filter = roms;
	page   = 0;
	while (!js.terminated && bbs.online) {
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
			if (n >= 1 && n <= roms.length) {
				if (play(roms[n - 1])) {    /* numbers index the FULL list */
					roms = sv_discover(SYNCRETRO_DIR + backslash(c.dir), c.ext, c.exclude);
					if (!roms.length) {
						console.putmsg("\r\n\1h\1rNo cartridges found in "
						    + c.dir + "/.\1n\r\n");
						console.pause();
						return;
					}
					for (i = 0; i < roms.length; i++)
						roms[i].num = i + 1;   /* alphabetical, and it never moves */
					filter = roms;
					page = 0;
				}
			}
			continue;
		}
	}
}

main();
