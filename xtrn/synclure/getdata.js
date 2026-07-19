// getdata.js -- fetch the freeware Lure of the Temptress game data for the
// synclure door.
//
// Lure of the Temptress was released as freeware by Revolution Software and
// ScummVM hosts it, so this script downloads it -- straight from ScummVM's own
// distribution point -- rather than this repository bundling copyrighted data
// or a sysop supplying a copy.
//
// Fetches lure-1.1.zip (the English build) and unpacks just the VGA disk
// resources (Disk1.vga .. Disk4.vga) and the freeware LICENSE.txt into THIS
// door directory, flattening the archive's lure/ subfolder. The EGA disks,
// Lure.exe and the PDFs the archive also carries are deliberately left out:
// ScummVM's lure engine runs the VGA data through lure.dat (see below), and
// taking VGA alone makes it detect a single game rather than the VGA/EGA pair
// the full archive would.
//
// lure.dat: the lure engine's own ScummVM engine-data file (ScummVM generates
// it from the game) is NOT part of this download -- it ships committed in this
// package directory, and the door finds it via --path.
//
// Integrity: the archive is verified against ScummVM's published
// lure-1.1.zip.sha256 using CryptContext(CryptContext.ALGO.SHA2) (cryptlib's
// SHA-2 context defaults to 256-bit output) before it is unpacked.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/synclure/getdata.js
//
// Idempotent: if Disk1.vga already exists the download is skipped. SpiderMonkey
// 1.8.5 compatible (no modern ES) -- runs unchanged on Windows.
//
// Copyright (C) 2026 Rob Swindell / synclure.  GPL-2.0.

load("http.js");

var BASE   = "https://downloads.scummvm.org/frs/extras/Lure%20of%20the%20Temptress";
var ZIP    = "lure-1.1.zip";
var MARKER = "Disk1.vga";

// The door's own directory (where this script lives). js.exec_dir is the
// running SCRIPT's own dir -- correct whether launched by the installer or by
// hand via jsexec -- unlike js.startup_dir (the BBS/jsexec working dir).
function door_dir()
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

// SHA-256 of a file's contents, streamed in chunks. File.read() returns a
// byte-per-char string that round-trips losslessly through
// CryptContext.encrypt(), high-bit and NUL bytes included.
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
	cc.encrypt("");   // empty final call finalizes the hash (cryptlib convention)
	return hexify(cc.hashvalue);
}

// Fetch <url> (ScummVM's published "<zip>.sha256") and pull the first 64-hex-
// char hash out of it -- a standard sha256sum-format file.
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

function main()
{
	var root = door_dir();
	print("Lure of the Temptress (synclure) game-data fetcher");
	print("  door directory: " + root);

	if (file_exists(root + MARKER)) {
		print("game: present");
		return 0;
	}

	var url = BASE + "/" + ZIP;
	var tmp = backslash(system.temp_dir) + ZIP;

	print("downloading " + ZIP + " ...");
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

	var expect;
	try {
		expect = fetch_sha256(url + ".sha256");
	} catch (e) {
		print("  ! could not fetch/parse " + ZIP + ".sha256: " + e);
		file_remove(tmp);
		return 1;
	}
	var got = sha256_of_file(tmp);
	if (got != expect) {
		print("  ! sha256 MISMATCH for " + ZIP);
		print("    expected: " + expect);
		print("    got:      " + got);
		file_remove(tmp);
		return 1;
	}
	print("  sha256 verified (" + got + ")");

	print("  extracting into " + root + " ...");
	try {
		// with_path=false + trailing recurse=true: flatten the archive's lure/
		// subfolder, taking only the VGA disks and the freeware license.
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, false, true, 0, "Disk*.vga", "LICENSE.txt", true);
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

if (typeof SYNCLURE_GETDATA_NO_MAIN == "undefined")
	exit(main());
