// getdata.js -- fetch the freeware Beneath a Steel Sky game data for the
// syncbass door: the CD build, into the door directory itself.
//
// Beneath a Steel Sky (Revolution Software, 1994) was released as freeware by
// its authors in 2003 and is hosted by ScummVM, so this script downloads it
// itself rather than expecting the sysop to supply a copy (unlike Master of
// Orion, syncmoo1/getdata.js).
//
// ONE build, not two. Unlike Flight of the Amazon Queen -- whose Talkie and
// Floppy releases are genuinely distinct data sets (syncqueen/getdata.js
// fetches both into talkie/ and floppy/) -- the BASS *CD* build renders BOTH
// speech AND on-screen subtitles from a single data set. So it alone serves
// every session: the door plays its speech to a terminal that has audio, and
// shows its subtitles to one that does not (the subtitles-auto default, see
// src/doors/syncscumm door/syncscumm.cpp). No separate floppy build is
// fetched -- ScummVM's BASS-Floppy-1.3.zip is the older 1994 content AND ships
// no sky.cpt, so it would be both the lesser version and incomplete. The CD
// build is self-contained: it includes sky.cpt (the sky engine's required
// data file), so the door finds everything and no SYNCSCUMM_DATA / --extrapath
// is needed.
//
// The four game files (sky.cpt, sky.dnr, sky.dsk, readme.txt) unpack straight
// into this directory. The door's install-xtrn.ini cmd passes NO --path, so
// door/syncscumm.cpp defaults it to the door's own startup directory (this
// one) and sst_select_datadir(".", audio, ...) -- finding no ./talkie or
// ./floppy subdir -- falls back to it for both audio and no-audio sessions
// ("flat single build"). A sysop who later wants the two-build split can drop
// talkie/ + floppy/ subdirs in here and the door picks per session
// automatically.
//
// Integrity: verified against ScummVM's published <zip>.sha256 using
// CryptContext(CryptContext.ALGO.SHA2) -- cryptlib's SHA-2 context defaults
// to 256-bit output. This is a real hash check, unlike the sibling fetchers
// (exec/load/syncretro_getcore.js, syncmoo1/getdata.js), which trust HTTPS +
// the known source and don't hash-verify at all.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/syncbass/getdata.js
//
// Idempotent: if sky.cpt already exists the build is left alone and reported
// "present". SpiderMonkey 1.8.5 compatible (no modern ES) -- runs unchanged on
// Windows, which a POSIX shell script could not.
//
// Copyright (C) 2026 Rob Swindell / syncbass.  GPL-2.0.

load("http.js");

var BASE = "https://downloads.scummvm.org/frs/extras/Beneath%20a%20Steel%20Sky";

// The one freeware build this door serves.  dir is "" -- it unpacks into the
// door directory itself (no subdir), the flat single-build layout the door
// falls back to when no --path names a talkie/floppy variant.  marker is the
// file fetch_build() uses to decide the build is already installed -- sky.cpt,
// the engine-data file the CD build carries and the floppy build lacks, so its
// presence proves a complete, runnable set.
var BUILDS = [
	{ zip: "bass-cd-1.2.zip", dir: "", marker: "sky.cpt", label: "CD (speech + subtitles)" }
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
// CryptContext.encrypt() -- including bytes with the high bit set and
// embedded NULs.
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
// char hash out of it -- the file is standard sha256sum format ("<hash>" or
// "<hash>  <filename>"); take field 1.
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
// present"), false on any failure (never throws).
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

function main()
{
	var root = door_dir();
	print("Beneath a Steel Sky (syncbass) game-data fetcher");
	print("  door directory: " + root);

	var i, ok = true;
	for (i = 0; i < BUILDS.length; i++) {
		if (!fetch_build(BUILDS[i], root))
			ok = false;
	}

	print("");
	if (file_exists(root + "sky.cpt"))
		print("installed: sky.cpt sky.dnr sky.dsk readme.txt");

	if (!ok) {
		print("");
		print("Fetch failed -- re-run to retry:");
		print("    jsexec ../xtrn/syncbass/getdata.js");
		return 1;
	}
	return 0;
}

if (typeof SYNCBASS_GETDATA_NO_MAIN == "undefined")
	exit(main());
