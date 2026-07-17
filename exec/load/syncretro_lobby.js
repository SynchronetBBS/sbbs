// syncretro_lobby.js -- the SyncRetro lobby, shared by every console install.
//
// One SyncRetro door binary hosts any libretro core, so one lobby serves any
// console. This file is that lobby: discovery, the picker, the play-activity
// board, and the door command line. It is the ONLY place that touches
// console/bbs/user; the model layer it stands on (syncretro_lib.js) is UI-free
// and tested headless under jsexec.
//
// A console install (xtrn/syncivision, xtrn/syncnes, ...) ships a three-line
// lobby.js that describes ITS console and calls syncretro_lobby(spec). Nothing
// here knows which console it is running -- that is what lets a new console be
// an install directory rather than a fork of this code.
//
//   load("syncretro_lobby.js");
//   syncretro_lobby({
//       dir:      js.exec_dir,                      // the door dir. REQUIRED.
//       name:     "Nintendo Entertainment System",  // what the player is shown
//       short:    "NES",                            // where a column is tight
//       core:     "fceumm_libretro",                // no extension: .so/.dll/.dylib
//       profile:  "pad",                            // the C door's key bindings
//       ext:      ["nes", "unf", "unif"],           // what a cartridge looks like
//       min_size: 8 * 1024,
//       max_size: 4 * 1024 * 1024,
//       bios:     [],                               // files the console needs
//       stdio:    false                             // true = run as a STDIO door
//   });
//
// `id` is derived from `short` (lower-cased, alphanumerics only) and names the
// per-user save directory and the ROM cache. It is NOT a separate key, because a
// separate key is a thing to get out of step.
//
// The sysop's syncretro.ini is still read, for the keys that are genuinely a
// sysop's business rather than the console's: [roms] exclude= and dir=. The
// console's identity is code, not configuration -- it does not vary per install.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("sbbsdefs.js");                       /* EX_NATIVE, EX_BIN, K_*, P_* */
load("key_defs.js");                       /* KEY_PAGEUP/PAGEDN/HOME/END */
load("syncretro_lib.js");

var syncretro_lobby_gl = load({}, "game_lobby.js");    /* rpad/clip/ago + live_nodes */

var SYNCRETRO_LOBBY_CELL_W      = 38;                  /* colored cell visible width (xtrn_sec look) */
var SYNCRETRO_LOBBY_HEADER_ROWS = 3;                   /* title, top-played, blank */
var SYNCRETRO_LOBBY_FOOTER_ROWS = 3;                   /* blank, prompt, +1 kept empty so a full
                                            * page never trips the terminal more-prompt */

/* Set once, by syncretro_lobby(). */
var syncretro_lobby_dir, syncretro_lobby_con, syncretro_lobby_rules, syncretro_lobby_bios, syncretro_lobby_binary, syncretro_lobby_core, syncretro_lobby_stdio, syncretro_lobby_cfg;

function syncretro_lobby_init(spec)
{
	var f, ini;
	var target, sep, sub, exe, cname, bpfx, cpfx;

	syncretro_lobby_dir   = backslash(spec.dir);
	syncretro_lobby_con   = syncretro_console(spec);
	syncretro_lobby_rules = syncretro_rules(spec);
	syncretro_lobby_bios  = spec.bios || [];
	/* How the door gets the player's connection. Default: a SOCKET (Synchronet
	 * hands the door one end of a loopback socketpair and pumps it). `stdio: true`
	 * instead has Synchronet fork the door on a raw pty (EX_STDIO|EX_BIN) and
	 * relay it -- the same shape Mystic uses on *nix, and so the way to exercise
	 * the door's -stdio path against a real session. */
	syncretro_lobby_stdio = spec.stdio ? true : false;

	/* The sysop's half of syncretro.ini. The console's half is the spec above:
	 * a sysop hides a ROM or moves the roms dir; a sysop does not redefine what
	 * an NES cartridge is. */
	syncretro_lobby_cfg = { lobby: {} };   /* game_lobby.js's shape; never null */
	f = new File(syncretro_lobby_dir + "syncretro.ini");
	if (f.open("r")) {
		ini = f.iniGetObject("roms");
		syncretro_lobby_cfg.lobby = f.iniGetObject("lobby") || {};
		f.close();
		if (ini) {
			if (ini.dir)
				syncretro_lobby_rules.dir = String(ini.dir);
			if (ini.exclude != null)
				syncretro_lobby_rules.exclude = syncretro_list(ini.exclude);
		}
	}

	/* The native artifacts -- the door binary and the libretro core -- live in a
	 * per-target sub-directory (syncretro_target(): win32, linux-x64, linux-arm64,
	 * darwin-arm64, freebsd-x64, ...) so one shared install can serve several
	 * hosts -- different OSes AND different architectures -- without their
	 * same-named binaries colliding. deploy.js and getcore.js put them there; the
	 * BIOS and cartridges are platform-independent and stay at the door root.
	 *
	 * Windows is FLAT (syncretro_target() -> ""): a .exe/.dll never collides with a *nix
	 * "syncretro"/".so" in a shared dir. A *nix host uses an "<os>-<arch>" sub-dir
	 * -- and the lobby PREFERS it but FALLS BACK to the flat door dir when it
	 * isn't populated (a single-host install, or the legacy symlink-deploy layout
	 * where `syncretro` symlinks straight to the build output). Binary and core
	 * are probed INDEPENDENTLY, since deploy.js and getcore.js install them
	 * separately.
	 *
	 * The command must start with the drive letter (Windows) or "/" (*nix) so
	 * external() recognizes it as ABSOLUTE and does not prepend the startup dir. A
	 * leading quote defeats that check (cmdline[1] != ':'), so syncretro_lobby_binary is used
	 * UNQUOTED below, exactly like the sibling doors' SD_BINARY. */
	target = syncretro_target(system.platform, system.architecture);
	sep    = syncretro_lobby_dir.charAt(syncretro_lobby_dir.length - 1);
	sub    = target ? target + sep : "";
	exe    = "syncretro" + (/^win/i.test(system.platform) ? ".exe" : "");
	cname  = syncretro_lobby_con.core + "." + syncretro_core_ext(syncretro_platform(system.platform));
	bpfx   = file_exists(syncretro_lobby_dir + sub + exe)   ? sub : "";
	cpfx   = file_exists(syncretro_lobby_dir + sub + cname) ? sub : "";

	syncretro_lobby_binary = syncretro_lobby_dir + bpfx + "syncretro%.";   /* "%." -> .exe on Windows */
	syncretro_lobby_core   = cpfx + cname;                     /* relative to the door's cwd */
}

/* The BIOS is what a player actually trips over: without it FreeIntv paints its
 * own LOAD EXEC FAIL screen and the door looks broken. Say so here instead. A
 * console with no BIOS (the NES) lists none, and this never fires. */
function syncretro_lobby_bios_missing()
{
	var missing = [];

	syncretro_lobby_bios.forEach(function (name) {
		if (!file_exists(syncretro_lobby_dir + name))
			missing.push(name);
	});
	return missing;
}

function syncretro_lobby_draw(roms, page, pages, board, cols, per_col)
{
	var i, j, line, idx, rom, t, plain, mp, mpv;

	console.clear();
	console.line_counter = 0;   /* we page ourselves (N/P); don't let the terminal
	                             * insert a more-prompt in the middle of a page */

	console.putmsg("\1h\1cSyncRetro \1n\1c-- " + syncretro_lobby_con.name + "\1n");
	console.crlf();

	/* "Top played" is built to a fixed VISIBLE width -- tracked in the plain-text
	 * `mpv`, since the colored `mp` carries \1x codes that take no column -- so it
	 * is always exactly ONE line. That keeps the header height constant; a line
	 * that wrapped past 80 cols would grow the header, push the page over an 80x24
	 * screen, and trip the terminal's pause. */
	mp = "";
	if (board.length) {
		mp  = "\1hTop played:\1n ";
		mpv = "Top played: ";
		for (i = 0; i < board.length && i < 5; i++) {
			t     = syncretro_lobby_gl.clip(board[i].label || board[i].title, 14);
			plain = format("%s (%d)  ", t, board[i].count);   /* no rank -- order shows it */
			if (mpv.length + plain.length > console.screen_columns - 1)
				break;
			mp  += format("\1c%s \1h(%d)\1n  ", t, board[i].count);
			mpv += plain;
		}
	}
	console.putmsg(mp);
	console.crlf();
	console.crlf();

	for (i = 0; i < per_col; i++) {
		line = "";
		for (j = 0; j < cols; j++) {
			idx = i + j * per_col;
			if (idx >= pages[page].length)
				break;
			rom   = pages[page][idx];
			line += syncretro_cell(rom.num, rom, SYNCRETRO_LOBBY_CELL_W) + " ";   /* fixed-width; no rpad */
		}
		if (line !== "")
			console.putmsg(line + "\r\n");
	}

	console.crlf();
	/* Condensed prompt, hotkeys in bright cyan: any number plays that cartridge,
	 * F searches, N/P page, Q quits. The unprompted aliases ('/' for F, '+'/'-'
	 * and Enter/PgUp/PgDn/Home/End for paging) are left off -- the row has to stay
	 * inside 80 columns, and every key it does name spells out its own word. No
	 * trailing CRLF -- the cursor rests on the bottom row, which is never scrolled,
	 * so the terminal never pauses. */
	console.putmsg(format(
	    "\1h\1c#\1n play   \1h\1cF\1nind   \1h\1cN\1next \1h\1cP\1nrev   \1h\1cQ\1nuit"
	    + "   \1cPage \1h%d\1n\1c of \1h%d\1n: ", page + 1, pages.length));
}

/* Returns true when the caller needs to rescan (the picked cartridge is no
 * longer on disk), false otherwise. */
function syncretro_lobby_play(rom)
{
	var home = system.data_dir + "user/" + format("%04d", user.number) + "/" + syncretro_lobby_con.id;
	var cmd, started, secs, label;

	if (!syncretro_quote_safe(rom.name)) {
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
	 * on *nix, reassembling the argument. But syncretro_lobby_binary (the leading program
	 * token) is deliberately NOT quoted; see syncretro_lobby_init(). -name %a is NOT quoted
	 * either: cmdstr() already quotes the alias, and doubling it would hand the
	 * door literal quote characters.
	 *
	 * -profile tells the door which console's key bindings to use (pad, intv).
	 * The door can infer it from the core's library_name when run bare from a
	 * command line, but the lobby KNOWS, so it says so. */
	/* -title / -console are the who's-online line: the door publishes
	 * "playing Astrosmash (Intellivision)" as this node's status, the way SyncDOOM
	 * names its WAD and map. We pass the PARSED title (syncretro_parse_title already
	 * stripped "(1981) (Mattel)" and any dump marker off the filename), because
	 * the door would otherwise have to re-implement that parsing in C.
	 *
	 * The console label is the long name when it fits a status line, else the
	 * short one: "Intellivision" reads better than "Intv", but "Nintendo
	 * Entertainment System" is too long to sit in a who's-online column. */
	label = syncretro_lobby_con.name.length <= 20 ? syncretro_lobby_con.name : syncretro_lobby_con.short;

	/* A SOCKET door is handed the connection (-s%H). A STDIO door is handed
	 * nothing: Synchronet forks it on a pty and relays fd 0/1 itself, so the
	 * socket argument must be ABSENT, not empty. EX_BIN is what makes that pty
	 * raw (cfmakeraw) and stops the LF->CRLF and CP437->UTF8 translation that
	 * would otherwise mangle a sixel frame. */
	cmd = syncretro_lobby_binary + (syncretro_lobby_stdio ? " -stdio" : " -s%H")
	    + " -t%T -name %a -core " + syncretro_lobby_core
	    + " -profile " + syncretro_lobby_con.profile
	    + ' -title "' + (rom.label || rom.title) + '" -console "' + label + '"'
	    + ' -home "' + home + '" "' + rom.path + '"';

	/* The Terminal Server logs "Executing external program: <console>" when it
	 * spawns the lobby; the cartridge is picked in here, so the node log would
	 * otherwise never say what was played. Same "X-" code as the server's own
	 * line (one grep finds both), and the same wording as the who's-online status
	 * the door publishes from -title/-console above. */
	bbs.logline("X-", "Playing " + (rom.label || rom.title) + " (" + label + ")");

	started = time();
	/* EX_NODISPLAY: on Windows, spawn the native door with CREATE_NO_WINDOW so
	 * no per-session console window pops up on the BBS machine (xtrn.cpp) -- the
	 * door draws to the CLIENT's terminal, and its own diagnostics are captured
	 * to data/syncretro/syncretro_n<node>.log. No-op on *nix (no console window).
	 * This is the lobby-launched equivalent of a registered xtrn's XTRN_NODISPLAY
	 * setting; EX_NODISPLAY is the proper EX_* spelling of that bit for bbs.exec. */
	bbs.exec(bbs.cmdstr(cmd),
	         EX_NATIVE | EX_BIN | EX_NODISPLAY | (syncretro_lobby_stdio ? EX_STDIO : 0),
	         syncretro_lobby_dir);
	secs = time() - started;

	/* Logged whatever the door's exit status: a crash is still a play. The console
	 * rides along, so one append-only log serves every console and the board can
	 * still show only this one's. */
	syncretro_log_play(syncretro_plays_path(system.data_dir), {
		t:       time(),
		user:    user.number,
		alias:   user.alias,
		rom:     rom.name,
		console: syncretro_lobby_con.id,
		secs:    secs
	});
	return false;
}

/* Discovery, through the on-disk hash cache (syncretro_lib.js).
 *
 * Without it, drawing this menu opened, read and hashed every cartridge -- and
 * the install is typically an SMB mount, so a remote node paid that as one round
 * trip per ROM, every single time. With it, a warm run opens exactly one file. A
 * cold run still pays the full scan, so it says so rather than appearing to
 * hang -- on a big ROM set that first scan is genuinely slow. */
function syncretro_lobby_discover()
{
	var cache = syncretro_cache_open(syncretro_cache_path(system.data_dir, syncretro_lobby_con.id));
	var cold  = !Object.keys(cache.entries).length;
	var roms;

	if (cold)
		console.putmsg("\r\n\1hScanning cartridges (first run, this takes a moment)...\1n");
	roms = syncretro_discover(syncretro_lobby_dir + backslash(syncretro_lobby_rules.dir), syncretro_lobby_rules, cache);
	syncretro_cache_flush(cache);
	if (cold)
		console.putmsg(" \1h\1c" + roms.length + "\1n\1h found.\1n\r\n");
	return roms;
}

function syncretro_lobby_number(roms)
{
	var i;

	for (i = 0; i < roms.length; i++)
		roms[i].num = i + 1;               /* alphabetical, and it never moves */
	return roms;
}

function syncretro_lobby(spec)
{
	var roms, plays, board, cols, per_col, per_page, pages, page, key, i, filter;
	var missing;

	syncretro_lobby_init(spec);

	missing = syncretro_lobby_bios_missing();
	if (missing.length) {
		console.putmsg("\r\n\1h\1rThe " + syncretro_lobby_con.name + " BIOS is missing from "
		    + syncretro_lobby_dir + ": " + missing.join(" and ") + "\r\n"
		    + "Without it no cartridge will run. Ask the sysop.\1n\r\n");
		console.pause();
		return;
	}

	roms = syncretro_lobby_discover();
	if (!roms.length) {
		console.putmsg("\r\n\1h\1rNo cartridges found in " + syncretro_lobby_rules.dir + "/.\1n\r\n");
		console.pause();
		return;
	}
	syncretro_lobby_number(roms);

	/* One-shot entry sound, if the sysop configured one -- the same helper and the
	 * same [lobby] enter_sound key SyncDuke and SyncDOOM use. Silent unless the
	 * terminal can decode audio files. After the guards above, so it never plays
	 * into an error message. */
	syncretro_lobby_gl.enter_sound(syncretro_lobby_dir, syncretro_lobby_cfg);

	filter = roms;
	page   = 0;
	while (!js.terminated && bbs.online) {
		plays    = syncretro_read_plays(syncretro_plays_path(system.data_dir), syncretro_lobby_con.id);
		board    = syncretro_top_played(plays, 5);
		cols     = syncretro_columns(console.screen_columns, SYNCRETRO_LOBBY_CELL_W);
		per_col  = syncretro_page_rows(console.screen_rows, SYNCRETRO_LOBBY_HEADER_ROWS, SYNCRETRO_LOBBY_FOOTER_ROWS);
		per_page = cols * per_col;
		pages    = syncretro_paginate(filter, per_page);
		if (page >= pages.length)
			page = pages.length ? pages.length - 1 : 0;
		if (!pages.length) {
			console.putmsg("\r\n\1hNothing matches. \1n");
			console.pause();
			filter = roms;
			continue;
		}

		syncretro_lobby_draw(roms, page, pages, board, cols, per_col);

		key = console.getkey(K_UPPER);
		if (key === "Q")
			return;

		/* Paging. The nav keys do what their labels say -- Enter and PgDn advance
		 * like N, PgUp goes back like P, Home and End jump to the ends -- because a
		 * player's fingers reach for them before they read the prompt. '+'/'-' page
		 * the same way: they sit next to each other on the keyboard (and on the
		 * numeric keypad, under the hand that just typed a cartridge number), so
		 * they read as forward/back without being told. Both are below '0' in ASCII,
		 * so neither can be mistaken for the start of a cartridge number below.
		 *
		 * PgUp is safe to bind even though the terminal layer translates it to
		 * CTRL_P, which is ALSO the BBS's node-message hotkey: parse_input_sequence()
		 * turns the escape sequence (ESC[V, ESC[5~) into the key code and returns
		 * before inkey() gets to its Ctrl-P case, so only a literally typed Ctrl-P
		 * pages a node. Same for PgDn/CTRL_N, Home/CTRL_B, End/CTRL_E. */
		if (key === "N" || key === "+" || key === "\r" || key === "\n" || key === KEY_PAGEDN) {
			if (page + 1 < pages.length) page++;
			continue;
		}
		if (key === "P" || key === "-" || key === KEY_PAGEUP) { if (page > 0) page--; continue; }
		if (key === KEY_HOME) { page = 0; continue; }
		if (key === KEY_END) { page = pages.length - 1; continue; }
		/* 'F'ind is the prompted key -- it names itself the way Next/Prev/Quit do.
		 * '/' stays bound as the unprompted alias for the fingers that expect it. */
		if (key === "F" || key === "/") {
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
				if (syncretro_lobby_play(roms[n - 1])) {    /* numbers index the FULL list */
					roms = syncretro_lobby_discover();
					if (!roms.length) {
						console.putmsg("\r\n\1h\1rNo cartridges found in "
						    + syncretro_lobby_rules.dir + "/.\1n\r\n");
						console.pause();
						return;
					}
					syncretro_lobby_number(roms);
					filter = roms;
					page = 0;
				}
			}
			continue;
		}
	}
}
