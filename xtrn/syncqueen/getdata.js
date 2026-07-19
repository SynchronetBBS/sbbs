// getdata.js -- fetch the freeware Flight of the Amazon Queen game data (both
// builds) for the syncqueen door: talkie/ (speech) and floppy/ (text).
//
// Unlike Master of Orion (syncmoo1/getdata.js), FOTAQ is officially freeware
// -- John Passfield & Steve Stamatiadis released it for free redistribution
// -- so this script downloads it itself, straight from ScummVM's own
// hosting, rather than expecting the sysop to supply a copy.
//
// "FOTAQ_Talkie-1.0.zip" (an earlier draft's guess) exists on ScummVM's
// server but has no matching .sha256 published, so it cannot be verified --
// this uses FOTAQ_Talkie-1.1.zip instead, which does have one. Verified by
// hand at authoring time: both zips download, both sha256s check out, and
// neither build needs ScummVM's external queen.tbl engine-data file (the
// Talkie build's queen.1c is a self-contained "rebuilt" resource file --
// ScummVM's own compression_queen tool embeds the lookup table inline --
// and the Floppy build's queen.1 is byte-identical in size to the version
// the engine hardcodes a table for, "PEM10"). So, unlike Beneath a Steel
// Sky, this title needs no SYNCSCUMM_DATA / --extrapath at all.
//
// Integrity: verified against ScummVM's published <zip>.sha256 using
// CryptContext(CryptContext.ALGO.SHA2) -- cryptlib's SHA-2 context defaults
// to 256-bit output (confirmed at authoring time against known SHA-256 test
// vectors, including embedded-NUL/high-bit binary input, via File.read()'s
// byte-preserving string round trip). This is a real hash check, unlike the
// sibling fetchers (exec/load/syncretro_getcore.js, syncmoo1/getdata.js),
// which trust HTTPS + the known source and don't hash-verify at all.
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

var BASE = "https://downloads.scummvm.org/frs/extras/Flight%20of%20the%20Amazon%20Queen";

// The two English freeware builds ScummVM hosts.  destdir is relative to the
// door directory; marker is the file fetch_build() uses to decide a build is
// already installed (matches the old getdata.sh's idempotency check).
var BUILDS = [
	{ zip: "FOTAQ_Talkie-1.1.zip", dir: "talkie", marker: "queen.1c", label: "talkie" },
	{ zip: "FOTAQ_Floppy.zip",     dir: "floppy", marker: "queen.1",  label: "floppy" }
];

// The door's own directory (where this script lives).  js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// Hex-encode a raw byte string (e.g. CryptContext.hashvalue).
function hexify(s)
{
	var out = "", i, c;
	for (i = 0; i < s.length; i++) {
		c = s.charCodeAt(i) & 0xff;
		out += (c < 16 ? "0" : "") + c.toString(16);
	}
	return out;
}

// SHA-256 of a file's contents, streamed in chunks (never buffers the whole
// file twice). CryptContext(CryptContext.ALGO.SHA2) defaults to 256-bit
// output; File.read() returns a byte-per-char string (each charCodeAt() is
// the raw 0-255 byte value), which round-trips losslessly through
// CryptContext.encrypt() -- verified at authoring time, including bytes with
// the high bit set and embedded NULs.
function sha256_of_file(path)
{
	var f = new File(path);
	if (!f.open("rb"))
		throw new Error("cannot open " + path + " for hashing");
	var cc = new CryptContext(CryptContext.ALGO.SHA2);
	var chunk;
	try {
		while ((chunk = f.read(65536)) != null && chunk.length > 0)
			cc.encrypt(chunk);
	} finally {
		f.close();
	}
	cc.encrypt("");   // empty final call: finalizes the hash (cryptlib convention)
	return hexify(cc.hashvalue);
}

// Fetch <url> (ScummVM's published "<zip>.sha256") and pull the first 64-hex-
// char hash out of it -- same as the old getdata.sh's `awk '{print $1}'` on a
// standard sha256sum-format file ("<hash>  <filename>").
function fetch_sha256(url)
{
	var req = new HTTPRequest();
	req.follow_redirects = 5;
	var body = req.Get(url);
	if (req.response_code != 200)
		throw new Error("HTTP " + req.response_code + " fetching " + url);
	var m = String(body).match(/([0-9a-fA-F]{64})/);
	if (!m)
		throw new Error("no sha256 hash found in " + url);
	return m[1].toLowerCase();
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
	var req = new HTTPRequest();
	req.follow_redirects = 5;
	var n = 0;
	try {
		n = req.Download(url, tmp);
	} catch (e) {
		print("  ! download failed: " + e);
		return false;
	}
	if (req.response_code != 200 || !n) {
		print("  ! download failed: HTTP " + req.response_code);
		if (file_exists(tmp))
			file_remove(tmp);
		return false;
	}
	print("  downloaded " + n + " bytes; verifying sha256 ...");

	var expect;
	try {
		expect = fetch_sha256(url + ".sha256");
	} catch (e) {
		print("  ! could not fetch/parse " + b.zip + ".sha256: " + e);
		file_remove(tmp);
		return false;
	}

	var got = sha256_of_file(tmp);
	if (got != expect) {
		print("  ! sha256 MISMATCH for " + b.zip);
		print("    expected: " + expect);
		print("    got:      " + got);
		file_remove(tmp);
		return false;
	}
	print("  sha256 verified (" + got + ")");

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
