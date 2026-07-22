// fetch-assets.js -- download the freeware Tiberian Dawn game data for SyncDawn.
//
// SyncDawn ships no game data. Electronic Arts released Command & Conquer:
// Tiberian Dawn as freeware, so this fetches a freeware game-data image,
// verifies its checksum, and extracts the MIX archives the engine needs --
// INCLUDING the FMV movie data: SyncConquer plays cutscenes (video plus
// A/V-synced audio) through the door's VQA backend, same as SyncAlert, so
// don't skip TD's movie mix(es). Run automatically (and prompted) by the
// installer via install-xtrn.ini's [exec:fetch-assets.js] step, or by hand:
//
//     jsexec ../xtrn/syncdawn/fetch-assets.js
//
// Idempotent: if the MIX files are already present in the assets dir they are
// left alone, so re-running it does nothing. A download/extract failure is
// reported but non-fatal -- the sysop can always drop the MIX files in by hand.
//
// This mirrors xtrn/syncalert/fetch-assets.js (Red Alert) exactly in mechanism;
// see that file's header for the streaming-download / streaming-SHA-1 /
// libarchive-reads-ISO9660 rationale. SpiderMonkey 1.8.5-compatible.
//
// Copyright(C) 2026 Rob Swindell / syncdawn. GPL-2.0.

// Source: the "Command & Conquer (Gold)" freeware item on archive.org (the
// C&C95 Windows Gold release EA made freeware). The download is a ZIP (not an
// ISO) -- Synchronet's Archive object reads ZIP through libarchive the same
// way. IMAGE_SHA1 is archive.org's own _files.xml sha1 of the ZIP, so it is
// verifiable. http:// (not https://) for the redirect lookup, like the RA
// fetcher: archive.org 302s to an https:// storage node whose cert chain
// cryptlib accepts; the bytes are SHA-1-verified regardless of transport.
var ASSETS_READY = true;
var IMAGE_URL  = "http://archive.org/download/cc_gold/c%26c_gold.zip";
var IMAGE_SHA1 = "b4fadb5ea087c704c68dc6f876d1375a1a16fe06";
var IMAGE_FILE = "cc_gold.zip";

// The MIX archives Vanilla TD needs, and their path INSIDE the ZIP (everything
// is under the "Command & Conquer (Gold)/" folder, lowercase). Archive.extract
// flattens to the lowercase basename; the extract loop below renames it to the
// UPPERCASE `name` the engine + door expect (case matters on Linux). This is the
// exact set the TD engine registers (tiberiandawn/init.cpp `new MFCD(...)`) plus
// the three theater mixes it loads on demand -- it does NOT use the DOS-mode
// LOCAL.MIX (absent from the Gold release), only CCLOCAL.MIX (hi-res bootstrap).
//
// THE UPDATE/PATCH MIXES ARE MANDATORY, NOT OPTIONAL: UPDATE/UPDATA/UPDATEC (the
// v1.06 "Gold" hi-res patch) carry the hi-res TITLE SCREEN (HTITLE.PCX), the
// 12-point fonts, and the menu PALETTE. Without them the engine loads a black
// palette and the whole game renders BLACK even though it reaches the menu --
// which is exactly what happened when this list was missing them. TRANSIT/ZOUNDS
// are likewise registered by init.cpp. MOVIES.MIX is the FMV (~773MB; keep it,
// or a sysop who wants no FMV can delete it). Sizes = the ZIP members' real sizes.
var GOLD = "Command & Conquer (Gold)/";
var MIXES = [
	{ name: "CCLOCAL.MIX",  member: GOLD + "cclocal.mix",  size: 155234 },
	{ name: "CONQUER.MIX",  member: GOLD + "conquer.mix",  size: 2461478 },
	{ name: "GENERAL.MIX",  member: GOLD + "general.mix",  size: 3604892 },
	{ name: "TRANSIT.MIX",  member: GOLD + "transit.mix",  size: 4649011 },
	{ name: "UPDATE.MIX",   member: GOLD + "update.mix",   size: 10233756 },
	{ name: "UPDATA.MIX",   member: GOLD + "updata.mix",   size: 3831786 },
	{ name: "UPDATEC.MIX",  member: GOLD + "updatec.mix",  size: 2254367 },
	{ name: "DESERT.MIX",   member: GOLD + "desert.mix",   size: 678795 },
	{ name: "TEMPERAT.MIX", member: GOLD + "temperat.mix", size: 639405 },
	{ name: "WINTER.MIX",   member: GOLD + "winter.mix",   size: 675763 },
	{ name: "SOUNDS.MIX",   member: GOLD + "sounds.mix",   size: 1315871 },
	{ name: "ZOUNDS.MIX",   member: GOLD + "zounds.mix",   size: 938490 },
	{ name: "SCORES.MIX",   member: GOLD + "scores.mix",   size: 86613286 },
	{ name: "SPEECH.MIX",   member: GOLD + "speech.mix",   size: 1222378 },
	{ name: "MOVIES.MIX",   member: GOLD + "movies.mix",   size: 773057994 }
];

load("http.js");
load("xtrn_mirror.js");

// The door's directory (where the binary and this script live). js.exec_dir is
// the running SCRIPT's own directory (xtrn/syncdawn), whether launched by the
// installer or by hand with jsexec. Fall back to startup_dir if exec_dir is unset.
function door_dir()
{
	var d = js.exec_dir || js.startup_dir || "./";
	return backslash(d);   // ensure a trailing path separator
}

// door_io.c's door_resolve_assets_dir() looks for "<the binary's own dir>/
// assets", and the binary is deployed beside this script -- so the assets dir
// is simply assets/ under the door dir. Created if missing.
function assets_dir()
{
	var dir = backslash(door_dir() + "assets");
	if (!file_isdir(dir))
		mkpath(dir);
	return dir;
}

// Streamed SHA-1 of a file, or undefined if it can't be opened.
function sha1_of(fname)
{
	var f = new File(fname);
	if (!f.open("rb"))
		return undefined;
	var hash = f.sha1_hex;
	f.close();
	return hash;
}

function main()
{
	if (!ASSETS_READY || !IMAGE_URL || !IMAGE_SHA1) {
		print("SyncDawn: the freeware-asset source is not configured yet.");
		print("  This fetcher needs a pinned image URL + SHA-1 and the MIX");
		print("  member paths -- see the TODO block at the top of");
		print("  xtrn/syncdawn/fetch-assets.js. Until then, drop the Tiberian");
		print("  Dawn MIX files into " + assets_dir() + " by hand.");
		return 1;
	}

	var dir = assets_dir();
	print("SyncDawn: assets directory is " + dir);

	var missing = [];
	for (var i = 0; i < MIXES.length; i++) {
		if (file_exists(dir + MIXES[i].name))
			print("  " + MIXES[i].name + " already present -- skipping");
		else
			missing.push(MIXES[i]);
	}
	if (!missing.length) {
		print("SyncDawn: Tiberian Dawn game data already installed -- nothing to do.");
		return 0;
	}

	// Downloaded into the door dir (not system.temp_dir). .gitignore covers the
	// image here. A previous run's image is reused rather than re-fetched.
	var img  = door_dir() + IMAGE_FILE;
	var part = img + ".part";
	var ok = false;
	var verified = false;   // set when the fresh download was already checked

	try {
		if (file_exists(img)) {
			print("  reusing the previously downloaded " + IMAGE_FILE);
		} else {
			print("  downloading the freeware Tiberian Dawn game data from "
			    + IMAGE_URL + " ...");
			// Verified here, before the rename, so a corrupt or truncated
			// transfer falls back to the Synchronet mirror instead of
			// landing a bad image. `name` is required: the mirror keeps
			// this under IMAGE_FILE, not the URL's own basename.
			var n = xtrn_mirror_download(IMAGE_URL, part, {
				name:   IMAGE_FILE,
				verify: function(p) { return sha1_of(p) == IMAGE_SHA1; }
			});
			if (!n) {
				print("  ! could not fetch the game data");
				return 1;
			}
			// Only name it IMAGE_FILE once it's complete, so an interrupted
			// download is never mistaken for a reusable image on the next run.
			if (!file_rename(part, img)) {
				print("  ! could not rename " + part + " to " + img);
				return 1;
			}
			print("  downloaded " + n + " bytes; checksum verified");
			verified = true;
		}

		if (!verified) {
			print("  verifying checksum ...");
			var hash = sha1_of(img);
			if (hash != IMAGE_SHA1) {
				print("  ! checksum mismatch (got " + hash + ", expected " + IMAGE_SHA1
				    + ") -- refusing to use " + IMAGE_FILE);
				print("  ! delete " + img + " and re-run to download it again");
				return 1;
			}
		}

		print("  extracting the MIX archives ...");
		var ar = new Archive(img);
		for (var m = 0; m < missing.length; m++) {
			ar.extract(dir, missing[m].member);
			// Archive.extract flattens the member to its basename -- lowercase in
			// this ZIP (conquer.mix); the engine + door want uppercase (CONQUER.MIX),
			// which matters on a case-sensitive host. Rename if they differ.
			var base = missing[m].member.replace(/^.*[\/\\]/, "");
			if (base != missing[m].name && file_exists(dir + base))
				file_rename(dir + base, dir + missing[m].name);
			var path = dir + missing[m].name;
			if (!file_exists(path)) {
				print("  ! failed to extract " + missing[m].name);
				return 1;
			}
			var sz = file_size(path);
			if (missing[m].size && sz != missing[m].size)
				print("  ! warning: " + missing[m].name + " is " + sz
				    + " bytes (expected " + missing[m].size
				    + ") -- the file may be corrupt");
			else
				print("  installed " + missing[m].name + " (" + sz + " bytes)");
			// The game data is read-only content; keep it readable by the BBS
			// user even if the door runs under a different account.
			file_chmod(path, 0444);
		}
		ok = true;
	} catch (e) {
		print("  ! error fetching the Tiberian Dawn assets: " + e);
	} finally {
		if (file_exists(part))
			file_remove(part);   // aborted mid-download
	}

	if (ok) {
		if (file_remove(img))
			print("  removed " + IMAGE_FILE + " (reclaimed the download)");
		print("SyncDawn: freeware Tiberian Dawn game data installed.");
	} else {
		print("SyncDawn: could not install the game data -- copy the Tiberian "
		    + "Dawn MIX files into " + dir + " by hand.");
	}
	return ok ? 0 : 1;
}

if (typeof SYNCDAWN_FETCH_ASSETS_NO_MAIN == "undefined")
	exit(main());
