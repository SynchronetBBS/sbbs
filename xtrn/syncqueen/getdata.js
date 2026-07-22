// getdata.js -- fetch the freeware Flight of the Amazon Queen game data (both
// builds) for the syncqueen door: talkie/ (speech) and floppy/ (text).
//
// Unlike Master of Orion (syncmoo1/getdata.js), FOTAQ is officially freeware
// -- John Passfield & Steve Stamatiadis released it for free redistribution
// -- so this script downloads it itself, straight from ScummVM's own
// hosting, rather than expecting the sysop to supply a copy.
//
// "FOTAQ_Talkie-1.0.zip" (an earlier draft's guess) exists on ScummVM's
// server but has no matching .sha256 published, so there is no publisher-
// attested value to pin -- this uses FOTAQ_Talkie-1.1.zip instead, which does
// have one, and its published sum is the constant below. Verified by
// hand at authoring time: both zips download, both sha256s check out, and
// neither build needs ScummVM's external queen.tbl engine-data file (the
// Talkie build's queen.1c is a self-contained "rebuilt" resource file --
// ScummVM's own compression_queen tool embeds the lookup table inline --
// and the Floppy build's queen.1 is byte-identical in size to the version
// the engine hardcodes a table for, "PEM10"). So, unlike Beneath a Steel
// Sky, this title needs no SYNCSCUMM_DATA / --extrapath at all.
//
// Integrity: verified against the PINNED sha256 on each build below -- the
// value ScummVM publishes alongside the archive -- using
// CryptContext(CryptContext.ALGO.SHA2), cryptlib's SHA-2 context defaulting to
// 256-bit output (confirmed at authoring time against known SHA-256 test
// vectors, including embedded-NUL/high-bit binary input, via File.read()'s
// byte-preserving string round trip).
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
//     jsexec ../xtrn/syncqueen/getdata.js
//
// Idempotent: a build whose marker file already exists (talkie/queen.1c,
// floppy/queen.1) is left alone and reported "present". SpiderMonkey 1.8.5
// compatible (no modern ES) -- runs unchanged on Windows, which a POSIX
// shell script (the previous getdata.sh) could not.
//
// Copyright (C) 2026 Rob Swindell / syncqueen.  GPL-2.0.

load("http.js");
load("xtrn_mirror.js");
load("sha256.js");

var BASE = "https://downloads.scummvm.org/frs/extras/Flight%20of%20the%20Amazon%20Queen";

// The two English freeware builds ScummVM hosts.  destdir is relative to the
// door directory; marker is the file fetch_build() uses to decide a build is
// already installed (matches the old getdata.sh's idempotency check).
var BUILDS = [
	{ zip: "FOTAQ_Talkie-1.1.zip", dir: "talkie", marker: "queen.1c", label: "talkie",
	  sha256: "a25cdd5e003a0a5e402af99b218cc7ea81ad032cb36b8c05df3bd1167038d8a8" },
	{ zip: "FOTAQ_Floppy.zip",     dir: "floppy", marker: "queen.1",  label: "floppy",
	  sha256: "2e59de85f708cdb32bf85c85b394ac091c05f7647e856b71f5b3ae73fde761e0" }
];

// The door's own directory (where this script lives).  js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// Fetch and unpack one build.  Returns true on success (including "already
// present"), false on any failure (never throws -- a failed build is
// reported and the other build is still attempted).
function fetch_build(b, root)
{
	var destdir = backslash(root + b.dir);

	if (file_exists(destdir + b.marker)) {
		print(b.label + ": present");
		return true;
	}
	mkpath(destdir);

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

	print("  extracting into " + destdir + " ...");
	try {
		var ar = new Archive(tmp);
		var got_n = ar.extract(destdir, false, true, 0);   // flatten, overwrite, all files
		print("  extracted " + got_n + " file(s)");
	} catch (e) {
		print("  ! extraction failed: " + e);
		file_remove(tmp);
		return false;
	}
	file_remove(tmp);

	if (!file_exists(destdir + b.marker)) {
		print("  ! extracted, but " + b.marker + " is still missing -- something's off");
		return false;
	}
	return true;
}

// Print the basenames present in a build's directory, like the old
// getdata.sh's closing `ls "$HERE/talkie" "$HERE/floppy"`.
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
	print("Flight of the Amazon Queen (syncqueen) game-data fetcher");
	print("  door directory: " + root);

	var i, ok = true;
	for (i = 0; i < BUILDS.length; i++) {
		if (!fetch_build(BUILDS[i], root))
			ok = false;
	}

	print("");
	for (i = 0; i < BUILDS.length; i++)
		list_dir(backslash(root + BUILDS[i].dir), BUILDS[i].dir);

	if (!ok) {
		print("");
		print("One or more builds failed to fetch -- re-run to retry:");
		print("    jsexec ../xtrn/syncqueen/getdata.js");
		return 1;
	}
	return 0;
}

if (typeof SYNCQUEEN_GETDATA_NO_MAIN == "undefined")
	exit(main());
