// getdata.js -- fetch the freeware fan game "Space Quest 0: Replicated" for
// the spacequest0 door, straight into the door directory.
//
// PROVENANCE / LICENSING -- read this before changing the source URL.
// Space Quest 0: Replicated (Jeff Stewart, 2003) is a fan-made AGI adventure,
// a prequel to Sierra's Space Quest series. It is NOT one of the officially-
// relicensed freeware titles (unlike Beneath a Steel Sky / Flight of the
// Amazon Queen, whose rights holders released them for redistribution). The
// author's page (https://www.wiw.org/~jess/replicated.html) carries no
// redistribution grant of its own and notes: "Roger Wilco and related
// materials are (c) Sierra On-Line. Space Quest is a registered trademark."
//
// So this repository deliberately does NOT bundle or re-host the game. This
// script automates what a sysop would do by hand: download the ZIP from the
// AUTHOR'S OWN host, verify it byte-for-byte, and unpack it into this door
// directory (which is git-ignored). The bytes never live in our repo; the
// author's site is the distributor. Same discipline as the syncnes/syncretro
// getroms helpers: "being downloadable is not a licence" -- fetch from source,
// pin + verify, never re-host. A sysop who does not want to serve a fan game
// that builds on Sierra's IP simply does not run this step.
//
// The AGI engine (added to the ScummVM build alongside sci) plays it; ScummVM
// detects the data as game id "sq0". AGI is a 16-color, keyboard/parser game
// -- no mouse needed.
//
// Integrity: the author publishes no checksum, so the expected SHA-256 and
// byte size are PINNED here (computed by hand from v1.04, rep_104.zip). If the
// author ever re-releases under the same filename, this hash will stop
// matching and the fetch will fail loudly rather than install something
// unverified -- update the pin deliberately. archive.org also mirrors this
// exact file (item SpaceQuest0Replicated) should the author's host go down;
// the sha256 pin lets a sysop verify a copy from anywhere is identical.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/spacequest0/getdata.js
//
// Idempotent: if LOGDIR (an AGI game file) already exists the game is left
// alone and reported "present". SpiderMonkey 1.8.5 compatible (no modern ES).
//
// Copyright (C) 2026 Rob Swindell / spacequest0 (this fetcher script only).
// GPL-2.0. The game data it downloads is Jeff Stewart's, not ours.

load("http.js");
load("xtrn_mirror.js");
load("sha256.js");

// The one pinned download.  url is the author's own host; sha256 + size are
// verified after download; marker (LOGDIR, a core AGI resource file) both
// tells us the unpack succeeded and makes re-runs idempotent.
var GAME = {
	url:    "https://www.wiw.org/~jess/download/rep_104.zip",
	zip:    "rep_104.zip",
	size:   673342,
	sha256: "c2d46dbf644ff215d515a3c3d5253981ad8c1da1d59bf2a7bd55be7102ae3525",
	marker: "LOGDIR",
	label:  "Space Quest 0: Replicated v1.04"
};

// The door's own directory (where this script lives).  js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);
}

function main()
{
	var root = door_dir();
	print("Space Quest 0: Replicated (spacequest0) game-data fetcher");
	print("  door directory: " + root);

	if (file_exists(root + GAME.marker)) {
		print(GAME.label + ": present");
		return 0;
	}

	var tmp = backslash(system.temp_dir) + GAME.zip;
	print(GAME.label + ": downloading " + GAME.zip + " from " + GAME.url + " ...");
	// Verify size + pinned sha256 before trusting the archive; a
	// failure here retries against the mirror.
	var n = xtrn_mirror_download(GAME.url, tmp, {
		verify: function(p) {
			var got_size = file_size(p);
			var got;

			if (got_size != GAME.size) {
				print("  ! size MISMATCH: expected " + GAME.size
				    + ", got " + got_size);
				return false;
			}
			got = sha256_of_file(p);
			if (got == GAME.sha256)
				return true;
			print("  ! sha256 MISMATCH");
			print("    expected: " + GAME.sha256);
			print("    got:      " + got);
			return false;
		}
	});
	if (!n) {
		print("  ! could not fetch " + GAME.zip);
		return 1;
	}
	print("  downloaded " + n + " bytes; size + sha256 verified");

	print("  extracting into " + root + " ...");
	try {
		var ar = new Archive(tmp);
		// Extract ONLY the AGI game files, by name. The ZIP also bundles a
		// DOS/Windows AGI interpreter (Space Quest - Replicated.exe + SDL.dll +
		// font_*.nbf) that ScummVM does not use. One of those, "Space Quest -
		// Replicated.exe", also fails the Archive object's safe-filename guard --
		// not because it is an .exe, but because its SPACES are outside
		// SAFEST_FILENAME_CHARS (filedat.c), a path/shell-safety check. The
		// file-list args (after max_files=0) restrict extraction to these
		// wildcards, so the interpreter files are never reached. VOL.* covers
		// VOL.0 (and any VOL.1+ a future release might split into).
		var got_n = ar.extract(root, false, true, 0,
			"LOGDIR", "PICDIR", "VIEWDIR", "SNDDIR", "OBJECT", "WORDS.TOK", "VOL.*");
		print("  extracted " + got_n + " file(s)");
	} catch (e) {
		print("  ! extraction failed: " + e);
		file_remove(tmp);
		return 1;
	}
	file_remove(tmp);

	if (!file_exists(root + GAME.marker)) {
		print("  ! extracted, but " + GAME.marker + " is missing -- something's off");
		return 1;
	}
	print("");
	print("installed: the AGI game files (LOGDIR, PICDIR, VIEWDIR, SNDDIR,");
	print("OBJECT, WORDS.TOK, VOL.*). ScummVM plays these as game id 'sq0'.");
	print("The ZIP's bundled DOS interpreter (.exe/.dll/.nbf) was skipped.");
	return 0;
}

if (typeof SPACEQUEST0_GETDATA_NO_MAIN == "undefined")
	exit(main());
