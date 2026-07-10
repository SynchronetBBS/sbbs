// syncivision_lib.js -- model layer for the SyncRetro (Intellivision) lobby.
//
// UI-FREE: no console, no bbs, no user. Everything it needs is passed in. That
// is what lets tests/test_syncivision_lib.js drive it under jsexec, where those
// objects do not exist. lobby.js layers the screen and the door command line on
// top. See src/doors/syncretro/LAUNCHER.md.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

// --- title parsing ----------------------------------------------------------
//
// ROM sets name files "Title (Year) (Publisher).int", with a trailing "[!]" or
// "[a1]" dump marker on some. Titles may contain their own parenthetical --
// "Adventure (AD&D - Cloudy Mountain)" -- so the year is found as the LAST
// 4-digit parenthetical group, not the first, and the publisher as the group
// after it. A name that does not fit keeps its whole self as the title: a
// wrong guess is worse than no guess.

function sv_strip_ext(name)
{
	var dot = name.lastIndexOf(".");

	return dot > 0 ? name.substr(0, dot) : name;
}

function sv_parse_title(filename)
{
	var name = sv_strip_ext(String(filename));
	var out  = { title: name, year: 0, publisher: "" };
	var m;

	// One or more stacked dump markers: "[!]", "[a1][!]".
	name = name.replace(/(\s*\[[^\]]*\])+\s*$/, "");

	// "<title> (<4-digit year>) (<publisher>)" anchored at the END of the name.
	// A year range, e.g. "(1982-83)", takes its first year.
	m = name.match(/^(.*)\s+\((\d{4})(?:-\d{2,4})?\)\s+\(([^()]*)\)$/);
	if (m) {
		out.title     = m[1];
		out.year      = parseInt(m[2], 10);
		out.publisher = m[3];
		return out;
	}

	// "<title> (<4-digit year>)" with no publisher.
	m = name.match(/^(.*)\s+\((\d{4})(?:-\d{2,4})?\)$/);
	if (m) {
		out.title = m[1];
		out.year  = parseInt(m[2], 10);
		return out;
	}

	out.title = name;
	return out;
}
