// syncretro_lib.js -- model layer for every SyncRetro console lobby.
//
// UI-FREE: no console, no bbs, no user. Everything it needs is passed in. That
// is what lets xtrn/syncivision/tests/test_syncretro_lib.js drive it under
// jsexec, where those objects do not exist. Each console's lobby.js layers the
// screen and the door command line on top.
// See src/doors/syncretro/LAUNCHER.md and M3_MULTICORE.md.
//
// WHY IT LIVES IN exec/load/ AND NOT IN THE DOOR DIR: it serves SEVERAL install
// dirs -- xtrn/syncivision (Intellivision), xtrn/syncnes (NES), and whatever
// console comes next -- which all run the same door binary against a different
// libretro core. A lib serving ONE door belongs in that door's dir (see
// syncdoom_lib.js, syncduke_lib.js); a lib serving several belongs here, beside
// game_lobby.js. Copying it per console would fork it, and it would drift.
//
// NOTHING HERE KNOWS WHICH CONSOLE IT IS. The console's identity (name, id) and
// its ROM rules (extensions, size band, BIOS images to reject) arrive as data,
// from that install's lobby.js -- which IS the console definition, and which also
// passes the same facts to the C door on its command line, so the two halves
// cannot disagree. Only [roms] dir/exclude comes from syncretro.ini: a sysop
// hides a ROM, but a sysop does not redefine what an NES cartridge is.
//
// SpiderMonkey 1.8.5: no let/const/arrows/template literals.
//
// Copyright(C) 2026 Rob Swindell / SyncRetro. GPL-2.0.

load("door_deploy.js");   // door_platform/arch/target: shared with every deploy.js

// --- title parsing ----------------------------------------------------------
//
// ROM sets name files "Title (Year) (Publisher).int", with a trailing "[!]" or
// "[a1]" dump marker on some. Titles may contain their own parenthetical --
// "Adventure (AD&D - Cloudy Mountain)" -- so the year is found as the LAST
// 4-digit parenthetical group, not the first, and the publisher as the group
// after it. A name that does not fit keeps its whole self as the title: a
// wrong guess is worse than no guess.
//
// The NES sets are named to a different convention (No-Intro / GoodNES) that
// staples CATALOGING TAGS onto the end instead: "Super Mario Bros. (World).nes",
// "Kirby's Adventure (USA) (Rev 1).nes", "Contra (U) [!].nes". Those tags are
// noise to a player choosing a game, so they come off the displayed title -- but
// they are NOT thrown away, because they are the only thing distinguishing two
// entries that are otherwise identical: "Contra (USA)" and "Contra (Europe)" are
// different dumps (NTSC vs a slower 50 Hz PAL conversion), and the variant
// collapse below would otherwise fold them onto one key and HIDE one of them.
// So a tag is parsed OUT of the title, kept in the entry, made part of the
// collapse key, and put back on screen only where a title would otherwise be
// ambiguous (see syncretro_disambiguate).
//
// A tag is recognized by its CONTENT, never by its position: a trailing
// parenthetical that is not a known tag is part of the title and stays there. A
// publisher ("(Mattel)") and a year are not tags, so the Intellivision naming is
// untouched by any of this.

function syncretro_strip_ext(name)
{
	var dot = name.lastIndexOf(".");

	return dot > 0 ? name.substr(0, dot) : name;
}

/* No-Intro region words, alone or comma-separated: "(USA)", "(USA, Europe)". */
var SYNCRETRO_REGIONS = [
	"world", "usa", "europe", "japan", "asia", "australia", "brazil", "canada",
	"china", "denmark", "finland", "france", "germany", "greece", "hong kong",
	"italy", "korea", "latin america", "mexico", "netherlands", "norway",
	"poland", "portugal", "russia", "scandinavia", "spain", "sweden", "taiwan",
	"uk", "united kingdom", "unknown"
];

/* GoodNES codes: "(U)", "(E)", "(J)", "(UE)", "(JU)", "(PD)", "(Unl)". */
var SYNCRETRO_GOODCODES = [
	"u", "e", "j", "w", "f", "g", "i", "s", "a", "b", "c", "k", "ue", "je",
	"ju", "jue", "pd", "unl"
];

/* Language codes, alone or listed: "(En)", "(En,Fr,De)". Spelled out rather than
 * accepting "any two letters", which would eat a title's own parenthetical. */
var SYNCRETRO_LANGS = [
	"en", "fr", "de", "es", "it", "ja", "nl", "pt", "sv", "no", "da", "fi",
	"zh", "ko", "ru", "pl", "el", "cs", "hu", "tr", "he", "ar", "ca", "sk",
	"uk", "hr", "sl", "et", "lv", "lt", "ro", "bg"
];

/* Release status / revision words: "(Rev 1)", "(Proto)", "(Beta 2)", "(Alt)". */
var SYNCRETRO_STATUS = [
	"proto", "prototype", "beta", "alpha", "sample", "demo", "unl",
	"unlicensed", "pirate", "hack", "aftermarket", "homebrew", "alt",
	"kiosk", "promo", "rev", "version", "v", "prg", "virtual console",
	"classic series", "gamecube", "switch online", "e-reader"
];

// Is this parenthetical body a cataloging tag rather than part of the title?
function syncretro_is_meta_tag(body)
{
	var s = String(body).toLowerCase().replace(/^\s+|\s+$/g, "");
	var parts, i, word;

	if (s === "")
		return false;
	if (/^\d{4}(-\d{2,4})?$/.test(s))
		return false;                   /* a year: the parse below owns it */

	/* A comma-separated list: every element must itself be a tag word. That is
	 * what keeps "(AD&D - Cloudy Mountain)" and "(Intv Corp)" out of here. */
	parts = s.split(",");
	for (i = 0; i < parts.length; i++) {
		word = parts[i].replace(/^\s+|\s+$/g, "");
		if (word === "")
			return false;
		if (SYNCRETRO_REGIONS.indexOf(word) >= 0)
			continue;
		if (SYNCRETRO_GOODCODES.indexOf(word) >= 0)
			continue;
		if (SYNCRETRO_LANGS.indexOf(word) >= 0)
			continue;
		/* "Rev 1", "Rev A", "Beta 2", "v1.1", "PRG0" -- a status word, with an
		 * optional trailing number/letter. */
		if (SYNCRETRO_STATUS.indexOf(word.replace(/[\s.]*[\d.]*[a-z]?$/, "")) >= 0)
			continue;
		if (SYNCRETRO_STATUS.indexOf(word) >= 0)
			continue;
		return false;                   /* one unknown element: not a tag */
	}
	return true;
}

function syncretro_parse_title(filename)
{
	var name = syncretro_strip_ext(String(filename));
	var out  = { title: name, year: 0, publisher: "", tags: [] };
	var m;

	// One or more stacked dump markers: "[!]", "[a1][!]".
	name = name.replace(/(\s*\[[^\]]*\])+\s*$/, "");

	// Trailing cataloging tags, right to left: "(USA) (Rev 1)" -> both. Stops at
	// the first trailing parenthetical that is not a known tag, so a year, a
	// publisher, or a title's own parenthetical ends the sweep.
	for (;;) {
		m = name.match(/^(.*?)\s*\(([^()]*)\)\s*$/);
		if (!m || !syncretro_is_meta_tag(m[2]))
			break;
		out.tags.unshift(m[2].replace(/^\s+|\s+$/g, ""));
		name = m[1];
	}
	out.title = name;

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

// --- the console's rules, as data ---------------------------------------------
//
// These were Intellivision constants in the code. They are now whatever that
// install's syncretro.ini says, because "which files in roms/ are cartridges"
// is a fact about the CONSOLE, not about the launcher:
//
//   Intellivision  .int/.bin/.rom   2 KB - 64 KB   rejects exec.bin + grom.bin
//   NES            .nes/.unf/.unif  8 KB - 4 MB    no BIOS at all
//                                                  (.fds DOES need disksys.rom)
//
// A BIOS is rejected by CONTENT as well as by name, so that a re-dropped ROM set
// cannot smuggle one back into the picker under a cartridge's name -- and by
// name as well as by content, since a re-dumped BIOS hashes differently but is
// still not a game. A console with no BIOS (the NES) simply lists neither.

// "64k" / "4m" / "8192" -> bytes. An unparseable value takes the default rather
// than becoming 0, which would silently reject every ROM.
function syncretro_size(v, dflt)
{
	var m = String(v == null ? "" : v).trim().toLowerCase().match(/^(\d+)\s*([km]?)b?$/);

	if (!m)
		return dflt;
	return parseInt(m[1], 10) * (m[2] === "k" ? 1024 : m[2] === "m" ? 1024 * 1024 : 1);
}

// A list, from either an array (a console spec, written in JS) or a comma-
// separated string (an ini value, typed by a sysop). Both spellings mean the
// same thing, so callers never have to care which they were handed.
function syncretro_list(v)
{
	var a;

	if (v == null)
		return [];
	a = (v instanceof Array) ? v : String(v).split(",");
	return a.map(function (s) { return String(s).trim(); })
	        .filter(function (s) { return s !== ""; });
}

// The discovery rules: what a cartridge looks like ON THIS CONSOLE. They come
// from the console's spec (its lobby.js), not from configuration -- a sysop
// hides a ROM or moves the roms dir, but a sysop does not redefine what an NES
// cartridge is. Only `dir` and `exclude` are overridable from syncretro.ini,
// which the lobby does after calling this.
function syncretro_rules(spec)
{
	var r = {
		dir:        "roms",
		ext:        [],
		exclude:    [],
		min_size:   1,
		max_size:   16 * 1024 * 1024,
		bios_md5:   [],
		bios_names: []
	};

	if (!spec)
		return r;
	if (spec.dir_name)
		r.dir = String(spec.dir_name);
	/* [".nes", "unf"] and ".nes, unf" must mean the same thing. */
	r.ext = syncretro_list(spec.ext).map(function (e) {
		return e.replace(/^\./, "").toLowerCase();
	});
	r.exclude    = syncretro_list(spec.exclude);
	r.min_size   = syncretro_size(spec.min_size, r.min_size);
	r.max_size   = syncretro_size(spec.max_size, r.max_size);
	r.bios_md5   = syncretro_list(spec.bios_md5).map(function (h) { return h.toLowerCase(); });
	r.bios_names = syncretro_list(spec.bios_names).map(function (n) { return n.toLowerCase(); });
	return r;
}

// The console's identity. `id` is DERIVED from `short` -- lower-cased and
// stripped to alphanumerics, because it names a directory (the per-user save
// dir) and a filename (the ROM cache), and a sysop's display string must never
// be able to reach either. It is not a separate key: a separate key is a thing
// to get out of step with the name beside it.
function syncretro_console(spec)
{
	var c = { name: "", short: "", profile: "pad", core: "", id: "" };

	if (spec) {
		if (spec.name)
			c.name = String(spec.name);
		if (spec.short)
			c.short = String(spec.short);
		if (spec.profile)
			c.profile = String(spec.profile).toLowerCase();
		if (spec.core)
			c.core = String(spec.core);
	}
	if (c.short === "")
		c.short = c.name;
	if (c.name === "")
		c.name = c.short;
	c.id = c.short.toLowerCase().replace(/[^a-z0-9]+/g, "");
	return c;
}

// Only these characters break out of the double quotes the door command line
// wraps a ROM path in. See LAUNCHER.md sec 8.
function syncretro_quote_safe(name)
{
	return !/["`$\\]/.test(String(name));
}

// md5_calc() returns BASE64 unless its second argument is true. Ask for hex.
function syncretro_file_md5(path, bytes)
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

// --- the discovery cache ------------------------------------------------------
//
// Discovery used to open, read and hash EVERY candidate on EVERY lobby entry.
// The arithmetic was never the problem: the round trips were. The install is an
// SMB mount, so a remote node paid one open + read + close per cartridge across
// the wire just to draw a menu -- a visible startup delay on a 200-ROM set, and
// unusable on the thousands a larger console's ROM set carries.
//
// So the hash is cached, keyed by the file's name + size + mtime. A warm run
// opens exactly ONE file (this cache) instead of one per ROM. cache.hashed is
// therefore an exact count of ROM opens, which is what the tests assert on.
//
// The cache is DERIVED: every entry can be recomputed from the ROM it describes.
// That is what lets it be written with none of the care plays.jsonl needs -- a
// lost update (data_dir is shared with a second host) costs a re-hash, never a
// wrong answer. A torn, stale, hand-edited or missing cache reads as empty, and
// discovery simply does what it did before.

var SYNCRETRO_CACHE_VERSION = 1;

function syncretro_cache_path(data_dir, id)
{
	// Same trailing-separator care as syncretro_plays_path(); see there.
	var dir = String(data_dir).replace(/[\\\/]+$/, "") + "/syncretro/";

	if (!file_isdir(dir))
		mkpath(dir);
	return dir + "roms." + String(id).toLowerCase().replace(/[^a-z0-9]+/g, "") + ".json";
}

function syncretro_cache_open(path)
{
	var cache = { path: path, entries: {}, dirty: false, hashed: 0, reused: 0 };
	var f = new File(path);
	var text, obj;

	if (!file_exists(path) || !f.open("r"))
		return cache;                  /* no cache yet: a cold run, not an error */
	text = f.read();
	f.close();
	try {
		obj = JSON.parse(text);
	} catch (e) {
		return cache;                  /* torn or hand-mangled: cold, never fatal */
	}
	if (obj && obj.v === SYNCRETRO_CACHE_VERSION && obj.entries)
		cache.entries = obj.entries;   /* a version bump invalidates by ignoring */
	return cache;
}

// Written only when syncretro_discover() changed something. temp-file + rename, so a
// reader on the other host never sees a half-written file.
function syncretro_cache_flush(cache)
{
	var tmp, f;

	if (!cache || !cache.dirty)
		return true;                   /* nothing to write is a success */

	tmp = cache.path + ".tmp";
	f = new File(tmp);
	if (!f.open("w"))
		return false;
	f.write(JSON.stringify({ v: SYNCRETRO_CACHE_VERSION, entries: cache.entries }));
	f.close();

	if (file_exists(cache.path))
		file_remove(cache.path);       /* rename-over fails on Windows */
	if (!file_rename(tmp, cache.path)) {
		file_remove(tmp);
		return false;
	}
	cache.dirty = false;
	return true;
}

function syncretro_excluded(name, excludes)
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
// marker is not part of the title (syncretro_parse_title strips it), so both
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
function syncretro_dump_rank(filename)
{
	var name = syncretro_strip_ext(String(filename));
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
function syncretro_dump_better(a, b)
{
	var ra = syncretro_dump_rank(a.name), rb = syncretro_dump_rank(b.name);

	if (ra !== rb)
		return ra < rb;
	return a.name < b.name;
}

function syncretro_collapse_variants(roms)
{
	var best  = {};        // key -> best entry seen so far
	var order = [];        // keys in first-seen order, so survivors keep it
	var out   = [];
	var i, r, key;

	for (i = 0; i < roms.length; i++) {
		r = roms[i];
		/* The cataloging tags are in the key on purpose, exactly as the publisher
		 * is: "Contra (USA)" and "Contra (Europe)" are two different dumps (NTSC
		 * and a 50 Hz PAL conversion), not two rips of one, and BOTH must survive
		 * -- leave them out and one is silently hidden from the picker. */
		key = r.title.toLowerCase() + "|" + r.year + "|" + r.publisher.toLowerCase()
		    + "|" + (r.tags || []).join(",").toLowerCase();
		if (!best.hasOwnProperty(key)) {
			best[key] = r;
			order.push(key);
			continue;
		}
		if (syncretro_dump_better(r, best[key]))
			best[key] = r;
	}
	for (i = 0; i < order.length; i++)
		out.push(best[order[i]]);
	return out;
}

// --- display titles -----------------------------------------------------------
//
// The tags are off the title (that was the point), which leaves one case where
// the player would be worse off than before: a sysop holding both "Contra (USA)"
// and "Contra (Europe)" now sees two identical lines and no way to tell which is
// which. So the tag comes BACK -- but only onto the entries that actually collide,
// and not onto the 99% that don't. Sets `label`, which is what the picker draws.
//
// The same rule fixes an older wart it did not cause: two different PORTS of one
// game -- "Pac-Man (1983) (Atarisoft)" and "Pac-Man (1983) (Intv Corp)" -- are
// deliberately both kept by the collapse above, and have always drawn as two
// identical "Pac-Man (1983)" lines. When a colliding entry has no tag to tell it
// apart, its publisher does the job.
function syncretro_disambiguate(roms)
{
	var count = {};
	var i, r, key, mark;

	for (i = 0; i < roms.length; i++) {
		key = roms[i].title.toLowerCase();
		count[key] = (count[key] || 0) + 1;
	}
	for (i = 0; i < roms.length; i++) {
		r = roms[i];
		key = r.title.toLowerCase();
		if (count[key] < 2) {
			r.label = r.title;
			continue;
		}
		mark = (r.tags && r.tags.length) ? r.tags.join(", ") : (r.publisher || "");
		r.label = mark ? r.title + " (" + mark + ")" : r.title;
	}
	return roms;
}

// `rules` is an syncretro_rules() object -- the console's extensions, size band, BIOS
// images and exclusions. `cache` is optional: an syncretro_cache_open() handle, or
// omitted/null to hash every candidate the way this did before the cache.
function syncretro_discover(roms_dir, rules, cache)
{
	var seen = {};         // content key -> index into out
	var out  = [];
	var dir  = backslash(roms_dir);
	var kept = cache ? {} : null;   // the cache we will have after this scan

	rules.ext.forEach(function (ext) {
		directory(dir + "*." + ext).forEach(function (path) {
			var name = file_getname(path);
			var size = file_size(path);
			var full, key, prev, parsed, mtime, ent;

			if (size < rules.min_size || size > rules.max_size)
				return;                       /* not a cartridge */
			if (syncretro_excluded(name, rules.exclude))
				return;
			if (rules.bios_names.indexOf(name.toLowerCase()) >= 0)
				return;                       /* a BIOS image, by its default name */

			/* The one place a ROM is opened. Everything above this line is a
			 * stat at worst, which is why the size/name gates come first. */
			if (cache) {
				mtime = file_date(path);
				ent   = cache.entries[name];
				if (ent && ent.s === size && ent.m === mtime
				    && typeof ent.h === "string" && ent.h.length === 32) {
					full = ent.h;             /* hit: no open */
					cache.reused++;
				} else {
					full = syncretro_file_md5(path, 0);
					cache.hashed++;
				}
			} else {
				full = syncretro_file_md5(path, 0);
			}
			if (full === "")
				return;                       /* unreadable: not our problem */

			/* Cached BEFORE the BIOS-by-content check, so exec.bin/grom.bin are
			 * rejected on a warm run without being re-read either. */
			if (cache)
				kept[name] = { s: size, m: mtime, h: full };

			if (rules.bios_md5.indexOf(full) >= 0)
				return;                       /* a BIOS image, whatever its name */

			/* Content identity: size plus the FULL-FILE hash, not a 4 KB prefix.
			 * A prefix collides across genuinely different cartridges of the same
			 * size -- "Pac-Man (1983) (Atarisoft).int" and "Pac-Man (1983) (Intv
			 * Corp).int" are two different ports (24576 bytes each) that share
			 * their first 4 KB but differ later in the file; keying on the
			 * prefix silently drops one of two real games. The full hash costs
			 * nothing extra: it is already computed above for the BIOS check,
			 * so reusing it for the dedupe key is free. Two names for the same
			 * full-file bytes are one game; keep the more descriptive name. */
			key = size + ":" + full;
			if (seen.hasOwnProperty(key)) {
				prev = out[seen[key]];
				if (name.length > prev.name.length) {
					prev.name = name;
					prev.path = path;
					parsed = syncretro_parse_title(name);
					prev.title     = parsed.title;
					prev.year      = parsed.year;
					prev.publisher = parsed.publisher;
					prev.tags      = parsed.tags;
				}
				return;
			}

			parsed = syncretro_parse_title(name);
			seen[key] = out.length;
			out.push({
				path:      path,
				name:      name,
				title:     parsed.title,
				year:      parsed.year,
				publisher: parsed.publisher,
				tags:      parsed.tags,
				size:      size
			});
		});
	});

	/* Replace the cache wholesale with what this scan actually saw, so a ROM the
	 * sysop deleted does not linger in it forever. Dirty if anything was hashed
	 * OR if the set of cached files changed (a pure deletion hashes nothing). */
	if (cache) {
		if (cache.hashed > 0
		    || Object.keys(kept).length !== Object.keys(cache.entries).length)
			cache.dirty = true;
		cache.entries = kept;
	}

	out = syncretro_disambiguate(syncretro_collapse_variants(out));

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

function syncretro_plays_path(data_dir)
{
	// Strip any trailing separator ("/" or, from a backslash()'d path, "\") and
	// rebuild with "/", so the result is the same whether or not data_dir came
	// with a trailing separator, and on every platform (Windows accepts "/").
	// backslash() alone would not do: it appends the OS-NATIVE separator only
	// when one is missing, so "<dir>" vs "<dir>/" would yield "<dir>\syncretro/..."
	// vs "<dir>/syncretro/..." on Windows -- the same path spelled two ways.
	var dir = String(data_dir).replace(/[\\\/]+$/, "") + "/syncretro/";

	if (!file_isdir(dir))
		mkpath(dir);
	return dir + "plays.jsonl";
}

// One line, appended under an exclusive open. Returns false rather than throwing:
// a lost play is not worth failing a player's session over.
function syncretro_log_play(path, rec)
{
	var f = new File(path);

	if (!f.open("a"))
		return false;
	f.writeln(JSON.stringify(rec));
	f.close();
	return true;
}

// `id` is optional: keep only that console's plays. One append-only log serves
// every console, so the board has to filter -- otherwise the NES lobby would
// show Astrosmash in its Top Played.
//
// A record with NO console field predates the field, when the Intellivision was
// the only console there was. It IS an Intellivision play, and is counted as one.
function syncretro_read_plays(path, id)
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
		if (!rec || !rec.rom)
			continue;
		if (id && (rec.console || "intv") !== id)
			continue;
		out.push(rec);
	}
	return out;
}

function syncretro_top_played(plays, n)
{
	var counts = {};
	var out = [];
	var rom, parsed;

	plays.forEach(function (p) {
		counts[p.rom] = (counts[p.rom] || 0) + 1;
	});
	for (rom in counts) {
		if (!counts.hasOwnProperty(rom))
			continue;
		parsed = syncretro_parse_title(rom);
		out.push({ rom: rom, title: parsed.title, tags: parsed.tags,
		           count: counts[rom] });
	}
	/* Same collision rule as the picker: two regions of one game played by two
	 * players are two lines on the board, and must not read as one game twice. */
	syncretro_disambiguate(out);

	out.sort(function (a, b) {
		if (a.count !== b.count)
			return b.count - a.count;
		return a.rom < b.rom ? -1 : (a.rom > b.rom ? 1 : 0);   /* stable, name-ordered */
	});
	return out.slice(0, n);
}

function syncretro_last_play(plays, user_number)
{
	var best = null;

	plays.forEach(function (p) {
		if (p.user === user_number && (best === null || p.t > best.t))
			best = p;
	});
	return best;
}

// --- layout ------------------------------------------------------------------
//
// Follows exec/xtrn_sec.js: multi-column above 80 columns, one column below it,
// reading DOWN each column. The column count is derived rather than hardcoded at
// two, so a 132-column terminal gets three and nobody has to think about it.

function syncretro_columns(screen_cols, cell_width)
{
	var n;

	if (screen_cols < 80)
		return 1;                      /* xtrn_sec.js's rule, and a good one */
	n = Math.floor(screen_cols / (cell_width + 1));
	return n < 1 ? 1 : n;
}

function syncretro_page_rows(screen_rows, header_rows, footer_rows)
{
	var n = screen_rows - header_rows - footer_rows;

	return n < 1 ? 1 : n;
}

function syncretro_paginate(items, per_page)
{
	var pages = [];
	var i;

	if (per_page < 1)
		per_page = 1;
	for (i = 0; i < items.length; i += per_page)
		pages.push(items.slice(i, i + per_page));
	return pages;
}

// One list cell, in xtrn_sec.js's look (XtrnProgLstFmt): a bright-cyan 3-digit
// number, a \xb3 (CP437 vertical bar), then the cyan title (+year). `width` is
// the cell's VISIBLE width; "%N.Ns" pads/clips the name to a fixed column count
// so the grid lines up even though the returned string carries \1x attribute
// codes (which take no column). The number+bar prefix is 6 visible columns.
function syncretro_cell(index, rom, width)
{
	var label = (rom.label || rom.title) + (rom.year ? " (" + rom.year + ")" : "");
	var namew = width - 6;

	if (namew < 1)
		namew = 1;
	return format("\1h\1c%3u \xb3 \1n\1c%-" + namew + "." + namew + "s\1n", index, label);
}

// --- platform / target token ------------------------------------------------
//
// The sub-directory (under the door dir) that holds the platform-specific NATIVE
// artifacts: the door binary and the libretro core. Keeping them in a
// per-target sub-dir rather than loose in the door dir is what lets one shared
// install serve several hosts at once -- e.g. a BBS spanning a Linux box and a
// Windows host on the same mount (Vertrauen), or two *nixes of different
// architecture. A bare "syncretro" / "freeintv_libretro.so" would collide
// between them. The BIOS and cartridges are platform-independent and stay at
// the door root.
//
// syncretro_target(platform, architecture) is the sub-dir name (or "" = flat door dir):
//   Windows       ""                      (flat: .exe/.dll never collide flat)
//   everything    "<os>-<arch>"           "linux-x64", "linux-arm64",
//   else          e.g.                     "freebsd-x64", "darwin-arm64"
//
// The lobby (lobby.js), the core installer (getcore.js) AND the binary installer
// (deploy.js) all call syncretro_target(), so they agree on the sub-dir by construction
// -- that is exactly why deploy is a jsexec script and not a shell/batch pair: a
// shell deploy would have to re-derive the token from `uname`, whose spellings
// differ from system.platform/system.architecture, and drift out of agreement.
//
// The normalization here is therefore about producing a STABLE, CLEAN,
// filesystem-safe token from Synchronet's own raw values, not about matching any
// external tool:
//   syncretro_platform() -- the OS half:
//     * Windows: the door is Win32-only by design (one binary covers a Win32 AND
//       a Win64 host), but system.platform is "Win64" on a 64-bit Synchronet.
//       Both collapse to "win32" -- and syncretro_target() maps that to "" (flat), since
//       a .exe/.dll never collides with a *nix name and there is one Win32 target.
//     * macOS: system.platform is "macOS"/"MacOSX" -> one stable "darwin".
//     * Linux/*BSD: lower-cased, with any stray non-alnum char (the "/" in
//       "GNU/Hurd") stripped so it cannot nest a path.
//   syncretro_arch() -- the CPU half, bucketed so equivalent spellings don't fragment
//     into separate dirs (i386/i686 -> x86, armv8/aarch64 -> arm64): x64 / x86 /
//     arm64 / arm, else the token verbatim (riscv64, ppc64, ...).
//
// All three take their inputs as arguments (UI-free, so the tests drive them).
// The core's file extension follows from syncretro_platform() (win32->dll, darwin->dylib,
// else->so), so the lobby, getcore.js and deploy all agree on where the core lives.
// The platform/arch/target token comes from exec/load/door_deploy.js, shared with
// every door's deploy.js -- because the deploy step and the lobby MUST agree on
// where the binary lives, and two copies of that rule is one copy too many. These
// stay as syncretro_-named wrappers so the callers (and the tests) do not care.
function syncretro_platform(platname)          { return door_platform(platname); }
function syncretro_arch(archname)              { return door_arch(archname); }
function syncretro_target(platname, archname)  { return door_target(platname, archname); }

// The libretro core's shared-library extension for a given syncretro_platform() token.
function syncretro_core_ext(plat)
{
	return plat == "win32" ? "dll" : plat == "darwin" ? "dylib" : "so";
}
