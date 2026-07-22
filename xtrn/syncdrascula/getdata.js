// getdata.js -- fetch the freeware Drascula: The Vampire Strikes Back game
// data for the syncdrascula door: the base game (Packet.001) plus, optionally,
// the CD music soundtrack.
//
// Drascula: The Vampire Strikes Back was released as freeware by its author
// (Alcachofa Soft / Emilio de Paz) and ScummVM hosts it, so this script
// downloads it itself -- straight from ScummVM's own distribution point --
// rather than this repository bundling copyrighted data or a sysop supplying a
// copy.
//
// Two archives, both unpacked into THIS door directory (a single flat build --
// unlike Flight of the Amazon Queen's talkie/floppy split):
//
//   drascula-1.0.zip           -- REQUIRED. The game itself: Packet.001, the
//                                 one resource file the drascula engine needs.
//                                 English text; the engine's own Adlib music
//                                 plays even without the soundtrack below.
//   drascula-audio-mp3-2.0.zip  -- OPTIONAL. The CD-audio soundtrack as MP3,
//                                 kept in an audio/ subdirectory (ScummVM's CD-
//                                 track convention: audio/trackNN.mp3). The MP3
//                                 pack is chosen over the OGG/FLAC ones because
//                                 build.sh force-enables libmad, so MP3 is
//                                 guaranteed to decode in every SyncSCUMM build
//                                 (Vorbis/FLAC are only autodetected). Absent
//                                 it, Drascula still plays with Adlib music.
//
// Integrity: each archive is verified before it is unpacked against the PINNED
// sha256 on its build below -- the value ScummVM publishes alongside it --
// using CryptContext(CryptContext.ALGO.SHA2) (cryptlib's SHA-2 context defaults
// to 256-bit output), the same real hash check the syncqueen/syncbass fetchers
// do.
//
// Pinned rather than fetched at run time, which is what this did before. The
// mirror fallback means an archive can arrive from a second host, and a hash
// retrieved from whichever host served the file proves nothing about it. A pin
// in git is checked once, by a person, and an archive that changed upstream is
// REJECTED rather than silently installed.
//
// If a download fails or fails that check, exec/load/xtrn_mirror.js retries
// against Synchronet's asset mirror before giving up.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/syncdrascula/getdata.js
//
// Idempotent: an archive whose marker file already exists (Packet.001 for the
// game, audio/track1.mp3 for the soundtrack) is left alone and reported
// "present". SpiderMonkey 1.8.5 compatible (no modern ES) -- runs unchanged on
// Windows, which a POSIX shell script could not.
//
// Copyright (C) 2026 Rob Swindell / syncdrascula.  GPL-2.0.

load("http.js");
load("xtrn_mirror.js");
load("sha256.js");

var BASE = "https://downloads.scummvm.org/frs/extras/" +
           "Drascula_%20The%20Vampire%20Strikes%20Back";

// The archives ScummVM hosts.  Both unpack into the door directory itself (no
// per-build subdir).  with_path preserves the archive's internal folders
// (the soundtrack keeps its audio/ subdir) or flattens them (the game archive
// is flat); file restricts extraction to just the entries we want, so the two
// archives' stray readme.txt / changelog files never land here or collide.
// A required=false archive is best-effort: its failure is warned, not fatal.
var BUILDS = [
	{ zip: "drascula-1.0.zip",           marker: "Packet.001",
	  with_path: false, file: "Packet.001",
	  label: "game (required)",          required: true,
	  sha256: "b731f6cb5a22ba8b4c3b3362f570b9a10a67b6cb0b395394b19a94b36e4e42de" },
	{ zip: "drascula-audio-mp3-2.0.zip", marker: "audio/track1.mp3",
	  with_path: true,  file: "audio/*.mp3",
	  label: "CD soundtrack (optional)", required: false,
	  sha256: "32bdb6d5163e538ffe20f6320e20e0b24050823be516775364be629ab1b52eee" }
];

// The door's own directory (where this script lives).  js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// Fetch and unpack one archive into the door directory.  Returns true on
// success (including "already present"), false on any failure (never throws --
// a failed optional archive is reported and the game archive is unaffected).
function fetch_build(b, root)
{
	if (file_exists(root + b.marker)) {
		print(b.label + ": present");
		return true;
	}

	var url = BASE + "/" + b.zip;
	var tmp = backslash(system.temp_dir) + b.zip;

	print(b.label + ": downloading " + b.zip + " ...");
	var n = xtrn_mirror_download(url, tmp, {
		verify: function(p) {
			var got = sha256_of_file(p);
			if (got == b.sha256)
				return true;
			print("  ! sha256 MISMATCH for " + b.zip);
			print("    expected: " + b.sha256);
			print("    got:      " + got);
			return false;
		}
	});
	if (!n) {
		print("  ! could not fetch " + b.zip);
		return false;
	}
	print("  downloaded " + n + " bytes; sha256 verified");

	print("  extracting into " + root + " ...");
	try {
		// Archive.extract(outdir, with_path, overwrite, max_files, file_list...)
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, b.with_path, true, 0, b.file);
		print("  extracted " + got_n + " file(s)");
	} catch (e) {
		print("  ! extraction failed: " + e);
		file_remove(tmp);
		return false;
	}
	file_remove(tmp);

	if (!file_exists(root + b.marker)) {
		print("  ! extracted, but " + b.marker + " is still missing -- something's off");
		return false;
	}
	return true;
}

// Print the basenames present in a directory (files only, not sub-dirs).
function list_dir(dir, label)
{
	var all = directory(dir + "*"), i, names = [];
	for (i = 0; i < all.length; i++) {
		if (all[i].charAt(all[i].length - 1) == "/")   // GLOB_MARK: a directory
			continue;
		names.push(file_getname(all[i]));
	}
	print(label + ": " + (names.length ? names.sort().join(" ") : "(empty)"));
}

function main()
{
	var root = door_dir();
	print("Drascula: The Vampire Strikes Back (syncdrascula) game-data fetcher");
	print("  door directory: " + root);

	var i, hard_fail = false;
	for (i = 0; i < BUILDS.length; i++) {
		if (fetch_build(BUILDS[i], root))
			continue;
		if (BUILDS[i].required)
			hard_fail = true;
		else
			print("  (optional -- Drascula still plays without it)");
	}

	print("");
	list_dir(root, "game dir");
	list_dir(backslash(root + "audio"), "audio");

	if (hard_fail) {
		print("");
		print("The required game data failed to fetch -- re-run to retry:");
		print("    jsexec ../xtrn/syncdrascula/getdata.js");
		return 1;
	}
	return 0;
}

if (typeof SYNCDRASCULA_GETDATA_NO_MAIN == "undefined")
	exit(main());
