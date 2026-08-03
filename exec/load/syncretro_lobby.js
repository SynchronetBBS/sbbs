// syncretro_lobby.js -- the SyncRetro lobby, shared by every console install.
//
// One SyncRetro door binary hosts any libretro core, so one lobby serves any
// console. This file is that lobby: discovery, the picker, the play-activity
// board, and the door command line. It is the ONLY place that touches
// console/bbs/user; the model layer it stands on (syncretro_lib.js) is UI-free
// and tested headless under jsexec.
//
// A console install (xtrn/syncivision, xtrn/syncnes, ...) ships a two-line
// lobby.js:
//
//   load("syncretro_lobby.js");
//   syncretro_lobby();
//
// Nothing here knows which console it is running -- that is what lets a new
// console be an install directory rather than a fork of this code. The
// console's identity (name, short, core, profile, shared_saves), its ROM
// rules ([roms] ext/min_size/max_size/exclude/bios_*) and everything else
// this lobby needs -- [text], [lobby], [idle] -- come from that install's
// syncretro.ini, read by syncretro_lobby_ini() below.
//
// syncretro.ini is SHIPPED, beside the package's lobby.js, and an upgrade
// replaces it. A sysop's own changes belong in syncretro.local.ini, beside
// it, read second and winning key by key -- so it holds only what they
// changed, and every new default added upstream still arrives underneath it.
//
// BOTH HALVES OF THE DOOR READ THE SAME SHIPPED FILE: this lobby, and the
// native binary. That is the whole reason it is a file rather than either
// one's code -- these facts used to travel from the lobby to the door on the
// COMMAND LINE, which Synchronet assembles into a 260-byte buffer on Windows
// and truncates there in silence, so a long cartridge name pushed the ROM
// argument off the end of the line.
//
// `id` is derived from `short` (lower-cased, alphanumerics only) and names the
// per-user save directory and the ROM cache. It is NOT a separate key, because a
// separate key is a thing to get out of step.
//
// `spec` survives as an escape hatch for a lobby run from an unusual
// directory: syncretro_lobby_init() below reads ONLY spec.dir (default
// js.exec_dir) and spec.data_dir (default system.data_dir -- an override
// point for tests, so a headless run never writes the derived core-hash
// cache or a shared-cabinet home directory into a live install's data/)
// and ignores every other key -- there is no console-describing spec key
// left to read.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("sbbsdefs.js");                       /* EX_NATIVE, EX_BIN, K_*, P_* */
load("key_defs.js");                       /* KEY_PAGEUP/PAGEDN/HOME/END */
load("syncretro_lib.js");

var syncretro_lobby_gl = load({}, "game_lobby.js");    /* rpad/clip/ago + live_nodes */
var syncretro_lobby_up = load({}, "userprops.js");     /* the cabinet preference; scoped for the
                                            * same reason as game_lobby.js above */

/* Idle timeout used when syncretro.ini has no [idle] timeout. This lobby always
 * passes -i, so THIS is the shipped policy on a lobby install -- and it must
 * match SR_IDLE_DEFAULT in src/doors/syncretro/syncretro_config.c, which
 * governs the no-lobby (DOOR32.SYS/other-BBS) path. Change both together. */
var SYNCRETRO_IDLE_DEFAULT      = 600;                 /* 10 minutes */

var SYNCRETRO_LOBBY_CELL_W      = 38;                  /* colored cell visible width (xtrn_sec look) */
var SYNCRETRO_LOBBY_HEADER_ROWS = 3;                   /* title, top-played, blank */
var SYNCRETRO_LOBBY_FOOTER_ROWS = 3;                   /* blank, prompt, +1 kept empty so a full
                                            * page never trips the terminal more-prompt */

/* Toggles the cabinet line on a shared-saves console. NOT "P": that letter is
 * already the prompt's own Prev-page hotkey (see the "prompt" text below and
 * the paging block in syncretro_lobby()), so binding it here too would make
 * one keypress do two different things depending on which the player meant.
 * "C" for Cabinet is free. */
var SYNCRETRO_LOBBY_CABINET_KEY = "C";

/* Every string this lobby displays, with the string it has always displayed as
 * its default. The sysop's syncretro.ini [text] section overrides any of them,
 * so an install with no [text] section draws exactly what it drew before this
 * existed.
 *
 * WRITE [text] KEYS WITH A COLON, NOT AN EQUALS SIGN:
 *
 *     title : \1h\1cSyncRetro \1n\1c-- %s\1n
 *
 * A colon makes Synchronet's ini parser treat the value as a C string literal
 * and unescape it (xpdev/ini_file.c key_name() -> c_unescape_str()), so "\1h" is
 * the Ctrl-A attribute code and "\xb3" the CP437 byte -- the same spelling
 * ctrl/text.dat uses, which is the point: a sysop already knows it, and nothing
 * here has to decode anything. A colon also preserves a TRAILING SPACE, which
 * several of these strings need. An `=` line is taken literally, backslashes and
 * all.
 *
 * A BLANK value means "draw nothing here", including any CRLF the default
 * carries: the section is read with blanks enabled, so present-but-empty is
 * distinguishable from absent. That is how the "Top played" row is turned off.
 *
 * The %-arguments each key is handed are fixed, listed beside it here and
 * documented in syncretro.ini. A string that ignores its arguments is
 * fine; one that asks for an argument never passed prints rubbish. */
var SYNCRETRO_LOBBY_TEXT = {
	/* header block */
	title:          "\1h\1cSyncRetro \1n\1c-- %s\1n",       /* console name */
	top_played:     "\1hTop played:\1n ",                   /* (no args) row label */
	top_played_fmt: "\1c%s \1h(%d)\1n  ",                   /* cartridge title, play count */
	/* the cabinet line -- drawn only on a shared-saves console (today only the
	 * arcade). %s is the "(C to switch)" hint (cabinet_hint below), already
	 * formatted and blank for a guest, whose choice cannot persist.
	 *
	 * Both lines below read as a RULE, not a status: "No saved games." was
	 * ambiguous on a fast read -- it can parse as "none saved yet" rather than
	 * "you cannot save here", and that misreading costs a player the game they
	 * believed was saved. */
	cabinet_public:  "\1h\1cCabinet:\1n  [Public]  Private%s\r\n"
	              + "\1n          Scores count.  Games cannot be saved here.",
	cabinet_private: "\1h\1cCabinet:\1n   Public  [Private]%s\r\n"
	              + "\1n           Scores are private.  Games save and resume.",
	cabinet_hint:    "       (%s to switch)",               /* the toggle key */
	/* the cartridge grid */
	cell_fmt:       SYNCRETRO_CELL_FMT,                     /* number, title */
	/* footer */
	prompt:         "\1h\1c#\1n play   \1h\1cF\1nind   \1h\1cN\1next \1h\1cP\1nrev   "
	              + "\1h\1cQ\1nuit   \1cPage \1h%d\1n\1c of \1h%d\1n: ",   /* page, page count */
	search:         "\r\nSearch: ",
	/* messages */
	no_match:       "\r\n\1hNothing matches. \1n",
	scanning:       "\r\n\1hScanning cartridges (first run, this takes a moment)...\1n",
	scanned:        " \1h\1c%d\1n\1h found.\1n\r\n",        /* cartridges found */
	no_roms:        "\r\n\1h\1rNo cartridges found in %s/.\1n\r\n",       /* ROM directory */
	bios_missing:   "\r\n\1h\1rThe %s BIOS is missing from %s: %s\r\n"     /* console, dir, files */
	              + "Without it no cartridge will run. Ask the sysop.\1n\r\n",
	unsafe_name:    "\r\n\1h\1rThat cartridge's filename contains a quote, backslash, "
	              + "backtick or dollar sign, which the door's command line cannot "
	              + "carry. Ask the sysop to rename it.\1n\r\n",
	rom_gone:       "\r\n\1h\1rThat cartridge is gone. Rescanning.\1n\r\n"
};

/* Set once, by syncretro_lobby(). syncretro_lobby_data_dir is spec.data_dir
 * (default system.data_dir) -- see the spec comment above; every data_dir-
 * derived path in this file (the core-hash cache, the shared/private -home)
 * reads it rather than system.data_dir directly, so a test pointing it at
 * system.temp_dir never touches the live install's data/. */
var syncretro_lobby_dir, syncretro_lobby_con, syncretro_lobby_rules, syncretro_lobby_bios, syncretro_lobby_binary, syncretro_lobby_stdio, syncretro_lobby_cfg;
var syncretro_lobby_data_dir;
/* The whole merged ini (syncretro_lobby_ini()'s return), the console's
 * libretro core hash, and games.ini/games.local.ini's rows keyed by romset --
 * all resolved ONCE in syncretro_lobby_init() and read per cartridge from
 * here rather than re-read or re-hashed. See syncretro_lobby_state_key(). */
var syncretro_lobby_ini_cache, syncretro_lobby_core_md5, syncretro_lobby_games;
var syncretro_lobby_cellw;                             /* [lobby] cell_width, or the default above */
var syncretro_lobby_header, syncretro_lobby_footer;    /* optional display files ("" = none) */
var syncretro_lobby_hrows, syncretro_lobby_frows;      /* rows those blocks occupy; see the draw */

/* One display string: the sysop's when the key appears in [text], else the
 * shipped default. Present-but-blank is a legitimate override meaning "draw
 * nothing", so absence is tested rather than truthiness. */
function syncretro_lobby_text(key)
{
	var t = syncretro_lobby_cfg.text;

	if (t && t[key] !== undefined)
		return String(t[key]);
	return SYNCRETRO_LOBBY_TEXT[key];
}

/* Print one of those strings, formatted with `args`. A blank string prints
 * nothing at all -- not even the CRLFs its default carries. */
function syncretro_lobby_print(key, args)
{
	var s = syncretro_lobby_text(key);

	if (s === "")
		return;
	console.putmsg(args && args.length ? format.apply(null, [s].concat(args)) : s);
}

/* One section, shipped then overlaid. `blanks` keeps a key that is present but
 * EMPTY -- a real override in [lobby] and [text] ("draw nothing", "no display
 * file"), where iniGetObject would otherwise drop it and make it
 * indistinguishable from an absent key. */
function syncretro_lobby_ini_section(base, local, section, blanks)
{
	var out = {};
	var b, l, k;

	b = base ? base.iniGetObject(section, false, blanks) : null;
	l = local ? local.iniGetObject(section, false, blanks) : null;
	if (b) {
		for (k in b)
			out[k] = b[k];
	}
	if (l) {
		for (k in l)
			out[k] = l[k];
	}
	return out;
}

/* syncretro.ini is SHIPPED and read first; syncretro.local.ini is the SYSOP'S
 * and read over the top, winning key by key. The shipped file is also read by
 * the DOOR, which is the whole point of it being a file: these are the facts
 * both halves need, and they used to travel from here to the door on the
 * command line. Synchronet assembles that line into a 260-byte buffer on
 * Windows and truncates it there in silence, so a long cartridge name pushed
 * the ROM argument -- the last thing on the line -- off the end, and the door
 * reported "(no ROM)" for content the BBS had just logged the full path of.
 *
 * Every section is an object, never null: a caller may index it without
 * guarding. Both files are optional. */
function syncretro_lobby_ini(dir)
{
	var base  = new File(backslash(dir) + "syncretro.ini");
	var local = new File(backslash(dir) + "syncretro.local.ini");
	var out;

	if (!base.open("r"))
		base = null;
	if (!local.open("r"))
		local = null;

	out = {
		console: syncretro_lobby_ini_section(base, local, "console", false),
		roms:    syncretro_lobby_ini_section(base, local, "roms", false),
		lobby:   syncretro_lobby_ini_section(base, local, "lobby", true),
		text:    syncretro_lobby_ini_section(base, local, "text", true),
		idle:    syncretro_lobby_ini_section(base, local, "idle", false),
		/* [options]: the core options pinned in syncretro.ini/local (retro_options.h)
		 * -- resolved here only so syncretro_lobby_state_key() can flatten them into
		 * the snapshot staleness key; never sent to the door on the command line.
		 * [state]: the sysop's suspend/resume switch (auto_resume). */
		options: syncretro_lobby_ini_section(base, local, "options", false),
		state:   syncretro_lobby_ini_section(base, local, "state", false)
	};

	if (base)
		base.close();
	if (local)
		local.close();
	return out;
}

function syncretro_lobby_init(spec)
{
	var f, ini;
	var target, sep, sub, exe, bpfx;

	if (!spec)
		spec = {};
	syncretro_lobby_dir = backslash(spec.dir || js.exec_dir);
	syncretro_lobby_data_dir = spec.data_dir || system.data_dir;
	ini = syncretro_lobby_ini(syncretro_lobby_dir);
	syncretro_lobby_ini_cache = ini;

	syncretro_lobby_con   = syncretro_console(ini.console);
	syncretro_lobby_rules = syncretro_rules(ini.roms);
	syncretro_lobby_bios  = syncretro_list(ini.console.bios);
	/* How the door gets the player's connection. Default: a SOCKET (Synchronet
	 * hands the door one end of a loopback socketpair and pumps it). `stdio`
	 * instead has Synchronet fork the door on a raw pty (EX_STDIO|EX_BIN) and
	 * relay it -- the same shape Mystic uses on *nix, and so the way to
	 * exercise the door's -stdio path against a real session. */
	syncretro_lobby_stdio = String(ini.console.stdio).toLowerCase() === "true";

	syncretro_lobby_cfg = { lobby: ini.lobby, text: ini.text, idle: ini.idle };

	/* A cell_fmt that lost its %s would draw a grid of numbers with no cartridge
	 * titles -- which reads as a broken door rather than as a bad setting. Say so
	 * and fall back, rather than obey it. */
	if (syncretro_lobby_cfg.text.cell_fmt !== undefined
	    && String(syncretro_lobby_cfg.text.cell_fmt).indexOf("%s") < 0) {
		log(LOG_WARNING, "syncretro: [text] cell_fmt has no %s (the cartridge title) "
		    + "-- using the built-in format");
		delete syncretro_lobby_cfg.text.cell_fmt;
	}

	syncretro_lobby_cellw = parseInt(syncretro_lobby_cfg.lobby.cell_width, 10);
	if (!(syncretro_lobby_cellw > 0))
		syncretro_lobby_cellw = SYNCRETRO_LOBBY_CELL_W;

	/* The optional display files: art the sysop drops in the door's own directory
	 * to head or foot the picker, the way xtrn_sec.js takes an xtrn_head/xtrn_tail
	 * menu file. An absent [lobby] header/footer key auto-detects lobby_header.*
	 * and lobby_footer.* here; a key names some other file (relative to this dir,
	 * or absolute); a key present but BLANK turns the file off.
	 *
	 * A header REPLACES the built-in title line, which is what xtrn_sec.js does
	 * when an xtrn_head file exists. The live "Top played" row is independent of
	 * it -- that row is data, not decoration -- and is turned off on its own by
	 * blanking [text] top_played.
	 *
	 * The row counts are only the FIRST guess at the page geometry; the draw
	 * measures what it actually printed and corrects them. See syncretro_lobby_draw(). */
	syncretro_lobby_header = syncretro_lobby_display_file("header", "lobby_header");
	syncretro_lobby_footer = syncretro_lobby_display_file("footer", "lobby_footer");
	syncretro_lobby_hrows = SYNCRETRO_LOBBY_HEADER_ROWS;
	syncretro_lobby_frows = SYNCRETRO_LOBBY_FOOTER_ROWS;
	if (syncretro_lobby_header)   /* replaces the title's one row with its own */
		syncretro_lobby_hrows += syncretro_lobby_gl.display_file_rows(syncretro_lobby_header) - 1;
	else if (syncretro_lobby_text("title") === "")
		syncretro_lobby_hrows--;
	if (syncretro_lobby_text("top_played") === "")
		syncretro_lobby_hrows--;
	if (syncretro_lobby_con.shared_saves)   /* the cabinet line -- see syncretro_lobby_draw() */
		syncretro_lobby_hrows += 2;
	if (syncretro_lobby_footer)
		syncretro_lobby_frows += syncretro_lobby_gl.display_file_rows(syncretro_lobby_footer);

	/* games.ini -- per-cabinet facts for a console whose ROM filenames are
	 * identifiers the emulator matches on and so cannot be renamed (arcade).
	 * The lobby reads the display TITLE and nothing else; the door reads the
	 * same file for the control labels its help screen needs (GAMES_INI.md).
	 *
	 * Optional and per-install: absent for every cartridge console. An ini
	 * degrades per line rather than all-or-nothing, so unlike the JSON it
	 * replaced there is no parse failure that can cost the whole file.
	 *
	 * iniGetAllObjects("romset"), NOT iniGetAllObjects(): the default name
	 * property is "name", which a section's own `name = ` key overwrites --
	 * the section name would be silently lost. */
	syncretro_names_set(null);
	if (file_exists(syncretro_lobby_dir + "names.json"))
		log(LOG_WARNING, "syncretro: names.json is no longer read -- "
			+ "re-enter any custom titles as games.local.ini sections");

	/* games.local.ini is read SECOND and wins, romset by romset: it is the
	 * sysop's file, where a retitled cabinet survives the pull that overwrites
	 * the shipped games.ini. Holding only the differences is the point -- the
	 * titles upstream adds keep arriving underneath. */
	var files = ["games.ini", "games.local.ini"];
	var map   = {};
	var found = false;
	var n, rows, i;

	/* Every parsed row, keyed by LOWERCASED romset -- reused by
	 * syncretro_lobby_games_save_state() so it need not read either file a
	 * second time. Lower-cased for the same reason syncretro_names_set() lower-
	 * cases its own keys: a romset's case is not guaranteed across platforms. */
	syncretro_lobby_games = {};
	for (n = 0; n < files.length; n++) {
		f = new File(syncretro_lobby_dir + files[n]);
		if (!f.open("r"))
			continue;
		found = true;
		rows  = f.iniGetAllObjects("romset");
		f.close();
		for (i = 0; i < rows.length; i++) {
			if (rows[i].romset && rows[i].name)
				map[rows[i].romset] = rows[i].name;
			if (rows[i].romset)
				syncretro_lobby_games[String(rows[i].romset).toLowerCase()] = rows[i];
		}
	}
	if (found)
		syncretro_names_set(map);

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
	bpfx   = file_exists(syncretro_lobby_dir + sub + exe)   ? sub : "";

	syncretro_lobby_binary = syncretro_lobby_dir + bpfx + "syncretro%.";   /* "%." -> .exe on Windows */
	/* Only the BINARY is probed here. The core is the door's own business now:
	 * it searches the door directory and the per-target sub-dir for the name in
	 * syncretro.ini, or for the one "*_libretro" it can find. Naming the core to
	 * the door cost 28 characters of a 260-character command line. */

	/* The core's hash is resolved once here rather than per cartridge -- it is
	 * one of the three inputs to the snapshot staleness key
	 * (syncretro_lobby_state_key()), and re-finding or re-hashing a
	 * multi-megabyte core on every lobby entry is exactly the cost
	 * syncretro_core_md5()'s own cache exists to avoid. syncretro_core_path()
	 * has to find the SAME file the door will load without being told -- see
	 * its own comment -- since the lobby never passes -core. */
	syncretro_lobby_core_md5 = syncretro_core_md5(
	    syncretro_core_path(syncretro_lobby_dir, syncretro_lobby_con.core,
	                        syncretro_core_ext(syncretro_platform(system.platform))),
	    syncretro_core_cache_path(syncretro_lobby_data_dir, syncretro_lobby_con.id));
}

/* Resolve one [lobby] display-file key. Absent -> the auto-detected default name;
 * blank -> off; otherwise the sysop's own path. game_lobby.js picks the extension
 * from what the terminal can render (.ans / .asc / .msg) and returns "" when
 * nothing is installed, so the ordinary install configures nothing and gets
 * nothing extra drawn. */
function syncretro_lobby_display_file(key, dflt)
{
	var name = syncretro_lobby_cfg.lobby[key];

	if (name === undefined)
		name = dflt;
	return syncretro_lobby_gl.display_file(syncretro_lobby_dir, name);
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

/* The "Top played" row: the live board, built to a fixed VISIBLE width -- the
 * formatted entry is measured with its \1x codes stripped, since those take no
 * column -- so the row is always exactly ONE line however the sysop colors it.
 * That keeps the header height constant; a line that wrapped past the screen
 * width would grow the header, push the page over an 80x24 screen, and trip the
 * terminal's pause. Nothing played yet -> an empty row, as before. */
function syncretro_lobby_top_played(board)
{
	var fmt = syncretro_lobby_text("top_played_fmt");
	var row = syncretro_lobby_text("top_played");
	var vis = syncretro_lobby_gl.plain(row).length;
	var i, t, entry, w;

	if (!board.length)
		return "";
	for (i = 0; i < board.length && i < 5 && fmt !== ""; i++) {
		t     = syncretro_lobby_gl.clip(board[i].label || board[i].title, 14);
		entry = format(fmt, t, board[i].count);   /* no rank -- order shows it */
		w     = syncretro_lobby_gl.plain(entry).length;
		if (vis + w > console.screen_columns - 1)
			break;
		row += entry;
		vis += w;
	}
	return row;
}

/* Draw one page. Returns the rows the header and footer blocks actually took, so
 * the caller can correct a page geometry computed from the wrong guess: a sysop's
 * header/footer display file is of unknown height until it has been printed (a
 * line longer than the screen wraps to two rows, art may position the cursor
 * itself), and the file's own line count is only an estimate.
 *
 * Measured with console.row, the cursor row the terminal layer tracks, which
 * console.clear() homes to 0. NOT with console.line_counter: that one exists to
 * drive the pause prompt and deliberately does not count a blank line printed at
 * column 0 while it is still zero, so a header that opens with a blank line
 * would measure short -- and a geometry corrected to a short measurement is
 * worse than no correction at all. */
function syncretro_lobby_draw(page, pages, board, cols, per_col)
{
	var i, j, line, idx, rom, hrows, frows;

	console.clear();
	console.line_counter = 0;   /* we page ourselves (N/P); don't let the terminal
	                             * insert a more-prompt in the middle of a page */

	/* Header: the sysop's display file if there is one, else the title line.
	 * P_NOCRLF because printfile() otherwise prepends a line break whenever the
	 * cursor is not already at the top of the screen -- which would put a blank
	 * row above the footer file on top of the separator drawn here. */
	if (syncretro_lobby_header)
		console.printfile(syncretro_lobby_header, P_NOPAUSE | P_NOCRLF);
	else if (syncretro_lobby_text("title") !== "") {
		syncretro_lobby_print("title", [syncretro_lobby_con.name]);
		console.crlf();
	}
	if (syncretro_lobby_text("top_played") !== "") {
		console.putmsg(syncretro_lobby_top_played(board));
		console.crlf();
	}
	/* The cabinet line: only on a shared-saves console, where there is a
	 * choice to draw at all (see syncretro_lobby_private()). A guest gets the
	 * line with no "(C to switch)" hint -- the key does nothing for them,
	 * since userprops.js silently no-ops a guest's set(). */
	if (syncretro_lobby_con.shared_saves) {
		var cabinet_hint = user.is_guest ? ""
		    : format(syncretro_lobby_text("cabinet_hint"), SYNCRETRO_LOBBY_CABINET_KEY);
		syncretro_lobby_print(syncretro_lobby_private() ? "cabinet_private" : "cabinet_public",
		                      [cabinet_hint]);
		console.crlf();
	}
	console.crlf();
	hrows = console.row;

	for (i = 0; i < per_col; i++) {
		line = "";
		for (j = 0; j < cols; j++) {
			idx = i + j * per_col;
			if (idx >= pages[page].length)
				break;
			rom   = pages[page][idx];
			line += syncretro_cell(rom.num, rom, syncretro_lobby_cellw,
			                       syncretro_lobby_text("cell_fmt")) + " ";   /* fixed-width; no rpad */
		}
		if (line !== "")
			console.putmsg(line + "\r\n");
	}

	frows = console.row;
	console.crlf();
	if (syncretro_lobby_footer)
		console.printfile(syncretro_lobby_footer, P_NOPAUSE | P_NOCRLF);
	/* Condensed prompt, hotkeys in bright cyan: any number plays that cartridge,
	 * F searches, N/P page, Q quits. The unprompted aliases ('/' for F, '+'/'-'
	 * and Enter/PgUp/PgDn/Home/End for paging) are left off -- the row has to stay
	 * inside 80 columns, and every key it does name spells out its own word. No
	 * trailing CRLF -- the cursor rests on the bottom row, which is never scrolled,
	 * so the terminal never pauses. */
	syncretro_lobby_print("prompt", [page + 1, pages.length]);

	/* +1 for the prompt's own row (ending without a line break, it never advances
	 * the cursor off that row) and +1 kept empty so a full page never trips the
	 * terminal's more-prompt. */
	return { hrows: hrows, frows: console.row - frows + 2 };
}

/* Returns true when the caller needs to rescan (the picked cartridge is no
 * longer on disk), false otherwise. */
/* The command line Synchronet will actually carry, and the ceiling it has.
 *
 * xtrn.cpp assembles an external's command line into `fullcmdline[MAX_PATH + 1]`
 * and SAFECOPY-truncates it there. MAX_PATH is 4096 on *nix but 260 on Windows,
 * and it is a PATH limit standing in for a command-line limit, so a door whose
 * arguments are longer than any one path in them overruns it. Truncation is
 * silent, and the argument lost is the LAST one -- the ROM. Worse, the BBS logs
 * the string it MEANT to run, so the log shows a cartridge's full path while the
 * door reports it has no ROM.
 *
 * Checked here rather than trusted, because everything that shortened this line
 * (syncretro.ini, the drop file, the door-side title parse) can be undone by one
 * innocent-looking addition, and because a sysop with a longer ROM filename than
 * anything we ship will meet the ceiling before we do. A door that says why it
 * broke costs one comparison.
 *
 * v3.22's xtrn.cpp logs its own error when this happens; this one fires on
 * v3.21 too, names the cartridge, and reaches the sysop's log rather than
 * needing him to be watching the terminal server's. */
var SYNCRETRO_CMDLINE_MAX = 260;   /* xtrn.cpp fullcmdline[MAX_PATH + 1], Windows */

function syncretro_lobby_check_cmdlen(cmd, rom)
{
	/* What the BBS assembles, not what is written here: %-specifiers expand
	 * (%a -> the alias, %H -> the socket) and the startup directory is NOT
	 * prepended, because the binary path is absolute. bbs.cmdstr() does that
	 * expansion with this session's own values, so the length measured is the
	 * length that will be truncated. */
	var full = bbs.cmdstr(cmd);

	if (full.length <= SYNCRETRO_CMDLINE_MAX)
		return;
	log(LOG_ERR, "SyncRetro: command line is " + full.length + " characters, over the "
	    + SYNCRETRO_CMDLINE_MAX + "-character limit Synchronet truncates at on Windows -- "
	    + "the cartridge argument will be cut off. Cartridge: " + rom.name);
}

/* DOOR32.SYS, written into THIS NODE's directory for the door to find.
 *
 * The connection, the alias and the session time reached the door as -s%H /
 * -name %a / -t%T until this existed. The ALIAS is the reason it now exists: it
 * is user-controlled text of up to 25 characters on a command line Synchronet
 * truncates at 260 on Windows, so whose account launched a cartridge decided
 * whether that cartridge's ROM argument survived the trip. Carried in the drop
 * file, none of it costs the line anything, and the length of the line stops
 * varying per player.
 *
 * The door needs no argument to find this: it looks in $SBBSNODE, which
 * external() exports on both platforms before spawning us. Synchronet writes
 * this same file for a REGISTERED external (xtrn_sec.cpp); the lobby writes its
 * own because it spawns the door itself, and because line 1 has to describe how
 * THE DOOR was launched, which is not necessarily how the lobby was.
 *
 * Returns false if the file could not be written, and the caller then puts the
 * arguments back on the command line -- a longer line is still better than a
 * door that cannot reach the player. */
function syncretro_lobby_dropfile(stdio)
{
	var path = system.node_dir + "door32.sys";
	var f    = new File(path);

	if (!f.open("w")) {
		log(LOG_WARNING, "SyncRetro: cannot write " + path
		    + " -- passing the connection on the command line instead");
		return false;
	}
	/* Line order is the DOOR32.SYS standard (src/doors/termgfx/door32.h). Comm
	 * type 0 means "no socket -- the BBS redirected the door's stdin/stdout",
	 * which is exactly what EX_STDIO does, so this line alone selects the door's
	 * stdio path and -stdio becomes unnecessary. */
	f.writeln(stdio ? "0" : "2");                        /* 1  comm type */
	f.writeln(stdio ? "-1" : bbs.cmdstr("%H"));          /* 2  socket handle */
	f.writeln("0");                                      /* 3  baud: DOS-era, unused */
	f.writeln(system.version_notice);                    /* 4  BBS software */
	f.writeln(String(user.number));                      /* 5  user number */
	f.writeln(user.name);                                /* 6  real name */
	f.writeln(user.alias);                               /* 7  alias */
	f.writeln(String(user.security.level));              /* 8  security level */
	/* MINUTES -- the format has no finer unit. It rounds DOWN, so the door can
	 * only ever under-estimate the time left, never over-grant it. -t%T stays on
	 * the command line anyway (7 characters) and wins, so this is the value a
	 * door run without one would use. */
	f.writeln(String(Math.floor(bbs.time_left / 60)));   /* 9  minutes left */
	f.writeln("1");                                      /* 10 emulation: ANSI */
	f.writeln(String(bbs.node_num));                     /* 11 node number */
	f.close();
	return true;
}

/* The door's -home: its cwd sandbox, and the save directory the core is
 * handed. Per-user for a cartridge console; ONE directory for every player on
 * an arcade console, so the high-score table is the machine's and not each
 * player's own private copy of it.
 *
 * Both live under data_dir, not in the door's own xtrn dir: these are
 * generated run-time state, and the shared one is no less generated for being
 * shared (CLAUDE.md, "Directory hierarchy"). Also where snapshots live -- the
 * door writes <rom-stem>.<key8>.state into this same directory -- which is
 * why syncretro_lobby_mark_resumable() calls this too. */
function syncretro_lobby_home()
{
	return syncretro_lobby_con.shared_saves
	    ? syncretro_lobby_data_dir + "syncretro/" + syncretro_lobby_con.id + "/shared"
	    : syncretro_lobby_data_dir + "user/" + format("%04d", user.number) + "/" + syncretro_lobby_con.id;
}

/* True when nobody else's session can compete for this player's game on this
 * console. Every console that is not a shared cabinet already is one, per
 * player, by construction (syncretro_lobby_home() above). On a shared
 * cabinet (today only xtrn/syncarcade) it is this player's own choice: the
 * "cabinet" preference, one key per console (syncretro_cabinet_key(),
 * syncretro_lib.js), held in the stock per-user properties file through
 * userprops.js. Absent, unreadable, or a guest (userprops.js short-circuits
 * a guest's get() to the default, since a guest account is shared and a
 * "private" cabinet keyed to it would be private in name only) all resolve
 * to public -- the safe default: no snapshot is offered rather than one
 * that might roll back the shared high-score table. */
function syncretro_lobby_private()
{
	var v;

	if (!syncretro_lobby_con.shared_saves)
		return true;    /* already a private machine; nothing to choose */
	v = syncretro_lobby_up.get("syncretro", syncretro_cabinet_key(syncretro_lobby_con.id),
	                           syncretro_cabinet_default());
	return String(v).toLowerCase() === "private";
}

/* Flips this player's cabinet preference for the current console. A no-op
 * off a shared cabinet, where there is nothing to choose, and -- through
 * userprops.js's own UFLAG_G guard -- a no-op for a guest: its set() there
 * returns true without writing anything, so the toggle key harmlessly does
 * nothing rather than offering a preference that could never persist. */
function syncretro_lobby_cabinet_toggle()
{
	if (!syncretro_lobby_con.shared_saves)
		return;
	syncretro_lobby_up.set("syncretro", syncretro_cabinet_key(syncretro_lobby_con.id),
	                       syncretro_lobby_private() ? "public" : "private");
}

/* The sysop's install-wide kill switch -- "Turning it off disables
 * suspend/resume install-wide" (the design doc's own words for
 * [state] auto_resume). Shipped default is permitted (true).
 *
 * String() FIRST, then compare -- never `ini.state.auto_resume || "true"`.
 * Synchronet's iniGetObject() auto-types an ini value of "false" into the JS
 * BOOLEAN false, which is falsy, so that `||` would silently replace an
 * explicit `false` with the "true" fallback and the switch could never
 * actually turn suspend/resume off.
 *
 * Split out from syncretro_lobby_state_key() so a caller that must not run
 * AT ALL while the feature is off (syncretro_lobby_mark_resumable()'s sweep)
 * can ask the same question without going through a per-ROM, permission-
 * gated key. */
function syncretro_lobby_auto_resume()
{
	return String(syncretro_lobby_ini_cache.state.auto_resume).toLowerCase() !== "false";
}

/* The whole suspend/resume decision, in one place, expressed as the key the
 * door is handed -- or "" for "not permitted", which the caller turns into an
 * absent -state flag.
 *
 * Three things must all hold, and each is owned by whoever knows the answer:
 * the console/romset capability (ours), the sysop's auto_resume switch, and
 * this player's cabinet. The door is told the outcome and never infers it. */
function syncretro_lobby_state_key(rom)
{
	var ini = syncretro_lobby_ini_cache;
	var cap, per_rom;

	if (!syncretro_lobby_auto_resume())
		return "";
	/* No snapshots on a shared cabinet, ever: a restore there would roll back
	 * the machine's high-score table to whenever the snapshot was taken. */
	if (syncretro_lobby_con.shared_saves && !syncretro_lobby_private())
		return "";
	/* The capability claim: per-console default, per-romset override. Same
	 * `||`-is-a-trap reasoning as syncretro_lobby_auto_resume() above: an
	 * explicit `save_state = false` auto-types to the boolean false, and
	 * `false || "false"` happens to still read as "false" -- by luck, not by
	 * design -- so this is written the same direct way regardless. */
	cap = String(ini.console.save_state).toLowerCase() === "true";
	per_rom = syncretro_lobby_games_save_state(rom.name);   /* "", "true", "false" */
	if (per_rom !== "")
		cap = per_rom === "true";
	if (!cap)
		return "";

	return syncretro_state_key(syncretro_lobby_core_md5,
	                           rom.md5,
	                           syncretro_state_opts(ini.options));
}

/* The per-romset override, from games.ini with games.local.ini over the top --
 * the same two-file pair and the same precedence the picker's display titles
 * already use. Returns "" when neither file mentions save_state for this
 * romset, which leaves the [console] default in force. */
function syncretro_lobby_games_save_state(rom_name)
{
	var romset = String(rom_name).replace(/\.[^.]*$/, "").toLowerCase();
	var rows = syncretro_lobby_games;      /* already loaded for display titles */
	var v;

	if (!rows || !rows[romset] || rows[romset].save_state === undefined)
		return "";
	v = String(rows[romset].save_state).toLowerCase();
	return (v === "true" || v === "false") ? v : "";
}

/* Marks each ROM object `resumed` when a snapshot exists that would restore
 * with THIS core/rom/options combination right now, and -- only while the
 * feature is switched on for this console -- sweeps out every snapshot in
 * the same directory that can never be restored again (see
 * syncretro_state_sweep()).
 *
 * The sweep's own idea of "current key" must NOT be syncretro_lobby_state_key():
 * that answers "is a snapshot PERMITTED", which is policy (the sysop's
 * auto_resume/save_state, a games.local.ini override, the player's cabinet)
 * and not a fact about the snapshot. A snapshot is dead when the cartridge is
 * gone, or its core/ROM/options changed -- never merely because permission is
 * currently withheld. Keying the sweep on the permission-gated key would
 * delete every valid snapshot the instant a sysop flips a switch they might
 * flip right back -- so this uses the RAW derivation, syncretro_state_key(),
 * which knows nothing about permission at all.
 *
 * The sweep call itself is also skipped outright while syncretro_lobby_auto_resume()
 * is false: a disabled install performs no file operations on anyone's
 * snapshots, full stop, rather than relying solely on the raw key above.
 *
 * One directory() read (inside syncretro_state_list()) regardless of how many
 * cartridges are listed; every key derivation below is pure computation over
 * the already-cached ini and core hash -- no file I/O. */
function syncretro_lobby_mark_resumable(roms)
{
	var list = syncretro_state_list(syncretro_lobby_home());
	var raw_keys = {};
	var opts = syncretro_state_opts(syncretro_lobby_ini_cache.options);
	var i, r;

	for (i = 0; i < roms.length; i++) {
		r = roms[i];
		raw_keys[r.name] = syncretro_state_key(syncretro_lobby_core_md5, r.md5, opts);
		r.resumed = syncretro_state_marked(list, r.name, syncretro_lobby_state_key(r));
	}
	if (syncretro_lobby_auto_resume())
		syncretro_state_sweep(list, roms, raw_keys);
}

function syncretro_lobby_play(rom)
{
	var home = syncretro_lobby_home();
	var cmd, started, secs, label, drop, pass_title;

	if (!syncretro_quote_safe(rom.name)) {
		syncretro_lobby_print("unsafe_name");
		console.pause();
		return false;
	}
	if (!file_exists(rom.path)) {
		syncretro_lobby_print("rom_gone");
		console.pause();
		return true;
	}
	mkpath(home);

	/* Quoting is load-bearing on the ARGUMENTS: xtrn.cpp splits the command line
	 * on bare spaces, and every real cartridge name (and a user's -home path) has
	 * them, so those are quoted -- which also routes external() through the shell
	 * on *nix, reassembling the argument. But syncretro_lobby_binary (the leading program
	 * token) is deliberately NOT quoted; see syncretro_lobby_init(). -name %a, on
	 * the fallback path where the drop file could not be written, is NOT quoted
	 * either: cmdstr() already quotes the alias, and doubling it would hand the
	 * door literal quote characters. */
	/* The who's-online line -- "playing Astrosmash (Intellivision)", the way
	 * SyncDOOM names its WAD and map -- is the DOOR's to publish, from
	 * syncretro.ini and the cartridge's filename. Neither the title nor the console
	 * is sent to it any more, except for a curated title it could not derive.
	 *
	 * This label is for the line LOGGED below, and it applies the same rule the
	 * door applies to the same two syncretro.ini keys (SR_CONSOLE_LABEL_MAX in
	 * syncretro_config.c): the long name when it fits a status column, else the
	 * short one. "Intellivision" reads better than "Intv"; "Nintendo
	 * Entertainment System" is too wide. The two must agree, or a cartridge is
	 * logged under one console name and shown online under another. */
	label = syncretro_lobby_con.name.length <= 20 ? syncretro_lobby_con.name : syncretro_lobby_con.short;

	/* How the door reaches the player is in the DROP FILE now, not here -- comm
	 * type 2 with the socket handle, or comm type 0 for a STDIO door, which
	 * Synchronet forks on a pty and relays fd 0/1 for itself. EX_BIN is what
	 * makes that pty raw (cfmakeraw) and stops the LF->CRLF and CP437->UTF8
	 * translation that would otherwise mangle a sixel frame. */
	/* NO -option HERE, and it is a hard constraint rather than a preference: the
	 * BBS assembles this command line into xtrn.cpp's `fullcmdline[MAX_PATH + 1]`
	 * and truncates it there SILENTLY at 260 characters -- a PATH limit standing
	 * in for a command-line limit. Two pinned core options once pushed the line
	 * to 334 and the ROM argument -- the last thing on it -- was simply cut off,
	 * giving a door that reported "(no ROM)" for a file the BBS had just logged
	 * the full path of. Core options are pinned in the console's syncretro.ini
	 * [options] section instead; see retro_options.h.
	 *
	 * What is left on the line is what cannot be anywhere else: the binary, the
	 * session limits, the per-user sandbox and the cartridge. Everything the
	 * console knows about itself moved to syncretro.ini, everything about the
	 * player and his connection moved to the drop file, and the title the door
	 * can parse from the cartridge's own filename is no longer sent at all --
	 * between them, from ~335 characters to ~150. Anything added here from now
	 * on still has to buy its space from something else on the line, and
	 * syncretro_lobby_check_cmdlen() is what notices when it doesn't.
	 *
	 * Which is why the ROM is passed RELATIVE to the door's directory rather than
	 * as the absolute path discovery found it at: "roms\pacman.zip" instead of
	 * "s:\sbbs\xtrn\syncarcade\roms\pacman.zip" is 24 characters back, and the
	 * ROM argument is the one that gets cut when the line does overflow.
	 *
	 * Safe because the BBS starts the door IN this directory -- Synchronet hands
	 * startup_dir to CreateProcess as the working directory on Windows
	 * (xtrn.cpp) and chdir()s to it before exec on *nix -- and the door resolves
	 * a relative ROM against its cwd before it chdir()s into the per-user
	 * sandbox (syncretro_config.c). It is no new dependency either: the door
	 * already finds syncretro.ini, its core and its BIOS relative to that same
	 * cwd, so a door started anywhere else could not run at all.
	 *
	 * Built from rules.dir, not a hardcoded "roms", so a sysop who moves the ROM
	 * directory keeps working -- the lobby names whatever directory it actually
	 * scanned. (The DOOR's own bare-name fallback is hardcoded to roms/, so
	 * naming the directory here is what keeps the two in agreement.) */
	/* Idle timeout: seconds, or 0 when disabled or the user is exempt. The ARS
	 * is evaluated HERE because the door has no scfg_t and no user record --
	 * and -i0 is passed explicitly rather than omitting the flag, so "exempt"
	 * positively overrides any [idle] timeout in the door's own ini. "s" is the
	 * default unit so this agrees with xpdev's parse_duration() reading the
	 * same key on the door side. */
	var idle_cfg = syncretro_lobby_cfg.idle || {};
	var idle_ars = idle_cfg.exempt_ars || "EXEMPT H";
	var idle_secs;
	if (bbs.compare_ars(idle_ars))
		idle_secs = 0;                  /* exempt: positively excused */
	else if (idle_cfg.timeout === undefined || String(idle_cfg.timeout) === "")
		idle_secs = SYNCRETRO_IDLE_DEFAULT;   /* unconfigured: the shipped policy */
	else
		idle_secs = syncretro_lobby_gl.parse_duration(idle_cfg.timeout, "s");

	/* -core, -profile and -console are gone from the line: the door reads all
	 * three from syncretro.ini, which is shipped beside this lobby precisely so
	 * they need not be carried. -s%H / -name %a are gone into the drop file. */
	drop = syncretro_lobby_dropfile(syncretro_lobby_stdio);

	/* -title only when the door could not arrive at the same string itself.
	 * It can parse "Astrosmash" out of "Astrosmash (1981) (Mattel).int" -- that
	 * is sr_title_from_rom() in src/doors/syncretro/syncretro_title.c, and those
	 * are exactly the consoles whose filenames are long enough to overflow the
	 * line. It cannot invent "Pac-Man" from "puckman.zip", nor reproduce the
	 * publisher this lobby appends to tell two identical titles apart -- and
	 * those are the short filenames, with room to spare. */
	pass_title = syncretro_is_mapped(rom.name) || rom.label !== rom.title;

	/* "" (not permitted) or an 8-hex-digit key -- see syncretro_lobby_state_key().
	 * -state's ABSENCE is what tells the door no snapshot is wanted; it never
	 * infers that from -home. 15 characters at most ( -state 12345678 ). */
	var state_key = syncretro_lobby_state_key(rom);

	cmd = syncretro_lobby_binary
	    + (drop ? "" : (syncretro_lobby_stdio ? " -stdio" : " -s%H") + " -name %a")
	    + " -t%T -i" + idle_secs
	    + (pass_title ? ' -title "' + (rom.label || rom.title) + '"' : "")
	    + (state_key ? " -state " + state_key : "")
	    + ' -home "' + home + '" "'
	    + backslash(syncretro_lobby_rules.dir) + rom.name + '"';

	syncretro_lobby_check_cmdlen(cmd, rom);

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
	syncretro_log_play(syncretro_plays_path(syncretro_lobby_data_dir), {
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
 * where an install is reached over a network filesystem, a node pays that as one
 * round trip per ROM, every single time. With it, a warm run opens exactly one file. A
 * cold run still pays the full scan, so it says so rather than appearing to
 * hang -- on a big ROM set that first scan is genuinely slow. */
function syncretro_lobby_discover()
{
	var cache = syncretro_cache_open(syncretro_cache_path(syncretro_lobby_data_dir, syncretro_lobby_con.id));
	var cold  = !Object.keys(cache.entries).length;
	var roms;

	if (cold)
		syncretro_lobby_print("scanning");
	roms = syncretro_discover(syncretro_lobby_dir + backslash(syncretro_lobby_rules.dir), syncretro_lobby_rules, cache);
	syncretro_cache_flush(cache);
	if (cold)
		syncretro_lobby_print("scanned", [roms.length]);
	return roms;
}

function syncretro_lobby_number(roms)
{
	var i;

	for (i = 0; i < roms.length; i++)
		roms[i].num = i + 1;               /* alphabetical, and it never moves */
	return roms;
}

/* The whole of a console package's lobby.js is `syncretro_lobby()`: what the
 * console IS now lives in the shipped syncretro.ini beside it, read by this
 * function and by the door. `spec` remains only for a door run from an
 * unusual directory (`{dir: "/some/where/"}`). */
function syncretro_lobby(spec)
{
	var roms, plays, board, cols, per_col, per_page, pages, page, key, i, filter;
	var missing, drawn, regeom;

	syncretro_lobby_init(spec || {});

	missing = syncretro_lobby_bios_missing();
	if (missing.length) {
		syncretro_lobby_print("bios_missing",
		    [syncretro_lobby_con.name, syncretro_lobby_dir, missing.join(" and ")]);
		console.pause();
		return;
	}

	roms = syncretro_lobby_discover();
	if (!roms.length) {
		syncretro_lobby_print("no_roms", [syncretro_lobby_rules.dir]);
		console.pause();
		return;
	}
	syncretro_lobby_number(roms);
	syncretro_lobby_mark_resumable(roms);

	/* One-shot entry sound, if the sysop configured one -- the same helper and the
	 * same [lobby] enter_sound key SyncDuke and SyncDOOM use. Silent unless the
	 * terminal can decode audio files. After the guards above, so it never plays
	 * into an error message. */
	syncretro_lobby_gl.enter_sound(syncretro_lobby_dir, syncretro_lobby_cfg);

	filter = roms;
	page   = 0;
	regeom = 0;
	while (!js.terminated && bbs.online) {
		plays    = syncretro_read_plays(syncretro_plays_path(syncretro_lobby_data_dir), syncretro_lobby_con.id);
		board    = syncretro_top_played(plays, 5);
		cols     = syncretro_columns(console.screen_columns, syncretro_lobby_cellw);
		per_col  = syncretro_page_rows(console.screen_rows, syncretro_lobby_hrows, syncretro_lobby_frows);
		per_page = cols * per_col;
		pages    = syncretro_paginate(filter, per_page);
		if (page >= pages.length)
			page = pages.length ? pages.length - 1 : 0;
		if (!pages.length) {
			syncretro_lobby_print("no_match");
			console.pause();
			filter = roms;
			continue;
		}

		drawn = syncretro_lobby_draw(page, pages, board, cols, per_col);

		/* A display file turned out to be a different height than the page geometry
		 * above assumed, so that geometry was wrong: adopt what was measured and
		 * draw the page again before anyone is asked for a key. It happens once,
		 * on the first page of a session (the measurement then sticks), and is
		 * bounded rather than trusted to converge -- a redraw loop would be worse
		 * than a page one row short.
		 *
		 * ONLY when a display file is installed. Without one the block heights are
		 * the constants this lobby has always drawn to, arrived at by counting the
		 * lines it prints; there is nothing for a measurement to discover, and
		 * letting one overrule them would put every stock install at the mercy of
		 * this arithmetic for no gain. */
		if ((syncretro_lobby_header || syncretro_lobby_footer)
		    && (drawn.hrows != syncretro_lobby_hrows || drawn.frows != syncretro_lobby_frows)
		    && regeom < 2) {
			syncretro_lobby_hrows = drawn.hrows;
			syncretro_lobby_frows = drawn.frows;
			regeom++;
			continue;
		}
		regeom = 0;

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
		/* The cabinet toggle -- only where syncretro_lobby_draw() drew the line
		 * to explain it, and only for a player who could hold the preference.
		 * Re-marking is the reason for the redraw, not merely the toggle line:
		 * a private cabinet's snapshots are invisible from the public one and
		 * back, so the " *" marks on the grid change with the cabinet. */
		if (key === SYNCRETRO_LOBBY_CABINET_KEY && syncretro_lobby_con.shared_saves && !user.is_guest) {
			syncretro_lobby_cabinet_toggle();
			syncretro_lobby_mark_resumable(roms);
			continue;
		}
		/* 'F'ind is the prompted key -- it names itself the way Next/Prev/Quit do.
		 * '/' stays bound as the unprompted alias for the fingers that expect it. */
		if (key === "F" || key === "/") {
			syncretro_lobby_print("search");
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
						syncretro_lobby_print("no_roms", [syncretro_lobby_rules.dir]);
						console.pause();
						return;
					}
					syncretro_lobby_number(roms);
					filter = roms;
					page = 0;
				}
				/* Refresh the marks whether or not a rescan happened: the game
				 * just played may have written or updated its own snapshot, and
				 * this is one directory() read, not a rediscovery. */
				syncretro_lobby_mark_resumable(roms);
			}
			continue;
		}
	}
}
