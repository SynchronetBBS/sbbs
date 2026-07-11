// test_syncretro_lib.js -- jsexec tests for the UI-free half of the lobby.
//
// Run:  /sbbs/exec/jsexec /home/rswindell/sbbs/xtrn/syncivision/tests/test_syncretro_lib.js
// Exits non-zero on any failure. SpiderMonkey 1.8.5: no let/const/arrows.

load("syncretro_lib.js");

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

writeln("1. title parsing");
var p;

p = sv_parse_title("Astrosmash (1981) (Mattel).int");
eq(p.title, "Astrosmash", "plain title");
eq(p.year, 1981, "plain year");
eq(p.publisher, "Mattel", "plain publisher");

// The dump marker is not part of the publisher.
p = sv_parse_title("Atlantis (1981) (Imagic) [!].int");
eq(p.title, "Atlantis", "title before [!]");
eq(p.year, 1981, "year with [!]");
eq(p.publisher, "Imagic", "publisher without [!]");

// Nested parens in the title must not be mistaken for the year group.
p = sv_parse_title("Adventure (AD&D - Cloudy Mountain) (1982) (Mattel).int");
eq(p.title, "Adventure (AD&D - Cloudy Mountain)", "title keeps its own parens");
eq(p.year, 1982, "year is the LAST 4-digit group");
eq(p.publisher, "Mattel", "publisher is the last group");

// An ampersand is ordinary text.
p = sv_parse_title("Advanced Dungeons & Dragons (1982) (Mattel).int");
eq(p.title, "Advanced Dungeons & Dragons", "ampersand survives");

// Parenthetical that is not a year stays in the title.
p = sv_parse_title("Armor Battle (USA, Europe) (Fast Tanks).int");
eq(p.year, 0, "no year found");
eq(p.title, "Armor Battle (USA, Europe) (Fast Tanks)", "unparsed name kept whole");

// A bare name has no year and no publisher.
p = sv_parse_title("4tris.int");
eq(p.title, "4tris", "bare name");
eq(p.year, 0, "bare name: no year");
eq(p.publisher, "", "bare name: no publisher");

// Extension is stripped case-insensitively, and only the last one.
eq(sv_parse_title("Utopia (1981) (Mattel).INT").title, "Utopia", "uppercase extension");
eq(sv_parse_title("Some.Game.Name.bin").title, "Some.Game.Name", "only last extension");

// Stacked dump markers, as the real set carries them.
p = sv_parse_title("League of Light (Prototype) (1983) (Activision) [a1][!].int");
eq(p.title, "League of Light (Prototype)", "stacked markers stripped");
eq(p.year, 1983, "stacked markers: year still found");
eq(p.publisher, "Activision", "stacked markers: publisher still found");

// A year RANGE yields its first year, not a fallback.
p = sv_parse_title("Bump 'N' Jump (1982-83) (Mattel).int");
eq(p.title, "Bump 'N' Jump", "year range: title");
eq(p.year, 1982, "year range: first year taken");
eq(p.publisher, "Mattel", "year range: publisher");

p = sv_parse_title("River Raid (1982-83) (Activision) [!].int");
eq(p.year, 1982, "year range plus marker");
eq(p.publisher, "Activision", "year range plus marker: publisher");

// A four-digit year with no range still works, and a bare parenthetical does not
// become a year.
p = sv_parse_title("Armor Battle (USA, Europe) (Fast Tanks).int");
eq(p.year, 0, "still no year where there is none");

writeln("2. quote safety");
check(sv_quote_safe("Astrosmash (1981) (Mattel).int"), "ordinary name is safe");
check(sv_quote_safe("Advanced Dungeons & Dragons (1982) (Mattel).int"), "ampersand is safe");
check(!sv_quote_safe('He said "hi".int'), "double quote is unsafe");
check(!sv_quote_safe("cost $5.int"), "dollar is unsafe");
check(!sv_quote_safe("back\\slash.int"), "backslash is unsafe");

writeln("3. md5 of a file's head");
// Build a scratch tree: two identical files under different names (the symlink
// case, which over SMB looks like two regular files), one BIOS image, one short
// file, one oversized file, one non-ROM extension.
var dir = "/tmp/sv_disco/";
var f;
if (file_isdir(dir))
	directory(dir + "*").forEach(function (p) { file_remove(p); });
mkpath(dir);

function put(name, bytes, fill)
{
	var g = new File(dir + name);
	var s = "";
	var i;

	g.open("wb");
	for (i = 0; i < bytes; i++)
		s += fill;
	g.write(s);
	g.close();
}

put("Astrosmash (1981) (Mattel).int", 8192, "A");
put("astrosmash.int", 8192, "A");                 // same content, short alias
put("Utopia (1981) (Mattel).int", 8192, "U");
put("tiny.int", 100, "t");                         // under 2 KB
put("huge.int", 70000, "h");                       // over 64 KB
put("notes.txt", 8192, "n");                       // wrong extension
put("Boring Game (1983) (Acme).bin", 4096, "b");

var md_a = sv_file_md5(dir + "Astrosmash (1981) (Mattel).int", 4096);
var md_b = sv_file_md5(dir + "astrosmash.int", 4096);
eq(md_a, md_b, "identical content hashes identically");
check(md_a.length === 32, "md5 is 32 hex chars, not base64");
check(/^[0-9a-f]+$/.test(md_a), "md5 is lowercase hex");
eq(sv_file_md5(dir + "does-not-exist.int", 4096), "", "missing file yields empty md5");

writeln("4. discovery");
// The Intellivision's rules, exactly as xtrn/syncivision/lobby.js declares them.
var INTV = sv_rules({
	ext:        ["int", "bin", "rom"],
	min_size:   2 * 1024,
	max_size:   64 * 1024,
	bios_names: ["exec.bin", "grom.bin"],
	bios_md5:   ["62e761035cb657903761800f4437b8af",
	             "0cd5946c6473e42e8e4c2137785e427f"]
});
var roms = sv_discover(dir, INTV);
var names = roms.map(function (r) { return r.name; });

check(names.indexOf("notes.txt") < 0, "wrong extension excluded");
check(names.indexOf("tiny.int") < 0, "under 2 KB excluded");
check(names.indexOf("huge.int") < 0, "over 64 KB excluded");
check(names.indexOf("Boring Game (1983) (Acme).bin") >= 0, ".bin included");

// Dedupe keeps the more descriptive name.
check(names.indexOf("Astrosmash (1981) (Mattel).int") >= 0, "dedupe keeps the long name");
check(names.indexOf("astrosmash.int") < 0, "dedupe drops the short alias");

// The real 221-cartridge set has two Pac-Man ports, by different publishers,
// that happen to share their first 4 KB and only differ later in the file:
// "Pac-Man (1983) (Atarisoft).int" and "Pac-Man (1983) (Intv Corp).int"
// (both 24576 bytes, head4k=54401dd74a, differing full-file hashes). A dedupe
// key built from a 4 KB prefix merges them and silently drops a real game;
// keying on the full file must not. put() cannot make same-prefix/
// different-tail files, so build them by hand: 4096 identical bytes, then a
// differing tail, landing both in the 2 KB-64 KB window.
function put_prefix_tail(name, prefix_fill, tail_fill, tail_len)
{
	var g = new File(dir + name);
	var prefix = "";
	var tail = "";
	var i;

	for (i = 0; i < 4096; i++)
		prefix += prefix_fill;
	for (i = 0; i < tail_len; i++)
		tail += tail_fill;
	g.open("wb");
	g.write(prefix);
	g.write(tail);
	g.close();
}

put_prefix_tail("Pac-Man (1983) (Atarisoft).int", "P", "1", 4096);
put_prefix_tail("Pac-Man (1983) (Intv Corp).int", "P", "2", 4096);

var pac_roms = sv_discover(dir, INTV);
var pac_names = pac_roms.map(function (r) { return r.name; });
check(pac_names.indexOf("Pac-Man (1983) (Atarisoft).int") >= 0,
      "same-4KB-prefix Pac-Man port (Atarisoft) survives dedupe");
check(pac_names.indexOf("Pac-Man (1983) (Intv Corp).int") >= 0,
      "same-4KB-prefix Pac-Man port (Intv Corp) survives dedupe");

// The dedupe must still collapse genuinely identical bytes under different
// names -- the fix is which hash is used, not whether dedupe happens at all.
check(pac_names.indexOf("Astrosmash (1981) (Mattel).int") >= 0
      && pac_names.indexOf("astrosmash.int") < 0,
      "identical full-file bytes still collapse to one entry");

// Sorted by title, case-insensitively.
var titles = roms.map(function (r) { return r.title.toLowerCase(); });
var sorted = titles.slice().sort();
eq(titles.join("|"), sorted.join("|"), "sorted by title");

// Parsed fields ride along.
roms.forEach(function (r) {
	if (r.name === "Astrosmash (1981) (Mattel).int") {
		eq(r.title, "Astrosmash", "discovery parses the title");
		eq(r.year, 1981, "discovery parses the year");
		eq(r.size, 8192, "discovery records the size");
	}
});

// Excludes are case-insensitive substrings.
var fewer = sv_discover(dir, sv_rules({ ext: ["int", "bin", "rom"], min_size: 2048,
                           max_size: 65536, exclude: "utopia" }));
check(fewer.map(function (r) { return r.name; }).indexOf("Utopia (1981) (Mattel).int") < 0,
      "exclude glob drops Utopia");

writeln("5. a BIOS image is never a game");
// exec.bin's real bytes, so the hash guard is exercised, not just the name.
var bios_src = "/sbbs/xtrn/syncivision/exec.bin";
if (file_exists(bios_src)) {
	var b = new File(bios_src);
	b.open("rb");
	var blob = b.read();
	b.close();
	var c = new File(dir + "Executive ROM, The (1978) (Mattel).int");
	c.open("wb"); c.write(blob); c.close();

	var guarded = sv_discover(dir, INTV);
	check(guarded.map(function (r) { return r.name; })
	      .indexOf("Executive ROM, The (1978) (Mattel).int") < 0,
	      "BIOS excluded by content hash, whatever it is named");
} else {
	writeln("  skip   BIOS hash guard (no exec.bin on this host)");
}

// A re-dumped BIOS: right size, right default name, WRONG content (so the
// hash check alone would miss it). The name check must catch it.
put("grom.bin", 2048, "x");
var by_name = sv_discover(dir, INTV);
check(by_name.map(function (r) { return r.name; }).indexOf("grom.bin") < 0,
      "a re-dumped grom.bin is excluded by name, not just by hash");
file_remove(dir + "grom.bin");

directory(dir + "*").forEach(function (p) { file_remove(p); });

writeln("6. activity store");
var pdir = "/tmp/sv_data/";
if (file_isdir(pdir + "syncretro/"))
	directory(pdir + "syncretro/*").forEach(function (p) { file_remove(p); });
mkpath(pdir);

var ppath = sv_plays_path(pdir);
check(ppath.indexOf("syncretro") >= 0, "plays path is under syncretro/");
check(file_isdir(pdir + "syncretro/"), "sv_plays_path created the directory");

check(sv_log_play(ppath, {t: 100, user: 1, alias: "Digital Man",
                          rom: "Utopia (1981) (Mattel).int", secs: 600}), "log 1");
check(sv_log_play(ppath, {t: 200, user: 2, alias: "Nelgin",
                          rom: "Astrosmash (1981) (Mattel).int", secs: 60}), "log 2");
check(sv_log_play(ppath, {t: 300, user: 1, alias: "Digital Man",
                          rom: "Astrosmash (1981) (Mattel).int", secs: 30}), "log 3");

var plays = sv_read_plays(ppath);
eq(plays.length, 3, "three plays read back");
eq(plays[2].rom, "Astrosmash (1981) (Mattel).int", "records keep their order");
eq(plays[0].alias, "Digital Man", "alias round-trips");

// A malformed line must not poison the file.
var junk = new File(ppath);
junk.open("a");
junk.writeln("this is not json");
junk.close();
eq(sv_read_plays(ppath).length, 3, "malformed line skipped, not fatal");

var top = sv_top_played(plays, 5);
eq(top.length, 2, "two distinct games");
eq(top[0].rom, "Astrosmash (1981) (Mattel).int", "most-played first");
eq(top[0].count, 2, "counted twice");
eq(top[0].title, "Astrosmash", "top entries carry a parsed title");
eq(top[1].count, 1, "runner-up counted once");
eq(sv_top_played(plays, 1).length, 1, "n caps the list");
eq(sv_top_played([], 5).length, 0, "no plays, no board");

var last = sv_last_play(plays, 1);
check(last !== null, "user 1 has a last play");
eq(last.t, 300, "last play is the most recent, not the first");
eq(sv_last_play(plays, 99), null, "unknown user has none");

// The board's tie-break is by ROM name, so two runs over the same data give the
// same array. Without it the order would follow object-key enumeration.
var tied = [
	{t: 1, user: 1, alias: "a", rom: "Zaxxon (1982) (Coleco).int",     secs: 1},
	{t: 2, user: 1, alias: "a", rom: "Astrosmash (1981) (Mattel).int", secs: 1},
	{t: 3, user: 1, alias: "a", rom: "Utopia (1981) (Mattel).int",     secs: 1}
];
var t1 = sv_top_played(tied, 5);
var t2 = sv_top_played(tied, 5);
eq(t1.length, 3, "three games, each played once");
eq(t1[0].rom, "Astrosmash (1981) (Mattel).int", "all-tied board sorts by ROM name");
eq(t1[2].rom, "Zaxxon (1982) (Coleco).int", "...ascending");
eq(t1.map(function (x) { return x.rom; }).join("|"),
   t2.map(function (x) { return x.rom; }).join("|"),
   "two calls over the same data agree");

// Ties on `t` resolve to the FIRST record in array order, deterministically.
var same_t = [
	{t: 500, user: 1, alias: "a", rom: "First.int",  secs: 1},
	{t: 500, user: 1, alias: "a", rom: "Second.int", secs: 1}
];
eq(sv_last_play(same_t, 1).rom, "First.int", "equal timestamps: first record wins");

// system.data_dir carries a trailing slash, but a caller may not.
var noslash = "/tmp/sv_slash";
var withslash = "/tmp/sv_slash/";
eq(sv_plays_path(noslash), sv_plays_path(withslash),
   "data_dir with and without a trailing slash agree");
check(file_isdir("/tmp/sv_slash/syncretro/"), "sv_plays_path created the directory");
directory("/tmp/sv_slash/syncretro/*").forEach(function (p) { file_remove(p); });

directory(pdir + "syncretro/*").forEach(function (p) { file_remove(p); });

writeln("7. dump-quality variant collapsing");

eq(sv_dump_rank("x [!].int"), 0, "verified marker ranks 0");
eq(sv_dump_rank("x.int"), 1, "no marker ranks 1");
eq(sv_dump_rank("x [a1].int"), 2, "alternate marker ranks 2");
eq(sv_dump_rank("x [a1][!].int"), 0, "stacked markers: best one wins");
eq(sv_dump_rank("x [o1].int"), 3, "overdump marker ranks 3");
eq(sv_dump_rank("x [b1].int"), 4, "bad-dump marker ranks 4");
eq(sv_dump_rank("x [z9].int"), 2, "unrecognized marker ranks 2");

function rom(name)
{
	var p = sv_parse_title(name);

	return { path: "/roms/" + name, name: name, title: p.title, year: p.year,
	         publisher: p.publisher, size: 8192 };
}

// The real set's three-way "League of Light (Prototype)" split: the [!] dump
// must survive over the alternate-plus-verified and the plain alternate.
var league = [
	rom("League of Light (Prototype) (1983) (Activision) [!].int"),
	rom("League of Light (Prototype) (1983) (Activision) [a1][!].int"),
	rom("League of Light (Prototype) (1983) (Activision) [a2].bin")
];
var lc = sv_collapse_variants(league);
eq(lc.length, 1, "three League of Light dumps collapse to one");
check(lc[0].name.indexOf("[!]") >= 0, "survivor is a verified dump");
check(lc[0].name.indexOf("[a2]") < 0, "survivor is not the [a2] alternate");

// A plain (rank 1) dump beats an overdump (rank 3), regardless of which one
// sv_discover() happened to see first.
var happy = [
	rom("Happy Trails (1983) (Activision).int"),
	rom("Happy Trails (1983) (Activision) [o1].int")
];
var hc = sv_collapse_variants(happy);
eq(hc.length, 1, "Happy Trails collapses to one");
eq(hc[0].name, "Happy Trails (1983) (Activision).int",
   "the plain dump beats the overdump");

// Publisher is part of the key: two different Pac-Man ports must NOT collapse
// into one, or a real game disappears from the picker.
var pacman = [
	{ path: "/roms/a", name: "Pac-Man (1983) (Intv Corp).int",
	  title: "Pac-Man", year: 1983, publisher: "Intv Corp", size: 8192 },
	{ path: "/roms/b", name: "Pac-Man (1983) (Atarisoft).int",
	  title: "Pac-Man", year: 1983, publisher: "Atarisoft", size: 8192 }
];
var pc = sv_collapse_variants(pacman);
eq(pc.length, 2, "two different Pac-Man ports both survive");

// A tie within a rank resolves to the lexicographically smaller name, not to
// whichever entry happened to come first.
var tied_variants = [
	rom("Tie Game (1983) (Acme) [a2].int"),
	rom("Tie Game (1983) (Acme) [a1].int")
];
var vc = sv_collapse_variants(tied_variants);
eq(vc.length, 1, "tied-rank variants collapse to one");
eq(vc[0].name, "Tie Game (1983) (Acme) [a1].int",
   "a same-rank tie keeps the lexicographically smaller name");

writeln("7. column layout");
eq(sv_columns(79, 36), 1, "below 80 cols: one column, always");
eq(sv_columns(40, 36), 1, "narrow terminal: one column");
eq(sv_columns(80, 36), 2, "80 cols: two columns");
eq(sv_columns(132, 36), 3, "132 cols: three columns");
check(sv_columns(80, 200) >= 1, "an absurd cell width still yields one column");

eq(sv_page_rows(24, 5, 2), 17, "24 rows minus header and footer");
check(sv_page_rows(4, 5, 2) >= 1, "a tiny terminal still shows one row");

var pages = sv_paginate([1, 2, 3, 4, 5], 2);
eq(pages.length, 3, "five items, two per page, three pages");
eq(pages[2].length, 1, "last page holds the remainder");
eq(sv_paginate([], 10).length, 0, "no items, no pages");

// sv_cell now carries \1x attribute codes; measure the VISIBLE width by stripping them.
function vislen(s) { return s.replace(/\x01./g, "").length; }

var cell = sv_cell(7, {title: "Astrosmash", year: 1981}, 36);
check(cell.indexOf("7") >= 0, "cell shows the number");
check(cell.indexOf("Astrosmash") >= 0, "cell shows the title");
check(cell.indexOf("1981") >= 0, "cell shows the year");
eq(vislen(cell), 36, "cell visible width is fixed to its column count");

var longcell = sv_cell(199, {title: "Advanced Dungeons and Dragons Treasure of Tarmin",
                             year: 1982}, 30);
eq(vislen(longcell), 30, "an over-long title is clipped to width, not wrapped");

var noyear = sv_cell(1, {title: "4tris", year: 0}, 36);
check(noyear.indexOf("(0)") < 0, "a missing year is omitted, not printed as 0");

writeln("N. platform token (native-artifact subdir)");
// Windows is Win32-only by design: both a 32- and a 64-bit Synchronet host
// resolve to the one "win32" subdir, so a Win64 host finds the Win32 door.
eq(sv_platform("Win32"), "win32", "Win32 -> win32");
eq(sv_platform("Win64"), "win32", "Win64 host still maps to the Win32 door dir");
// macOS: system.platform ("macOS"/"MacOSX") and deploy's uname ("Darwin")
// must agree on one token.
eq(sv_platform("macOS"),  "darwin", "macOS -> darwin");
eq(sv_platform("MacOSX"), "darwin", "MacOSX -> darwin");
eq(sv_platform("Darwin"), "darwin", "Darwin -> darwin");
// *nixes: lower-cased, matching `uname -s | tr A-Z a-z`.
eq(sv_platform("Linux"),   "linux",   "Linux -> linux");
eq(sv_platform("FreeBSD"), "freebsd", "FreeBSD -> freebsd");
eq(sv_platform("NetBSD"),  "netbsd",  "NetBSD -> netbsd");
// A "/" (as in "GNU/Hurd") must not nest a path.
check(sv_platform("GNU/Hurd").indexOf("/") < 0, "no path separator survives in the token");
// The core extension follows from the platform token.
eq(sv_core_ext("win32"),  "dll",   "win32 core is .dll");
eq(sv_core_ext("darwin"), "dylib", "darwin core is .dylib");
eq(sv_core_ext("linux"),  "so",    "linux core is .so");

writeln("N. architecture bucket (system.architecture <-> uname -m must agree)");
// system.architecture spellings...
eq(sv_arch("x64"),   "x64",   "system.architecture x64 -> x64");
eq(sv_arch("i686"),  "x86",   "system.architecture i686 -> x86");
eq(sv_arch("i386"),  "x86",   "system.architecture i386 -> x86");
eq(sv_arch("arm64"), "arm64", "system.architecture arm64 -> arm64");
eq(sv_arch("armv7"), "arm",   "system.architecture armv7 -> arm");
eq(sv_arch("armv8"), "arm64", "system.architecture armv8 (aarch64) -> arm64");
// ...and the `uname -m` spellings for the SAME hosts land on the same bucket.
eq(sv_arch("x86_64"),  "x64",   "uname x86_64 -> x64");
eq(sv_arch("amd64"),   "x64",   "uname amd64 -> x64");
eq(sv_arch("aarch64"), "arm64", "uname aarch64 -> arm64");
eq(sv_arch("armv7l"),  "arm",   "uname armv7l -> arm");
// An unrecognized arch passes through (lower-cased, sanitized).
eq(sv_arch("riscv64"), "riscv64", "riscv64 passes through");

writeln("N. target sub-dir (platform + arch)");
eq(sv_target("Win32", "i686"),   "",            "Windows is flat (.exe/.dll never collide)");
eq(sv_target("Win64", "x64"),    "",            "64-bit Windows host is flat too");
eq(sv_target("Linux", "x64"),    "linux-x64",   "Linux x86_64");
eq(sv_target("Linux", "aarch64"),"linux-arm64", "Linux arm64 does not collide with linux-x64");
eq(sv_target("FreeBSD","amd64"), "freebsd-x64", "FreeBSD amd64");
eq(sv_target("macOS", "arm64"),  "darwin-arm64","Apple Silicon");

writeln("N. console rules, as data ([roms] / [console] in syncretro.ini)");

// The console's rules come from ITS lobby.js spec, not from a default buried in
// the shared code: nothing here knows an Intellivision from an NES.
eq(INTV.ext.join(","), "int,bin,rom", "Intellivision extensions");
eq(INTV.min_size, 2048,  "Intellivision min size");
eq(INTV.max_size, 65536, "Intellivision max size");
eq(INTV.bios_md5.length, 2, "the two Intellivision BIOS hashes");

// An array (a JS spec) and a comma string (an ini value) mean the same thing.
eq(sv_list(["a", "b"]).join(","), "a,b", "a list from an array");
eq(sv_list(" a , b ").join(","),  "a,b", "a list from a string, trimmed");
eq(sv_list(null).length, 0, "a missing list is empty, not a crash");

// Sizes: bare bytes, k and m suffixes.
eq(sv_size("8192", 0), 8192,       "bare byte count");
eq(sv_size("64k", 0),  65536,      "k suffix");
eq(sv_size("4m", 0),   4194304,    "m suffix");
eq(sv_size("4M", 0),   4194304,    "suffix is case-insensitive");
eq(sv_size("", 99),    99,         "an empty size takes the default");
eq(sv_size("banana", 99), 99,      "an unparseable size takes the default, NOT 0");

// The NES: a different console, expressed entirely as data. No BIOS at all --
// and an EMPTY bios_md5 key must OVERRIDE the Intellivision default, not fall
// back to it, or the NES would reject a ROM that happened to hash like exec.bin.
var NES = sv_rules({
	ext:      [".nes", ".unf", ".unif"],   /* the leading dot is optional */
	min_size: "8k",
	max_size: "4m"
	/* no bios_md5, no bios_names: the NES has no BIOS */
});
eq(NES.ext.join(","), "nes,unf,unif", "the leading dot is optional in ext");
eq(NES.min_size, 8192,    "NES min size");
eq(NES.max_size, 4194304, "NES max size");
eq(NES.bios_md5.length, 0,   "the NES has no BIOS to reject");
eq(NES.bios_names.length, 0, "...by name either");

// A 256 KB NES ROM is a cartridge; the same file would be rejected on the
// Intellivision's 64 KB ceiling. Same code, different data.
check(262144 >= NES.min_size && 262144 <= NES.max_size, "256 KB is a NES cartridge");
check(!(262144 >= INTV.min_size && 262144 <= INTV.max_size), "...and not an Intellivision one");

// The console's identity.
var ci = sv_console({ name: "Nintendo Entertainment System", short: "NES", profile: "pad" });
eq(ci.name, "Nintendo Entertainment System", "the long name is what the lobby shows");
eq(ci.short, "NES", "the short name fits a header column");
eq(ci.id, "nes", "the id is derived from short: no fourth key to get out of step");
eq(ci.profile, "pad", "the C door's profile rides in the same section");

// id must be safe to use as a filename component (it names the cache and the
// per-user save dir), whatever the sysop types.
eq(sv_console({ short: "PC Engine" }).id, "pcengine", "spaces do not survive into the id");
eq(sv_console({ short: "../etc" }).id, "etc", "a path traversal cannot come through the id");
eq(sv_console({ name: "Intellivision" }).short, "Intellivision", "short falls back to name");
eq(sv_console({ short: "Intv" }).id, "intv", "the Intellivision id stays intv: saves must not be orphaned");
eq(sv_console({ name: "X" }).profile, "pad", "the default profile is the generic RetroPad");
eq(sv_console(null).id, "", "no [console] section at all is legal");

writeln("N. discovery cache");
//
// The point of the cache is ROUND TRIPS, not arithmetic: discovery used to open
// and read every candidate on every lobby entry, which over SMB is one open +
// read + close per ROM across the wire. sv_file_md5() is the ONLY thing in the
// lib that opens a ROM, so cache.hashed is an exact count of file opens -- which
// is what these tests assert on. A warm run must open nothing.

// Its own fixtures: test 4's scratch dir is wiped at the end of test 4.
var croms = "/tmp/sv_cache_roms/";
var cdir  = "/tmp/sv_cache/";
[croms, cdir].forEach(function (d) {
	if (file_isdir(d))
		directory(d + "*").forEach(function (p) { file_remove(p); });
	mkpath(d);
});
if (file_isdir(cdir + "syncretro/"))
	directory(cdir + "syncretro/*").forEach(function (p) { file_remove(p); });

function cput(name, bytes, fill)
{
	var g = new File(croms + name);
	var s = "";
	var i;

	g.open("wb");
	for (i = 0; i < bytes; i++)
		s += fill;
	g.write(s);
	g.close();
}

cput("Astrosmash (1981) (Mattel).int", 8192, "A");
cput("Utopia (1981) (Mattel).int", 8192, "U");
cput("Boring Game (1983) (Acme).bin", 4096, "b");
cput("huge.int", 70000, "h");                      // over 64 KB: never a candidate

var cpath = sv_cache_path(cdir, "intv");
check(cpath.indexOf("roms.intv.json") > 0, "cache path is named for the console id");

// Cold: nothing cached, so every surviving candidate is hashed.
var c1 = sv_cache_open(cpath);
var r1 = sv_discover(croms, INTV, c1);
eq(r1.length, 3, "cold run finds the three cartridges");
check(c1.hashed > 0, "cold run hashes (opened " + c1.hashed + " files)");
eq(c1.reused, 0, "cold run reuses nothing");
check(sv_cache_flush(c1), "cold run writes the cache");
check(file_exists(cpath), "the cache file exists after a flush");

// Warm: same bytes, same mtimes -> ZERO opens, and the same answer.
var c2 = sv_cache_open(cpath);
var r2 = sv_discover(croms, INTV, c2);
eq(c2.hashed, 0, "WARM RUN OPENS NOTHING");
check(c2.reused > 0, "warm run reuses the cached hashes");
eq(r2.length, r1.length, "warm run finds the same number of ROMs");
eq(r2.map(function (r) { return r.name; }).join("|"),
   r1.map(function (r) { return r.name; }).join("|"),
   "warm run returns the identical list, in the identical order");

// Stale: one file's content changes (new size), so exactly that one re-hashes.
cput("Utopia (1981) (Mattel).int", 9000, "V");
var c3 = sv_cache_open(cpath);
sv_discover(croms, INTV, c3);
eq(c3.hashed, 1, "a changed file re-hashes, and only it");
check(c3.reused > 0, "the unchanged files still come from the cache");
sv_cache_flush(c3);

// A file that disappears must not linger in the cache forever.
var before = sv_cache_open(cpath);
check(before.entries.hasOwnProperty("huge.int") === false,
      "a size-rejected file was never cached (it is not a candidate)");
file_remove(croms + "Boring Game (1983) (Acme).bin");
var c4 = sv_cache_open(cpath);
sv_discover(croms, INTV, c4);
sv_cache_flush(c4);
var c5 = sv_cache_open(cpath);
check(!c5.entries.hasOwnProperty("Boring Game (1983) (Acme).bin"),
      "a deleted ROM is pruned from the cache");

// Torn / hand-mangled / truncated cache: treated as cold, never fatal.
var tf = new File(cpath);
tf.open("w");
tf.write("{ this is not json");
tf.close();
var c6 = sv_cache_open(cpath);
eq(c6.entries ? Object.keys(c6.entries).length : -1, 0, "an unparseable cache reads as empty");
var r6 = sv_discover(croms, INTV, c6);
check(c6.hashed > 0, "a torn cache falls back to hashing");
eq(r6.length, r1.length - 1, "and still finds every ROM (less the one deleted above)");

// No cache at all: the old signature still works, and still discovers.
var r7 = sv_discover(croms, INTV);
eq(r7.length, r6.length, "sv_discover() with no cache argument behaves as before");

writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
