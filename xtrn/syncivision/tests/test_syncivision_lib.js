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

writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
