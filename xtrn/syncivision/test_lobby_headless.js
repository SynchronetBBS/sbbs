// test_lobby_headless.js -- drive lobby.js under jsexec, with no terminal.
//
// lobby.js needs console/bbs/user, which jsexec does not provide. That is why
// the model layer lives in syncretro_lib.js. But "we cannot test the screen"
// is not the same as "we cannot execute the file": stub the three objects, feed
// a scripted key sequence, and capture the door command line it would run.
//
// This catches a parse error, a misnamed syncretro_* call, a wrong argument order, and
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
var logged = [];    // node-log lines the lobby wrote via bbs.logline()
var wrote = [];     // raw bytes written to the "terminal" -- the audio APCs land here
var probe = [];     // canned terminal reply, one char per inkey() (empty = silence)
var failures = 0;

// The last page the lobby drew ("Page 3 of 7" -> 3), or 0 if it drew none. The
// footer is redrawn every trip round the picker loop, so the LAST one is where
// the scripted keys left us. The Ctrl-A attribute codes have to come out first:
// the footer highlights the number, so the raw bytes read "Page \1h3\1n\1c of",
// and a /Page (\d+)/ against those never matches.
function last_page(text)
{
	var m = String(text).replace(/\x01./g, "").match(/Page (\d+) of/g);

	return m ? parseInt(m[m.length - 1].replace(/\D+/g, ""), 10) : 0;
}

function check(cond, msg)
{
	writeln((cond ? "  ok   " : "  FAIL ") + msg);
	if (!cond)
		failures++;
}

var fake_console = {
	screen_columns: 80,
	screen_rows: 24,
	// game_lobby.js's enter_sound loads cterm_lib.js, which talks to the terminal.
	// cterm_version >= 0 makes it skip its load-time DA query, so `probe` answers
	// only the libsndfile query we care about. With `probe` empty, inkey returns ""
	// at once, query_fb's read loop ends, and the terminal reports "no audio".
	cterm_version: 1330,
	write:   function (s) { wrote.push(String(s)); },
	inkey:   function () { return probe.length ? probe.shift() : ""; },
	clearkeybuffer: function () { },
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
	exec:   function (cmd, mode, dir) { launched.push(cmd); return 0; },
	logline: function (code, str) { logged.push(code + " " + str); return true; }
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

// The lobby loads the lib itself, but this harness needs syncretro_* too.
load("syncretro_lib.js");

var GUARD = syncretro_plays_path(system.data_dir);
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
// Assert the FOOTER, not the board: the "Top played" board only paints once plays
// exist, and this harness refuses to run unless the plays file is absent -- so on
// this first pass there is nothing for it to draw. (This check used to look for
// "Most played" or "[1-", neither of which the lobby has ever printed, so it could
// only ever fail.)
check(last_page(text) === 1, "painted the footer, on page 1 (saw " + last_page(text) + ")");
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

	// The leading program token is the door binary, and it is deliberately NOT
	// quoted -- the opposite of what -home needs. external() only treats the command
	// as an ABSOLUTE path when it starts with the drive letter (Windows) or '/'
	// (*nix); a leading quote defeats that test and the door would be looked up
	// under the startup dir instead. See syncretro_lobby_init().
	//
	// Assert the contract rather than a hard-coded path. The "<os>-<arch>" sub-dir
	// is OPTIONAL: never present on Windows (always flat), and on *nix only when
	// it is populated -- otherwise the binary sits at the door root. Either layout
	// is legal, so pinning the path to one of them is wrong whichever you pick.
	// (The old check pinned the flat path AND asserted the token was quoted, so it
	// could only ever fail -- on both counts.) NOTE: splitting on a space is sound
	// only because the path must not contain one -- the quoting that would make a
	// spaced path safe is the quoting external() forbids.
	// fake_bbs.cmdstr() does no %-expansion, so "%." is still literal here.
	var token = cmd.split(" ")[0];          // the leading program token
	var abs   = /^win/i.test(system.platform) ? /^[A-Za-z]:/ : /^\//;
	check(token.indexOf('"') < 0, "the binary token carries no quote (it would defeat external()'s absolute-path check)");
	check(abs.test(token), "the binary token is an absolute path (" + token + ")");
	check(/syncretro%\.$/.test(token), "the binary token is the door binary, with its %. specifier");
	check(/-home "[^"]*"/.test(cmd), "the -home path is quoted");
	check(cmd.indexOf('-name "%a"') < 0, "-name's %a is NOT double-quoted");
}

writeln("3. the play was logged");
LIVE_PLAYS = syncretro_plays_path(system.data_dir);
var plays = syncretro_read_plays(LIVE_PLAYS);
check(plays.length === 1, "one play recorded (saw " + plays.length + ")");
if (plays.length) {
	check(plays[0].user === 1, "records the user number");
	check(plays[0].alias === "Digital Man", "records the alias");
	check(/\.(int|bin|rom)$/i.test(plays[0].rom), "records the cartridge filename");
	check(typeof plays[0].secs === "number", "records a duration");
}

writeln("4. the play was named in the node log");
check(logged.length === 1, "one logline (saw " + logged.length + ")");
if (logged.length) {
	writeln("     log: " + logged[0]);
	// Same "X-" code the Terminal Server's own "Executing external program" line
	// uses, so one grep over node.log finds the lobby and what it launched.
	check(logged[0].indexOf("X- Playing ") === 0, "uses the X- code and says what is playing");
	check(logged[0].indexOf("(Intellivision)") > 0, "names the console");
}

writeln("5. search narrows the list without renumbering");
out = []; launched = [];
keys = ["/", "utopia", "Q"];
try {
	load(js.exec_dir + "lobby.js");
	check(true, "search ran without throwing");
} catch (e) {
	check(false, "search threw: " + e);
}
var t5 = out.join("");
check(t5.toLowerCase().indexOf("utopia") >= 0, "the filtered page shows Utopia");

writeln("6. F finds, the same as the '/' alias");
out = []; launched = [];
keys = ["F", "utopia", "Q"];
try {
	load(js.exec_dir + "lobby.js");
	check(true, "F search ran without throwing");
} catch (e) {
	check(false, "F search threw: " + e);
}
var t6 = out.join("");
check(t6.toLowerCase().indexOf("utopia") >= 0, "F filtered the page down to Utopia");
check(t6.indexOf("\1cF\1nind") >= 0, "the prompt names F, not '/'");

// '+'/'-' alias N/P. These assert against the drawn page number rather than a
// stub call, so they prove the keys reached the pager -- an unbound key would
// simply redraw the same page and leave the count unmoved.
writeln("7. '+' pages forward like N, '-' back like P");
out = []; launched = [];
keys = ["+", "Q"];
load(js.exec_dir + "lobby.js");
check(last_page(out.join("")) === 2, "'+' advanced to page 2 (saw "
      + last_page(out.join("")) + ")");

out = [];
keys = ["+", "+", "-", "Q"];
load(js.exec_dir + "lobby.js");
check(last_page(out.join("")) === 2, "'+','+','-' landed back on page 2 (saw "
      + last_page(out.join("")) + ")");

out = [];
keys = ["-", "Q"];
load(js.exec_dir + "lobby.js");
check(last_page(out.join("")) === 1, "'-' on page 1 stays on page 1 (saw "
      + last_page(out.join("")) + ")");

check(launched.length === 0, "no paging or search key launched a door");

// Drive game_lobby.js's helper directly, with our own cfg and a scratch file: the
// harness must not write a [lobby] key into the sysop's live syncretro.ini just to
// test itself. The stub console ANSWERS the libsndfile probe here, so
// supports_audio_files() is true and the sound is really uploaded and queued --
// this is the one check that fails if the lobby stops reaching the terminal.
writeln("8. a configured enter_sound reaches a terminal that decodes audio");
var snd_dir = backslash(system.temp_dir);
var snd_name = "syncretro_enter_test.wav";
var sf = new File(snd_dir + snd_name);
if (!sf.open("wb")) {
	check(false, "could not write the scratch sound " + snd_dir + snd_name);
} else {
	sf.write("RIFF$\x00\x00\x00WAVEfmt not-a-real-wav");   // enter_sound only reads bytes
	sf.close();
	wrote = [];
	probe = "\x1b[=7;100;1n\x1b[1;1R".split("");   // "audio APC + libsndfile", then the DSR sentinel
	var gl8 = load({}, "game_lobby.js");
	gl8.enter_sound(snd_dir, { lobby: { enter_sound: snd_name } });
	var w8 = wrote.join("");
	check(w8.indexOf("C;S;lobby_enter;") >= 0, "uploaded the sound under the lobby_enter name");
	check(w8.indexOf("A;Load") >= 0, "loaded it into a slot");
	check(w8.indexOf("A;Queue") >= 0, "queued it on a channel");
	file_remove(snd_dir + snd_name);
}

writeln("9. no sound reaches a terminal that cannot decode audio");
wrote = [];
probe = [];                                        // silence: the probe times out -> no audio
var gl9 = load({}, "game_lobby.js");
gl9.enter_sound(snd_dir, { lobby: { enter_sound: snd_name } });
check(wrote.join("").indexOf("A;Queue") < 0, "queued nothing");

if (LIVE_PLAYS !== "" && file_exists(LIVE_PLAYS))
	file_remove(LIVE_PLAYS);           /* leave the live data dir as we found it */
check(!file_exists(LIVE_PLAYS), "harness left no play record behind");
writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
