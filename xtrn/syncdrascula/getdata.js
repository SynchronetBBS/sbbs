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
// Integrity: each archive is verified against ScummVM's published
// <zip>.sha256 using CryptContext(CryptContext.ALGO.SHA2) (cryptlib's SHA-2
// context defaults to 256-bit output) before it is unpacked -- the same real
// hash check the syncqueen/syncbass fetchers do.
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
	  label: "game (required)",          required: true },
	{ zip: "drascula-audio-mp3-2.0.zip", marker: "audio/track1.mp3",
	  with_path: true,  file: "audio/*.mp3",
	  label: "CD soundtrack (optional)", required: false }
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
// file twice).  CryptContext(CryptContext.ALGO.SHA2) defaults to 256-bit
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

// Fetch <url> (ScummVM's published "<zip>.sha256") and pull the first 64-hex-
// char hash out of it -- a standard sha256sum-format file ("<hash>  <name>").
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
