// test_lobby_headless.js -- drive lobby.js under jsexec, with no terminal.
//
// lobby.js needs console/bbs/user, which jsexec does not provide. That is why
// the model layer lives in syncivision_lib.js. But "we cannot test the screen"
// is not the same as "we cannot execute the file": stub the three objects, feed
// a scripted key sequence, and capture the door command line it would run.
//
// This catches a parse error, a misnamed sv_* call, a wrong argument order, and
// an unquoted ROM path -- every failure mode that would otherwise first appear
// when a player connects.
//
// SAFETY: the lobby writes a play record, and `system` is a PERMANENT global in
// jsexec -- assigning `this.system` does NOT shadow it -- so the record lands in
// the REAL data_dir. This script therefore REFUSES to run if a plays.jsonl
// already exists, rather than risk deleting a sysop's activity log.
//
// Run:  jsexec /sbbs/xtrn/syncivision/test_lobby_headless.js
//       (must run from the INSTALL dir: it needs exec.bin, roms/ and the ini)

var out = [];       // everything the lobby "printed"
var keys = [];      // scripted keystrokes, consumed front-to-back
var launched = [];  // door command lines bbs.exec() was asked to run
var failures = 0;

function check(cond, msg)
{
	writeln((cond ? "  ok   " : "  FAIL ") + msg);
	if (!cond)
		failures++;
}

var fake_console = {
	screen_columns: 80,
	screen_rows: 24,
	clear:   function () { },
	crlf:    function () { out.push("\n"); },
	putmsg:  function (s) { out.push(String(s)); },
	pause:   function () { },
	getkey:  function () { return keys.length ? keys.shift() : "Q"; },
	getstr:  function () { return keys.length ? keys.shift() : ""; },
	getnum:  function () { return keys.length ? parseInt(keys.shift(), 10) : 0; },
	ungetstr: function () { }
};

var fake_bbs = {
	online: true,       // the picker loop's `while (... && bbs.online)` guard
	cmdstr: function (s) { return s; },     // no %-expansion needed for this check
	exec:   function (cmd, mode, dir) { launched.push(cmd); return 0; }
};

var fake_user = { number: 1, alias: "Digital Man" };

// `system` is a PERMANENT global in jsexec: assigning this.system does not
// shadow it, so the lobby writes its play record to the REAL data_dir. We assert
// against that path and delete the file afterwards. (Discovered by this harness:
// the first version asserted against a temp dir and saw zero plays.)
var LIVE_PLAYS = "";

// load(scope, file) does NOT inject these into the loaded script's scope --
// lobby.js resolves `console` against the GLOBAL object. So install them there.
this.console = fake_console;
this.bbs     = fake_bbs;
this.user    = fake_user;

// The lobby loads the lib itself, but this harness needs sv_* too.
load(js.exec_dir + "syncivision_lib.js");

var GUARD = sv_plays_path(system.data_dir);
if (file_exists(GUARD)) {
	writeln("REFUSING: " + GUARD + " already exists.");
	writeln("This test appends a play record and removes the file afterwards.");
	writeln("Move the real log aside first if you mean to run it.");
	exit(2);
}

writeln("1. lobby.js parses and runs to a clean quit");
keys = ["Q"];
try {
	load(js.exec_dir + "lobby.js");
	check(true, "loaded and returned without throwing");
} catch (e) {
	check(false, "threw: " + e);
}
var text = out.join("");
check(text.indexOf("SyncRetro") >= 0, "painted its title");
check(text.indexOf("Most played") >= 0 || text.indexOf("[1-") >= 0, "painted a board or a footer");
check(launched.length === 0, "quitting launched no door");

writeln("2. picking entry 1 launches the door with a QUOTED rom path");
out = []; launched = [];
keys = ["1", "1", "Q"];        // a digit, then getnum() returns 1, then quit
try {
	load(js.exec_dir + "lobby.js");
} catch (e) {
	check(false, "threw while launching: " + e);
}
check(launched.length === 1, "exactly one door launch (saw " + launched.length + ")");
if (launched.length) {
	var cmd = launched[0];
	writeln("     cmd: " + cmd);
	check(cmd.indexOf('"') >= 0, "the rom path is quoted");
	check(/"[^"]*\.(int|bin|rom)"$/i.test(cmd), "the quoted argument is the cartridge, and is last");
	check(cmd.indexOf("-core freeintv_libretro.so") >= 0, "names the core");
	check(cmd.indexOf("-s%H") >= 0 && cmd.indexOf("-t%T") >= 0, "passes socket and time");
	check(cmd.indexOf("-name %a") >= 0, "passes the alias (which cmdstr quotes)");
	check(cmd.indexOf("-home ") >= 0, "passes a per-user home");

	// The binary path and the -home path must also be quoted: a space in
	// js.exec_dir or system.data_dir (e.g. a future Windows "Program Files"
	// install) would otherwise split the command line into extra shell words.
	// fake_bbs.cmdstr() does no %-expansion, so the "%." specifier is still
	// literal here -- the quotes must wrap it whole regardless.
	var binary = js.exec_dir + "syncretro%.";
	check(cmd.indexOf('"' + binary + '"') >= 0,
	      "the binary path is quoted whole, including the %. specifier");
	check(/-home "[^"]*"/.test(cmd), "the -home path is quoted");
	check(cmd.indexOf('-name "%a"') < 0, "-name's %a is NOT double-quoted");
}

writeln("3. the play was logged");
LIVE_PLAYS = sv_plays_path(system.data_dir);
var plays = sv_read_plays(LIVE_PLAYS);
check(plays.length === 1, "one play recorded (saw " + plays.length + ")");
if (plays.length) {
	check(plays[0].user === 1, "records the user number");
	check(plays[0].alias === "Digital Man", "records the alias");
	check(/\.(int|bin|rom)$/i.test(plays[0].rom), "records the cartridge filename");
	check(typeof plays[0].secs === "number", "records a duration");
}

writeln("4. search narrows the list without renumbering");
out = []; launched = [];
keys = ["/", "utopia", "Q"];
try {
	load(js.exec_dir + "lobby.js");
	check(true, "search ran without throwing");
} catch (e) {
	check(false, "search threw: " + e);
}
var t4 = out.join("");
check(t4.toLowerCase().indexOf("utopia") >= 0, "the filtered page shows Utopia");

if (LIVE_PLAYS !== "" && file_exists(LIVE_PLAYS))
	file_remove(LIVE_PLAYS);           /* leave the live data dir as we found it */
check(!file_exists(LIVE_PLAYS), "harness left no play record behind");
writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
