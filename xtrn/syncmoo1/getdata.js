// getdata.js -- install Master of Orion 1 (MoO1 v1.3) LBX game data for the
// syncmoo1 door from a copy the SYSOP already owns and has placed in the door
// directory.  It does NOT download anything.
//
// 1oom is only the engine; the MoO1 data files are commercial content
// (c) Simtex / MicroProse, NOT shipped with this door -- supplying a
// legally-owned copy is the sysop's responsibility (see README.md and
// DESIGN.md section 15).  This script only extracts/copies from what the sysop
// provides, looking (in the door directory) for:
//
//   * loose *.lbx files already dropped in the door dir (nothing to do), or
//     *.lbx found in a subdirectory -- e.g. an extracted GOG/DOS game folder --
//     which are copied up into the door dir; or
//   * an archive (.zip / .7z / .tar / ... ) dropped in the door dir that
//     contains the *.lbx files -- the .lbx members are extracted.
//
// Installed names are lower-cased (DOS/GOG copies are often FONTS.LBX; 1oom's
// loader tries lower- AND upper-case, but lower-case is the tidy norm on a
// case-sensitive filesystem, and avoids mixed-case "Fonts.lbx" misses).
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time after dropping a copy in place:
//
//     jsexec ../xtrn/syncmoo1/getdata.js
//
// Idempotent: LBX files already present in the door dir are left alone, so
// re-running only fills in what is still missing.  SpiderMonkey 1.8.5
// compatible (no modern ES).
//
// Copyright (C) 2026 Rob Swindell / syncmoo1.  GPL-2.0.

// The complete MoO1 v1.3 LBX data set 1oom can use (lower-case).  The rest are
// gameplay/audio/cinematics; the two in REQUIRED below are hard prerequisites.
var LBX_SET = [
	"backgrnd.lbx", "colonies.lbx", "council.lbx", "design.lbx", "diplomat.lbx",
	"embassy.lbx", "eventmsg.lbx", "firing.lbx", "fonts.lbx", "help.lbx",
	"intro.lbx", "intro2.lbx", "introsnd.lbx", "landing.lbx", "missile.lbx",
	"music.lbx", "names.lbx", "nebula.lbx", "newscast.lbx", "planets.lbx",
	"research.lbx", "screens.lbx", "ships.lbx", "ships2.lbx", "soundfx.lbx",
	"space.lbx", "spies.lbx", "starmap.lbx", "starview.lbx", "techno.lbx",
	"v11.lbx", "vortex.lbx", "winlose.lbx"
];

// The files 1oom hard-requires; without either, the door cannot start at all:
//   fonts.lbx  what lbxfile_find_dir() looks for to locate the data directory
//              (1oom/src/lbx.c) -- absent, the door reports "could not find
//              the LBX files".
//   v11.lbx    the marker for MoO1 *v1.3*, checked by ui_late_init()
//              (1oom/src/ui/classic/uiclassic.c); without it the door aborts
//              with "V11.LBX not found!".
// So having fonts.lbx alone is NOT enough to declare the install playable.
//
// v11.lbx is a trap worth knowing about: in the shipped v1.3 releases (Steam,
// GOG) it is the ONE data file whose name is lower-case -- every other LBX is
// upper-case.  So a hand copy done with a "*.LBX" glob onto a case-sensitive
// filesystem silently leaves it behind, producing a door directory that looks
// complete but cannot start.  (This script is immune: it matches .lbx
// case-insensitively and lower-cases what it installs.)
var REQUIRED = ["fonts.lbx", "v11.lbx"];

function missing_required(have)
{
	return REQUIRED.filter(function (n) { return !have[n]; });
}

// Archive file extensions we'll try to pull *.lbx members out of.  The Archive
// object is libarchive-backed and reads all of these; any other (non-.lbx) file
// in the door dir is ignored.  Longest-suffix forms are matched too (.tar.gz).
var ARCHIVE_EXT = [
	".zip", ".7z", ".rar", ".arj", ".lzh", ".lha", ".arc", ".zoo", ".cab",
	".iso", ".cpio", ".tar", ".tgz", ".tbz", ".tbz2", ".txz",
	".tar.gz", ".tar.bz2", ".tar.xz", ".gz", ".bz2", ".xz"
];

// The door's own directory (where this script and the binary live).  js.exec_dir
// is the running SCRIPT's own dir -- correct whether launched by the installer
// or by hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

function base_lc(path)  { return String(file_getname(path)).toLowerCase(); }
function is_lbx(name)   { return /\.lbx$/i.test(String(name)); }

function has_archive_ext(name)
{
	var lc = String(name).toLowerCase(), i, e;
	for (i = 0; i < ARCHIVE_EXT.length; i++) {
		e = ARCHIVE_EXT[i];
		if (lc.length >= e.length && lc.substr(lc.length - e.length) == e)
			return true;
	}
	return false;
}

// Map of lower-cased *.lbx basenames present directly in the door dir.
function present_lbx(dir)
{
	var have = {}, all = directory(dir + "*"), i, b;
	for (i = 0; i < all.length; i++) {
		if (all[i].charAt(all[i].length - 1) == "/")   // GLOB_MARK: a directory
			continue;
		b = base_lc(all[i]);
		if (is_lbx(b))
			have[b] = true;
	}
	return have;
}

// Recursively gather *.lbx file paths under a directory (bounded depth, so a
// deep tree can't run away).  Used to find LBX inside a subfolder the sysop
// extracted their game into.
function find_loose_lbx(dir, depth, out)
{
	if (depth < 0)
		return;
	var all = directory(dir + "*"), i, e;
	for (i = 0; i < all.length; i++) {
		e = all[i];
		if (e.charAt(e.length - 1) == "/")
			find_loose_lbx(e, depth - 1, out);
		else if (is_lbx(e))
			out.push(e);
	}
}

// Copy one source *.lbx into the door dir under its lower-cased name, unless we
// already have that name.  Returns true if a new file was placed.
function place_copy(src, dir, have)
{
	var name = base_lc(src);
	if (have[name])
		return false;
	var dst = dir + name;
	if (file_copy(src, dst)) {
		have[name] = true;
		print("  copied " + name + " (from " + src + ")");
		return true;
	}
	print("  ! failed to copy " + src);
	return false;
}

// Extract the *.lbx members of one archive into the door dir (lower-cased),
// skipping any we already have.  Returns the count newly installed.  A file
// that isn't a readable archive is skipped quietly.
function extract_from_archive(path, dir, have)
{
	var ar, entries, i, name, lc, produced, target, got = 0;

	try { ar = new Archive(path); }
	catch (e) { return 0; }   // not an archive we can open

	try { entries = ar.list(false, "*.lbx"); }   // case-insensitive, any subpath
	catch (e2) {
		print("  ! cannot read archive " + file_getname(path) + ": " + e2);
		return 0;
	}

	for (i = 0; i < entries.length; i++) {
		name = entries[i].name;
		if (!name || entries[i].type == "directory" || !is_lbx(name))
			continue;
		lc = base_lc(name);
		if (have[lc])
			continue;
		try {
			// with_path=false flattens the member to its basename in `dir`.
			ar.extract(dir, false, true, 0, name);
			produced = dir + file_getname(name);   // extract keeps the member's case
			target   = dir + lc;
			if (produced != target && file_exists(produced)) {
				if (file_exists(target)) file_remove(produced);
				else                     file_rename(produced, target);
			}
			if (file_exists(target)) {
				have[lc] = true;
				got++;
				print("  extracted " + lc + " (from " + file_getname(path) + ")");
			}
		} catch (e3) {
			print("  ! failed to extract " + name + ": " + e3);
		}
	}
	return got;
}

function missing_of(have)
{
	return LBX_SET.filter(function (n) { return !have[n]; });
}

function main()
{
	var dir = door_dir();
	print("Master of Orion (syncmoo1) game-data installer");
	print("  door directory: " + dir);

	if (!file_exists(dir + "syncmoo1") && !file_exists(dir + "syncmoo1.exe"))
		print("  note: the syncmoo1 binary is not here yet -- build & deploy it "
		    + "(build.sh then deploy.sh) so the door can run once data is in place.");

	var have    = present_lbx(dir);
	var missing = missing_of(have);

	if (!missing.length) {
		print("All " + LBX_SET.length + " MoO1 LBX files are already present -- nothing to do.");
		return 0;
	}
	print("  present: " + (LBX_SET.length - missing.length) + "/" + LBX_SET.length
	    + " LBX files; looking for a copy to install the other " + missing.length + " ...");

	var i, sub, loose = [], top = directory(dir + "*");

	// 1) *.lbx the sysop extracted into a subfolder (e.g. a GOG/DOS game dir).
	for (i = 0; i < top.length; i++) {
		sub = top[i];
		if (sub.charAt(sub.length - 1) == "/")
			find_loose_lbx(sub, 3, loose);
	}
	for (i = 0; i < loose.length; i++)
		place_copy(loose[i], dir, have);

	// 2) archives dropped directly in the door dir.
	for (i = 0; i < top.length; i++) {
		if (top[i].charAt(top[i].length - 1) == "/")
			continue;
		if (has_archive_ext(file_getname(top[i])))
			extract_from_archive(top[i], dir, have);
	}

	// Re-evaluate what we ended up with.
	have    = present_lbx(dir);
	missing = missing_of(have);
	var got = LBX_SET.length - missing.length;

	if (!missing.length) {
		print("Success: all " + LBX_SET.length + " MoO1 LBX files installed.");
		return 0;
	}

	var need = missing_required(have);
	if (need.length) {
		print("");
		if (have["fonts.lbx"] && !have["v11.lbx"]) {
			// Everything but v11.lbx.  Overwhelmingly this is the lower-case
			// trap, not a pre-v1.3 copy: v11.lbx is the only lower-case-named
			// LBX in the shipped v1.3 releases, so a hand "cp *.LBX" on a
			// case-sensitive filesystem drops exactly this one file.  Lead
			// with that; the door would otherwise just abort at launch with
			// 1oom's bare "V11.LBX not found!".
			print("Missing v11.lbx -- everything else is here.  The door cannot start");
			print("without it (1oom checks it as the Master of Orion v1.3 marker).");
			print("");
			print("v11.lbx is the ONE data file that ships with a lower-case name; all");
			print("the others are upper-case.  A copy made with a \"*.LBX\" pattern will");
			print("silently skip it.  Check the copy you took the data from -- it is");
			print("very likely still sitting there.  Then either copy it in by hand or");
			print("re-run:  jsexec ../xtrn/syncmoo1/getdata.js");
			print("");
			print("If v11.lbx really is absent at the source, that copy predates v1.3,");
			print("which is the only version 1oom can run: apply the official v1.3");
			print("update, or use a v1.3 release (Steam, or GOG's \"Master of Orion");
			print("1+2\").");
		} else {
			print("No usable Master of Orion 1 data found (missing: " + need.join(" ") + ").");
			print("");
			print("Master of Orion is commercial content (c) Simtex / MicroProse and is");
			print("NOT shipped with this door.  Supply your own legally-owned MoO1 v1.3");
			print("copy (sold as \"Master of Orion 1+2\" on GOG, among others), then put");
			print("ONE of these into the door directory:");
			print("    " + dir);
			print("  - the loose *.lbx files, or");
			print("  - a .zip / archive that contains them, or");
			print("  - an extracted GOG/DOS game folder (a subdirectory holding *.lbx),");
			print("and re-run:  jsexec ../xtrn/syncmoo1/getdata.js");
		}
		return 1;
	}

	// Both prerequisites present: the door starts, but the set is incomplete.
	print("Installed " + got + "/" + LBX_SET.length + " LBX files.  Still missing:");
	print("    " + missing.join(" "));
	print("The door can start (fonts.lbx and v11.lbx are present) but some content");
	print("may be absent.  Add the missing files to the door dir and re-run to");
	print("complete the set.");
	return 0;
}

if (typeof SYNCMOO1_GETDATA_NO_MAIN == "undefined")
	exit(main());
