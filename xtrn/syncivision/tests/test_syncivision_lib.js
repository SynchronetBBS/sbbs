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

writeln(failures ? "FAIL: " + failures + " failure(s)" : "ok: 0 failures");
exit(failures ? 1 : 0);
