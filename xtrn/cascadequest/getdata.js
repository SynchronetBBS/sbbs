// getdata.js -- fetch the freeware SCI fan game "Cascade Quest" for the
// cascadequest door, straight into the door directory.
//
// PROVENANCE / LICENSING -- read before changing the source URL.
// Cascade Quest is an ORIGINAL adventure game by Phil Fortier ("Troflip",
// author of the SCI Companion dev tool), built on Sierra's SCI interpreter
// but with its own story and art -- NOT a Sierra IP fan game (contrast
// spacequest0). He posted it as freeware on the SCI community site
// (sciprogramming.com); he has since built a commercial reboot, "Cascadia
// Quest" (Steam), so this free original is his older release.
//
// It carries no explicit redistribution grant of its own, so -- exactly as
// for spacequest0 -- this repository bundles NO game data. This script
// downloads the ZIP from the author-community host, verifies it byte-for-byte,
// and unpacks only the SCI resource files into this (git-ignored) directory.
// The bytes never live in our repo; sciprogramming.com is the distributor.
// archive.org also mirrors it (item sci_Cascade_Quest_Demo). A sysop who does
// not want to serve it simply does not run this step.
//
// The SCI engine (added to the ScummVM build alongside agi) plays it; ScummVM
// detects the resources as game id "sci-fanmade" (its generic fan-made SCI
// id), described "Cascade Quest". It is an EGA, parser-driven SCI game (type
// commands + arrow/keypad movement, SCI's menu bar for the rest).
//
// Integrity: no checksum is published, so the expected SHA-256 and byte size
// are PINNED here (computed by hand from the current CascadeQuest.zip). If the
// author re-releases under the same name, the hash stops matching and the
// fetch fails loudly rather than installing something unverified -- update the
// pin deliberately.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/cascadequest/getdata.js
//
// Idempotent: if resource.map (an SCI resource file) already exists the game
// is left alone and reported "present". SpiderMonkey 1.8.5 compatible.
//
// Copyright (C) 2026 Rob Swindell / cascadequest (this fetcher script only).
// GPL-2.0. The game data it downloads is Phil Fortier's, not ours.

load("http.js");

// The one pinned download.  url is the author-community host; sha256 + size
// are verified after download; marker (resource.map, the SCI resource index)
// both proves the unpack succeeded and makes re-runs idempotent.
var GAME = {
	url:    "https://sciprogramming.com/esd_scigames/CascadeQuest.zip",
	zip:    "CascadeQuest.zip",
	size:   808169,
	sha256: "ab3c2a7b9b46191408a6e13746f0e1654c142c11577ff7da6f61c1002dad7303",
	marker: "resource.map",
	label:  "Cascade Quest"
};

function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);
}

function hexify(s)
{
	var out = "", i, c;
	for (i = 0; i < s.length; i++) {
		c = s.charCodeAt(i) & 0xff;
		out += (c < 16 ? "0" : "") + c.toString(16);
	}
	return out;
}

// SHA-256 of a file, streamed in chunks. File.read() is byte-preserving and
// round-trips losslessly through CryptContext.encrypt(); the SHA2 context
// defaults to 256-bit output.
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
	cc.encrypt("");   // finalize (cryptlib convention)
	return hexify(cc.hashvalue);
}

function main()
{
	var root = door_dir();
	print("Cascade Quest (cascadequest) game-data fetcher");
	print("  door directory: " + root);

	if (file_exists(root + GAME.marker)) {
		print(GAME.label + ": present");
		return 0;
	}

	var tmp = backslash(system.temp_dir) + GAME.zip;
	print(GAME.label + ": downloading " + GAME.zip + " from " + GAME.url + " ...");
	var req = new HTTPRequest();
	req.follow_redirects = 5;
	var n = 0;
	try {
		n = req.Download(GAME.url, tmp);
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

	var got_size = file_size(tmp);
	if (got_size != GAME.size) {
		print("  ! size MISMATCH: expected " + GAME.size + ", got " + got_size);
		file_remove(tmp);
		return 1;
	}
	var got = sha256_of_file(tmp);
	if (got != GAME.sha256) {
		print("  ! sha256 MISMATCH");
		print("    expected: " + GAME.sha256);
		print("    got:      " + got);
		file_remove(tmp);
		return 1;
	}
	print("  downloaded " + n + " bytes; sha256 verified (" + got + ")");

	// Extract ONLY the SCI resource files. The ZIP also bundles the DOS SCI
	// interpreter + drivers (SCIV.EXE, *.DRV, *.PIF, ...) that ScummVM does
	// not use; the resource.* wildcard restricts extraction to resource.map,
	// resource.001 and resource.cfg -- the set ScummVM detects and runs from.
	print("  extracting the SCI resources into " + root + " ...");
	try {
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, false, true, 0, "resource.*");
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
	print("installed the SCI resources (resource.map, resource.001,");
	print("resource.cfg). ScummVM plays these as game id 'sci-fanmade'");
	print("(Cascade Quest); the ZIP's DOS interpreter was skipped.");
	return 0;
}

if (typeof CASCADEQUEST_GETDATA_NO_MAIN == "undefined")
	exit(main());
