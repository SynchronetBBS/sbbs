// getdata.js -- fetch the freeware SCI fan game "Betrayed Alliance Book 1"
// for the betrayedalliance door, straight into the door directory.
//
// PROVENANCE / LICENSING -- the cleanest of this door family.
// Betrayed Alliance Book 1 is an original EGA, parser-driven SCI adventure by
// Ryan Slattery ("Slattstudio"), built on Sierra's SCI interpreter but with
// its own story and art -- NOT Sierra IP. Unlike Space Quest 0 (Sierra IP, no
// grant) and Cascade Quest (no explicit grant), it ships an explicit MIT
// LICENSE.TXT: "Permission is hereby granted, free of charge, ... to use,
// copy, ... publish, distribute ... copies of the Software". So redistribution
// is expressly permitted -- we could even bundle it -- but this script keeps
// the family's fetch-from-source model anyway (light repo, always the pinned
// release), downloading from the AUTHOR'S OWN GitHub release. To honor the MIT
// notice clause, LICENSE.TXT is extracted alongside the game resources.
//
// The SCI engine (added to the ScummVM build alongside agi) plays it; ScummVM
// detects the resources as game id "sci-fanmade" (its generic fan-made SCI
// id), described "Betrayed Alliance 1.3.3.1" -- the newest version ScummVM's
// detection tables recognize (verified against engines/sci/detection_tables.h;
// the pinned zip's resource.map is a byte-exact match).
//
// The release zip nests everything under a top-level folder and bundles a
// whole DOSBox package (DOSBox.exe, SCIV.EXE, *.DRV, SDL*.dll) for playing on
// modern Windows -- none of which ScummVM uses. The extract below takes ONLY
// the SCI resources (resource.map/001, RESOURCE.CFG) plus LICENSE.TXT, with
// recurse=true so the nested folder is flattened away.
//
// Integrity: the author publishes no checksum, so the expected SHA-256 and
// byte size are PINNED here (release-v1.3.3.1, the -minimal.zip asset). A
// re-release under the same URL that changes the bytes will fail the check
// rather than install something unverified -- update the pin deliberately.
//
// Run automatically (and prompted) by the installer via install-xtrn.ini's
// [exec:getdata.js] step, or by hand any time:
//
//     jsexec ../xtrn/betrayedalliance/getdata.js
//
// Idempotent: if resource.map already exists the game is left alone and
// reported "present". SpiderMonkey 1.8.5 compatible.
//
// Copyright (C) 2026 Rob Swindell / betrayedalliance (this fetcher only).
// GPL-2.0. The game data it downloads is Ryan Slattery's, MIT-licensed.

load("http.js");

// The one pinned download -- the author's own GitHub release asset.
var GAME = {
	url:    "https://github.com/Slattstudio/BetrayedAllianceBook1/releases/download/release-v1.3.3.1/BetrayedAllianceBook1-v1.3.3.1-minimal.zip",
	zip:    "BetrayedAllianceBook1-v1.3.3.1-minimal.zip",
	size:   3383186,
	sha256: "e89866eabdfa7e3759e061b9780af5f9c0d1e2c54e686a4a9fd641b4a657b029",
	marker: "resource.map",
	label:  "Betrayed Alliance Book 1 v1.3.3.1"
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
	print("Betrayed Alliance Book 1 (betrayedalliance) game-data fetcher");
	print("  door directory: " + root);

	if (file_exists(root + GAME.marker)) {
		print(GAME.label + ": present");
		return 0;
	}

	var tmp = backslash(system.temp_dir) + GAME.zip;
	print(GAME.label + ": downloading from the author's GitHub release ...");
	var req = new HTTPRequest();
	req.follow_redirects = 5;   // GitHub redirects release assets to its CDN
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

	// Extract ONLY the SCI resources + the MIT license, recursing into the
	// zip's top-level folder and flattening it away (with_path=false). The
	// bundled DOSBox package (DOSBox.exe, SCIV.EXE, *.DRV, SDL*.dll, the .asm/
	// .sh/.conf launchers) is not matched, so it is never extracted. The
	// trailing "true" is the recurse flag; "resource.*"/"LICENSE.TXT" match on
	// basename because of it.
	print("  extracting the SCI resources + license into " + root + " ...");
	try {
		var ar = new Archive(tmp);
		var got_n = ar.extract(root, false, true, 0, "resource.*", "LICENSE.TXT", true);
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
	print("RESOURCE.CFG) + LICENSE.TXT. ScummVM plays these as game id");
	print("'sci-fanmade' (Betrayed Alliance 1.3.3.1); the ZIP's bundled");
	print("DOSBox/DOS interpreter was skipped.");
	return 0;
}

if (typeof BETRAYEDALLIANCE_GETDATA_NO_MAIN == "undefined")
	exit(main());
