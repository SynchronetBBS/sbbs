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

// --- discovery ---------------------------------------------------------------
//
// roms/ is a dumped ROM set, not a curated folder: it carries metadata litter,
// BIOS images that are unplayable as cartridges, and (here) short symlink
// aliases which -- over the install's SMB view -- are indistinguishable from
// regular files, so readlink() is useless and dedupe must go by content.

// exec.bin and grom.bin. Rejected by CONTENT, so a re-dropped ROM set cannot
// smuggle one back in under a cartridge's name.
var SV_BIOS_MD5 = [
	"62e761035cb657903761800f4437b8af",   // exec.bin, 8192 bytes
	"0cd5946c6473e42e8e4c2137785e427f"    // grom.bin, 2048 bytes
];

// The entire Intellivision cartridge range. Anything outside it is not a game.
var SV_MIN_SIZE = 2 * 1024;
var SV_MAX_SIZE = 64 * 1024;

// Only these characters break out of the double quotes the door command line
// wraps a ROM path in. See LAUNCHER.md sec 8.
function sv_quote_safe(name)
{
	return !/["`$\\]/.test(String(name));
}

// md5_calc() returns BASE64 unless its second argument is true. Ask for hex.
function sv_file_md5(path, bytes)
{
	var f = new File(path);
	var data;

	if (!f.open("rb"))
		return "";
	data = bytes > 0 ? f.read(bytes) : f.read();
	f.close();
	if (data === null || data === undefined)
		return "";
	return md5_calc(data, true);
}

function sv_excluded(name, excludes)
{
	var lower = String(name).toLowerCase();
	var i;

	for (i = 0; i < excludes.length; i++)
		if (excludes[i] !== "" && lower.indexOf(String(excludes[i]).toLowerCase()) >= 0)
			return true;
	return false;
}

// --- variant collapsing -------------------------------------------------------
//
// The content dedupe above cannot see this case: a ROM set ships several
// dumps of the SAME game whose BYTES DIFFER --
// "Dreadnaught Factor, The (1983) (Activision) [!].int" beside
// "... [a1].bin" -- so size+hash sees two different games. But the dump
// marker is not part of the title (sv_parse_title strips it), so both
// entries render identically in the picker: the player sees two lines that
// look like duplicates, and has no way to tell that one is a verified good
// dump and the other an inferior rip. 12 of the 221 cartridges in the
// reference set are affected. This step keys on title + year + publisher
// (publisher is in the key on purpose -- "Pac-Man (1983) (Intv Corp)" and
// "Pac-Man (1983) (Atarisoft)" are two different ports, not two dumps of
// one game, and must both survive) and keeps only the best-ranked marker
// per key.
//
// This *hides* files that exist on disk -- it never touches the
// filesystem, and the losing dump is still sitting in roms_dir afterward.
// A sysop who disagrees with the automatic pick has two escape hatches:
// `[roms] exclude=` in syncretro.ini to hide a specific file outright, or
// simply deleting the unwanted dump.

// rank: 0 = "[!]" verified, 1 = no marker, 2 = "[a*]" or anything
// unrecognized, 3 = "[o*]" overdump, 4 = "[b*]" bad dump. A name may carry
// stacked markers ("[a1][!]"), so this looks at every trailing bracket
// group and returns the BEST (lowest) rank found, not just the last one.
function sv_dump_rank(filename)
{
	var name = sv_strip_ext(String(filename));
	var m = name.match(/((?:\s*\[[^\]]*\])+)\s*$/);
	var markers, best, i, body, rank;

	if (!m)
		return 1;                     /* no trailing marker: a plain dump */

	markers = m[1].match(/\[[^\]]*\]/g);
	best = 4;
	for (i = 0; i < markers.length; i++) {
		body = markers[i].slice(1, -1).toLowerCase();
		rank = 2;                     /* unrecognized marker: alternate */
		if (body === "!")
			rank = 0;
		else if (/^a\d*$/.test(body))
			rank = 2;
		else if (/^o\d*$/.test(body))
			rank = 3;
		else if (/^b\d*$/.test(body))
			rank = 4;
		if (rank < best)
			best = rank;
	}
	return best;
}

// True if `a` should replace `b` as the kept dump for their shared key:
// the better (lower) rank wins, and a tie resolves to the lexicographically
// smaller name so the result never depends on directory() enumeration order.
function sv_dump_better(a, b)
{
	var ra = sv_dump_rank(a.name), rb = sv_dump_rank(b.name);

	if (ra !== rb)
		return ra < rb;
	return a.name < b.name;
}

function sv_collapse_variants(roms)
{
	var best  = {};        // key -> best entry seen so far
	var order = [];        // keys in first-seen order, so survivors keep it
	var out   = [];
	var i, r, key;

	for (i = 0; i < roms.length; i++) {
		r = roms[i];
		key = r.title.toLowerCase() + "|" + r.year + "|" + r.publisher.toLowerCase();
		if (!best.hasOwnProperty(key)) {
			best[key] = r;
			order.push(key);
			continue;
		}
		if (sv_dump_better(r, best[key]))
			best[key] = r;
	}
	for (i = 0; i < order.length; i++)
		out.push(best[order[i]]);
	return out;
}

function sv_discover(roms_dir, exts, excludes)
{
	var seen = {};         // content key -> index into out
	var out  = [];
	var dir  = backslash(roms_dir);

	exts.forEach(function (ext) {
		directory(dir + "*." + ext).forEach(function (path) {
			var name = file_getname(path);
			var size = file_size(path);
			var head, key, prev, parsed;

			if (size < SV_MIN_SIZE || size > SV_MAX_SIZE)
				return;                       /* not a cartridge */
			if (sv_excluded(name, excludes))
				return;

			head = sv_file_md5(path, 4096);
			if (head === "")
				return;                       /* unreadable: not our problem */
			if (SV_BIOS_MD5.indexOf(sv_file_md5(path, 0)) >= 0)
				return;                       /* a BIOS image, whatever its name */

			/* Content identity: size plus the head hash. Two names for the same
			 * bytes are one game; keep the more descriptive name. */
			key = size + ":" + head;
			if (seen.hasOwnProperty(key)) {
				prev = out[seen[key]];
				if (name.length > prev.name.length) {
					prev.name = name;
					prev.path = path;
					parsed = sv_parse_title(name);
					prev.title     = parsed.title;
					prev.year      = parsed.year;
					prev.publisher = parsed.publisher;
				}
				return;
			}

			parsed = sv_parse_title(name);
			seen[key] = out.length;
			out.push({
				path:      path,
				name:      name,
				title:     parsed.title,
				year:      parsed.year,
				publisher: parsed.publisher,
				size:      size
			});
		});
	});

	out = sv_collapse_variants(out);

	out.sort(function (a, b) {
		var x = a.title.toLowerCase(), y = b.title.toLowerCase();

		return x < y ? -1 : (x > y ? 1 : 0);
	});
	return out;
}

// --- the activity store ------------------------------------------------------
//
// Append-only, one JSON object per completed play. No aggregate counts file:
// data_dir is an SMB mount that a second host also writes, so a read-modify-write
// of a counts file is a lost-update race. Everything is derived by scanning,
// which at BBS scale is a few thousand lines.

function sv_plays_path(data_dir)
{
	var dir = backslash(data_dir) + "syncretro/";

	if (!file_isdir(dir))
		mkpath(dir);
	return dir + "plays.jsonl";
}

// One line, appended under an exclusive open. Returns false rather than throwing:
// a lost play is not worth failing a player's session over.
function sv_log_play(path, rec)
{
	var f = new File(path);

	if (!f.open("a"))
		return false;
	f.writeln(JSON.stringify(rec));
	f.close();
	return true;
}

function sv_read_plays(path)
{
	var f = new File(path);
	var out = [];
	var lines, i, rec;

	if (!file_exists(path) || !f.open("r"))
		return out;
	lines = f.readAll();
	f.close();
	for (i = 0; i < lines.length; i++) {
		if (lines[i] === "")
			continue;
		try {
			rec = JSON.parse(lines[i]);
		} catch (e) {
			continue;              /* a torn or hand-edited line is not fatal */
		}
		if (rec && rec.rom)
			out.push(rec);
	}
	return out;
}

function sv_top_played(plays, n)
{
	var counts = {};
	var out = [];
	var rom;

	plays.forEach(function (p) {
		counts[p.rom] = (counts[p.rom] || 0) + 1;
	});
	for (rom in counts)
		if (counts.hasOwnProperty(rom))
			out.push({ rom: rom, title: sv_parse_title(rom).title, count: counts[rom] });

	out.sort(function (a, b) {
		if (a.count !== b.count)
			return b.count - a.count;
		return a.rom < b.rom ? -1 : (a.rom > b.rom ? 1 : 0);   /* stable, name-ordered */
	});
	return out.slice(0, n);
}

function sv_last_play(plays, user_number)
{
	var best = null;

	plays.forEach(function (p) {
		if (p.user === user_number && (best === null || p.t > best.t))
			best = p;
	});
	return best;
}
