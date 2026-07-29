// test_syncretro_lobby.js -- jsexec tests for the lobby's configurable strings
// and its optional header/footer display files.
//
// Run:  /sbbs/exec/jsexec /home/rswindell/sbbs/xtrn/syncivision/tests/test_syncretro_lobby.js
// Exits non-zero on any failure. SpiderMonkey 1.8.5: no let/const/arrows.
//
// The lobby is the half of SyncRetro that talks to the terminal, so it normally
// cannot be tested headlessly at all. Two things make these tests possible:
//
//   * syncretro_lobby_init() reads syncretro.ini and resolves the display files
//     without touching console/bbs/user, so the whole configuration path runs
//     under jsexec against a scratch door directory.
//   * the DRAW is exercised against a console stub defined below. What is being
//     tested there is the row arithmetic -- that the header and footer blocks
//     report the height they actually printed, which is what the page geometry
//     is computed from -- not the terminal's own rendering.

load("syncretro_lobby.js");     /* pulls in sbbsdefs/key_defs/syncretro_lib/game_lobby */

var failures = 0;

function check(cond, msg)
{
	writeln((cond ? "  ok   " : "  FAIL ") + msg);
	if (!cond)
		failures++;
}

function eq(got, want, msg)
{
	check(got === want, msg + "  (got [" + got + "] want [" + want + "])");
}

// --- scratch door directory -------------------------------------------------

var dir = system.temp_dir + "syncretro_lobby_test/";
var i;

function reset_dir()
{
	if (file_isdir(dir)) {
		var old = directory(dir + "*");
		for (i = 0; i < old.length; i++)
			file_remove(old[i]);
	}
	mkpath(dir);
}

function write_file(name, text)
{
	var f = new File(dir + name);
	if (!f.open("w"))
		throw new Error("cannot write " + dir + name);
	f.write(text);
	f.close();
}

// The console's identity, exactly as a console install's lobby.js declares it.
var SPEC = {
	dir:   dir,
	name:  "Intellivision",
	short: "Intv",
	core:  "freeintv_libretro",
	ext:   ["int", "bin"]
};

// --- console stub -----------------------------------------------------------
//
// Enough of the Terminal Server's console object for the draw: everything it
// prints lands in `out`, and `row` advances per line feed exactly as the
// terminal layer's inc_row() does. `row` is what the draw measures the header
// and footer heights with, and console.clear() homes it, as the real clear()
// does by way of the form-feed it sends.
//
// line_counter is here only because the draw resets it (its job is the terminal's
// pause prompt); nothing is measured with it. It is NOT modelled: the real one
// declines to count a blank line printed at column 0 while it is still zero,
// which is exactly why the draw does not use it.
var out = "";
var console = {
	screen_columns: 80,
	screen_rows:    24,
	line_counter:   0,
	row:            0,
	ansi:           false,      /* what this stubbed terminal claims to support */
	color:          true,
	rip:            false,
	charset:        "CP437",
	term_supports: function (flag) {
		if (flag == USER_ANSI)  return this.ansi;
		if (flag == USER_COLOR) return this.color;
		if (flag == USER_RIP)   return this.rip;
		return false;
	},
	write_: function (s) {
		out += s;
		for (var n = 0; n < s.length; n++)
			if (s.charAt(n) == "\n")
				this.row++;
	},
	clear:    function () { out = ""; this.line_counter = 0; this.row = 0; },
	crlf:     function () { this.write_("\r\n"); },
	putmsg:   function (s) { this.write_(s); },
	/* The lobby passes P_NOCRLF, so this never prepends a line break of its own. */
	printfile: function (path, mode) {
		var f = new File(path);
		if (!f.open("r"))
			return;
		if (!(mode & P_NOCRLF) && this.row > 0)
			this.crlf();
		this.write_(f.read());
		f.close();
	}
};

function plain(s) { return s.replace(/\x01./g, ""); }

writeln("1. defaults -- an install with no [text] section");

reset_dir();
syncretro_lobby_init(SPEC);

eq(syncretro_lobby_text("title"), "\1h\1cSyncRetro \1n\1c-- %s\1n", "title falls back to the shipped default");
eq(syncretro_lobby_cellw, 38, "cell width falls back to the shipped default");
eq(syncretro_lobby_header, "", "no header file installed");
eq(syncretro_lobby_footer, "", "no footer file installed");
eq(syncretro_lobby_hrows, 3, "header block is the historic 3 rows");
eq(syncretro_lobby_frows, 3, "footer block is the historic 3 rows");

writeln("2. [text] overrides, written with the ini's ':' string-literal separator");

reset_dir();
write_file("syncretro.ini",
    "[text]\n"
  + "title : \\1h\\1rMY ARCADE \\1n-- %s\n"
  + "search : \"Find what? \"\n"
  + "cell_fmt : \\1w%3u. \\1n%s\n"
  + "[lobby]\n"
  + "cell_width = 30\n");
syncretro_lobby_init(SPEC);

eq(syncretro_lobby_text("title"), "\x01h\x01rMY ARCADE \x01n-- %s",
   "'\\1h' becomes a Ctrl-A attribute code, as in text.dat");
eq(syncretro_lobby_text("search"), "Find what? ",
   "quotes are stripped and the TRAILING SPACE survives");
eq(syncretro_lobby_text("no_match"), "\r\n\1hNothing matches. \1n",
   "an unlisted key keeps its default");
eq(syncretro_lobby_cellw, 30, "[lobby] cell_width is honored");
eq(plain(syncretro_cell(7, {title: "Astrosmash"}, syncretro_lobby_cellw,
                        syncretro_lobby_text("cell_fmt"))).length, 30,
   "the configured cell format fills the configured width");

writeln("3. a blank value means 'draw nothing', and is not the same as absent");

reset_dir();
write_file("syncretro.ini", "[text]\ntop_played :\n");
syncretro_lobby_init(SPEC);

eq(syncretro_lobby_text("top_played"), "", "a blank key overrides the default");
eq(syncretro_lobby_hrows, 2, "suppressing the Top played row shortens the header");

writeln("4. a cell_fmt that lost its %s is refused, not obeyed");

reset_dir();
write_file("syncretro.ini", "[text]\ncell_fmt : \\1c%3u only\n");
syncretro_lobby_init(SPEC);
eq(syncretro_lobby_text("cell_fmt"), SYNCRETRO_CELL_FMT,
   "a title-less cell format falls back to the built-in one");

writeln("5. display files -- auto-detected, named, or turned off");

reset_dir();
write_file("lobby_header.asc", "\1h\1cSYNCRETRO\1n\r\n---------\r\n");
write_file("house.msg", "one line\r\n");
syncretro_lobby_init(SPEC);

eq(syncretro_lobby_header, dir + "lobby_header.asc", "lobby_header.* is auto-detected");
eq(syncretro_lobby_footer, "", "no footer file, and none is invented");
eq(syncretro_lobby_hrows, 4, "a 2-row header replaces the 1-row title line");

reset_dir();
write_file("lobby_header.asc", "x\r\n");
write_file("house.msg", "one line\r\n");
write_file("syncretro.ini", "[lobby]\nheader =\nfooter = house\n");
syncretro_lobby_init(SPEC);

eq(syncretro_lobby_header, "", "a BLANK header key turns the auto-detected file off");
eq(syncretro_lobby_footer, dir + "house.msg", "a named footer resolves its own extension");
eq(syncretro_lobby_hrows, 3, "with the header file off, the title line is back");
eq(syncretro_lobby_frows, 4, "the footer file's row is added to the footer block");

// An explicit extension is honored as given; a missing file resolves to "".
var gl = load({}, "game_lobby.js");
eq(gl.display_file(dir, "house.msg"), dir + "house.msg", "an explicit extension is used as given");
eq(gl.display_file(dir, "house.ans"), "", "an explicit extension that is not installed finds nothing");
eq(gl.display_file(dir, "nosuchfile"), "", "an uninstalled display file resolves to nothing");
eq(gl.display_file(dir, ""), "", "no name, no file");
eq(gl.display_file_rows(dir + "nosuchfile.asc"), 0, "a missing file occupies no rows");

// The extension follows the terminal, in the order Synchronet's own menu file
// lookup uses.
write_file("lobby_header.ans", "\x1b[1;36mSYNCRETRO\x1b[0m\r\n");
write_file("lobby_header.msg", "\1h\1cSYNCRETRO\1n \xb3 CP437\r\n");
write_file("lobby_header.rip", "!|1RIP\r\n");
write_file("lobby_header.mon", "monochrome\r\n");

eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.msg",
   "a CP437 terminal prefers the .msg over the .asc");
console.charset = "UTF-8";
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.msg",
   "so does a UTF-8 terminal");
// The one case that reverses: .msg may carry CP437 box-drawing characters a
// US-ASCII terminal asked not to be sent.
console.charset = "US-ASCII";
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.asc",
   "an ASCII-only terminal prefers the .asc over the .msg");
file_remove(dir + "lobby_header.asc");
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.msg",
   "...but still takes the .msg when that is all there is");
write_file("lobby_header.asc", "plain\r\n");
console.charset = "CP437";

console.ansi = true;
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.ans",
   "an ANSI terminal prefers the .ans");
console.color = false;
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.mon",
   "an ANSI terminal with no color prefers the .mon");
console.color = true;
console.rip = true;
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.rip",
   "a RIP terminal outranks them all");
console.rip = false;
console.ansi = false;

// PETSCII sits between the ANSI files and the msg/asc pair.
write_file("lobby_header.seq", "petscii\r\n");
console.charset = "CBM-ASCII";
eq(gl.display_file(dir, "lobby_header"), dir + "lobby_header.seq",
   "a Commodore terminal gets the .seq");
console.charset = "CP437";
file_remove(dir + "lobby_header.seq");
file_remove(dir + "lobby_header.rip");
file_remove(dir + "lobby_header.mon");
file_remove(dir + "lobby_header.ans");
file_remove(dir + "lobby_header.msg");

writeln("6. the draw reports the rows it actually printed");

var ROMS = [];
for (i = 1; i <= 12; i++)
	ROMS.push({num: i, name: "g" + i + ".int", title: "Game " + i, year: 0});

function draw_page(board)
{
	var cols    = syncretro_columns(console.screen_columns, syncretro_lobby_cellw);
	var per_col = syncretro_page_rows(console.screen_rows, syncretro_lobby_hrows, syncretro_lobby_frows);
	var pages   = syncretro_paginate(ROMS, cols * per_col);
	return syncretro_lobby_draw(0, pages, board, cols, per_col);
}

reset_dir();
syncretro_lobby_init(SPEC);          /* stock: title + Top played + blank */
var drawn = draw_page([]);
eq(drawn.hrows, 3, "stock header measures the 3 rows it printed");
eq(drawn.frows, 3, "stock footer measures the 3 rows it accounts for");
check(out.indexOf("SyncRetro") >= 0, "the stock title was drawn");
check(out.indexOf("Game 1") >= 0, "the cartridge grid was drawn");
check(out.indexOf("Page 1 of") >= 0 || plain(out).indexOf("Page 1 of") >= 0, "the prompt was drawn");

// The estimate and the measurement must agree for a plain sequential header --
// that agreement is what keeps the ordinary install from paying a corrective
// redraw on its first page.
reset_dir();
write_file("lobby_header.asc", "line one\r\nline two\r\nline three\r\n");
write_file("lobby_footer.asc", "footer line\r\n");
syncretro_lobby_init(SPEC);
var est_h = syncretro_lobby_hrows, est_f = syncretro_lobby_frows;
drawn = draw_page([]);
eq(drawn.hrows, est_h, "the header file's estimated height is what it actually prints");
eq(drawn.frows, est_f, "the footer file's estimated height is what it actually prints");
eq(drawn.hrows, 5, "3-row header file + Top played + blank");
eq(drawn.frows, 4, "blank + footer file + prompt + the spare row");
check(out.indexOf("line three") >= 0, "the header file's content was drawn");
check(out.indexOf("footer line") >= 0, "the footer file's content was drawn");
check(out.indexOf("SyncRetro") < 0, "the header file REPLACED the built-in title line");

// A header that OPENS WITH A BLANK LINE is the case that rules out measuring
// with console.line_counter: the real one refuses to count a blank line printed
// at column 0 while it is still zero, so that header would measure short and the
// page would be sized as if it had more room than it does.
reset_dir();
write_file("lobby_header.asc", "\r\n\1h\1cSYNCRETRO\1n\r\n\r\n");
syncretro_lobby_init(SPEC);
drawn = draw_page([]);
eq(drawn.hrows, 5, "a header opening with a blank line measures all of its rows");

// Nothing configured for the header at all: no file, and a blanked title.
reset_dir();
write_file("syncretro.ini", "[text]\ntitle :\n");
syncretro_lobby_init(SPEC);
eq(syncretro_lobby_hrows, 2, "a blanked title shortens the header by its row");
drawn = draw_page([]);
eq(drawn.hrows, 2, "and that is what the draw prints");
check(out.indexOf("SyncRetro") < 0, "the title really is gone");

// The Top played row stays exactly one line however long the board is.
reset_dir();
syncretro_lobby_init(SPEC);
var board = [];
for (i = 0; i < 5; i++)
	board.push({title: "A Very Long Cartridge Title " + i, count: 999});
drawn = draw_page(board);
eq(drawn.hrows, 3, "a full Top played board still occupies one row");
var row = plain(out.split("\r\n")[1]);
check(row.length < console.screen_columns, "the Top played row fits the screen width  (" + row.length + " cols)");

// --- games.local.ini, the sysop's overlay -----------------------------------
//
// games.ini is shipped, so anything a sysop writes into it is lost to the next
// pull. games.local.ini holds only what differs and is read over the top, which
// is what lets a retitled cabinet survive an upgrade WITHOUT freezing the rest
// of the file: the titles upstream adds keep arriving underneath.

writeln("games.local.ini overlay");

reset_dir();
write_file("games.ini", "[puckman]\nname = Pac-Man (Japan)\n\n"
	+ "[bzone]\nname = Battlezone\n");
syncretro_lobby_init(SPEC);
eq(syncretro_parse_title("puckman.zip").title, "Pac-Man (Japan)",
	"the shipped file titles a cabinet on its own");

reset_dir();
write_file("games.ini", "[puckman]\nname = Pac-Man (Japan)\n\n"
	+ "[bzone]\nname = Battlezone\n");
write_file("games.local.ini", "[puckman]\nname = PAC-MAN!\n\n"
	+ "[myrom]\nname = House Cabinet\n");
syncretro_lobby_init(SPEC);
eq(syncretro_parse_title("puckman.zip").title, "PAC-MAN!",
	"the sysop's title wins over the shipped one");
eq(syncretro_parse_title("bzone.zip").title, "Battlezone",
	"a romset the sysop did not touch keeps the shipped title");
eq(syncretro_parse_title("myrom.zip").title, "House Cabinet",
	"the sysop can title a romset the shipped file has never heard of");

// The local file alone is a working install: a sysop who deleted or never
// received games.ini still gets their own titles.
reset_dir();
write_file("games.local.ini", "[myrom]\nname = House Cabinet\n");
syncretro_lobby_init(SPEC);
eq(syncretro_parse_title("myrom.zip").title, "House Cabinet",
	"games.local.ini works with no games.ini beside it");

reset_dir();
if (file_isdir(dir))
	rmdir(dir);

writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
