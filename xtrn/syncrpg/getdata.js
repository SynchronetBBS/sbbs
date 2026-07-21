// getdata.js -- fetch the pinned Yume Nikki game data for THIS installed
// SyncRPG (EasyRPG) door package.
//
// Yume Nikki is freeware; the pinned source is rmarchiv.de's "Yume Nikki --
// English 0.10a (Steam)" release (page https://rmarchiv.de/games/1089,
// download id 4174) -- a plain ZIP containing a ready RM2003 tree under
// yumenikki/ with ASCII asset filenames, so Synchronet's native Archive
// (libarchive) can unpack it directly with no transcode. This is the
// EasyRPG-community's own recommended source; see
// src/doors/syncrpg/PROVENANCE.md /
// docs/superpowers/specs/2026-07-19-syncrpg-door-design.md for why the
// archive.org copy (an LZH-in-ZIP with Shift-JIS names) was rejected --
// Synchronet's Archive class does ZIP but not LHA.
//
// This is the INSTALLED-PACKAGE twin of src/doors/syncrpg/getdata.js (the
// source tree's own copy, which drops the same data into test/games/ for the
// headless boot test) -- same pinned URL/hash, same extraction logic; only
// the destination differs (THIS directory, not the source checkout).
//
// THE TOKENED-URL PROBLEM: rmarchiv.de does not serve id 4174 at a fixed
// direct URL. https://rmarchiv.de/games/download/4174 (no token) 302s back to
// the game page; https://rmarchiv.de/games/download/4174/<token> serves the
// file, but <token> is single-use -- reusing an already-consumed token 302s
// back to the game page instead of the zip. So there is no stable hardcodable
// direct URL; this script fetches the game page fresh every run and scrapes
// out that run's current token, then downloads immediately while it is still
// good. (The page's declared Content-Type on some responses is misleadingly
// "text/html" even when the body is the real zip -- don't trust that header;
// the sha256 check below is what actually proves the payload is right.)
//
// Integrity: the downloaded zip's sha256 is verified against the pinned value
// below (independently confirmed against this exact release) using
// CryptContext(CryptContext.ALGO.SHA2) before it is unpacked -- the same real
// hash check the syncdrascula/synclure fetchers do. A mismatch is fatal:
// nothing is extracted and the temp file is removed.
//
// Extracts into THIS directory, where the archive's own yumenikki/ top-level
// folder lands -- install-xtrn.ini's cmd points the door's game argument at
// that same "yumenikki" sub-directory (relative to this package's startup
// dir). Idempotent -- safe to re-run; if yumenikki/RPG_RT.ldb is already
// present it is left alone.
//
// Run by hand any time:
//
//     jsexec ../xtrn/syncrpg/getdata.js
//
// SpiderMonkey 1.8.5 compatible (no modern ES) -- runs unchanged on Windows
// and Linux, unlike a POSIX shell fetch script.
//
// Copyright (C) 2026 Rob Swindell / syncrpg.  GPL-2.0.

load("http.js");

// rmarchiv.de "Yume Nikki -- English 0.10a (Steam)": page + download id.
var LANDING_URL = "https://rmarchiv.de/games/1089";
var DOWNLOAD_ID = "4174";

// Pinned sha256 of that exact release's zip (~79 MB), independently verified.
var EXPECT_SHA256 = "82af2204e2bb39597ac2454d53fc15b2651ea8ec8a5c7258c1ea45ac98e2124d";

// The archive's own top-level folder + the file EasyRPG/liblcf need to find
// to recognize an RM2003 project -- our marker for "already installed".
var MARKER = "yumenikki/RPG_RT.ldb";

// This script's own directory (correct whether launched by hand or by
// install-xtrn.js) -- js.exec_dir is the running SCRIPT's own dir, unlike
// js.startup_dir (the BBS/jsexec working dir).
function pkg_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);
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
// output; File.read() returns a byte-per-char string that round-trips
// losslessly through CryptContext.encrypt(), high-bit and NUL bytes included.
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

// Fetch the game's landing page fresh and scrape out THIS run's direct-
// download URL for DOWNLOAD_ID. The href looks like
// ".../games/download/4174/1784613712" -- the trailing number is a one-shot
// token minted per page load; there is no fixed URL to hardcode, so this must
// run every time, immediately before the download.
function resolve_download_url()
{
	var req = new HTTPRequest();
	req.follow_redirects = 5;
	var body = req.Get(LANDING_URL);
	if (req.response_code != 200)
		throw new Error("HTTP " + req.response_code + " fetching " + LANDING_URL);
	var re = new RegExp("games/download/" + DOWNLOAD_ID + "/(\\d+)");
	var m = String(body).match(re);
	if (!m)
		throw new Error("could not find a download/" + DOWNLOAD_ID + "/<token> link on " + LANDING_URL);
	return "https://rmarchiv.de/games/download/" + DOWNLOAD_ID + "/" + m[1];
}

function main()
{
	var root = pkg_dir();
	print("Yume Nikki (syncrpg) game-data fetcher");
	print("  game directory: " + root);

	if (file_exists(root + MARKER)) {
		print("game: present");
		return 0;
	}

	print("resolving current download URL from " + LANDING_URL + " ...");
	var url;
	try {
		url = resolve_download_url();
	} catch (e) {
		print("  ! could not resolve a download URL: " + e);
		return 1;
	}
	print("  " + url);

	var tmp = backslash(system.temp_dir) + "yumenikki-0.10a-steam.zip";
	print("downloading ...");
	var req = new HTTPRequest();
	req.follow_redirects = 5;
	var n = 0;
	try {
		n = req.Download(url, tmp);
	} catch (e) {
		print("  ! download failed: " + e);
		return 1;
	}
	if (req.response_code != 200 || !n) {
		print("  ! download failed: HTTP " + req.response_code);
		if (file_exists(tmp))
			file_remove(tmp);
		return 1;
	}
	print("  downloaded " + n + " bytes; verifying sha256 ...");

	var got = sha256_of_file(tmp);
	if (got != EXPECT_SHA256) {
		print("  ! sha256 MISMATCH -- refusing to extract");
		print("    expected: " + EXPECT_SHA256);
		print("    got:      " + got);
		file_remove(tmp);
		return 1;
	}
	print("  sha256 verified (" + got + ")");

	print("  extracting into " + root + " ...");
	try {
		// Archive.extract(outdir, with_path, overwrite, max_files, file_list...)
		// with_path=true: the archive's own yumenikki/ folder is preserved, so
		// it lands as root/yumenikki/... -- no filter, the archive is entirely
		// under that one folder, and every filename in it is plain ASCII.
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, true, true, 0);
		print("  extracted " + got_n + " file(s)");
	} catch (e) {
		print("  ! extraction failed: " + e);
		file_remove(tmp);
		return 1;
	}
	file_remove(tmp);

	if (!file_exists(root + MARKER)) {
		print("  ! extracted, but " + MARKER + " is still missing -- something's off");
		return 1;
	}
	print("game: installed");
	return 0;
}

if (typeof SYNCRPG_GETDATA_NO_MAIN == "undefined")
	exit(main());
