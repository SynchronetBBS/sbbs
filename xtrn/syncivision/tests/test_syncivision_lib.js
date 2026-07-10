// test_syncivision_lib.js -- jsexec tests for the UI-free half of the lobby.
//
// Run:  /sbbs/exec/jsexec /home/rswindell/sbbs/xtrn/syncivision/tests/test_syncivision_lib.js
// Exits non-zero on any failure. SpiderMonkey 1.8.5: no let/const/arrows.

load(js.exec_dir + "../syncivision_lib.js");

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
var roms = sv_discover(dir, ["int", "bin", "rom"], []);
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

var pac_roms = sv_discover(dir, ["int", "bin", "rom"], []);
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
var fewer = sv_discover(dir, ["int", "bin", "rom"], ["utopia"]);
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

	var guarded = sv_discover(dir, ["int", "bin", "rom"], []);
	check(guarded.map(function (r) { return r.name; })
	      .indexOf("Executive ROM, The (1978) (Mattel).int") < 0,
	      "BIOS excluded by content hash, whatever it is named");
} else {
	writeln("  skip   BIOS hash guard (no exec.bin on this host)");
}

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

writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
